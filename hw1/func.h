#pragma once

#include <algorithm>
#include <functional>
#include <thread>
#include <vector>

template <typename T>
void ApplyFunction(std::vector<T> &data,
                   const std::function<void(T &)> &transform, int threadCount) {
  if (data.empty())
    return;

  const size_t n = data.size();
  if (threadCount <= 0)
    threadCount = 1;
  if (static_cast<size_t>(threadCount) > n) {
    threadCount = static_cast<int>(n);
  }

  if (threadCount == 1) {
    for (auto &item : data) {
      transform(item);
    }
    return;
  }

  std::vector<std::thread> threads;
  threads.reserve(threadCount);

  const size_t chunkSize = (n + threadCount - 1) / threadCount;

  for (int i = 0; i < threadCount; ++i) {
    const size_t start = i * chunkSize;
    if (start >= n)
      break;

    const size_t end = std::min(start + chunkSize, n);

    threads.emplace_back([&data, &transform, start, end]() {
      for (size_t j = start; j < end; ++j) {
        transform(data[j]);
      }
    });
  }

  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}