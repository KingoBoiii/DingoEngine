#pragma once

#include <cstdint>

/// Packs a semantic version (major.minor.patch) into one uint32_t: major in 7 bits (22-28),
/// minor in 10 bits (12-21), patch in 12 bits (0-11). A part wider than its field is not masked and
/// spills into the field above. Packed versions compare correctly as plain integers.
#define DE_MAKE_VERSION(major, minor, patch) ((((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))

/// Extracts the major version (0-127) from a version packed by DE_MAKE_VERSION.
#define DE_VERSION_MAJOR(version) (((uint32_t)(version) >> 22U) & 0x7FU)
/// Extracts the minor version (0-1023) from a version packed by DE_MAKE_VERSION.
#define DE_VERSION_MINOR(version) (((uint32_t)(version) >> 12U) & 0x3FFU)
/// Extracts the patch version (0-4095) from a version packed by DE_MAKE_VERSION.
#define DE_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFFU)
