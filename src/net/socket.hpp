#pragma once

#include <string_view>
#include <system_error>

namespace hearth::net {

/// Builds a std::system_error from the current errno, prefixed with `what`.
std::system_error errno_error(std::string_view what);

/// Move-only owner of a file descriptor. Closes it on destruction.
class Socket {
public:
  Socket() noexcept = default;
  explicit Socket(int fd) noexcept : fd_(fd) {}
  ~Socket();

  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;
  Socket(Socket &&other) noexcept;
  Socket &operator=(Socket &&other) noexcept;

  [[nodiscard]] int fd() const noexcept { return fd_; }
  [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
  explicit operator bool() const noexcept { return valid(); }

  /// Gives up ownership without closing.
  int release() noexcept;
  /// Closes the current descriptor (if any) and takes ownership of `fd`.
  void reset(int fd = -1) noexcept;

  void set_nonblocking(bool enabled) const;
  void set_cloexec() const;
  void set_nodelay(bool enabled) const;
  /// Prevents SIGPIPE on writes to a closed peer where the platform supports
  /// a per-socket option (macOS). Linux uses MSG_NOSIGNAL instead.
  void set_nosigpipe() const;

private:
  int fd_ = -1;
};

/// Self-pipe used to wake a thread blocked in poll() from another thread.
class WakePipe {
public:
  WakePipe();

  [[nodiscard]] int read_fd() const noexcept { return read_.fd(); }
  /// Async-signal-safe and thread-safe; never blocks.
  void notify() const noexcept;
  /// Consumes all pending notifications.
  void drain() const noexcept;

private:
  Socket read_;
  Socket write_;
};

} // namespace hearth::net
