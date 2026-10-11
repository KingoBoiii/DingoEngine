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

**As built (A):**
- The class is `VirtualFileSystem` (static). `Resolve` returns `std::optional<VfsResolvedPath>` with a `VfsMountId` (0 = a disk file outside every mount) rather than a `Mount*`. Besides the planned calls it has `SetRoot`/`GetRoot`, `GetVirtualPath` (what `ResolveRawAssetPath` now returns: `<root>/<key>` for a packed file, the disk file otherwise, the path unchanged when nothing matches) and `GetLogicalKey`, plus mount bookkeeping (`IsMounted`, `GetMountCount`, `GetMountName`).
- Logical keys are UTF-8 on every platform (`generic_u8string`), the encoding package entry names use, so a non-ASCII name matches its packed entry on Windows too.
- An absolute path folds to a key against the root first, then against each mounted directory; that key is then looked up newest mount first, so an absolute path under the root still finds a file a package overrides. An absolute path outside every mount is read from disk unchanged (the engine shader directory, `%TEMP%` copies in the Animation Test).
- `ResolveRawAssetPath` keeps its old shortcut: an absolute or empty path is returned untouched without a lookup.
- Packages plug in through an engine-internal `Internal::VfsArchive` interface (`src/DingoEngine/Core/VfsArchive.h`, `MountArchive`), so the public header names no pack type.
- `FileSystem::ReadImage` keeps its file-name error line and adds it after `ReadImageFromMemory`'s own.
- `.events` text is read through the VFS into an `std::istringstream`; the tokenizer already treated `\r` as space, so CRLF files parse as before.
- Verified on Linux (Debug and Release, Xvfb + llvmpipe): the smoke test's 20 cases, 10 examples and Marionette `--check` pass with the same PASS counts as the untouched baseline (394 + 107).

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

**As built (B):**
- `Internal::AssimpVfsIOSystem` is installed on the importer before both reads; the importer owns and frees it. Paths reach assimp as generic UTF-8 (`ToAssimpPath`) and come back the same way, so a non-ASCII model path survives on Windows. `Seek` from the end follows assimp's own `MemoryIOStream` (`size - offset`).
- A name is looked up as given first and percent-decoded only when that misses, so a file whose real name contains `%` still loads.
- `ComparePaths` compares logical keys, so a model's own path given two ways is one file.
- The one-entry cache shares its bytes between streams (`shared_ptr`), so an FBX pivot re-read or a glTF reopening its `.bin` doesn't read twice.
- `MiniAudioVfs` is opaque outside its `.cpp` (`MiniAudioData.h` holds a pointer); it is created before `ma_engine_init` and freed after `ma_engine_uninit` and on every failed init path. `LoadClip` passes miniaudio the UTF-8 path.
- Verified on Linux (Debug and Release, Xvfb + llvmpipe): the smoke test passes with the baseline counts (394 + 107); a manual Asset Manager Test run hot-reloaded a texture, a shader and a glTF model through the VFS.

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

**As built (C):**
- Header (64 B): magic, version 1, flags, 16-byte salt, TOC offset / packed size / raw size / CRC32, entry count and an 8-byte key check (zero until D). `TocEntry` is 42 B packed, followed by its UTF-8 name. The TOC must end the file (`TocPackedSize == fileSize - TocOffset`), and every entry has to lie between the header and the TOC, so a truncated file fails as a damaged header, not as a bad read later.
- `.glb` is size-checked like any other file (the 5% rule), not skipped: Marionette's animation-library GLBs deflate to about 45%. Only `.png .jpg .jpeg .ogg .mp3` skip deflate.
- stb skips the zlib Adler-32, so the TOC and every entry carry a CRC32 of the inflated bytes; a damaged entry logs "is damaged (CRC mismatch)" and reads as missing, while the rest of the package keeps working.
- `Package` sorts the files by path (deterministic apart from the random salt) and skips the output file when it sits inside the source. The case-clash check runs on every platform. An empty source, a missing source or an unwritable output fail with `PackageResult::Error` set.
- `Unpackage` registers only files with a known asset type; `.bin`, `.events`, includes and licences are mounted but not registered. An asset already registered (loaded loose, or held by an earlier package) is reused, not duplicated, and reloaded when it was `Ready`, so the package's copy takes over in place.
- `ClosePackage` hands a registration it created to another mounted package holding the same file instead of removing it; one nobody else holds is `Remove`d.
- `GetAssetSource(handle)` was added for the F6 panel (phase E): "loose", the package's file name, or "missing".
- `AudioClip::GetFrameCount()` was added so the Pack Test can compare a packed clip's decode with the loose one.
- `AssetManager` path keys are built with `generic_u8string` (they used `generic_string()`, which MSVC converts to the ANSI code page, throwing on a name it can't map).
- `DingoPack` never calls `Log::Initialize`, so it writes no `Dingo.log` beside the packages. Exit codes: 0 ok, 1 failure, 2 usage.
- Pack Test (`--test=pack`, 41 checks): packaging, excludes and `.dpakignore`, refusals, byte-for-byte reads of every file, a skinned glTF with `.bin` and `.events`, a GLB, audio, a font, a shader, texture pixels by readback, `LoadAsync` from a package while the main thread reads it, a patch package overriding and reloading in place, a pack-only shader with an `#include`, `ClosePackage` falling back, and damaged, truncated and non-package files.
- Verified on Linux (Debug and Release, Xvfb + llvmpipe): the smoke test's 21 cases (the Pack Test's 41 checks included), 10 examples and Marionette `--check` (107) pass; `DingoPack pack/list/verify` on Marionette's assets (41 files, 7.1 MB into 3.7 MB) and its refusals (usage, a non-package) by hand.

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

**As built (D):**
- Nonce = salt bytes 4–15 with the first four XORed with the entry index (little-endian); the TOC uses index `0xFFFFFFFF` and the key check `0xFFFFFFFE`, which no entry reaches (`k_MaxEntries` = `0xFFFF0000`). Counter 0 everywhere. The salt is fresh per `Package` call, so packing the same files with the same key twice gives different bytes and no nonce is ever reused across builds.
- The header's `KeyCheck` holds the first 8 keystream bytes of the key-check nonce, so a wrong key fails as "wrong key" before the TOC is touched, and a missing key on an encrypted package as "no key was given". A null key writes and opens unencrypted packages, so a base package and an unencrypted patch can be mounted together. An entry flagged encrypted inside an unencrypted package reads as damaged.
- `embedPackKey(keyFile = "dpak.key")` writes `build/generated/<project>/DingoPackKey.h` **when premake runs**, not as a prebuild step: the Windows CI builds with an MSBuild that has no premake on its path, and gmake doesn't order custom-build outputs before the objects that include them. The key is split into two 16-byte halves, each XORed with a mask derived from the key itself (FNV-1a, then an LCG), kept in four `static const volatile` arrays (two reversed), so the header changes only when the key does. A missing or malformed key file stops premake with the project's name.
- Each of the ten examples has a committed `dpak.key` (made with `DingoPack genkey`, its comment saying it is public on purpose) and calls `embedPackKey()` after `copyAssimpRuntime()`. Nothing reads the header until phase E.
- The Pack Test (52 checks) adds: both RFC 8439 vectors (2.3.2 block, 2.4.2 encryption) and the round trip; `PackageKey` hex and key-file parsing; a null key and a key one bit off refused (reader and `Unpackage`); no file names or file signatures (`textures/`, `#type vertex`, the GLB, PNG, Exif and WAVE headers, each 5 bytes or more so ciphertext can't match one by chance) in the encrypted package, while the unencrypted one shows the PNG and Exif headers; two packagings of the same files differ. The base package is encrypted, so every load, damage and override check now runs through decryption.
- `strings` on an encrypted package: random ciphertext holds short printable runs, so a case-insensitive grep for a three-letter token finds a few by chance (4 hits for `glb`/`png` in Marionette's 3.6 MB package, none of them a file name); `\.glb`, `\.png` and `#type` find nothing.
- Verified on Linux (Debug and Release, Xvfb + llvmpipe): the smoke test's 21 cases (Pack Test 52), 10 examples and Marionette `--check` (107) pass; `DingoPack` packs Marionette encrypted, verifies it with its key and refuses it without one or with Candlewick's.

## Phase E: application integration

- Games call `Unpackage` themselves, so they choose when and which packs to open (a base pack, then DLC or patches).
- For convenience, `ApplicationParams.Assets` gains `PackageKey` and `AutoUnpackage` (default true). With it on, `AssetManager::Initialize` checks for `<exeDir>/<exe stem>.dpak` and `Unpackage`s it if it exists.
  - In that case the loose directory is not mounted unless the game opts in (`AllowLooseFiles`).
  - This also removes the "cwd must be the exe directory" requirement for packed builds.
- The F6 Assets tab shows each asset's source (loose / pack name).

**As built (E):**
- The three settings live on `AssetManagerParams` (`ApplicationParams.Assets`): `Dingo::PackageKey PackageKey` (spelled qualified, as CLAUDE.md requires of a member named after its type), `AutoUnpackage = true`, `AllowLooseFiles = false`, with fluent setters. The package looked for is `Platform::GetExecutableDirectory() / <executable stem>.dpak`.
- `Initialize` still sets the VFS root to `RootDirectory` (so `<root>/key` absolute paths and the legacy `assets/` prefix fold onto package keys) but mounts the directory only when there is no package or `AllowLooseFiles` is set; the package mounts above it. Without the file nothing changes, so every dev build behaves as before. A package that fails to open logs an error naming it; a mounted one logs that loose files aren't read. `Shutdown` now clears the VFS root whether or not a directory was mounted.
- Every example includes `<DingoPackKey.h>` and passes `Dingo::Generated::GetPackageKey()`. Marionette turns `AutoUnpackage` off for `--hot-reload` and `--live-edit-demo`, which edit loose files.
- F6's registry table has a Source column ("loose", the package's name, "missing"), refreshed once a second because resolving stats loose files; its path cells are UTF-8 (`generic_u8string`).
- Verified on Linux (Debug, Xvfb + llvmpipe): a copy of the Marionette executable beside its encrypted `Marionette.dpak`, in a directory with no `assets/` above it, started from an unrelated working directory, passes `--check` (107) and logs the mount; Candlewick the same way starts, runs and closes cleanly. Smoke results are under F, which ran on the same build.

## Phase F: build and CI

- New `scripts/pack-assets.sh` and `.ps1`, which build DingoPack if needed and pack one example.
  - Packing happens at packaging time, not as a post-build step, so dev builds stay fast.
- `build-release.yml`: each example job replaces `robocopy ... assets` with packing into `<Example>.dpak` beside the exe. The DingoPack binary joins the engine release zip and tarball.
- `scripts/package-linux-programs.sh:23` and `scripts/linux-tester/make-bundle.sh` ship the `.dpak`. The test framework keeps loose assets, since it is a dev tool.
- `CLAUDE.md`'s "New executable checklist" gains `embedPackKey()` and the pack step.

**As built (F):**
- `scripts/pack-assets.sh <Example> <out.dpak> [Config]` and `pack-assets.ps1 -Example -Output [-Configuration]` take the output path as a required argument, with no default beside the build's own executable: a package there would be auto-mounted by every dev run and shadow the loose files being edited. Both build DingoPack when it's missing (make / MSBuild on `DingoPack.vcxproj`) and `verify` what they wrote.
- `build-release.yml`: the Distribution build packs every `examples/*` into its own bin folder (plus the licence files at the root of its assets, so Marionette's KayKit licences stay readable) before the binaries are uploaded, and the ten example jobs lost their `robocopy ... assets` step, so each zip holds the executable, the DLLs, the `.dpak` and any licence text. DingoPack (exe and DLLs) goes into the engine zip under `tools\DingoPack\`, and into the Linux tarball under `tools/`.
- `package-linux-programs.sh` ships each example as executable + `.dpak` + licence files; the test framework keeps `assets/` and `imgui.ini`. `make-bundle.sh` packs the tester bundle's examples the same way, and its README gains a packed-assets checklist line.
- `scripts/ci/linux-smoke-test.sh` ends with two packed runs: copies of Marionette (`--check`) and Candlewick beside their `.dpak`s in a temp directory, started from another one, each required to log its mount. So every CI configuration exercises DingoPack, the encryption, the auto-mount and the packed loads.
- `CLAUDE.md`'s checklist gains `embedPackKey()`, the key in `ApplicationParams`, and where Windows and Linux packaging pack.

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
