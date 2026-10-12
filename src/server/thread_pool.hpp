#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace hearth::server {

/// Fixed-size pool of std::threads draining a mutex + condition_variable
/// protected FIFO queue.
class ThreadPool {
public:
  using Task = std::function<void()>;

  /// Starts `threads` workers (at least one).
  explicit ThreadPool(std::size_t threads);
  /// Calls shutdown().
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  /// Queues a task. Returns false (and drops the task) after shutdown().
  bool submit(Task task);

  /// Stops accepting tasks, runs everything already queued, joins workers.
  /// Must be called from a single owning thread; repeated calls are no-ops.
  void shutdown();

  [[nodiscard]] std::size_t size() const noexcept { return workers_.size(); }
  [[nodiscard]] std::size_t pending() const;

private:
  void worker_loop();

  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<Task> queue_;
  bool stopping_ = false;
  std::vector<std::thread> workers_;
};

} // namespace hearth::server
