#include "server/server.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <thread>

#include <poll.h>

namespace hearth::server {

namespace {

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

constexpr milliseconds kMaxPollWait{1000};

net::ConnectionOptions make_connection_options(const ServerConfig &config, Metrics &metrics) {
  net::ConnectionOptions options;
  options.metrics = &metrics;
  options.limits.max_header_bytes = config.max_header_bytes;
  options.limits.max_body_bytes = config.max_body_bytes;
  options.max_requests = config.max_requests_per_connection;
  options.write_timeout = config.write_timeout;
  return options;
}

} // namespace

std::size_t resolve_thread_count(std::size_t requested) noexcept {
  if (requested > 0) {
    return requested;
  }
  return std::max(1U, std::thread::hardware_concurrency());
}

Server::Server(ServerConfig config)
    : config_(std::move(config)), connection_options_(make_connection_options(config_, metrics_)),
      listener_(config_.host, config_.port), pool_(resolve_thread_count(config_.threads)) {}

Server::~Server() {
  stop();
  pool_.shutdown();
}

void Server::stop() noexcept {
  stopping_.store(true);
  wake_.notify();
}

void Server::run() {
  std::vector<pollfd> fds;
  std::vector<ConnectionPtr> polled;

  while (!stopping_.load()) {
    adopt_returned();

    // Close idle connections past their deadline and work out how long we
    // may sleep before the next one expires.
    const auto now = Clock::now();
    milliseconds timeout = kMaxPollWait;
    for (auto it = idle_.begin(); it != idle_.end();) {
      const auto deadline = it->second->last_activity() + config_.idle_timeout;
      if (deadline <= now) {
        it = idle_.erase(it);
        continue;
      }
      timeout = std::min(timeout, std::chrono::ceil<milliseconds>(deadline - now));
      ++it;
    }

    fds.clear();
    polled.clear();
    fds.push_back({wake_.read_fd(), POLLIN, 0});
    fds.push_back({listener_.fd(), POLLIN, 0});
    for (const auto &[fd, connection] : idle_) {
      fds.push_back({fd, POLLIN, 0});
      polled.push_back(connection);
    }

    const int ready =
        ::poll(fds.data(), static_cast<nfds_t>(fds.size()), static_cast<int>(timeout.count()));
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw net::errno_error("poll");
    }
    if (ready == 0) {
      continue;
    }
    if (fds[0].revents != 0) {
      wake_.drain();
    }
    if (fds[1].revents != 0) {
      accept_pending();
    }
    for (std::size_t i = 2; i < fds.size(); ++i) {
      if (fds[i].revents != 0) {
        auto &connection = polled[i - 2];
        idle_.erase(connection->fd());
        dispatch(std::move(connection));
      }
    }
  }

  // Graceful shutdown: stop accepting, drop idle connections, let workers
  // finish what they are doing (they answer with "Connection: close").
  listener_.close();
  idle_.clear();
  pool_.shutdown();
  const std::scoped_lock lock(returned_mutex_);
  returned_.clear();
}

void Server::accept_pending() {
  while (auto socket = listener_.accept()) {
    socket->set_nodelay(true); // best effort: failure only affects latency
    auto connection =
        std::make_shared<net::Connection>(std::move(*socket), router_, connection_options_);
    const int fd = connection->fd();
    idle_.emplace(fd, std::move(connection));
  }
}

void Server::dispatch(ConnectionPtr connection) {
  // If the pool has already shut down the task is dropped, which closes
  // the connection.
  pool_.submit([this, connection = std::move(connection)] { process(connection); });
}

void Server::process(const ConnectionPtr &connection) {
  const auto outcome = connection->on_readable(stopping_.load());
  if (outcome == net::Connection::Outcome::KeepOpen && !stopping_.load()) {
    return_to_loop(connection);
  }
}

void Server::return_to_loop(ConnectionPtr connection) {
  {
    const std::scoped_lock lock(returned_mutex_);
    returned_.push_back(std::move(connection));
  }
  wake_.notify();
}

void Server::adopt_returned() {
  std::vector<ConnectionPtr> batch;
  {
    const std::scoped_lock lock(returned_mutex_);
    batch.swap(returned_);
  }
  for (auto &connection : batch) {
    const int fd = connection->fd();
    idle_.emplace(fd, std::move(connection));
  }
}

} // namespace hearth::server
