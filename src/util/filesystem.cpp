#include "./filesystem.hpp"

#include <fstream>

namespace kappa {

fn load_entire_file(const char* path) -> UniqueArray<u8> {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return {};
  }
  const usize filesz = (usize)file.tellg();
  auto array = make_array<u8>(uninitialized, filesz);
  file.seekg(0);
  file.read((char*)array.data(), filesz);
  file.close();
  return array;
}

} // namespace kappa
