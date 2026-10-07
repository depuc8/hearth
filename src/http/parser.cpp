#include "http/parser.hpp"

#include "http/url.hpp"

#include <algorithm>
#include <limits>

namespace hearth::http {

namespace {

bool is_visible(char c) noexcept {
  const auto u = static_cast<unsigned char>(c);
  return u > 0x20 && u < 0x7f;
}

bool is_field_value_char(char c) noexcept {
  const auto u = static_cast<unsigned char>(c);
  return u == '\t' || (u >= 0x20 && u != 0x7f); // obs-text (>= 0x80) allowed
}

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

std::string_view trim_ows(std::string_view s) noexcept {
  const auto first = s.find_first_not_of(" \t");
  if (first == std::string_view::npos) {
    return {};
  }
  const auto last = s.find_last_not_of(" \t");
  return s.substr(first, last - first + 1);
}

bool parse_content_length(std::string_view s, std::size_t &out) noexcept {
  if (s.empty()) {
    return false;
  }
  std::size_t value = 0;
  for (const char c : s) {
    if (!is_digit(c)) {
      return false;
    }
    const auto digit = static_cast<std::size_t>(c - '0');
    if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }
  out = value;
  return true;
}

/// Strips "http://host[:port]" from an absolute-form target.
std::string_view origin_form(std::string_view target) noexcept {
  for (const std::string_view scheme : {"http://", "https://"}) {
    if (target.size() >= scheme.size() &&
        iequals(target.substr(0, scheme.size()), scheme)) {
      const auto slash = target.find('/', scheme.size());
      return slash == std::string_view::npos ? std::string_view("/")
                                             : target.substr(slash);
    }
  }
  return target;
}

} // namespace

bool Request::keep_alive() const {
  if (version_major == 1 && version_minor >= 1) {
    return !headers.has_token("Connection", "close");
  }
  return headers.has_token("Connection", "keep-alive");
}

ParseStatus RequestParser::status() const noexcept {
  switch (state_) {
  case State::Complete:
    return ParseStatus::Complete;
  case State::Error:
    return ParseStatus::Error;
  default:
    return ParseStatus::Incomplete;
  }
}

bool RequestParser::in_progress() const noexcept {
  return state_ != State::RequestLine || !line_.empty();
}

void RequestParser::reset() {
  state_ = State::RequestLine;
  request_ = Request{};
  line_.clear();
  header_bytes_ = 0;
  body_remaining_ = 0;
  error_status_ = Status::BadRequest;
  error_reason_.clear();
}

bool RequestParser::fail(Status status, std::string reason) {
  state_ = State::Error;
  error_status_ = status;
  error_reason_ = std::move(reason);
  return false;
}

ParseResult RequestParser::feed(std::string_view data) {
  std::size_t pos = 0;
  while (pos < data.size()) {
    switch (state_) {
    case State::RequestLine:
    case State::Headers: {
      const auto newline = data.find('\n', pos);
      const std::size_t end =
          newline == std::string_view::npos ? data.size() : newline + 1;
      header_bytes_ += end - pos;
      if (header_bytes_ > limits_.max_header_bytes) {
        fail(state_ == State::RequestLine ? Status::UriTooLong
                                          : Status::RequestHeaderFieldsTooLarge,
             state_ == State::RequestLine ? "request line too long"
                                          : "header section too large");
        return {ParseStatus::Error, end};
      }
      line_.append(data.substr(pos, end - pos));
      pos = end;
      if (newline == std::string_view::npos) {
        return {ParseStatus::Incomplete, pos};
      }
      std::string_view line(line_);
      line.remove_suffix(1); // '\n'
      if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
      }
      const bool ok = on_line(line);
      line_.clear();
      if (!ok) {
        return {ParseStatus::Error, pos};
      }
      if (state_ == State::Complete) {
        return {ParseStatus::Complete, pos};
      }
      break;
    }
    case State::Body: {
      const std::size_t take = std::min(body_remaining_, data.size() - pos);
      request_.body.append(data.substr(pos, take));
      pos += take;
      body_remaining_ -= take;
      if (body_remaining_ == 0) {
        state_ = State::Complete;
        return {ParseStatus::Complete, pos};
      }
      break;
    }
    case State::Complete:
    case State::Error:
      return {status(), pos};
    }
  }
  return {status(), pos};
}

bool RequestParser::on_line(std::string_view line) {
  if (line.find('\r') != std::string_view::npos) {
    return fail(Status::BadRequest, "bare CR in line");
  }
  if (state_ == State::RequestLine) {
    // RFC 9112 2.2: ignore empty lines before the request line.
    if (line.empty()) {
      return true;
    }
    return parse_request_line(line);
  }
  if (line.empty()) {
    return finish_headers();
  }
  return parse_header_line(line);
}

bool RequestParser::parse_request_line(std::string_view line) {
  const auto sp1 = line.find(' ');
  const auto sp2 = sp1 == std::string_view::npos ? sp1 : line.find(' ', sp1 + 1);
  if (sp2 == std::string_view::npos) {
    return fail(Status::BadRequest, "malformed request line");
  }
  const auto method = line.substr(0, sp1);
  const auto target = line.substr(sp1 + 1, sp2 - sp1 - 1);
  const auto version = line.substr(sp2 + 1);

  if (method.empty() || !std::all_of(method.begin(), method.end(), is_tchar)) {
    return fail(Status::BadRequest, "invalid method");
  }
  if (target.empty() || !std::all_of(target.begin(), target.end(), is_visible)) {
    return fail(Status::BadRequest, "invalid request target");
  }
  if (version.size() != 8 || version.substr(0, 5) != "HTTP/" ||
      !is_digit(version[5]) || version[6] != '.' || !is_digit(version[7])) {
    return fail(Status::BadRequest, "malformed HTTP version");
  }
  request_.version_major = version[5] - '0';
  request_.version_minor = version[7] - '0';
  if (request_.version_major != 1 || request_.version_minor > 1) {
    return fail(Status::HttpVersionNotSupported, "unsupported HTTP version");
  }

  request_.method = std::string(method);
  request_.target = std::string(target);
  if (target == "*") {
    request_.path = "*";
  } else {
    const auto form = origin_form(target);
    if (form.front() != '/' || form.find('#') != std::string_view::npos) {
      return fail(Status::BadRequest, "invalid request target");
    }
    const auto question = form.find('?');
    auto decoded = percent_decode(form.substr(0, question));
    if (!decoded) {
      return fail(Status::BadRequest, "invalid percent-encoding in path");
    }
    request_.path = std::move(*decoded);
    if (question != std::string_view::npos) {
      request_.query = std::string(form.substr(question + 1));
    }
  }
  state_ = State::Headers;
  return true;
}

bool RequestParser::parse_header_line(std::string_view line) {
  if (line.front() == ' ' || line.front() == '\t') {
    return fail(Status::BadRequest, "obsolete line folding");
  }
  const auto colon = line.find(':');
  if (colon == std::string_view::npos || colon == 0) {
    return fail(Status::BadRequest, "malformed header line");
  }
  const auto name = line.substr(0, colon);
  if (!std::all_of(name.begin(), name.end(), is_tchar)) {
    return fail(Status::BadRequest, "invalid header name");
  }
  const auto value = trim_ows(line.substr(colon + 1));
  if (!std::all_of(value.begin(), value.end(), is_field_value_char)) {
    return fail(Status::BadRequest, "invalid header value");
  }
  if (request_.headers.size() >= limits_.max_header_count) {
    return fail(Status::RequestHeaderFieldsTooLarge, "too many headers");
  }
  request_.headers.add(std::string(name), std::string(value));
  return true;
}

bool RequestParser::finish_headers() {
  const auto &headers = request_.headers;
  const auto hosts = headers.count("Host");
  if (hosts > 1 || (hosts == 0 && request_.version_minor >= 1)) {
    return fail(Status::BadRequest, "exactly one Host header is required");
  }

  const bool chunked = headers.contains("Transfer-Encoding");
  const bool has_length = headers.contains("Content-Length");
  if (chunked && has_length) {
    return fail(Status::BadRequest,
                "both Transfer-Encoding and Content-Length present");
  }
  if (chunked) {
    return fail(Status::NotImplemented, "Transfer-Encoding is not supported");
  }

  std::size_t length = 0;
  bool seen = false;
  for (const auto &h : headers.all()) {
    if (!iequals(h.name, "Content-Length")) {
      continue;
    }
    std::size_t value = 0;
    if (!parse_content_length(h.value, value)) {
      return fail(Status::BadRequest, "invalid Content-Length");
    }
    if (seen && value != length) {
      return fail(Status::BadRequest, "conflicting Content-Length headers");
    }
    length = value;
    seen = true;
  }
  if (length > limits_.max_body_bytes) {
    return fail(Status::PayloadTooLarge, "request body too large");
  }

  body_remaining_ = length;
  if (length == 0) {
    state_ = State::Complete;
  } else {
    request_.body.reserve(length);
    state_ = State::Body;
  }
  return true;
}

} // namespace hearth::http
