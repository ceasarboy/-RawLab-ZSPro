# iOS App Icon

Skeuomorphic camera-lens mark (knurled metal ring, engraved distance scale,
coated glass with amber/cyan flares, 9-blade iris) on the charcoal
gray-and-yellow brand background. The amber `#FFD13A` and cyan `#4CDAE9`
coating reflections carry the former dual-frame mark's brand colors; the
interlocked double-frame is preserved as a printed mark on the lens bezel.

The opaque 1024-pixel master is
`Assets.xcassets/AppIcon.appiconset/AppIcon-1024x1024@1x.png`.
All catalog sizes are downscaled from it with LANCZOS resampling.

Regenerate every platform asset (iOS catalog, macOS `RawLabMac/Resources/AppIcon.png`,
Android adaptive foregrounds, Windows `AppIcon.ico`) from the master:

```sh
python scripts/gen_icon.py       # renders master-1024.png / mac-1024.png / fg-108.png
python scripts/export_assets.py  # writes all platform assets
```

`RawLabMac/Resources/AppIcon.png` is shared by the macOS tile, the WPF window
icons and the Android `rawlabAssets` bundle, so a single regeneration covers
those clients. The Windows executable icon additionally comes from
`RawLabWindows/AppIcon.ico` via `<ApplicationIcon>` in the csproj.

Validate all catalog entries from the repository root:

```sh
swift RawLab/tests/AppIconTests.swift RawLab/RawLab/Resources/Assets.xcassets/AppIcon.appiconset
```

The test checks dimensions, the absence of an alpha channel, and the presence
of the amber and cyan coating reflections (thresholds scale with sample count
so the check holds down to the 20pt entry).
