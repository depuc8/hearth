#include "server/metrics.hpp"

namespace hearth::server {

MetricsSnapshot Metrics::snapshot() const noexcept {
  MetricsSnapshot s;
  s.active_connections = active_connections_.load(std::memory_order_relaxed);
  s.total_connections = total_connections_.load(std::memory_order_relaxed);
  s.total_requests = total_requests_.load(std::memory_order_relaxed);
  s.uptime_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started_)
          .count();
  return s;
}

std::string Metrics::to_json() const {
  const auto s = snapshot();
  return "{\"active_connections\":" + std::to_string(s.active_connections) +
         ",\"total_connections\":" + std::to_string(s.total_connections) +
         ",\"total_requests\":" + std::to_string(s.total_requests) +
         ",\"uptime_seconds\":" + std::to_string(s.uptime_seconds) + "}";
}

} // namespace hearth::server
