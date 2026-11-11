#pragma once

#include "net/connection.hpp"
#include "net/listener.hpp"
#include "net/socket.hpp"
#include "router/router.hpp"
#include "server/config.hpp"
#include "server/metrics.hpp"
#include "server/thread_pool.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace hearth::server {

/// The HTTP server.
///
/// One event-loop thread (whoever calls run()) owns the listening socket and
/// every *idle* connection, and waits on them with poll(). When a connection
/// becomes readable it is removed from the poll set and handed to the worker
/// pool; the worker parses and answers whatever is buffered and then hands the
/// connection back (or drops it). A connection is therefore touched by exactly
/// one thread at a time, and idle keep-alive connections cost no worker.
class Server {
public:
  /// Binds the listening socket immediately, so port() is valid right away.
  explicit Server(ServerConfig config);
  ~Server();

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server(Server &&) = delete;
  Server &operator=(Server &&) = delete;

  /// Register routes before calling run(); the router is read-only after.
  router::Router &router() noexcept { return router_; }
  [[nodiscard]] const ServerConfig &config() const noexcept { return config_; }
  [[nodiscard]] std::uint16_t port() const noexcept { return listener_.port(); }
  [[nodiscard]] const Metrics &metrics() const noexcept { return metrics_; }

  /// Serves until stop() is called. On return every connection is closed and
  /// all in-flight requests have been answered.
  void run();

  /// Requests a graceful shutdown. Thread-safe; may be called before run().
  void stop() noexcept;

private:
  using ConnectionPtr = std::shared_ptr<net::Connection>;

  void accept_pending();
  void dispatch(ConnectionPtr connection);
  void process(const ConnectionPtr &connection);
  void return_to_loop(ConnectionPtr connection);
  void adopt_returned();

  ServerConfig config_;
  // Declared before anything holding connections so it outlives them.
  Metrics metrics_;
  net::ConnectionOptions connection_options_;
  net::Listener listener_;
  net::WakePipe wake_;
  router::Router router_;
  ThreadPool pool_;
  std::atomic<bool> stopping_{false};

  std::mutex returned_mutex_;
  std::vector<ConnectionPtr> returned_; // guarded by returned_mutex_

  std::unordered_map<int, ConnectionPtr> idle_; // event-loop thread only
};

/// Effective worker count for a config (resolves 0 to the core count).
std::size_t resolve_thread_count(std::size_t requested) noexcept;

} // namespace hearth::server
