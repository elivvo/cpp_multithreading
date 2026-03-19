#include "shared_queue.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

constexpr unsigned int kTextMessageType = 1;

static void usage(const char *prog) {
  std::fprintf(stderr,
               "Usage: %s <shm_name> <total_size_bytes> <message_prefix> "
               "[count]\n",
               prog);
  std::fprintf(stderr, "Example: %s /my_queue 1048576 hello 10\n", prog);
}

int main(int argc, char **argv) {
  if (argc < 4) {
    usage(argv[0]);
    return 1;
  }

  std::string shm_name = argv[1];
  size_t total_size = std::strtoull(argv[2], nullptr, 10);
  std::string prefix = argv[3];
  int count = (argc >= 5) ? std::atoi(argv[4]) : 10;

  ProducerNode producer(shm_name, total_size);

  for (int i = 0; i < count; ++i) {
    std::string msg = prefix + " #" + std::to_string(i);
    producer.send(kTextMessageType, msg.data(),
                  static_cast<unsigned int>(msg.size()));
    std::printf("Producer: sent \"%s\"\n", msg.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return 0;
}
