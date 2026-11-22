#pragma once
#include <Toshi/Typedefs.h>

// Presentation metadata only: never used to change action bindings.
enum class AVRControllerFamily
{
	Generic,
	Touch,
	Index,
	Vive,
	WMR
};
enum class AVRControlIcon
{
	LeftA,
	LeftB,
	LeftX,
	LeftY,
	RightA,
	RightB,
	RightX,
	RightY,
	LeftTrigger,
	RightTrigger,
	LeftGrip,
	RightGrip,
	LeftStick,
	RightStick,
	LeftStickClick,
	RightStickClick,
	Menu,
	PauseChord,
	LeftPad,
	RightPad,
	LeftPadClick,
	RightPadClick,
	DPad,
	Up,
	Right,
	Down,
	Left,
	Unavailable,
	Count
};
struct AVRControllerPrompt
{
	AVRControllerFamily family = AVRControllerFamily::Generic;
	AVRControlIcon      icon   = AVRControlIcon::Unavailable;
};
