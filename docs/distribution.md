# Shipping a Game: Asset Packages (.dpak)

A shipped game should not carry a loose `assets/` folder that anyone can browse, copy or
swap files in. DingoEngine packs a game's assets into one file, `<Game>.dpak`: deflated where
that pays, encrypted with the game's own key, and read in memory at run time. Nothing is
extracted to disk.

```
MyGame/
  MyGame(.exe)      the executable (and on Windows its DLLs)
  MyGame.dpak       every asset, packed and encrypted
```

During development nothing changes: with no `.dpak` beside the executable the engine reads
the loose `assets/` folder as before, hot-reload included.

## How it works

- **Virtual file system.** Every engine file read (textures, models and their `.bin` and
  textures, `.events` sidecars, shaders and their `#include`s, fonts, audio) goes through
  `VirtualFileSystem`, which looks a root-relative path such as `textures/player.png` up in
  its mounts, newest first. The asset root is one mount and a package is another, so the
  same `Load("textures/player.png")` or `Texture::CreateFromFile("textures/player.png")`
  works loose and packed.
- **Auto-mount.** At start-up the `AssetManager` looks for `<executable name>.dpak` in the
  executable's own directory. If it is there it is mounted with your key, and the loose root
  is *not* mounted (so a stray file next to the game can't stand in for a packed one); the
  game starts from any working directory. If it isn't there, the loose root is used.
- **Packages you open yourself.** `AssetManager::Unpackage(path, key)` mounts any other
  package (DLC, a patch) on top: a later package wins over earlier ones and over loose
  files, and an asset already loaded from a file it overrides is reloaded in place.
  `ClosePackage` takes it away again.

## Workflow

### 1. Give the game a key

```bash
DingoPack genkey dpak.key            # 32 random bytes as 64 hex digits
```

Keep `dpak.key` next to the game's `premake5.lua` (the examples commit theirs; see
[Keeping the key](#keeping-the-key) for a real game).

### 2. Embed it

In the game's `premake5.lua`, after `copyAssimpRuntime()`:

```lua
copyAssimpRuntime()
embedPackKey()          -- reads ./dpak.key; embedPackKey("keys/game.key") for another file
```

`embedPackKey()` runs when premake does and writes `build/generated/<Project>/DingoPackKey.h`,
holding the key split into two XOR-masked halves that `Dingo::Generated::GetPackageKey()`
puts back together at run time. Re-run premake after changing the key.

Pass it to the engine:

```cpp
#include <DingoPackKey.h>

ApplicationParams params = ApplicationParams{
    // ...
    .Assets = Dingo::AssetManagerParams().SetPackageKey(Dingo::Generated::GetPackageKey()),
};
```

### 3. Write paths root-relative

Write `"textures/player.png"`, not `"assets/textures/player.png"`. The engine still
accepts the old spelling (it strips the asset root's folder name, warning once in Debug),
but root-relative paths are what package entries are called.

Paths are case-sensitive on Linux. A package matches case-insensitively (and warns outside
Distribution when the case differs), but the loose file on Linux would not load, so keep
the case your files have.

### 4. Pack and ship

```bash
scripts/pack-assets.sh MyGame out/MyGame/MyGame.dpak            # Linux
scripts\pack-assets.ps1 -Example MyGame -Output out\MyGame\MyGame.dpak   # Windows
```

or call the tool yourself:

```bash
DingoPack pack <assetsDir> <out.dpak> --key dpak.key [--exclude <glob>]...
DingoPack list <package.dpak> --key dpak.key
DingoPack verify <package.dpak> --key dpak.key
```

Put the package beside a **copy** of the executable for shipping. Don't leave one next to
the executable in `build/bin/`: every dev run would then mount it and ignore the loose
files you are editing (the log says so: "loose files under ... are not read").

Files you don't want packed go in the source root's `.dpakignore` (one glob per line, `#`
comments; `*` stays within a folder, `**` crosses folders, a pattern without `/` matches a
file name anywhere, a trailing `/` matches a folder) or `--exclude`. The release scripts copy
licence files at the root of an example's assets beside the package as well, so they stay
readable.

The release workflow and `scripts/package-linux-programs.sh` do this for every example:
each ships its executable (and DLLs) beside `<Example>.dpak`, its licence files and
`THIRD-PARTY-NOTICES.md`, and on Linux its icon and desktop entry (below).

### 5. Name, icon and version

```lua
copyAssimpRuntime()
exeMetadata {
    Name = "My Game",                 -- required
    Description = "One line about it.",
    Version = "1.2.0",                -- defaults to the engine's version
    Company = "My Studio",
    Copyright = "Copyright (c) 2026 My Studio",
}
embedPackKey()
```

Only `Name` is required; a field left out is left out of the version resource. Set
`Version` to your game's own: without it the executable carries the engine's version and
build number (`VERSION`, or `DE_VERSION` and `DE_BUILD_NUMBER` in CI). `exeMetadata()` reads
`icon.ico` and `icon.png` beside the game's `premake5.lua` (`Icon = "art/game"` for
`art/game.ico` and `.png`); a missing file just leaves that icon out. When premake runs it
writes:

- `build/generated/<Project>/<Project>.rc`, compiled into the Windows executable: the
  version resource, which Explorer's Details tab shows, and the icon, named `GLFW_ICON` so
  GLFW puts it on the window and the taskbar too.
- `build/generated/<Project>/<Project>.desktop` and `<Project>.png`: a desktop entry with
  placeholders where the install folder goes, and the icon. `scripts/package-linux-programs.sh`
  ships both with `install-desktop-entry.sh`, which writes a copy naming the folder into
  `~/.local/share/applications` (`--remove` takes it out), since a desktop entry can't
  hold a relative path.

`scripts/icons/make-icons.sh` draws the examples' placeholder tiles from
`scripts/icons/icons.txt` (ImageMagick). The Linux window itself shows no icon: GLFW takes
it from pixels the game passes at run time, which the engine doesn't do yet.

### From code

```cpp
PackageResult result = AssetManager::Package(PackageParams()
    .SetSourceDirectory("assets")
    .SetOutputPath("out/MyGame.dpak")
    .SetKey(*PackageKey::FromFile("dpak.key"))
    .AddExclude("*.psd"));
```

`Package` is static and needs no running application (DingoPack is this call). A null
key writes an unencrypted package.

## Options

| `AssetManagerParams` | Default | Effect |
|---|---|---|
| `PackageKey` | null | The key the auto-mounted package opens with. A null key opens only an unencrypted package. |
| `AutoUnpackage` | `true` | Mount `<exe dir>/<exe name>.dpak` at start-up when it exists. |
| `AllowLooseFiles` | `false` | With the package mounted, also mount the loose root underneath it, so files the package lacks load loose. |

Hot-reload never fires for a packed file (it has no write time). F6's registry table shows
where each asset comes from in its Source column.

## Threat model

Be clear about what this buys you. **A `.dpak` is deterrence, not protection.**

What it does:

- No browsable folder: models, textures, sounds and shaders aren't sitting in the install
  directory for anyone to copy or drag into another game.
- No casual extraction: entries and the table of contents are encrypted (ChaCha20, RFC
  8439, with a nonce per entry from a random per-package salt), so a hex editor, `strings`
  or an archive tool shows neither file names nor file contents. Generic "game asset
  extractor" tools don't know the format.
- No casual modding by file replacement: dropping a changed PNG next to the game does
  nothing, because loose files aren't read while the package is mounted.
- Integrity: every entry carries a CRC32, so a damaged download fails loudly (the entry
  reads as missing and the log names it) instead of rendering garbage.

What it does not do:

- **The key ships with the game.** It has to: the game decrypts its own assets. It is
  masked so it isn't one plain run of bytes in the executable, but anyone with a debugger
  can stop the game after `GetPackageKey()` and read it, or simply dump decrypted assets
  from memory or from the GPU. Every shipped game's encryption has this property;
  determined people will get your assets.
- **No authentication.** There is no MAC or signature. Someone who has the key can build
  a modified package that the game will load. Don't rely on packages for anti-cheat or for
  anything a player could gain by changing.
- **Caches are not encrypted.** Compiled shaders (SPIR-V/DXBC) and font atlases are
  written to `.cache/` beside the executable (or the user's data directory) on first run.
  They are derived data, not your source files, but they are readable.
- **Not for secrets.** Don't put API keys, credentials or anything else private in a
  package.

### Keeping the key

The examples commit their `dpak.key` files so that anyone can build and pack them. A real
game should keep its key out of a public repository: generate it once, store it as a CI
secret, and write it to `dpak.key` before running premake in the release job. A leaked
key only matters as much as the threat model above says it does, but there is no reason to
publish it.

## Format

For the curious (the code is `src/DingoEngine/Asset/Pack/`): a 64-byte header (magic
`DPAK`, version, flags, 16-byte salt, the table of contents' offset, sizes and CRC32, the
entry count, and 8 bytes of key check so a wrong key fails as such), then the entry blobs,
then the table of contents (one 42-byte record per entry: path hash, offset, packed and raw
sizes, CRC32, flags, followed by the UTF-8 name). Entries and the table of contents are
deflated (stb's zlib; an entry is stored raw when deflate saves under 5%, and `.png`,
`.jpg`, `.ogg` and `.mp3` are never deflated), then encrypted. An entry is at most 2 GiB.
A package is read with positional reads, so the loader thread and the main thread read it
at once.

## Limits

- A `.dpak` is deterrence, not protection: the key ships with the game, and there is no signature
  ([Threat model](#threat-model)).
- Hot-reload never fires for a packed file, and an entry is at most 2 GiB.
- `exeMetadata` is for the examples and games; the DingoPack tool keeps none. Its version
  resource and icon are Windows-only, and on Linux it becomes a desktop entry installed by
  `install-desktop-entry.sh`. A Linux window itself shows no icon.
- `exeMetadata`'s `Version` must look like `1.2` or `1.2.3`.
- Shader and font-atlas caches are not encrypted ([Threat model](#threat-model)).
