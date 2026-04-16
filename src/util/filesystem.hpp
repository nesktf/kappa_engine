#pragma once

#include "./array.hpp"

namespace kappa {

fn load_entire_file(const char* path) -> UniqueArray<u8>;

} // namespace kappa
