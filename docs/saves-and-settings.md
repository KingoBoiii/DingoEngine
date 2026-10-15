# Saves & Settings

v1.0 adds a small save system: `SaveData`, a typed tree written as JSON; `SaveSlots`, named save
files in the player's folder; and `Settings`, a ready-made slot for display, audio and input
options. It saves what you put in it. There is no scene serialization: a game writes the state it
needs (a level, a position, an inventory) and rebuilds the rest when it loads.

```cpp
#include <DingoEngine/Core/SaveData.h>
#include <DingoEngine/Core/Settings.h>
```

## SaveData

A `SaveData` value is null, a bool, an integer (`int64_t`), a float (`double`), a string, an array
or an object. Assigning a value sets its type; `operator[]` with a key turns a null into an object
and adds the member, `Push` turns it into an array:

```cpp
SaveData save;
save["level"] = 3;
save["player"]["name"] = "Wanderer";
save["player"]["position"] = player.GetComponent<Transform3DComponent>().Position;  // glm::vec3
save["player"]["health"] = 0.75f;
save["difficulty"] = Difficulty::Hard;   // an enum is stored as its integer
for (const Item& item : inventory)
    save["inventory"].Push(item.Name);
```

`glm::vec2/3/4`, `ivec2/3` and `quat` are written as arrays of numbers (a quaternion as x, y, z,
w). Reading never throws: each `As*` takes a fallback for a value of another type, and `Get` reads
a member by key with a fallback for a missing one. Read through a const reference, as below: the
non-const `operator[]` creates every member it names and turns a value of another type into an empty
object, so reading a loaded save that way and saving it again rewrites the file.

```cpp
const SaveData& save = *loaded;   // from ReadFile or SaveSlots::Read
const int level = save.Get("level", 1);
const glm::vec3 position = save["player"]["position"].AsVec3();
const std::string name = save["player"].Get("name", std::string("Nobody"));
const Difficulty difficulty = save.Get("difficulty", Difficulty::Normal);
for (size_t i = 0; i < save["inventory"].Size(); i++)
    AddItem(save["inventory"][i].AsString());
```

A number reads as either kind: an integer as a float, and a float as an integer when it is whole.
A float that isn't finite or is beyond `float`'s range (`1e400` in a file reads as infinity) gives
the fallback in `AsFloat`, `Get<float>` and the vector and quaternion reads, and so does an `ivec`
item outside `int`'s range.
`Get<T>` with an integer type gives the fallback for a value outside `T`'s range, except a 64-bit
unsigned `T`: a `uint64_t` from 2^63 up is stored as the negative integer with the same bits, so
`Get<uint64_t>` (a UUID, say) reads the bits back, and a negative value reads as a large one. The const
`operator[]` returns a null for a missing key, so a chain of lookups on an old save without the
member falls through to the fallback. An object keeps its keys sorted, so the same data always
writes the same file.

`ToJson(pretty)` and `SaveData::FromJson(text, &error)` convert directly. JSON has no NaN or
infinity, so a non-finite float is written as null (with a warning). Strings are UTF-8 (a `u8""`
literal works too); invalid bytes come back as U+FFFD. `FromJson` refuses nesting deeper than 256.

Misuse never corrupts the tree: `operator[]` with a key on a value that isn't an object makes it
an empty object, `Push` on one that isn't an array an empty array (both logged as errors), and the
non-const `At` past the end hands back a scratch value. `nullptr` is a null; any other pointer
doesn't compile (it would have been a bool). A reference from `operator[]`, `Find` or `Push`
survives adding other members, so `save["backup"] = save["player"]` copies safely; removing that
member, `Clear`, or assigning or destroying its parent ends it.

## Files

```cpp
save.WriteFile(path);                          // false (logged) on failure
std::optional<SaveData> loaded = SaveData::ReadFile(path, &error);
```

A write is atomic. The JSON goes to `<path>.tmp` and is flushed to the disk; then the current file
becomes `<path>.bak` and the new one takes its place. A crash, a kill or a power cut at any moment
leaves a complete save under one of those names. `ReadFile` reads `<path>`; when it is missing (a
write stopped between its two renames) a complete `<path>.tmp`, then `<path>.bak`; when it doesn't
parse, `<path>.bak`, logging which it used. The next write first moves such a `.tmp` to `<path>`,
so the generation `ReadFile` returned becomes the `.bak`. Writes from several threads take turns.
`Dingo-UnitTests` checks all this by killing a process in the middle of its writes.

## Save slots

`SaveSlots` names the files for you, in `Application::GetSaveDirectory()`:

| | |
|---|---|
| `Write(slot, data)` / `Read(slot)` | `<slot>.json`, through `WriteFile` / `ReadFile`. |
| `Exists(slot)` / `Delete(slot)` | A slot exists when its file or its `.bak` is on disk, or a complete `.tmp` without the file. `Delete` removes the `.bak` and any leftover `.tmp` too. |
| `List()` | Every slot `Exists` reports, most recently written first, with its `LastWriteTime`. |
| `IsValidName(slot)` | 1 to 64 of `A-Z a-z 0-9 _ -`, and not a Windows device name (`CON`, `PRN`, `AUX`, `NUL`, `COM0`–`COM9`, `LPT0`–`LPT9`, in any case), which can't be saved there. The rule is the same on every platform, so saves move between them. Any other name fails with an error. |

The directory is `GetUserDataDir(Name)/saves` (`%LOCALAPPDATA%\<Name>\saves`,
`~/.local/share/<Name>/saves`), beside the logs. `ApplicationParams::SaveDirectory` moves it, and
`--save-dir=<path>` on the command line overrides both, for a test run that must not touch the
player's saves.

## Settings

`Settings` holds the options most games offer:

| | |
|---|---|
| `Display` | `Mode` (`Windowed`, `BorderlessFullscreen`, `ExclusiveFullscreen`), the windowed `Width`/`Height`, exclusive fullscreen's `FullscreenMode` and `VSync` |
| `Audio` | `MasterVolume`, and `BusVolumes` by bus name |
| `Input` | `GamepadDeadzone` |
| `Game` | a `SaveData` object for the game's own options, saved as it is |

```cpp
Settings settings = Settings::Load();         // the "settings" slot over the current state
settings.Audio.BusVolumes["Music"] = 0.6f;
settings.Display.Mode = WindowMode::BorderlessFullscreen;
settings.Game["invertY"] = true;
settings.Apply();                              // window, volumes, deadzone
settings.Save();
```

`Capture()` reads the running app: the window's mode and windowed size, VSync, the master volume,
the deadzone, and the volume of each bus the settings already name (the engine can't list a game's
buses). `Load()` reads the slot over `Capture()`, so an entry the file lacks keeps the current
value; malformed entries are ignored and out-of-range ones clamped. `Apply()` changes only what
differs, so applying unchanged settings doesn't blink the window. It sets the volumes of the buses
that exist, so create the game's buses first. A [headless](application-and-layers.md#headless)
app has no window: `Capture()` keeps its base's `Display` and `Apply()` skips it.

The launch flags `--window=WxH` and `--no-vsync` hold the window for the whole run without
touching what the player chose. Under `--window`, `Apply()` stores the mode and size it is given
without changing the window, and `Capture()` returns them. Under `--no-vsync`, the swap chain stays
off while `Apply()` and `Capture()` keep the VSync setting. So a menu that saves during such a run
saves the player's settings, not the flag's (see
[Command-line arguments](application-and-layers.md#command-line-arguments)).

To open the window in the saved mode rather than switching after it appears, name the slot in
`ApplicationParams`:

```cpp
ApplicationParams params{
    .CommandLineArgs = args,
    .Name = "MyGame",
    .SettingsSlot = std::string(Settings::k_DefaultSlot),
};
```

The window then starts from the slot's display settings, and its master volume and deadzone are
set at start. Bus volumes still wait for the game's `Settings::Load().Apply()` after it creates its
buses.

## Display modes

`Window::GetDisplayModes()` lists the modes of the monitor the window is on, largest first and
without duplicates, for a resolution list; `GetDesktopDisplayMode()` is that monitor's own mode.
See [Window modes](application-and-layers.md#window-modes) for switching.

## Limits

- There is no scene serialization and no save versioning: a game writes the state it needs and
  reads an old save through `Get` fallbacks.
- A save is plain JSON, neither encrypted nor signed.
- Numbers are `int64_t` and `double`; a non-finite float is written as null, and `FromJson` refuses
  nesting deeper than 256.
- Slot names are 1 to 64 of `A-Z a-z 0-9 _ -`, and not a Windows device name.
- `Settings` keeps the volume of the buses it already names only, since the engine can't list a
  game's buses, and `Apply()` sets only buses that exist when it runs.
