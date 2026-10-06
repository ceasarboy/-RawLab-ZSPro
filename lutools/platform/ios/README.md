# iOS 集成指南 / iOS Integration

通过共享 C API 将 Sony2Fuji 核心接入 Swift/Objective-C 应用。此目录是集成文档，不包含可直接运行的独立 iOS 示例项目。

Integrate the Sony2Fuji core into a Swift/Objective-C app through the shared C API. This directory contains integration documentation, not a standalone runnable iOS sample.

旧版文档中的直接 C++ 桥接示例未覆盖完整的 F-Log2 和显示输出处理，已改为调用共享照片 API，避免移动端实现另一套色彩流程。桌面验证结果不代表 iOS 设备已验证。

The former direct-C++ bridge examples omitted parts of F-Log2 and display-output processing. Use the shared photo API instead of implementing a second mobile color pipeline. Desktop verification does not establish iOS device coverage.

## 1. 构建准备 / Build Prerequisites

需要 macOS、完整 Xcode/iOS SDK、CMake、pkg-config，以及为目标平台和架构编译的 LibRaw 和依赖。不可把 macOS Homebrew LibRaw 链接到 iOS。

Requires macOS, full Xcode with the iOS SDK, CMake, pkg-config, and LibRaw/dependencies built for the target platform and architecture. Do not link macOS Homebrew LibRaw into iOS.

仓库脚本预期以下 LibRaw pkg-config 目录，详见 [build.sh](../../build.sh)：

The repository script expects these LibRaw pkg-config directories; see [build.sh](../../build.sh):

```text
lutools/build-ios-libraw/install/iphoneos-arm64/lib/pkgconfig
lutools/build-ios-libraw/install/iphonesimulator-arm64/lib/pkgconfig
lutools/build-ios-libraw/install/iphonesimulator-x86_64/lib/pkgconfig
```

准备好依赖后，从仓库根目录运行：

Once dependencies are ready, run from the repository root:

```bash
cd lutools
bash build.sh ios
```

该脚本在选择 iOS 目标前还会运行默认环境的 `pkg-config --exists libraw` 检查。因此默认 pkg-config 搜索路径也需要可发现 LibRaw；宿主机安装只能满足此预检，不能替代上面的 iOS 依赖。只准备了目标平台依赖时，可使用下面的手动 CMake 命令绕过该宿主预检。

Before selecting an iOS target, the script also runs `pkg-config --exists libraw` in the default environment. LibRaw metadata must therefore be discoverable on the default pkg-config search path. A host installation can satisfy this preflight but cannot replace the iOS dependencies above. If only target-platform dependencies are available, use the manual CMake command below to bypass that host preflight.

该脚本会重建自己的 `build-ios-*` 输出目录。手动构建单个设备目标时，可从仓库根目录运行以下命令；`DEVELOPER_DIR` 必须指向可用且已配置的完整 Xcode。

The script recreates its own `build-ios-*` output directories. To build one device target manually, run the following from the repository root. `DEVELOPER_DIR` must point to an available, configured full Xcode installation.

```bash
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
PKG_CONFIG_PATH="$PWD/lutools/build-ios-libraw/install/iphoneos-arm64/lib/pkgconfig" \
cmake -S lutools -B lutools/build-ios-iphoneos-arm64 -GXcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT="$(xcrun --sdk iphoneos --show-sdk-path)" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DBUILD_SHARED_LIB=ON -DBUILD_CLI=OFF -DIOS=ON
cmake --build lutools/build-ios-iphoneos-arm64 --config Release
```

`13.0` 是当前核心构建脚本的目标值，不是已验证的应用兼容性承诺。应检查生成物、依赖和实际目标设备。

`13.0` is the current core script's deployment setting, not a verified app compatibility claim. Check the resulting binaries, dependencies and actual target devices.

## 2. Framework 与 Xcode / Frameworks and Xcode

当前 CMake 的 iOS 共享目标生成 `sony2fuji.framework`。设备与模拟器构建必须分别使用对应 SDK 和 LibRaw。

The current iOS shared CMake target produces `sony2fuji.framework`. Device and simulator builds require their corresponding SDK and LibRaw.

已有设备和 arm64 模拟器 Framework 后，从仓库根目录打包：

After building device and arm64-simulator frameworks, package them from the repository root:

```bash
xcodebuild -create-xcframework \
  -framework lutools/build-ios-iphoneos-arm64/Release-iphoneos/sony2fuji.framework \
  -framework lutools/build-ios-iphonesimulator-arm64/Release-iphonesimulator/sony2fuji.framework \
  -output lutools/build-ios/sony2fuji.xcframework
```

以上示例提供 arm64 模拟器切片；需要 Intel 模拟器时，应先准备包含所需模拟器架构的 Framework，再打包。不要把两个同平台的单架构模拟器 Framework 当成独立平台重复加入。

This example includes an arm64 simulator slice. For Intel simulators, first prepare a simulator framework containing the required simulator architectures. Do not add two single-architecture simulator frameworks as if they were distinct platforms.

1. 将 Framework/XCFramework 加入目标的 `Frameworks, Libraries, and Embedded Content`，动态 Framework 使用 `Embed & Sign`。
   Add the framework/XCFramework to the target's `Frameworks, Libraries, and Embedded Content`; use `Embed & Sign` for the dynamic framework.
2. 配置头文件搜索路径指向 `lutools/include`，在桥接头中引入下面的 C 头文件。
   Point header search paths to `lutools/include` and import the C header below in the bridging header.
3. 同时处理目标架构的 LibRaw、C++ 运行库和其他动态依赖，并检查签名与加载路径。
   Include target-architecture LibRaw, the C++ runtime and other dynamic dependencies, checking signatures and load paths.

```objc
#include "sony2fuji/ffi/sony2fuji_c.h"
```

## 3. 共享 C API / Shared C API

以下示例演示一次全分辨率文件导出。输入、LUT 和输出路径由调用方提供，输出不得覆盖原 RAW；不要用 `EXACT` 配合零宽高表示原尺寸。

The example performs one full-resolution file export. The caller supplies input, LUT and output paths; output must not overwrite the RAW. Do not use `EXACT` with zero dimensions to request native size.

```c
#include "sony2fuji/ffi/sony2fuji_c.h"

sony2fuji_status render_photo(const char* input, const char* lut, const char* output) {
    sony2fuji_session* session = NULL;
    sony2fuji_status status = sony2fuji_session_create(&session);
    if (status != SONY2FUJI_STATUS_OK) return status;

    sony2fuji_request request = {0};
    request.version = SONY2FUJI_REQUEST_VERSION;
    request.struct_size = sizeof(request);
    request.input_type = SONY2FUJI_INPUT_RAW;
    request.input_path = input;
    request.lut_path = lut;
    request.lut_strength = 1.0f;
    request.wb_mode = SONY2FUJI_WB_CAMERA;
    request.temperature = 6500.0f;
    request.brightness = request.contrast = request.saturation = 1.0f;
    request.intent = SONY2FUJI_INTENT_FINAL;
    request.size_mode = SONY2FUJI_SIZE_NATIVE;
    request.output_target = SONY2FUJI_TARGET_FILE;
    request.output_path = output;
    request.output_format = SONY2FUJI_OUTPUT_PNG;

    status = sony2fuji_process(session, &request, NULL);
    sony2fuji_session_destroy(session);
    return status;
}
```

交互编辑应复用 session，而不是每个滑杆事件都创建一个。所有同一 session 的调用串行执行；释放输出缓冲用 `sony2fuji_release_buffer`，结束会话用 `sony2fuji_session_destroy`。失败时读取 `sony2fuji_status_message`。

For editing, reuse a session rather than recreating it for every slider event. Serialize all calls on that session. Release output buffers with `sony2fuji_release_buffer` and sessions with `sony2fuji_session_destroy`; inspect `sony2fuji_status_message` on failure.

`SONY2FUJI_WB_CAMERA` 配合 6500 K/0 tint 表示不附加相对 RGB 偏移，不表示相机拍摄色温固定为 6500 K。绝对 RAW Kelvin 使用 `SONY2FUJI_WB_TEMPERATURE`，拍摄值通过 getter 获取。

With `SONY2FUJI_WB_CAMERA`, 6500 K/0 tint adds no relative RGB shift; it does not assert that the camera shot at 6500 K. Use `SONY2FUJI_WB_TEMPERATURE` for absolute RAW Kelvin and the getter for estimated as-shot values.

## 4. Metal 与预览 / Metal and Previews

在已创建的 session 上设置 GPU 模式。Auto 在加速失败时回退 CPU，Force 用于显式验证 GPU，不能作为所有设备都支持的保证。

Set GPU mode on an existing session. Auto falls back to CPU if acceleration fails; Force is for explicit GPU validation, not a guarantee that every device supports it.

```c
sony2fuji_gpu_config config = {0};
config.version = SONY2FUJI_GPU_CONFIG_VERSION;
config.struct_size = sizeof(config);
config.mode = SONY2FUJI_GPU_AUTO;
sony2fuji_session_set_gpu_config(session, &config);
```

预览使用 `SONY2FUJI_INTENT_PREVIEW` 和 `preview_long_edge`；需要输出像素时选择 BUFFER 并传入缓冲结构。缩小预览和交互 half-size 不能替代完整导出；请保留精确渲染与导出的质量隔离。

Use `SONY2FUJI_INTENT_PREVIEW` and `preview_long_edge` for previews. Select BUFFER and supply a buffer struct when requesting pixels. Reduced previews and interactive half-size processing must not replace full-quality exports.

## 5. Swift、资源与性能 / Swift, Resources and Performance

- 使用轻量 Swift/Objective-C 封装调用 C API，在后台串行队列处理图像，在主线程更新 UI。
  Call the C API from a thin Swift/Objective-C wrapper, process images on a serial background queue, and update UI on the main thread.
- 将选择的 CUBE 加入应用资源，传入实际资源路径。例如仓库的 `FLog2_to_ETERNA_65grid_V.1.00.cube`；不能假定 `ETERNA_BT709.cube` 已存在。
  Add the selected CUBE to app resources and pass its real path, such as the repository's `FLog2_to_ETERNA_65grid_V.1.00.cube`; do not assume `ETERNA_BT709.cube` exists.
- 大图会同时占用 RAW、浮点图像、GPU 和显示缓冲内存。限制在途任务、复用当前会话，并及时释放缓冲。
  Large images consume RAW, floating-point, GPU and display buffers. Limit in-flight work, reuse the current session and release buffers promptly.
- 当前应用侧适配可参考 [iOS 处理封装](../../../RawLab/RawLab/Core/RawProcessing/Sony2FujiProcessor.swift)；不要把 AppKit 代码直接搬到 iOS。
  See the [iOS processing adapter](../../../RawLab/RawLab/Core/RawProcessing/Sony2FujiProcessor.swift); do not copy AppKit-specific code into iOS.

## 6. 排错与参考 / Troubleshooting and References

| 问题 / Symptom | 检查 / Check |
| --- | --- |
| Framework 加载失败 / Framework fails to load | Embed & Sign、SDK、架构、动态依赖路径 / Embedding, signing, SDK, architecture and dependency paths |
| 找不到符号 / Missing symbols | C++、LibRaw 及其依赖是否链接到正确目标 / Correct linkage of C++, LibRaw and its dependencies |
| 输出尺寸无效 / Invalid output size | NATIVE 或正数 EXACT 尺寸 / NATIVE or positive EXACT dimensions |
| 内存或延迟过高 / Excessive memory or latency | 有界预览、串行队列、释放缓冲 / Bounded previews, serial work and buffer release |

[公共头文件](../../include/sony2fuji/ffi/sony2fuji_c.h)是当前接口依据；[C API 说明](../../docs/ios-api.md)中的历史示例应与头文件和本页核对。[色彩约定](../../docs/color-contract.md)定义 LUT 输入及白平衡行为。

The [public header](../../include/sony2fuji/ffi/sony2fuji_c.h) is the current interface reference. Cross-check historical examples in the [C API notes](../../docs/ios-api.md) against the header and this page. The [color contract](../../docs/color-contract.md) defines LUT input and white-balance behavior.

构建和设备兼容性需要在你的 Xcode、iOS SDK 和目标设备上验证；本次文档更新未执行 iOS 构建。

Validate builds and compatibility with your Xcode, iOS SDK and target devices. No iOS build was run for this documentation update.
