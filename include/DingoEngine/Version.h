#pragma once

#include <cstdint>

/**************************************************
***		SEMANTIC VERSION						***
***												***
*** Macros for encoding and decoding semantic	***
*** version numbers (major.minor.patch) into a	***
*** single 32-bit integer.						***
***												***
*** DE_MAKE_VERSION(major, minor, patch):		***
***   Packs major (7 bits), minor (10 bits),	***
***   and patch (12 bits) into a uint32_t.		***
***												***
*** DE_VERSION_MAJOR(version):					***
***   Extracts the major version from packed.	***
*** DE_VERSION_MINOR(version):					***
***   Extracts the minor version from packed.	***
*** DE_VERSION_PATCH(version):					***
***   Extracts the patch version from packed.	***
***												***
**************************************************/

#define DE_MAKE_VERSION(major, minor, patch) ((((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))

#define DE_VERSION_MAJOR(version) (((uint32_t)(version) >> 22U) & 0x7FU)
#define DE_VERSION_MINOR(version) (((uint32_t)(version) >> 12U) & 0x3FFU)
#define DE_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFFU)
