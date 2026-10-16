#include "net/socket.hpp"

#include <cerrno>
#include <string>
#include <utility>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace hearth::net {

std::system_error errno_error(std::string_view what) {
  return {errno, std::generic_category(), std::string(what)};
}

Socket::~Socket() {
  reset();
}

Socket::Socket(Socket &&other) noexcept : fd_(other.release()) {}

Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    reset(other.release());
  }
  return *this;
}

int Socket::release() noexcept {
  return std::exchange(fd_, -1);
}

void Socket::reset(int fd) noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
  }
  fd_ = fd;
}

void Socket::set_nonblocking(bool enabled) const {
  const int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0) {
    throw errno_error("fcntl(F_GETFL)");
  }
  // fcntl's flag word is an int by POSIX definition.
  const int updated = enabled ? (flags | O_NONBLOCK)   // NOLINT(bugprone-signed-bitwise)
                              : (flags & ~O_NONBLOCK); // NOLINT(bugprone-signed-bitwise)
  if (::fcntl(fd_, F_SETFL, updated) < 0) {
    throw errno_error("fcntl(F_SETFL)");
  }
}

void Socket::set_cloexec() const {
  if (::fcntl(fd_, F_SETFD, FD_CLOEXEC) < 0) {
    throw errno_error("fcntl(F_SETFD)");
  }
}

bool Socket::set_nodelay(bool enabled) const noexcept {
  const int value = enabled ? 1 : 0;
  return ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value)) == 0;
}

void Socket::set_nosigpipe() const {
#ifdef SO_NOSIGPIPE
  const int value = 1;
  if (::setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value)) < 0) {
    throw errno_error("setsockopt(SO_NOSIGPIPE)");
  }
#endif
}

WakePipe::WakePipe() {
  int fds[2] = {-1, -1};
  if (::pipe(fds) < 0) {
    throw errno_error("pipe");
  }
  read_.reset(fds[0]);
  write_.reset(fds[1]);
  for (const Socket *end : {&read_, &write_}) {
    end->set_nonblocking(true);
    end->set_cloexec();
  }
}

void WakePipe::notify() const noexcept {
  const char byte = 1;
  // A full pipe already guarantees a pending wakeup, so EAGAIN is fine.
  [[maybe_unused]] const auto written = ::write(write_.fd(), &byte, 1);
}

void WakePipe::drain() const noexcept {
  char buffer[64];
  while (::read(read_.fd(), buffer, sizeof(buffer)) > 0) {
  }
}

} // namespace hearth::net
