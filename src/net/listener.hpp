#pragma once

#include "net/socket.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace hearth::net {

/// A non-blocking TCP listening socket.
class Listener {
public:
  /// Resolves `host` (numeric or name, IPv4 or IPv6) and binds to `port`.
  /// Port 0 asks the kernel for an ephemeral port; see port().
  Listener(const std::string &host, std::uint16_t port);

  [[nodiscard]] int fd() const noexcept { return socket_.fd(); }
  /// The actual bound port.
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

  /// Accepts one pending connection. Returns nullopt when none is pending.
  /// The returned socket is non-blocking and close-on-exec.
  std::optional<Socket> accept() const;

  void close() noexcept { socket_.reset(); }

private:
  Socket socket_;
  std::uint16_t port_ = 0;
};

} // namespace hearth::net
