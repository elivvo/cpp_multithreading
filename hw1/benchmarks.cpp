#include "func.h"
#include <benchmark/benchmark.h>
#include <cmath>
#include <numeric>
#include <vector>

// Простое вычисление
static void Easy_ST(benchmark::State &state) {
  std::vector<int> data(state.range(0));
  std::iota(data.begin(), data.end(), 0);

  for (auto _ : state) {
    ApplyFunction<int>(data, [](int &val) { val = val + 1; }, 1);
  }
}
BENCHMARK(Easy_ST)->RangeMultiplier(2)->Range(1 << 10, 1 << 16);

static void Easy_MT(benchmark::State &state) {
  std::vector<int> data(state.range(0));
  std::iota(data.begin(), data.end(), 0);
  const int threads = 4;

  for (auto _ : state) {
    ApplyFunction<int>(data, [](int &val) { val = val + 1; }, threads);
  }
}
BENCHMARK(Easy_MT)->RangeMultiplier(2)->Range(1 << 10, 1 << 16);

// Сложное вычисление
static void Hard_ST(benchmark::State &state) {
  std::vector<double> data(state.range(0));
  std::iota(data.begin(), data.end(), 0.0);

  for (auto _ : state) {
    ApplyFunction<double>(
        data,
        [](double &val) {
          volatile double res = 0.0;
          for (int i = 0; i < 1000; ++i) {
            res += std::sin(val) * std::cos(val);
          }
          val = res;
        },
        1);
  }
}
BENCHMARK(Hard_ST)->RangeMultiplier(2)->Range(1 << 10, 1 << 14);

static void Hard_MT(benchmark::State &state) {
  std::vector<double> data(state.range(0));
  std::iota(data.begin(), data.end(), 0.0);
  const int threads = 4;

  for (auto _ : state) {
    ApplyFunction<double>(
        data,
        [](double &val) {
          volatile double res = 0.0;
          for (int i = 0; i < 1000; ++i) {
            res += std::sin(val) * std::cos(val);
          }
          val = res;
        },
        threads);
  }
}
BENCHMARK(Hard_MT)->RangeMultiplier(2)->Range(1 << 10, 1 << 14);

BENCHMARK_MAIN();