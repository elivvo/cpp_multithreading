#include "shared_queue.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

constexpr unsigned int kTextMessageType = 1;

static void usage(const char *prog) {
  std::fprintf(
      stderr, "Usage: %s <shm_name> <total_size_bytes> [max_messages]\n", prog);
  std::fprintf(stderr, "Example: %s /my_queue 1048576 10\n", prog);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    usage(argv[0]);
    return 1;
  }

  std::string shm_name = argv[1];
  size_t total_size = std::strtoull(argv[2], nullptr, 10);
  int max_messages = (argc >= 4) ? std::atoi(argv[3]) : 0;

  ConsumerNode consumer(shm_name, total_size);

  int received = 0;
  while (max_messages == 0 || received < max_messages) {
    std::vector<std::byte> payload = consumer.receive_of_type(kTextMessageType);
    std::string text(reinterpret_cast<const char *>(payload.data()),
                     payload.size());
    std::printf("Consumer: got \"%s\"\n", text.c_str());
    ++received;
  }

  return 0;
}
