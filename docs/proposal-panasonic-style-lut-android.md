# 方案草案：安卓端自定义 LUT 载入 + 编辑补全（松下 Lumix 式体验）

> 状态：**草案，待定夺**（2026-10-03 记录）。本文档尚未实现，仅记录调研结论与分阶段计划。
> 基线：仓库 `main` 分支 + 本地两处构建修复（见文末"环境备注"）。
> **更新（2026-10-03 晚）：P0 + P1-1 已交付并经用户真机验证（LUT 三模式导入/组合器/双 LUT/底座适配/四调整滑杆/缩放检视/批量导入/缩略图）。裁切经真机验证异常后整体回退，缩放检视替代。STD 底座适配已实现（官方映射表求逆），牛顿符号 bug 已修，待真机重验观感。全过程复盘见 docs/lessons-lut-feature.md。文献调研已完成，结论使本方案的多处假设得到修正与强化，见 [`docs/research/00-synthesis-feasibility.md`](research/00-synthesis-feasibility.md)（含对 P0–P3 的修订与新增的"P0− 架构分层"步骤）。**

## 背景与目标

松下 Lumix 相机支持把 LUT 文件载入机身、在取景/成片上直接生效，配套 Lumix Lab 应用负责 LUT 管理，但其编辑能力弱（裁切、亮度等）。目标：在 RawLab Android 上实现同级的 **任意 LUT 载入/管理**，并补齐 **编辑短板**，形成"RAW 全尺寸显影 + 自定义风格 LUT + 完整调整"的安卓应用。

## 三个核心问题的调研结论

### 1. 各家 RAW 是否自带风格？

**有"底色"，没有"风格"。**

- RAW = 传感器马赛克线性数据 + 元数据（WB 系数、相机色彩矩阵、标定）。风格（佳能暖、Leica 德味、理光正片、富士胶片模拟）几乎全部在各家 JPEG 引擎里：默认色调曲线、分区饱和度、色相微调。
- 例外：富士 RAW 内记录胶片模拟选择，但只是设置标记，需富士引擎套用。
- 可测量的"底色"差异确实存在：各家 CFA 染料光谱响应不同 → 相机原生色域不同 → 同场景经各自矩阵转 sRGB 后肤色/天空/植被色相有系统性偏移。
- 对本方案的意义：管线以 sRGB 线性为归一化工作空间（现有做法），LUT 锚定在明确声明的编码上——这正是松下 V-Log LUT 模式的优越性（LUT 与机身解耦）。同一 LUT 在不同底色机身上输出有细微差异，属物理事实，契约声明即可（现有立场：不承诺跨相机色彩等价）。

### 2. 如何提取合理的 LUT（富士/理光/Leica 风格）？

按可靠度排序的三个来源：

| 来源 | 适用 | 说明 |
|---|---|---|
| **A. 官方公开 LUT** | 富士、松下 | 富士官方发布 GFX 胶片模拟 LUT（ETERNA 等，社区"胶片工坊"项目即以此为参考）；V-Log LUT 官方提供。**Leica、理光无官方 LUT** |
| **B. 参考配对拟合** | 全部（无官方 LUT 时唯一途径） | 同场景获得"中性基准 + 目标风格"两组渲染（目标机身拍色卡+多场景，或官方样张），逐亮度层拟合 3D LUT |
| **C. 社区现成 LUT 包** | 视授权 | 授权参差，逐个核查后方可入库（沿用 ThirdPartyNotices 立场） |

拟合（来源 B）的工程要点——决定成品质量：

- **钉死输入/输出编码**：LUT 文件头带 `#Gamma/#Gamut` 合约注释，沿用仓库现有机制；
- **高光滚降拆成 1D 色调曲线**，不塞进 3D LUT（立方体网格对陡峭高光采样不足）；
- 单调性/正则化约束防阴影噪声放大与色带；肤色区加权；
- **留出场景**计算 ΔE00 验证，不只看拟合集。

工具链：`lutools` CLI 已是现成的"中性基准渲染器 + 验证器"，新增 `fit-lut` 子命令；拟合与 ΔE 用 Python colour-science。预期管理：拟合可达"观感近似"——Leica 层次感（中低对比曲线+色相偏移）可达性好；理光分裂调（冷阴影/暖高光）可达性中等；**复刻不了色彩科学本身**（契约一贯立场）。

### 3. 松下 app 的弱项 → 我们的差距清单（已对照代码核实）

| 能力 | Lumix Lab | RawLab 现状 |
|---|---|---|
| LUT 载入/管理 | 强 | 仅内置 10 个富士 LUT；photo API 拒绝非 F-Log2 合约的外部 LUT |
| 裁切 | 弱 | 管线无 crop 概念 |
| 亮度/色调 | 弱 | 曝光已有；`sony2fuji_c.h:118-128` 中 **contrast/saturation/tone_curve/sharpening 核心已支持，安卓 JNI 未露出**（只传 strength/exposure/温度/色调） |
| RAW 全尺寸导出 | 弱/无 | 强（JPEG / 16-bit PNG，GLES 加速，双级预览） |

关键底座（可直接复用，无需造轮子）：

- `LUTApplicator`：通用三线性插值，不挑 LUT（`lutools/src/core/lut_applicator.cpp`）；
- GLES 端 `sampler3D` + `lut_size` uniform：与网格尺寸无关，33/65 皆可（`lutools/src/gpu/gles_lut.cpp`）；
- `LUTParser`：已能读 CUBE；合约校验在 photo API 层，非解析器层。

## 分阶段开发计划

### P0：自定义 LUT 载入（对齐松下体验）——核心增量

1. C API 新增"通用 LUT 模式"：跳过 F-Log2 合约校验；输入编码二选一（现有中性 log-logistic 显示曲线 / F-Log2 log）；LUT 元数据（名称、编码）进渲染缓存键。
2. 合约文档更新：`lutools/docs/color-contract.md` 增加"通用模式"章节，声明输入编码约定与不做跨相机等价承诺。
3. 安卓端：
   - SAF 文件选择导入 `.cube`；
   - LUT 库管理界面（列表/缩略图/改名/删除；缩略图 = 中性样张套 LUT 渲染小图）；
   - 输入空间选择器（显示曲线/log）。

### P1：编辑补全

1. JNI/Kotlin 露出 contrast、saturation、tone_curve、sharpening（核心已支持，改动小）。
2. 新增裁切：crop 矩形数据结构 + 交互预览按裁切视口渲染 + 预览坐标→全尺寸导出坐标映射；导出不写回原图的既有原则不变。

### P2：LUT 生产线（桌面侧）

1. `lutools` 新增 `fit-lut` 子命令（输入：参照对/色卡数据；输出：带合约注释的 `.cube`）。
2. 首批风格包：富士系从官方 GFX LUT 转编码；理光/Leica 走拟合；每个 LUT 附 ΔE 验证记录（沿用仓库 verification 文化）。

### P3（可选，需先确认范围）

"把 LUT 灌进松下机身"（Lumix Lab 的传输半边）是 MTP/无线通道的另一套工程，与 RAW 编辑无关。**建议立项时切开，不混入本方案。**

## 待定夺的问题

1. P0 输入编码默认值：中性显示曲线（对齐现有中性预览）还是 log（对齐松下习惯）？倾向：两者都给，默认显示曲线。
2. 社区 LUT 包是否入库：授权核查工作量大，倾向 P2 自产优先、社区包仅做个人使用不随包分发。
3. P3 是否需要。

## 环境备注（本地构建已验证）

- 本机 SDK 在 `F:\rawlab\android-sdk`，Gradle 在 `F:\rawlab\tools\gradle-8.11.1`；构建命令见 `RawLabAndroid/README.md` + `RAWLAB_LIBRAW_SOURCE=F:/rawlab/tools/LibRaw-0.21.5`（离线 LibRaw）。
- 本地补丁一：`lutools/src/ffi/sony2fuji_c.cpp:695` NDK `__int128` → `to_string` 二义性修复（值得反馈上游）。
- 本地补丁二：`RawLabAndroid/app/build.gradle.kts` 增加 `RAWLAB_LIBRAW_SOURCE` 环境变量开关（不设时行为同上游，适合提 PR）。
