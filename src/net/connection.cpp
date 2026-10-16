#include "net/connection.hpp"

#include "net/io.hpp"
#include "version.hpp"

#include <array>
#include <ctime>
#include <exception>
#include <string>

#include <sys/socket.h>

namespace hearth::net {

namespace {

constexpr std::size_t kReadChunk = std::size_t{16} * 1024;
/// Upper bound on recv() calls per wakeup, so one busy client can't
/// monopolize a worker. Level-triggered poll() brings us back.
constexpr int kMaxReadsPerWake = 16;

const std::string &server_header() {
  static const std::string value = "hearth/" + std::string(hearth::version());
  return value;
}

} // namespace

Connection::Connection(Socket socket, const router::Router &router, ConnectionOptions options)
    : socket_(std::move(socket)), router_(router), options_(options), parser_(options.limits),
      last_activity_(Clock::now()) {
  if (options_.metrics != nullptr) {
    options_.metrics->connection_opened();
  }
}

Connection::~Connection() {
  if (options_.metrics != nullptr) {
    options_.metrics->connection_closed();
  }
}

Connection::Outcome Connection::on_readable(bool draining) {
  std::array<char, kReadChunk> buffer{};
  for (int reads = 0; reads < kMaxReadsPerWake; ++reads) {
    const auto result = recv_some(socket_.fd(), buffer.data(), buffer.size());
    if (result.status == IoStatus::WouldBlock) {
      break;
    }
    if (result.status != IoStatus::Ok) {
      // Peer closed or reset. Anything half-received is abandoned.
      return Outcome::Close;
    }
    last_activity_ = Clock::now();

    std::string_view data(buffer.data(), result.bytes);
    while (!data.empty()) {
      const auto parsed = parser_.feed(data);
      data.remove_prefix(parsed.consumed);
      if (parsed.status == http::ParseStatus::Incomplete) {
        break;
      }
      if (parsed.status == http::ParseStatus::Error) {
        reject(parser_.error_status());
        return Outcome::Close;
      }
      ++requests_served_;
      const bool keep_alive =
          parser_.request().keep_alive() && !draining && requests_served_ < options_.max_requests;
      if (!respond(parser_.request(), keep_alive) || !keep_alive) {
        return Outcome::Close;
      }
      parser_.reset();
    }
  }
  last_activity_ = Clock::now();
  return Outcome::KeepOpen;
}

bool Connection::respond(const http::Request &request, bool keep_alive) {
  if (options_.metrics != nullptr) {
    options_.metrics->request_received();
  }
  http::Response response;
  try {
    response = router_.dispatch(request);
  } catch (const std::exception &) {
    response = http::Response::error(http::Status::InternalServerError);
  }
  if (!keep_alive) {
    response.headers.set("Connection", "close");
  } else if (request.version_minor == 0) {
    response.headers.set("Connection", "keep-alive");
  }
  return write(response, request.method != "HEAD");
}

void Connection::reject(http::Status status) {
  if (options_.metrics != nullptr) {
    options_.metrics->request_received();
  }
  auto response = http::Response::error(status);
  response.headers.set("Connection", "close");
  if (!write(response, true)) {
    return;
  }
  // Half-close and briefly discard what the client is still sending, so the
  // kernel doesn't answer unread data with an RST that destroys our error
  // response before the client reads it.
  ::shutdown(socket_.fd(), SHUT_WR);
  std::array<char, 4096> sink{};
  const auto deadline = Clock::now() + std::chrono::milliseconds(200);
  std::size_t discarded = 0;
  while (discarded < std::size_t{256} * 1024 && Clock::now() < deadline &&
         wait_readable(socket_.fd(), std::chrono::milliseconds(50))) {
    const auto r = recv_some(socket_.fd(), sink.data(), sink.size());
    if (r.status != IoStatus::Ok) {
      break;
    }
    discarded += r.bytes;
  }
}

bool Connection::write(http::Response &response, bool include_body) {
  response.headers.set("Server", server_header());
  response.headers.set("Date", http::http_date(std::time(nullptr)));
  return send_all(socket_.fd(), response.serialize(include_body), options_.write_timeout);
}

} // namespace hearth::net
