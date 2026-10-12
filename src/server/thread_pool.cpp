#include "server/thread_pool.hpp"

#include <algorithm>
#include <exception>
#include <iostream>

namespace hearth::server {

ThreadPool::ThreadPool(std::size_t threads) {
  const std::size_t count = std::max<std::size_t>(threads, 1);
  workers_.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    workers_.emplace_back([this] { worker_loop(); });
  }
}

ThreadPool::~ThreadPool() { shutdown(); }

bool ThreadPool::submit(Task task) {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return false;
    }
    queue_.push_back(std::move(task));
  }
  ready_.notify_one();
  return true;
}

void ThreadPool::shutdown() {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  ready_.notify_all();
  for (auto &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

std::size_t ThreadPool::pending() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}

void ThreadPool::worker_loop() {
  for (;;) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
      if (queue_.empty()) {
        return; // stopping and fully drained
      }
      task = std::move(queue_.front());
      queue_.pop_front();
    }
    try {
      task();
    } catch (const std::exception &e) {
      std::cerr << "hearth: task threw: " << e.what() << '\n';
    } catch (...) {
      std::cerr << "hearth: task threw a non-standard exception\n";
    }
  }
}

} // namespace hearth::server
