#pragma once

#include "http/headers.hpp"

#include <string>

namespace hearth::http {

struct Request {
  std::string method;
  /// The raw request-target as sent, e.g. "/a%20b?x=1".
  std::string target;
  /// Percent-decoded path component, e.g. "/a b". "*" for OPTIONS *.
  std::string path;
  /// Raw query string without the '?', e.g. "x=1".
  std::string query;
  int version_major = 1;
  int version_minor = 1;
  Headers headers;
  std::string body;

  /// HTTP/1.1 is persistent unless "Connection: close"; HTTP/1.0 only
  /// persists with "Connection: keep-alive".
  [[nodiscard]] bool keep_alive() const;
};

} // namespace hearth::http
