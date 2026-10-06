# Panasonic Lumix Lab / LUMIX LUT 工作流深度调研

> 调研日期：2026-10-04。本文只做研究与开发门禁，不修改实现代码。证据分为【一手官方】、【一手 APK/逆向实测】、【二手/通用理论】；APK 结论基于用户提供的 `Panasonic LUMIX Lab_3.0.1.apk` 解包目录和 JADX/dexdump 结果，不能等同于 Panasonic 公布的算法规格。

## 0. 结论先行

当前最强结论不是“STD_to_VLOG_2nd 不能逆”，而是“RawLab 把一个相机照片风格底座到 V-Log 的内部合成链当成了可逐点反求的普通颜色变换，并且把求逆失败后的不连续结果写回成一张 look LUT”。`STD_to_VLOG_2nd.cube` 的中心差分 Jacobian 在内部样本中仍为正、最小奇异值约 0.1043、最大条件数约 6.24；因此在 33³ 格点上没有证据表明它本身出现全局不可逆或多对一折叠。真正危险的是：它的灰轴并不是 RawLab `neutralDisplayDecode -> V-Log encode` 所期望的灰轴，末端和暗部偏差很大；同时大量单轴输出导数为负，说明它不是可用“独立通道单调曲线”理解的变换，而是有显著 channel cross-talk / hue-volume warp。

RawLab 适配器的数学契约是：对每个中性显示目标 `v`，构造 `target = VLogEncode(neutralDisplayDecode(v))`，然后数值求 `STD_to_VLOG_2nd^{-1}(target)`；再对解做 hard clamp、邻居平滑、按 residual 淡出 look。代码中 `clamp01(map.apply(v))` 把可达域外的目标投影成了逐点的 clamp 输出，且 Newton/LM 解在每个网格点独立进行。即使增加了 coarse seed、跳变平滑和 confidence fade，这也不是严格的连续可达域投影；平坦白墙上的微小 RGB 噪声可能被映射到不同的解支、不同的局部色相，最终表现为彩色 speckle/banding。

APK 证据显示，Lumix Lab 不是简单“把用户 CUBE 直接作用于一张已变成 STD 的照片”。它有三条不同路径：

1. 普通编辑用户 LUT：解析 `.cube`，要求 `LUT_3D_SIZE` 为 33 或 17；将 `#LUMIXPHOTOSTYLE` 写入/规范化并记录为 LUT 的 `photoStyle` metadata；在 GPU LUT filter 中把 RGB 输入按 0..1 查 33³/17³ LUT，支持 intensity 混合。APK 中未见普通用户 CUBE 依据 `STD/CINEV2/VLOG` 自动调用 `models/mergeLut`；底座标签主要是元数据、相机传输和 LUT 管理语义。
2. Panasonic RAW/照片风格链：`photoStyleLUT/<camera>/lut_<style>.bin` 的 low/high/base 三套二进制资产交给 `techorfilter.ImageFilter.nativeCnv2/nativeCnv2RGB`；另有 `identity_33.cube`（3D identity）与 `identity_33_tone.tone`（1D identity tone）作为默认/编辑状态。`setPhotoStyleLUT(...)` 明确传入三组 photo-style LUT 数据与尺寸，`getUseToneL(...)` 单独生成/应用 L tone 1D 数据，并通过 `validToneL/validToneRGB`、`cubeFact/cubeFact2` 及两 LUT intensity 参与 native pipeline。
3. 自动从图片估计 LUT：`PI_3D_LUT_v1.0.0_model_B_fp16.ort` 推理输出 33³ CUBE；若目标 photo style 不是 VLOG，`C6780w.m9872a` 从 `models/mergeLut/<style>_to_VLOG*.cube` 拷贝底座转换表，然后调用 native `mergeCubeFiles(base, user, output)` 合并，再写 `#LUMIXPHOTOSTYLE <style>`。这说明“底座转换/合并”确实存在，但证据对应的是自动 LUT estimation 与 VLOG 目标链，不能直接证明用户手动导入 LUT 一定会同样合并。

因此优先级为：先在 Windows 宿主复现/对齐官方链（A），同时建立可达域和连续 chroma compression 诊断（C/B）；暂时禁止继续调 Newton 迭代、增大 LUT 尺寸或仅靠 independent RGB clamp 修补。只有当 A 的官方链对齐和 CUBE/PNG 误差、灰轴、平坦场噪声测试通过后，才决定是否需要把 A 烘焙为用户可见 LUT。

---

## 1. 证据范围与文件索引

### 1.1 一手官方来源

- Panasonic Digital Camera support 的官方 LUT 页：<https://av.jpn.support.panasonic.com/support/global/cs/dsc/download/lut/index.html>。页面明确给出下载文件 `VLog_to_V709_forV35_EN.zip`，并说明使用 LUT 需要 compliant software/playback device；相机安装时使用 `.vlt`、文件名和存储卡规则。它证明的是 Panasonic 面向 LUMIX 相机的 V-Log→V709 技术 LUT 下载与机身安装约束，不证明任意 photo-style 用户 CUBE 的自动底座转换。
- Panasonic VARICAM LUT Library：<https://pro-av.panasonic.net/en/cinema_camera_varicam_eva/support/lut/index.html>。官方说明 V-Log/V-Gamut 用 log 保存超过 14 stops 与宽色谱；Conversion LUT 负责 V-Log 到较窄显示动态范围并转换到 Rec.709 等 monitor space，属于 technical LUT；Artistic Look LUT 将 technical conversion 与 gamma/contrast/saturation/bias 等 creative changes 合并。官方还明确区分 `.VLT`（VariCam camera）、`E-E.CUBE`（post color grading）和 `E-L.CUBE`（external LUT box），混用会得到错误图像。该页可作为“技术 transform 与 look 的语义分层”的一手依据，但 VariCam 页面不等同于 LUMIX Lab 私有 RAW editor 的完整实现。
- Panasonic 页面未公开 `STD_to_VLOG_2nd.cube` 的公式、逆算法、merge 细节或 LUMIX Lab 3.0.1 源码；以下 APK 证据须明确标为逆向观察。

### 1.2 APK 一手逆向证据

用户提供：`F:/rawlab/lumix-lab-re/Panasonic LUMIX Lab_3.0.1.apk`，解包：`F:/rawlab/lumix-lab-re/apk-extracted`。为获得可读代码，使用 JADX 输出到 `F:/rawlab/jadx-out`；dex 原始反汇编保留于 `F:/rawlab/classes-dexdump.txt`、`F:/rawlab/classes2-dexdump.txt`。关键类/函数：

- `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/util/C6780w.java`：ONNX 自动 LUT 生成、base→VLOG merge、CUBE 写入及 `#LUMIXPHOTOSTYLE` 注释。
- `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/util/C6750v.java`：style 到 merge LUT 的映射。
- `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/controller/fragment/LutFragment.java`：用户 CUBE 选择、33/17 检查、photo-style metadata 注入。
- `F:/rawlab/jadx-out/sources/p028C5/RunnableC0607F8.java`：导入 CUBE 的存储、metadata 和 LUT PhotoStyle 记录。
- `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/util/C6747u.java`：CUBE/tone parser；读取 `#LUMIXPHOTOSTYLE`、`#GRAINEFFECT`、`#COLORNOISE`、`#SHARPNESS`、`#NOISEREDUCTION` 和 `LUT_3D_SIZE`。
- `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/widget/GlLUTFilter.java`、`GPUImageLUTFilter.java`、`GPUImageLUTFilter17.java`：GPU 3D LUT 查表和 intensity blend shader。
- `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/util/C6749u1.java`、`F:/rawlab/jadx-out/sources/com/techor/techorfilter/ImageFilter.java`：RAW native pipeline 参数、独立 tone LUT、photo-style LUT 和两 LUT intensity。
- `F:/rawlab/lumix-lab-re/apk-extracted/lib/arm64-v8a/libllc.so` 导出 `Java_..._convertToCube`、`Java_..._mergeCubeFiles`；`libcube_merge.so` 导出 `cube_merge_files`，并带字符串 `read_cube_body`、`LUT_3D_SIZE %d`、`./cube_merge base.cube user.cube merge.cube`。

---

## 2. APK 资产完整分类与语义

### 2.1 资产总览

`assets` 顶层统计（用户 APK）：

| 组 | 数量/规模 | 语义判断 |
|---|---:|---|
| `LUTFile` | 5 个，约 2.99 MB | parser/编辑器默认的 identity 3D LUT、identity tone、gamma-corrected helper、示例 film CUBE |
| `models/mergeLut` | 13 个 33³ CUBE | 自动 LUT generator 的底座到 VLOG technical/merge transform |
| `photoStyleLUT` | 8 个相机目录、426 个 BIN、约 61.9 MB | 相机型号特定的内置 Photo Style LUT 资产；每 style 有 base、`_high`、`_low` 变体 |
| 顶层 `*.cube` | 5 个 | 预安装/下载的 look LUT；头部有 `#LUMIXPHOTOSTYLE STD/NAT/CNEV2` |
| `*.tone` | 2 个 | 独立 1D tone curve 数据，不是 3D look |
| `PI_3D_LUT...ort` | 58.2 MB | 图片到 33³ LUT 的 ONNX 估计模型，不是 LUT 本身 |
| `33cube.png` | 499 B | CUBE preview/identity 贴图资源 |

### 2.2 `identity_33.cube` 与 `identity_33_tone.tone`

路径：

- `F:/rawlab/lumix-lab-re/apk-extracted/assets/LUTFile/identity_33.cube`
- `F:/rawlab/lumix-lab-re/apk-extracted/assets/LUTFile/identity_33_tone.tone`

`identity_33.cube` 的头为 `# Identity .cube file`、`LUT_3D_SIZE 33`，数据按 R-fast 顺序从 `(0,0,0)` 开始 `0,0,0`、`0.03125,0,0`，最终为单位立方体。它是**3D identity placeholder / neutral 3D stage**，不是照片风格 look，也不是 STD→VLOG transform。

`identity_33_tone.tone` 的头为 `# Identity .tone file`、`LUT_SIZE 33`，值从 0 到 1、步长 1/32，是**独立 1D tone identity**。APK `C6749u1.m9794r()` 在默认/重置时把两个 identity CUBE 和 identity tone 装入状态；`ImageFilter.getUseToneL` 的签名单独接收 1D tone size/data，证实 tone 与 3D CUBE 是分层的。

### 2.3 `gamma_corrected_33.cube` 与 `gamma_corrected_33_tone.tone`

路径：

- `F:/rawlab/lumix-lab-re/apk-extracted/assets/LUTFile/gamma_corrected_33.cube`
- `F:/rawlab/lumix-lab-re/apk-extracted/assets/LUTFile/gamma_corrected_33_tone.tone`

两者头部分别为 `# Gamma corrected .cube file`/`# Gamma corrected .tone file`。tone 数据为：0、0.206938、0.283578、…、1.0；CUBE 的 R 轴同样重复该 1D curve，G/B 保持 identity。这是**预计算 gamma/tone helper**：把单通道 gamma curve 作为 1D tone 或等效 3D diagonal LUT 表达，目的是给 UI/native filter 使用。它不是 creative photo-style look，不是 style→VLOG transform，也不是可据此推断的通用 sRGB gamma 定义。代码搜索显示运行时主 RAW 初始化直接引用 `identity_33.cube` 和 `identity_33_tone.tone`，未见 Java 直接引用 `gamma_corrected_33.*`；因此对其运行时使用应写成“资产存在、语义由文件名/形状支持；当前 JADX 证据未定位到主路径调用”。

### 2.4 `models/mergeLut/*_to_VLOG*.cube`

路径：`F:/rawlab/lumix-lab-re/apk-extracted/assets/models/mergeLut/`，13 个 33³ CUBE：

`709LIKE_to_VLOG_2nd.cube`、`CINEA2_to_VLOG_2nd.cube`、`CINED2_to_VLOG_2nd.cube`、`CINEV2_to_VLOG_2nd.cube`、`FLAT_to_VLOG_2nd.cube`、`LAND_to_VLOG_2nd.cube`、`LCLAS_to_VLOG.cube`、`LCLASG_to_VLOG.cube`、`LCNEO_to_VLOG_2nd.cube`、`NAT_to_VLOG_2nd.cube`、`PORT_to_VLOG_2nd.cube`、`STD_to_VLOG_2nd.cube`、`VIVID_to_VLOG_2nd.cube`。

CUBE 文件头如 `# STD_to_VLOG_16jiku_250725.cube`、`LUT_3D_SIZE 33`，没有 `#Gamma/#Gamut` 公共 metadata。`C6750v` 的 Java map 明确把 `STD` 映射到 `STD_to_VLOG_2nd.cube`，并为 NAT/PORT/VIVD/LCLASN/LAND/FLAT/CNEV2/CNED2/CNEA2/709L/LCLAS/LCLASG 指定对应表。它们的语义应分成两层：

- **技术 transform / merge base**：把某个 Panasonic Photo Style 的显示/照片风格域送到 VLOG 目标域，供自动 LUT estimation 输出链合并；名称和调用位置支持这一点。
- **不是用户最终 look**：它们本身不是 Leica/Fuji 等创意 look；也不是“STD 照片风格本身”。

重要边界：C6780w 对 `str != VLOG` 才取 map，复制 `models/mergeLut/<style_to_vlog>`，然后 `mergeCubeFiles(base, generated_user_lut, output)`；`VLOG` 则直接采用 ONNX 输出。这证明 APK 中有底座变换链，但只对自动 LUT 生成路径直接可证。手动导入路径 `LutFragment.onActivityResult -> m9294Q0 -> RunnableC0607F8` 只解析、注入 `#LUMIXPHOTOSTYLE`、保存 CUBE 和 style ID，未见同一 `mergeCubeFiles` 调用。

### 2.5 顶层 `*.cube` 与 `#LUMIXPHOTOSTYLE`

`pre_installed_lut_list.json` 明确列出：

- `Teal Flat-S.cube`：`photoStyle: STD`
- `Smoky Color-S.cube`：`photoStyle: STD`
- `Clear-S.cube`：`photoStyle: STD`
- `Backlight-N.cube`：`photoStyle: NAT`
- `Filmlike-V2.cube`：`photoStyle: CNEV2`

这些文件头的 `#LUMIXPHOTOSTYLE STD/NAT/CNEV2` 是输入底座 metadata，表示该 look 预期在相应 Panasonic Photo Style/照片风格基底上使用。它们是**photo-style look**，不是技术 transform。APK 导入时会把同名/规范化 style 写入 CUBE 注释和数据库 `LutPhotoStyleId`；没有标签时 style 可为空/无底座，而不是自动证明输入域。

`LutFragment.m9276q0` 还解析/替换 `#GRAINEFFECT`、`#COLORNOISE`、`#SHARPNESS`、`#NOISEREDUCTION` 为内部 level；这些是伴随 LUT 的额外效果 metadata，不等于 LUT 数学中的颜色 transform。`RunnableC0607F8` 把 `photoStyle`、path、name、四种 extra effect 存入 `LutInfoEntity`。

### 2.6 `photoStyleLUT/<camera>/lut_*.bin`

路径示例：`F:/rawlab/lumix-lab-re/apk-extracted/assets/photoStyleLUT/G9M2/`；相机目录由 `C6725m1.java` 映射：S5M2、G9M2、GH7、S9、S1M2、S1M2E、S1RM2、L10。每个相机有 `lut_standard.bin`、`lut_vivid.bin`、`lut_natural.bin`、`lut_flat.bin`、`lut_cinelike_v2.bin`、`lut_vlog.bin` 等风格，部分相机另有 HLG/Cinelike A2/L.Classic/Gold 等。

每种 style 通常有：

- `lut_<style>.bin`：4184 B，PSL1 头，低/基础版本；
- `lut_<style>_high.bin`、`lut_<style>_low.bin`：215710 B，PSL1 头，高/低变体。

这些不是公开 `.cube`，也不能按普通 33³ 文本表直接解释。`C6749u1.m9777P()` 根据 camera ID 和 PhotoStyle ID 构造 base/high/low 三个资源路径，`C6652I0` 的 BIN parser 读取二进制 header/data；随后 `ImageFilter.setPhotoStyleLUT(styleId, size/data, size/data, size/data)` 传入 native SDK。它们是**相机型号特定的 Photo Style technical/rendering assets**，包含 Panasonic 原生风格与传感器/相机 pipeline 语义；从资产名不能把 `lut_standard.bin` 等当作用户 look CUBE。

### 2.7 资产分类总表

| 资产 | 输入域 | 输出域 | 应用顺序/用途 | look 还是 technical |
|---|---|---|---|---|
| `identity_33.cube` | 33³ normalized RGB | 同域 | native pipeline 默认 3D stage | technical placeholder |
| `identity_33_tone.tone` | 1D tone code | 同域 | tone stage 默认 | technical placeholder |
| `gamma_corrected_33.*` | 1D/diagonal RGB | gamma-corrected code | tone/gamma helper；主调用未定位 | technical helper |
| `STD_to_VLOG_2nd.cube` 等 | Panasonic style domain | VLOG target domain | 自动 LUT estimation 前的 base merge | technical transform |
| `#LUMIXPHOTOSTYLE STD` 顶层 CUBE | STD style output/base assumption | look output | 用户 LUT 查表，叠加强度 | photo-style look |
| `photoStyleLUT/<camera>/*.bin` | camera-specific raw/photo-style processing input | camera style pipeline output | native `ImageFilter`；三套 low/high/base | technical/rendering asset |
| ONNX `PI_3D_LUT...ort` | 224×224 RGB image | 33³ LUT data | 自动估计 look | look estimator/model |

---

## 3. Lumix Lab 用户 LUT 导入、底座转换与应用顺序

### 3.1 手动导入的确定事实

`LutFragment` 用 `GET_CONTENT`，MIME 为 `application/octet-stream`/`application/zip`，结果 request code 200。对单个 CUBE，`onActivityResult` 复制到 app files；`m9294Q0` 调 `C6747u.m9653N` 解析。parser：

- 识别 `#LUMIXPHOTOSTYLE`，规范化别名，如 `709LIKE -> 709L`、`CINE V2 -> CNEV2`、`CINE D2 -> CNED2`；
- 读取 `LUT_3D_SIZE`；只接受 33/17 结果用于 UI import；
- 建立 `LutInfoEntity`，保存 `LutPhotoStyleId`、path、name、extra metadata；
- `RunnableC0607F8` 把前 100 行和 `#LUMIXPHOTOSTYLE` 重新写入 `lut_33/<name>_<timestamp>.cube`，不做颜色数值变换；
- CUBE preview 先被解析为 33²×33 的 Bitmap，GPU filter 再查表。

因此，**对用户手动导入的普通 CUBE，APK 直接可证的是“直接查 LUT + intensity blend”，不是“导入时一定自动改写为 V-Log 或统一底座”。** `photoStyle` label 会被保留，供相机传输、编辑状态和效果显示；是否在相机传输/特定 camera RAW SDK path 发生额外约束，应以该路径进一步动态抓包或实机 A/B 验证，不能凭标签臆断。

### 3.2 GPU 查表和量化

`GlLUTFilter` / `GPUImageLUTFilter` 的 shader：

- 输入 `textureColor.rgb` 乘 32（33³），或乘 16（17³）；
- floor/ceil 找 8 个邻点；
- 按 RGB fractional order 选择 tetrahedral-like 4/6 vertex formula；代码不是简单 8-corner trilinear；
- `mix(input, c_out, intensity)` 做用户 LUT intensity；
- LUT 贴图使用 33×33² 或 17×17² 的 2D packing，输出经过 GPU texture precision 与 bitmap 8-bit 量化约束。

`C6747u.m9655O` 解析 CUBE 时生成 `Bitmap.ARGB_8888`，每个 LUT 值做 `round(value * 255)`；这是明确的**8-bit LUT preview/import representation**，而不是保持原始浮点 CUBE。native RAW SDK 另有 binary LUT path，不能把两者精度混为一谈。对 RawLab 来说，用户实测“松下白墙只有黑白噪声”可能来自 native RAW/photo-style pipeline 或 tone/chroma NR，而非普通手动 CUBE shader 单独提供的保证。

### 3.3 底座转换的确定边界

APK 中可复现的自动路径：

```text
输入图片 -> resize/JPEG decode -> ONNX PI_3D_LUT model -> generated 33³ CUBE
    -> 如果 photo style != VLOG：style_to_VLOG_2nd.cube + generated CUBE
       -> native libcube_merge.so / mergeCubeFiles
    -> 写入 #LUMIXPHOTOSTYLE <style>
```

`C6780w.m9872a` 中 `str == VLOG` 时不加载 merge map；否则使用 `C6750v` map。`libllc.so` 静态依赖 `cube_merge_files`，Java native declaration 为 `mergeCubeFiles(String base, String user, String merge)`. `libcube_merge.so` stripped，只有一个 `cube_merge_files` export 和极少 parser strings，无法从符号得到更详细数学定义；它至少是 CUBE 读取/合并器，不是证据充分的逆求解器。

`ImageFilter.nativeCnv2/nativeCnv2RGB` 的签名同时接收：style LUT size/data、cubeFact、second LUT、second intensity、valid tone flags、L tone size/data、R/G/B tone size/data。`C6749u1` 调用中 `setPhotoStyleLUT` 先装相机 PhotoStyle，`getUseToneL` 单独装 tone；`RawProcessingConfigBean.initData` 从 edit state 载入 `editLutIntensity` 与 `editLut2Intensity`。因此 native RAW editor 的“底座 + tone + 3D + look strength”是组合 pipeline，而不是单张最终 CUBE 的语义。

### 3.4 STD/CineV2/V-Log 是否统一支持

证据分层如下：

- **V-Log**：官方 Panasonic 页面明确 V-Log/V-Gamut→V709 技术 LUT；APK `C6750v` 对 VLOG 不做 style→VLOG merge（直接使用 ONNX 生成表）。
- **STD/CineV2 等自动估计**：APK 明确为每种 style 提供 `*_to_VLOG_2nd.cube`，说明自动 estimator 需要把不同 base 对齐到 VLOG target；STD、CINEV2、CINED2、CINEA2、709LIKE、NAT、PORT、VIVID 等均有映射。
- **手动导入 arbitrary CUBE**：只解析 `#LUMIXPHOTOSTYLE` 和 CUBE 数据，未见按 style 名查 map 并 merge；因此不能声称用户 LUT 在手动 import 中直接支持所有底座的自动转换。更稳妥的产品语义是：`#LUMIXPHOTOSTYLE` 是输入底座声明/metadata；若用户要跨底座，应显式选择 transform 或由 Windows 宿主预处理。

### 3.5 LUT、tone、噪声是否分离

是。APK 代码和 native signatures 形成互相支持的证据链：

- 3D CUBE/photo-style data 独立传入；
- tone 1D 数据独立传入；
- RAW config 还有 `noise_L`、`noise_C`、`nrLevel`、`cromaNrLevel`、`validCromaNrLevel`；
- LUT extra metadata 有 grain/color noise/sharpness/noise reduction；
- `GlPanaToneCurveFilter` 使用一张 256×1 RGBA tone texture，RGB 通道独立取样，先做 RGB tone curve，再可叠 composite tone；
- 普通 GPU LUT filter 只做 LUT/intensity，不承担独立降噪。

这支持用户观察：松下 App 的白墙“只有黑白噪声”不是简单证明 CUBE 颜色映射无问题；它可能是 native RAW path 在 LUT/PhotoStyle 前后做了 luminance/chroma noise separation、highlight handling、或把低可信色度压回灰轴。

---

## 4. 理论定位：为什么 RawLab 彩噪、松下白墙主要亮度噪声

### 4.1 不是“只差一个降噪开关”

白墙是低色度、高亮度、可能接近 sensor/channel clipping 的极端诊断输入。理想白墙的 RGB 噪声若高度相关，主要落在 `(1,1,1)` 亮度方向，经过温和、单调、近灰轴保持的 pipeline 后仍表现为 black/white luminance grain。RawLab 彩噪意味着 pipeline 把原本小的 channel residual / demosaic chroma error 放大或重新定向到色度平面；至少需要检查：

1. **Metamerism / camera colorimetry**：同一个视觉白墙不是唯一光谱；Panasonic native PhotoStyle LUT 与 RawLab LibRaw camera matrix 的输入坐标不完全同构。相同中性三刺激近似经过不同 CFA/matrix/white-balance path 可能有不同 channel residual。LUT 不能从三通道值恢复光谱；这是物理不可辨识性，不可由更多 Newton iterations 修复。
2. **色域溢出 / reachable gamut**：`STD_to_VLOG` 输出域只是其 33³ table 采样得到的三维可达体，不是整个 `[0,1]^3`。目标 `VLogEncode(neutralDecode(v))` 可能位于该可达体外，特别是 RawLab neutral 的高光和中灰曲线与 Panasonic STD 目标不一致时。逐通道 clamp 将一个三维外部目标投影到 box corner，不保持 hue/chroma，且不同邻点可落在不同 corner。
3. **Noise covariance 与 channel coupling**：若输入噪声协方差 `Σ_RGB` 的主特征向量接近亮度轴，变换 Jacobian `J` 后为 `J Σ Jᵀ`。当 `J` 有明显 off-diagonal（该 LUT 的许多交叉导数正负交替）时，原本“近灰轴”的噪声会获得色度分量；若输入落在 clipping/soft-threshold 区，independent clamp 还会把相关噪声的几何形状切成不连续片段。
4. **局部 Jacobian / inverse amplification**：若前向 map 在某处最小奇异值很小，逆 Jacobian 的范数变大，小输入噪声会造成大 STD 解扰动。STD map 的内部 33³ 中心差分最小奇异值仍约 0.104，最大 condition number 约 6.24，未显示灾难性病态；但局部 trilinear cell 采样的 det 可低到约 `1.73e-7`（单位未归一化时），有约一半样本 `|det|<1e-6`，说明线性插值 cell 在部分区域接近退化，不能按“平滑可逆解析函数”假定。
5. **LUT grid continuity / quantization**：若逆解 lattice 相邻点独立落到不同 clamped solution branch，三线性/四面体插值只会在两个错误分支之间平滑，无法恢复真实连续 map；8-bit bitmap LUT representation 还会把小色度差异量化为可见色块。RawLab 当前 CPU `LUTApplicator` 是 float table + trilinear，但 GPU 路径另有显式 staging；实际导出/预览必须做 bit-depth parity。
6. **look LUT 的二次放大**：适配得到的 LUT 还要作用于 Leica/Fuji 风格 look。若 look 在近白/高饱和边界的局部 Jacobian 放大色度方向，则 `J_look · J_adapt^{-1}` 会把 map inversion 的小误差变成彩噪。松下链则把 PhotoStyle/base technical transform、tone、look strength、noise controls 分层，可能在 look 前先压低色度可信度。

### 4.2 Metamerism 的正确定位

Metamerism 是解释“跨相机/跨底座不能仅靠数值 CUBE 完全等价”的物理边界，不是本次彩噪的唯一原因。RawLab 已通过 LibRaw `rgb_cam -> sRGB`，但 RawLab 的 `color_converter.cpp` 和 Panasonic native SDK 的 camera-specific color pipeline、WB-before-demosaic、highlight blend、photo-style LUT 不同；因此“把 Panasonic STD CUBE 逆套到 RawLab neutral”本身需要一个相机 input transform 契约。必须把 metamerism 写进失败模式，而不是把所有差异归结为 denoise。

### 4.3 色域/可达域与 clamp

RawLab `solvePoint` 中：

```cpp
const RGB e = clamp01(map.apply(v));
...
const RGB mapped = clamp01(map.apply(s));
```

这两个 clamp 让 solver 优化的是 `clamp(map(s))`，而不是原始 `map(s)`。如果目标在 map 的可达域外，解不是逆，而是某种依赖初值/边界的近似投影；`det < 1e-12` 时直接 break，随后仍把当前 `s` 写入 lattice。`candidate` 也把每个输入轴限制在 `[0,1]`。对溢出目标，更合理的是在 perceptual/JzAzBz/ICtCp/CAM16-UCS 等空间定义 hue-preserving chroma compression，或在 map reachable set 上求连续最近点；不应把 independent channel clamp 当成 gamut mapping。

### 4.4 为什么松下能保持白墙主要无彩

不能从 APK 静态证据断言 Panasonic 的确切 NR 顺序，但有四条可证/可合理推断的事实：

- native `ImageFilter` 接收 `noise_L/noise_C/nrLevel/cromaNrLevel` 等参数；
- PhotoStyle LUT、tone 1D 和 3D LUT 是分层接口；
- PhotoStyle assets 是 camera-specific low/high/base binary，不是 RawLab 直接使用的普通 CUBE；
- 官方/用户实测显示白墙输出以亮度噪声为主。

因此松下很可能在其 camera-specific rendering contract 中将低色度/低可信区沿 neutral axis 处理，或者其 transform 的输入噪声相关性与 LUT 相配。这里应使用“native pipeline 的 color-noise policy / base transform alignment 是候选解释”，而非声称“确认使用了某种神秘降噪算法”。

---

## 5. `STD_to_VLOG_2nd.cube` 数值检查（可复现）

### 5.1 复现脚本与方法

以下 Python 代码直接读取用户 APK 资产；不依赖 RawLab 修改。运行环境已验证 Python `numpy 2.4.5`、`scipy 1.18.0`。建议保存为临时脚本或直接在 Python REPL 运行；本报告不新增脚本文件。

```python
from pathlib import Path
import numpy as np
from scipy.linalg import svdvals

p = Path(r"F:/rawlab/lumix-lab-re/apk-extracted/assets/models/mergeLut/STD_to_VLOG_2nd.cube")
N, v = 33, []
for s in p.read_text(errors="replace").splitlines():
    q = s.strip().split()
    if len(q) == 3:
        try: v.append([float(x) for x in q])
        except ValueError: pass
a = np.array(v).reshape(N, N, N, 3)  # index [b,g,r], R fastest
```

中心 Jacobian 定义为：

```python
J = np.column_stack(((a[b,g,r+1]-a[b,g,r-1])/(2/(N-1)),
                     (a[b,g+1,r]-a[b,g-1,r])/(2/(N-1)),
                     (a[b+1,g,r]-a[b,g,r])/(2/(N-1))))
```

### 5.2 输出域、灰轴与单调性

实际结果：

- 尺寸：33³ = 35,937 points；无 NaN/Inf；所有表值在 `[0,1]` 内。
- 输出 range：R `[0.125000, 0.684778]`，G `[0.125000, 0.685089]`，B `[0.099165, 0.683982]`。
- `(0,0,0)` 输出严格 `[0.125,0.125,0.125]`，但 `(1,1,1)` 输出约 `[0.684778,0.685089,0.683982]`，不是 `[1,1,1]`，说明这是 log/中间 transform，不是 display-output look。
- 灰轴样本：

| 输入灰 | map R | map G | map B |
|---:|---:|---:|---:|
| 0 | .125000 | .125000 | .125000 |
| .03125 | .199413 | .205207 | .194641 |
| .125 | .293272 | .296456 | .288589 |
| .25 | .353813 | .356260 | .349244 |
| .5 | .451735 | .453560 | .448665 |
| .75 | .549111 | .550946 | .545554 |
| .875 | .601499 | .603844 | .596848 |
| 1 | .684778 | .685089 | .683982 |

灰轴通道差异最大约 G 0.005794、B 0.004772，不能把 transform 视作完全灰轴保持。

RawLab 适配器的预期 target 为 `VLogEncode(neutralDisplayDecode(v))`。用当前 `neutralDisplay` 逆函数和 `vlogEncode` 复算，target 在 display 1.0 处只有约 `.599118`，而 STD map 端点为约 `.684778/.685089/.683982`；整条灰轴 map-target 最大绝对误差约 `.08597`，R/G/B mean absolute error 约 `.02212/.02399/.02021`。这不是小的浮点误差，而是**目标灰轴契约不一致**。

沿每个输入轴检查输出单调性（33³ 表所有相邻边）：

| 输入轴 | 输出 R 负步 | 输出 G 负步 | 输出 B 负步 |
|---|---:|---:|---:|
| R | 0 (0%) | 660 (1.894%) | 9,276 (26.618%) |
| G | 554 (1.590%) | 0 (0%) | 18,091 (51.914%) |
| B | 8,549 (24.532%) | 7,409 (21.261%) | 0 (0%) |

所以 R→R、G→G、B→B 主响应单调，但 cross-channel 响应大量非单调，尤其 B 输出对 G 轴约 51.9% 负边、对 B 轴本身仍单调。这正是“不能按三个 independent 1D curves 求逆”的数值证据。

### 5.3 Jacobian、条件数、可逆性

内部 29,791 个中心样本结果：

- `det(J)` min `0.00769943`，max `2.42424675`，median `0.03220996`，negative count `0`。
- singular values：最小/median/max（按各列统计）约：
  - σ1: `0.35520 / 0.44560 / 1.91611`
  - σ2: `0.17294 / 0.34278 / 1.46190`
  - σ3: `0.10431 / 0.21625 / 0.88579`
- condition number：min `1.17228`，median `2.28010`，95th percentile `2.97130`，99th percentile `3.59498`，max `6.24388`；没有 `cond > 10`。
- Worst condition location (input approx `[0.875,0.03125,0.03125]`)：J ≈ `[[.22766,.05346,-.04226],[.07294,.56976,-.03074],[-.02794,.40262,1.25414]]`, singular values `[1.32713,.56275,.21255]`, cond `6.24388`。
- Minimum determinant location (input approx `[.625,.65625,.6875]`)：J ≈ `[[.193952,.129424,.023936],[.074288,.263936,.054672],[.004608,.072208,.200432]]`, det `.00769943`。

这组结果说明：在**格点中心差分尺度**下，不能把 STD map 判为明显多对一或全局折叠。可是三线性 cell 内的 Jacobian 不是常数；对 32³ cells、每 cell 3×3×3 内点采样，det 最小约 `1.73e-7`、最大约 `1.75e-4`，约 442,023/884,736 样本 `|det|<1e-6`，但没有负 det。注意这里的 det 使用“每个 normalized input coordinate 的实际 cell step”导数；它与中心差分的归一化尺度不同，不能直接横向比较绝对数值，重点是 cell 内可能接近退化且不应假定解析全局单值逆。

33³ 输出格点按四舍五入到 6/5/4/3 位小数都仍有 35,937 个 unique values；因此未发现“表值精度直接造成大量精确重复”。但这不排除 GPU/Bitmap 的 8-bit packing 和 inverse lattice 的 branch discontinuity。

### 5.4 当前 RawLab 适配逻辑的数学不满足项

文件：`F:/rawlab/RawLab/lutools/src/core/lut_adapt.cpp`，头文件：`F:/rawlab/RawLab/lutools/include/sony2fuji/lut_adapt.h`。

1. **目标输出不在官方 map 的已验证灰轴契约上**：RawLab 自定义 neutral log-logistic display curve 的端点/中灰与 Panasonic map 不同，却直接要求 map inverse 命中该 target。
2. **把三维外部目标做 box clamp**：`clamp01(map.apply(v))` 改写了残差函数；不可达 target 没有定义明确的 perceptual projection。
3. **逐点独立解**：每个 fine lattice point `solvePoint(map,target,6)`，失败后仍写入 `stdS`；同一连续输入邻域没有共同 regularized inverse objective。
4. **固定有限差分和未检查局部可逆性**：`h=1/128`，只算 forward perturbation，未检查 central difference、cell det、singular value、condition number；`det` 小就 break，但没有把该点标为 invalid/weight 0。
5. **平滑不是约束求解**：`kJump=.05` 的 6-neighbour mean 会改变目标 residual，且可能跨越真实色域边界；它只是事后去噪，不保证单调、injectivity、或 map-consistent continuity。
6. **confidence fade 混合错层风险**：residual 大时把 `looked` 向中性 `v` 淡回，创造的是“残差依赖的 look strength”，不是对真实底座 transform 的连续色度压缩；会把白墙/近灰不同噪点送入不同 `w`。
7. **solver 初值并非真正三维 coarse seed**：`coarseSeed` 只从 coarse lattice 的中性 diagonal `(i,i,i)` 插值，且 fine point seed 用 `(r+g+b)/3`，对饱和颜色没有完整三维 coarse interpolation。`diagIn/diagOut` 被计算但没有用于 seed 或 solve。
8. **末端 `vlogEncode` clamp 到 [0,1]**：而官方 map 的输出实际只到约 `.685`；RawLab target 的高光分布与官方 VLOG domain 未对齐，导致可达域外比例必须量化，而不是假定均可逆。
9. **当前通用 CUBE LUT 仍 trilinear**：`F:/rawlab/RawLab/lutools/src/core/lut_applicator.cpp` 使用 8-corner trilinear；Lumix 普通 shader 使用按 fractional order 的 tetrahedral-like formula。插值差异不是彩噪唯一来源，但应纳入 parity test。
10. **RawLab neutral pipeline 与实际 Panasonic PhotoStyle pipeline 不同**：RawLab `raw_processor.cpp` 是 LibRaw + camera matrix + neutral log-logistic；Panasonic native `ImageFilter` 有 camera-specific PhotoStyle LUT、L/R/G/B tone arrays、noise_L/noise_C/cromaNrLevel 等参数。把官方底座转换表单独接在 RawLab neutral 后面不是完整等价。

---

## 6. 候选解决方案排序

### P0 / 第一优先：A. 复现 Panasonic 官方底座变换链（tone 1D + 3D merge LUT）

**方案**：先不做用户可见的逐点逆 CUBE。把 Windows 宿主验证器拆成：

```text
RawLab linear/camera-neutral
 -> 明确的 base normalization / tone 1D
 -> Panasonic style-specific 3D base LUT 或 *_to_VLOG merge transform
 -> user look LUT（仅在其声明的 base domain）
 -> gamut/noise policy
 -> display encode
```

对于自动 LUT 链，严格复现 APK 观察到的 `generated LUT -> style_to_VLOG -> mergeCubeFiles`，并用相同 33³ 数据测试。对于手动用户 LUT，不要偷偷猜底座：若 metadata 是 `STD/CNEV2/VLOG`，先把它标作 contract；需要跨底座时显式选择 transform。A 的价值是先回答“彩噪是否来自底座契约错误”，而不是继续优化 inverse solver。

**Windows 宿主先验验证**：必须先做。验证项：

- gray ramp、RGB ramps、hue sweep、near-white flat field、dark flat field；
- RawLab neutral vs Panasonic-style chain 每一步导出 EXR/16-bit PNG/float CUBE；
- 对官方 `STD_to_VLOG_2nd` 的 table application 与 `mergeCubeFiles` 输出做 byte/float parity；
- user look 在 STD 原域直接应用与经官方 merge chain 应用的 A/B；
- CUBE 33/17、float vs 8-bit packed、tetrahedral-like vs trilinear。

**通过标准**：灰轴误差、near-white chroma σ、gradient banding、ΔE/Δhue 必须不劣于 Panasonic reference；平坦墙 color-noise energy 下降到黑白噪声级别；所有 intermediate domain 都有明确 range/contract。

### P1：C. 按局部 Jacobian/可达域做连续 chroma compression

**方案**：对 `target` 不可达或局部 σmin 低的区域，在 JzAzBz/ICtCp/CAM16-UCS 等感知/色度空间沿固定 hue/轻微 lightness 调整 chroma，使 target 进入 `map(D)`；然后求 inverse。投影必须是连续的，输入/输出和邻域共享 regularization；输出不是 independent `min/max` clamp。

**先验验证**：Windows 宿主必须做。先用 synthetic flat noise：给白墙添加不同 `Σ_RGB`，测 `JΣJᵀ`、chroma σ、hue variance；对比 independent channel clamp、RGB radial compression、J-aware compression。通过标准是：near-white gray axis 不断裂；compression weight/chroma 以及 residual 对输入连续；无彩色 speckle/banding。

**实现门禁**：只有 P0 证明必须跨域 inverse 且 A 仍不能达标，才进入代码。不要把 C 直接塞入当前 `solvePoint`。

### P1.5：B. 感知/色度空间 gamut compression 后再适配

**方案**：在进入底座 inverse 前，把 RawLab neutral 的宽域/高光 target 做 hue-preserving gamut compression；优先保护 lightness/neutral axis，压缩 chroma 而不是三个通道独立裁切。B 是 C 的较简单全局版本，可作为宿主 baseline。

**Windows 先验验证**：必须先做。用 ColorChecker、near-white ramp、synthetic saturated boundary 和用户白墙对比 compression strength。通过标准：白墙无彩、肤色/蓝天 hue error 可接受、低饱和区不被灰化、连续一阶梯度无 banding。

### P2：D. 只做 LUT 输入域适配，不对输出 look 再适配

**方案**：将“输入底座 STD→共同工作域”与“输出 creative look”分开；不要把 look 的 output 再通过一个不匹配底座逆变换。用户 look 若本来声明 `#LUMIXPHOTOSTYLE STD`，只适配输入端到 STD contract，输出保持 look 的定义域；若输出目标为 VLOG，另建 technical transform，不与 look 混成一张不透明 cube。

**优点**：避免 `looked = look.apply(s)` 后再 confidence fade/二次 map 放大；更接近 Panasonic technical/look 分层。**风险**：若输入域本身没有良定义，仍需 A/B/C 的 normalization。

**先验验证**：用 identity look、官方预安装 STD looks、CNEV2/NAT looks 先做。identity 必须严格 identity；STD base 中性 ramp 必须不出现彩轴；输出 look 的 ΔE/contrast 只在其 contract 输入上评估。

### P3：增加 LUT 尺寸/换插值/仅改 Newton

这些是辅助实验，不是根治方案：

- 33→65 只能降低 table sampling error，不能修复错误 domain、不可达 target 或 metadata 误用；
- trilinear→tetrahedral-like 可能改善 hue interpolation，但不会让错误 inverse 变为正确 inverse；
- 增加迭代次数/改变 damping 只会更精确地找到错误的 clamp projection；
- 继续做 neighbor mean 可能暂时减 speckle，但会牺牲 map fidelity，不能作为发布方案。

### 其它合理方案 E：不生成跨底座 baked CUBE，运行时分层应用

把 LUT 资产及 contract 存成：`baseStyle`、`technicalTransform`、`lookLUT`、`toneCurve`、`gamutPolicy`。运行时按顺序执行，必要时把最后的组合仅用于 cache/导出。这样可以保留 1D tone 精度、根据图像/噪声动态选择 chroma policy，避免把所有东西量化成一张 33³ 表。对于 Windows/Android GPU，优先保留 FP16/FP32 intermediate，最终显示才量化。

---

## 7. 开发门禁：先做什么、通过标准、暂禁什么

### 7.1 实验顺序

1. **锁定基线和证据**：把 APK、资产 SHA-256、RawLab 当前 binary/commit、用户白墙原图/PNG/导出结果登记。报告此版本只读；不要修改代码。
2. **资产解析单测**：对所有 13 个 `mergeLut` CUBE、identity/gamma assets、预安装顶层 CUBE，检查 size、R-fast order、finite/range、gray axis、单轴/交叉轴 monotonicity。
3. **Windows float reference**：写独立宿主实验器（可以是临时外部工具，不改渲染核心）复现 CUBE apply、官方 VLOG encode、neutral decode、merge order；输出每步 float buffer。
4. **官方链 A/B**：对 STD/CNEV2/NAT/VLOG 四组：用户 look 直接 apply、base transform 后 apply、合并后 apply；比较官方 APK/实机导出（若可取得）与 RawLab。
5. **平坦场噪声矩阵**：白/灰墙，ISO/曝光/欠曝等级，采集至少无 LUT、直接 look、当前 inverse adaptation、A、B/C；测 `σY`、`σchroma`、RGB covariance、局部 hue variance、near-white banding。
6. **连续性与可达域**：在 33³ cells 内采样 trilinear Jacobian、singular values、reachable residual；对逆 lattice 测 6/26 邻居 jump、residual、monotonicity、灰轴。
7. **插值/量化 parity**：CPU trilinear、Lumix tetrahedral-like、GPU texture/8-bit packing、FP16/float 互比，确保彩噪不是 shader/table layout bug。
8. **再决定方案**：只有 P0/A 与 B/C synthetic tests 通过后，才决定实现运行时分层或受约束 inverse；不要先扩展 UI 或导出格式。

### 7.2 建议通过标准

- **灰轴**：identity 与 technical transform 在定义域内单调；near-gray 的 `max-min(R,G,B)` 不得随亮度出现尖峰；目标与 reference 的灰轴误差按定义域报告，不允许把不同域的误差混成“平均颜色误差”。
- **连续性**：任何合法 ramp 的相邻输出 jump 不得产生孤立尖峰；6/26 邻居 inverse jump 与同尺寸直接 look baseline 比较；不允许通过一刀切 clamp 隐藏 residual。
- **噪声**：白墙色度噪声能量 `σ_chroma`、hue variance、彩色像素比例分别报告；目标是接近 Panasonic reference 的黑白噪声而非只降低总方差。不得以“整体更平滑”替代色度指标。
- **数值**：finite、range、R-fast index、33/17 parser、CPU/GPU parity；local `σmin`、condition、cell determinant 都要有 histogram 和 worst coordinates。
- **外观**：肤色、蓝天、植被、混合光、高光 specular、暗部平坦场分桶；ΔE00/Δhue/亮度曲线一起看，不能只看 PSNR/平均 RGB。
- **失败可解释**：target outside reachable map 必须计数并标记 policy；失败点不能默默写入 CUBE 后宣称“逆成功”。

### 7.3 暂时禁止

- 禁止把 `STD_to_VLOG_2nd` 当作普通 display look 直接作用于 RawLab neutral。
- 禁止在没有 Windows 宿主数值报告前继续改 `solvePoint` 的迭代次数、damping、jump threshold、confidence fade。
- 禁止独立通道 hard clamp 作为正式 gamut mapping；只能作为明确标注的诊断 baseline。
- 禁止用 33→65、trilinear→tetrahedral、8-bit→16-bit 单一改动宣称修复了彩噪。
- 禁止把 `#LUMIXPHOTOSTYLE STD` 当作“这张 LUT 能自动把任意输入变成 STD”的证明；它是 base/style contract metadata。
- 禁止把 Panasonic `photoStyleLUT/*.bin`、官方 `mergeLut` CUBE 或 APK 资源直接随 RawLab 分发，除非许可/授权另行核实；本报告只引用本地用户提供资产作研究。
- 禁止把“松下白墙黑白噪声”归因于“只是降噪”而不测 `Σ_RGB`、map Jacobian、reachable residual、tone/LUT order。

---

## 8. 参考证据清单

### 一手官方

1. Panasonic Digital Camera About LUT：<https://av.jpn.support.panasonic.com/support/global/cs/dsc/download/lut/index.html>
2. Panasonic VARICAM LUT Library：<https://pro-av.panasonic.net/en/cinema_camera_varicam_eva/support/lut/index.html>
3. RawLab repository’s pre-existing color-contract references: `F:/rawlab/RawLab/lutools/docs/color-contract.md`, `F:/rawlab/RawLab/docs/research/01-color-science-lut-math.md`.

### 一手 APK/逆向

1. APK root: `F:/rawlab/lumix-lab-re/Panasonic LUMIX Lab_3.0.1.apk`
2. Extracted assets: `F:/rawlab/lumix-lab-re/apk-extracted/assets/LUTFile`, `.../models/mergeLut`, `.../photoStyleLUT`, top-level `.cube`.
3. Auto LUT and merge: `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/util/C6780w.java` lines 44–115, 205–241.
4. Style map: `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/util/C6750v.java` lines 6–21.
5. Import/parser: `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/controller/fragment/LutFragment.java` lines 5419–5563 and `.../util/C6747u.java` lines 806–976.
6. Import persistence/metadata: `F:/rawlab/jadx-out/sources/p028C5/RunnableC0607F8.java` lines 30–89.
7. GPU 33/17 shader: `F:/rawlab/jadx-out/sources/com/panasonic/p321jp/lumixlab/widget/GlLUTFilter.java`, `GPUImageLUTFilter.java`, `GPUImageLUTFilter17.java`.
8. Native RAW LUT/tone API: `F:/rawlab/jadx-out/sources/com/techor/techorfilter/ImageFilter.java` lines 59–70; `.../util/C6749u1.java` lines 620–665, 3387–3400, and native calls around 1438–1460.
9. Native exports: `F:/rawlab/lumix-lab-re/apk-extracted/lib/arm64-v8a/libllc.so`; merge helper `.../libcube_merge.so`.

### 二手/通用理论（用于解释，不冒充 Panasonic 事实）

- CIE metamerism definition: <https://eilv.cie.co.at>
- ACES Input Transform / Reference Gamut Compression / encoding docs: <https://docs.acescentral.com/system-components/input-transforms/> and <https://docs.acescentral.com/rgc/overview/>
- ITU-R BT.2100 ICtCp: <https://www.itu.int/rec/R-REC-BT.2100>
- JzAzBz: Safdar et al., DOI <https://doi.org/10.1364/OE.25.015131>
- OpenColorIO/FFmpeg LUT interpolation docs: <https://opencolorio.readthedocs.io/> and <https://ffmpeg.org/ffmpeg-filters.html>

---

## 9. 研究限制

- 未在本报告中动态运行 Android APK，也未抓取真实 Panasonic camera/LUMIX Lab network or PTP traffic；用户实测是重要黑盒证据，但不替代 source-level proof。
- `libcube_merge.so` stripped，只有 `cube_merge_files` export 和 parser strings；merge 的 exact formula 尚未知。
- `photoStyleLUT/*.bin` 的 proprietary header/payload 未完整逆向；只能依据 Java/native call graph 分类为相机特定 rendering asset。
- 未拿到用户实际白墙 RAW、RawLab 输出与 Panasonic 输出的同一像素数据，因此噪声结论是理论 + APK pipeline evidence + 用户观察的可检验假设，不是最终因果证明。


## 10. 实验 A/B 实测结果（2026-10-04 补充）

工具：`host-harness/pipeline_experiment.cpp`（噪声场 256 次实现 × 4 灰度 × 4 条管线；
逆适配臂使用 `sony2fuji_adapt_std_lut` 的实际产物 `velvia-adapted.cube`，DISPLAY 模式直查）。

| 管线 | 灰0.3 亮度/色度 | 灰0.5 | 灰0.7 | 灰0.9 |
|---|---|---|---|---|
| 中性参考 | 2.33 / 0.80 | 2.33 / 0.80 | 2.33 / 0.80 | 2.33 / 0.80 |
| look 直套（未适配） | 5.01 / 1.20 | 2.94 / 0.92 | 3.77 / 1.04 | 3.64 / 1.32 |
| 官方分层链 tone→look | 3.04 / 0.97 | 3.74 / 0.77 | 6.00 / 1.22 | 11.19 / 0.91 |
| **逆适配 LUT（实际产物）** | **11.42 / 2.27** | **7.47 / 1.91** | **9.61 / 2.43** | **4.07 / 3.04** |

结论：

1. **逆适配 LUT 的色度噪声是所有管线中最高的**（0.9 灰度处 3.04/255，是直套 1.32 的
   2.3 倍、分层链 0.91 的 3.3 倍；0.3 灰度处 2.27 vs 直套 1.20）。亮度噪声同样最高
   （0.3 灰度 11.42 vs 直套 5.01）。这定量证实：**彩噪来自逆适配格点本身的噪声**
   （逐点求解 + clamp 分支 + 平滑残余），而不是 STD 底座 tone 曲线固有的放大。
2. 官方分层链（tone→look）的色度噪声接近中性参考（0.77–1.22），仅亮度在 0.9 处
   被放大（11.19，tone 曲线高光斜率所致）——这解释了松下 App 白墙"只有黑白噪声"：
   其链路是分层 tone+LUT，色度上近似灰保持。
3. 因此正确方向是 P0/A（复现官方分层链）而非继续修补逆求解器：适配 LUT 应被
   **"中性→STD tone 1D + look"的分层应用替代**，或者适配器需重写为"tone 重映射 + look"，
   不做逐点 3D 逆。

## 决议

- 采用**分层链**作为 STD 底座的正式实现：中性值 → neutralDecode → STD tone 1D
  （官方 gamma_corrected_33_tone.tone 数据）→ look LUT 查表（DISPLAY 域）。
- `sony2fuji_adapt_std_lut`（3D 逆）降级为实验代码，不再用于导入路径。
- 换用其它底座（CNEV2/NAT 等）时，只需对应 tone 表（若官方提供）或保留 STD 表近似，
  待后续实验。
