#include "condvar.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Один поток ждёт, другой будит через NotifyOne.
static void BM_SingleWaiterNotifyOne(benchmark::State &state) {
  for (auto _ : state) {
    ConditionVariable cv;
    std::mutex m;
    bool ready = false;

    std::thread worker([&] {
      std::unique_lock<std::mutex> lock(m);
      cv.Wait(lock, [&] { return ready; });
    });

    std::this_thread::sleep_for(1ms);

    {
      std::lock_guard<std::mutex> lock(m);
      ready = true;
    }
    cv.NotifyOne();

    worker.join();
  }
}
BENCHMARK(BM_SingleWaiterNotifyOne);

// Несколько потоков ждут и пробуждаются через NotifyAll.
static void BM_ManyWaitersNotifyAll(benchmark::State &state) {
  const int threadsCount = static_cast<int>(state.range(0));

  for (auto _ : state) {
    ConditionVariable cv;
    std::mutex m;
    bool ready = false;
    std::atomic<int> woken{0};

    std::vector<std::thread> workers;
    workers.reserve(threadsCount);

    for (int i = 0; i < threadsCount; ++i) {
      workers.emplace_back([&] {
        std::unique_lock<std::mutex> lock(m);
        cv.Wait(lock, [&] { return ready; });
        woken.fetch_add(1, std::memory_order_relaxed);
      });
    }

    std::this_thread::sleep_for(1ms);

    {
      std::lock_guard<std::mutex> lock(m);
      ready = true;
    }
    cv.NotifyAll();

    for (auto &t : workers) {
      t.join();
    }

    benchmark::DoNotOptimize(woken.load());
  }
}
BENCHMARK(BM_ManyWaitersNotifyAll)->RangeMultiplier(2)->Range(1, 64);

// Несколько потоков ждут, но пробуждается 1
static void BM_ManyWaitersNotifyOne(benchmark::State &state) {
  const int threadsCount = static_cast<int>(state.range(0));
  for (auto _ : state) {
    ConditionVariable cv;
    std::mutex m;
    bool ready = false;
    std::atomic<int> woken{0};

    std::vector<std::thread> workers;
    for (int i = 0; i < threadsCount; ++i) {
      workers.emplace_back([&] {
        std::unique_lock lock(m);
        cv.Wait(lock, [&] { return ready; });
        woken.fetch_add(1, std::memory_order_relaxed);
      });
    }

    std::this_thread::sleep_for(1ms);

    {
      std::lock_guard lock(m);
      ready = true;
    }

    state.ResumeTiming();
    while (woken.load(std::memory_order_relaxed) < threadsCount) {
      cv.NotifyOne();
      std::this_thread::yield();
    }
    state.PauseTiming();

    for (auto &t : workers)
      t.join();

    state.ResumeTiming();
  }
}
BENCHMARK(BM_ManyWaitersNotifyOne)->RangeMultiplier(2)->Range(1, 64);

BENCHMARK_MAIN();