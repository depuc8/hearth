#pragma once

#include "http/request.hpp"
#include "http/status.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace hearth::http {

struct ParserLimits {
  /// Request line plus all header lines, including line terminators.
  std::size_t max_header_bytes = std::size_t{8} * 1024;
  std::size_t max_header_count = 100;
  std::size_t max_body_bytes = std::size_t{1024} * 1024;
};

enum class ParseStatus { Incomplete, Complete, Error };

struct ParseResult {
  ParseStatus status;
  /// Bytes of the input that belong to this request. On Complete, anything
  /// after `consumed` is the start of the next (pipelined) request.
  std::size_t consumed;
};

/// Incremental HTTP/1.x request parser. Feed it bytes as they arrive, in
/// chunks of any size (including one byte at a time). Supports
/// Content-Length bodies; Transfer-Encoding is rejected with 501.
class RequestParser {
public:
  explicit RequestParser(ParserLimits limits = {}) : limits_(limits) {}

  ParseResult feed(std::string_view data);

  [[nodiscard]] ParseStatus status() const noexcept;
  /// True once any byte of the current request has been seen (ignoring
  /// leading blank lines).
  [[nodiscard]] bool in_progress() const noexcept;

  [[nodiscard]] const Request &request() const noexcept { return request_; }
  [[nodiscard]] Request &request() noexcept { return request_; }

  /// Valid when status() == Error: the status code to answer with.
  [[nodiscard]] Status error_status() const noexcept { return error_status_; }
  [[nodiscard]] const std::string &error_reason() const noexcept { return error_reason_; }

  /// Prepares for the next request on the same connection.
  void reset();

private:
  enum class State { RequestLine, Headers, Body, Complete, Error };

  bool fail(Status status, std::string reason);
  bool on_line(std::string_view line);
  bool parse_request_line(std::string_view line);
  bool parse_header_line(std::string_view line);
  bool finish_headers();

  ParserLimits limits_;
  State state_ = State::RequestLine;
  Request request_;
  std::string line_;
  std::size_t header_bytes_ = 0;
  std::size_t body_remaining_ = 0;
  Status error_status_ = Status::BadRequest;
  std::string error_reason_;
};

} // namespace hearth::http
