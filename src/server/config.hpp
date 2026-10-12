#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace hearth::server {

struct ServerConfig {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8080;
  /// Worker threads; 0 means std::thread::hardware_concurrency().
  std::size_t threads = 0;
  std::string doc_root = "./public";

  /// Close a connection that sends nothing for this long.
  std::chrono::milliseconds idle_timeout{5000};
  /// Give up on a client that won't read its response for this long.
  std::chrono::milliseconds write_timeout{5000};
  /// After this many requests a connection is closed ("Connection: close").
  std::size_t max_requests_per_connection = 1000;

  std::size_t max_header_bytes = 8 * 1024;
  std::size_t max_body_bytes = 1024 * 1024;
};

} // namespace hearth::server
