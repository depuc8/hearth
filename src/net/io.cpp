#include "net/io.hpp"

#include <cerrno>

#include <poll.h>
#include <sys/socket.h>

namespace hearth::net {

namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

bool would_block(int err) noexcept {
  return err == EAGAIN || err == EWOULDBLOCK;
}

bool wait_for(int fd, short events, std::chrono::milliseconds timeout) noexcept {
  pollfd pfd{fd, events, 0};
  for (;;) {
    const int rc = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (rc < 0 && errno == EINTR) {
      continue;
    }
    return rc > 0;
  }
}

} // namespace

IoResult recv_some(int fd, char *buffer, std::size_t length) noexcept {
  for (;;) {
    const auto n = ::recv(fd, buffer, length, 0);
    if (n > 0) {
      return {IoStatus::Ok, static_cast<std::size_t>(n)};
    }
    if (n == 0) {
      return {IoStatus::Closed, 0};
    }
    if (errno == EINTR) {
      continue;
    }
    return {would_block(errno) ? IoStatus::WouldBlock : IoStatus::Error, 0};
  }
}

bool send_all(int fd, std::string_view data, std::chrono::milliseconds timeout) noexcept {
  while (!data.empty()) {
    const auto n = ::send(fd, data.data(), data.size(), kSendFlags);
    if (n > 0) {
      data.remove_prefix(static_cast<std::size_t>(n));
      continue;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n < 0 && would_block(errno) && wait_for(fd, POLLOUT, timeout)) {
      continue;
    }
    return false;
  }
  return true;
}

bool wait_readable(int fd, std::chrono::milliseconds timeout) noexcept {
  return wait_for(fd, POLLIN, timeout);
}

} // namespace hearth::net
