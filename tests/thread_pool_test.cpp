#include "server/thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

using hearth::server::ThreadPool;
using namespace std::chrono_literals;

TEST(ThreadPool, RunsEveryTask) {
  std::atomic<int> count{0};
  {
    ThreadPool pool(4);
    for (int i = 0; i < 1000; ++i) {
      ASSERT_TRUE(pool.submit([&] { count.fetch_add(1); }));
    }
  } // destructor drains the queue
  EXPECT_EQ(count.load(), 1000);
}

TEST(ThreadPool, UsesMultipleThreads) {
  ThreadPool pool(4);
  EXPECT_EQ(pool.size(), 4U);
  std::mutex mutex;
  std::set<std::thread::id> seen;
  std::atomic<int> arrived{0};
  for (int i = 0; i < 4; ++i) {
    pool.submit([&] {
      {
        const std::scoped_lock lock(mutex);
        seen.insert(std::this_thread::get_id());
      }
      arrived.fetch_add(1);
      // Hold each worker until all four tasks are running at once.
      const auto deadline = std::chrono::steady_clock::now() + 5s;
      while (arrived.load() < 4 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
      }
    });
  }
  pool.shutdown();
  EXPECT_EQ(seen.size(), 4U);
}

TEST(ThreadPool, SubmitAfterShutdownIsRejected) {
  ThreadPool pool(2);
  pool.shutdown();
  EXPECT_FALSE(pool.submit([] {}));
  pool.shutdown(); // idempotent
}

TEST(ThreadPool, ShutdownFinishesQueuedWork) {
  std::atomic<int> count{0};
  ThreadPool pool(1);
  for (int i = 0; i < 50; ++i) {
    pool.submit([&] {
      std::this_thread::sleep_for(100us);
      count.fetch_add(1);
    });
  }
  pool.shutdown();
  EXPECT_EQ(count.load(), 50);
  EXPECT_EQ(pool.pending(), 0U);
}

TEST(ThreadPool, ThrowingTaskDoesNotKillWorker) {
  std::atomic<int> count{0};
  {
    ThreadPool pool(1);
    pool.submit([] { throw std::runtime_error("boom"); });
    pool.submit([&] { count.fetch_add(1); });
  }
  EXPECT_EQ(count.load(), 1);
}

TEST(ThreadPool, ZeroThreadsMeansOne) {
  ThreadPool pool(0);
  EXPECT_EQ(pool.size(), 1U);
}
