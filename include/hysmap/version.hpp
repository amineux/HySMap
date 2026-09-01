#pragma once

#define HYSMAP_VERSION_MAJOR 0
#define HYSMAP_VERSION_MINOR 2
#define HYSMAP_VERSION_PATCH 0
#define HYSMAP_VERSION_STRING "0.2.0"

namespace hysmap {
inline constexpr const char* version() { return HYSMAP_VERSION_STRING; }
}  // namespace hysmap
