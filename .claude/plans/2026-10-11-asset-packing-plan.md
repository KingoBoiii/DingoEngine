# Asset packing for shipped games (`.dpak`)

## Context

Today a released game is its executable plus a plain copy of its `assets/` folder: `build-release.yml` robocopies `examples/<X>/assets` into each zip, and `scripts/package-linux-programs.sh` does `cp -r assets`. Anyone can open, copy or rip a game's PNGs, GLBs, WAVs and GLSL. The goal is for a shipped game to carry **one encrypted, compressed archive** (`<Game>.dpak`) in place of the folder. Development keeps loose files and hot reload.

**What this protects against.** The decryption key has to ship inside the executable, as it does for Unity and Unreal paks. So this stops browsing, "rename to .zip" and off-the-shelf asset rippers. It does not stop someone who reverse-engineers the exe. The docs will say this plainly; copyright and the EULA are the real protection.

**Decisions (owner, 2026-10-11):**
- Pack, compress and encrypt (ChaCha20, per-game key).
- Compression is stb's deflate, which is already compiled into `FileSystem.cpp`, so no new vendor dependency.
- ChaCha20 is written in-house (about 150 lines, checked against the RFC 8439 vectors).

**Where the engine stands.** There is no VFS. Every load ends in a direct OS read:
- `stbi_load` by path (`FileSystem.cpp:48`)
- `std::ifstream` for shaders, includes, fonts and `.events` (`NvrhiShader.cpp:604`, `ShaderIncludes.cpp:154`, `Font.cpp:76-94`, `Model.cpp:1313`)
- assimp's default IO (`Model.cpp:958`, `:966`)
- `ma_sound_init_from_file` (`MiniAudioEngine.cpp:214`)

The one shared seam is `Internal::ResolveRawAssetPath` (`AssetPath.h`, defined at `AssetManager.cpp:45-59`), plus the AssetManager's `ResolvePath` (`:1093`). Memory-based decoders already exist for images (`FileSystem::ReadImageFromMemory`) and fonts (FreeType loads from memory). Engine shaders are already embedded in Release and Distribution builds.

## Public API (on `AssetManager`, the owner's choice)

```cpp
struct PackageParams {            // fluent, like every *Params
    std::filesystem::path SourceDirectory;   // default: the manager's asset root
    std::filesystem::path OutputPath;        // e.g. "build/dist/Marionette.dpak"
    PackageKey Key;                          // 32 bytes, from the game's generated key header
    std::vector<std::string> Exclude;        // globs; plus the root's .dpakignore
};
struct PackageResult {            // what Package() returns
    bool Success; std::string Error;
    uint32_t FileCount; uint64_t RawBytes, PackedBytes;
    std::vector<std::string> Warnings;       // case clashes, skipped files…
};

static PackageResult AssetManager::Package(const PackageParams&);   // static: pure file work, needs no running app
PackageHandle AssetManager::Unpackage(const std::filesystem::path& dpak, const PackageKey&);
std::span<const AssetHandle> AssetManager::GetPackageAssets(PackageHandle) const;
bool AssetManager::IsPackageMounted(PackageHandle) const;
void AssetManager::ClosePackage(PackageHandle);   // unloads its assets, then unmounts
```

- **`Unpackage` never writes files to disk.** Extracting them would put the assets right back in a folder. Instead it opens the archive, mounts it into the VFS and **registers every entry it holds** (`Import`, not `Load`). The game then works exactly as it does today:

  ```cpp
  auto pack = assets.Unpackage("Marionette.dpak", GetPackKey());
  AssetHandle knight = assets.Load("characters/Knight.glb");   // comes from the pack
  for (AssetHandle h : assets.GetPackageAssets(pack)) assets.LoadAsync(h);  // e.g. a loading screen
  ```

  It returns a `PackageHandle` (a UUID, `k_InvalidAsset`-style 0 on failure: a missing file, a bad key, a corrupt TOC; logged). The asset handles come from `GetPackageAssets` or from the usual path lookup.
- **Override order.** A package mounted later wins over earlier mounts and over loose files, so a patch `.dpak` can replace single assets.
- **Raw factories go through the same VFS.** `Texture::CreateFromFile`, `Font::Create` and the others read packed files too, so existing game code keeps working.
- **The `DingoPack` CLI** (phase C) is a ~30-line `main` that calls `AssetManager::Package`. CI and scripts use it. A game or a future editor can also call `Package` itself.

"As built" notes are added here per phase as the work lands.

---

## Phase A: VFS with directory mounts only (no behaviour change)

**New:** `include/DingoEngine/Core/VirtualFileSystem.h` and `src/DingoEngine/Core/VirtualFileSystem.cpp`.
- Ordered mounts, where a later mount overrides an earlier one (this also makes patch packs possible later).
- Paths are logical: forward slashes, `lexically_normal`, root-relative.
- API:
  - `Resolve(path)` returns `{ LogicalKey, DiskPath (empty when packed), Mount* }`.
  - `Exists`, `ReadBytes` (returns `std::optional<std::vector<uint8_t>>`), `GetWriteTime` (`nullopt` for a packed file) and `IsLooseFile`.
- **Thread safety.** The texture and audio decodes run on the AssetManager worker. The mount table sits behind a `std::shared_mutex`, written only at startup or shutdown, and reads are reentrant.
- **Legacy `"assets/..."` paths.** Seven examples call `Font::Create("assets/fonts/arialbd.ttf")` and similar. Today these work only because of the cwd fallback. The VFS retries with the leading root-folder name stripped, then falls back to a cwd-relative path, as now.

**Rewire, with nothing packed yet:**
- `ResolveRawAssetPath` delegates to the VFS.
- `AssetManager::Initialize` mounts its root as mount 0. Its missing-root warning (`:836`) checks the mounts, not the disk.
- `FileSystem`:
  - Add `ReadFileBytes`, promoted from the file-local `Utils::ReadFileBytes` in `Font.cpp:76`.
  - `ReadTextFile` and `ReadImage` are rebuilt on `ReadBytes` + `stbi_load_from_memory`.
  - This also fixes the Windows Unicode issue, where `path.string()` is passed to stb today.
- Shaders:
  - `NvrhiShader::GetShaderSources` (`:598-604`) switches to `Vfs::Exists`/`ReadBytes`.
  - `ShaderIncludes.cpp:128-154` dedupes on the logical key, not `weakly_canonical`.
- Models:
  - The external-texture probe (`Model.cpp:172-222`) and the `.events` reads (`:1030`, `:1091`, `:1313`) go through the VFS.
  - `modelDir` and `m_FilePath` (`:992-999`) stay logical.
- Hot reload:
  - `Utils::ReadWriteTime`, `CompanionChanged` and `Model.cpp:64-68` use `Vfs::GetWriteTime`.
  - Hot reload skips a file that isn't loose.

## Phase B: assimp and miniaudio adapters (still loose files)

**`src/DingoEngine/Graphics/AssimpVfsIO.{h,cpp}`**: an `Assimp::IOSystem` and `IOStream` over a memory buffer. `importer.SetIOHandler(...)` is called before both `ReadFile`s.
- The stream needs a full `Seek`, `Tell` and `FileSize` for format sniffing.
- `getOsSeparator()` returns `'/'`, and both separators are accepted.
- glTF `.bin` URIs are URL-decoded (`%20`).
- `Close` deletes the stream.
- A one-entry cache of the last-read bytes covers the FBX pivot re-read.

**`src/DingoEngine/Audio/MiniAudio/MiniAudioVfs.{h,cpp}`**: a struct whose first member is `ma_vfs_callbacks`, implementing all eight callbacks (`onWrite` returns an error).
- `onOpen` reads the whole entry into memory.
- `ma_engine_init` gets an `ma_engine_config` with `pResourceManagerVFS` set.
- `MA_SOUND_FLAG_DECODE` and `ma_sound_init_copy` keep working, because the result is still a resource-manager data buffer.
- `MiniAudioEngine.cpp:204` `exists` changes to `Vfs::Exists`.

## Phase C: the `.dpak` format, reader and `DingoPack` tool (no encryption yet)

**Pack core:** `src/DingoEngine/Asset/Pack/` (`PackFormat.h`, `PackReader`, `PackWriter`, `Crc32`, `Deflate`). It is engine-internal and is used by `AssetManager::Package` and `Unpackage`.

**Format:**
- Header: magic `DPAK`, version, flags, 16-byte salt, TOC offset and size.
- Entry blobs.
- TOC, which is deflated: per entry `{ FNV-1a 64 path hash, name, offset, packed size, raw size, CRC32, flags (Deflated / Encrypted) }`. File names live only inside the TOC, so phase D hides them.

**Compression:**
- Inflate uses `stbi_zlib_decode_buffer` (`stb_image.h:532`) into a buffer sized from the TOC's raw size.
- Deflate uses `stbi_zlib_compress` (`stb_image_write.h:895`, `extern` but undeclared, so it gets its own declaration in `Deflate.cpp`; the result is released with `free`).
- An entry is stored raw if deflate saves less than 5%. Already-compressed formats skip deflate entirely: `.png .jpg .jpeg .ogg .mp3 .glb`, where `.glb` is size-checked.
- stb doesn't verify Adler-32, so the CRC32 is the integrity check.
- Entries are capped at 2 GiB because the stb API uses `int` sizes.

**Case:** the writer rejects two paths that differ only by case. The reader matches case-insensitively and warns, outside Distribution, when the case differs. This way a game can't work packed and then fail loose on Linux.

**Reading:** one file handle with positional reads (`pread` / `ReadFile` + `OVERLAPPED`), so no seek state is shared between threads.

**`AssetManager` API** (see "Public API" above):
- `Package` walks the source directory, applies `.dpakignore` and `Exclude`, and writes through `PackWriter` with `IO::WriteFileAtomic`.
- `Unpackage` calls `Vfs::MountPack`, then `Import`s each TOC entry. The asset type comes from the extension via `AssetTypeFromExtension`, and the `PackageHandle` → handles list is kept in `AssetManagerData`.
- `ClosePackage` unloads the package's assets, unmounts, and drops their registrations.
- Phase C ships with a null key (no encryption) and phase D switches it on.

**`tools/DingoPack/`:** a console project that links the engine and calls `copyAssimpRuntime()` like any executable.
- `DingoPack pack <assetsDir> <out.dpak> --key <file>` calls `AssetManager::Package`.
- `list` and `verify` open the archive through the pack reader.

## Phase D: encryption and the key

- **`Pack/ChaCha20.{h,cpp}`:** in-house, with the RFC 8439 test vectors in the Pack Test.
  - It is applied after deflate.
  - Each entry's nonce is derived from the salt and its entry index; the TOC gets its own nonce.
  - There is no MAC, by design (this is deterrence only), and the docs say so.
- **Where the key lives.** It cannot live in the engine lib, which ships prebuilt as `DingoEngine.lib` / `libDingoEngine.a`.
  - Each game has a key file, e.g. `examples/<X>/dpak.key` (32 random bytes, hex).
  - A premake helper `embedPackKey()` in the root `premake5.lua` generates a game-side header with the key split and XOR-masked.
  - The game passes the key to the engine through `ApplicationParams`.
  - DingoPack reads the same file.
  - Example keys are committed. `docs/distribution.md` tells real games to keep theirs in a CI secret.

## Phase E: application integration

- Games call `Unpackage` themselves, so they choose when and which packs to open (a base pack, then DLC or patches).
- For convenience, `ApplicationParams.Assets` gains `PackageKey` and `AutoUnpackage` (default true). With it on, `AssetManager::Initialize` checks for `<exeDir>/<exe stem>.dpak` and `Unpackage`s it if it exists.
  - In that case the loose directory is not mounted unless the game opts in (`AllowLooseFiles`).
  - This also removes the "cwd must be the exe directory" requirement for packed builds.
- The F6 Assets tab shows each asset's source (loose / pack name).

## Phase F: build and CI

- New `scripts/pack-assets.sh` and `.ps1`, which build DingoPack if needed and pack one example.
  - Packing happens at packaging time, not as a post-build step, so dev builds stay fast.
- `build-release.yml`: each example job replaces `robocopy ... assets` with packing into `<Example>.dpak` beside the exe. The DingoPack binary joins the engine release zip and tarball.
- `scripts/package-linux-programs.sh:23` and `scripts/linux-tester/make-bundle.sh` ship the `.dpak`. The test framework keeps loose assets, since it is a dev tool.
- `CLAUDE.md`'s "New executable checklist" gains `embedPackKey()` and the pack step.

## Phase G: caches, docs and examples

- **Caches:**
  - The shader and font-atlas caches (`NvrhiShader.cpp:109-126`, `Font.cpp:69-74`) are keyed by logical path rather than absolute path.
  - Packed builds keep writing them under `.cache`. These are compiled SPIR-V and atlases, not source, which is acceptable at this protection level.
  - Optional follow-up issue: pre-warm the caches into the pack and stop shipping GLSL.
- **Examples:** migrate to root-relative paths (drop the `"assets/"` prefix). The VFS warns once, in Debug, when the legacy prefix-strip is used.
- **Docs:**
  - New `docs/distribution.md`: the workflow, key handling and an honest threat model.
  - Update `docs/asset-pipeline.md`, fix the stale cwd text in `docs/getting-started.md`, and add a CLAUDE.md entry.
  - ROADMAP.md: add this under v1.0, which is where the itch.io and Steam release lives.

## Critical files

- `include/DingoEngine/Asset/AssetManager.h` (`Package`, `Unpackage`, `GetPackageAssets`, `ClosePackage`, `PackageParams`, `PackageResult`, `PackageHandle`)
- `src/DingoEngine/Asset/AssetManager.cpp`, `AssetPath.h`, `AssetManagerData.h`
- `src/DingoEngine/Core/FileSystem.cpp`, `include/DingoEngine/Core/FileSystem.h`
- `src/DingoEngine/Graphics/NVRHI/NvrhiShader.cpp`, `src/DingoEngine/Graphics/ShaderIncludes.cpp`
- `src/DingoEngine/Graphics/Model.cpp`, `src/DingoEngine/Graphics/Font.cpp`
- `src/DingoEngine/Audio/MiniAudio/MiniAudioEngine.cpp`
- `include/DingoEngine/Core/Application.h` (`ApplicationParams`)
- `premake5.lua`, `.github/workflows/build-release.yml`, `scripts/package-linux-programs.sh`
- New: `Core/VirtualFileSystem.*`, `Asset/Pack/*`, `Graphics/AssimpVfsIO.*`, `Audio/MiniAudio/MiniAudioVfs.*`, `tools/DingoPack/`, `scripts/pack-assets.*`, `docs/distribution.md`

**Reused rather than rebuilt:**
- `FileSystem::ReadImageFromMemory`
- FreeType's from-memory path in `Font.cpp:335`
- `Platform::GetExecutableDirectory`
- `IO::WriteFileAtomic` (DingoPack's output)
- `scripts/embed.lua`'s pattern for the key header
- `Tests/TestChecks.h` for PASS/FAIL checks

## Verification

- **After A and B:** the full test app runs unchanged (`--test=asset`, `--test=anim`, `--test=bus`, `--test=Text`, the model tests). Marionette `--check` and the Linux `xvfb-run scripts/ci/linux-smoke-test.sh` pass. Hot reload of a texture, a shader with an `#include`, and a model + `.events` still works.
- **New Pack Test (`--test=pack`):**
  - It calls `AssetManager::Package` on `test/assets` (writing into the cache directory), then `Unpackage`s the result over the loose files.
  - It checks that `GetPackageAssets` covers every file, and that `ClosePackage` makes later lookups fall back to the loose files.
  - It then checks that the packed and loose versions are identical for each type:
    - texture pixels via `ReadPixels`
    - model vertex, joint and clip counts, and `.events` marks
    - an audio clip's frame count
    - a font's glyph metrics
    - a shader with an `#include`
  - It also covers:
    - ChaCha20 against the RFC 8439 vectors
    - a flipped byte, which must fail the CRC check without crashing
    - a wrong key, which must fail cleanly
    - concurrent `LoadAsync` from the worker thread
- **End to end:**
  - Package Marionette and Candlewick with the new scripts.
  - Delete `assets/`, run them from a different cwd, and play.
  - `strings Marionette.dpak | grep -i -e glb -e png -e "#type"` must find nothing.
  - `DingoPack verify` passes.
  - Check this on Windows (Vulkan, DX11, DX12) and on Linux CI's Distribution job.
