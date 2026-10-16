#include "http/response.hpp"

#include <array>

namespace hearth::http {

Response Response::text(Status status, std::string body, std::string_view content_type) {
  Response response;
  response.status = status;
  response.headers.set("Content-Type", std::string(content_type));
  response.body = std::move(body);
  return response;
}

Response Response::error(Status status) {
  std::string body = std::to_string(code(status));
  body += ' ';
  body += reason_phrase(status);
  body += '\n';
  return text(status, std::move(body));
}

std::string Response::serialize(bool include_body) const {
  const bool body_allowed = !forbids_body(status);
  std::string out;
  out.reserve(128 + body.size());
  out += "HTTP/1.1 ";
  out += std::to_string(code(status));
  out += ' ';
  out += reason_phrase(status);
  out += "\r\n";
  for (const auto &h : headers.all()) {
    if (!body_allowed && iequals(h.name, "Content-Length")) {
      continue;
    }
    out += h.name;
    out += ": ";
    out += h.value;
    out += "\r\n";
  }
  if (body_allowed && !headers.contains("Content-Length")) {
    out += "Content-Length: ";
    out += std::to_string(body.size());
    out += "\r\n";
  }
  out += "\r\n";
  if (include_body && body_allowed) {
    out += body;
  }
  return out;
}

std::string http_date(std::time_t when) {
  std::tm tm{};
  gmtime_r(&when, &tm);
  std::array<char, 64> buffer{};
  const auto n = std::strftime(buffer.data(), buffer.size(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
  return {buffer.data(), n};
}

} // namespace hearth::http
