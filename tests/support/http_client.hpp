#pragma once

#include "http/headers.hpp"
#include "net/socket.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

namespace hearth::test {

struct ClientResponse {
  int status = 0;
  http::Headers headers;
  std::string body;
};

/// Minimal blocking HTTP/1.1 client over a raw socket, for integration tests.
/// It sends exactly the bytes it is given so tests control the wire format.
class TestClient {
public:
  explicit TestClient(std::uint16_t port) : socket_(::socket(AF_INET, SOCK_STREAM, 0)) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(socket_.fd(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
      throw net::errno_error("connect");
    }
  }

  void send(std::string_view data) {
    while (!data.empty()) {
#ifdef MSG_NOSIGNAL
      const auto n = ::send(socket_.fd(), data.data(), data.size(), MSG_NOSIGNAL);
#else
      const auto n = ::send(socket_.fd(), data.data(), data.size(), 0);
#endif
      if (n <= 0) {
        throw net::errno_error("send");
      }
      data.remove_prefix(static_cast<std::size_t>(n));
    }
  }

  /// Sends a GET with Host and optional extra header lines (each ending in CRLF).
  void get(std::string_view path, std::string_view extra = {}) {
    send("GET " + std::string(path) + " HTTP/1.1\r\nHost: localhost\r\n" +
         std::string(extra) + "\r\n");
  }

  /// Reads one response. nullopt if the server closed the connection (or
  /// nothing arrived within the timeout) before a full response.
  std::optional<ClientResponse> read_response(bool head = false,
                                              std::chrono::milliseconds timeout =
                                                  std::chrono::milliseconds(5000)) {
    std::size_t header_end = std::string::npos;
    while ((header_end = buffer_.find("\r\n\r\n")) == std::string::npos) {
      if (!fill(timeout)) {
        return std::nullopt;
      }
    }
    ClientResponse response;
    const std::string head_block = buffer_.substr(0, header_end);
    buffer_.erase(0, header_end + 4);

    std::size_t line_end = head_block.find("\r\n");
    const std::string status_line = head_block.substr(0, line_end);
    if (status_line.rfind("HTTP/1.1 ", 0) != 0 || status_line.size() < 12) {
      throw std::runtime_error("bad status line: " + status_line);
    }
    response.status = std::stoi(status_line.substr(9, 3));
    while (line_end != std::string::npos) {
      const std::size_t start = line_end + 2;
      line_end = head_block.find("\r\n", start);
      const std::string line = head_block.substr(start, line_end - start);
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        response.headers.add(line.substr(0, colon), line.substr(colon + 2));
      }
    }

    std::size_t length = 0;
    if (const auto cl = response.headers.get("Content-Length"); cl && !head) {
      length = std::stoul(std::string(*cl));
    }
    while (buffer_.size() < length) {
      if (!fill(timeout)) {
        return std::nullopt;
      }
    }
    response.body = buffer_.substr(0, length);
    buffer_.erase(0, length);
    return response;
  }

  /// True if the server closes the connection within `timeout`.
  bool closed_by_peer(std::chrono::milliseconds timeout) {
    for (;;) {
      pollfd pfd{socket_.fd(), POLLIN, 0};
      if (::poll(&pfd, 1, static_cast<int>(timeout.count())) <= 0) {
        return false;
      }
      char byte = 0;
      const auto n = ::recv(socket_.fd(), &byte, 1, 0);
      if (n <= 0) {
        return true;
      }
      buffer_.push_back(byte);
    }
  }

  [[nodiscard]] int fd() const { return socket_.fd(); }

private:
  bool fill(std::chrono::milliseconds timeout) {
    pollfd pfd{socket_.fd(), POLLIN, 0};
    if (::poll(&pfd, 1, static_cast<int>(timeout.count())) <= 0) {
      return false;
    }
    char chunk[16384];
    const auto n = ::recv(socket_.fd(), chunk, sizeof(chunk), 0);
    if (n <= 0) {
      return false;
    }
    buffer_.append(chunk, static_cast<std::size_t>(n));
    return true;
  }

  net::Socket socket_;
  std::string buffer_;
};

} // namespace hearth::test
