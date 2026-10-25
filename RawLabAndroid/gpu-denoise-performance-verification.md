# Android GPU Denoise Export Performance Verification

Date: 2026-10-25. Local physical-device measurements of the Android export path with
wavelet denoise enabled. This record extends
[CPU denoise performance](cpu-denoise-performance-verification.md) and the historical
[GPU baseline](gpu-verification.md); it is not a release-note claim for every Android GPU.

## Scope and Device

- OnePlus 11 (`PGEM10`), Android, arm64, Qualcomm Adreno 740.
- Driver: OpenGL ES 3.2 `V@0676.76.1` (2025-11-28 build). Queried limits: SSBO 536870912
  bytes, 65535 workgroups, 3D texture 2048.
- Fixture: `P2290048.ORF`, 5184 x 3888 (20.1 MP), Clean denoise `40/46/50`, default
  look, 1600 px preview followed by a full-resolution JPEG export, GPU mode Auto.
  The user report behind this round was "export is usable without denoise,
  noticeably slow with denoise".
- The shared core already routed the wavelet denoise stages through the GLES compute
  backend before this round; this round reduces GPU execution and CPU submission cost,
  with no quality-setting, algorithm or output-format change.
- Evidence logs: `instrument-denoise10..17.log`, `logcat-denoise4.log` and `build-app.log`
  in the local work root (outside Git). Device executables live in `/data/local/tmp`;
  timing JSON is written to the app-private `files/denoise-timing` directory.

## Measurements

Same fixture, same settings, warm app. Times are one sample per build revision, not a
statistical performance guarantee.

| Revision | Export | `filter()` | GPU compute inside filter | Backend |
| --- | ---: | ---: | ---: | --- |
| First working GLES denoise | 30770 / 30860 ms | 28385 ms | 27863 ms (72 planes) | GLES (2) |
| UBO slots + `glFinish` stage boundaries | 18991 / 14786 ms | 16701 ms | timing split by stage | GLES (2) |
| Shader interior fast path (final) | 11397 / 11055 ms | 8746 ms | 8208 ms (72 planes) | GLES (2) |

`analysisMs` (888 -> 795) and `restoreMs` (603 -> 592) are unchanged; the whole gain is in
the filter. `RawLabDenoiseFilter` totals for the final run: `tiles=24 planes=72
gatherMs=322 computeMs=8208 copyMs=200`, i.e. gather/copy are about 6% of the filter and
GPU execution remains 94%. A denoise-off control export of the same workflow (CPU
whole-image path) took 6010 ms, so enabling denoise now adds about 5 s instead of about
25 s. The two paths differ in encoder work and are compared as context only.

Per-plane structure on the final build (1536 x 1344, depth 6, chroma 576 x 1344):
`alloc 2.3-6.5 ms`, `dispatch 45-157 ms`, `exec 5.6-20.2 ms`, `read 4.5-20.3 ms`, with
`swt 19-51 ms`, `energy 24-106 ms`, `iswt 5-21 ms`. The 4-level 256 x 1008 plane reports
`swt=11.4 energy=5.4 iswt=2.0`. After the fast path, CPU-side submission (84 dispatches
per plane) is the largest single component for many tiles, not arithmetic.

Memory during the final 20 MP export: Native Heap about 1.21 GB, TOTAL PSS about 1.44 GB,
Swap about 118 KB. Full-resolution exports remain memory-intensive; this round does not
claim a memory reduction. The later 50 MP follow-up runs and their memory peaks are
recorded below.

## Quality Gates

The shader rewrite only changes how taps are addressed. Kahan compensation order,
threshold formula, reconstruction coefficients and window order are unchanged.

- `gpu_filter_tests` (built on device from `lutools/tests/metal_denoise_tests.cpp`,
  Android `EXCLUDE_FROM_ALL` target) compared GPU output against the wavelib/OpenCV
  reference with absolute/relative tolerance `5e-5`:

  | Case | Max abs error | RMSE |
  | --- | ---: | ---: |
  | db2 tiled filter, depth 4/6, shrink off/on | 7.75e-7 - 2.03e-6 | 7.36e-8 - 1.31e-7 |
  | db2 reconstruction (shrink off) | 7.15e-7 - 2.21e-6 | 9.99e-8 - 1.31e-7 |
  | complete wavelet denoiser, three Clean options | 5.96e-8 - 1.49e-7 | 2.82e-9 - 3.00e-8 |
  | source-pixel vignette/grain, five option sets | 0 - 6.58e-5 | 0 - 3.75e-6 |

- `wavelet_denoise_tests` on the device passes reflected tile planes, one/two-thread
  bitwise tiling parity, banded normalize/restore byte-identical to the full-image run,
  immutable AdobeRGB reload and the numerical suite.
- The exported JPEG byte length is identical (3848414 bytes) before and after the fast
  path, consistent with unchanged pixels at the same settings.
- Preview (1600 x 1200) stays at about 2.0-2.1 s and is unaffected; denoise is applied to
  the export only.

## Follow-up (2026-10-26): Diagnostics Removed, Higher-Resolution Runs

The temporary per-tile/per-plane counters were removed from
`lutools/src/core/wavelet_denoise.cpp` and `lutools/src/gpu/compute_denoise.cpp`; the three
`finish()` boundaries stay with comments describing buffer reuse and readback ordering,
and the now-unused `<chrono>`/`<android/log.h>` includes were dropped. The summary
`RawLabDenoise` line (`analysisMs/filterMs/restoreMs`) is kept as the ongoing instrument.

Re-verified after cleanup with the same 20 MP fixture and settings:

| Run | Resolution | LUT | Export | `filter()` | Backend | JPEG bytes |
| --- | --- | --- | ---: | ---: | --- | ---: |
| 20 MP, clean build | 5184 x 3888 | none | 11786 ms | 9349 ms | GLES (2) | 3848414 |
| 50 MP (`P2290056.ORF`) | 6120 x 8160 | none | 25839 ms | 20722 ms | GLES (2) | 6775893 |
| 50 MP + 7.4 MB CUBE | 6120 x 8160 | managed CUBE | 28968 ms | 19929 ms (CPU) | CPU (0) | 6217455 |

- The 20 MP JPEG byte length is unchanged from the optimized run, so cleanup did not alter
  pixels. Analysis/restore stayed at 881/592 ms.
- 50 MP without a LUT succeeds on the GLES path: `analysisMs=1765 filterMs=20722
  restoreMs=1589`. Peak memory sampled every 2 s during the export: Native Heap 2.60 GB,
  TOTAL PSS 2.79 GB (no OOM on this 16 GB device).
- 50 MP with a managed CUBE also completes. With a look active the whole-image render runs
  on the CPU (`gpuPipeline=0`, `backend=0`) because the display-input LUT is processed there,
  while the wavelet denoise stays on GLES (`gpuUsed=1`, `filterMs=19929`); peak Native Heap
  2.62 GB, TOTAL PSS 2.79 GB. The previously reported 45 MP + LUT `exit 255` OOM was not
  reproduced on this build.
- Native gates re-run on the device after the cleanup: `gpu_filter_tests` all PASS with the
  same error envelope (max 7.15e-7 - 2.21e-6 for wavelet cases, 6.58e-5 for photo
  effects) and `wavelet_denoise_tests` all PASS including the banded restore parity.
- Panasonic STD base adaptation: `LookLibraryTest` (16 JVM tests) passes, including
  `adaptStdBaseRegistersTheAdaptedLookUnderTheHeldStorageLock`, and
  `LookLibraryInstrumentedTest` passes 4/4 on the device. One device assertion was stale
  since the base commit: it expected the raw `UnsupportedOperationException` while the
  library intentionally wraps validator failures into `InvalidLookException`; the test now
  asserts the typed exception.

## Changes Behind the Numbers

- `lutools/src/gpu/compute_denoise_shader.h`: interior-pixel fast paths for the SWT,
  energy-window and ISWT stages. Interior taps use a base address and a fixed stride
  instead of per-tap `periodicIndex`/`reflectIndex` modulo and branching; edge pixels keep
  the original indexed path, so boundary semantics are unchanged.
- `lutools/src/gpu/gles_denoise.cpp`: uniform buffer split into 256-byte slots so each
  dispatch writes a fresh range, and GL error checking moved after the dispatch batch.
- `lutools/src/gpu/compute_denoise.{h,cpp}` + `gles_denoise.cpp::finish()`: explicit
  `glFinish` boundaries after the forward SWT, after the threshold stage and before the
  readback. They made wall-clock stage attribution truthful and bound buffer reuse; the
  final `finiteOutput` reads once instead of copying twice.
- `lutools/CMakeLists.txt` + `lutools/tests/metal_denoise_tests.cpp`: the GPU parity test
  now builds on Android when GPU and wavelet denoise are enabled; chroma-denoise-only
  checks are compiled out, and the guided-filter reference is skipped when the Android
  OpenCV SDK has no `ximgproc` (it has no contrib modules).
- `lutools/src/ffi/sony2fuji_c.cpp`: wavelet-denoise backend selection is decoupled from the
  whole-image pipeline. A display-input look forces the render through the CPU, but the
  filter keeps the session GPU mode, so Auto still uses GLES for denoise and only falls
  back after a real failure. The Android `RawLabDenoise` summary line was added here.

## Lessons

1. Attribute before optimizing. The first suspicion was tile gather/copy overhead; the
   split timers showed GPU compute at 98% of the filter, which moved the work into the
   shaders.
2. GLES submission is not execution. Adding `glFinish` boundaries dropped measured
   `filter()` time from 28.4 s to 16.7 s without touching arithmetic: queued dispatches
   were previously timed as if they had completed. Stage timers around GLES work need
   explicit synchronization.
3. GLSL ES `const` accepts only constant expressions. Declaring computed locals `const`
   produced 14 Adreno compile errors, silently fell back to CPU (`backend:0`, 33.2 s) and
   looked like a performance regression. Read shader compile logs before timing.
4. Per-tap index math was the dominant GPU cost, not memory bandwidth. Base-address
   recurrence cut energy windows from about 110 ms to 23 ms, ISWT from 26 ms to 5 ms and
   SWT from 45 ms to 24 ms per plane.
5. Keep an exact CPU reference gate. The fast path is only acceptable because GPU output
   stays within 5e-5 of the wavelib/OpenCV reference and the banded restore is bitwise
   identical.
6. Capture device logs as a stream started before the run. Post-hoc `adb logcat -d`
   filters returned nothing once ColorOS system logging rotated the buffer; a live
   `adb logcat ... | Tee-Object` preserved the shader error and stage logs.
7. Start the main activity before device tests on ColorOS. Without it, the frozen process
   can sit at 0% CPU and stall instrumentation.
8. Wiring inside a shader is a code change, but its removal is a change too. After the
   non-GPU diagnostics were deleted, the 20 MP export was rebuilt and re-measured; the
   unchanged JPEG byte count (3848414) was the cheapest proof that pixels had not moved.
9. `backend:0` does not always mean "denoise fell back". With a look (LUT) active, the
   whole-image render deliberately runs on the CPU (`gpuPipeline=0`), so preview/export
   report backend 0 while the wavelet filter still runs on GLES (`gpuUsed=1`). Read
   `gpuPipeline`, `fallback` and `gpuUsed` together before calling it a fallback.
10. High-resolution success is device-specific. 50 MP denoise export peaks near 2.79 GB
    PSS and still completes on a 16 GB phone, but that headroom says nothing about 8 GB
    devices; keep a lower-memory filter/restore path on the candidate list.
11. Check stale expectations against the base commit before debugging your own change. The
    failing look-library device assertion already expected the pre-ZSPro exception type at
    the base revision; the product intentionally wraps validator errors into
    `InvalidLookException`, so the test, not the library, was wrong.

## Remaining Coverage and Next Steps

- 50 MP is measured for denoise export and denoise + LUT; the earlier 45 MP + LUT OOM did
  not reproduce on this build, but memory peaks near 2.8 GB PSS still leave little headroom
  on smaller devices, so a lower-memory filter/restore path stays a candidate.
- Candidate follow-ups, none implemented yet: merge the four SWT column dispatches into
  two, cut `glMemoryBarrier` count, tile the energy window through shared memory, and
  evaluate a coarser chroma center size (1024 -> 1536 costs about 176 MB more GPU buffer).
- The Panasonic CUBE / "adapt to Panasonic std base" import lock is fixed and covered by
  the JVM regression test plus the device look-library tests; a manual import of a real
  Panasonic CUBE through the UI is still the final confirmation step.
- No Mali/PowerVR matrix, thermal endurance run, or 16 KB page-size device pass in this
  round.

## Reproduction

From `RawLabAndroid/`, with the existing Debug CMake build directory and the environment
used for the app build (`RAWLAB_LIBRAW_SOURCE`, `RAWLAB_LIBTIFF_SOURCE`, `ZSPRO_DNGSDK`,
`OPENCV_ANDROID_SDK`):

```sh
cmake --build app/.cxx/Debug/<build-hash>/arm64-v8a --target gpu_filter_tests
adb push app/build/intermediates/cxx/Debug/<build-hash>/obj/arm64-v8a/gpu_filter_tests /data/local/tmp/
adb shell 'chmod 755 /data/local/tmp/gpu_filter_tests; cd /data/local/tmp; \
  LD_LIBRARY_PATH=/data/local/tmp/lib ./gpu_filter_tests'
```

The timing test is opt-in and writes one JSON row per configuration:

```sh
adb shell am instrument -w \
  -e class 'com.rawlab.android.engine.DenoiseExportTimingTest' \
  -e denoiseTimingEnabled true \
  -e denoiseTimingRaw /sdcard/Download/P2290048.ORF \
  com.rawlab.android.test/androidx.test.runner.AndroidJUnitRunner
```

Only the owned fixture, test executables and the app-private timing directory are used;
no personal media is read or modified.
