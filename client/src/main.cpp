#include "ConnectionManager.hpp"
#include "Streamer.hpp"

int main() {
  Streamer *streamer = new Streamer();
  ConnectionManager *connMan = new ConnectionManager(streamer);
  connMan->setSsrc(streamer->getSsrc());
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
