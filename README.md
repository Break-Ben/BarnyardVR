![BarnyardVR](docs/images/BarnyardVRWithIcon.png)

BarnyardVR is a community-made OpenXR VR port of *Barnyard*, based on [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard).

## Features

- OpenXR support with stereoscopic VR rendering, six-degree-of-freedom head tracking, and a desktop mirror.
- VR controller support with vibration, custom button prompts, and optional modern control remapping.
- First-person gameplay.
- Snap turning and independent head-relative movement and UI options.
- VR minigame improvements, including head-directed aiming and lives displayed above players in Mud Jumpers.
- Adjustable VR resolution and anti-aliasing, with an FPS debug HUD to help tune performance.

## Notes

> [!IMPORTANT]
> **SUPPORT THE DEVELOPERS OF THE TOSHI ENGINE**
>
> We do not own the Toshi Engine in any way, shape or form, and this project would not have been possible had they not developed TOSHI in the first place. The Toshi Engine is currently owned by [THQ Nordic](https://www.thqnordic.com); we highly urge you to follow & support their projects if you like this project!

> [!WARNING]
> **DO NOT USE THIS DECOMPILATION PROJECT AS A MEANS TO PIRATE BARNYARD OR ANY OTHER TOSHI GAMES!**
>
> We do not condone using this project as a means for piracy in any form. This project was made with love and care for the source material and was created for purely educational purposes.

## Installation

1. Install a legally obtained Windows copy of *Barnyard* and have an OpenXR-compatible PC VR headset and runtime set up.
2. Download the Windows ZIP from [GitHub Releases](https://github.com/Break-Ben/BarnyardVR/releases). Select a pre-release if that is the version you want to use.
3. Copy your game files to a separate folder first to avoid conflicts with other mods or changes to your original installation. Extract the ZIP contents into the copy, alongside its existing `Data` folder. Merge folders if prompted; keep the original game files.
4. Start your VR runtime, connect your headset and controllers, and launch `BarnyardVR.exe` from the copied game folder.

The release does not include the original game's assets. Physical VR controllers, a gamepad, or keyboard and mouse can be used; hand tracking is not supported.

## Settings

Open **Options → VR Settings**. Defaults are shown in bold; resolution and MSAA changes require a restart.

| Setting | Values | Description |
| --- | --- | --- |
| Control Layout | **Modern**, Classic | Modern remaps controls for VR controllers and modern conventions; Classic uses the original gamepad layout. |
| Snap Turning | **Disabled**, 15°, 30°, 45°, 60°, 90° | Turn smoothly when disabled, or turn by the selected angle with each stick deflection. |
| Movement | **Fixed**, Head Relative | Move relative to the game camera or your headset direction while on foot. |
| UI Position | **Fixed**, Head Relative | Keep the interface at the session or recenter forward direction, or let it follow your view where supported. |
| Vibration | **On**, Off | Enable or disable vibration on VR controllers and gamepads. |
| Resolution | 50–200% in 10% steps; **100%** | Change VR render resolution relative to the runtime recommendation; higher values can improve clarity but reduce performance. |
| MSAA | Off, **2x**, 4x, 8x | Smooth jagged edges; higher levels can reduce performance. Unsupported levels fall back to a lower level. |
| Debug HUD | **Off**, On | Show VR loop FPS, resolution, and MSAA to help tune performance. |

## Content Disclosure

- The original *Barnyard* game and its assets were not created using AI. BarnyardVR uses files from your own legally obtained copy of the game.
- Thank you to [InfiniteC0re](https://github.com/InfiniteC0re), the author of [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard), from which this port is forked and on which it is heavily based, for extensive manual reverse engineering and AI-assisted decompilation work.
- AI was used heavily to write and adapt code for the BarnyardVR port.

## Building

### Windows

Building on Windows requires:

- Visual Studio 2026 with the Desktop development with C++ workload and Windows SDK

Clone the repository with the following commands:

	git clone https://github.com/Break-Ben/BarnyardVR.git
	cd BarnyardVR
	git submodule update --init --recursive
	./GenerateProjects-WIN-DX11-VS2026.bat

Then open the generated solution file with Visual Studio and build it. The Windows game executable is named `BarnyardVR.exe`.

## Contributing

The project is open to any kind of contributions! Report issues on [GitHub](https://github.com/Break-Ben/BarnyardVR/issues) or in the [Discord community](https://discord.gg/j7vbsNP4my).
