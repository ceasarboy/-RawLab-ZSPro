# 04 · 从参照对拟合 3D LUT：方法学调研

> 调研目的：为安卓 RAW 显影编辑器建立"中性渲染 vs 目标风格渲染"参照对 → 3D LUT 的精品级生产线。
> 调研日期：2026-10-03。标注规则：【一手】= 论文/源码/官方文档原文；【二手】= 媒体/论坛/综述转述。

---

## 1. 拟合算法谱系：从 3x3 矩阵到神经方法

**最小二乘 3x3 矩阵（可含偏置）**。最基础的做法是把参照对的 RGB 对排成矩阵做最小二乘（可增广偏置列，即 4x3）。OCIO 官方工具链即以此为核心：`ociolutimage --generate` 生成色卡图 → 过目标变换 → `--extract` 抽回数值 → `ociolutmatsolve --solve` 解出最优 3x3+offset 矩阵【一手，https://opencolorio.readthedocs.io/en/latest/guides/using_ocio/using_ocio.html】。Python colour-science 提供 `colour.matrix_colour_correction`，已实现三种方法：`Cheung 2004`、`Finlayson 2015`（向量多项式）、`Vandermonde`（多项式展开）【一手，https://github.com/colour-science/colour】。矩阵法的优点是外推稳定、无 banding 风险；缺点是只能表达"准平面"变换，对饱和区的 hue twist（见第 4 节）无能为力。

**多项式回归（带偏置）**。将每通道 RGB 升维成 3/7/11/20… 项多项式（Vandermonde 展开）再做线性回归，是相机色彩特征化（camera characterization）的标准套路，经典教材为 Kang《Computational Color Technology》(SPIE Press)【二手，多数论文引用】。风险：高阶项数值病态（需归一化/正则），训练色域外外推剧烈发散。

**3D lattice 回归**。Gupta 等在 CIC 2009 提出 lattice regression：把控制点直接放在 LUT lattice 上优化，以 lattice 能量项做平滑正则，并在论文中与多项式、RBF、TPS 对比，结论是"精度与平滑可以同时拿到"——17x17x17 LUT 在测试色上优于各传统方法【一手，PDF：http://www.mayagupta.org/publications/2009-CIC-Smooth-LUTsWithAnimation.pdf；后续扩展为 IEEE TIP 2014 "Lattice Regression"】。Google 的 TensorFlow Lattice 库把这套思想产品化，提供带**单调性/形状约束**的 lattice 模型（Keras 层）【一手，https://www.tensorflow.org/lattice ，https://github.com/tensorflow/lattice】——这是"带单调性正则的 lattice 优化"可直接复用的工程实现。

**RBF / 薄板样条（TPS）**。经典散乱数据插值（Hung 1993 对 3D LUT 样条插值的早期系统研究【二手转述】）。TPS 可视为带一阶平滑正则的核回归，全平滑但全局核使远端外推退化为线性趋势；样本量大（SG 140 色块 x 多场景）时求解开销大。lattice 论文正是以此二者为对照组，可作为选择依据【一手，同上 PDF】。

**基于优化的 lattice / 深度 3D LUT**。近年一条清晰的论文线把"优化 lattice + 约束"做成体系：Zeng 等的 Image-Adaptive 3D LUT（ICPR 2020 → TPAMI 2022，学习多个基 LUT + 轻量 CNN 预测权重，训练时加**单调性/值域约束**）【一手 repo：https://github.com/HuiZeng/Image-Adaptive-3DLUT ，~939 stars】；CLUT-Net（低秩分解压缩 LUT，https://github.com/Xian-Bei/CLUT）；SepLUT（ECCV 2022，用可分解结构**从构造上保证有界与平滑**）；SA-3DLUT（ICCV 2021 空间感知）；NILUT（条件神经隐式 LUT，arXiv 2023）【二手，汇总自搜索结果与论文引用链】。对"静态风格 LUT 拟合"最有价值的是它们的**约束设计**：单调、有界、平滑正则——直接决定了成品有没有 banding。

**神经色彩迁移（neural colour transfer）**。主综述为 Jing 等 "Neural Style Transfer: A Review"（arXiv:1705.04058，TPAMI 2020 版含 color transfer 专章）【一手，https://arxiv.org/abs/1705.04058 】；延伸阅读：Style Transfer: A Decade Survey（arXiv:2506.19278）、Evaluation in NST: A Review（arXiv:2401.17109，专门讨论评测）【一手，arxiv.org】。判断：像素级深度方法依赖图像内容（语义、空间先验），**无法完整蒸馏进一张静态 3D LUT**；可借鉴的是其损失函数思想（如语义区域加权误差），而非直接替换拟合器。

**横向对比**（综合一手论文与上述引用链）：
| 方法 | 精度（域内） | 平滑性 | 外推行为 | 工程复杂度 |
|---|---|---|---|---|
| 3x3+偏置 | 低-中（色相扭转表达不了） | 好 | 线性、安全 | 极低 |
| 多项式(11~27项) | 中-高 | 中 | 域外爆炸 | 低 |
| lattice 回归+平滑正则 | 高 | 高 | 受控（网格边界钳制） | 中 |
| RBF/TPS | 高 | 高 | 全局线性化 | 中 |
| 优化 lattice/深度 | 高 | 可控（约束） | 可控 | 高 |

**对本项目的启示**：主线采用"lattice 回归 + 平滑正则 + 单调性约束"（复用 Gupta 的正则设计与 TF Lattice 的约束思路），以多项式回归（Finlayson 2015，colour-science 实现）做快速基线和初始化，3x3 矩阵法（OCIO ociolutmatsolve）做白平衡级兜底与交叉验证。深度方法只抄约束，不抄模型。

---

## 2. 开源工具盘点（成熟度 / 许可证）

| 工具 | 用途 | 许可证 | 活跃度 | 链接 |
|---|---|---|---|---|
| colour-science | 色彩校正/多项式拟合/色差/ColorChecker 数据集（`BabelColor Average` 等） | BSD-3 | 高（学术级维护） | https://github.com/colour-science/colour 【一手】 |
| OpenColorIO v2 | ociolutmatsolve / ociolutimage / ociobakelut（烘焙 .cube/.3dl）/ ociomakeclf | BSD-3 | 高 | https://opencolorio.readthedocs.io/en/latest/guides/using_ocio/using_ocio.html 【一手】 |
| LUTCalc（Ben Turley） | log/gamut 转换与监控 LUT 生成、分析、预览 | MIT（作者声明） | 中（~350 stars，网页版可用） | https://github.com/cameramanben 【一手 README】 |
| darktable-chart | **RAW+JPEG 参照对 → .dtstyle（tone curve + ColorChecker 模块）**，即现成的"参照对拟合器" | GPL（darktable 主体） | 高 | https://docs.darktable.org / 介绍博客 https://jo.dreggn.org 【一手】 |
| ImageMagick | `hald:` 生成 identity Hald、`-hald-clut` 应用；snibgo 的 sparse hald 反推教程 | Apache-2.0 类（IM 许可） | 高 | http://im.snibgo.com 【一手教程，社区作者】 |
| G'MIC | 色彩映射/transfer 命令，社区曾用于反推 CLUT | CeCILL | 高 | pixls 论坛一手自述：https://discuss.pixls.us/t/how-was-the-rawtherapee-film-simulation-collection-created-and-thanks/34265 |
| Lightroom/ACR "Export as LUT" | 从调整结果烘焙 LUT → 可作为 Profile 使用 | 闭源 | — | 限制分析【二手】：https://f64academy.com （烘焙滑块、无局部调整、不感知曝光） |
| 3D LUT Creator | 商业调色工具，A/V/C/H 色彩体（ACHV）引擎 | 商业 | — | https://3dlutcreator.com/ 【一手官网，但 ACHV 无公开算法文档，仅视频教程【二手】】 |
| clut2dtstyle 等小工具 | HaldCLUT ↔ darktable style 转换 | 各异 | 低（多已弃用，darktable 3.0 原生支持 HaldCLUT） | https://github.com/manu-mannattil/clut2dtstyle 【一手 README】 |

注：用户提到的 "hald-clut-fit" 未检索到同名知名仓库；ImageMagick 生态的反推工作流以 snibgo 的 sparse hald 教程为最接近实现。

**对本项目的启示**：拟合内核不必从零写——用 colour-science 做拟合与色差计算、OCIO 工具链做交叉验证与 LUT 烘焙/格式转换；darktable-chart 的"参照对 → 样式"流程是极好的对照实现（可读 GPL 源码学习其网格拟合细节，但代码不可直接并入闭源产品）。LUTCalc 说明 MIT 许可下把 log 曲线数学做成纯函数库是成熟路径。

---

## 3. 数据采集协议：参照对怎么拍、怎么造

**色卡选择**。ColorChecker Classic 24 的色域明显小于 ColorChecker SG 140（"Macbeth at 40" 甘特图对比【二手，https://swppusa.com 】）；文献中高精度相机特征化普遍采用 SG 140（如 MDPI 2021 用 SG 做 XYZ 恢复【二手，https://www.mdpi.com 】）。24 卡的肤色轴（2 行）仍然最精细，但饱和区样本不足——而风格 LUT 恰恰要改饱和区。

**社区 LUT 作者的公开方法论**（一手自述）：
- RawTherapee Film Simulation 集合（Pat David）：绝大多数 CLUT 是"在编辑器里对 identity Hald 反复主观调整"而成，也部分用 G'MIC 从现有包反推；明确自述"不追求色彩准确，追求触发记忆的相似感"，并坦承没做色卡控制实验。帖中提到严谨路径：Peter（Out of Focus 博客）**用胶片拍色卡、同条件下数码拍同卡，再从对照中导出 LUT**【一手，https://discuss.pixls.us/t/how-was-the-rawtherapee-film-simulation-collection-created-and-thanks/34265 】。
- bastibe/Darktable-Film-Simulation-Panel：**用 darktable-chart + 实拍 ColorChecker** 重建 Fuji 与 Ricoh "Positive Film" 风格【一手，https://github.com/bastibe/Darktable-Film-Simulation-Panel 】。
- Leeming LUT Pro（商业）：每台相机分析**大量实拍镜头**标定色彩响应，并强制用户遵守严格机内设置协议（指定 Picture Profile、黑位、ISO、精确 WB、ETTR 曝光），因为 LUT 只在该协议下成立【一手，https://www.leeminglutpro.com/ 】。

**参照对的两种来源**：
1. **合成对（同 RAW 双路渲染）**：中性渲染 = 自研 RAW 管线输出（可控、可复现）；目标渲染 = 厂商 JPEG / 胶片扫描。优点：像素级对齐、无配准误差；缺点：被参考引擎的局部调整（自适应 WB、降噪、动态 tone mapping）污染，那些是静态 LUT 无法复现的部分。
2. **实拍对（同场景双机/双设定）**：覆盖真实光谱与真实噪声，但引入配准、曝光、WB 差异，需先做亮度/WB 归一再拟合。

**覆盖要求**（综合文献与上述实践）：场景类型覆盖肤色/天空/植被/混合光/夜景各≥ 数十张；亮度轴从中灰到 -4EV（暗部）与 +2EV（高光）；饱和区必须靠 SG 卡或高饱和花卉/霓虹补足。胶片扫描对（若做胶片风格）还需固定冲扫批次，避免药水/扫描仪漂移混入。

**对本项目的启示**：以"合成对"为主干（像素对齐、可回归），胶片/厂商风格再用"实拍对"做补充校正；每风格固定采集协议文档化（Leeming 式的 Setup Guide 思路同样适用于自家拟合管线——输入不标准，拟合必翻车）。

---

## 4. 质量控制与常见翻车点清单

| 翻车点 | 成因 | 检测方法 |
|---|---|---|
| 蓝天 hue twist（青→紫漂移） | ① 3x3/低阶多项式表达不了饱和蓝区的非线性色相；② CIELAB 蓝区"blue hue nonlinearity"是公式级已知弱点，ΔE 权重在此区不均匀 | 合成蓝天 ramp（hue 扫描）上画 Δhue(θ) 曲线；per-hue error 图 |
| 肤色漂移 | 肤色恰在 24 卡高采样区，拟合偏向它，牺牲其他区；或训练对 WB 不一致 | 肤色 patch 子集单独报告 ΔE00；真人盲评 |
| 高光去饱和/色相断裂 | lattice 边界外数据稀疏；RGB 超界被钳制到灰 | 近白 ramp（0.9–1.0）上检测饱和度突降点 |
| 阴影噪声放大 | 拟合对在暗部噪点差异被当作"色彩"学进 LUT，暗部曲线过陡 | 暗部平坦区域噪声 σ 对比；-4EV 场景实拍检查 |
| 33 点网格色带 | lattice 分辨率不足 + trilinear 插值阶次低；tetrahedral 更平滑 | 对合成渐变图做梯度直方图/波纹分析 |
| 单调性破坏 → banding | 无约束拟合产生非单调响应 | 沿对角线 ramp 检查 dOut/dIn≥0；TF Lattice 单调约束可从源头消除 |
| 工具坑：3D LUT 索引 | OCIO 官方 issue 曾记录 LUT 索引按 31.0 而非 32.0 计算，造成 ~3% 增益错误 | 应用前后中性 ramp 严格归零【一手 issue：https://github.com/AcademySoftwareFoundation/OpenColorIO 】 |

**ΔE 指标选用**。ΔE76 均匀性差仅适合粗筛；ΔE00 是小色差事实标准（FADGI 复制级阈值 avg ≤ 2.0 的讨论【二手，https://www.researchgate.net/publication/…"Exploring the limits of color accuracy in technical photography"】）；**大色差/宽色域场景建议 ΔEz（CAM16-SCD 基，Wang/Li/Luo 2017）**，colour-science `colour.difference` 已内置【一手文档，https://colour.readthedocs.io/en/master/colour.difference.html 】；无间隔观察条件还有 ΔENS（Mirjalili & Luo 2019，arXiv:1904.11293）【一手】。平均 ΔE 会掩盖方向性错误——hue twist 可以藏在低均值里，必须配 per-hue/per-luma 分层。另可参考专门的色彩变换平滑度 metric 论文（"A metric for the evaluation of color perceptual smoothness"）【二手】。

**对本项目的启示**：把"翻车点"全部转成 CI 里的合成图回归用例（ramp/hue 扫描/暗部平坦场），人工只看分层报告；ΔE 用三件套（ΔE00 主体 + ΔEz 大色差 + Δhue 方向性），拒绝单一均值。

---

## 5. 已知风格数据的可获取性与许可证

- **Fujifilm**：官方**不**提供 film simulation 的 LUT 包；fujifilm-x.com 仅按机型提供 "F-Log LUT file and IDT file"（F-Log/F-Log2 → Rec.709 转换，见 X-H2S、GFX100 II 固件页的下载指引）【一手，https://www.fujifilm-x.com 】。胶片模拟本身在机内/X RAW Studio 实现，属闭源固件行为。官方下载受站点协议约束，未见"可再分发"条款 → **默认不可再分发，也不可直接逆向植随包**。社区替代：Fujify.me 等第三方付费包（单用户授权）、bastibe 的 darktable-chart 重建（MIT）。
- **Panasonic**：官方 VariCam LUT Library 提供 **35 个免费 3D LUT**（三种格式），V-Log/V-Gamut 转换 LUT 亦官方分发【一手，https://pro-av.panasonic.net 】。免费下载≠可再分发：EULA 原文需在下载时逐条确认（本项目已有 Panasonic 风格调研文档可衔接）。
- **社区重构项目**（可再分发性关键）：
  - t3mujinpack：Kodak Portra/Fuji 系列模拟，**CC0（公有领域）**，可再分发【一手，https://github.com/t3mujinpack/t3mujinpack 】。
  - bastibe/Darktable-Film-Simulation-Panel：MIT【一手】。
  - RawTherapee 官方 Film Simulation 集合：托管于 rawtherapee.com，来源混杂（主观调整+反推），帖内未给统一许可，引用需逐文件核实【一手自述，许可状态未明 → 待核】。
  - cedeber/hald-clut 等聚合仓库：许可各异，逐条核查。
- **风险提示**：即使算法上"风格再现"合法，"Portra""Leica"等是商标；产品文案应描述特征（"暖调低反差人像"）而非冒用品牌名。

**对本项目的启示**：可自由使用的素材只有 CC0/MIT 类社区重建；官方厂商 LUT 仅可作**离线拟合参照**（用户自有设备的文件），绝不可打包分发。自有风格应从自建参照对拟合，避免任何"克隆某品牌"的供应链风险。

---

## 6. 验证体系设计（精品级回归 + 盲评）

**客观层（CI 自动化）**：
1. 固定场景集：色卡场景（D50/D65/A 标准光各一）+ 真实场景（肤色/蓝天/植被/混合光/夜景）+ 合成 ramp（对角、hue 环、暗部平坦场）。
2. 指标阈值建议：ΔE00 按区域分桶——肤色桶 avg ≤ 1.5、全景 avg ≤ 2.0（对齐 FADGI 复制级）、ΔEz 看大色差尾部、Δhue 方向图人工抽查；平滑性指标（banding 计数）对合成 ramp 为硬阈值。
3. 每次拟合变更全量回归，报告随 diff 发布。

**主观层（psychophysical 盲评）**。ISO 20462-2:2005 明确 paired comparison 是图像质量评估"可靠性与准确性最高"的方法，并给出 triplet comparison 变体【一手标准页，https://www.iso.org 】；配对结果可换算成 JOD（Just Objectionable Difference）尺度【二手，ResearchGate 上 JOD 缩放方法论文】。协议设计参考点：A/B 随机呈现、双盲、≥10 名观察者、经校准的标准显示器、未知来源标识；另可引入 memory matching（对胶片风格"凭记忆匹配"正是胶片模拟的本质任务，CIE 外观研讨会见此范式【二手，https://files.cie.co.at 】）。RIT 系文献（Braun 等跨媒体匹配实验）提供成熟范式【二手】。

**对本项目的启示**：客观回归守住"不发翻车版本"，盲评守住"不发难用版本"；两者都进发版 checklist。盲评用 JOD 汇总而非"喜欢率"，才能跨版本追踪。

---

## 参考来源汇总（按节）

1. Gupta et al., Building Accurate and Smooth ICC Profiles by Lattice Regression (CIC 2009) — http://www.mayagupta.org/publications/2009-CIC-Smooth-LUTsWithAnimation.pdf
2. TensorFlow Lattice — https://www.tensorflow.org/lattice ，https://github.com/tensorflow/lattice
3. OCIO Using OCIO 指南（ociolutmatsolve/ociolutimage/ociobakelut）— https://opencolorio.readthedocs.io/en/latest/guides/using_ocio/using_ocio.html
4. colour-science — https://github.com/colour-science/colour ；colour.difference（ΔEz）— https://colour.readthedocs.io/en/master/colour.difference.html
5. LUTCalc — https://github.com/cameramanben ，https://cameramanben.github.io
6. Image-Adaptive-3DLUT (TPAMI 2022) — https://github.com/HuiZeng/Image-Adaptive-3DLUT ；CLUT-Net — https://github.com/Xian-Bei/CLUT ；SepLUT (ECCV 2022)；SA-3DLUT (ICCV 2021)；NILUT (arXiv 2023)
7. Jing et al., Neural Style Transfer: A Review — https://arxiv.org/abs/1705.04058 ；延伸：arXiv:2401.17109、arXiv:2506.19278
8. darktable-chart 教程 — https://pixls.us 、https://jo.dreggn.org 、https://docs.darktable.org
9. RawTherapee Film Simulation 集合来源自述 — https://discuss.pixls.us/t/how-was-the-rawtherapee-film-simulation-collection-created-and-thanks/34265
10. Leeming LUT Pro — https://www.leeminglutpro.com/
11. VariCam LUT Library — https://pro-av.panasonic.net ；Fujifilm F-Log LUT/IDT — https://www.fujifilm-x.com
12. t3mujinpack (CC0) — https://github.com/t3mujinpack/t3mujinpack ；bastibe 面板 (MIT) — https://github.com/bastibe/Darktable-Film-Simulation-Panel
13. 色卡对比：SG vs CC — https://swppusa.com ("Macbeth at 40")；MDPI 2021 (SG 特征化) — https://www.mdpi.com
14. 色差：Mirjalili & Luo 2019 (ΔENS) — https://arxiv.org/abs/1904.11293 ；FADGI ΔE00≤2 讨论（二手）；ISO 20462-2:2005 — https://www.iso.org
15. 3D LUT Creator — https://3dlutcreator.com/ （ACHV 无公开算法文档）

## 待办（下一步验证）

- [ ] 逐条下载/阅读上表标"待核"的许可证原文（RawTherapee 集合、VariCam EULA、Fujifilm 下载协议）。
- [ ] 阅读 lattice regression TIP 2014 全文正则项细节，复现小规模实验。
- [ ] 用 colour-science Finlayson 2015 与自写 lattice 拟合器对同一参照对出 ΔE 对比报告。
