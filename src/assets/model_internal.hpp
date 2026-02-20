#pragma once

#include "./model.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_map>

namespace kappa::assets {

struct model_allocator {
  template<typename T>
  T* alloc(size_t n) {
    std::allocator<T> alloc;
    return alloc.allocate(n);
  }

  template<typename T>
  void dealloc(T* ptr, size_t count) {
    std::allocator<T> alloc;
    alloc.deallocate(ptr, count);
  }
};

struct model3d_data::model_internal {
  model_allocator alloc;
  std::unordered_map<std::string_view, u32> anim_registry;
  std::unordered_map<std::string_view, u32> mesh_registry;
  std::unordered_map<std::string_view, u32> material_registry;
  std::unordered_map<std::string_view, u32> bone_registry;
  std::unordered_map<std::string_view, u32> armature_registry;
};

struct model3d_loader::loader_internal {
  Assimp::Importer importer;
  buffer_name model_name;
  buffer_path model_path;
  bits32 importer_flags;
};

} // namespace kappa::assets
