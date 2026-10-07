#pragma once

#include "http/headers.hpp"
#include "http/status.hpp"

#include <ctime>
#include <string>
#include <string_view>

namespace hearth::http {

struct Response {
  Status status = Status::Ok;
  Headers headers;
  std::string body;

  /// A response with `body` and the given Content-Type.
  static Response text(Status status, std::string body,
                       std::string_view content_type = "text/plain; charset=utf-8");
  /// A plain-text error page such as "404 Not Found\n".
  static Response error(Status status);

  /// Serializes to wire format. Adds Content-Length unless the status forbids
  /// a body. With `include_body == false` (HEAD requests) the headers still
  /// describe the body but the body itself is omitted.
  [[nodiscard]] std::string serialize(bool include_body = true) const;
};

/// IMF-fixdate as required for the Date header, e.g.
/// "Sun, 06 Nov 1994 08:49:37 GMT".
std::string http_date(std::time_t when);

} // namespace hearth::http
