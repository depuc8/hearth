#include "net/io.hpp"
#include "net/listener.hpp"
#include "net/socket.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std::chrono_literals;
using hearth::net::Listener;
using hearth::net::Socket;

namespace {

bool fd_is_open(int fd) {
  return ::fcntl(fd, F_GETFD) != -1;
}

Socket connect_loopback(std::uint16_t port) {
  Socket s(::socket(AF_INET, SOCK_STREAM, 0));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  EXPECT_EQ(::connect(s.fd(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);
  return s;
}

} // namespace

TEST(Socket, ClosesDescriptorOnDestruction) {
  int fd = -1;
  {
    Socket s(::socket(AF_INET, SOCK_STREAM, 0));
    fd = s.fd();
    ASSERT_TRUE(fd_is_open(fd));
  }
  EXPECT_FALSE(fd_is_open(fd));
}

TEST(Socket, MoveTransfersOwnership) {
  Socket a(::socket(AF_INET, SOCK_STREAM, 0));
  const int fd = a.fd();
  Socket b(std::move(a));
  EXPECT_FALSE(a.valid()); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(b.fd(), fd);
  Socket c;
  c = std::move(b);
  EXPECT_EQ(c.fd(), fd);
  EXPECT_TRUE(fd_is_open(fd));
}

TEST(Socket, ReleaseDoesNotClose) {
  Socket s(::socket(AF_INET, SOCK_STREAM, 0));
  const int fd = s.release();
  EXPECT_FALSE(s.valid());
  EXPECT_TRUE(fd_is_open(fd));
  ::close(fd);
}

TEST(Listener, BindsEphemeralPortAndAccepts) {
  Listener listener("127.0.0.1", 0);
  ASSERT_NE(listener.port(), 0);
  EXPECT_FALSE(listener.accept().has_value()); // nothing pending, no block

  Socket client = connect_loopback(listener.port());
  ASSERT_TRUE(hearth::net::wait_readable(listener.fd(), 2000ms));
  auto accepted = listener.accept();
  ASSERT_TRUE(accepted.has_value());

  ASSERT_TRUE(hearth::net::send_all(client.fd(), "ping", 1000ms));
  ASSERT_TRUE(hearth::net::wait_readable(accepted->fd(), 2000ms));
  char buf[8];
  const auto r = hearth::net::recv_some(accepted->fd(), buf, sizeof(buf));
  ASSERT_EQ(r.status, hearth::net::IoStatus::Ok);
  EXPECT_EQ(std::string(buf, r.bytes), "ping");

  client.reset();
  ASSERT_TRUE(hearth::net::wait_readable(accepted->fd(), 2000ms));
  EXPECT_EQ(hearth::net::recv_some(accepted->fd(), buf, sizeof(buf)).status,
            hearth::net::IoStatus::Closed);
}

TEST(Listener, RejectsUnresolvableHost) {
  EXPECT_ANY_THROW(Listener("no-such-host.invalid", 0));
}

TEST(WakePipe, NotifyMakesReadEndReadable) {
  hearth::net::WakePipe pipe;
  EXPECT_FALSE(hearth::net::wait_readable(pipe.read_fd(), 0ms));
  pipe.notify();
  pipe.notify();
  EXPECT_TRUE(hearth::net::wait_readable(pipe.read_fd(), 0ms));
  pipe.drain();
  EXPECT_FALSE(hearth::net::wait_readable(pipe.read_fd(), 0ms));
}
