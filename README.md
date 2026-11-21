# BarnyardVR

[OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard) | [API Docs](https://opentoshi.net/OpenBarnyard/docs/) | [OpenJPOG](https://github.com/AdventureT/OpenJPOG) | [Modifications](https://www.youtube.com/@infc0re) | [Discord](https://discord.gg/j7vbsNP4my)

BarnyardVR is a community port of the video game Barnyard into VR with OpenXR, based heavily on [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard).

## VR input

Use physical OpenXR controllers, a regular gamepad, or keyboard/mouse. Hand tracking is not supported. BarnyardVR rejects detected pinch-emulated controller input so hand gestures do not trigger gameplay actions or switch button prompts. This does not change the headset/runtime's global hand-tracking settings.

In VR, look with the headset to aim milk squirts in the Barnyard and minigames. Both hip-fire and aimed squirting use headset forward, with the existing firing buttons unchanged. Mouse/right-stick aim controls turn the player and view in place. Lock-on takes priority over headset direction: the character faces the target and squirts use the existing target-directed aim until unlocked. The indicator marks the aiming point rather than the stream’s eventual landing point; gravity and inherited player momentum still affect the stream. Non-VR aiming is unchanged.

## VR settings

Open **Options → VR Settings** using the normal menu controls. This native menu is also available in flat-screen mode so settings can be prepared without a headset.

The initial page contains **dummy options only**, with no gameplay or rendering effect:

- **Dummy Toggle:** Off by default; left/right or confirm toggles it.
- **Dummy Level:** 1–5, default 3; left/right adjusts it, confirm cycles it.
- **Restore VR Defaults:** asks for confirmation (No is the first choice).
- **Save and Back:** writes changes and returns. The normal Back/Cancel control also saves.

Settings load on first access from `BarnyardVR.cfg` in the save directory chosen by `SaveLoadSKU` (normally `Save` next to the executable). This is separate from `OpenBarnyard.cfg` and `savedata.by`. Missing entries use defaults; unknown keys and malformed/out-of-range values are ignored. Changes are written only on leaving the page, using a temporary file before replacing the previous config. Save errors are reported in-game and retain the edited values for retry.

```ini
DummyEnabled=0
DummyLevel=3
```

For future port options, add typed values and validated setters to `VR/AVRSettings`, have the relevant subsystem consume those values, and expose them through `AVROptionsState`. Do not store port settings in the original binary save layout. The dummy options have no command-line overrides; existing VR arguments (including `-vrscale`) are unchanged. When connecting real startup-sensitive settings, load them before their subsystem initializes, label restart requirements in the menu, and apply any command-line overrides after loading without writing those overrides back to the file.

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
