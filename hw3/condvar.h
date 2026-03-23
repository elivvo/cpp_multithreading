#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>

#include <atomic>
#include <climits>
#include <mutex>
#include <unistd.h>

inline void FutexWait(void *value, int expectedValue) {
  syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, expectedValue, nullptr, nullptr,
          0);
}

inline void FutexWake(void *value, int count) {
  syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
}

class ConditionVariable {
private:
  std::atomic<int> seq_{0};

public:
  ConditionVariable() = default;
  ConditionVariable(const ConditionVariable &) = delete;
  ConditionVariable &operator=(const ConditionVariable &) = delete;

  template <class Lock> void Wait(Lock &lock) {
    int old = seq_.load(std::memory_order_acquire);

    lock.unlock();
    FutexWait(&seq_, old);
    lock.lock();
  }

  template <class Lock, class Predicate> void Wait(Lock &lock, Predicate pred) {
    while (!pred()) {
      Wait(lock);
    }
  }

  void NotifyOne() noexcept {
    seq_.fetch_add(1, std::memory_order_release);
    FutexWake(&seq_, 1);
  }

  void NotifyAll() noexcept {
    seq_.fetch_add(1, std::memory_order_release);
    FutexWake(&seq_, INT_MAX);
  }
};