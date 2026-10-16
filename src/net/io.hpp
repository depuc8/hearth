#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>

namespace hearth::net {

enum class IoStatus { Ok, WouldBlock, Closed, Error };

struct IoResult {
  IoStatus status;
  std::size_t bytes;
};

/// One recv() on a non-blocking socket, retrying on EINTR.
IoResult recv_some(int fd, char *buffer, std::size_t length) noexcept;

/// Writes all of `data` to a non-blocking socket, waiting up to `timeout`
/// for the socket to become writable each time the kernel buffer is full.
/// Returns false if the peer went away, an error occurred or it timed out.
bool send_all(int fd, std::string_view data, std::chrono::milliseconds timeout) noexcept;

/// Waits until `fd` is readable (or hung up). False on timeout.
bool wait_readable(int fd, std::chrono::milliseconds timeout) noexcept;

} // namespace hearth::net
