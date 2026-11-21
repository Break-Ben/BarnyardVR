# BarnyardVR

[OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard) | [API Docs](https://opentoshi.net/OpenBarnyard/docs/) | [OpenJPOG](https://github.com/AdventureT/OpenJPOG) | [Modifications](https://www.youtube.com/@infc0re) | [Discord](https://discord.gg/j7vbsNP4my)

BarnyardVR is a community port of the video game Barnyard into VR with OpenXR, based heavily on [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard).

## VR input

Use physical OpenXR controllers, a regular gamepad, or keyboard/mouse. Hand tracking is not supported. BarnyardVR rejects detected pinch-emulated controller input so hand gestures do not trigger gameplay actions or switch button prompts. This does not change the headset/runtime's global hand-tracking settings.

In VR, look with the headset to aim milk squirts in the Barnyard and minigames. Both hip-fire and aimed squirting use headset forward, with the existing firing buttons unchanged. Mouse/right-stick aim controls turn the player and view in place. Lock-on takes priority over headset direction: the character faces the target and squirts use the existing target-directed aim until unlocked. The indicator marks the aiming point rather than the stream’s eventual landing point; gravity and inherited player momentum still affect the stream. Non-VR aiming is unchanged.

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
