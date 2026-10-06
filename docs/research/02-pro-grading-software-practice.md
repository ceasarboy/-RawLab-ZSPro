# 专业调色/后期软件的 LUT 使用实践调研（DaVinci Resolve / FilmLight / ACES / OCIO）

> 目的：为 RawLab（安卓 RAW 显影编辑器，支持松下 Lumix 式"用户载入 LUT"）提炼专业软件对 LUT 的正确用法、行业共识与反面教训。
> 写作约定：**【一手】**= 直接核对官方文档/白皮书/规范原文；**【二手】**= 经可信第三方转述或社区权威讨论确认。所有引文均为英文原文摘录。
> 调研日期：2026-10-03。前置阅读：`01-color-science-lut-math.md`（传递函数/LUT 数学基础）。

---

## 0. TL;DR

专业管线对 LUT 的共识可以压缩成一句话：**LUT 是"实现层"的交换格式，不是"语义层"的色彩管理体系**。DaVinci Resolve 官方手册明确把 LUT 定位为手动工作流的工具，并点名 LUT 相比色彩管理的三个缺陷（钳位越界数据、插值方法造成跨软件不一致、需要为每个源/目标对维护文件）；FilmLight 则从 2010 年的 Truelight Primer 到 2023 年的官方文章一以贯之：优先用 GPU 上直接求值的数学公式，LUT 会削顶且精度不足。ACES 把"风格"定义为 LMT（一个输入输出都有明确定义的变换链），OCIO 把"编码变换链"放在配置文件里而不是 LUT 文件里——两者都是"用语义化的变换链取代裸 3D LUT"。对我们的启示：**支持 LUT，但把 LUT 视为"带声明编码的一种变换格式"，内部管线一律走参数化/公式化的色彩管理，LUT 只在边缘进入（导入、预览、导出）**。

---

## 1. DaVinci Resolve：官方文档中 LUT 的定位，RCM 与 CST

**【一手】来源：DaVinci Resolve 19.1 Reference Manual（2025-02），PDF：[documents.blackmagicdesign.com — DaVinci_Resolve_19_Reference_Manual.pdf](https://documents.blackmagicdesign.com/UserManuals/DaVinci_Resolve_19_Reference_Manual.pdf)**（下称"手册"）。以下引文均出自该 PDF 文本层。

### 1.1 四种色彩科学：LUT 属于"手动"一侧

手册把 Project Settings 的 Color Science 分为四档："**DaVinci YRGB color science**: in which you manage all and any color transforms from one color space to another **manually, using either LUTs or manual adjustments**. **DaVinci YRGB Color Managed**: Enables the Resolve color-managed workflow (RCM) for grading. **DaVinci ACEScc / ACEScct**: standardized color management schemes." 即：官方把"LUT 驱动"与"色彩管理驱动"视为两条并列且互斥的工作路线，LUT 是非托管路线的实现手段。

### 1.2 RCM 的场景参考工作流与官方"反 LUT"表述

手册在 RCM 一章明确对比了手动 LUT 流程：

> "However, RCM's automation can make this process faster by **freeing you from the need to locate and maintain a large number of LUTs** to accommodate your various workflows. Also, the **matrix math** used by RCM (as well as the Color Space Transform operation) **extracts high-precision, wide-latitude image data** from each supported camera format... These are all advantages when compared to lookup tables, which **can have plenty of precision, but can clip out-of-bounds image data** and **introduce issues when differing lookup table interpolation methods cause minor inconsistencies with color space transformations from application to application**."

这是厂商文档里少见的、把 LUT 三大缺陷写进正文的一句话：越界钳位、插值不一致、跨软件不可移植。与之配套，RCM 从场景参考数据出发，"you can convert from a larger color space to a smaller one and back again without clipping or a loss of quality"——并注明 "**if you apply a LUT** or use Soft Clip within a grade, **then clipping will occur**"。RCM 预设（SDR Rec.709、HDR Rec.2020、**DaVinci Wide Gamut**——"a log-encoded grading space"作为调色中间空间等）负责 Input Color Space → Timeline working space → Output Color Space 的全自动转换，越界色按预设被裁剪或映射。

### 1.3 CST（Color Space Transform）：点变换的角色

当用户不走 RCM 时，手册推荐用 **CST ResolveFX/Fusion 节点**做逐节点的编码转换："Exposes four pop-up menus that let you set an **Input Colorspace, Input Gamma, Output Colorspace, Output Gamma**"，并带 **Gamut Mapping** 控制："to accommodate workflows where you need to transform one color space into another that has a **dramatically larger or smaller gamut**"。CST 本质是"带参数的矩阵+传递函数变换"（可精确求值、可声明编码），这正是它被放在 RCM 体系内、而裸 LUT 被放在手动体系内的原因。RCM 还允许对个别 clip "**Bypass Color Management** ... to manually manage that clip using LUTs, the Color Space Transform node, or simply by doing manual grading"——说明官方模型里 LUT 是色彩管理的**例外路径**，不是默认路径。

### 1.4 LUT 的挂载点与插值：官方给出的工程细节

- **挂载点**：Lookup Tables 面板提供 Input / Output / Video Monitor / Color Viewer / Scopes 五组 1D+3D LUT 槽位，规则是 "**1D LUTs at each step are always applied before 3D LUTs**"。关键语义区分：Video Monitor LUT "are **only temporarily applied for purposes of monitoring; they're never applied to rendered media**"，官方推荐的 Display LUT 用途是 "applying a **film print emulation LUT in a Log workflow**, or ... a **monitor calibration LUT**"——即**观看/监看用途**，不进入交付数据。
- **格式**："3D LUTs that are created by DaVinci Resolve are in the **.cube format, configured as 33x33x33 cubes with 32-bit floating point processing**. DaVinci Resolve can also read and use LUTs in the **Shaperlut format**"（shaper 前置曲线格式）。
- **插值**：Project Settings 提供 **3D Lookup Table Interpolation**：**Trilinear（默认）**——"backward compatible ... and **matches the look of LUTs being applied in other applications**"；**Tetrahedral**——"higher image quality LUT and DCTL processing, with **reduced color-banding**... recommended for projects that don't need backward compatibility with ... LUTs created in other applications"。官方同时承认 trilinear 是为了"和其他软件长得一样"，tetrahedral 才是画质上限——**这是"同一 .cube 各软件效果不同"的官方级确认**。
- **钳位**：Color 页节点内 LUT 有专门 NOTE："**Applying a LUT within a node will clip any image data falling below 0 and above 1.**"
- **导出限制**：Generate 3D LUT 时 "you should limit yourself to using only **Primaries palette and Custom Curves** palette controls ... **These are the only grading controls that can be mathematically converted into a LUT**"，Windows/OpenFX 节点"will be ignored"——官方亲口承认 LUT 只能承载调色语言的一个极小子集。

### 1.5 对本项目的启示

- 采用 Resolve 的**五槽位模型**的简化版：区分"数据路径 LUT"（Input/Output，参与像素交付）与"监看路径 LUT"（Monitor/Viewer，永不写入导出数据）。移动端默认只在数据路径放一个 LUT 槽。
- 节点内 LUT 必须**先决定工作域**：要么在 clamp 前（0-1 以外的数据先经过我们的线性工作空间再进 LUT 的输入域），要么明确把 LUT 输入钳位到其声明域并提示用户。
- 3D LUT 插值直接上 **tetrahedral**（若性能不足再降 trilinear 并在 UI 标注）；这是官方盖章的画质排序。

---

## 2. FilmLight Baselight 的反面立场

### 2.1 "Why use a Truelight profile instead of a LUT?"（2010 白皮书）

**【一手】FilmLight《Truelight Primer》（FL-TL-TN-0369 v1.1, 2010）：[filmlight.ltd.uk/pdf/whitepapers/FL-TL-TN-0369-Primer.pdf](https://www.filmlight.ltd.uk/pdf/whitepapers/FL-TL-TN-0369-Primer.pdf)**。该文档有专门一节标题即"Why use a Truelight profile instead of a LUT?"，论据原文：

> "**A LUT can only pair one input space with one output space.** If the output space is an uncalibrated display then a LUT built for it (using some suitable calibration data) cannot be used for anything else. To use LUTs effectively, you therefore need to **create a LUT for each specific display device**. **A LUT also cannot tell you anything about any input colours that may have fallen outside the output display's gamut.**"

对应的正面方案是"profile（语义化校准数据的集合）→ 现场生成 LUT cube + **gamut alarm**（out-of-gamut 高亮诊断）"：端口性来自元数据，精度来自"按需生成"，色域问题由显式检测接管。注意 FilmLight 并不否认 cube 的用途——Primer 全篇讲的就是如何**正确地生成与应用** viewing cube（"The cube does not alter the source data, but provides a **viewing transformation**"）。

### 2.2 2023 年官方文章：公式 vs LUT 的明确排序

**【一手】FilmLight《Colour in modern broadcast post-production》（2023-03）：[filmlight.ltd.uk/store/news_articles/colour-in-modern-broadcast-post-production/](https://www.filmlight.ltd.uk/store/news_articles/colour-in-modern-broadcast-post-production/)**。在讲色彩管理（每一步都知道自己在什么空间）时给出关键句：

> "These transformations **can be applied via look-up tables (LUTs), but applying formulas directly on the GPU is better, as they do not clip the signal and are much more precise.**"

文章同时给出 LUT 误用的教科书案例（对应我们支持的 Lumix 场景）：

> "If you apply a **V-Log to Rec.1886 conversion LUT directly to V-Log material as a first step**... you could have just shot in Rec.709."

——即：把"归一化 LUT"当作第一步，等于主动扔掉 log 场景参考数据的全部动态范围优势。正确做法是 scene-referred 调色 + DRT 收尾（"The DRT achieves SDR and HDR output through **global mapping**"），交付端才做"trim pass"。

### 2.3 Truelight Colour Spaces：用浮点 GPU 公式取代 3D LUT 机制

**【一手】[filmlight.ltd.uk/workflow/truelight.php](https://www.filmlight.ltd.uk/workflow/truelight.php)**：Truelight Colour Spaces 让色彩空间转换拥有 "the speed, accuracy, and dynamic range permitted by **native floating-point GPU functionality**"，其函数集 "allows complex transforms **formerly only possible with 3D LUT mechanisms**"。Baselight 5.0 引入的架构元素：Mastering Colour Space、新默认工作空间 "**FilmLight T-Log / E-Gamut**"、新 DRT 家族 "**Truelight CAM**"（用户口中的 T-CAM）与 "Truelight Scene Looks"。命名澄清：检索 FilmLight 官方站点未发现名为 "CHROMA" 的管线产品；其现代体系的官方命名是 **TRUE（Truelight）体系** + Truelight CAM DRT（Baselight v7 进一步以公式化 DRT 支持了 ACES 2.0，见【二手】[Newsshooter 报道](https://www.newsshooter.com/2026/02/03/filmlight-baselight-v7-truelight-colour-space-improvements/)）。

**总结 FilmLight 的理由清单**：LUT 精度受限且削顶（clip）、一进一出无法复用、对越域色彩"无可奉告"、无法携带语义（需为每对源/目标各建一份）。这些结论与 Resolve 手册 1.2 节完全一致。

### 2.4 对"我们仍要支持 LUT"意味着什么

- LUT 在专业管线中的**合法生态位**是：viewing/监看变换、设备校准、look 的**交换/导出格式**、以及把"数学上可表示的调色子集"打包带走——而不是管线内部的表达方式。
- 因此 RawLab 内部应维持"**声明编码的变换链**"（工作空间 + 参数化操作 + DRT），LUT 只在入口翻译成内部链（含 shaper/域适配），在出口翻译成 .cube/CLF 导出。

---

## 3. ACES：为什么"风格"是 LMT 而不是裸 3D LUT

### 3.1 架构与 Output Transform

**【一手】[docs.acescentral.com — Output Transforms](https://docs.acescentral.com/system-components/output-transforms/)**：Output Transform 把 scene-referred 的 ACES2065-1 转为"a rendered state suitable for display on a specific output device"；ACES 1.1 起 "the RRT and ODT were **concatenated and designated an Output Transform (RRT+ODT)**"。ACES 2.0 的实现是**纯算法**：先转 Hellwig 2022 色貌模型简化的 **JMh** 中间量，tone mapping 只作用于 J（明度），chroma compression 只作用于 M（彩度），从而 "the **hue** of the original ACES value [is] maintained"；再由独立的 Display Encoding Transform 按显示设备的 primaries/白点/EOTF 编码。**整套 Output Transform 是一组有解析公式的算子，不是一张 LUT**。

### 3.2 LMT/Look Transform 的定义与约束

**【一手】[docs.acescentral.com — Look Transforms](https://docs.acescentral.com/system-components/look-transforms/)**：定义 "an **ACES-to-ACES transform** that systematically changes the appearance of ACES-encoded data"。要点：

- 输入输出都被钉死在 ACES2065-1（AP0 线性），因此 look 是**可组合、可移植、可交换**的：可以换成另一台相机照样工作；"Well-designed LMTs should be portable across applications and, where practical, **preserve ACES's high dynamic range and wide gamut**"。
- 复杂风格被鼓励做成系统化变换而非人工匹配："film print emulation ... is better **modeled in a systematic transform** than by requiring a colorist to match 'by eye'"。
- 一个 look "can be as simple as on-set **ASC CDL** values, can carry a **show LUT** as a base look"——注意即便用 show LUT，它也只是 LMT 内部的实现件，LMT 本身的边界（输入=AP0，输出=AP0）由体系保证。

**为什么是 LMT 而不是裸 3D LUT**：裸 3D LUT 没有"输入是什么编码"的语义，也没有动态范围保证（33³ 采样点之间的 HDR 行为靠外推）；而 LMT 把这两件事变成体系约束。落实在交换格式上，**【一手】[AMF 用户指南](https://docs.acescentral.com/amf/guides/user/)** 把 AMF 定义为"a '**color recipe**' that travels with your footage"，由 inputTransform / lookTransform / outputTransform 三段组成；其中 LUT 形态的 look "must apply in **AP0 linear**, so AMF requires **CLF** rather than cube LUTs"——CLF "works natively with linear color spaces like ACES"，支持扩展范围浮点并在文件头携带色彩空间信息。**官方直接否决了 .cube 作为 look 载体**。

### 3.3 ACEScct 的动机

**【一手】[docs.acescentral.com — ACEScct](https://docs.acescentral.com/encodings/acescct/)**：副标题即"A **Quasi-Logarithmic** Encoding of ACES Data for use within Color Grading Systems"。在 0.0078125 断点以下加线性 toe，"at **colorists' request** to better match 'traditional legacy log film scan encodings and camera log encoding functions'"，让 lift 类操作产生传统的阴影"milking/fogging"而非 ACEScc 的死黑。规范同时强调：ACEScct 值可以超 1 / 低于 0、**不应被钳位**（除非创作需要）、没有文件容器、**仅限软件内部使用，不得用于交换或存档**。这与 01 篇"log 是编码不是交换格式"互证。

### 3.4 aces-dev（→ aces-core）与 indie 可借鉴性

**【一手】[github.com/ampas/aces-dev](https://github.com/ampas/aces-dev)（ACES 2.0 起更名 aces-core）README**："This repository houses **core CTL library functions** ... including **Rendering algorithms used in ACES Output Transforms (e.g., tonescale, chroma compress, gamut compress)** ... This repository was named **aces-dev** in versions of ACES prior to the 2.0 release." 官方参考实现是**代码（CTL）而非 LUT 文件**，LUT 只是这些代码的下游产物之一（AMF→CLF）。

**对 indie 开发者**：ACES 全链路（IDT 参数表 → LMT → 公式化 DRT → AMF 清单）是可直接抄作业的"参考架构"；其许可证（Apache-2.0）允许吸收进商业应用。移动端完全不必实现全部 ACES，但"LMT 的边界思想 + AMF 的三段式清单 + 公式化 DRT"三层都可平移到我们自己的管线。

---

## 4. OpenColorIO（OCIO）：用配置管理"编码变换链"

### 4.1 配置结构：语义层在 config，不在 LUT 文件

**【一手】[OCIO 概念总览](https://opencolorio.readthedocs.io/en/latest/concepts/overview/overview.html)**："The meat of an OCIO::Config is a **list of named ColorSpaces**"；"ColorSpaces contain an **ordered list of transforms**, which define the conversion to and from the Config's '**reference**' space"；另有 **roles**（"abstract colorspace naming (e.g. specify the 'lnh' colorspace as the **scene_linear** role)"）、**displays/views**（显示设备与观看方式）、**looks**（"a color transform which **applies a creative look**"）。设计铁律："its only knowledge of color science comes from its execution of the transforms defined in the OCIO configuration file"，colorspace 名称 "**should NEVER be hard-coded into client software**"。应用只认角色和语义名，管线随配置（`$OCIO`）整体切换——这是"变换链管理"与"裸 LUT 文件"的本质区别：**语义、组合、优化都在系统层完成**。内部实现上，transforms 编译为算子（Lut1DOp、Lut3DOp、MtxOffsetOp、LogOp...），执行前 "Collapsed with appropriate neighbors"（优化器合并冗余步骤）。

### 4.2 算子集：LUT 只是众多算子之一

**【一手】[OCIO 配置编写指南](https://opencolorio.readthedocs.io/en/latest/guides/authoring/authoring.html)**：`FileTransform`（"Applies a lookup table (LUT)"，支持 .cube/.spi*/.clf/.cc 等）、`CDLTransform`（"Applies an **ASC CDL** compliant grade"）、`FixedFunctionTransform`（"one of a set of **fixed, special purpose, mathematical operators**"）、`BuiltinTransform`（内置权威变换，如 XYZ→sRGB）。两个对 LUT 的关键限制被明文写入：

- 3D LUT "**cannot be inverted**, so it is considered a '**display only**' colorspace"；配置里用 3D LUT 做 look 时必须**显式提供 inverse_transform**（1D LUT 可自动求逆）。
- **shaper LUT**：allocation/allocationvars "may still be used to automatically generate a '**shaper LUT**' when baking LUTs unless one is explicitly specified"，lg2 型 allocation（对数采样，如 `[-8, 8, 0.00390625]`）的意图是 "**maintain maximum fidelity and minimize clamping**"——把 HDR 数据烘进 3D LUT 时必须配 shaper，否则暗部/高光全毁。且 v2 起 "the **GPU renderer does not need to bake color transforms** and the allocation vars are not utilized unless an application requests the legacy GPU shader"。

### 4.3 OCIO v2 的 GPU 一致性机制（移动端 GLES 直接参考）

**【一手】[OCIO 2.0 Release Notes](https://opencolorio.readthedocs.io/en/latest/releases/ocio_2_0.html)**：

- v1 的问题被官方承认：旧 GPU 路径 "**did not fully match the CPU renderer and was not suitable for rendering final frames**"。
- v2 的解法："The **GPU renderer has been completely replaced** for OCIO v2 and now **computes pixels using the same operators as the CPU**"，"You should experience a **faithful match between CPU and GPU results**"。即：**逐算子生成 GLSL**，能精确求值的算子（矩阵、log、gamma、CDL、FixedFunction、ACES 算子——"new operators have been added to facilitate an **exact match to ACES Output Transforms**"）直接内联进 shader；只有无法解析求值的算子才烘成纹理采样（1D 纹理 + 3D 纹理），并辅以预/后缩放（shaper）保证采样域覆盖。
- **动态属性**："parameters that may be adjusted even after a transform has been converted into a Processor"——"On the GPU, these are **mapped to uniforms**"。曝光/对比度这类实时滑杆不重建管线，只更新 uniform。
- 可逆性："In OCIO v2, **all transforms may be inverted**"（配合 v2 的 DisplayViewTransform 反向取色等用法）。
- 灰区经验：CLF/精度讨论帖（**【二手】[ACES Central — Best of existing LUT formats](https://community.acescentral.com/t/best-of-existing-lut-formats/1703)**）记录了社区对 1D LUT 求逆精度、IndexMap、half 精度的实测担忧，Doug Walker（OCIO 主管）指出逆 1D LUT 评估 "is too slow for realtime processing"，Autodesk 因此转用 halfDomain 正向 LUT——**"LUT 求逆是性能陷阱"是可复用的工程结论**。

### 4.4 对本项目的启示

RawLab 的 GLES 渲染器可以直接采用 OCIO v2 的分层策略：**解析算子（矩阵/log/gamma/CDL）→ GLSL 内联求值；不可解析算子（用户 3D LUT）→ 纹理 + tetrahedral 采样；动态参数 → uniform**。CPU 端维护同构的 C++ 求值路径（用于导出、直方图分析、取色），两者共享同一套算子定义，从架构上保证所见即所得。这比"整条管线烘成一张大 LUT"在精度和灵活性上高一档。

---

## 5. 跨软件 LUT 一致性问题：原因清单

"同一个 .cube 在 Resolve / Lightroom / 剪映里效果不同"不是玄学，可拆成五个可验证的机制：

1. **输入编码假设未声明（根因）**。.cube 格式（IRIDAS 起源，Adobe《Cube LUT Specification 1.0》文档化，2013）只有 `TITLE / LUT_1D_SIZE / LUT_3D_SIZE / DOMAIN_MIN / DOMAIN_MAX` 等字段，**没有任何输入/输出色彩空间与传递函数字段**——这是格式层面的缺失（【一手】规范原文，经 [ACES Central 格式讨论帖](https://community.acescentral.com/t/best-of-existing-lut-formats/1703)与社区镜像确认；规范 PDF 原地址已失效）。所以"这个 LUT 吃 V-Log 还是 Rec.709"全靠文件名与口头约定——Panasonic 官方 LUT 的命名 `VLog_to_V709_forV35_ver100.cube`（**【一手】[Panasonic V-Log LUT 官方下载页](https://av.jpn.support.panasonic.com/support/global/cs/dconnect/lut/index.html)**；另有 [VariCam LUT Library](https://pro-av.panasonic.net/en/support/lut_library/) 35 颗 LUT，官方即区分"display conversion LUTs"与"artistic look LUTs"两类）就是把编码写进文件名的行业现状。FilmLight 的 V-Log 误用案例（2.2 节）就是编码假设错配的直接后果。
2. **域缩放与数据 levels**。LUT 的数值域（.cube 默认 [0,1]，或 DOMAIN_MIN/MAX 定义的子域）与图像的编码 levels（full 0-1 vs video 64-940/4-1019）不匹配时，输入先被错误缩放再进 LUT。Resolve 手册 Data Levels 一章明确："Whenever media using one data range is converted into another data range, each color component's minimum and maximum data levels are remapped"——而 LUT 文件本身不声明 levels，跨软件时各家的默认假设不同。【一手】Resolve 手册。
3. **插值方法差异**。trilinear vs tetrahedral 对 17³/33³ 表内格点的再现不同（Resolve 手册原文："differing lookup table interpolation methods cause **minor inconsistencies ... from application to application**"；Trilinear 的设计目标竟然是 "**matches the look of LUTs being applied in other applications**"）。【一手】Resolve 手册；CLF 帖（二手）补充了非均匀域 1D 表的精度争议。
4. **色彩管理叠加（双重变换）**。宿主若开了色彩管理（RCM 预设、LR 的显影管线、剪映自带的 HDR/SDR 处理），用户 LUT 会被叠加在自动变换之上，或反之用户以为加了 LUT 而宿主又"帮"做了一次归一化。Resolve 为此专门提供 per-clip "Bypass Color Management"（手册原文见 1.3 节）。Lightroom Classic 是极端案例：**不接受 LUT 直接应用**，LUT 只能转成 LutProfile（Lr 7.3 / ACR 10.3 引入，2018）在 Adobe 自家显影管线的特定阶段以"配置文件"身份生效，应用位置与 Resolve 的节点级完全不同（【二手】[转换教程](https://scottdavenportphoto.com/blog/how-to-convert-a-lut-to-a-lightroom-camera-profile)及社区讨论；同一讨论反复指出 LUT-as-profile 会 clip 数据而 LR 曲线不会）。剪映等消费级 NLE 的 LUT 管线无官方文档，属未验证区域，只可作对照组。
5. **钳位与精度**。Resolve 节点内 LUT "will clip any image data falling below 0 and above 1"（一手）；FilmLight："they **do not clip the signal**"说的正是公式相对 LUT 的优势、反衬 LUT 丢越界数据（一手）。再加上 8bit/16bit 整数管线与 FP32 管线的差异，同一 LUT 在不同位深宿主里会出现 banding 差异。

**对本项目的启示**：我们的 LUT 导入器必须要求（或推断后强制确认）**输入编码三元组：传递函数 × 色域 × 数据 levels**，并把它记录到工程元数据；LUT 应用点固定在"归一化到工作空间之后、DRT 之前"，宿主自身的色彩管理永远显式可见，避免叠加歧义；插值用 tetrahedral，位深全程 FP32/FP16。

---

## 6. 结论：行业共识的 LUT 使用守则

综合四家一手来源，"LUT 使用守则"高度收敛：

1. **声明编码**：LUT 必须携带（或由工程强制声明）输入/输出的传递函数、色域、数据 levels；做不到就拒绝载入。参照 AMF 的 inputTransform/lookTransform/outputTransform 三段式【一手 ACES】与 OCIO 的"colorspace 永不硬编码"【一手 OCIO】。
2. **限制用途**：LUT 适合 viewing/监看变换、设备校准、look 交换导出、胶片仿真等**固定映射**；不适合充当归一化第一步（FilmLight V-Log 案例）、不适合 HDR/宽色域数据的全管线表达（shaper 例外，见 5）、不适合承载参数化创作。
3. **shaper 曲线**：把 HDR/对数数据烘进 3D LUT 必须配对数域 shaper（OCIO lg2 allocation："maintain maximum fidelity and minimize clamping"【一手】）；消费 .cube（无 shaper 段）时要么先转换到其声明域，要么警告精度损失。
4. **避免链式叠 LUT**：两张 3D LUT 级联等于精度相乘、钳位相乘；Resolve 的挂载点模型（每步 1D 先于 3D）与 FilmLight"公式进 GPU"都指向：**链里的每一步要么是参数化算子，要么只有最后一个显示变换允许是 3D LUT**。
5. **内部用公式，边缘用 LUT**：Resolve 官方把矩阵数学的精度列为对 LUT 的优势；FilmLight 直接说公式 "do not clip the signal and are much more precise"；ACES 2.0 Output Transform 是纯算法；OCIO v2 GPU 与 CPU 跑同一套算子。四家一致：**LUT 是格式/接口，公式是本体**。
6. **插值与位深**：默认 tetrahedral + FP32；承认 trilinear 只为兼容旧观感。
7. **保留 scene-referred 原数据**：LUT 永远不写入"事实层"（RAW 显影的可逆参数），只进入"视图层"或导出渲染。

### LUT 边界表（LUT 适合做什么 / 不适合做什么）

| 维度 | LUT 适合 | LUT 不适合 | 依据 |
|---|---|---|---|
| 数据通路 | 监看/校准/viewing 变换（不落盘） | 交付数据的中间归一化（第一步 de-log） | Resolve 手册 Display LUT 语义；FilmLight V-Log 案例 |
| 表达能力 | 调色子集的固化导出（primaries/曲线可转 LUT） | Windows、限定器、模糊锐化、任何区域性操作 | Resolve Generate LUT "will be ignored" 条款 |
| 动态范围 | 配 shaper 后的静态风格预览 | HDR 全域管线变换（无 shaper 即毁暗部高光） | OCIO allocation/shaper 文档；ACEScct"不得钳位" |
| 色域映射 | 越域颜色的**截断式**映射（可接受的观感妥协） | 保色相/保细节的 gamut compression（需解析算法） | FilmLight"LUT 对越域色彩无可奉告"；ACES 2.0 RGC/输出变换均为算法 |
| 互换性 | 在"编码已声明"的双方之间搬运气（相机→监看、DI→外挂 LUT box） | 跨软件搬运"创作意图"（无元数据、插值/CM 叠加不可控） | .cube 规范无元数据；Resolve 跨软件插值差异原文；AMF 改用 CLF |
| 可逆性 | 1D LUT（通常可逆） | 3D LUT 求逆（OCIO："display only"） | OCIO authoring 文档；ACES CLF 帖 |

---

## 附：来源清单（一手/二手）

**一手**
1. Blackmagic Design《DaVinci Resolve 19.1 Reference Manual》(2025-02)：[PDF](https://documents.blackmagicdesign.com/UserManuals/DaVinci_Resolve_19_Reference_Manual.pdf)（§Project Settings–Lookup Tables、§Data Levels, Color Management, and ACES、§Resolve Live、Fusion CST 节点）
2. FilmLight《Truelight Primer》FL-TL-TN-0369 v1.1 (2010)：[PDF](https://www.filmlight.ltd.uk/pdf/whitepapers/FL-TL-TN-0369-Primer.pdf)
3. FilmLight《Colour in modern broadcast post-production》(2023)：[官方文章](https://www.filmlight.ltd.uk/store/news_articles/colour-in-modern-broadcast-post-production/)
4. FilmLight Truelight Colour Spaces 工作流页：[filmlight.ltd.uk/workflow/truelight.php](https://www.filmlight.ltd.uk/workflow/truelight.php)；Baselight 5.0 色彩管理视频（引述自官方页）：[Vimeo](https://vimeo.com/226904614)
5. ACES 文档：[Look Transforms](https://docs.acescentral.com/system-components/look-transforms/)、[Output Transforms](https://docs.acescentral.com/system-components/output-transforms/)、[ACEScct](https://docs.acescentral.com/encodings/acescct/)、[AMF User Guide](https://docs.acescentral.com/amf/guides/user/)、[文档索引](https://docs.acescentral.com/)
6. ACES 参考代码库 aces-dev（→ aces-core）：[github.com/ampas/aces-dev](https://github.com/ampas/aces-dev)
7. OpenColorIO：[概念总览](https://opencolorio.readthedocs.io/en/latest/concepts/overview/overview.html)、[配置编写指南](https://opencolorio.readthedocs.io/en/latest/guides/authoring/authoring.html)、[2.0 Release Notes](https://opencolorio.readthedocs.io/en/latest/releases/ocio_2_0.html)、[开发指南](https://opencolorio.readthedocs.io/en/latest/guides/developing/developing.html)
8. Adobe《Cube LUT Specification 1.0》(2013，IRIDAS 血统；原 Adobe 地址已失效，正文关键字段经多镜像与 ACES Central 帖核对)
9. Panasonic 官方 V-Log/V-Gamut LUT 页：[av.jpn.support.panasonic.com](https://av.jpn.support.panasonic.com/support/global/cs/dconnect/lut/index.html)；[VariCam LUT Library](https://pro-av.panasonic.net/en/support/lut_library/)

**二手**
10. ACES Central CLF 视觉工作小组帖 "Best of existing LUT formats"：[community.acescentral.com/t/1703](https://community.acescentral.com/t/best-of-existing-lut-formats/1703)（Nick Shaw / Haarm-Pieter Duiker / Doug Walker 讨论）
11. Newsshooter 对 Baselight v7 Truelight Colour Space 更新的报道（2026）：[newsshooter.com](https://www.newsshooter.com/2026/02/03/filmlight-baselight-v7-truelight-colour-space-improvements/)
12. Lightroom LutProfile 转换实践（Lr 7.3 / ACR 10.3 引入）：[scottdavenportphoto.com](https://scottdavenportphoto.com/blog/how-to-convert-a-lut-to-a-lightroom-camera-profile) 及 Lr 7.3 社区讨论

**未能核实/存疑**：检索未发现 FilmLight 有名为 "CHROMA" 的官方管线命名（用户侧常见称谓疑似与 Truelight/TRUE 体系混淆）；剪映等消费级应用的 LUT 处理细节无官方文档，未采信。
