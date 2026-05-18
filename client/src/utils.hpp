#include <string>

std::string generate_uuid();

struct Dimensions {
  int width;
  int height;
};

Dimensions getValidDimensions(int w, int h);
