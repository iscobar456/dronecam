#include "utils.hpp"
#include <string>
#include <uuid/uuid.h>

std::string generate_uuid() {
  uuid_t uuid;
  char cname[37];

  uuid_generate(uuid);
  uuid_unparse(uuid, cname);

  return std::string(cname);
}

Dimensions getValidDimensions(int w, int h) {
  Dimensions d{};
  if (std::string(PLATFORM) == "RPI") {
    d.width = w - (w % 16);
    d.height = h - (h % 16);
  } else {
    if (w >= 1920) {
      d.width = 1920;
      d.height = 1080;
    } else if (w >= 1280) {
      d.width = 1280;
      d.height = 720;
    } else if (w >= 640) {
      d.width = 640;
      if (h >= 480) {
        d.height = 480;
      } else {
        d.height = 360;
      }
    } else {
      d.width = 320;
      d.height = 240;
    }
  }
  return d;
}
