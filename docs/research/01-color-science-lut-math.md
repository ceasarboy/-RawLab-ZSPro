# 色彩科学与 LUT 数学基础调研（传递函数 / 色域 / LUT / 插值 / Metamerism / 色域映射）

> 目的：为 RawLab（LibRaw + 自研 C++ 管线）实现松下 Lumix 式"用户载入 LUT"功能打理论基础。
> 写作约定：**【一手】**= 直接核对官方规范/白皮书/论文原文（或其官方文档站）；**【二手】**= 经可信第三方（开源实现文档、权威教材、厂商手册）转述确认。所有公式均经过数值锚点复核（如 18% 中灰映射到的 10-bit 码值）。
> 调研日期：2026-10-03。

---

## 1. 传递函数与编码（gamma vs. log）

**本质区别。** gamma 类传递函数（sRGB、BT.1886）是幂函数 `V = L^γ`，为**显示端/参考空间**设计，其压缩特性围绕感知对比度而非大动态范围记录；log 类编码（Cineon 血统）用 `V = a + b·log10(c·x + d)` 的形式把**场景线性光**（scene-referred，往往 14+ stop）压缩到 10-bit 码值范围内，按**曝光量级**（每 stop 大致对应固定码值增量）分配码值，接近负片胶片的密度特性。Poynton 对 gamma 的定义与动机是权威参考【一手教材，[Gamma FAQ](https://www.poynton.com/PDFs/GammaFAQ.pdf)】；ACES 文档把"log 型编码用于 scene-referred 中间态"作为体系设计明确表述【一手，[Overview of the ACES Encodings](https://docs.acescentral.com/encodings/overview/)】。

**主流 log 曲线数学形式与设计意图**（均为"对数主体 + 线性趾部"的两段/三段结构，x 为场景线性反射率，0.18 为中灰；码值为 10-bit legal range）：

| 曲线 | 主体（x ≥ 截断点） | 0 / 18% / 90% | 设计要点 |
|---|---|---|---|
| **S-Log3** | `CV = 420 + 261.5·log10((x+0.01)/0.19)`；x < 0.01125 时线性趾 `CV = 95 + (171.21−95)·x/0.01125` | 4 / 420 / 598 | 黑位以下留 foot（CV 4 仍在编码内），负信号不硬裁；曲线刻意贴近胶片 log，与 LogC3 形式几乎同构 |
| **V-Log**（归一化） | `y = 0.241514·log10(x+0.00873) + 0.598206`；x < 0.01 时 `y = 5.6x + 0.125` | 128 / 433 / 602 | 面向 14+ stop；黑位 128 以下留负值余量 |
| **C-Log2** | `y = 0.281863·log10(87.0994x+1) + 0.0353881`（full range） | — / ≈407 / — | 18% 放在 ~39.8%，高光余量最大（配合 2/3 英寸大动态机型） |
| **C-Log3** | 三段：阴影/高光为对称 log 段 + **中段线性** `y = 2.30698x + 0.07306` | — / ≈352 / — | 中灰与 C-Log 对齐（~34.3%），牺牲高光余量换更弱阴影噪点 |
| **F-Log** | `y = 0.344676·log10(0.555556x+0.009468) + 0.790453` | 95 / 470 / 705 | 与 ACES 流程兼容设计 |
| **F-Log2** | `y = 0.245281·log10(5.555556x+0.064829) + 0.384316` | 95 / 400 / 570 | 18% 下移到 400，为 X-H2S 15 stop 换更多高光余量 |
| **N-Log** | y < 0.328：`x = (650/1023)(y+0.0075)^{1/3}`（**立方根趾**）；否则 `x = (150/1023)·ln(y) + 619/1023` | — / ≈372 / — | 唯一用立方根做趾部的：阴影对比更平缓，兼顾 10-bit |
| **LogC3 (EI800)** | `t = 0.24719·log10(5.555556x+0.052272) + 0.385537`；x < 0.010591 线性趾 | ~92 / ~400 / ~571 | 曲线随 EI 变化（EI800 是基准），"Scene Exposure Factor" 输入约定 |
| **LogC4** | `E' = (log2(a·E+64) − 6)/14 · b + c`（a=(2¹⁸−16)/117.45 等，见规范）；负值走线性外推 `E = E'·s + t` | ~95 / ~400 | base-2、14 stop 映射到 [0,1] 整数域，负值段显式定义 |
| **ACEScct** | x ≤ 0.0078125：`y = 10.5402377·x + 0.07290553`（线性趾）；否则 `y = (log2(x)+9.72)/17.52` | — / 0.4135 / — | "准对数"，应调色师要求让 lift/shadow 行为与传统胶片 log 一致；**仅限内部使用，禁止存档/交换** |

来源（一手白皮书 + 实现复核）：S-Log3 见 Sony《Technical Summary for S-Gamut3.Cine/S-Log3 and S-Gamut3/S-Log3》（经由 [Sony 官方社区发布的白皮书](https://ap.community.sony.com)）；V-Log 见 Panasonic《VARICAM V-Log/V-Gamut》（2014）；C-Log 系列见 Canon《Canon Log Gamma Curves》白皮书（[downloads.canon.com](http://downloads.canon.com)）与 Thorpe 2012；F-Log/F-Log2 见 Fujifilm《F-Log Data Sheet Ver.1.1》《F-Log2 Data Sheet Ver.1.0》；N-Log 见 Nikon《N-Log Specification Document v1.0.0》（2018，[NPS 技术页](https://nps.nikonimaging.com/technical_info/technical_solutions/z7_z6_tips/n-log_specifications/)）；LogC3 见 ARRI《ALEXA – Log C Curve – Usage in VFX》（2012），LogC4 见 Cooper & Brendel《ARRI LogC4 Logarithmic Color Space Specification》（2022）；ACEScct 见 Academy S-2016-001（[docs.acescentral.com/encodings/acescct/](https://docs.acescentral.com/encodings/acescct/)）。
以上公式与锚点均经 colour-science（BSD-3）源码逐一复核【二手实现，[sony.py / panasonic_v_log.py / canon.py / fujifilm_f_log.py / arri.py / nikon_n_log.py](https://colour.readthedocs.io/en/develop/_modules/colour/models/rgb/transfer_functions/panasonic_v_log.html)】。
**来源间矛盾**：colour 文档标注 Panasonic Fig 2.2 表个别 12-bit 值与其计算相差 1 个码值（512/1732/2408 vs 512/1733/2409），属厂商表内舍入不一致，不影响结构。

**对本项目的启示**：所有相机 log 的共同骨架是 `log 主体 + 线性趾部 + 显式负值处理`。我们的管线应在 LUT 输入端用**官方逆函数**（不用近似拟合）把 log 解码到场景线性/工作空间，且解码器必须支持 0 以下输入（foot 段线性外推），否则相机 log 素材黑位以下会被截成平面。ACEScct"仅限内部"的告诫也适用于所有厂商 log：log 是编码，不是交换格式。

## 2. 色域与工作空间

- **显示/交付域**：Rec.709（[ITU-R BT.709](https://www.itu.int/rec/R-REC-BT.709)）、Rec.2020（[ITU-R BT.2020](https://www.itu.int/rec/R-REC-BT.2020)）、Display P3（DCI-P3 色度 + D65 白点 + sRGB 传递函数，Apple 定义【一手，[Apple 开发者文档](https://developer.apple.com/documentation/coregraphics/displayp3)】）。
- **相机厂商宽色域**（均为 D65 锚定、三原色定义的宽域，覆盖超出 Rec.2020 的可见域子集，色度坐标见各自白皮书）：S-Gamut3 / S-Gamut3.Cine（Sony 白皮书，同上；后者略收窄便于调色）、V-Gamut（Panasonic）、F-Gamut（Fujifilm）、ARRI Wide Gamut（ARRI LogC/AWG 白皮书）【一手】。
- **ACES**：AP0（ACES2065-1，含光谱轨迹外点，仅用于存档/交换）与 AP1（ACEScg/ACEScct 的工作域，≈略超 Rec.2020）——"AP0 用于 interchange，AP1 用于 working"为官方表述【一手，[Overview of the ACES Encodings](https://docs.acescentral.com/encodings/overview/)】。
- **为什么 LUT 必须声明输入/输出色域 + 传递函数**：一个 3D LUT 只是从三维输入到三维输出的**无语义数值映射**；同一个 LUT 作用于"V-Log 编码的 V-Gamut 值"和"LogC 编码的 AWG 值"会得到完全不同的颜色。行业对此的标准答案是把"色域 × 传递函数 × 观看变换"作为元数据随 LUT 流转：ACES 的 AMF（ACES Metadata File）就是为此设计的描述文件【一手，[AMF 用户指南](https://docs.acescentral.com/amf/guides/user/)】；OCIO 的 config 以 named transform + colorspaces 显式声明同一事实【一手，[OCIO Config syntax](https://opencolorio.readthedocs.io/en/latest/config_syntax.html)】。

**对本项目的启示**：LUT 载入 UI 必须强制用户选择（或从文件/AMF 读取）"LUT 期望的输入空间"与"目标输出空间"；管线内部应固定一个工作空间（建议 AP1 或我们自定义的 wide-RGB）+ 线性或 log 编码，LUT 前后用确定性色域矩阵/逆 OETF 做桥接。**绝不**把"LUT 名字"当作空间定义。

## 3. LUT 的数学表示

- **1D LUT**：每通道独立映射，无通道串扰；只能表达每通道 tone/白平衡类操作（`R' = f(R)` 等）。
- **3D LUT**：晶格点表 `L[i][j][k]`，输入空间被划分为 N³ 网格，格点间用插值（见第 4 节）。常见 N=17/33/65。
- **.cube 格式**：原始规范是 IRIDAS（后为 Adobe SpeedGrade）的 *Cube LUT Specification 1.0*（作者 Patrick Palmer 等）；Adobe 原始 PDF 链接已随 SpeedGrade 停更下架（[Adobe 社区确认](https://community.adobe.com)），规范要点经下游实现确认【一手规范已下架 / 二手转述】：关键字 `TITLE`、`LUT_1D_SIZE`、`LUT_3D_SIZE`、`DOMAIN_MIN/DOMAIN_MAX`；`LUT_3D_SIZE` 取 2–65（[Eizo ColorNavigator 7 手册](https://www.eizoglobal.com)）；数据按 R 变化最快（蓝变化最慢）排布（[Codex LUT 格式指南](https://help.codex.online)）。OpenColorIO 把 .cube/.csp/.3dl 作为一等文件格式支持【一手，[OCIO 文档](https://opencolorio.readthedocs.io/en/latest/config_syntax.html)】。
- **Shaper（1D 预整形）+ 3D 晶格**：动机是动态范围分配。log/线性数据的值域极不均匀——若直接把场景线性值均匀装入 33³ 晶格，阴影（信息最密集处）每个格点间隔会吞掉数 stop，高光却大量浪费格点。工程做法是先用一条 1D LUT（通常是 log 型 shaper）把输入压到 [0,1] 再查 3D 表，输出侧再反 shaper。OCIO 在 GPU 渲染路径中会自动生成 shaper LUT 正是此原因【一手，[OCIO Config syntax（shaper LUT 一节）](https://opencolorio.readthedocs.io/en/latest/config_syntax.html)】；ACES 的 CLF（Common LUT Format）把 1D/1D+3D/range 等组合形式标准化【一手，[CLF Specification](https://docs.acescentral.com/clf/specification/)】。
- **HaldCLUT**：Eskil Steenberg 提出（"Hald"即其 ID）：把整个颜色立方体按确定性图案排进一张 2D 图，恒等图（identity）经任何照片编辑器处理后即得到该编辑的 LUT 图像；level n 的图边长为 n³（level 8 → 512×512），实现至少需支持 level 16【一手，[quelsolaar.com/technology/clut.html](http://www.quelsolaar.com/technology/clut.html)】。FFmpeg 提供 `haldclut`/`haldclutsrc` 滤镜【一手，[FFmpeg filters](https://ffmpeg.org/ffmpeg-filters.html)】。优点：可以直接用 Photoshop/Lightroom 的完整编辑链"离线烘焙"LUT；缺点：等价于一次 3D LUT 查表 + 图像级解析度限制。
- **尺寸与量化误差**：17³=4913、33³=35937、65³=274625 个格点。误差来源有二：晶格密度（格点间插值误差随 N 增大以约 1/N² 量级下降，教材级分析见 Kang《Computational Color Technology》SPIE Press、Poynton《Digital Video and HD》【一手教材】）与格点本身的位深（8-bit 格点在渐变上直接引入 banding）。ACES 2.0 放弃 LUT 化 Output Transform 的官方理由之一即"LUT 表示精度不足"（见第 6 节）。

**对本项目的启示**：Lumix 式载入 LUT 至少支持 17/33/65 的 .cube（含 1D shaper + 3D 组合的 .cube）与 HaldCLUT；格点存储用 half-float 或 16-bit，避免 8-bit 格点；管线内部建议固定"linear → 1D log shaper → 3D LUT → shaper 逆 → linear"的通用骨架，使任意 log 输入都能以均匀精度进入晶格。

## 4. 插值方法

- **四面体插值的经典出处**：Kasson, Plouffe & Nin，《A Tetrahedral Interpolation Technique for Color Space Conversion》，SPIE *Device-Independent Color Imaging*（Proc. SPIE 1670, 1992），后有 IBM J. Res. Dev. 版本（被引 79+）【一手论文，[IBM Research 页](https://research.ibm.com)、[Jim Kasson 博客索引](https://blog.kasson.com)】。
- **精度差异**：trilinear 用包围立方体 8 个角点加权，会在饱和色区域把颜色向立方体中心（灰轴）方向"稀释"，产生可测的色相偏移与饱和度损失；tetrahedral 只用包含输入点的那个四面体的 4 个顶点，邻域其它色相不参与混合，色相/饱和度保持显著更好，且在 17³/33³ 低分辨率下差异最明显，65³ 以上两者趋同【一手教材：Kang《Computational Color Technology》SPIE Press 的数值对比；二手工程确认：[darktable 手册（3D LUT 模块，tetrahedral 为默认）](https://docs.darktable.org/usermanual/4.2/en/)】。
- **工程实践的选择**：OpenColorIO 规定 3D LUT 的 `INTERP_BEST` 解析为 **TETRAHEDRAL**【一手，[OCIO 文档](https://opencolorio.readthedocs.io)】；FFmpeg `lut3d` 滤镜提供 `interp=nearest|trilinear|tetrahedral` 选项【一手，[FFmpeg filters](https://ffmpeg.org/ffmpeg-filters.html)】；调色硬件厂商（FSI 监视器）与 Resolve 社区亦以 tetrahedral 为高精度选项宣传/偏好（二手转述）。
- **外推 / 超出 domain 的输入**：.cube 用 `DOMAIN_MIN/DOMAIN_MAX` 声明输入域，规范默认 0..1；**domain 外的输入未定义**，业界通行做法是把输入 clamp 到 domain（Eizo 手册明确要求 DOMAIN_MIN=0 0 0 才受支持；FFmpeg 亦 clamp）【二手转述】。跨 domain 外推不是插值能解决的问题——正确做法是在 LUT 前置 tone/gamut 预映射。

**对本项目的启示**：3D LUT 查表直接实现 tetrahedral（算法量小、分支可预测，适合 NEON）；1D LUT 用线性插值即可；输入端必须 clamp 到声明的 DOMAIN 并把"越界比例"暴露到 UI，避免用户误以为高光细节被 LUT 忠实处理。

## 5. 同色异谱（Metamerism）：LUT 与相机解耦的物理边界

- **定义**：CIE ILV 把 metamerism 定义为"一对光谱不同的刺激在同一观察者/照明条件下产生相同三刺激值的性质"【一手定义，[CIE e-ILV](https://eilv.cie.co.at)；转述见 [SDC 文章](https://sdc.org.uk)】。
- **相机为何"看到"的颜色不同**：Luther 条件指出，相机要完美复现色度，其三个光谱响应必须是人眼 CIE CMF 的线性组合；真实 CFA/滤光片/IR-cut 的光谱响应违反该条件，且**各家传感器响应差异巨大**——厂商白皮书均附实测光谱响应曲线（Sony、Panasonic、ARRI 白皮书同上）【一手】。综述见 *Spectral color characterization of digital cameras: a review*（[ADS 索引](https://ui.adsabs.harvard.edu)）；测量与表征方法标准化于 ISO 17321-1（数码相机色彩表征）【一手标准】。由此产生"相机同色异谱"：两个在 CIE 色度上不同的物体色块，可能在 A 相机上读数相同、在 B 相机上不同。
- **对 LUT 的推论**：一个 LUT 是"某相机响应空间 → 某显示空间"的映射。同一 LUT 直接套在不同底色的相机素材上，等于把不同的输入光谱学映射到同一目标，结果必然不一致——这不是 LUT 写得不好，而是物理边界。ACES 的解法是 **Input Transform 按相机系统逐一定义**（"converts … from a given camera system to ACES RGB relative exposure values"），先把素材归一到共同的相对曝光空间，之后的 LUT 才是良定义且可复用的【一手，[ACES Input Transforms](https://docs.acescentral.com/system-components/input-transforms/)】。
- 另一相关现象是**观察者同色异谱**（同一组三刺激值在个体视觉差异下失配，CIE 有专门的 special metamerism index: change in observer 发布【一手，[CIE 170-2 / e-ILV](https://cie.co.at/publications)】），主要影响校色监视器而非 LUT 本身，本文仅备注。

**对本项目的启示**："LUT 与相机解耦"只能在"先经过相机特定 colorimetric 变换（我们的 LibRaw 相机矩阵 + DCP/厂商矩阵 + 逆 log）进入统一工作空间"**之后**成立。产品语义应当是：用户 LUT 挂在统一工作空间之后；同时提供按机型的标准 Input Transform（松下 V-Log/V-Gamut→工作空间等），并在文档中说明跨机型不一致的原因。

## 6. 色彩外观与色域映射（简）

- **一句话级概念**：CIECAM02/CAM16 是色貌模型（考虑适应与观察条件）；**JzAzBz** 是面向 HDR/WCG 的感知均匀色空间（Safdar, Cui, Kim & Luo, *Optics Express* 25(13):15131, 2017，[DOI 10.1364/OE.25.015131](https://doi.org/10.1364/OE.25.015131)【一手论文】），其衍生 ΔE ITP 被 ITU-R BT.2124 采用；**ICtCp** 由 Dolby 提出并进入 ITU-R BT.2100，I 通道优化恒定亮度、改善色度/饱和度线性（[Dolby ICtCp 页](https://professional.dolby.com)、[ITU-R BT.2100](https://www.itu.int/rec/R-REC-BT.2100)【一手】）。
- **"LUT 不能做色域压缩"的共识与原因**：色域压缩需要在饱和/高光区做**方向感知、随亮度与色度连续变化的非线性映射**（典型做法在 hue-linear 的外观空间中做 chroma/gamut compression）；固定晶格 LUT 只能离散采样该映射，插值会在晶格间引入色相偏移，且 LUT 分辨率有限导致压缩曲线在渐变处出现带状/裁切感。ACES 的工程决策印证此点：ACES 1.x 反馈集中就有"高饱和光源伪影""需修复负值问题"等抱怨【一手，[ACES 1 Feedback](https://docs.acescentral.com/background/v1-feedback/)】；ACES 2.0 将 Output Transform 改为**全参数化代码**（tone scale + chroma compression + gamut compression 统一算法），官方理由是参数化表示"更精确、无需 3D LUT、精度更高"，且消除了旧版 gamut 边界处的裁切感伪影【一手，[About ACES 2 / About Rendering](https://docs.acescentral.com/background/about-aces-2/)、[V1 Feedback](https://docs.acescentral.com/background/v1-feedback/)】；其 Reference Gamut Compression 也是参数化工具而非 LUT【一手，[RGC](https://docs.acescentral.com/rgc/overview/)】。
- 但要注意**分寸**：LUT 不适合承载色域压缩，**不等于** LUT 没用——把"归一化后的外观变换（look）"烘焙成 LUT 正是调色行业的常规用法，前提是输入已被归一且映射本身是空间无关的纯查表语义。

**对本项目的启示**：用户 LUT 只应承担"风格映射"（空间已归一）；色域压缩/高光滚降由管线内置的参数化模块（可参考 ACES 2.0 的 chroma/gamut compression 结构，或简单分段 hue-preserving gamut clip）承担，且位于 LUT 之前。这同时规避了"LUT 高光裁切"类投诉。

---

## 附：主要来源清单（按节）

1. Poynton, *Gamma FAQ* — https://www.poynton.com/PDFs/GammaFAQ.pdf
2. ACES Docs（Encodings / ACEScct / Input Transforms / V1 Feedback / About ACES 2 / RGC / CLF / AMF）— https://docs.acescentral.com/
3. Sony《S-Gamut3.Cine/S-Log3 White Paper & Technical Summary》— https://ap.community.sony.com
4. Panasonic《VARICAM V-Log/V-Gamut》(2014)；Canon《Canon Log Gamma Curves》；Fujifilm《F-Log / F-Log2 Data Sheets》；Nikon《N-Log Specification v1.0.0》— https://nps.nikonimaging.com/technical_info/technical_solutions/z7_z6_tips/n-log_specifications/
5. ARRI《ALEXA – Log C Curve – Usage in VFX》(2012)、《ARRI LogC4 Specification》(2022) — https://www.arri.com/en/camera-systems/learn-help/technical-downloads
6. colour-science 传递函数实现与文档（对厂商常数的复核）— https://colour.readthedocs.io/en/develop/_modules/colour/models/rgb/transfer_functions/panasonic_v_log.html
7. Adobe/IRIDAS《Cube LUT Specification 1.0》（原文已下架，要点经 Eizo/Codex/OCIO 转述）— https://community.adobe.com
8. Kasson, Plouffe & Nin (1992), tetrahedral interpolation — https://research.ibm.com
9. OCIO 文档（config syntax、shaper、INTERP_BEST=TETRAHEDRAL）— https://opencolorio.readthedocs.io
10. FFmpeg filters（lut3d / haldclut）— https://ffmpeg.org/ffmpeg-filters.html
11. Eskil Steenberg, Hald CLUT — http://www.quelsolaar.com/technology/clut.html
12. Safdar et al. (2017), JzAzBz, Opt. Express 25(13):15131 — https://doi.org/10.1364/OE.25.015131
13. Dolby ICtCp / ITU-R BT.2100 — https://professional.dolby.com / https://www.itu.int/rec/R-REC-BT.2100
14. CIE e-ILV（metamerism 词条）— https://eilv.cie.co.at；ISO 17321-1（相机色彩表征）
