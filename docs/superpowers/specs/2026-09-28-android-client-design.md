# RawLab Android Client Design

## Status and Scope

The user approved the first-release scope in chat on 2026-09-28: a native
Kotlin/Jetpack Compose Android client, backed by the existing C++ core, with
RAW import, built-in films, neutral/result comparison, exposure and white
balance controls, and full-resolution export. This document records the
implementation contract approved in chat, including the subsequent approval
of an in-app album browser plus a document-picker fallback.

Development stays in `codex/android-client`, in the managed worktree created
from `origin/main` at `9de1885774e2fb09492332b1abfef6025978f4d0`.

Non-goals: desktop feature parity, folder trees, histogram, arbitrary LUT
import, batch processing, camera capture, cloud services, persistent edit
catalogs, background export after process death, and store publication.

## Approach

Use one independent Gradle application under `RawLabAndroid/`, Kotlin,
Jetpack Compose Material 3, lifecycle ViewModel, coroutines, and a small JNI
adapter calling `lutools/include/sony2fuji/ffi/sony2fuji_c.h`.

This is preferred over a WebView wrapper, which would still require native
RAW/file integration, and over a cross-platform framework, which would add a
second runtime without replacing the existing Swift clients. The rendering
algorithm and public C ABI remain shared and unchanged.

Set minimum Android API to 26. Build `arm64-v8a` for phones and `x86_64` for
emulator coverage. The screen adapts to portrait, landscape, and wider
windows; dedicated tablet workflows are not part of this release.

## User Experience

The initial screen is the editor with a native open-file action, not a
marketing page. Once loaded, the photo occupies the main space. The top app
bar has open, comparison, and export actions. A continuous bottom adjustment
surface provides film selection, strength, exposure, and white balance.
In wide landscape windows, the same controls move beside the canvas so that
the short viewport does not reduce the photograph to a tiny strip.

Use neutral-gray photo backgrounds, yellow selection accents, native
Material controls, and the existing film-label bitmap assets. Controls use
Chinese resource strings; film names retain their proper names. Preserve
system insets, Back behavior, 48 dp touch targets, accessible descriptions,
light/dark themes, and scalable text. Long film names wrap without resizing
their selection slots. The control area scrolls when height is constrained.

The film picker includes Neutral plus the same ten display films used by
the Mac client. Exclude technical FLog2-to-FLog2-709 and WDR conversions.
The image fits the available canvas. Comparison toggles between the selected
result and a paired neutral/result view, stacked in narrow portrait layouts
and side by side in wide layouts. Both views use the same exposure and WB;
neutral means no film LUT, not the original embedded JPEG.

Expose film strength 0-100%, exposure -5 to +5 EV, As Shot WB, temperature
2000-50000 K, and tint -150 to +150. Use sliders and numeric entry for exposure
and custom WB, with individual and all-adjustment reset. All-adjustment
reset preserves the selected photo and film. As Shot reset restores camera
gains, not a rounded Kelvin estimate. Kelvin/tint are disabled when the core
reports no usable camera calibration. Keep the standard scene exposure
baseline fixed; advanced baseline modes are out of scope.

Show loading, rendering, export progress, recoverable failure, and export
completion explicitly. Keep the last successful image visible during edits,
but disable export until the current settings have an exact successful
preview. An import failure preserves the previous usable photo.

## Files and Resources

Provide an in-app MediaStore album grid and a system document-picker fallback.
Request READ_MEDIA_IMAGES on API 33+, additionally handle
READ_MEDIA_VISUAL_USER_SELECTED on API 34+, and READ_EXTERNAL_STORAGE with
maxSdkVersion 32 on older supported devices. Request permission only on an
album action, not at startup. Recheck access on resume and refresh the grid;
support partial access/reselection, denial, and permission revocation. The
document picker remains available without album permission. List RAW media
only; ordinary JPEG/HEIC editing is not part of this RAW client.

Offer save-to-album via MediaStore on API 29+ and save-to-file on every
supported version. Older devices use document creation rather than asking
for legacy write access. Publish MediaStore output only after a successful
copy, deleting the pending row on failure. Do not request MANAGE_EXTERNAL_STORAGE
or resolve `content://`
to guessed filesystem paths. Copy a selected input on an IO worker into an
app-private working file, using an app-generated filename. Native code only
receives app-owned filesystem paths. Provider display names are labels, not
trusted path components. Reject empty or unreadable input without replacing
the current document.

Export first renders into a new app-private temporary file, then copies the
completed file to the user-selected output URI. Freeze the source and edit
settings for that export, and reject an output URI equal to the input URI.
Never open the input URI for writing. Publish success only after copying and
closing the output stream; surface provider/storage failures and clean up
temporary output. Cancellation of a system picker changes no edit state.

Copy built-in LUT assets to app-private paths as needed, off the main thread.
Reuse repository LUTs and artwork through a build-time resource task instead
of committing duplicate large files. Include notices for the core, LibRaw,
Adobe DNG-derived code, stb, and any actual bundled dependencies.

## Rendering and Ownership

`EditorViewModel` owns observable UI state and one processing owner. A single
dedicated worker serializes native creation, rendering, export, and disposal.
One native session belongs to that owner, never to an Activity or a global
singleton. Activity recreation retains the ViewModel and active document.
Process death may return to the empty editor; this release does not claim
persistent edits or resumable exports.

Allow one in-flight render and one latest pending edit. Requests carry a
document/settings revision. A completed obsolete request cannot overwrite
newer state or enable export. Closing drops pending work, allows the current
non-interruptible native call to finish, then destroys the session on its
worker. Coroutine cancellation must not free a session used by native code.

Use bounded RGBA previews: 1000-pixel long edge while dragging, then a
1600-pixel exact preview on release or numeric entry. Neutral and film images
render serially through the same session and are published as a matching
pair. INTERACTIVE is only used for PREVIEW + BUFFER requests; exact previews
disable the interactive flag. Export uses FINAL + NATIVE and a FILE target,
never an upscaled preview. JPEG quality is 95; PNG uses the core's 16-bit
writer.

JNI initializes request version/size and identity values explicitly:
brightness/contrast/saturation and WB multipliers are 1. Camera WB uses the
legacy relative identity temperature 6500/tint 0 in the request; displayed
as-shot estimates come from `sony2fuji_session_get_raw_white_balance`.
Custom Kelvin/tint selects `SONY2FUJI_WB_TEMPERATURE`. Apply no additional
display gamma, EXIF rotation, or alternate RAW exposure compensation.

Copy returned RGBA buffers into owned Android bitmaps respecting the native
stride. Release every C buffer on all paths. JNI converts status codes to
typed failures, validates handles and arguments, and prevents C++ exceptions
from crossing the JVM boundary. Start with GPU OFF for deterministic CPU
behavior. Android GLES acceleration is not a first-release dependency, and
must not be described as equivalent to the Mac Metal pipeline.

## Build Integration

Environment audit on 2026-09-28 found OpenJDK 21.0.8, CMake 4.2.1, and
pkg-config, but no Android SDK/NDK in the checked locations and no Gradle or
sdkmanager on PATH. Homebrew LibRaw 0.21.5 is host-only. The existing
`lutools/build.sh android` neither selects per-ABI LibRaw dependencies nor
packages their runtime libraries; it is not the new client's build pipeline.

Ship a Gradle wrapper and pinned compatible AGP, Kotlin/Compose compiler,
Compose BOM, SDK, NDK, and CMake versions in the application build. Resolve
exact versions against official compatibility documentation during setup;
do not use floating dependency versions. Use `externalNativeBuild` for JNI.

Build LibRaw from a pinned upstream source for each ABI, retaining its
licenses and source provenance. Do not link Homebrew host binaries or commit
locally generated `.so` files. Keep Android-specific build glue inside
`RawLabAndroid/`; make only necessary target-scoped changes to shared CMake
if required. Disable OpenMP. Build and package all non-system native
dependencies and validate 16 KB ELF/APK alignment.

Expected modules/files:

- `RawLabAndroid/app/`: manifest, Compose editor, ViewModel, document storage,
  settings model, resource catalog, native adapter, unit/device tests.
- `RawLabAndroid/app/src/main/cpp/`: JNI implementation and native build glue.
- `RawLabAndroid/scripts/`: reproducible native dependency preparation and
  verification commands.
- `RawLabAndroid/README.md`: setup, build/install, supported scope, and actual
  verification evidence.
- Root README and Android integration guide: link to the concrete client and
  replace the documentation-only claim when the implementation exists.

## Verification and Acceptance

Write focused regressions before implementing their corresponding behavior:

1. Settings identity/ranges, As Shot reset, and request preview/export mapping.
2. One-in-flight/latest-pending behavior, obsolete-result rejection, export
   revision consistency, and close-during-render ordering.
3. Album permission full/partial/denied states across API levels, reselection,
   revocation, RAW filtering, import cancellation/failure, URI-to-private-file
   copy, output-copy failure, pending MediaStore cleanup, and unchanged input.
4. Real Sony RAW native smoke: neutral and film differ, exposure/WB controls
   affect output, previews remain bounded, JPEG/PNG preserve native dimensions,
   and PNG is 16-bit. Missing input and invalid handles fail safely.
5. Compose empty/loading/error/ready states, disabled export while dirty,
   controls, comparison, and picker cancellation.

Run the existing C++ baseline via `bash lutools/test.sh`. For Android run
Gradle unit tests, lint, debug APK assembly, native dependency/alignment
inspection, and connected instrumentation tests when a device is available.
Inspect emulator/device screenshots in portrait and landscape, light/dark,
and enlarged text. Check a real RAW workflow end to end, including reopening,
rotation during render, and exporting a readable file.

Full-resolution RAW processing has significant native memory demand even
when previews are small. Record actual device/emulator memory and failure
behavior; do not promise arbitrary sensor sizes on low-memory phones. No
performance claim or all-camera support claim follows from compiling an APK.

An SDK/NDK, Java runtime, emulator, or fixture that is unavailable is a named
verification blocker, not a passing check. Do not publish or push a release
as part of this task.

## References

- [Shared color contract](../../../lutools/docs/color-contract.md)
- [Shared C ABI](../../../lutools/include/sony2fuji/ffi/sony2fuji_c.h)
- [Current Android integration](../../../lutools/platform/android/README.md)
- [Android document storage](https://developer.android.com/training/data-storage/shared/documents-files)
- [Compose BOM](https://developer.android.com/develop/ui/compose/bom)
- [Native 16 KB support](https://developer.android.com/guide/practices/page-sizes)
