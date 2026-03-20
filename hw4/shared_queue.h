#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

inline constexpr unsigned int kProtocolVersion = 1;
inline constexpr unsigned int kMagic = 0x4d535143;

struct alignas(64) QueueHeader {
  unsigned int magic;
  unsigned int version;
  std::size_t capacity_bytes;
  std::atomic<std::size_t> head;
  std::atomic<std::size_t> tail;
};

struct MessageHeader {
  std::atomic<unsigned int> ready;
  unsigned int type;
  unsigned int length;
};

class ProducerNode {
private:
  int fd_;
  void *addr_;
  size_t size_;
  QueueHeader *header_;
  std::byte *buffer_;
  size_t capacity_;

public:
  ProducerNode(std::string_view shm_name, size_t total_size_bytes)
      : fd_(-1), addr_(nullptr), size_(0), header_(nullptr), buffer_(nullptr),
        capacity_(0) {
    if (shm_name.empty() || shm_name[0] != '/') {
      throw std::invalid_argument("shm_name must start with '/'");
    }

    int fd = shm_open(shm_name.data(), O_RDWR | O_CREAT | O_EXCL, 0600);
    bool created = false;

    if (fd >= 0) {
      created = true;
      if (ftruncate(fd, static_cast<off_t>(total_size_bytes)) != 0) {
        ::close(fd);
        shm_unlink(shm_name.data());
        throw std::runtime_error("ftruncate failed");
      }
    } else {
      fd = shm_open(shm_name.data(), O_RDWR, 0600);
      if (fd < 0) {
        throw std::runtime_error("shm_open failed");
      }
    }

    struct stat st {};
    if (fstat(fd, &st) != 0) {
      ::close(fd);
      throw std::runtime_error("fstat failed");
    }

    size_t mapped_size = static_cast<size_t>(st.st_size);
    if (mapped_size < sizeof(QueueHeader) + 1024) {
      ::close(fd);
      throw std::runtime_error("shared memory too small");
    }

    void *addr =
        mmap(nullptr, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      ::close(fd);
      throw std::runtime_error("mmap failed");
    }

    fd_ = fd;
    addr_ = addr;
    size_ = mapped_size;

    header_ = static_cast<QueueHeader *>(addr_);
    buffer_ = static_cast<std::byte *>(addr_) + sizeof(QueueHeader);
    capacity_ = size_ - sizeof(QueueHeader);

    if (created) {
      std::memset(addr_, 0, size_);
      header_->magic = kMagic;
      header_->version = kProtocolVersion;
      header_->capacity_bytes = capacity_;
      header_->head.store(0, std::memory_order_relaxed);
      header_->tail.store(0, std::memory_order_relaxed);
    } else {
      if (header_->magic != kMagic) {
        throw std::runtime_error("bad magic");
      }
      if (header_->version != kProtocolVersion) {
        throw std::runtime_error("protocol version mismatch");
      }
      if (header_->capacity_bytes > capacity_) {
        throw std::runtime_error("queue capacity larger than mapping");
      }
      capacity_ = header_->capacity_bytes;
    }
  }

  ~ProducerNode() {
    if (addr_) {
      munmap(addr_, size_);
    }
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  void send(unsigned int type, const void *data, unsigned int length) {
    if (!data || length == 0) {
      throw std::invalid_argument("message length must be > 0");
    }

    const size_t payload_len = length;
    const size_t msg_size = sizeof(MessageHeader) + payload_len;
    if (msg_size > capacity_) {
      throw std::runtime_error("message too large");
    }

    while (true) {
      std::size_t head = header_->head.load(std::memory_order_acquire);
      std::size_t tail = header_->tail.load(std::memory_order_acquire);

      std::size_t used = tail - head;
      std::size_t free_bytes = capacity_ - used;

      size_t tail_pos = static_cast<size_t>(tail % capacity_);
      size_t to_end = capacity_ - tail_pos;

      if (free_bytes < msg_size || to_end < msg_size) {
        std::this_thread::yield();
        continue;
      }

      std::size_t new_tail = tail + msg_size;
      if (!header_->tail.compare_exchange_weak(tail, new_tail,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
        continue;
      }

      write_message_at(tail_pos, type, data, payload_len);
      return;
    }
  }

  void write_message_at(size_t pos, unsigned int type, const void *data,
                        size_t len) {
    auto *hdr = reinterpret_cast<MessageHeader *>(buffer_ + pos);

    hdr->ready.store(0, std::memory_order_relaxed);
    hdr->type = type;
    hdr->length = static_cast<unsigned int>(len);

    std::memcpy(buffer_ + pos + sizeof(MessageHeader), data, len);

    hdr->ready.store(1, std::memory_order_release);
  }
};

class ConsumerNode {
private:
  int fd_;
  void *addr_;
  size_t size_;
  QueueHeader *header_;
  std::byte *buffer_;
  size_t capacity_;

public:
  ConsumerNode(std::string_view shm_name, size_t total_size_bytes)
      : fd_(-1), addr_(nullptr), size_(0), header_(nullptr), buffer_(nullptr),
        capacity_(0) {
    (void)total_size_bytes;

    if (shm_name.empty() || shm_name[0] != '/') {
      throw std::invalid_argument("shm_name must start with '/'");
    }

    int fd = shm_open(shm_name.data(), O_RDWR, 0600);
    if (fd < 0) {
      throw std::runtime_error("shm_open failed");
    }

    struct stat st {};
    if (fstat(fd, &st) != 0) {
      ::close(fd);
      throw std::runtime_error("fstat failed");
    }

    size_t mapped_size = static_cast<size_t>(st.st_size);
    if (mapped_size < sizeof(QueueHeader) + 1024) {
      ::close(fd);
      throw std::runtime_error("shared memory too small");
    }

    void *addr =
        mmap(nullptr, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      ::close(fd);
      throw std::runtime_error("mmap failed");
    }

    fd_ = fd;
    addr_ = addr;
    size_ = mapped_size;

    header_ = static_cast<QueueHeader *>(addr_);
    buffer_ = static_cast<std::byte *>(addr_) + sizeof(QueueHeader);
    capacity_ = size_ - sizeof(QueueHeader);

    if (header_->magic != kMagic) {
      throw std::runtime_error("bad magic");
    }
    if (header_->version != kProtocolVersion) {
      throw std::runtime_error("protocol version mismatch");
    }
    if (header_->capacity_bytes > capacity_) {
      throw std::runtime_error("queue capacity larger than mapping");
    }
    capacity_ = header_->capacity_bytes;
  }

  ~ConsumerNode() {
    if (addr_) {
      munmap(addr_, size_);
    }
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  std::vector<std::byte> receive_of_type(unsigned int expected_type) {
    while (true) {
      std::size_t head = header_->head.load(std::memory_order_acquire);
      std::size_t tail = header_->tail.load(std::memory_order_acquire);

      if (head == tail) {
        std::this_thread::yield();
        continue;
      }

      size_t head_pos = static_cast<size_t>(head % capacity_);
      auto *hdr = reinterpret_cast<MessageHeader *>(buffer_ + head_pos);

      if (!hdr->ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
        continue;
      }

      unsigned int type = hdr->type;
      unsigned int len = hdr->length;
      size_t total_msg = sizeof(MessageHeader) + len;

      std::vector<std::byte> payload(len);
      std::memcpy(payload.data(), buffer_ + head_pos + sizeof(MessageHeader),
                  len);

      header_->head.store(head + total_msg, std::memory_order_release);

      if (type == expected_type) {
        return payload;
      }
    }
  }
};
