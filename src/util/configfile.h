#pragma once

#include <string>
#include <json.hpp>

namespace top1 {

using json = nlohmann::json;

class ConfigFile {
public:

  std::string path;

  json data;

  ConfigFile() {}

  ConfigFile(std::string path, json data = {}) :
    path (path), data (data) {}

  ConfigFile(ConfigFile&) = delete;
  ConfigFile(ConfigFile&&) = delete;

  void read();
  void write();
};

}
