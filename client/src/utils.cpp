#include "utils.hpp"
#include <string>
#include <uuid/uuid.h>

std::string generate_uuid() {
  uuid_t uuid;
  char cname[37];

  uuid_generate(uuid);
  uuid_unparse(uuid, cname);

  std::string cname_str(cname);
  return cname_str;
}
