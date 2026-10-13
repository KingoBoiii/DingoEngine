#!/bin/bash
# Builds the parts of LunarG's Linux Vulkan SDK the engine links (Vulkan-Headers, SPIRV-Cross, shaderc)
# and the static assimp 6.0.4, for machines that can't download the SDK (sdk.lunarg.com is blocked in
# Claude Code cloud containers). LINUX-SUPPORT.md Appendix A explains the choices.
#
# Usage: scripts/build-linux-toolchain.sh [cache-dir]   (default: ~/.cache/dingo-toolchain)
# Prints the toolchain directory as the last line of stdout: VULKAN_SDK=<dir>/vulkan, and
# <dir>/assimp/libassimp.a belongs in vendor/assimp/lib/linux-x86_64/. A finished toolchain is reused;
# changing a version below builds a new one beside it. Progress goes to stderr.
set -euo pipefail

VULKAN_TAG=vulkan-sdk-1.4.357.0
SHADERC_TAG=v2026.4
ASSIMP_TAG=v6.0.4

CACHE=${1:-$HOME/.cache/dingo-toolchain}
DEST=$CACHE/$VULKAN_TAG-shaderc-$SHADERC_TAG-assimp-$ASSIMP_TAG-gcc$(gcc -dumpversion)

if [[ -f $DEST/.complete ]]; then
	echo "$DEST"
	exit 0
fi

STAGE=$DEST.partial
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
rm -rf "$STAGE"
mkdir -p "$STAGE/vulkan/include" "$STAGE/vulkan/lib" "$STAGE/assimp"
cd "$WORK"

step() { echo "[toolchain] $*" >&2; }

step "Vulkan-Headers $VULKAN_TAG"
git clone -q --depth 1 -b "$VULKAN_TAG" https://github.com/KhronosGroup/Vulkan-Headers.git
cmake -S Vulkan-Headers -B build-vh -G Ninja -DCMAKE_INSTALL_PREFIX="$STAGE/vulkan" -DVULKAN_HEADERS_ENABLE_MODULE=OFF >&2
cmake --install build-vh >&2

step "SPIRV-Cross $VULKAN_TAG"
git clone -q --depth 1 -b "$VULKAN_TAG" https://github.com/KhronosGroup/SPIRV-Cross.git
cmake -S SPIRV-Cross -B build-sc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DCMAKE_INSTALL_PREFIX="$STAGE/vulkan" -DSPIRV_CROSS_SHARED=OFF -DSPIRV_CROSS_CLI=OFF -DSPIRV_CROSS_ENABLE_TESTS=OFF >&2
cmake --build build-sc >&2
cmake --install build-sc >&2

step "shaderc $SHADERC_TAG"
git clone -q --depth 1 -b "$SHADERC_TAG" https://github.com/google/shaderc.git
(cd shaderc && ./utils/git-sync-deps) >&2
cmake -S shaderc -B build-shaderc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON \
	-DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DENABLE_GLSLANG_BINARIES=OFF -DSPIRV_TOOLS_BUILD_STATIC=ON >&2
cmake --build build-shaderc --target shaderc_combined >&2
cp -r shaderc/libshaderc/include/shaderc "$STAGE/vulkan/include/"
cp build-shaderc/libshaderc/libshaderc_combined.a "$STAGE/vulkan/lib/"

step "assimp $ASSIMP_TAG"
git clone -q --depth 1 -b "$ASSIMP_TAG" https://github.com/assimp/assimp.git
cmake -S assimp -B build-assimp -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_WARNINGS_AS_ERRORS=OFF >&2
cmake --build build-assimp >&2
cp build-assimp/lib/libassimp.a "$STAGE/assimp/"

touch "$STAGE/.complete"
rm -rf "$DEST"
mv "$STAGE" "$DEST"
step "done: $DEST"
echo "$DEST"
