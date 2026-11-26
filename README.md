![BarnyardVR](docs/images/BarnyardVRWithIcon.png)

BarnyardVR is a community-made OpenXR VR port of the 2006 video game **Barnyard**, based on [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard).

## Features

- OpenXR support with stereoscopic VR rendering, 6DOF head tracking and a desktop mirror.
- VR controller support with vibration, custom button prompts, gamepad-like stick response and optional modern control remapping.
- Full first-person support in the Barnyard and suitable minigames.
- Snap/smooth turning options and head-relative movement/UI options.
- VR-specific minigame improvements, including head-directed aiming and rendering suitable UI in 3D space.
- Adjustable VR resolution and anti-aliasing options, with an FPS debug HUD to help tune performance.

## Installation

1. Install a Windows copy of Barnyard and have an OpenXR-compatible PCVR headset and runtime set up.
2. Download the [latest Windows release ZIP file](https://github.com/Break-Ben/BarnyardVR/releases/latest).
3. (Optional but recommended) Copy your game files to a separate folder first to avoid conflicts with other mods or changes to your original installation.
4. Extract the ZIP contents into the game directory, alongside the existing `Barnyard.exe`.
5. Start your PCVR headset/runtime and launch `BarnyardVR.exe` from the game folder.

## Settings

BarnyardVR contains options that are configurable in-game via **Options → VR Settings**. Defaults are shown in bold. Some changes require a restart.

| Setting | Values | Description |
| --- | --- | --- |
| Control Layout | **Modern**, Classic | Modern remaps controls for VR controllers and modern conventions. Classic uses the original gamepad layout. |
| Snap Turning | **Disabled**, 15°, 30°, 45°, 60°, 90° | Turn smoothly when disabled, or turn by the selected angle. |
| Toggle Sprint | **Off**, On | Press sprint to toggle it; on foot it stops when you stop moving, and on a bike boost stops when you release the accelerator or brake. Both stop when stamina runs out. |
| Movement | **Fixed**, Head Relative | While on foot, move relative to the VR forward direction when fixed, or your headset direction when head-relative. |
| UI Position | **Fixed**, Head Relative | Keep the interface at the VR forward direction when fixed, or let it follow your headset direction (where supported) when head-relative. |
| Vibration | **On**, Off | Toggle vibration on VR controllers and gamepads. |
| Resolution (Requires Restart) | 50–200%; **100%** | Change VR render resolution factor. Higher values are recommended to improve visual clarity (since the game isn't demanding), but it may cost performance on weaker computers. |
| MSAA (Requires Restart) | Off, **2x**, 4x, 8x | Smooth jagged edges. Higher levels can reduce performance. Levels unsupported by your hardware fall back to a lower level. |
| Debug HUD | **Off**, On | Display an overlay with info such as FPS, resolution, MSAA, and controller stick inputs. |

## Content Disclosure

- BarnyardVR works with the original Barnyard game, created and owned by [THQ Nordic](https://www.thqnordic.com) without the use of AI. Its files are not included in this repository.
- This port is forked from and heavily based on [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard). Thank you to [InfiniteC0re](https://github.com/InfiniteC0re) for extensive manual reverse engineering and AI-assisted decompilation work - this project would not be possible without it!
- AI was used heavily to write and adapt code for the BarnyardVR port.

## Notes

> [!IMPORTANT]
> **SUPPORT THE DEVELOPERS OF THE TOSHI ENGINE**
>
> We do not own the Toshi Engine in any way, shape or form and this project would not have been possible had they not developed TOSHI in the first place. The Toshi Engine is currently owned by [THQ Nordic](https://www.thqnordic.com); we highly urge you to follow & support their projects if you like this project!

> [!WARNING]
> **DO NOT USE THIS DECOMPILATION PROJECT AS A MEANS TO PIRATE BARNYARD OR ANY OTHER TOSHI GAMES!**
>
> We do not condone using this project as a means for piracy in any form. This project was made with love and care for the source material and was created for purely educational purposes.

## Building

### Windows

Building on Windows requires:

- Visual Studio 2026 with the Desktop development with C++ workload and Windows SDK

Clone the repository with the following commands:
```bat
git clone https://github.com/Break-Ben/BarnyardVR.git
cd BarnyardVR
git submodule update --init --recursive
.\GenerateProjects-WIN-DX11-VS2026.bat
```
Then open the generated solution file with Visual Studio and build it. The Windows game executable is named `BarnyardVR.exe`.

## Contributing

The project is open to any kind of contributions! Report issues on [GitHub](https://github.com/Break-Ben/BarnyardVR/issues) or Discord.
