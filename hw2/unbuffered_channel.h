#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

template <class T> class UnbufferedChannel {
private:
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  bool closed_ = false;

  struct PendingSend {
    T value;
    bool completed = false;
    bool has_receiver = false;
  };

  struct PendingRecv {
    std::optional<T> value;
    bool completed = false;
  };

  std::queue<PendingSend *> senders_;
  std::queue<PendingRecv *> receivers_;

public:
  void Send(const T &value) {
    PendingSend pending;
    pending.value = value;
    std::unique_lock<std::mutex> lock(mtx_);

    if (closed_) {
      throw std::runtime_error("Channel closed");
    }

    if (!receivers_.empty()) {
      PendingRecv *receiver = receivers_.front();
      receiver->value = std::move(value);
      receiver->completed = true;
      receivers_.pop();
      pending.completed = true;
      pending.has_receiver = true;
      cv_.notify_all();
      return;
    }

    PendingSend *ptr = &pending;
    senders_.push(ptr);

    cv_.wait(lock, [ptr] { return ptr->completed; });
  }

  std::optional<T> Recv() {
    PendingRecv pending;
    PendingRecv *ptr = &pending;

    std::unique_lock<std::mutex> lock(mtx_);

    if (closed_ && senders_.empty()) {
      return std::nullopt;
    }

    if (!senders_.empty()) {
      PendingSend *sender = senders_.front();
      pending.value = sender->value;
      pending.completed = true;
      sender->completed = true;
      sender->has_receiver = true;
      senders_.pop();
      cv_.notify_all();
      return std::move(pending.value);
    }

    receivers_.push(ptr);

    cv_.wait(lock, [ptr, this] { return ptr->completed || closed_; });

    if (closed_ && !ptr->completed) {
      return std::nullopt;
    }

    return ptr->value;
  }

  void Close() {
    std::unique_lock<std::mutex> lock(mtx_);
    closed_ = true;
    cv_.notify_all();
  }
};