#include "net/listener.hpp"

#include <cerrno>
#include <memory>
#include <stdexcept>
#include <string>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace hearth::net {

namespace {

constexpr int kBacklog = 1024;

std::uint16_t bound_port(int fd) {
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);
  if (::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) < 0) {
    throw errno_error("getsockname");
  }
  if (addr.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<const sockaddr_in6 *>(&addr)->sin6_port);
  }
  return ntohs(reinterpret_cast<const sockaddr_in *>(&addr)->sin_port);
}

} // namespace

Listener::Listener(const std::string &host, std::uint16_t port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo *raw = nullptr;
  const std::string service = std::to_string(port);
  const int rc = ::getaddrinfo(host.empty() ? nullptr : host.c_str(),
                               service.c_str(), &hints, &raw);
  if (rc != 0) {
    throw std::runtime_error("cannot resolve '" + host +
                             "': " + ::gai_strerror(rc));
  }
  const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> results(
      raw, &::freeaddrinfo);

  int last_errno = 0;
  for (const addrinfo *ai = results.get(); ai != nullptr; ai = ai->ai_next) {
    Socket candidate(::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
    if (!candidate) {
      last_errno = errno;
      continue;
    }
    const int yes = 1;
    ::setsockopt(candidate.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (::bind(candidate.fd(), ai->ai_addr, ai->ai_addrlen) == 0 &&
        ::listen(candidate.fd(), kBacklog) == 0) {
      socket_ = std::move(candidate);
      break;
    }
    last_errno = errno;
  }
  if (!socket_) {
    errno = last_errno;
    throw errno_error("bind " + host + ":" + service);
  }
  socket_.set_nonblocking(true);
  socket_.set_cloexec();
  port_ = bound_port(socket_.fd());
}

std::optional<Socket> Listener::accept() const {
  for (;;) {
    Socket client(::accept(socket_.fd(), nullptr, nullptr));
    if (client) {
      client.set_nonblocking(true);
      client.set_cloexec();
      client.set_nosigpipe();
      return client;
    }
    switch (errno) {
    case EINTR:
    case ECONNABORTED:
      continue;
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
    // Out of descriptors: leave the connection in the backlog and retry on
    // the next loop iteration instead of crashing the server.
    case EMFILE:
    case ENFILE:
      return std::nullopt;
    default:
      throw errno_error("accept");
    }
  }
}

} // namespace hearth::net
