# Fuji Pipeline and Mac Verification Client

Approved by the user in this task. Implementation details are delegated.

- Keep the C++ library as the single rendering engine for CLI, iOS and macOS.
- Decode camera data without histogram auto-bright or integer output RGB conversion. Apply the camera matrix in floating point, document the relative exposure convention, and retain negative and super-white working values until explicit gamut/display boundaries. No claim of absolute camera reflectance calibration.
- Input exposure and white balance precede F-Log2 and the film LUT. Blend the neutral display result with the film result, never with Log values. Output uses an explicit sRGB display convention; reject Log-output or unidentified LUT contracts in the photo-rendering API.
- The generic CUBE reader supports standard R-fast order, domains and floating-point table outputs. GPU and CPU use the same coordinates.
- Correct 8/16-bit RAW handling, true 16-bit PNG encoding and Metal buffer ownership. Keep existing C request layout compatible.
- Add numerical tests and real RAW smoke checks. Preserve the existing ARW and untracked DJI DNG.
- Add RawLab Mac, a native SwiftUI/AppKit utility with open/drop RAW, ten bundled film simulations, compatible custom LUTs, exposure/white-balance/strength, neutral/result comparison, synchronized zoom/pan, histogram/clipping indicators, JPEG/16-bit PNG export and errors/loading states.
- Native APIs target macOS 14+, but the verified local package targets macOS 26+ because the installed LibRaw binary requires 26. Older deployment requires matching rebuilt dependencies and is not claimed as verified. Local host-architecture build, no signing account or network service. Preserve the existing iOS project.
- Native compact toolbar, neutral gray image canvas and inspector. No landing page, catalog, cloud or batch manager.
- Deliver build/test scripts and a runnable .app. Do not commit, push or alter source images.
