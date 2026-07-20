thumbgrid
=====
Image viewer. Fast, easy to use. Optional video support.

> **thumbgrid** is a hard fork of [easymodo/qimgv](https://github.com/easymodo/qimgv),
> originally written by easymodo and its contributors. It remains free software
> under the GNU GPL v3. The original copyright and attribution are retained — see
> [`NOTICE`](NOTICE) and [`LICENSE`](LICENSE).

## Development process

thumbgrid is maintained with extensive use of coding agents. Most fork-specific
code, tests, documentation, and maintenance changes have been drafted or
implemented by agents under maintainer direction.

The maintainer sets project priorities, evaluates user-facing behavior, and
runs available builds and tests, but does not claim C++ expertise. Agent-produced
changes should therefore be treated as AI-assisted work and are accepted based
on available validation rather than asserted manual code review.

Agents are development tools, not project authors or copyright holders.
Upstream qimgv attribution and copyright notices remain intact; see `NOTICE`
and `LICENSE`.

**Versioning:** thumbgrid uses calendar versioning (`YYYY.M.N`), independent of
upstream. The **git tag is the single source of truth**: CMake derives the
version from the latest `vYYYY.M.N` tag (`src/appversion.cpp` is generated from
[`appversion.cpp.in`](src/appversion.cpp.in) at build time), and it is reported
by `thumbgrid --version`. See [Releasing](#releasing) to cut a new version.

## Screenshots

Main window         |  Folder view   |  Thumbnails  |  Settings window
:------------------:|:--------------:|:------------:|:-----------------:|
[![Main window](src/distrib/screenshots/image.webp)](src/distrib/screenshots/image.webp) | [![Folder view](src/distrib/screenshots/folder-view.webp)](src/distrib/screenshots/folder-view.webp) | [![Thumbnails](src/distrib/screenshots/thumbnails.webp)](src/distrib/screenshots/thumbnails.webp) | [![Settings](src/distrib/screenshots/preference.webp)](src/distrib/screenshots/preference.webp)

## Key features:

- Fully configurable, including themes, shortcuts

- High quality scaling

- Basic image editing: Crop, Rotate and Resize

- EXIF/IPTC/XMP metadata via exiv2: view tags (with an optional full-metadata
  mode), preserve metadata when saving edits, and strip all metadata for privacy

- Folder view with file management: copy / cut / paste of files and folders
  (symlink-aware), create directory (F7), rename (F2), and an on-disk async
  folder thumbnail cache

- Ability to quickly copy / move images to different folders

- Experimental video playback via libmpv

- Folder view mode

- Ability to run shell scripts

- Qt6-only build (upstream Qt5 support has been dropped)

## Default control scheme:

| Action  | Shortcut |
| ------------- | ------------- |
| Next image  | Right arrow / MouseWheel |
| Previous image  | Left arrow / MouseWheel |
| Goto first image  | Home |
| Goto last image  | End |
| Zoom in  | Ctrl+MouseWheel / Crtl+Up |
| Zoom out  | Ctrl+MouseWheel / Crtl+Down |
| Zoom (alt. method) | Hold right mouse button & move up / down |
| Fit mode: window | 1 |
| Fit mode: width | 2 |
| Fit mode: 1:1 (no scaling) | 3 |
| Switch fit modes  | Space |
| Toggle fullscreen mode  | DoubleClick / F / F11 |
| Exit fullscreen mode | Esc |
| Show EXIF panel  | I |
| Crop image  | X |
| Resize image  | R |
| Rotate left  | Ctrl+L |
| Rotate Right  | Ctrl+R |
| Open containing directory | Ctrl+D |
| Slideshow mode | ~ |
| Shuffle mode | Ctrl+~ |
| Quick copy  | C |
| Quick move  | M |
| Move to trash | Delete |
| Delete file  | Shift+Delete |
| Save  | Ctrl+S |
| Save As  | Ctrl+Shift+S |
| Folder view | Enter / Backspace |
| Open | Ctrl+O |
| Print / Export PDF | Ctrl+P |
| Settings  | P |
| Exit application | Esc / Ctrl+Q / Alt+X / MiddleClick |

Note: you can configure every shortcut by going to __Settings > Shortcuts__

# User interface

The idea is to have a uncluttered, simple and easy to use UI. You can see UI elements only when you need them.

There is a pull-down panel with thumbnails, as well as folder view. You can also bring up a context menu via right click.

## Using quick copy / quick move panels

Bring up the panel with C or M shortcut. You will see 9 destination directories, click on the folder icon to change them.

With panel visible, use 1 - 9 keys to copy/move current image to corresponding directory.

When you are done press C or M again to hide the panel.

## Folder view & file management

In folder view you can manage files directly: copy / cut / paste files and
folders (symlink-aware), create a new directory (F7) and rename (F2). Thumbnails
are cached on disk and generated asynchronously.

## Metadata (exiv2)

thumbgrid reads Exif / IPTC / XMP tags via exiv2 (always built in). The image-info panel (`I`) has an optional full-metadata
mode. Metadata is preserved when saving edits, and a strip-metadata action lets
you remove all metadata for privacy. Both are available from the context menu.

## Running scripts

You can run custom scripts on a current image.

Open __Settings > Scripts__. Press Add. Here you can choose between a shell command and a shell script.

Example of a command:

`convert %file% %file%_.pdf`

Example of a shell script file (`$1` will be image path):
```
#!/bin/bash
gimp "$1"
```
_Note: The script file must be an executable. Also, "shebang" (`#!/bin/bash`) needs to be present._

When you've created your script go to __Settings > Shortcuts__, then select it and assign a shortcut like for any regular action.

## HiDPI (Linux / MacOS only)

If thumbgrid appears too small / too big on your display, you can override the scale factor. Example:
```
QT_SCALE_FACTOR="1.5" thumbgrid /path/to/image.png
```
You can put it in `thumbgrid.desktop` file to make it permanent. Using values less than `1.0` is not supported.

thumbgrid should also obey the global scale factor set in KDE's systemsettings.

## High quality scaling

thumbgrid supports nicer scaling filters when compiled with `opencv` support (ON by default, but might vary depending on your linux distribution). Filter options are available in __Settings > Scaling__. `Bicubic` or `bilinear+sharpen` is recommended.

# Additional image formats

thumbgrid can open some extra formats via third-party image plugins.

| Format  | Plugin |
| ------- | ------------- |
| WebP | Qt ImageFormats (`qt6-imageformats` / `qt6-image-formats-plugins`) |
| JPEG-XL | [github.com/novomesk/qt-jpegxl-image-plugin](https://github.com/novomesk/qt-jpegxl-image-plugin) |
| AVIF | [github.com/novomesk/qt-avif-image-plugin](https://github.com/novomesk/qt-avif-image-plugin) |
| APNG | [github.com/Skycoder42/QtApng](https://github.com/Skycoder42/QtApng) |
| HEIF / HEIC | [github.com/jakar/qt-heif-image-plugin](https://github.com/jakar/qt-heif-image-plugin) |
| RAW | [https://gitlab.com/mardy/qtraw](https://gitlab.com/mardy/qtraw) |

# Installation

On Arch Linux, install from the fork's own pacman repo or from the AUR
(`thumbgrid-bin`) — see [Arch Linux package](#arch-linux-package) below. This
fork does not ship to the upstream distribution channels it inherited from
qimgv (AUR `qimgv-git`, apt/dnf/zypper/pkg, Chocolatey, WinGet); elsewhere,
build from source or grab a CI-built package from the fork's releases.

## Build from source (GNU+Linux)

Requirements: a C++17 compiler (GCC 9+), CMake 3.13+, and Qt6
(`Core Widgets Svg PrintSupport OpenGLWidgets`). Optional features pull in
exiv2 (metadata), OpenCV (HQ scaling) and mpv (video).

The easiest way is the interactive helper, which can install the minimal Qt6
build dependencies for your distro and then configure / build / run:

```
./run.sh
```

Or build directly with CMake:

```
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Exiv2 (metadata), the mpv plugin (video playback) and OpenCV (high quality
scaling) are always built in and require those libraries at build time. The
only feature toggle is:

```
-DKDE_SUPPORT=ON|OFF      # blur behind the window on KDE (default OFF)
```

Install with:

```
sudo cmake --install build
```

## Arch Linux package

**Recommended: pacman repo.** Add to `/etc/pacman.conf`:

```ini
[thumbgrid]
SigLevel = Optional TrustAll
Server = https://do-i.github.io/thumbgrid/arch/$arch
```

Then:

```
sudo pacman -Sy thumbgrid
```

Subsequent `sudo pacman -Syu` runs will pick up rebuilt thumbgrid packages
automatically, including rebuilds triggered by Arch ABI updates to Qt,
Exiv2, OpenCV, or mpv. The repo is published by
[`arch-package.yml`](.github/workflows/arch-package.yml) to the `gh-pages`
branch on every version tag, and can also be re-triggered manually
(`workflow_dispatch` with a bumped `pkgrel`) to republish after an
Arch-side ABI change with no thumbgrid code change.

`SigLevel = Optional TrustAll` means packages are unsigned; treat this as a
personal/low-stakes distribution method until package signing is added.

**AUR:** [`thumbgrid-bin`](https://aur.archlinux.org/packages/thumbgrid-bin)
repackages the same prebuilt release binary, for those who prefer an AUR
helper:

```
paru -S thumbgrid-bin   # or: yay -S thumbgrid-bin
```

The AUR package is updated manually and only for versions promoted as stable,
so it can lag the pacman repo above — which is the path that tracks every
release and ABI rebuild.

**Manual fallback:** download `*.pkg.tar.zst` from a
[GitHub Release](https://github.com/do-i/thumbgrid/releases) and install with
`sudo pacman -U thumbgrid-*.pkg.tar.zst` (no auto-updates via this path).

**Building from source:** a minimal Qt6 PKGBUILD lives in
[`packaging/arch/`](packaging/arch/). From that directory:

```
makepkg -si
```

# Releasing

For maintainers: see [Releasing](CONTRIBUTING.md#releasing) in CONTRIBUTING.md.

# Donate

If you wish to give a few bucks, please consider donating to the original auther: [easymodo/qimgv](https://github.com/easymodo)
