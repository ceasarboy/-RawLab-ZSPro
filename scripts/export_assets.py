# -*- coding: utf-8 -*-
"""Export the final RawLab lens icon to every platform asset."""
import os
import struct
import shutil
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(HERE, "..")

IOS_DIR = os.path.join(REPO, "RawLab", "RawLab", "Resources", "Assets.xcassets", "AppIcon.appiconset")
MAC_ICON = os.path.join(REPO, "RawLabMac", "Resources", "AppIcon.png")
AND_RES = os.path.join(REPO, "RawLabAndroid", "app", "src", "main", "res")
WIN_DIR = os.path.join(REPO, "RawLabWindows")

IOS_SIZES = {
    "AppIcon-1024x1024@1x.png": 1024,
    "AppIcon-20x20@1x.png": 20,
    "AppIcon-20x20@2x.png": 40,
    "AppIcon-20x20@3x.png": 60,
    "AppIcon-29x29@1x.png": 29,
    "AppIcon-29x29@2x.png": 58,
    "AppIcon-29x29@3x.png": 87,
    "AppIcon-40x40@1x.png": 40,
    "AppIcon-40x40@2x.png": 80,
    "AppIcon-40x40@3x.png": 120,
    "AppIcon-60x60@2x.png": 120,
    "AppIcon-60x60@3x.png": 180,
    "AppIcon-76x76@1x.png": 76,
    "AppIcon-76x76@2x.png": 152,
    "AppIcon-83.5x83.5@2x.png": 167,
}

master = Image.open(os.path.join(HERE, "icon-build", "master-1024.png")).convert("RGB")

# 1. iOS catalog (opaque, no alpha)
for name, size in IOS_SIZES.items():
    im = master if size == 1024 else master.resize((size, size), Image.LANCZOS)
    im.save(os.path.join(IOS_DIR, name))
    print("ios", name, im.size)

# 2. macOS tile (RGBA, transparent outside the rounded tile)
mac = Image.open(os.path.join(HERE, "icon-build", "mac-1024.png"))
mac.save(MAC_ICON)
print("mac", mac.size, mac.mode)

# 3. Android adaptive foregrounds (108dp canvas, lens inside the 66dp safe zone)
fg = Image.open(os.path.join(HERE, "icon-build", "fg-108.png"))
for dpi, size in (("mdpi", 108), ("hdpi", 162), ("xhdpi", 216), ("xxhdpi", 324), ("xxxhdpi", 432)):
    d = os.path.join(AND_RES, f"drawable-{dpi}")
    os.makedirs(d, exist_ok=True)
    fg.resize((size, size), Image.LANCZOS).save(os.path.join(d, "ic_launcher_foreground.png"))
    print("android", dpi, size)

vec = os.path.join(AND_RES, "drawable", "ic_launcher_foreground.xml")
if os.path.exists(vec):
    os.remove(vec)
    print("android removed", vec)

# 4. Windows .ico (PNG-compressed entries, smallest first)
ico_sizes = [16, 24, 32, 48, 64, 128, 256]
frames = [master.resize((s, s), Image.LANCZOS) for s in ico_sizes]
import io
blobs = []
for im in frames:
    buf = io.BytesIO()
    im.save(buf, format="PNG")
    blobs.append(buf.getvalue())

count = len(blobs)
offset = 6 + 16 * count
ico = struct.pack("<HHH", 0, 1, count)
for s, blob in zip(ico_sizes, blobs):
    ico += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(blob), offset)
    offset += len(blob)
ico += b"".join(blobs)
with open(os.path.join(WIN_DIR, "AppIcon.ico"), "wb") as f:
    f.write(ico)
print("windows ico", len(ico), "bytes")

print("ALL DONE")
