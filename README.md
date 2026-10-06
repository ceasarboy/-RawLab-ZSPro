# RawLab ZSPro

跨品牌 RAW 显影与胶片风格 LUT 工作站。基于开源项目 **RawLab**（MIT）深度增强的分支版本，包含共享 C++ 处理核心、命令行工具和原生 Windows / Mac / Android 客户端。

A cross-brand RAW development and film-style LUT workstation. RawLab ZSPro is an enhanced distribution built on the open-source **RawLab** project (MIT), sharing a C++ processing core, command-line tools and native Windows / Mac / Android editors.

**原项目 / Original upstream project: https://github.com/dancancer/RawLab** —— 本仓库是其分支发行版，原始代码的 MIT 许可与版权声明完整保留，见 [LICENSE](LICENSE)。

## ZSPro 相对上游的主要增强 / What ZSPro adds on top of upstream

以下增强集中在 Android 端与共享核心（当前版本 v1.6.8）：

The enhancements below focus on the Android client and the shared core (current version v1.6.8):

### LUT 库系统 / LUT library
- **三种输入编码**：显示编码（通用风格 LUT）、F-Log2 输入（富士官方技术转换）、以及导入时按 `#Gamma` 注释自动识别。
- **批量导入 + 底座选择**：一次导入多张 `.cube`，可选自动识别 / 中性 / 松下 STD 底座；每张 LUT 渲染参考场景缩略图。
- **松下 STD 底座适配**：基于从 LUMIX LAB 提取的官方 STD→V-Log 映射数据，按"中性 → 场景线性 → STD 显示曲线（官方灰轴反演，1D 灰保持）→ look 查表"的分层链适配，强色调 LUT 无色彩断层、无插值彩噪。
- **双 LUT 组合器**：独立全屏界面，A/B 双槽 + 各自强度 + 实时预览，可烘焙存盘为新 LUT。
- **手动底座切换**：长按 LUT 在中性 / 松下 STD 底座间切换，切换即重新适配。
- **Tetrahedral（四面体）插值**：CPU / GLES / Direct3D 11 / Metal 四条渲染路径同步替换三线性，4 次采样替代 8 次，饱和区色相不漂移；格点处输出与源数据逐位一致。

### 显影与修正 / Development and fixes
- **LibRaw 0.22.0**：解锁松下 DC-S9 等新机型的 RAW 解码（0.21.x 下 S9 RW2 解压错位，画面白底紫点），相机白平衡与色彩矩阵元数据完整。
- **温度白平衡兼容修复**：LibRaw 相机矩阵表缺失的机型（如 OM-5）回退 sRGB 矩阵并锚定 6500K，色温滑杆不再失效；有完整矩阵的机型保持精确路径。
- **十项调整滑杆**：曝光、对比度、饱和度、色调曲线、锐化、高光、阴影、色温、色调、胶片强度，全链路 CPU/GPU 一致。
- **缩放检视**：双指 1–8× 质心锚定缩放，检视局部细节。
- **穷举验证体系**：R/G/B/gray 四条 256 级 ramp 全量检查、全格点普查、格点恒等、饱和度阶梯（`host-harness/ramp_check.cpp`、`sat_check.cpp`）。

## 功能预览 / Preview

![RawLab Mac：中性与 Velvia 效果对比 / Neutral and Velvia comparison](docs/images/rawlab-mac-velvia.png)

RawLab Mac 支持中性与胶片效果并排对比、胶片 LUT 切换、曝光与白平衡等参数调整，以及全分辨率导出。图中左侧为中性渲染，右侧为 Velvia 效果。

RawLab Mac provides side-by-side neutral/film comparison, LUT selection, exposure and white-balance adjustments, and full-resolution export. The screenshot shows the neutral render on the left and Velvia on the right.

## 下载 / Download

### RawLab ZSPro Android

**v1.6.8**：见 [docs/releases/android-v1.6.8.md](docs/releases/android-v1.6.8.md)。支持 Android 8.0+，ARM64 / x86_64 通用 APK；GLES 不可用时自动回退 CPU。

### 上游原版 / Upstream RawLab

[下载 RawLab Windows v0.1.0 / Download Windows v0.1.0](https://github.com/dancancer/RawLab/releases/tag/windows-v0.1.0)

支持 Windows 10/11 x64。解压后运行 `RawLab.exe`；独立运行包包含 .NET 运行时，默认 Direct3D 11 硬件加速，可自动回退 CPU。

Requires Windows 10/11 x64. Extract the ZIP and run `RawLab.exe`. The package includes the .NET runtime and supports Direct3D 11 acceleration with CPU fallback.

[下载 RawLab Mac v0.1 / Download v0.1](https://github.com/dancancer/RawLab/releases/tag/v0.1)

支持 Apple Silicon（arm64）和 macOS 26 或更高版本。应用采用 ad-hoc 签名，未经过 Apple 公证。

Requires Apple Silicon (arm64) and macOS 26 or later. The app is ad-hoc signed and has not been notarized by Apple.

[下载 RawLab Android v0.1.0 / Download Android v0.1.0](https://github.com/dancancer/RawLab/releases/tag/android-v0.1.0)

支持 Android 8.0+，提供 ARM64 / x86_64 通用签名 APK。

Requires Android 8.0+. The signed APK includes ARM64 / x86_64.

## 构建 / Build

```bash
# Android（离线构建，LibRaw 源码树可从 www.libraw.org 下载）
cd RawLabAndroid
RAWLAB_LIBRAW_SOURCE=/path/to/LibRaw-0.22.0 ./gradlew assembleDebug
```

要求 Android SDK（`local.properties` 指向）、NDK、CMake 3.22+。构建配方与验证见 `RawLabAndroid/README.md`；宿主机测试工具（含 ramp/sat 检查器）见 `host-harness/`（仓库外目录，随工作区分发）。

## 分支说明 / Branching

- `main`：上游基线（保留原始 RawLab 历史，便于对照与向上游贡献修复）。
- `zspro`：ZSPro 开发分支，所有增强与品牌化提交在此进行；稳定后合回 `main` 并打 `zspro-v*` 标签发版。
- 通用修复（构建修复、白平衡兼容等）整理在 `pr/build-fixes` 分支，适合向上游提交 PR。

## 文档 / Documentation

- [ZSPro 增强的设计依据与教训 / Enhancement lessons](docs/lessons-lut-feature.md)
- [松下管线逆向与适配研究 / Panasonic pipeline research](docs/research/05-panasonic-lumix-lab-pipeline.md)
- [Windows 客户端 / Windows editor](RawLabWindows/README.md)
- [Android 客户端 / Android editor](RawLabAndroid/README.md)
- [Mac 客户端 / Mac editor](RawLabMac/README.md)
- [处理核心与命令行工具 / Processing core and CLI](lutools/README.md)
- [RAW、色彩空间与 LUT 处理约定 / RAW, color-space and LUT contract](lutools/docs/color-contract.md)

## 开源协议与致谢 / License and attribution

- 本项目原创代码沿用上游的 [MIT License](LICENSE)（Copyright (c) 2026 RawLab contributors）；ZSPro 的修改同样以 MIT 发布。
- 应用内「系统设置 → 开源许可」展示完整的第三方声明：LibRaw 0.22.0（LGPL-2.1 / CDDL 双许可，官方源码与 SHA-256 见应用内声明）、zlib 1.3.1、stb 图像库，以及随包分发的松下 STD→V-Log 功能性数据的**个人使用边界**说明。
- 随包分发的第三方 LUT 与数据不改变其各自许可；用户自行导入的 LUT 不随本应用分发。商标与品牌名（松下 LUMIX、富士 FILM 等）归各自权利人所有，本应用与这些厂商无关。

Original code is licensed under the upstream [MIT License](LICENSE); ZSPro modifications are likewise MIT. In-app "Licenses" lists complete third-party notices (LibRaw, zlib, stb, and the personal-use boundary of the bundled Panasonic STD data).

## 知识产权声明 / IP notice

- 本项目原创代码所实现的色彩数学（色温拟合、传输函数、LUT 适配等）均来自**公开文献、公开规范与许可齐全的第三方库**（LibRaw、Adobe DNG SDK——其许可文本随仓库分发于 [lutools/third_party/Adobe-DNG-SDK-LICENSE.txt](lutools/third_party/Adobe-DNG-SDK-LICENSE.txt)），不包含任何厂商的专有代码或固件数据。
- MIT 许可按其条款提供，**不构成任何专利许可的明示授予**；相关第三方库按其自身许可（LibRaw 的 CDDL 部分含专利授权条款）提供各自的专利安排。商业分发前请自行评估所在司法辖区的专利环境。
- 本应用是独立项目，**未获得松下、富士、尼康、奥林巴斯、Adobe 等任何厂商的授权或背书**；提及的相机型号与品牌仅用于描述兼容性（名义性合理使用）。随包的松下 STD→V-Log 映射数据仅限个人使用互操作目的，禁止单独再分发。
