#!/usr/bin/env bash
set -euo pipefail

ASSIMP_VERSION=6.0.4
SDK_ROOT=/opt/vulkansdk
ASSIMP_LIB=/opt/assimp-$ASSIMP_VERSION/libassimp.a
SUDO=$([ "$(id -u)" -eq 0 ] || echo sudo)

export DEBIAN_FRONTEND=noninteractive
$SUDO apt-get update -q
$SUDO apt-get install -y -q build-essential cmake ninja-build python3 zlib1g-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  xvfb xauth mesa-vulkan-drivers python3-xlib curl xz-utils git

if [ ! -x "$SDK_ROOT/current/x86_64/bin/glslc" ]; then
  version=${VULKAN_SDK_VERSION:-$(curl -sSfL https://vulkan.lunarg.com/sdk/latest/linux.txt | tr -d '[:space:]')}
  $SUDO mkdir -p "$SDK_ROOT"
  curl -sSfL "https://sdk.lunarg.com/sdk/download/$version/linux/vulkansdk-linux-x86_64-$version.tar.xz" \
    | $SUDO tar -xJ -C "$SDK_ROOT"
  $SUDO ln -sfn "$SDK_ROOT/$version" "$SDK_ROOT/current"
fi

if [ ! -f "$ASSIMP_LIB" ]; then
  src=$(mktemp -d)
  git clone -q --depth 1 -b "v$ASSIMP_VERSION" https://github.com/assimp/assimp.git "$src"
  cmake -S "$src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_WARNINGS_AS_ERRORS=OFF
  cmake --build "$src/build"
  $SUDO install -D -m 644 "$src/build/lib/libassimp.a" "$ASSIMP_LIB"
  rm -rf "$src"
fi

lvp=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -1)
$SUDO tee /etc/profile.d/dingoengine.sh >/dev/null <<EOF
export VULKAN_SDK=$SDK_ROOT/current/x86_64
export VK_ADD_LAYER_PATH=$SDK_ROOT/current/x86_64/share/vulkan/explicit_layer.d
case ":\${LD_LIBRARY_PATH:-}:" in *":$SDK_ROOT/current/x86_64/lib:"*) ;; *) export LD_LIBRARY_PATH=$SDK_ROOT/current/x86_64/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH} ;; esac
export VK_DRIVER_FILES=$lvp
EOF
grep -qs dingoengine.sh ~/.bashrc || echo '. /etc/profile.d/dingoengine.sh' >> ~/.bashrc

repo=
for candidate in "${DINGO_REPO:-}" "$PWD" /home/user/DingoEngine; do
  if [ -n "$candidate" ] && [ -f "$candidate/premake5.lua" ] && [ -f "$candidate/.gitmodules" ]; then
    repo=$candidate
    break
  fi
done

if [ -n "$repo" ]; then
  git -C "$repo" submodule update --init --recursive --jobs 8
  mkdir -p "$repo/vendor/assimp/lib/linux-x86_64"
  cp "$ASSIMP_LIB" "$repo/vendor/assimp/lib/linux-x86_64/"
  echo "DingoEngine setup: submodules and assimp ready in $repo"
else
  echo "DingoEngine setup: repo not cloned yet; in the session run:"
  echo "  git submodule update --init --recursive && mkdir -p vendor/assimp/lib/linux-x86_64 && cp $ASSIMP_LIB vendor/assimp/lib/linux-x86_64/"
fi

"$SDK_ROOT/current/x86_64/bin/glslc" --version | head -1
