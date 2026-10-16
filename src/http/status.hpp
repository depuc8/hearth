#pragma once

#include <string_view>

namespace hearth::http {

enum class Status : int {
  Ok = 200,
  NoContent = 204,
  MovedPermanently = 301,
  NotModified = 304,
  BadRequest = 400,
  Forbidden = 403,
  NotFound = 404,
  MethodNotAllowed = 405,
  RequestTimeout = 408,
  PayloadTooLarge = 413,
  UriTooLong = 414,
  RequestHeaderFieldsTooLarge = 431,
  InternalServerError = 500,
  NotImplemented = 501,
  ServiceUnavailable = 503,
  HttpVersionNotSupported = 505,
};

constexpr int code(Status status) noexcept {
  return static_cast<int>(status);
}

/// The standard reason phrase, e.g. "Not Found". Unknown codes give "Unknown".
std::string_view reason_phrase(Status status) noexcept;

/// True for statuses that must not carry a body (1xx, 204, 304).
constexpr bool forbids_body(Status status) noexcept {
  const int c = code(status);
  return (c >= 100 && c < 200) || c == 204 || c == 304;
}

} // namespace hearth::http
