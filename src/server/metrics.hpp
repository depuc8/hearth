#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace hearth::server {

struct MetricsSnapshot {
  std::int64_t active_connections = 0;
  std::uint64_t total_connections = 0;
  std::uint64_t total_requests = 0;
  std::int64_t uptime_seconds = 0;
};

/// Server-wide counters, updated concurrently by the event loop and every
/// worker.
///
/// Each counter is a std::atomic so that `++` is one indivisible
/// read-modify-write. With a plain integer, two threads can both load 41,
/// both store 42, and one increment is lost; that is also a data race, which
/// is undefined behaviour (see docs/atomic-counter-race.md). Relaxed ordering
/// is enough: the counters don't publish any other data, we only need each
/// increment to be atomic and eventually visible.
class Metrics {
public:
  Metrics() : started_(std::chrono::steady_clock::now()) {}

  void connection_opened() noexcept {
    active_connections_.fetch_add(1, std::memory_order_relaxed);
    total_connections_.fetch_add(1, std::memory_order_relaxed);
  }
  void connection_closed() noexcept {
    active_connections_.fetch_sub(1, std::memory_order_relaxed);
  }
  void request_received() noexcept {
    total_requests_.fetch_add(1, std::memory_order_relaxed);
  }

  [[nodiscard]] MetricsSnapshot snapshot() const noexcept;
  /// The snapshot as a compact JSON object (served at GET /stats).
  [[nodiscard]] std::string to_json() const;

private:
  std::atomic<std::int64_t> active_connections_{0};
  std::atomic<std::uint64_t> total_connections_{0};
  std::atomic<std::uint64_t> total_requests_{0};
  std::chrono::steady_clock::time_point started_;
};

} // namespace hearth::server
