# Android 集成指南 / Android Integration

通过 NDK/JNI 调用 Sony2Fuji 共享 C API。可构建的 Kotlin/Compose 客户端位于 [RawLabAndroid](../../../RawLabAndroid/README.md)，包含相册权限、JNI、源代码构建依赖和设备测试。本目录保留第三方宿主集成说明，不是独立发布的 Android SDK。

Call the shared Sony2Fuji C API through NDK/JNI. The buildable [RawLabAndroid client](../../../RawLabAndroid/README.md) includes album permissions, JNI, source-built dependencies and device tests. This directory remains a third-party integration guide, not a separately published Android SDK.

旧文档的静态全局 C++ 封装未完整实现当前 F-Log2 照片管线，且 Activity 销毁不会自动释放全局对象。以下使用共享 API 和显式会话所有权说明集成方式。

The former static-global C++ example did not implement the complete current F-Log2 photo pipeline, and destroying an Activity did not release its global objects. The guide below uses the shared API and explicit session ownership instead.

## 1. 构建依赖 / Build Prerequisites

需要 CMake 3.15+、C++17、Android NDK、pkg-config，以及为每个目标 ABI 构建的 LibRaw 和依赖。宿主机 Homebrew/Linux LibRaw 不能代替 Android 库。

Requires CMake 3.15+, C++17, the Android NDK, pkg-config, and LibRaw/dependencies built for each target ABI. Host Homebrew/Linux LibRaw cannot substitute for Android libraries.

示例从仓库根目录运行，路径占位符必须替换为你的 Android 工具链与 arm64 LibRaw 安装路径：

Run from the repository root and replace the placeholders with your Android toolchain and arm64 LibRaw installation:

```bash
export ANDROID_NDK=/path/to/android-ndk
export ANDROID_LIBRAW_PREFIX=/path/to/android-libraw/arm64-v8a

PKG_CONFIG_LIBDIR="$ANDROID_LIBRAW_PREFIX/lib/pkgconfig" \
cmake -S lutools -B lutools/build-android-arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=c++_shared \
  -DBUILD_SHARED_LIB=ON -DBUILD_CLI=OFF \
  -DSONY2FUJI_ENABLE_OPENMP=OFF
cmake --build lutools/build-android-arm64-v8a --config Release -j 6
```

Android API 24 是当前构建脚本的默认值，不代表所有设备或应用配置已验证。为其他 ABI 构建时，同步更换 `ANDROID_ABI`、构建目录和 LibRaw pkg-config 路径。

Android API 24 is the current script's default, not proof of compatibility with every device or app configuration. For another ABI, change `ANDROID_ABI`, the build directory and the LibRaw pkg-config path together.

[build.sh](../../build.sh) 的 `android` 入口可循环构建 `arm64-v8a`、`armeabi-v7a`、`x86`、`x86_64`，并复制至 `platform/android/jniLibs/<ABI>`。它从 `lutools/` 运行，依赖 `nproc` 且不会替你编译或选择各 ABI 的 LibRaw；跨平台环境优先使用上面的显式单 ABI 配置。

The `android` entry point in [build.sh](../../build.sh) loops over `arm64-v8a`, `armeabi-v7a`, `x86` and `x86_64`, copying results to `platform/android/jniLibs/<ABI>`. Run it from `lutools/`. It requires `nproc` and does not build or select per-ABI LibRaw for you; the explicit single-ABI command above is preferable when configuring a cross-platform environment.

## 2. Android Studio 集成 / Android Studio Integration

将核心和所有非系统动态依赖放进匹配 ABI 的 `jniLibs`，包括构建选择的 `libc++_shared.so`。只保留应用实际构建和测试过的 ABI。

Place the core and every non-system shared dependency in matching `jniLibs` directories, including `libc++_shared.so` when selected by the build. Include only ABIs the app actually builds and tests.

```text
app/src/main/
  java/com/example/rawlab/NativeProcessor.kt
  cpp/CMakeLists.txt
  cpp/sony2fuji-jni.cpp
  jniLibs/arm64-v8a/libsony2fuji.so
  jniLibs/arm64-v8a/<other-shared-dependencies>
```

下面是应用侧 JNI 构建示意。将 `SONY2FUJI_INCLUDE_DIR` 指向仓库的 `lutools/include`；通过应用 Gradle 的 `externalNativeBuild.cmake` 引用此文件。

This is an app-side JNI build example. Point `SONY2FUJI_INCLUDE_DIR` at the repository's `lutools/include` and reference this file from Gradle's `externalNativeBuild.cmake` configuration.

```cmake
cmake_minimum_required(VERSION 3.15)
project(sony2fuji_jni LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(SONY2FUJI_INCLUDE_DIR "" CACHE PATH "Path to lutools/include")

add_library(sony2fuji SHARED IMPORTED)
set_target_properties(sony2fuji PROPERTIES IMPORTED_LOCATION
    "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libsony2fuji.so")
add_library(sony2fuji-jni SHARED sony2fuji-jni.cpp)
target_include_directories(sony2fuji-jni PRIVATE "${SONY2FUJI_INCLUDE_DIR}")
target_link_libraries(sony2fuji-jni PRIVATE sony2fuji android log)
```

Gradle 的 `abiFilters`、CMake ABI 和打包的库必须一致。请使用宿主项目实际支持的 Android Gradle Plugin、SDK 与依赖版本，而不是把旧示例中的固定版本当成要求。

Gradle `abiFilters`, the CMake ABI and packaged libraries must agree. Use the Android Gradle Plugin, SDK and dependency versions supported by the host project instead of treating old sample version pins as requirements.

## 3. JNI 与 C API / JNI and the C API

当前接口以[公共头文件](../../include/sony2fuji/ffi/sony2fuji_c.h)为准。JNI 层需要：

The [public header](../../include/sony2fuji/ffi/sony2fuji_c.h) defines the current interface. The JNI layer should:

1. 为每个处理器拥有一个 `sony2fuji_session*`，通过 `jlong` 或等效宿主句柄管理，显式创建和销毁。
   Own one `sony2fuji_session*` per processor, expose it through a `jlong` or equivalent host handle, and create/destroy it explicitly.
2. 将 Java 字符串转为 UTF-8 路径，检查获取失败，并在使用后调用 `ReleaseStringUTFChars`。
   Convert Java strings to UTF-8 paths, check acquisition failures and call `ReleaseStringUTFChars` after use.
3. 初始化 request 的版本和大小，以及亮度/对比度/饱和度的恒等值 1。完整 RAW 导出使用 `FINAL` + `NATIVE`，不能用零尺寸 `EXACT`。
   Initialize request version/size and identity brightness/contrast/saturation values of 1. Use `FINAL` + `NATIVE` for full-resolution RAW export, not zero-sized `EXACT`.
4. 调用 `sony2fuji_process` 并返回状态码；BUFFER 输出在宿主复制或使用后必须通过 `sony2fuji_release_buffer` 释放。
   Call `sony2fuji_process` and return its status; release BUFFER output with `sony2fuji_release_buffer` after host-side copying or use.

完整的[共享 C 调用示例](../ios/README.md)同样适用于 Android 的 native 层；平台差别在 SDK、资源访问、JNI 和 UI，而不是重新实现 LUT 管线。

The complete [shared C call example](../ios/README.md) also applies to Android's native layer. Platform differences concern the SDK, resources, JNI and UI, not a separate LUT pipeline.

以下 Kotlin 类仅定义宿主侧接口；需要在 `sony2fuji-jni.cpp` 中实现所声明的 native 方法，并保持包名和 JNI 符号一致：

This Kotlin class defines an illustrative host interface only. Implement its native methods in `sony2fuji-jni.cpp` and keep package names and JNI symbols consistent:

```kotlin
package com.example.rawlab

class NativeProcessor : AutoCloseable {
    private var handle = nativeCreate()

    init { check(handle != 0L) { "Cannot create RAW session" } }

    fun render(input: String, lut: String, output: String): Int {
        check(handle != 0L) { "Processor is closed" }
        return nativeRender(handle, input, lut, output)
    }

    override fun close() {
        if (handle != 0L) {
            nativeDestroy(handle)
            handle = 0
        }
    }

    private external fun nativeCreate(): Long
    private external fun nativeRender(handle: Long, input: String, lut: String, output: String): Int
    private external fun nativeDestroy(handle: Long)

    companion object {
        init { System.loadLibrary("sony2fuji-jni") }
    }
}
```

同一实例的 render、配置和 close 必须串行调用；不要把单一静态全局处理器同时交给多个 Activity 或任务。

Serialize render, configuration and close calls on the same instance. Do not share one static global processor concurrently across Activities or tasks.

## 4. GPU 与生命周期 / GPU and Lifecycle

Android 照片后端需要 OpenGL ES 3.1，支持缩放、曝光/色彩转换、中性映射、F-Log2、LUT/强度和逐像素明暗处理。Auto 在不可用时回退 CPU；Force 会报告处理失败。RAW 解码及文件编码仍在 CPU；邻域锐化/降噪目前不走 GLES。

The Android photo backend requires OpenGL ES 3.1 and accelerates resizing, exposure/matrices, neutral mapping, F-Log2, LUT/blending and pointwise tone. Auto falls back to CPU; Force fails when unavailable. RAW decoding/file encoding and neighborhood sharpening/denoise are not GLES-accelerated.

会话拥有 EGL context、缓冲及单张线性预览缓存。全尺寸图像按设备限制分块上传；预览缓存按实际 RAW 解码修订号失效，不能供导出使用。`sony2fuji_session_get_last_backend` 返回新增的 `SONY2FUJI_BACKEND_GLES` 时，才证明本次完整像素请求在 GLES 上完成。旧的通用 LUT 函数不使用此照片缓存。

Sessions own their EGL context, buffers and one linear preview cache. Full-size images stream in bounded bands. The preview cache is keyed by the actual RAW decode revision and never supplies exports. `SONY2FUJI_BACKEND_GLES` from `sony2fuji_session_get_last_backend` confirms the photo request used GLES. The legacy generic LUT function does not use this photo cache.

在已创建 session 上设置：

Configure an existing session:

```c
sony2fuji_gpu_config config = {0};
config.version = SONY2FUJI_GPU_CONFIG_VERSION;
config.struct_size = sizeof(config);
config.mode = SONY2FUJI_GPU_AUTO;
sony2fuji_session_set_gpu_config(session, &config);
```

单次导出可以在后台执行并使用 `use` 释放会话；交互编辑应由生命周期明确的所有者复用会话和串行队列。下面的 `processor.render` 是上面的示意接口，不是随仓库提供的 Kotlin API。

For a one-off export, run in the background and use `use` to close the session. An editor should reuse its session under a lifecycle-aware owner and serial queue. The `render` method below belongs to the illustrative interface above, not a Kotlin API shipped by this repository.

```kotlin
withContext(Dispatchers.Default) {
    NativeProcessor().use { processor ->
        val status = processor.render(inputPath, lutPath, outputPath)
        check(status == 0) { "RAW processing failed: $status" }
    }
}
```

## 5. 文件访问与内存 / File Access and Memory

- native 核心接收文件系统路径，不处理 Android `content://` URI 或运行时权限。宿主应用负责取得访问授权，将资源提供为可读路径，并选择可写输出位置。
  The native core accepts filesystem paths, not Android `content://` URIs or runtime permissions. The host must obtain access, provide readable paths and choose a writable output location.
- 权限应按目标 Android 版本和文件来源配置；旧文档的 `WRITE_EXTERNAL_STORAGE` / `requestLegacyExternalStorage` 不是通用方案。
  Configure permissions for the target Android version and file source; the old `WRITE_EXTERNAL_STORAGE` / `requestLegacyExternalStorage` sample is not a universal solution.
- 预览限制尺寸和在途任务数。完成 native 工作后再关闭会话；Activity 的 `onDestroy` 不会自动释放任意 C++ 全局对象。
  Bound preview dimensions and in-flight work. Close sessions after native work finishes; an Activity's `onDestroy` does not automatically destroy arbitrary C++ globals.
- LUT assets 必须由应用复制或解析为核心可读取的文件路径。原始 RAW 和用户选择的输出路径不能相同。
  The app must make bundled LUT assets available as readable file paths. The output path must differ from the original RAW.

## 6. 排错与验证 / Troubleshooting and Verification

| 问题 / Symptom | 检查 / Check |
| --- | --- |
| `UnsatisfiedLinkError` | JNI 包名/符号、ABI、全部 .so 依赖 / JNI names/symbols, ABI and every shared dependency |
| 找不到 `libsony2fuji.so` / Missing library | `jniLibs` 和 CMake 导入路径 / `jniLibs` and CMake import paths |
| native 崩溃 / Native crash | Android Studio native debugger、logcat、会话生命周期 / Native debugger, logcat and session lifetime |
| 色彩异常 / Unexpected color | [色彩约定 / Color contract](../../docs/color-contract.md)、正确的 LUT 输入和 request 默认值 / Correct LUT input and request defaults |

```bash
adb logcat | grep Sony2Fuji
```

按你的项目启用 Android Studio native debugging。至少在实际目标设备上验证 RAW 加载、GPU 回退、取消/关闭、内存和导出；当前客户端真机结果见 [GPU 验证记录](../../../RawLabAndroid/gpu-verification.md)。

Enable Android Studio native debugging for your project. Validate RAW loading, GPU fallback, cancellation/close behavior, memory and export on actual target devices. See the current client's [device evidence](../../../RawLabAndroid/gpu-verification.md).

更多构建和数值约定见[核心 README](../../README.md)与[平台集成说明](../../docs/platform-integration.md)。

See the [core README](../../README.md) and [platform integration notes](../../docs/platform-integration.md) for build details and numerical contracts.
