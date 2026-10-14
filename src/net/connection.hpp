#pragma once

#include "http/parser.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "net/socket.hpp"
#include "router/router.hpp"
#include "server/metrics.hpp"

#include <chrono>
#include <cstddef>

namespace hearth::net {

struct ConnectionOptions {
  http::ParserLimits limits;
  std::size_t max_requests = 1000;
  std::chrono::milliseconds write_timeout{5000};
  /// Optional; must outlive every Connection that uses it.
  server::Metrics *metrics = nullptr;
};

/// One client connection: owns the socket and the incremental parser, and
/// turns readable bytes into routed responses. Not thread-safe; the server
/// guarantees only one thread touches a Connection at a time.
class Connection {
public:
  using Clock = std::chrono::steady_clock;

  enum class Outcome { KeepOpen, Close };

  Connection(Socket socket, const router::Router &router,
             ConnectionOptions options);
  ~Connection();

  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  Connection(Connection &&) = delete;
  Connection &operator=(Connection &&) = delete;

  [[nodiscard]] int fd() const noexcept { return socket_.fd(); }
  [[nodiscard]] Clock::time_point last_activity() const noexcept {
    return last_activity_;
  }
  [[nodiscard]] std::size_t requests_served() const noexcept {
    return requests_served_;
  }

  /// Reads what is available without blocking, answers every complete
  /// request, and says whether the connection should stay open.
  /// `draining` (server shutting down) forces "Connection: close".
  Outcome on_readable(bool draining);

private:
  /// Routes and writes one response. False if the write failed.
  bool respond(const http::Request &request, bool keep_alive);
  /// Writes an error response and half-closes politely.
  void reject(http::Status status);
  /// Adds Server/Date headers and sends. False if the write failed.
  bool write(http::Response &response, bool include_body);

  Socket socket_;
  const router::Router &router_;
  ConnectionOptions options_;
  http::RequestParser parser_;
  std::size_t requests_served_ = 0;
  Clock::time_point last_activity_;
};

} // namespace hearth::net
