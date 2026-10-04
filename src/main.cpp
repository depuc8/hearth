// Phase-2 bootstrap: a single-threaded server that answers every request with
// a fixed response. Replaced by the full server in later phases.
#include "net/io.hpp"
#include "net/listener.hpp"
#include "version.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);
  const auto port =
      static_cast<std::uint16_t>(argc > 1 ? std::atoi(argv[1]) : 8080);
  hearth::net::Listener listener("0.0.0.0", port);
  std::cout << "hearth " << hearth::version() << " listening on port "
            << listener.port() << std::endl;

  constexpr std::string_view kResponse = "HTTP/1.1 200 OK\r\n"
                                         "Content-Type: text/plain\r\n"
                                         "Content-Length: 14\r\n"
                                         "Connection: close\r\n"
                                         "\r\n"
                                         "Hello, hearth\n";
  using namespace std::chrono_literals;
  for (;;) {
    if (!hearth::net::wait_readable(listener.fd(), 1000ms)) {
      continue;
    }
    while (auto client = listener.accept()) {
      std::string request;
      char buffer[4096];
      while (request.find("\r\n\r\n") == std::string::npos &&
             hearth::net::wait_readable(client->fd(), 2000ms)) {
        const auto r = hearth::net::recv_some(client->fd(), buffer, sizeof(buffer));
        if (r.status != hearth::net::IoStatus::Ok) {
          break;
        }
        request.append(buffer, r.bytes);
      }
      hearth::net::send_all(client->fd(), kResponse, 2000ms);
    }
  }
}
