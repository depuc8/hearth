#include "server/metrics.hpp"
#include "support/http_client.hpp"
#include "support/server_fixture.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <vector>

using hearth::server::Metrics;
using hearth::test::RunningServer;
using hearth::test::TestClient;
using namespace std::chrono_literals;

namespace {

/// Starts all threads at the same instant to maximize contention.
template <typename Fn> void hammer(int threads, Fn &&body) {
  std::atomic<bool> go{false};
  std::vector<std::thread> pool;
  pool.reserve(static_cast<std::size_t>(threads));
  for (int t = 0; t < threads; ++t) {
    pool.emplace_back([&] {
      while (!go.load()) {
        std::this_thread::yield();
      }
      body();
    });
  }
  go.store(true);
  for (auto &th : pool) {
    th.join();
  }
}

std::uint64_t json_field(const std::string &json, const std::string &name) {
  const std::regex pattern("\"" + name + "\":(-?[0-9]+)");
  std::smatch match;
  if (!std::regex_search(json, match, pattern)) {
    ADD_FAILURE() << "field " << name << " missing in " << json;
    return 0;
  }
  return std::stoull(match[1].str());
}

std::string fetch_stats(std::uint16_t port) {
  TestClient client(port);
  client.get("/stats", "Connection: close\r\n");
  const auto r = client.read_response();
  return r.body;
}

} // namespace

TEST(Metrics, StartsAtZero) {
  const Metrics m;
  const auto s = m.snapshot();
  EXPECT_EQ(s.active_connections, 0);
  EXPECT_EQ(s.total_connections, 0U);
  EXPECT_EQ(s.total_requests, 0U);
}

TEST(Metrics, CountsOpenCloseAndRequests) {
  Metrics m;
  m.connection_opened();
  m.connection_opened();
  m.connection_closed();
  m.request_received();
  const auto s = m.snapshot();
  EXPECT_EQ(s.active_connections, 1);
  EXPECT_EQ(s.total_connections, 2U);
  EXPECT_EQ(s.total_requests, 1U);
}

TEST(Metrics, JsonShape) {
  Metrics m;
  m.request_received();
  const auto json = m.to_json();
  EXPECT_TRUE(std::regex_match(json, std::regex(R"(\{"active_connections":0,)"
                                                R"("total_connections":0,)"
                                                R"("total_requests":1,)"
                                                R"("uptime_seconds":[0-9]+\})")))
      << json;
}

// Concurrency: many threads incrementing the same counters. With plain ints
// this loses updates (and TSan reports a data race); with std::atomic the
// totals are exact. Run under the tsan preset to check for races.
TEST(MetricsConcurrency, RequestCounterIsExactUnderContention) {
  Metrics m;
  constexpr int kThreads = 16;
  constexpr int kPerThread = 50'000;
  hammer(kThreads, [&] {
    for (int i = 0; i < kPerThread; ++i) {
      m.request_received();
    }
  });
  EXPECT_EQ(m.snapshot().total_requests, static_cast<std::uint64_t>(kThreads) * kPerThread);
}

TEST(MetricsConcurrency, OpenCloseBalanceUnderContention) {
  Metrics m;
  constexpr int kThreads = 16;
  constexpr int kPerThread = 20'000;
  hammer(kThreads, [&] {
    for (int i = 0; i < kPerThread; ++i) {
      m.connection_opened();
      m.connection_closed();
    }
  });
  const auto s = m.snapshot();
  EXPECT_EQ(s.active_connections, 0);
  EXPECT_EQ(s.total_connections, static_cast<std::uint64_t>(kThreads) * kPerThread);
}

TEST(MetricsConcurrency, ReadersSeeMonotonicTotals) {
  Metrics m;
  std::atomic<bool> done{false};
  std::atomic<bool> went_backwards{false};
  std::thread reader([&] {
    std::uint64_t last = 0;
    while (!done.load()) {
      const auto now = m.snapshot().total_requests;
      if (now < last) {
        went_backwards.store(true);
      }
      last = now;
    }
  });
  hammer(8, [&] {
    for (int i = 0; i < 20'000; ++i) {
      m.request_received();
    }
  });
  done.store(true);
  reader.join();
  EXPECT_FALSE(went_backwards.load());
  EXPECT_EQ(m.snapshot().total_requests, 160'000U);
}

// --- /stats over the wire ------------------------------------------------------

TEST(StatsEndpoint, ServesJson) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/stats");
  const auto r = client.read_response();
  ASSERT_TRUE(r.received());
  EXPECT_EQ(r.status, 200);
  EXPECT_EQ(r.headers.get("Content-Type"), "application/json");
  EXPECT_EQ(r.headers.get("Cache-Control"), "no-store");
  EXPECT_EQ(json_field(r.body, "active_connections"), 1U);
  EXPECT_EQ(json_field(r.body, "total_requests"), 1U); // counts itself
}

TEST(StatsEndpoint, CountsRequestsIncludingErrors) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/");
  ASSERT_TRUE(client.read_response());
  client.get("/missing");
  ASSERT_TRUE(client.read_response());
  client.get("/stats");
  const auto r = client.read_response();
  ASSERT_TRUE(r.received());
  EXPECT_EQ(json_field(r.body, "total_requests"), 3U);
  EXPECT_EQ(json_field(r.body, "total_connections"), 1U);
}

TEST(StatsEndpoint, PostIs405) {
  RunningServer server;
  TestClient client(server.port());
  client.send("POST /stats HTTP/1.1\r\nHost: h\r\nContent-Length: 0\r\n\r\n");
  const auto r = client.read_response();
  ASSERT_TRUE(r.received());
  EXPECT_EQ(r.status, 405);
  EXPECT_EQ(r.headers.get("Allow"), "GET, HEAD");
}

TEST(StatsEndpoint, ActiveConnectionsTrackOpenAndClose) {
  RunningServer server;
  {
    std::vector<std::unique_ptr<TestClient>> clients;
    for (int i = 0; i < 3; ++i) {
      clients.push_back(std::make_unique<TestClient>(server.port()));
      clients.back()->get("/");
      ASSERT_TRUE(clients.back()->read_response()); // ensures it was accepted
    }
    // 3 idle clients + the one asking.
    EXPECT_EQ(json_field(fetch_stats(server.port()), "active_connections"), 4U);
  } // clients disconnect here

  // The server notices closes asynchronously; poll briefly.
  std::uint64_t active = 99;
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (std::chrono::steady_clock::now() < deadline) {
    active = json_field(fetch_stats(server.port()), "active_connections");
    if (active == 1) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_EQ(active, 1U);
  EXPECT_GE(server.server().metrics().snapshot().total_connections, 5U);
}

TEST(StatsEndpoint, ExactTotalsUnderConcurrentLoad) {
  RunningServer server;
  constexpr int kClients = 16;
  constexpr int kRequestsEach = 50;
  std::vector<std::thread> clients;
  clients.reserve(kClients);
  for (int c = 0; c < kClients; ++c) {
    clients.emplace_back([&] {
      TestClient client(server.port());
      for (int i = 0; i < kRequestsEach; ++i) {
        client.get("/style.css");
        EXPECT_TRUE(client.read_response());
      }
    });
  }
  for (auto &t : clients) {
    t.join();
  }
  const auto json = fetch_stats(server.port());
  EXPECT_EQ(json_field(json, "total_requests"),
            static_cast<std::uint64_t>(kClients * kRequestsEach + 1));
  EXPECT_EQ(json_field(json, "total_connections"), static_cast<std::uint64_t>(kClients + 1));
}

TEST(StatsEndpoint, AllConnectionsReleasedAfterStop) {
  auto server = std::make_unique<RunningServer>();
  std::vector<std::unique_ptr<TestClient>> clients;
  for (int i = 0; i < 5; ++i) {
    clients.push_back(std::make_unique<TestClient>(server->port()));
    clients.back()->get("/");
    ASSERT_TRUE(clients.back()->read_response());
  }
  server->stop();
  EXPECT_EQ(server->server().metrics().snapshot().active_connections, 0);
}
