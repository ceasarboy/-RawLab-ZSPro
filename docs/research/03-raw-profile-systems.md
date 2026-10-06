# 03 · RAW 风格体系调研：相机色彩与"摄影风格"的规范化表示

> 调研目的：为 RawLab（LibRaw 解码 + 自研 C++ 管线 + F-Log2 LUT）论证"摄影风格如何被规范化表示"。
> 证据分级：【一手】= 规范原文 / 源码 / 官方页面原文；【二手】= 厂商文档、技术博客。日期：2026-10-03。

---

## 1. Adobe DNG 规范的色彩模型：矩阵 + 1D 曲线 + 3D 表的分层表示

一手来源：*Digital Negative Specification 1.7.0.0*（Adobe，2023-06，官方入口 https://www.adobe.com/go/dng_spec ，全文 PDF 镜像：https://github.com/emmcb/adobe-dng-sdk/blob/master/DNG_Spec_1_7_0_0.pdf ）【一手】；DNG SDK 1.7 源码镜像 https://github.com/emmcb/adobe-dng-sdk 【一手】。

**ColorMatrix1/2 与双光源标定。** 规范原文（1.7.0.0 §Tag，p.32）："ColorMatrix1 defines a transformation matrix that converts XYZ values to reference camera native color space values, under the first calibration illuminant."——注意方向是 **XYZ→相机**，用于白平衡求解。DNG 支持一/二/三组标定（CalibrationIlluminant1/2/3 + ColorMatrix1/2/3），"recommended that one of the calibrations be for a low color temperature illuminant (e.g., Standard-A) and the second … D55 or D65"（p.97）；两组之间 DNG 1.2 起强制"linear interpolation using inverse correlated color temperature"（p.97）。

**ForwardMatrix1/2。** 定义（p.51）："A matrix that maps white balanced camera colors to XYZ D50 colors."引入它的两个理由（p.99）：一是让 profile 作者控制色适应算法（推荐 linear Bradford），二是"white balance adjustment … to be done by scaling the camera coordinates rather than by adapting the resulting XYZ values, which has been found to work better in extreme cases"。前向路径公式（p.100）：`CameraToXYZ_D50 = FM * D * Inverse(AB * CC)`；无 ForwardMatrix 时走 `CameraToXYZ = Inverse(AB*CC*CM)` 再乘 Bradford 适应矩阵。DNG SDK 源码（1.4 时代，Android/LineageOS 镜像）逐行实现了 `fColorMatrix1 = fAnalogBalance * fCameraCalibration1 * fColorMatrix1` 与双光源插值（https://github.com/LineageOS/android_external_dng_sdk ，source/dng_color_spec.cpp）【一手】。

**HueSatMap：3D 查找表。** 表项由三元组构成（p.49）："The first entry is hue shift in degrees; the second entry is saturation scale factor; and the third entry is a value scale factor."应用流程（§Applying the Hue/Saturation/Value Mapping Table，p.100）：XYZ(D50) → **ProPhoto RGB 线性原色**（RIMM）→ HSV → 三线性插值索引 → 应用 hue/sat/value 增量 → 回 XYZ；色相维用 wrap-around 数学。规范明确建议"It is recommended that these tables be limited to use a ValueDivisions equal to 1"（即 2.5D，只按 hue/sat 索引）。这张表承担**相机标定残差**的非线性修正——矩阵搞不定的光谱响应差异，靠它补。

**LookTable（DNG 1.4 引入）与 ProfileToneCurve。** LookTable 与 HueSatMap 同格式、同色彩空间，但位置被规范显式区分（p.56，原文）："it should be applied **later in the processing pipe, after any exposure compensation and/or fill light stages, but before any tone curve stage**"——即 LookTable 是"创作风格层"。ProfileToneCurve（p.50）是 0–1 线性 gamma 下的 1D 默认色调曲线，"DNG readers should interpolate the curve using a cubic spline"，SDR profile 首尾必须 (0,0)/(1,1)。DNG SDK 1.7 的渲染代码（dng_render.cpp 内嵌 profile 渲染类）确认了像素级顺序：按白点选择 HueSatMap → 曝光 ramp（BaselineExposure）→ **LookTable** → **ProfileToneCurve**（附斜率延拓）→ `fRGBtoFinal` 输出空间矩阵（https://github.com/emmcb/adobe-dng-sdk/blob/master/dng_sdk/source/dng_render.cpp ，L750–1030）【一手】。

**结论：Adobe 从未用"单一 LUT"表示相机或风格，而是一个五层模型——① 色度学矩阵（物理标定，双/三光源插值）→ ② HueSatMap（标定残差，3D 表）→ ③ 曝光补偿 → ④ LookTable（风格，3D 表）→ ⑤ ProfileToneCurve（全局色调）→ ⑥ 输出空间矩阵。相机底色与创作风格在数据结构上被强制解耦。**

**对本项目的启示**：我们的"F-Log2 LUT"只相当于 ④⑤ 层；①② 层（相机矩阵 + 标定残差）缺失会让"富士风格"叠在错误的底色上。正确做法是复现分层：矩阵先行（camera→XYZ→F-Log2 工作空间），风格 LUT 置于色调映射之后；且风格层应与标定层可独立替换（例如同一 F-Log2 LUT 可套不同机身的矩阵）。

---

## 2. DCP 生态：Adobe Profile Editor、Lumariver 与 Camera Calibration 面板

DNG 1.2 起把上述 tag 组正式化为 "camera profile"（规范 p.13："DNG 1.2.0.0 and later formalizes the concept of a 'camera profile' and allows multiple camera profiles to be embedded"），文件化即 **DCP（DNG Camera Profile）**【一手】。

**Adobe DNG Profile Editor**（免费官方工具，官方入口 https://www.adobe.com/go/dng_profile_editor ，文档 https://helpx.adobe.com/camera-raw/dng-profile-editor.html ；Adobe 站点拒绝自动抓取，本节 Adobe 工具细节按官方入口定位、未逐字验证）【官方文档】。它支持两种工作方式：基于已有 profile 手工调整（即 Camera Calibration 面板同源的滑杆），或基于 ColorChecker 图表拟合生成完整 DCP。

**Lumariver Profile Designer 2**（瑞典 Xarepo，付费；页面 https://www.lumariver.com/ ，用户手册 https://www.lumariver.com/lrpd-manual/ ）【一手】。手册描述的 DCP 结构与 DNG 规范一致："linear matrix (mandatory)" + 可选 "non-linear LUT" + 色调算子 + look 预设；支持"Make DNG profiles, single, dual or triple-illuminant"，目标卡内置 ColorChecker 系列（如 ColorChecker Passport），可"Combine several targets using the multi-target feature"。输出格式三种："DNG Camera Profile (DCP), ICC format, or Cube LUT format"；手册还点破了 ICC 路线不可移植的根因："there is no standard for how the raw converter should pre-process the camera's raw data"。**自制 DCP 的标准流程**由此归纳：RAW 拍摄色卡（每种光照一张）→ 选标定光源（D50/D65）→ 拟合线性矩阵 → 可选拟合 HueSatMap 式 LUT → 导出 DCP 至 Adobe profiles 目录【一手】。

**Camera Calibration 面板**（Lightroom/ACR，官方帮助 https://helpx.adobe.com/lightroom-classic/help/camera-calibration.html ，同样未能自动验证全文）：面板的 Profile 下拉即选择嵌入的 DCP；Red/Green/Blue Primary 的 Hue/Saturation 滑杆与 Shadows Tint 在概念上对应 DCP 的矩阵/表项微调，是"用户可改风格参数"暴露为滑杆的例子【二手，待人工复核】。

**对本项目的启示**：行业已把"风格"做成可分发、可命名、带指纹签名的文件（DCP），并把用户可调范围（primaries hue/sat）设计成滑杆。RawLab 的"摄影风格"不应是裸 LUT 文件，而应是带元数据的包：矩阵 + 曲线 + LUT + 名称/许可，至少在内部 schema 上对齐 DCP 的分层。

---

## 3. LibRaw 的色彩处理：能力边界

一手来源：LibRaw 0.21 官方文档 https://www.libraw.org/docs/API-datastruct.html 与 https://www.libraw.org/docs/API-CXX.html ；源码 https://github.com/LibRaw/LibRaw 【一手】。

**数据结构**（API-datastruct.html）：`libraw_colordata_t` 含 `cam_xyz[4][3]`（camera→XYZ）、`rgb_cam[3][4]`（camera→sRGB）、`ccm[3][4]`，以及 `dng_color[2]`（`libraw_dng_color_t{ illuminant; calibration[4][4]; colormatrix[4][3]; }`）——即 LibRaw 完整读取 DNG 双光源矩阵；另有相机 tone curve `curve[0x10000]`。

**dcraw_process 路径**（API-CXX.html）：white balance（`use_camera_wb`）→ scale_colors → demosaic → highlight 恢复（`highlight` 0–9：0 裁剪、1 不裁剪、2 混合、3+ 重建）→ `convert_to_rgb`（`output_color` 0–8：raw/sRGB/Adobe RGB/Wide Gamut/ProPhoto/XYZ/ACES/DCI-P3/Rec.2020；`output_bps` 8/16）→ gamma 与 `no_auto_bright`。矩阵选择由 `use_camera_matrix` 控制（0 不用/1 用，DNG 恒用/3 取 DNG 元数据）。文档同时自我声明："The function is intended solely for demonstration and testing purposes; it is assumed that its source code will be used … as the reference material"——dcraw_process 本身不是产品管线。

**与 Adobe 的差距**：`libraw_types.h` 中**不存在** HueSatMap / LookTable / ProfileToneCurve 的对应结构（对照第 1 节 DCP 全量 tag）——LibRaw 自有色彩路径是"矩阵 + 全局 tone curve"，无 3D 表。它提供的折中是集成 Adobe DNG SDK（`use_dng_sdk`，运行警告 `LIBRAW_DNG_STAGE2_APPLIED / STAGE3_APPLIED`，即让 SDK 接管 stage2/3 渲染，见 API-datastruct.html 的 output params 表）【一手】。X-Trans 解码则由自带 `src/demosaic/xtrans_demosaic.cpp` 完成【一手】。

**生态弥补**：RawTherapee 自实现完整 DCP——`rtengine/dcp.cc` 解析 ForwardMatrix1/2、双 HueSatMap、LookTable 与 tone curve，注释直言 "// Apply the HueSatMap. Ported from Adobes reference implementation."、"Always prefer ForwardMatrix to ColorMatrix"（https://github.com/RawTherapee/RawTherapee/blob/dev/rtengine/dcp.cc ，L1160–1180、L1340、L1500–1633、L2014）【一手】；darktable 则不走 DCP，改用 color calibration（CAT）+ filmic/sigmoid 的 scene-referred 方案（见第 4 节）。

**对本项目的启示**：用 LibRaw 拿到"线性相机 RGB + 正确矩阵"是安全的；但只要产品宣称"富士/相机风格"，就必须在自研管线里补齐 3D 表环节（读 DCP 的 HueSatMap/LookTable，或用自家标定），否则我们的 LUT 前置底色比 ACR/LR 粗糙一层。

---

## 4. 开源 RAW 处理器的"风格"实现：位置即语义

**RawTherapee：HaldCLUT 胶片模拟**【一手源码 + 官方 Wiki】。
- 机制：CLUT 以 HaldCLUT 身份图（Hald identity image）编码，工具入口 https://rawpedia.rawtherapee.com/Film_Simulation （RawPedia；官方 CLUT 合集也在此分发）。源码 `rtengine/clutstore.cc` 的 `HaldCLUT::getRGB` 做三线性采样，`splitClutFilename` 约定 **CLUT 文件名可内嵌目标工作空间**（默认 sRGB），应用前经 XYZ 做工作空间↔CLUT 空间往返转换【一手】。
- 位置：应用代码位于 `ImProcFunctions::rgbProc`（`rtengine/improcfun.cc`，L2197–2240、L3141–3200）——与曝光、色调曲线、HSV 同一段落，在 ICM（输入色彩管理）之后、Lab 局部编辑之前。即：**胶片模拟被安排在"线性基色调完成之后、创作性局部处理之前"**。
- 顺序含义：先曝光/色调、后 LUT——与 DNG LookTable 的规范位置（曝光补偿之后、tone curve 之前）同构。

**darktable：scene-referred 管线**【一手：官方手册 + release notes】。
- **filmic rgb**（3.0 起，"the successor to the filmic module from darktable 2.6"，源自 Blender 的 Filmic by T. J. Sobotka；手册 https://docs.darktable.org/usermanual/4.6/en/module-reference/processing-modules/filmic-rgb/ ）：作用是"expand or contract the dynamic range of the scene to fit the dynamic range of the display"，工作于 scene-referred 像素，内置高光重建。
- **sigmoid**（4.4 起成为默认选项之一：release notes 列出 "scene-referred (filmic) / scene-referred (sigmoid)"，https://github.com/darktable-org/darktable/releases/tag/release-4.4.0 ；手册 https://docs.darktable.org/usermanual/4.8/en/module-reference/processing-modules/sigmoid/ ）：用广义 log-logistic 曲线重映射色调。
- **LUT 3D 模块**：手册原文——"normally used for film simulation and color grading"；支持 `.cube`、`.3dl`、HaldCLUT `.png`、`.gmz`；"by default, the module is placed **after the filmic rgb module** in the pixelpipe and should be applied to a **neutral image** (without first applying a specific look)"；插值提供 tetrahedral/trilinear/pyramid；且对相机 log LUT 有专门警告：F-Log、S-Log3 类 LUT "should be manually placed between the demosaic and input color profile modules"（https://docs.darktable.org/usermanual/4.6/en/module-reference/processing-modules/lut-3d/ ）【一手】。
- 版本勘误：LUT 3D 模块**始于 darktable 3.4**（2020-12 release notes 提及 lut3d 模块及其 OpenCV 依赖，https://github.com/darktable-org/darktable/releases/tag/release-3.4.0 ），并非 4.4+；4.4 引入的是 sigmoid。

**对本项目的启示**：两家开源旗舰把"风格"钉在同一位置——**色调映射之后、显示变换之前**（darktable 甚至默认如此排序）；而 log 型转换 LUT 必须在色彩空间转换前。我们的管线恰好是"F-Log2 中间态 → 矩阵 → 线性 → tone mapping → LUT"时，暗合 darktable 的两条规则：F-Log2 解码 LUT 属于后者，富士风格 LUT 属于前者，**不能混为一层**。

---

## 5. 富士特例：相机引擎、官方 LUT 与 X-Trans

**X RAW Studio**【一手】：官方定义——"X RAW STUDIO is an application that, when a FUJIFILM camera is connected via USB, uses the camera's processor to perform RAW development"（https://fujifilm-x.com/en-us/products/software/x-raw-studio/ ）。即胶片模拟的权威实现只在机身 ISP 里，桌面端借相机 SoC 复现，官方从不发布其色彩管线的数值描述。

**官方 F-Log/F-Log2 LUT**【一手】：下载页 https://fujifilm-x.com/en-us/support/download/lut/ 提供 **按机型分发**的 `.cube` LUT（GFX 系列与 X 系列，覆盖 X-H2S/X-T5/X100VI 等，2026 年仍在更新），用途是把该机型 **F-Log/F-Log2 录制素材**转换到显示空间；页面注明 "Availability of F-Log2 C / F-Log2 varies by model"。许可方面：该页**未展示**任何独立 LUT 许可条款，仅链接站点级 Terms of Use（与某些第三方转述的"仅限富士用户使用"的说法不一致，需人工核对压缩包内 EULA 后再引用）。另注意：这些 LUT 是 **log→显示** 的技术转换，与"胶片模拟"（Provia/Velvia 等）不是同一类东西；胶片模拟本身无官方 LUT。

**X-Trans 对第三方的影响**【一手代码 + 二手】：富士裸 RAW 需专用 demosaic；LibRaw 维护独立实现 `xtrans_demosaic.cpp`，RawTherapee 另带多套 X-Trans 算法（含频率域改进）。不同 demosaic 改变细节锐度与彩色噪声纹理，可视为"画质风格"的一部分；但品牌"色彩风格"的主体仍在色彩/色调体系，demosaic 差异是次要项。

**对本项目的启示**：富士没有公开 DCP；官方仅给了 F-Log2→显示的 LUT 与（不可复现的）机身胶片模拟。RawLab 若要逼近"富士原厂观感"，现实路径是：忠实实现 F-Log2 解码 LUT（矩阵+曲线部分官方可验证）+ 自标定的 X-Trans/矩阵底色 + 社区胶片模拟 CLUT（如 RawTherapee 合集）作为风格层，并明示与原厂的差异边界。

---

## 6. "相机底色"的科学表述：光谱灵敏度与 CCM

**光谱灵敏度**【二手·严谨博客】：Jack Hogan（Photographic Science and Technology）："By multiplying the cascaded responses of every component in the imaging system together wavelength by wavelength, we obtain the Spectral Response of the system as a whole"，且 CFA 染料与 QE "can vary substantially depending on generation, manufacturer and the intended application"（https://www.strollswithmydog.com/camera-spectral-sensitivity/ ；延伸：raw→三刺激值连接 https://www.strollswithmydog.com/raw-color-space-connection/ 、线性色彩变换与 CCM https://www.strollswithmydog.com/linear-color-transforms/ ）。学术界对应物是 EPFL Jiang/Gu/Süsstrunk 的相机光谱灵敏度测量集（WACV 2013, "What is the space of spectral sensitivity functions for digital color cameras?"；原 ivrl.epfl.ch 页面已下线，数据多经二手镜像引用）【二手】。

**CCM 的可测性**【二手·厂商文档】：Imatest 明确把品牌底色差异操作化：从 ≥9 色块色卡图"calculates a color correction matrix (CCM)"做线性修正（https://www.imatest.com/docs/colormatrix/ ）。结合第 1 节可见行业共识：**矩阵（CCM/ColorMatrix）只是线性最优解，光谱灵敏度差异造成的残余误差（尤其 gamut 边界）必须由 3D 表（HueSatMap）吸收**——这正是 DNG 分层结构的科学动机。

**对本项目的启示**："富士底色"应表述为可测对象：对同一色卡/光源，比较 camera RGB→目标空间的 ΔE 残差矩阵；产品文档可以用"底色 = 光谱灵敏度差异的投影"一句话讲清为什么不换 LUT 而换矩阵会造成不可调和的色偏。

---

## 7. 总结：风格表示的行业范式与 RawLab 的落点

跨体系对照（均为一手证据）：

| 层 | Adobe DNG/DCP | RawTherapee | darktable | 富士官方 |
|---|---|---|---|---|
| 物理标定 | ColorMatrix1/2(+3) + AB/CC，逆 CCT 插值 | dcp.cc 同实现 | color calibration (CAT) | 机身 ISP（不公开） |
| 标定残差 | HueSatMap（3D/2.5D 表） | dcp.cc 同实现 | — | 不公开 |
| 曝光/色调 | ProfileToneCurve（1D） | rgbProc 色调曲线 | exposure + filmic/sigmoid | 不公开 |
| 风格 | LookTable（3D 表，位置在曝光后、曲线前） | HaldCLUT（rgbProc 后段，先色调后 LUT） | lut3d 默认在 filmic rgb 后 | 官方仅 F-Log/F-Log2 `.cube`（log→显示） |
| 输出空间 | RGBtoFinal 矩阵 | ICM/输出 ICC | output color profile | LUT 出口 |

**范式**：① 相机与风格分层；② "矩阵 + 1D 曲线 + 3D 表"的混合表示；③ 风格 LUT 固定挂在色调映射之后；④ log 转换 LUT 单独归类于解码侧。对 RawLab：将内部风格 schema 升级为 DCP 同构的五层（matrix / huesat / exposure / look / tone），F-Log2 LUT 作为解码层而非风格层，即可同时获得与 ACR/LR 对话的能力和清晰的风格可替换性。
