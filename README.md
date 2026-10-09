# Spyro Ultrawide Pack: source

Source code for the mod [Ultrawide Pack with Full-Width Menus and FOV Hotkeys](https://www.nexusmods.com/spyroreignitedtrilogy/mods/288) for Spyro Reignited Trilogy (PC).

The mod download contains three things. This repository holds the source of the two parts that were written for the mod.

| Part | Where the source is |
|---|---|
| FOV hotkeys plugin (`SpyroFOVHotkeys.asi`) | [`fov_hotkeys/`](fov_hotkeys) in this repository |
| Full-width menus (`UltrawideMenuBackgrounds_*_P.pak`) | [`menu_backgrounds/`](menu_backgrounds) in this repository |
| Ultrawide fix (`SpyroReignitedTrilogy.WidescreenFix.asi`) and ASI loader (`dsound.dll`) | Not in this repository. They are included in the mod unchanged from [WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack) and [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by ThirteenAG, both MIT licensed. The fix was written by xan1242. |

## Files shipped in the All-in-One download

| File | Origin | SHA-256 |
|---|---|---|
| `dsound.dll` | Ultimate ASI Loader, from the [WidescreenFixesPack "spyro" release](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/spyro) | `09313d37a775eaeceddb5f17d8ac4813fb4ee8cf5ef73d83d2470f90aa67b424` |
| `SpyroReignitedTrilogy.WidescreenFix.asi` | same release | `531cb20b81a70c292277e1f01da781ca37df3902824985069e0f7ca3aa6c22c7` |
| `SpyroFOVHotkeys.asi` | built from `fov_hotkeys/fovkeys.c` | `3c673c25b80d22667bcde08e81a1d3b24039d1c56f8a4e9749eaf761023ebcd8` |

## FOV hotkeys plugin

`SpyroFOVHotkeys.asi` is a small DLL (9,728 bytes) that the Ultimate ASI Loader loads from the game's `scripts` folder, next to the widescreen fix.

What it does:

- The widescreen fix multiplies the camera FOV by one number, `FOVScalar`, which it reads once from its ini file.
- The plugin changes that number in memory when the player presses a hotkey, and writes the new value to the same ini file.
- Default keys: Numpad + (wider), Numpad - (narrower), Numpad * (reset). They can be changed in `SpyroFOVHotkeys.ini`.

What it touches:

- It imports only `kernel32` and `user32` functions. There is no C runtime, no network code and no other dependency.
- It reads the three configured keys with `GetAsyncKeyState`, and only while the game window is the active window.
- It writes one `float` inside the loaded widescreen fix, and it rewrites the `FOVScalar` line of `SpyroReignitedTrilogy.WidescreenFix.ini`.
- It only acts when the loaded fix is exactly the released build (checked by build timestamp, image size and the bytes of the two instructions that use the value). With any other build it does nothing.

### Build

Requires [zig](https://ziglang.org/download/) 0.17.0. Zig is used as a C cross compiler, so this works on Linux, macOS and Windows.

```sh
cd fov_hotkeys
ZIG=/path/to/zig ./build.sh
```

`build.sh` runs:

```sh
zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -shared -s \
  -nostdlib -fno-builtin -ffreestanding -fno-stack-protector -fno-sanitize=all \
  -isystem <zig>/lib/libc/include/any-windows-any \
  -Wl,-e,DllMain -o SpyroFOVHotkeys.asi fovkeys.c -lkernel32 -luser32
```

Building with zig 0.17.0 on Linux gave the file with the SHA-256 listed above.

### Tests

- `tests/test_native.c` tests the ini parsing and rewriting on any system: `gcc -fsanitize=address,undefined -o test tests/test_native.c && ./test`
- `tests/harness_windows.c` loads the real widescreen fix and a test build of the plugin (`-DFOVKEYS_SIM`, which replaces the keyboard by a variable) in a small Windows program and checks taps, key repeat, reset, the limits and the ini file.

## Full-width menus

The game draws every menu inside a fixed 1920x1080 canvas that is centred on the screen. The scripts change the left and right margins of the background and overlay layers in the cooked widget files, so those layers extend past the canvas and cover an ultrawide screen. No textures are changed.

- `uasset.py` is a minimal reader for cooked Unreal Engine 4.19 packages.
- `widen_backgrounds.py` finds the layers, changes their margins and fixes the sizes and offsets in the package afterwards.

### Build

Requires Python 3 and [repak](https://github.com/trumank/repak).

1. Extract the original files from the game:

   ```sh
   repak unpack -o orig \
     -i Falcon/Plugins/UserInterface/Content/MenuScreens \
     -i Falcon/Plugins/UserInterface/Content/HUD \
     "<game>/Falcon/Content/Paks/pakchunk0-WindowsNoEditor.pak"
   repak unpack -o orig \
     -i Falcon/Plugins/Characters/Player/CPS3341_SgtByrd/Content/Blueprints \
     "<game>/Falcon/Content/Paks/pakchunk1-WindowsNoEditor.pak"
   ```

2. Widen the layers. The last number is the overflow per side, in units of the 1920x1080 menu canvas (340 for 21:9, 960 for 32:9):

   ```sh
   python3 menu_backgrounds/widen_backgrounds.py orig mod 340
   ```

3. Pack:

   ```sh
   repak pack --version V4 -m ../../../ mod UltrawideMenuBackgrounds_21x9_P.pak
   ```

The game's own files are not in this repository. You need your own copy of the game to build the .pak.

## Credits

- xan1242 for the Spyro Reignited Trilogy Widescreen Fix, and ThirteenAG for WidescreenFixesPack and the Ultimate ASI Loader
- killer-m and Brandondorf9999 for the earlier Flawless Widescreen plugin
- trumank for repak
