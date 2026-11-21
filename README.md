# BarnyardVR

[OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard) | [API Docs](https://opentoshi.net/OpenBarnyard/docs/) | [OpenJPOG](https://github.com/AdventureT/OpenJPOG) | [Modifications](https://www.youtube.com/@infc0re) | [Discord](https://discord.gg/j7vbsNP4my)

BarnyardVR is a community port of the video game Barnyard into VR with OpenXR, based heavily on [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard).

## VR input

Use physical OpenXR controllers, a regular gamepad, or keyboard/mouse. Hand tracking is not supported. BarnyardVR rejects detected pinch-emulated controller input so hand gestures do not trigger gameplay actions or switch button prompts. This does not change the headset/runtime's global hand-tracking settings.

In VR, look with the headset to aim milk squirts in the Barnyard and minigames. Both hip-fire and aimed squirting use headset forward, with the existing firing buttons unchanged. Mouse/right-stick aim controls turn the player and view in place. Lock-on takes priority over headset direction: the character faces the target and squirts use the existing target-directed aim until unlocked. The indicator marks the aiming point rather than the stream’s eventual landing point; gravity and inherited player momentum still affect the stream. Non-VR aiming is unchanged.

## VR settings

Open **Options → VR Settings** using the normal menu controls. This native menu is also available in flat-screen mode so settings can be prepared without a headset.

- **Snap Turning:** Disabled (default), 15°, 30°, 45°, 60° or 90°. Left/right cycles the choices. Changes take effect in VR and are saved when confirmed.
- **Restore Defaults:** asks for confirmation (No is the first choice).
- **Confirm / Cancel:** use the same device-aware prompts as the other options pages (Enter confirms, Escape cancels on keyboard). Confirm on the setting saves and returns; Cancel discards all edits, including Restore Defaults, and restores the value from when the page opened.

When enabled, joystick camera yaw turns by the selected angle once per deflection. Return the stick towards centre before another snap (including reversing direction). The activation threshold is 70% and the release threshold is 30%, preventing jitter and held-stick repeats. Applies to normal camera turning, aimed squirting, and vehicle freelook; pitch stays unchanged. Mouse look, keyboard look keys, physical headset movement and non-VR camera controls are not snapped. This does not change vehicle steering or movement-stick mappings.

Settings load on first access from `BarnyardVR.cfg` in the save directory chosen by `SaveLoadSKU` (normally `Save` next to the executable). This is separate from `OpenBarnyard.cfg` and `savedata.by`. Missing entries use defaults; unknown keys (including the old dummy keys) and malformed/unsupported values are ignored. Changes are written only when confirming the page, using a temporary file before replacing the previous config. Save errors are reported in-game and retain the edited values for retry.

```ini
# 0 disables snapping; supported angles: 15, 30, 45, 60, 90
SnapTurnDegrees=0
```

For future port options, add typed values and validated setters to `VR/AVRSettings`, have the relevant subsystem consume those values, and expose them through `AVROptionsState`. Do not store port settings in the original binary save layout. Snap turning has no command-line override; existing VR arguments (including `-vrscale`) are unchanged. When connecting startup-sensitive settings, load them before their subsystem initializes, label restart requirements in the menu, and apply any command-line overrides after loading without writing those overrides back to the file.

## Notes

> [!IMPORTANT]
> **SUPPORT THE DEVELOPERS OF THE TOSHI ENGINE**
>
> We do not own the Toshi Engine in any way, shape or form, and this project would not have been possible had they not developed TOSHI in the first place. The Toshi Engine is currently owned by [THQ Nordic](https://www.thqnordic.com); we highly urge you to follow & support their projects if you like this project!

> [!WARNING]
> **DO NOT USE THIS DECOMPILATION PROJECT AS A MEANS TO PIRATE BARNYARD OR ANY OTHER TOSHI GAMES!**
>
> We do not condone using this project as a means for piracy in any form. This project was made with love and care for the source material and was created for purely educational purposes.

## Building

### Windows

Building on Windows requires:

- Visual Studio 2026

Clone the repository with the following commands:

	git clone https://github.com/Break-Ben/BarnyardVR.git
	cd BarnyardVR
	git submodule update --init --recursive
	./GenerateProjects-WIN-DX11-VS2026.bat

Then open the generated solution file with Visual Studio and build it. The Windows game executable is named `BarnyardVR.exe`.

## Contributing

The project is open to any kind of contributions!

## Third-Party libraries
- [premake](https://github.com/premake/premake-core)
- [tracy](https://github.com/wolfpld/tracy)
- [fmod](https://fmod.com/)
- [Opcode](http://www.codercorner.com/Opcode.htm)
