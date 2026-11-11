#include "support/http_client.hpp"
#include "support/server_fixture.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using hearth::test::RunningServer;
using hearth::test::TestClient;

TEST(Integration, GetIndexOverRealSocket) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 200);
  EXPECT_EQ(r->body, "<h1>hearth</h1>");
  EXPECT_EQ(r->headers.get("Content-Type"), "text/html; charset=utf-8");
  EXPECT_EQ(r->headers.get("Content-Length"), "15");
}

TEST(Integration, ServerAndDateHeadersArePresent) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/style.css");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->headers.get("Server"), "hearth/0.1.0");
  const auto date = r->headers.get("Date").value_or("");
  EXPECT_NE(date.find(" GMT"), std::string_view::npos) << date;
}

TEST(Integration, HeadHasLengthButNoBody) {
  RunningServer server;
  TestClient client(server.port());
  client.send("HEAD /style.css HTTP/1.1\r\nHost: h\r\n\r\n");
  const auto r = client.read_response(/*head=*/true);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 200);
  EXPECT_EQ(r->headers.get("Content-Length"), "20");
  // The next response must start right away: no stray body bytes.
  client.get("/docs/readme.txt");
  const auto next = client.read_response();
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(next->body, "read me");
}

TEST(Integration, MissingFileIs404) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/missing.html");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 404);
  EXPECT_EQ(r->body, "404 Not Found\n");
}

TEST(Integration, WrongMethodIs405WithAllow) {
  RunningServer server;
  TestClient client(server.port());
  client.send("DELETE /index.html HTTP/1.1\r\nHost: h\r\n\r\n");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 405);
  EXPECT_EQ(r->headers.get("Allow"), "GET, HEAD");
}

TEST(Integration, TraversalOverTheWireIs403) {
  RunningServer server;
  for (const char *path :
       {"/../etc/passwd", "/%2e%2e/%2e%2e/etc/passwd", "/..%2F..%2Fetc%2Fpasswd"}) {
    TestClient client(server.port());
    client.get(path);
    const auto r = client.read_response();
    ASSERT_TRUE(r.has_value()) << path;
    EXPECT_EQ(r->status, 403) << path;
  }
}

TEST(Integration, KeepAliveReusesOneConnection) {
  RunningServer server;
  TestClient client(server.port());
  for (int i = 0; i < 5; ++i) {
    client.get(i % 2 == 0 ? "/" : "/style.css");
    const auto r = client.read_response();
    ASSERT_TRUE(r.has_value()) << "request " << i;
    EXPECT_EQ(r->status, 200);
    EXPECT_FALSE(r->headers.get("Connection").has_value());
  }
}

TEST(Integration, ConnectionCloseIsHonored) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/", "Connection: close\r\n");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 200);
  EXPECT_EQ(r->headers.get("Connection"), "close");
  EXPECT_TRUE(client.closed_by_peer(2s));
}

TEST(Integration, Http10ClosesByDefault) {
  RunningServer server;
  TestClient client(server.port());
  client.send("GET / HTTP/1.0\r\n\r\n");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->headers.get("Connection"), "close");
  EXPECT_TRUE(client.closed_by_peer(2s));
}

TEST(Integration, Http10KeepAliveIsHonored) {
  RunningServer server;
  TestClient client(server.port());
  for (int i = 0; i < 2; ++i) {
    client.send("GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
    const auto r = client.read_response();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->headers.get("Connection"), "keep-alive");
  }
}

TEST(Integration, IdleConnectionIsClosedAfterTimeout) {
  auto config = RunningServer::default_config();
  config.idle_timeout = 200ms;
  RunningServer server(config);
  TestClient client(server.port());
  client.get("/");
  ASSERT_TRUE(client.read_response().has_value());
  const auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(client.closed_by_peer(3s));
  EXPECT_GE(std::chrono::steady_clock::now() - start, 150ms);
}

TEST(Integration, SilentConnectionIsClosedAfterTimeout) {
  auto config = RunningServer::default_config();
  config.idle_timeout = 200ms;
  RunningServer server(config);
  TestClient client(server.port());
  EXPECT_TRUE(client.closed_by_peer(3s));
}

TEST(Integration, RequestCapClosesConnection) {
  auto config = RunningServer::default_config();
  config.max_requests_per_connection = 3;
  RunningServer server(config);
  TestClient client(server.port());
  for (int i = 1; i <= 3; ++i) {
    client.get("/");
    const auto r = client.read_response();
    ASSERT_TRUE(r.has_value());
    if (i < 3) {
      EXPECT_FALSE(r->headers.get("Connection").has_value());
    } else {
      EXPECT_EQ(r->headers.get("Connection"), "close");
    }
  }
  EXPECT_TRUE(client.closed_by_peer(2s));
}

TEST(Integration, PipelinedRequestsAreAnsweredInOrder) {
  RunningServer server;
  TestClient client(server.port());
  client.send("GET /index.html HTTP/1.1\r\nHost: h\r\n\r\n"
              "GET /style.css HTTP/1.1\r\nHost: h\r\n\r\n"
              "GET /docs/readme.txt HTTP/1.1\r\nHost: h\r\n\r\n");
  for (const char *expected : {"<h1>hearth</h1>", "body { color: red; }", "read me"}) {
    const auto r = client.read_response();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->body, expected);
  }
}

TEST(Integration, SlowClientByteByByte) {
  RunningServer server;
  TestClient client(server.port());
  const std::string request = "GET /style.css HTTP/1.1\r\nHost: h\r\n\r\n";
  for (const char c : request) {
    client.send(std::string_view(&c, 1));
    std::this_thread::sleep_for(1ms);
  }
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->body, "body { color: red; }");
}

TEST(Integration, MalformedRequestIs400AndCloses) {
  RunningServer server;
  TestClient client(server.port());
  client.send("THIS IS NOT HTTP\r\n\r\n");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 400);
  EXPECT_EQ(r->headers.get("Connection"), "close");
  EXPECT_TRUE(client.closed_by_peer(2s));
}

TEST(Integration, UnsupportedVersionIs505) {
  RunningServer server;
  TestClient client(server.port());
  client.send("GET / HTTP/2.0\r\nHost: h\r\n\r\n");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 505);
}

TEST(Integration, OversizedHeadersAre431) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/", "X-Big: " + std::string(10000, 'x') + "\r\n");
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 431);
}

TEST(Integration, OversizedBodyIs413) {
  auto config = RunningServer::default_config();
  config.max_body_bytes = 1024;
  RunningServer server(config);
  TestClient client(server.port());
  client.send("POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 4096\r\n\r\n" +
              std::string(4096, 'b'));
  const auto r = client.read_response();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->status, 413);
}

TEST(Integration, ConcurrentClients) {
  RunningServer server;
  constexpr int kClients = 32;
  constexpr int kRequestsEach = 25;
  std::atomic<int> ok{0};
  std::vector<std::thread> clients;
  clients.reserve(kClients);
  for (int c = 0; c < kClients; ++c) {
    clients.emplace_back([&] {
      TestClient client(server.port());
      for (int i = 0; i < kRequestsEach; ++i) {
        client.get("/style.css");
        const auto r = client.read_response();
        if (r && r->status == 200 && r->body == "body { color: red; }") {
          ok.fetch_add(1);
        }
      }
    });
  }
  for (auto &t : clients) {
    t.join();
  }
  EXPECT_EQ(ok.load(), kClients * kRequestsEach);
}

TEST(Integration, MoreIdleConnectionsThanWorkers) {
  // With 2 workers, 20 open keep-alive connections must all still be served:
  // idle connections live in the event loop, not on worker threads.
  auto config = RunningServer::default_config();
  config.threads = 2;
  RunningServer server(config);
  std::vector<std::unique_ptr<TestClient>> clients;
  clients.reserve(20);
  for (int i = 0; i < 20; ++i) {
    clients.push_back(std::make_unique<TestClient>(server.port()));
  }
  for (int round = 0; round < 2; ++round) {
    for (auto &client : clients) {
      client->get("/");
      const auto r = client->read_response(false, 2s);
      ASSERT_TRUE(r.has_value());
      EXPECT_EQ(r->status, 200);
    }
  }
}

TEST(Integration, StopClosesIdleConnectionsPromptly) {
  RunningServer server;
  TestClient client(server.port());
  client.get("/");
  ASSERT_TRUE(client.read_response().has_value());
  const auto start = std::chrono::steady_clock::now();
  server.stop();
  EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
  EXPECT_TRUE(client.closed_by_peer(1s));
}

TEST(Integration, StopBeforeRunReturnsImmediately) {
  hearth::server::ServerConfig config;
  config.host = "127.0.0.1";
  config.port = 0;
  config.threads = 1;
  hearth::server::Server server(config);
  server.stop();
  server.run(); // must not block
  SUCCEED();
}
