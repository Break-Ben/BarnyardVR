"""Build the bundled OpenXR prompt atlas. Requires Pillow 11.3.0 (pip install Pillow==11.3.0).

Source assets/licence: Assets/VRPrompts/PROVENANCE.md. Run from any directory.
The runtime DDS and preview PNG are deterministic; no network or game data needed.
Cell ordering matches AVRControlIcon / AVRControllerFamily in AVRPromptTypes.h.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Assets/VRPrompts/Source"
OUTPUT = ROOT / "Content/Common/Data/VR"
FAMILIES = ["Generic", "Touch", "Index", "Vive", "WMR"]
ICONS = ["LeftA", "LeftB", "LeftX", "LeftY", "RightA", "RightB", "RightX", "RightY",
         "LeftTrigger", "RightTrigger", "LeftGrip", "RightGrip", "LeftStick", "RightStick",
         "LeftStickClick", "RightStickClick", "Menu", "PauseChord", "LeftPad", "RightPad",
         "LeftPadClick", "RightPadClick", "DPad", "Up", "Right", "Down", "Left", "Unavailable"]
CELL = 64
FONT = ImageFont.load_default(size=13)
SMALL = ImageFont.load_default(size=10)

def label(draw, xy, text, font=FONT):
    draw.text(xy, text, anchor="mm", font=font, fill="white", stroke_width=0)

def cell(family, icon):
    image = Image.new("RGBA", (CELL, CELL))
    d = ImageDraw.Draw(image)
    hand = "L" if icon.startswith("Left") and icon != "Left" else "R" if icon.startswith("Right") and icon != "Right" else ""
    kind = icon.removeprefix("Left").removeprefix("Right") if hand else icon
    filename = None
    if family == "Touch":
        names = {"A": "A", "B": "B", "X": "X", "Y": "Y", "Trigger": "LT" if hand == "L" else "RT",
                 "Grip": "Left_Grab" if hand == "L" else "Right_Grab", "Stick": "Left_Stick" if hand == "L" else "Right_Stick"}
        if kind in names:
            filename = "Oculus_" + names[kind] + ".png"
    elif family == "Vive":
        names = {"Trigger": "LT" if hand == "L" else "RT", "Grip": "Girp_Left" if hand == "L" else "Grip_Right",
                 "Menu": "Menu", "Pad": "Touch"}
        if kind in names:
            filename = "Vive_" + names[kind] + ".png"
    if filename:
        original = Image.open(SOURCE / filename).convert("RGBA")
        # Keep the source silhouette, without nesting it in two extra keycaps.
        original = original.crop(original.getbbox())
        original.thumbnail((60, 52), Image.Resampling.LANCZOS)
        image.alpha_composite(original, ((CELL-original.width)//2, (52-original.height)//2))
        label(d, (32, 57), hand if hand else "MENU", SMALL)
    else:
        d.rounded_rectangle((3, 3, 60, 60), radius=12, fill=(26, 30, 36, 255), outline="white", width=2)
        words = {"Trigger": "TRIG", "Grip": "GRIP", "Stick": "STICK", "StickClick": "CLICK",
                 "Pad": "PAD", "PadClick": "PRESS", "Menu": "MENU", "DPad": "D-PAD",
                 "Up": "UP", "Right": "RIGHT", "Down": "DOWN", "Left": "LEFT", "Unavailable": "N/A"}
        if icon == "PauseChord":
            label(d, (32, 22), "L + R")
            label(d, (32, 40), "CLICK", SMALL)
        else:
            label(d, (32, 26), words.get(kind, kind))
            if hand:
                label(d, (32, 46), hand, SMALL)
    return image

if __name__ == "__main__":
    atlas = Image.new("RGBA", (CELL * len(ICONS), CELL * len(FAMILIES)))
    for row, family in enumerate(FAMILIES):
        for column, icon in enumerate(ICONS):
            atlas.alpha_composite(cell(family, icon), (column*CELL, row*CELL))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    atlas.save(OUTPUT / "ControllerPrompts.dds")  # Uncompressed RGBA DDS; no platform tool required.
    atlas.save(ROOT / "Assets/VRPrompts/ControllerPrompts.png")
