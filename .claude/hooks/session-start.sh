#!/bin/bash
# Gets a Claude Code cloud session ready to build and run the engine on Linux. The container is cached
# after this hook, so the slow parts (the toolchain build, apt, submodules) run once per cached image.
# Its stdout reaches the model's context: keep it to one line and send everything else to the log.
set -euo pipefail

if [[ "${CLAUDE_CODE_REMOTE:-}" != "true" ]]; then
	exit 0
fi

REPO=${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}
LOG=${TMPDIR:-/tmp}/dingo-session-start.log
exec 3>&1 >"$LOG" 2>&1

fail() {
	echo "Linux toolchain setup failed at: $1 (log: $LOG)" >&3
	exit 0
}

packages=(build-essential cmake ninja-build python3 zlib1g-dev libx11-dev libxrandr-dev libxinerama-dev
	libxcursor-dev libxi-dev xvfb xauth mesa-vulkan-drivers python3-xlib vulkan-validationlayers)
missing=()
for package in "${packages[@]}"; do
	dpkg -s "$package" >/dev/null 2>&1 || missing+=("$package")
done
if ((${#missing[@]})); then
	{ apt-get update -q && DEBIAN_FRONTEND=noninteractive apt-get install -y -q "${missing[@]}"; } || fail "apt-get install ${missing[*]}"
fi

git -C "$REPO" submodule update --init --recursive --jobs 4 || fail "git submodule update"

toolchain=$("$REPO/scripts/build-linux-toolchain.sh" | tail -1) || fail "scripts/build-linux-toolchain.sh"

mkdir -p "$REPO/vendor/assimp/lib/linux-x86_64"
cp -u "$toolchain/assimp/libassimp.a" "$REPO/vendor/assimp/lib/linux-x86_64/"

icd=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json 2>/dev/null | head -1 || true)
if [[ -n "${CLAUDE_ENV_FILE:-}" ]]; then
	echo "export VULKAN_SDK=$toolchain/vulkan" >>"$CLAUDE_ENV_FILE"
	if [[ -n $icd ]]; then
		echo "export VK_DRIVER_FILES=$icd" >>"$CLAUDE_ENV_FILE"
	fi
fi

echo "Linux toolchain ready: VULKAN_SDK=$toolchain/vulkan, assimp in vendor/assimp/lib/linux-x86_64, llvmpipe via VK_DRIVER_FILES. Run ./Generate-Linux.sh, then make." >&3
