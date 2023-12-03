#pragma once

#include "collection_types.h"

#include <engine/math.inl>

namespace grunka {

// Loads a palette from a JAS-PAL .pal file into the array.
bool load_palette(const char *file_path, foundation::Array<math::Color4f> &palette);

} // namespace grunka
