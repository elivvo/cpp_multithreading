#include "func.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>
#include <vector>

// Простая арифметика (умножение на 2)
TEST(ApplyFunctionTest, EasyArithmetic) {
  std::vector<int> data = {1, 2, 3, 4, 5};

  ApplyFunction<int>(data, [](int &val) { val *= 2; }, 1);

  std::vector<int> expected = {2, 4, 6, 8, 10};
  EXPECT_EQ(data, expected);
}

// Корректность данных при многопоточности
TEST(ApplyFunctionTest, MultiThreadedCorrectness) {
  const int size = 1000;
  std::vector<int> data(size);
  std::iota(data.begin(), data.end(), 0);

  ApplyFunction<int>(data, [](int &val) { val += 10; }, 4);

  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(data[i], i + 10);
  }
}

// Потоков больше чем элементов
TEST(ApplyFunctionTest, TooManyThreads) {
  std::vector<int> data = {1, 2, 3};

  ApplyFunction<int>(data, [](int &val) { val = 100; }, 10);

  for (int val : data) {
    EXPECT_EQ(val, 100);
  }
}

// Пустой вектор
TEST(ApplyFunctionTest, EmptyVector) {
  std::vector<int> data;
  ApplyFunction<int>(data, [](int &val) { val = 1; }, 5);

  EXPECT_TRUE(data.empty());
}

// Много потоков на один элемент
TEST(ApplyFunctionTest, OneElementManyThreads) {
  std::vector<int> data = {42};
  ApplyFunction<int>(data, [](int &val) { val = 99; }, 5);

  EXPECT_EQ(data[0], 99);
}

// 0 потоков
TEST(ApplyFunctionTest, ZeroThreads) {
  std::vector<int> data = {1, 2};
  ApplyFunction<int>(data, [](int &val) { val++; }, 0);

  EXPECT_EQ(data[0], 2);
  EXPECT_EQ(data[1], 3);
}

// Работа с другим типом
TEST(ApplyFunctionTest, TypeDouble) {
  std::vector<double> data = {1.5, 2.5, 3.5};
  ApplyFunction<double>(data, [](double &val) { val *= 2.0; }, 2);

  EXPECT_DOUBLE_EQ(data[0], 3.0);
  EXPECT_DOUBLE_EQ(data[1], 5.0);
  EXPECT_DOUBLE_EQ(data[2], 7.0);
}