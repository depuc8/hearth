#include "http/status.hpp"

namespace hearth::http {

std::string_view reason_phrase(Status status) noexcept {
  switch (status) {
  case Status::Ok:
    return "OK";
  case Status::NoContent:
    return "No Content";
  case Status::MovedPermanently:
    return "Moved Permanently";
  case Status::NotModified:
    return "Not Modified";
  case Status::BadRequest:
    return "Bad Request";
  case Status::Forbidden:
    return "Forbidden";
  case Status::NotFound:
    return "Not Found";
  case Status::MethodNotAllowed:
    return "Method Not Allowed";
  case Status::RequestTimeout:
    return "Request Timeout";
  case Status::PayloadTooLarge:
    return "Content Too Large";
  case Status::UriTooLong:
    return "URI Too Long";
  case Status::RequestHeaderFieldsTooLarge:
    return "Request Header Fields Too Large";
  case Status::InternalServerError:
    return "Internal Server Error";
  case Status::NotImplemented:
    return "Not Implemented";
  case Status::ServiceUnavailable:
    return "Service Unavailable";
  case Status::HttpVersionNotSupported:
    return "HTTP Version Not Supported";
  }
  return "Unknown";
}

} // namespace hearth::http
