#include "configfile.h"

#include <fstream>
#include <plog/Log.h>

namespace top1 {

void ConfigFile::read() {
  std::ifstream stream;
  stream.open(path);
  if (!stream) {
    LOGI << "Empty JsonFile, creating " << path;
    stream.close();
    write();
    stream.open(path);
  }
  stream >> data;
  stream.close();
}

void ConfigFile::write() {
  std::ofstream stream(path, std::ios::trunc);
  stream << std::setw(2) << data << std::endl;
  stream.close();
}

}
