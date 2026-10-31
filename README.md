# Moonlight PC

[Moonlight PC](https://moonlight-stream.org) is an open source PC client for NVIDIA GameStream and [Sunshine](https://github.com/LizardByte/Sunshine).

Moonlight also has mobile versions for [Android](https://github.com/moonlight-stream/moonlight-android) and [iOS](https://github.com/moonlight-stream/moonlight-ios).

You can follow development on our [Discord server](https://moonlight-stream.org/discord) and help translate Moonlight into your language on [Weblate](https://hosted.weblate.org/projects/moonlight/moonlight-qt/).

 [![Build](https://img.shields.io/github/actions/workflow/status/moonlight-stream/moonlight-qt/build.yml?branch=master)](https://github.com/moonlight-stream/moonlight-qt/actions/workflows/build.yml?query=branch%3Amaster)
 [![Downloads](https://img.shields.io/github/downloads/moonlight-stream/moonlight-qt/total)](https://github.com/moonlight-stream/moonlight-qt/releases)
 [![Translation Status](https://hosted.weblate.org/widgets/moonlight/-/moonlight-qt/svg-badge.svg)](https://hosted.weblate.org/projects/moonlight/moonlight-qt/)

## Features
 - Hardware accelerated video decoding on Windows, Mac, and Linux
 - H.264, HEVC, and AV1 codec support (AV1 requires Sunshine and a supported host GPU)
 - YUV 4:4:4 support (Sunshine only)
 - HDR streaming support
 - 7.1 surround sound audio support
 - 10-point multitouch support (Sunshine only)
 - Gamepad support with force feedback and motion controls for up to 16 players
 - Support for both pointer capture (for games) and direct mouse control (for remote desktop)
 - Support for passing system-wide keyboard shortcuts like Alt+Tab to the host

## Additions in this fork

This fork adds the following on top of upstream Moonlight PC. Builds for every platform are available from this repository's [GitHub Actions](https://github.com/FzeNiX/moonlight-qt/actions) (open a run and download from **Artifacts**).

### Performance stats overlay
- The stats are drawn on a translucent dark panel with light text, so they stay readable on bright and dark scenes.
- Three layouts: **Full** (every statistic with descriptions), **Compact** (a few short, aligned lines) and **Mini** (a single line).
- The overlay can sit in the **top left** or **top right** corner.
- **Full** and **Compact** include a **frame time graph** with two lines: `net` is the interval between frames arriving from the network (shows Wi-Fi jitter and lost frames), and `display` is the interval between frames being shown (shows V-Sync misses and client-side stutter). The legend shows the worst value of each line, and a dotted line marks the target frame interval.
- Settings: **Settings → Advanced Settings** (stats layout and position).

### Image quality
- **Video sharpening** using AMD FidelityFX Super Resolution 1.0 RCAS, which restores detail softened by video compression.
- **Brightness, contrast and saturation** adjustments, for example when the stream looks more washed out than the host's screen. These are folded into the YUV→RGB conversion, so they add no rendering cost.
- Settings: **Settings → Basic Settings**. Both can also be changed live while streaming from the stream menu (see below).
- Renderer support:

  | Renderer | Sharpening | Color adjustments |
  |---|---|---|
  | Direct3D 11 (Windows default) | ✅ | ✅ |
  | Vulkan / libplacebo (macOS default, Linux HDR) | ✅ | ✅ |
  | EGL (Linux SDR default) | ❌ | ✅ |
  | Others (DXVA2, VAAPI/VDPAU/DRM direct, Metal, AVSampleBufferDisplayLayer) | ❌ | ❌ |

  On Linux, enabling sharpening prefers the Vulkan renderer.

### In-stream menu
Press **Ctrl+Alt+Shift+A** while streaming to open the stream menu. While it is open, the mouse is released and mouse/keyboard input goes to the menu instead of the host.
- **Sliders** for sharpness, brightness, contrast and saturation. Drag or click a slider, or scroll over a row to step it. Changes apply instantly to the current stream only and are not saved. **Reset** restores the values from your settings.
- **Clipboard sync** with [Apollo](https://github.com/ClassicOldSong/Apollo) hosts: send this PC's clipboard to the host, or copy the host's clipboard to this PC (see [Apollo hosts](#apollo-hosts)).
- **Connected controllers**, with their battery or wired status and the controller type reported to the host.
- Settings the current renderer can't apply are shown as unavailable.

| Key (menu open) | Action |
|---|---|
| Up / Down | Select a slider or button |
| Left / Right | Adjust the selected slider |
| Enter / Space | Activate the selected button |
| R | Reset the sliders to your settings |
| Esc (or click outside the menu) | Close the menu |

### Per-PC settings
Right-click a PC (or open its menu) and choose **PC Settings**:
- **Preferred address**: Automatic, Local network, Remote/public IP, IPv6, or the manually added address/DDNS name. The preferred address is tried first when connecting, and Moonlight falls back to the other known addresses if it doesn't respond. To use a DDNS name, add the PC again with **Add PC** using that name; Moonlight merges it into the existing PC.
- **Custom stream settings for this PC**, which replace the global settings for streams from that PC: resolution (including 16:10, 21:9 and 32:9 presets, or a custom size), frame rate, bitrate (or automatic for the chosen resolution and frame rate), display mode, controller type, Apollo virtual display and render scale, HDR, remote desktop mouse mode, performance stats, sharpening and color adjustments.

The global settings are never changed by a PC's custom settings. Streams started from the command line keep using their command-line options, but the preferred address still applies.

### Controllers
- **Controller type sent to host**: Automatic, Xbox, or PlayStation (DualShock 4). When the host's gamepad setting is `auto` (the default in Sunshine and Apollo), the host emulates the controller type the client reports. Choosing Xbox also hides motion and touchpad support, which would otherwise make the host pick a DualShock 4. Windows hosts can emulate Xbox 360 and DualShock 4 controllers.
- Settings: **Settings → Gamepad Settings**, or per PC in **PC Settings**.

### Apollo hosts
[Apollo](https://github.com/ClassicOldSong/Apollo) is a Sunshine fork with extra host features. This fork supports:
- **Virtual display**: Apollo creates a virtual monitor that matches this PC's resolution and frame rate for the stream, then removes it afterwards. No dummy plug or resolution scripts are needed on the host. Requires a Windows host. Setting: **Settings → Host Settings**, or per PC.
- **Host render scale** (50–200%): Apollo renders the host display at this scale of the stream resolution, then encodes it at the stream resolution. Above 100% supersamples for a sharper image; below 100% reduces the host's GPU load. Setting: **Settings → Host Settings**, or per PC.
- **Clipboard sync** from the in-stream menu. In Apollo's web UI, grant this client the clipboard read/set permissions.
- **Permission awareness**: Apollo grants full permissions only to the first paired client, while later clients can only list apps and view streams. Moonlight now:
  - warns before streaming when Apollo hasn't allowed this PC to launch apps or send keyboard, mouse or controller input;
  - explains permission problems when Apollo rejects a launch;
  - shows the granted and missing permissions, plus the virtual display status, in the PC's **View Details**;
  - greys out the clipboard buttons when the matching permission is missing.

Grant permissions for each client in Apollo's web UI.

### Keyboard shortcuts
All shortcuts use **Ctrl+Alt+Shift** plus a key while streaming. Shortcuts added by this fork are marked with ★.

| Shortcut | Action |
|---|---|
| Ctrl+Alt+Shift+Q | End the stream |
| Ctrl+Alt+Shift+E | End the stream and exit Moonlight |
| Ctrl+Alt+Shift+Z | Release / recapture the mouse and keyboard |
| Ctrl+Alt+Shift+X | Toggle full-screen |
| Ctrl+Alt+Shift+D | Minimize the stream window |
| Ctrl+Alt+Shift+S | Show / hide performance stats |
| ★ Ctrl+Alt+Shift+O | Cycle the stats layout: Full → Compact → Mini |
| ★ Ctrl+Alt+Shift+P | Move the stats overlay between top left and top right |
| ★ Ctrl+Alt+Shift+A | Open / close the in-stream menu |
| Ctrl+Alt+Shift+M | Toggle mouse mode (games / remote desktop) |
| Ctrl+Alt+Shift+C | Show / hide the local cursor (remote desktop mouse mode) |
| Ctrl+Alt+Shift+L | Toggle confining the mouse to the stream window |
| Ctrl+Alt+Shift+K | Toggle capturing system shortcuts such as Alt+Tab |
| Ctrl+Alt+Shift+V | Type the clipboard's text on the host |

### Gamepad shortcuts
Hold the buttons together while streaming. On Xbox controllers, Select is View, Start is Menu, and L1/R1 are LB/RB.

| Buttons | Action |
|---|---|
| Start+Select+L1+R1 | End the stream |
| Select+L1+R1+X | Show / hide performance stats |
| ★ Select+L1+R1+Y | Cycle the stats layout |
| ★ Select+L1+R1+B | Move the stats overlay between top left and top right |
| Hold Start | Toggle controller mouse mode (if enabled in settings) |

### New command-line options
For `moonlight stream`:

| Option | Values |
|---|---|
| `--performance-overlay-mode` | `full`, `compact`, `mini` |
| `--performance-overlay-position` | `left`, `right` |
| `--video-sharpening` / `--no-video-sharpening` | |
| `--video-sharpness` | `0`–`100` |
| `--controller-type` | `auto`, `xbox`, `playstation` |
| `--virtual-display` / `--no-virtual-display` | Apollo hosts |
| `--render-scale` | `50`–`200` (Apollo hosts) |
 
## Downloads
- [Windows, macOS, and Steam Link](https://github.com/moonlight-stream/moonlight-qt/releases)
- [Snap (for Ubuntu-based Linux distros)](https://snapcraft.io/moonlight)
- [Flatpak (for other Linux distros)](https://flathub.org/apps/details/com.moonlight_stream.Moonlight)
- [AppImage](https://github.com/moonlight-stream/moonlight-qt/releases)
- [Raspberry Pi 4 and 5](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Raspberry-Pi-4)
- [Generic ARM 32-bit and 64-bit Debian packages](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-ARM%E2%80%90based-Single-Board-Computers) (not for Raspberry Pi)
- [Experimental RISC-V Debian packages](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-RISC%E2%80%90V-Single-Board-Computers)
- [NVIDIA Jetson and Nintendo Switch (Ubuntu L4T)](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Linux4Tegra-(L4T)-Ubuntu)

### Nightly Builds
- [Downloads](https://nightly.link/moonlight-stream/moonlight-qt/workflows/build/master)

#### Special Thanks

[![Hosted By: Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.com)

Hosting for Moonlight's Debian and L4T package repositories is graciously provided for free by [Cloudsmith](https://cloudsmith.com).

## Building

### Windows Build Requirements
* Qt 6.11 SDK or later (earlier versions may work but are not officially supported)
* [Visual Studio 2026](https://visualstudio.microsoft.com/downloads/) (Community edition is fine)
* Select **MSVC** option during Qt installation. MinGW is not supported.
* [7-Zip](https://www.7-zip.org/) (only if building installers for non-development PCs)
* Graphics Tools (only if running debug builds)
  * Install "Graphics Tools" in the Optional Features page of the Windows Settings app.
  * Alternatively, run `dism /online /add-capability /capabilityname:Tools.Graphics.DirectX~~~~0.0.1.0` and reboot.

### macOS Build Requirements
* Qt 6.11 SDK or later (earlier versions may work but are not officially supported)
* Xcode 15 or later (earlier versions may work but are not officially supported)
* [create-dmg](https://github.com/sindresorhus/create-dmg) (only if building DMGs for use on non-development Macs)

### Linux/Unix Build Requirements
* Qt 6 is recommended, but Qt 5.12 or later is also supported (replace `qmake6` with `qmake` when using Qt 5).
* GCC or Clang
* FFmpeg 4.0 or later
* Install the required packages:
  * Debian/Ubuntu:
    * Base Requirements: `libegl1-mesa-dev libgl1-mesa-dev libopus-dev libsdl2-dev libsdl2-ttf-dev libssl-dev libavcodec-dev libavformat-dev libswscale-dev libva-dev libvdpau-dev libxkbcommon-dev wayland-protocols libdrm-dev`
    * Qt 6 (Recommended): `qt6-base-dev qt6-declarative-dev libqt6svg6-dev qt6-wayland qml6-module-qtquick-controls qml6-module-qtquick-templates qml6-module-qtquick-layouts qml6-module-qtqml-workerscript qml6-module-qtquick-window qml6-module-qtquick`
    * Qt 5: `qtbase5-dev qt5-qmake qtdeclarative5-dev qtquickcontrols2-5-dev qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2 qml-module-qtquick2 qtwayland5`
  * RedHat/Fedora (RPM Fusion repo required):
    * Base Requirements: `openssl-devel SDL2-devel SDL2_ttf-devel ffmpeg-devel libva-devel libvdpau-devel opus-devel pulseaudio-libs-devel alsa-lib-devel libdrm-devel`
    * Qt 6 (Recommended): `qt6-qtsvg-devel qt6-qtdeclarative-devel`
    * Qt 5: `qt5-qtsvg-devel qt5-qtquickcontrols2-devel`
* Building the Vulkan renderer requires a `libplacebo-dev`/`libplacebo-devel` version of at least v7.349.0 and FFmpeg 6.1 or later.

### Steam Link Build Requirements
* [Steam Link SDK](https://github.com/ValveSoftware/steamlink-sdk) cloned on your build system
* STEAMLINK_SDK_PATH environment variable set to the Steam Link SDK path

**Steam Link Hardware Limitations**  
Moonlight builds for Steam Link are subject to hardware limitations of the Steam Link device:
* Maximum resolution: **1080p (1920x1080)**
* Maximum framerate: **60 FPS**
* Maximum video bitrate: **40 Mbps**
* **HDR streaming is not supported** on the original hardware

### Docker containers
If you want to use Docker for building, look at [this repo](https://github.com/cgutman/moonlight-packaging) containing canonical containers
for different architectures, which handle building deps and extra linking for you.

### Build Setup Steps
1. Install the latest Qt SDK (and optionally, the Qt Creator IDE) from https://www.qt.io/download
    * You can install Qt via Homebrew on macOS, but you will need to use `brew install qt --with-debug` to be able to create debug builds of Moonlight.
    * You may also use your Linux distro's package manager for the Qt SDK as long as the packages are Qt 5.12 or later.
    * This step is not required for building on Steam Link, because the Steam Link SDK includes Qt 5.14.
2. Download submodules and dependencies
    * Run `git submodule update --init --recursive` from within `moonlight-qt/`.
    * On Windows and macOS, you must also run `setup-deps.ps1` (Windows) or `setup-deps.py` (macOS).
    * Perform these steps each time you pull new changes from the Git repository.
3. Open the project in Qt Creator or build from qmake on the command line.
    * To build a binary for use on non-development machines, use the scripts in the `scripts` folder.
        * For Windows builds, use `scripts\build-arch.bat` and `scripts\generate-bundle.bat`. Execute these scripts from the root of the repository within a Qt command prompt. Ensure  7-Zip binary directory is on your `%PATH%`.
        * For macOS builds, use `scripts/generate-dmg.sh`. Execute this script from the root of the repository and ensure Qt's `bin` folder is in your `$PATH`.
        * For Steam Link builds, run `scripts/build-steamlink-app.sh` from the root of the repository.
    * To build from the command line for development use on macOS or Linux, run `qmake6 moonlight-qt.pro` then `make debug` or `make release`.
        * The final binary will be placed in `app/moonlight`.
    * To create an embedded build for a single-purpose device, use `qmake6 "CONFIG+=embedded" moonlight-qt.pro` and build normally.
        * This build will lack windowed mode, Discord/Help links, and other features that don't make sense on an embedded device.
        * For platforms with poor GPU performance, add `"CONFIG+=gpuslow"` to prefer direct KMSDRM rendering over GL/Vulkan renderers. Direct KMSDRM rendering can use dedicated YUV/RGB conversion and scaling hardware rather than slower GPU shaders for these operations.

## Contribute
1. Fork us
2. Write code
3. Send Pull Requests

Check out our [website](https://moonlight-stream.org) for project links and information.
