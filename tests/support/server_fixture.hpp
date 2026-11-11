#pragma once

#include "server/app.hpp"
#include "server/server.hpp"
#include "support/temp_dir.hpp"

#include <memory>
#include <thread>

namespace hearth::test {

/// Runs a real Server on an ephemeral loopback port in a background thread,
/// serving a small generated document root.
class RunningServer {
public:
  explicit RunningServer(server::ServerConfig config = default_config()) {
    root_.write("index.html", "<h1>hearth</h1>");
    root_.write("style.css", "body { color: red; }");
    root_.write("docs/readme.txt", "read me");
    config.host = "127.0.0.1";
    config.port = 0;
    config.doc_root = root_.path().string();
    server_ = std::make_unique<server::Server>(config);
    server::install_default_routes(*server_);
    thread_ = std::thread([this] { server_->run(); });
  }

  ~RunningServer() { stop(); }

  RunningServer(const RunningServer &) = delete;
  RunningServer &operator=(const RunningServer &) = delete;
  RunningServer(RunningServer &&) = delete;
  RunningServer &operator=(RunningServer &&) = delete;

  /// Stops the server and waits for run() to return.
  void stop() {
    if (thread_.joinable()) {
      server_->stop();
      thread_.join();
    }
  }

  [[nodiscard]] std::uint16_t port() const { return server_->port(); }
  server::Server &server() { return *server_; }
  [[nodiscard]] const TempDir &root() const { return root_; }

  static server::ServerConfig default_config() {
    server::ServerConfig config;
    config.threads = 4;
    return config;
  }

private:
  TempDir root_;
  std::unique_ptr<server::Server> server_;
  std::thread thread_;
};

} // namespace hearth::test
