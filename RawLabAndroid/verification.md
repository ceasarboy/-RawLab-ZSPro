# Android Verification

This records the initial CPU-only client. Current hardware acceleration and
portrait-first UI evidence is in [gpu-verification.md](gpu-verification.md).

Date: 2026-09-28. Local development evidence, not a store release or a claim
of compatibility with every Android device.

## Build and Host Checks

- macOS host, OpenJDK 21, Gradle wrapper 8.11.1, AGP 8.9.2, Kotlin 2.1.20.
- SDK 35, NDK 27.2.12479018, CMake 3.22.1.
- `:app:assembleDebug` and `:app:assembleDebugAndroidTest`: passed.
- Both `arm64-v8a` and `x86_64` compile from LibRaw source; no host binary links.
- `:app:testDebugUnitTest`: 8 tests passed. Covers settings/reset/ranges,
  API-specific permission requests/full-partial-denied classification, RAW
  filtering, latest-pending rendering, close ordering, and working-file copies.
- Host `tests/request_test.cpp`: passed. Covers native identity defaults,
  camera/temperature WB, PREVIEW vs FINAL/NATIVE, invalid values, native OOM
  classification, and truncated/padded RGBA buffers.
- `:app:lintDebug`: 0 errors, 10 warnings. Seven dependency-update notices,
  two optional KTX suggestions, and one legacy backup-configuration suggestion.
  Backup is disabled; current-platform extraction rules exclude app data.
- `scripts/check-native.sh`: APK zip alignment and ELF LOAD alignment passed
  for all eight packaged libraries across both ABIs, including libc++ and the
  transitive AndroidX graphics library. This is not a 16 KB device runtime test.
- Shared C++ suite: 6/6 passed after the CMake integration change. Optional
  local DJI/extra Sony fixtures are absent in this worktree; those additional
  tests were not run. The repository Sony fixture is present.

## Device Checks

Android 15/API 35 Google APIs ARM64 emulator, AVD `rawlab-api35-arm64`, device
`emulator-5554`. The image reports about 2.4 GiB RAM plus swap.

Final `:app:connectedDebugAndroidTest`: 6/6 passed, 0 skipped, on the tree with
the JNI status/buffer validation fixes and landscape layout adjustment.

Instrumentation covers:

- Empty editor import actions and disabled export.
- Sony RAW import through the ViewModel, a changed film/exposure, Activity
  recreation with retained edits, and a failed import preserving the previous
  usable photo.
- Native Sony 7008x4672 RAW: bounded RGBA preview, differing film/exposure/WB
  output, full-resolution 16-bit PNG and JPEG, missing-file failure, and
  idempotent close/rejected use after close.
- MediaStore publication after a complete copy, removal of a failed pending
  row, and rejection of an output URI equal to the input URI.

Manual Android 15 checks exercised the real system permission dialog:
selected Limited Access, selected the Sony RAW, confirmed the app displayed
the partial-access label and that RAW, and reopened/cancelled the
reselection permission dialog. System document import from Downloads also
opened the RAW without broad album access.
The editor's JPEG -> Save to Album action then completed end to end with
Velvia selected. MediaStore reported the resulting JPEG as 7008x4672 with
`is_pending=0`; the source RAW remained a separate unchanged media item.

Visual inspection used actual emulator screenshots in portrait/light and
landscape/dark with font scale 1.3. The first landscape inspection found that
the bottom controls left too little image height; controls now sit beside
the canvas in wide landscape. A confirmation screenshot shows both images
and controls without overlap. Font scale/theme/rotation were restored after
checking. Evidence includes `album-partial-access.png` and
`landscape-dark-large-text.png` under `app/build/verification/`.

One preview/edit sample from `dumpsys meminfo` reported approximately 742 MiB
total PSS (not a peak measurement). Native memory dominates. No low-memory
device performance or failure-recovery claim is made from this sample.

## Evidence Locations

Generated evidence is intentionally not committed:

- `app/build/reports/tests/testDebugUnitTest/`
- `app/build/reports/androidTests/connected/debug/`
- `app/build/reports/lint-results-debug.html`
- `app/build/verification/` for emulator screenshots.
- `app/build/outputs/apk/debug/app-debug.apk` for the installable debug build.

## Remaining Coverage

No physical-device run, API 26-34 runtime matrix, x86_64 runtime run, 16 KB
page-size device run, process-death export recovery, or all-camera format
matrix. Album permission classification for older API levels has JVM coverage,
not runtime coverage on those versions. GPU acceleration is intentionally off.

## 2026-10-03 Generic LUT Mode + Four Adjustments (build-level verification)

Implements the layered pipeline contract in `lutools/docs/color-contract.md`
(§Pipeline Layering): session-level `sony2fuji_session_set_lut_mode` (PHOTO /
DISPLAY / FLOG2_INPUT), `#Gamma`/`#Gamut` contract surfacing in `LUT3D`
(`rawGamma`/`rawGamut`/`inputIsFLog2`), a display-input branch in all four
render paths (CPU, GLES, Metal, D3D11), and user LUT import/library UI
(SAF import with encoding sniffing, per-LUT encoding switch, delete).
Also exposes contrast/saturation/tone curve/sharpening end to end.

Verified at build level on this host:

- Host contract test `RawLabAndroid/tests/request_test.cpp` (MinGW g++ 16.2,
  -std=c++17 -Wall -Wextra): passes, including new field propagation and
  range rejections (contrast > 2, sharpening > 2, tone curve > 1).
- `:app:assembleDebug` compiles arm64-v8a and x86_64 native (NDK 27.2) and
  Kotlin: BUILD SUCCESSFUL. GLES path compiled but not executed on a device.
- `:app:testDebugUnitTest`: passes.

Not yet verified (requires a device/emulator run):

- GLES rendering of DISPLAY-mode LUTs (shader branch `lutOnDisplay`) —
  CPU/GPU parity deltas are unmeasured.
- SAF import flow, encoding auto-sniffing on real files, and the editing UI.
- Metal and D3D11 branches are Mac/Windows-only and compiled as source review
  only; the D3D11 `control.z == 2` branch mirrors the GLES uniform.

Known follow-ups: crop (P1-2, needs a request-level or session-level crop
with preview/export coordinate mapping), real-device GPU verification, and
`fit-lut` production tooling (P2).

## 2026-10-03 (evening) Crop (P1-2)

Session-level normalized crop (`sony2fuji_session_set_crop`), applied after
tone/detail and before sizing on every render. CPU path crops the pixel
buffer; GLES/D3D11 map a crop window onto the source through their sampling
(offset + stride + window-size uniforms/constants), so previews and exports
render at cropped resolution instead of cropping after the fact. Metal
renders non-full crops on CPU for now (window mapping pending, guard in the
wrapper). Android UI: crop tool with a draggable window overlay (corner
handles + move), apply/reset; reset lives in the crop tool and global reset.

Host verification (`host-harness/lut_mode_harness.cpp`, 27/27 PASS):

- Half-crop output matches the left half of a full render pixel-exactly
  (delta 0.0) on both CPU and D3D11.
- Full-crop regression: identity still equals neutral on D3D11 (a cbuffer
  field-order bug introduced and caught during this change; fixed).
- CPU vs D3D11 parity delta 1 (8-bit rounding) unchanged.

Android build green (`assembleDebug` + `testDebugUnitTest`). Pending device
verification: crop gesture flow, cropped export resolution/aspect, GPU/CPU
consistency with an active crop.

## 2026-10-03 (late) Crop reverted; zoom added; look stacking (second LUT slot)

Crop (P1-2) was reverted at the user's request after device-side rendering
abnormalities: the native window plumbing, GLES/D3D11 mappings, C API entry,
JNI, UI and harness cases were all removed; the D3D11 208-byte constant
layout is restored. Replacements and additions:

- Preview inspect: pinch-zoom (1-8x, centroid-anchored), drag to pan,
  double-tap reset on the single-image canvas. Compare-wipe keeps its
  original single-finger drag (zoom applies when compare is off).
- Look stacking: `sony2fuji_session_set_lut_b(path, strength)` - a second
  display-encoding look slot applied sequentially in the same float buffer
  (mix against the primary blend result), before tone adjustments. Log-input
  contracts are rejected in this slot (UNSUPPORTED). CPU: anchor-copy blend.
  GLES: second sampler3D (binding 1) + texelFetch twin of lookup, chained in
  the shader. D3D11: t2 buffer + lutBInfo constant (224-byte layout).
  Metal: stacked renders fall back to CPU pending kernel work.
- Android UI: long-press a user LUT -> "用作叠加 LUT"; strength page gains a
  叠加强度 slider with 移除叠加; session slot set per render like lut_mode.

Host harness (`lut_mode_harness.cpp`) now 30/30 PASS including four stacking
cases: B with strength 1 changes the render; A+B differs from B-over-neutral
(sequential, A contributes); F-Log2 contract LUT rejected in the slot;
strength 0 clears. Android build green. Pending device verification: zoom
gestures, stacking workflow, GPU/CPU parity with an active stack slot.

## 2026-10-03 (night) LUT composer (user-directed stacking redesign)

User rejected the editor-inline stacking controls in favor of a dedicated
screen with live preview and persistence. Delivered:

- Editor stacking UI removed; editor keeps single-LUT semantics.
- Full-screen LUT composer (胶片页 -> 组合 LUT): slot A + strength, slot B +
  strength, live photo preview (renders through the same queue/session at
  800px), name field, save.
- Bake: `sony2fuji_bake_look_lut(A, sA, B, sB, out, size)` composes the chain
  per lattice point in display encoding and writes a standalone .cube; both
  inputs must be display-encoding looks (log contracts rejected). Baked at
  65-grid (matches the built-in library tier; 33-grid rebake measured delta 9
  on strong-curve pairs, 65-grid brings the end-to-end delta to 5/255).
- Saved LUTs register into UserLutStore as display-encoding entries and are
  auto-selected after save.

Host harness 30/30 PASS including three bake cases (A-only identity to
source, log-contract rejection, baked-vs-dual-slot end-to-end delta 5).
Android build green. Pending device verification: composer workflow, saved
LUT reuse, and the user's Panasonic STD pack (root-folder LUTs carry
`#LUMIXPHOTOSTYLE STD` = display encoding, usable in DISPLAY mode; Vlog-based
files are not usable until a V-Log input transform exists).

## 2026-10-03 (late II) LUT library completion: batch import, thumbnails, STD base adaptation

User redirected the library effort: not the Panasonic-STD pack as-is, but
LUTs based on a neutral curve - or alternatively, the official Panasonic STD
base parameters. Found the latter inside the Lumix Lab APK the user provided
(F:/rawlab/lumix-lab-re, originals saved to F:/rawlab/panasonic-official):

- Official STD->V-Log mapping tables for all 12 photo styles
  (models/mergeLut/STD_to_VLOG_2nd.cube et al; black 0 -> 0.125 matches the
  V-Log spec, self-validating).
- Official STD tone curve (LUTFile/gamma_corrected_33_tone.tone; midpoint
  0.73 ~= sRGB, strong shadow lift) and five official STD/CNEV2-based looks.

Implemented:

- V-Log encode/decode + neutralDisplayDecode in C++ (lutools vlog.h /
  lut_adapt.cpp), ported from the colour-science-verified anchors.
- `sony2fuji_adapt_std_lut(in, map, out, size)`: Gauss-Newton numerical
  inversion of the official STD->V-Log map with raster-coherent seeding,
  then resampling of the STD-domain look through the inverse. Output is a
  display-encoding look .cube.
- Import: `#LUMIXPHOTOSTYLE STD` headers are auto-detected at import and
  adapted automatically (official map bundled as an app asset); batch import
  via OpenMultipleDocuments running on a background coroutine; per-LUT
  reference-scene thumbnails (192x128, hue sweep + skin/sky/foliage/gray)
  rendered natively at import and shown in the film picker.

Host harness additions pending (vlog anchors, adaptation sanity); Android
build + unit tests green. Known limitation: adaptation is one-way at import
(original replaced); Vlog-based files remain unsupported.

## 2026-10-03 (night II) Root cause of red screen fixed; host harness 33/33

The all-red render came from the STD adaptation's Newton solver: the code
ran `s += J^-1 * (target - M(s))` with err = target - M(s), i.e. **positive
feedback** (the correct Newton step is `s -= J^-1 * (M(s) - target)`), plus
a seed chain that propagated stuck corner points and a `break` on singular
Jacobians near domain boundaries. Fixed by restoring the forward Newton sign,
dropping the seed chain, and adding a brute-force nearest-lattice fallback
for the few boundary points where J goes singular.

The bug had escaped notice because the C++ adaptation was never host-verified.
Now it is: `adapt_check.cpp` + harness cases verify adapted identity through
the official map keeps black black, white bright (~0.87 triple on the STD
base) and the neutral axis monotone. Host harness total: 33/33 PASS
(includes the three earlier bake cases). Android build green.

Device verification pending: STD-based packs (胡子 RedNose/Leica/official)
should now render with visibly correct - not red - grading after choosing
the Panasonic STD base.

## Progress close-out (2026-10-03 end of day)

Device-verified by the user: LUT import (single/batch + base chooser), library
thumbnails, manual base switching now working end to end after the Newton-sign
fix. Composer stacking confirmed working earlier. Still device-pending: STD
base adaptation visual quality (fixed solver awaits re-check), zoom gestures.

Full lessons-learned write-up: docs/lessons-lut-feature.md.
Host harness: 33/33 PASS (lut modes, identity parity, backend parity, stack
slot, bake, STD adaptation endpoints/monotonicity).

## 2026-10-04 STD adaptation posterization fixed (host-verified with renders)

User reported color posterization/speckle on STD-adapted LUTs (greens), and
suspected gamut overflow - confirmed. Root cause chain: the neutral pipeline
clips saturated colors harder than the STD base (e.g. foliage R/B clip to 0),
so the adaptation target vlog(nd(v)) falls OUTSIDE the official map's
attainable output for those points; per-point Newton + nearest-lattice
fallback created a discontinuous cliff at the gamut boundary, and trilinear
interpolation across the cliff produced the colored speckle.

Fix (all host-verified with rendered evidence, F:/rawlab/scene-*.png):

1. Levenberg-Marquardt projection replaces hard Newton: for unattainable
   targets it converges continuously to the nearest attainable point.
2. The solved STD-input lattice is smoothed (2 passes, jump-triggered
   neighbour averaging) before the look is applied.
3. Confidence fade: points with solve residual > 0.03 fade the look toward
   neutral (fully neutral beyond residual 0.15), so gamut-overflow colors
   stay near their neutral rendering instead of clipping to corners.

Host renders (768x512 reference scene, neutral vs adapted Velvia vs raw
Velvia): adapted output is smooth - no banding, no speckle - with plausible
Velvia character (greens pushed toward yellow-green, saturation lift).
Import crash also fixed: heavy native work moved off the main thread
(withContext(Dispatchers.Default)).

## 2026-10-04 (morning) STD adaptation switched to layered tone chain

Pipeline experiment (host-harness/pipeline_experiment.cpp, 4 paths x noise
field) showed the inverse-3D adapted LUT had the highest chroma noise of all
paths (sigma 2.27-3.04/255 vs direct-apply 0.92-1.32 vs layered 0.77-1.22)
and high luma noise (11.42 at 0.3 gray). Root cause: per-point 3D inversion
of the official map creates lattice discontinuities (clamped corners differ
between neighbours) - irreducible by solver tuning.

The STD adaptation is now the LAYERED chain: neutral display v ->
neutralDecode -> scene linear -> STD display curve (1D, derived by inverting
the official STD_to_VLOG gray axis: M(g,g,g) = V-Log code of STD display g,
so vlogDecode(M_gray) = scene linear; (lin, g) pairs form the STD display
response) -> look LUT. The gray axis of the adapted LUT is now EXACTLY gray
(R=G=B verified on host), so the adaptation itself adds zero chroma noise on
neutral content; chroma response equals the look's own.

Host: harness 33/33 still green (adaptation endpoints/monotonicity cases now
run against the layered implementation and pass trivially - gray axis exact),
noise_check chroma sigma at/below the raw-apply baseline, scene render clean.
The old per-point 3D inverse remains in lut_adapt.cpp history but is replaced
by the layered path in the shipped code path.

## 2026-10-04 Exhaustive 256-level ramp verification (user-directed)

User challenged the verification coverage: sampled spot checks (9-point
diagonals, 4 gray levels) are what let the earlier bugs reach the device.
Added `host-harness/ramp_check.cpp`: EVERY level of R, G, B, gray ramps
(4 x 256 = 1024 points) through the real LUTApplicator, plus a full-lattice
6-neighbour jump census. Permanent harness target.

Results on adapted Velvia (layered adaptation, 65³):

| check | R ramp | G ramp | B ramp | gray ramp |
|---|---|---|---|---|
| range violations | 0 | 0 | 0 | 0 |
| own-channel monotonicity | 0 | 0 | 0 | 1 (Δ=6e-5, raw LUT's own 8-bit dark-end quantization) |
| smoothness (jump > 0.12) | 0 | 0 | 0 | 0 |
| max adjacent jump | 0.0088 | 0.0149 | 0.0115 | 0.0085 |
| endpoints | 0.005 -> (0.98, 0.14, 0.09) | -> (0.31, 0.85, 0.002) | -> (0.53, 0.27, 0.97) | -> (0.879, 0.876, 0.876) |

Baseline on the RAW (unadapted) Velvia for comparison: gray monotonicity
violations 6, gray-preservation deviation 0.0059 (adapted: 0.0042 - BETTER),
max lattice jump 0.2218 (adapted: 0.1481 - SMOOTHER than the source look).

Conclusions: the layered adaptation's ramps are smooth and gray-preserving;
the single sub-1e-4 gray dip is inherited 8-bit quantization from the source
LUT's dark end. The full 1024-row table: F:/rawlab/ramp-table.txt (adapted)
and ramp-table-raw.txt (raw baseline).

## Final close-out (2026-10-04)

Exhaustive ramp verification added (ramp_check.cpp, permanent harness target):
R/G/B/gray x 256 levels all pass on the layered adapted Velvia; the adapted
lattice is smoother (max neighbour jump 0.148) and more gray-preserving
(dev 0.0042) than the raw look itself (0.222 / 0.0059). Full 1024-row tables:
F:/rawlab/ramp-table.txt (adapted), ramp-table-raw.txt (raw baseline).
Research report 05 covers the Lumix Lab pipeline analysis and the experiment
that ruled out the inverse-3D approach. Lessons doc extended to 12 items.
