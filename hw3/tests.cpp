#include "condvar.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

static constexpr auto k10ms = std::chrono::milliseconds(10);
static constexpr auto k20ms = std::chrono::milliseconds(20);
static constexpr auto k50ms = std::chrono::milliseconds(50);
static constexpr auto k100ms = std::chrono::milliseconds(100);

// Один поток ждёт, другой будит через NotifyOne
TEST(ConditionVariableTest, SingleWaiterNotifyOne) {
  ConditionVariable cv;
  std::mutex m;
  bool ready = false;
  bool proceeded = false;

  std::thread worker([&] {
    std::unique_lock<std::mutex> lock(m);
    cv.Wait(lock, [&] { return ready; });
    proceeded = true;
  });

  std::this_thread::sleep_for(k10ms);

  {
    std::lock_guard<std::mutex> lock(m);
    ready = true;
  }
  cv.NotifyOne();

  worker.join();

  EXPECT_TRUE(proceeded);
}

// Несколько потоков ждут и просыпаются через NotifyAll
TEST(ConditionVariableTest, MultipleWaitersNotifyAll) {
  ConditionVariable cv;
  std::mutex m;
  bool ready = false;
  constexpr int kThreads = 8;
  std::atomic<int> woken{0};

  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&] {
      std::unique_lock<std::mutex> lock(m);
      cv.Wait(lock, [&] { return ready; });
      woken.fetch_add(1, std::memory_order_relaxed);
    });
  }

  std::this_thread::sleep_for(k10ms);

  {
    std::lock_guard<std::mutex> lock(m);
    ready = true;
  }
  cv.NotifyAll();

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(woken.load(), kThreads);
}

// NotifyOne будит только одного ожидающего
TEST(ConditionVariableTest, NotifyOneWakesSingleWaiter) {
  ConditionVariable cv;
  std::mutex m;
  bool ready = false;
  constexpr int kThreads = 4;
  std::atomic<int> woken{0};

  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&] {
      std::unique_lock<std::mutex> lock(m);
      cv.Wait(lock, [&] { return ready; });
      woken.fetch_add(1, std::memory_order_relaxed);
    });
  }

  std::this_thread::sleep_for(k10ms);

  {
    std::lock_guard<std::mutex> lock(m);
    ready = true;
  }
  cv.NotifyOne();

  std::this_thread::sleep_for(k20ms);
  EXPECT_GE(woken.load(), 1);

  cv.NotifyAll();
  for (auto &t : threads) {
    t.join();
  }
}

// Ложное пробуждение
TEST(ConditionVariableTest, PredicateAlreadyTrue) {
  ConditionVariable cv;
  std::mutex m;
  bool condition = true;
  bool entered = false;

  std::thread worker([&] {
    std::unique_lock<std::mutex> lock(m);
    entered = true;
    cv.Wait(lock, [&] { return condition; });
  });

  worker.join();

  EXPECT_TRUE(entered);
}

// Notify до Wait
TEST(ConditionVariableTest, NotifyBeforeWait) {
  ConditionVariable cv;
  std::mutex m;
  bool notified = false;
  bool waited = false;

  {
    std::lock_guard<std::mutex> lock(m);
    notified = true;
    cv.NotifyOne();
  }

  std::this_thread::sleep_for(k10ms);

  std::thread waiter([&] {
    std::unique_lock<std::mutex> lock(m);
    waited = true;
    cv.Wait(lock, [&] { return notified; });
  });

  waiter.join();

  EXPECT_TRUE(notified);
  EXPECT_TRUE(waited);
}

// Несколько уведомлений подряд
TEST(ConditionVariableTest, MultipleNotifiesSingleWake) {
  ConditionVariable cv;
  std::mutex m;
  bool ready = false;
  std::atomic<int> wake_count{0};

  std::thread waiter([&] {
    std::unique_lock<std::mutex> lock(m);
    cv.Wait(lock, [&] { return ready; });
    wake_count.fetch_add(1, std::memory_order_relaxed);
  });

  std::this_thread::sleep_for(k10ms);

  {
    std::lock_guard<std::mutex> lock(m);
    ready = true;
  }

  for (int i = 0; i < 5; ++i) {
    cv.NotifyOne();
  }

  waiter.join();

  EXPECT_EQ(wake_count.load(), 1);
}

// Мьютекс освобождается во время ожидания
TEST(ConditionVariableTest, MutexReleasedDuringWait) {
  ConditionVariable cv;
  std::mutex m;
  bool can_proceed = false;
  std::atomic<bool> other_entered{false};

  std::thread waiter([&] {
    std::unique_lock<std::mutex> lock(m);
    cv.Wait(lock, [&] { return can_proceed; });
  });

  std::thread checker([&] {
    std::this_thread::sleep_for(k10ms);
    std::lock_guard<std::mutex> lock(m);
    other_entered.store(true, std::memory_order_relaxed);
  });

  std::this_thread::sleep_for(k20ms);

  {
    std::lock_guard<std::mutex> lock(m);
    can_proceed = true;
  }
  cv.NotifyOne();

  waiter.join();
  checker.join();

  EXPECT_TRUE(other_entered.load());
}

// Быстрая последовательность notify/wait
TEST(ConditionVariableTest, RapidNotifyWaitSequence) {
  ConditionVariable cv;
  std::mutex m;
  std::atomic<int> counter{0};
  constexpr int kIterations = 20;

  std::thread worker([&] {
    for (int i = 0; i < kIterations; ++i) {
      std::unique_lock<std::mutex> lock(m);
      cv.Wait(lock, [&] { return counter.load() > i; });
    }
  });

  for (int i = 0; i < kIterations; ++i) {
    std::this_thread::sleep_for(k10ms);
    counter.fetch_add(1, std::memory_order_relaxed);
    cv.NotifyOne();
  }

  worker.join();

  EXPECT_EQ(counter.load(), kIterations);
}