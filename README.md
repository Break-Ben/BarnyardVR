# BarnyardVR

[OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard) | [API Docs](https://opentoshi.net/OpenBarnyard/docs/) | [OpenJPOG](https://github.com/AdventureT/OpenJPOG) | [Modifications](https://www.youtube.com/@infc0re) | [Discord](https://discord.gg/j7vbsNP4my)

BarnyardVR is a community port of the video game Barnyard into VR with OpenXR, based heavily on [OpenBarnyard](https://github.com/InfiniteC0re/OpenBarnyard).

## VR input

Use physical OpenXR controllers, a regular gamepad, or keyboard/mouse. Hand tracking is not supported. BarnyardVR rejects detected pinch-emulated controller input so hand gestures do not trigger gameplay actions or switch button prompts. This does not change the headset/runtime's global hand-tracking settings.

In VR, look with the headset to aim milk squirts in the Barnyard and minigames. Both hip-fire and aimed squirting use headset forward, with the existing firing buttons unchanged. Mouse/right-stick aim controls turn the player and view in place. Lock-on takes priority over headset direction: the character faces the target and squirts use the existing target-directed aim until unlocked. The indicator marks the aiming point rather than the stream’s eventual landing point; gravity and inherited player momentum still affect the stream. Non-VR aiming is unchanged.

## VR settings

Open **Options → VR Settings** using the normal menu controls. This native menu is also available in flat-screen mode so settings can be prepared without a headset.

- **Control Layout:** Modern (default) or Classic. Left/right switches layouts for gamepads and OpenXR controllers in gameplay and minigames. Keyboard/mouse bindings are unchanged. Classic keeps the original controller bindings.
- **Snap Turning:** Disabled (default), 15°, 30°, 45°, 60° or 90°. Left/right cycles the choices. Changes take effect in VR and are saved when confirmed.
- **Movement:** Head Relative (default) or Fixed. Head Relative uses the headset's horizontal heading for ordinary on-foot movement in VR, including strafing; it does not change vehicle controls or specialized minigame input.
- **UI Position:** Head Relative (default) or Fixed. Head Relative gently follows horizontal turns with a small dead zone during supported freely turning first-person minigames (currently Chasing Chicks). The panel remains fixed in other situations, including menus and fixed-view minigames. This can be selected independently of movement direction.
- **Restore Defaults:** asks for confirmation, with No selected by default.
- **Confirm / Cancel:** use the same device-aware prompts as the other options pages (Enter confirms, Escape cancels on keyboard). Confirm on a setting saves and returns; Cancel discards all edits, including Restore Defaults, and restores the values from when the page opened.

When enabled, joystick camera yaw turns by the selected angle once per deflection. Return the stick towards centre before another snap (including reversing direction). The activation threshold is 70% and the release threshold is 30%, preventing jitter and held-stick repeats. Applies to normal camera turning, aimed squirting, and vehicle freelook; pitch stays unchanged. Mouse look, keyboard look keys, physical headset movement and non-VR camera controls are not snapped. This does not change vehicle steering or movement-stick mappings.

Settings load on first access from `BarnyardVR.cfg` in the save directory chosen by `SaveLoadSKU` (normally `Save` next to the executable). This is separate from `OpenBarnyard.cfg` and `savedata.by`. Missing entries use defaults; unknown keys (including the old dummy keys) and malformed/unsupported values are ignored. Changes are written only when confirming the page, using a temporary file before replacing the previous config. Save errors are reported in-game and retain the edited values for retry.

```ini
# 0 disables snapping; supported angles: 15, 30, 45, 60, 90
SnapTurnDegrees=0
# 1 = Modern (default), 0 = Classic
ModernControls=1
# 1 = head-relative walking (default), 0 = fixed
HeadRelativeMovement=1
# 1 = head-relative UI in supported minigames (default), 0 = fixed
UIFollowsHead=1
```

### Controller layouts

Modern resolves each game's native controller map rather than changing the original controls asset. Input dispatch and button-name/glyph lookup use the same effective map. Sticks and directional navigation retain their native behavior. Modern UI uses Quest A to accept and B to go back/cancel. Gameplay action preferences are:

| Action | Modern preference (Quest button names) |
| --- | --- |
| Jump | A (bottom face button) |
| Bike accelerate / brake | Right trigger / left trigger |
| Car-chase accelerate / brake / handbrake | Right trigger / left trigger / A |
| Squirt aim (hold) / shoot | Left trigger / right trigger |
| Target lock | Left grip |
| Primary interact | B |
| Use, reload | X |
| Dodge, leave | B when not occupied by Interact; otherwise displaced |
| Alternate fire | Right grip |
| Sprint / boost | Left stick click |
| Inventory | Y |
| Bell / horn | Y when not occupied by Inventory; otherwise displaced |
| Bike boost (`Trick1`) | Left stick click |
| Bike left / right airborne trick (`Trick3` / `Trick4`) | Left grip / right grip |
| Previous / next selection | Left grip / right grip |

These are **priorities, not additive binds**: when a minigame uses the same source button for multiple state-dependent actions, the group stays together. Higher-priority actions (jump, pedals, shooting, interaction) win; displaced actions move with their original button group, so no original action is removed and no new button-sharing is introduced. Follow the game's effective button prompts for the particular activity. Generic face-button/QTE and direction-only maps retain their native layout. UI maps separately prefer A for OK/Yes and B for Cancel/Back/No. The resolver covers normal play, bike/races, car chase, golf, pool, darts, cooking, tapper, cow tipping, gate crash, honey collecting, gopher shop, statues, whack-a-raccoon, and the herding/defender/squirt/microgame contexts through their shared maps. Triggers remain digital; this does not add analog throttle.

The bike's native command names are misleading: `Trick1` is sprint/boost, `Trick3` and `Trick4` control the left/right airborne tricks, and holding both selects the combined trick variant. `Trick2` has no bike handler; Modern adds no preference or handler for it. Existing gameplay handlers, Classic bindings and keyboard/mouse controls are unchanged.

**Activity-specific Modern controls:** inventory uses **A to select/use** and **Y or B to close**, with its own effective map so on-foot B interaction is unaffected. Golf uses **A to swing**, **left trigger to zoom out**, and **right trigger to zoom in**. Darts uses **A to throw**; pool, cooking, tapper, whack-a-raccoon, gopher shop and honey collecting also prefer A for their primary action. These activity preferences take priority over the generic combat layout above.

**Loading/instruction screen (both layouts):** A or B continues once the activity is ready; Start/Pause and the original keyboard/mouse controls still work. The prompt says “Press A or B” only while controller input is active; keyboard/mouse use the original localized prompt, including in VR. This is separate from the front-end title screen.

**Title screen (both layouts):** press Quest A or B to enter the main menu. While controller input is active, the title prompt shows “Press A or B”; keyboard/mouse retain the original localized prompt and input behavior, including in VR.

**OpenXR pause (both layouts):** the application menu button acts as Start/Pause. If the controller has no application-accessible menu (for example Index), click both thumbsticks together. System/dashboard buttons are not used. Standalone OpenXR stick clicks have a short 0.2-second chord-detection delay; quick clicks are still delivered. Existing virtual-gamepad prompt artwork is reused, not replaced with headset-specific artwork. Gamepads retain their normal Start/Pause button.

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
