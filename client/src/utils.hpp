#include <iostream>
#include <string>

#define DEBUG_MSG(...)                                                         \
  do {                                                                         \
    std::cerr << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " " << __func__ \
              << " ";                                                          \
    std::cerr << __VA_ARGS__ << std::endl;                                     \
    std::cerr.flush();                                                         \
  } while (0)

std::string generate_uuid();
