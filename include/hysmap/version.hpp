#pragma once

#define HYSMAP_VERSION_MAJOR 0
#define HYSMAP_VERSION_MINOR 1
#define HYSMAP_VERSION_PATCH 0
#define HYSMAP_VERSION_STRING "0.1.0"

namespace hysmap {
inline constexpr const char* version() { return HYSMAP_VERSION_STRING; }
}  // namespace hysmap
