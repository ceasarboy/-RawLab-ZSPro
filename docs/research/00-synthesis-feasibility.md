# 综合论证：RawLab 自定义 LUT 功能的可行性（基于文献调研）

> 本文是 `docs/research/` 下四份调研报告的合成与可行性裁决。四份报告均以一手来源（官方规范、白皮书、论文、源码）为主证：
> - [01 色彩科学与 LUT 数学](01-color-science-lut-math.md)（传递函数/色域/LUT 表示/插值/metamerism）
> - [02 专业调色软件实践](02-pro-grading-software-practice.md)（DaVinci Resolve 手册原文/FilmLight/ACES/OCIO）
> - [03 RAW 风格体系](03-raw-profile-systems.md)（DNG 1.7 规范/DCP/LibRaw 边界/RawTherapee/darktable）
> - [04 LUT 拟合方法学](04-lut-fitting-methods.md)（算法谱系/工具/采集协议/翻车点/验证体系）
> 日期：2026-10-03。前序方案：[proposal-panasonic-style-lut-android.md](../proposal-panasonic-style-lut-android.md)。

---

## 一、裁决先行

**项目可行，但可行性与"怎么支持 LUT"强绑定。** 文献给出的不是"能不能做"的答案（能，且有成熟科学支撑），而是一个更严格的条件：**LUT 只能作为"风格层"进入管线，且必须携带显式的输入编码声明**。做到这两点，这就是一个有理论依据的精品功能；做不到（例如把用户 LUT 直接怼在任意位置、不问编码），那就是一个会因"同一 LUT 效果和达芬奇不一样"被用户抛弃的功能——而这恰恰是跨软件 LUT 不一致问题的头号成因（02 报告 §5）。

RawLab 现有管线意外地接近正确形态（03 报告对照 RawTherapee/darktable 的结论）：我们的中性 log-logistic 显示曲线之后再挂 LUT，与 darktable lut3d 默认位置（filmic 之后）和 DNG LookTable 的规范位置（曝光后、色调曲线前）同构。**主要缺的不是 LUT 能力，是"声明编码"与"分层语义"。**

---

## 二、从理论推导出的六条架构原则（论证核心）

### 原则 1：语义分层——"相机底色"与"创作风格"必须解耦

Adobe DNG 1.7 规范用五层表示色彩：色度学矩阵（物理标定，双光源插值）→ HueSatMap（标定残差，3D 表）→ 曝光补偿 → **LookTable（创作风格层）** → ProfileToneCurve → 输出矩阵（03 报告 §1，规范原文位置定义）。相机光谱响应差异造成的色偏，矩阵（CCM）只能做线性最优解，残余必须由 3D 表吸收；而"风格"是独立的再上面一层。**推论：我们若宣称"富士风格"，LUT 只对齐 ④ 层；①② 层（相机矩阵 + 标定残差）的粗糙度决定了风格的上限。LibRaw 只提供矩阵层（03 报告 §3），无 3D 表结构——这是现有底座最大的一块能力缺口，也是 ACR/LR 与我们的真实差距所在。**

### 原则 2：LUT 是格式，公式是本体

四家专业体系异口同声（02 报告）：Resolve 手册把"LUT 驱动"与"色彩管理"列为两条并列路线并点名 LUT 三缺陷（钳位越界、插值不一致、每对空间需维护一份文件）；FilmLight 十三年一以贯之"GPU 直接求值公式，不削顶且更精确"；ACES 2.0 把 Output Transform 做成纯算法（tone scale + chroma compression + gamut compression），AMF 元数据甚至**否决 .cube**（look 必须用 CLF）；OCIO v2 内部把一切编译为算子、LUT 只是其中一种。**推论：内部管线应维持"声明编码的参数化算子链"（矩阵/log/CDL 类），LUT 只在两个边缘出现——入口（把用户 LUT 翻译成内部链的一段）与出口（导出 .cube/CLF）。** 这同时解释了为什么本方案绝不能"把整条渲染烘成一张大 LUT"。

### 原则 3：输入编码三元组必须强制声明

.cube 格式从 IRIDAS 时代起就**没有**输入/输出色彩空间与传递函数字段（01 报告 §3；02 报告 §5）——"这个 LUT 吃 V-Log 还是 Rec.709"全靠文件名约定（Panasonic 官方 LUT 把编码写进文件名 `VLog_to_V709...` 即行业现状）。跨软件不一致的五个机制（编码假设、levels 缩放、插值差异、色彩管理叠加、钳位与位深）中第一个是根因。**推论：导入器必须要求或推断后强制确认"传递函数 × 色域 × 数据 levels"，拒绝无声明载入；应用点固定在"归一化到工作空间之后"。**

### 原则 4：物理边界——metamerism 决定"LUT 与相机解耦"的适用域

各品牌 CFA 光谱响应违反 Luther 条件且差异巨大（01 报告 §5，CIE/ISO 17321 定义），同一 LUT 直接套在不同底色素材上必然不一致——这是物理，不是 LUT 写得不好。ACES 的解法是**按相机系统定义 Input Transform** 先归一。**推论：产品语义 = "用户 LUT 挂在统一工作空间之后"；同时我们提供按机型的标准解码（F-Log2/V-Log/S-Log3 等官方逆函数，01 报告 §1 已核全部数学形式）；文档明示跨机型差异的原因。这与松下生态的用户预期完全一致。**

### 原则 5：LUT 不承担色域压缩与高光滚降

ACES 2.0 弃用 LUT 化 Output Transform 的官方理由：参数化表示"更精确、无 3D LUT、消除 gamut 边界裁切感伪影"；色域压缩需要方向感知、随亮度色度连续变化的非线性映射，固定晶格只能离散采样它（01 报告 §6）。**推论：管线的 gamut clip / highlight 滚降必须是 LUT 之前的参数化模块；用户 LUT 只接收已压域的输入。这从源头规避"LUT 高光裁切"类投诉。**

### 原则 6：拟合生产线 = "lattice 回归 + 约束"，验证 = 客观回归 + 盲评

- 主线算法：lattice regression + 平滑/单调正则（Gupta CIC2009/TIP2014；约束思路可借 TensorFlow Lattice），以 colour-science 多项式（Finlayson 2015）做基线、OCIO `ociolutmatsolve` 做矩阵级交叉验证；深度方法只抄约束不抄模型（04 报告 §1）。
- 数据：合成参照对（同 RAW 双路渲染，像素对齐）为主干，实拍对补充；采集协议文档化（Leeming 式——输入不标准拟合必翻车）。
- 验证：ΔE00 分桶（肤色 ≤1.5、全景 ≤2.0 对齐 FADGI）+ ΔEz 大色差尾部 + per-hue 方向图 + 合成 ramp 的 banding 硬阈值；主观层按 ISO 20462-2 paired comparison/JOD 盲评（04 报告 §4、§6）。
- 翻车点清单（蓝天 hue twist/肤色漂移/高光去饱和/阴影噪声放大/网格色带/非单调）全部转为 CI 合成图回归用例（04 报告 §4）。

---

## 三、分维度可行性

| 维度 | 判断 | 依据 |
|---|---|---|
| **理论** | ✅ 成熟科学 | log 传递函数有官方数学定义且经 colour-science 逐一数值复核（01 §1）；插值有 30+ 年文献（Kasson & Plouffe 1992）；拟合有 CIC/TIP 论文线（04 §1） |
| **工程** | ✅ 需分层重构，无需发明新科学 | GLES 端 sampler3D 与网格尺寸无关；tetrahedral 插值算法量小适合 NEON（01 §4）；OCIO v2 的"算子内联 GLSL + LUT 走纹理 + 动态参数走 uniform"是移动端直接参考（02 §4.3）；缺口 = 分层 schema（原则 1）+ 编码声明 UI（原则 3）+ 参数化 gamut/tone 模块（原则 5） |
| **数据与许可** | ⚠️ 有清晰合规路径 | 富士官方 LUT 默认不可再分发（页面无许可条款，需核包内 EULA）；松下 VariCam LUT Library 免费但 EULA 待核；**可再分发的只有 CC0/MIT 社区重建**（t3mujinpack CC0、bastibe MIT）；正道 = 自产拟合 + 官方 LUT 仅作离线参照；商标风险：文案描述特征不冒用品牌名（04 §5） |
| **市场定位** | ✅ 精品逻辑成立 | 专业管线普遍不认真服务"RAW+风格 LUT 的摄影场景"（Resolve 是视频工具、LR 不收 LUT、darktable 是极客向）；小众爱好愿意为"契约+验证+跨机型一致性"付费——这恰是我们能用 02/03 报告的行业共识建立的可验证护城河 |

**反可行性（诚实声明）**：三条做不到就要砍范围——①复刻富士机身胶片模拟：权威实现在闭源固件 ISP（X RAW Studio 借机身 SoC，03 §5），无官方数值，只能社区近似，宣传必须用"观感参考"措辞；②用 LUT 实现 HDR/宽域全管线：精度与钳位天花板在（01 §3、02 §1.2），必须守原则 5 的分界；③追求"和某品牌 JPEG 逐像素一致"：metamerism + 引擎局部处理（自适应 WB/降噪/DRT）决定了静态 LUT 的物理上限（04 §3）。

---

## 四、对原 P0–P3 计划的修订

原方案（proposal）方向不变，按调研结论强化四处：

1. **P0 增加"编码声明"为第一优先级**：导入 .cube 时强制三元组确认（传递函数×色域×levels），存入应用元数据；无声明拒绝载入或标红警示。插值直接实现 **tetrahedral**（Resolve 官方画质排序），格点 half-float 存储。
2. **P0 的"通用 LUT 模式"细化为两个挂载点**（对齐 Resolve 五槽位模型的简化版）：`look LUT`（归一化+gamut 压域之后，即现有中性曲线之后）与 `log decode LUT`（解码层，供 V-Log/S-Log3 类素材专用，官方逆函数内置）。两者不可混淆（03 §4 darktable 对 F-Log 类 LUT 的位置警告同款）。
3. **新增 P0.5：参数化 tone/gamut 模块**（原则 5）——参考 ACES 2.0 的 tonescale/chroma compress/gamut compress 结构做简化实现，置于 look LUT 之前；这同时把"对比度/高光滚降"从 LUT 里解放出来，回应用户对松下 app 调节弱的抱怨。
4. **P2 拟合生产线直接按 04 报告 §6 建**：CI 回归（合成 ramp 场景集 + ΔE 三件套 + banding 硬阈值）与 ISO 20462 盲评双轨，作为"精品"的发版门槛；lattice 拟合器先复现 Gupta 论文，与 colour-science/OCIO 交叉验证。

**实施顺序修正**：原 P0（导入与管理 UI）之前插入"P0− 架构分层"：把现有 `sony2fuji_request` 的处理顺序固化为"矩阵 → 曝光 → gamut 压域 → 中性显示曲线 → look LUT → 输出"并写进 color-contract.md（对应 DNG 五层的位置语义）。这是纯文档+管线顺序确认的工作，但决定了后面所有功能挂在哪里。

---

## 五、验证清单（2026-10-03 已执行，记录见 `verification/` 目录）

- [x] **富士/松下官方 LUT 包 EULA 核对** → [eula-check-verification.md](verification/eula-check-verification.md)：包内均无独立 EULA；站点条款均"仅个人使用、禁止复制/分发/修改"→ 红线确认（不可随包分发，仅离线拟合参照/用户自备导入）。意外收获：富士已官方发布以胶片模拟命名的风格 LUT 且 `.cube` 头带编码声明（`#Gamma/#Gamut`），可作导入器自动识别目标。
- [x] **lattice regression 实验 + 多项式基线对比** → [lattice-fit-verification.md](verification/lattice-fit-verification.md)：17³ lattice 精度最优（随机/色卡 mean ΔE00 = 0.034/0.022）但 λ=0 时中性带 34 处单调违例——确认生产拟合器必须带单调/平滑约束、ΔE 均值必须配 ramp 检查。勘误：Finlayson 2015 根多项式无常数项，不能表达黑位抬升，仅适用于保中性的标定类变换，通用基线应为普通 deg-3 多项式。
- [x] **V-Log/S-Log3/F-Log2 解码与 colour-science 对拍** → [log-decode-verification.md](verification/log-decode-verification.md)：三条曲线与官方实现全域最大偏差 ≤2.8e-16（机器精度）、8 项厂商锚点全 PASS。勘误：V-Log 解码分支点是 y=0.181（非 0.125）；厂商码值表有 legal/full range 两种口径；F-Log2 与 F-Log 必须分别实现（colour-science 无 F-Log2，按官方 Data Sheet 自实现并核锚点）。C++ 规格与单元测试锚点已备。
- [x] **四面体 vs 三线性性能预算** → [interp-performance-verification.md](verification/interp-performance-verification.md)：CPU 代理下四面体仅慢 22%（693 vs 847 MP/s）；GPU 端采样 4 vs 8 次，有望持平或更快。恒等 LUT 精确性测试在开发中抓出两处实现错误（基准点写错、排序分支写错）——该测试必须进 CI。真机帧率数字待接入设备后回填。
- [ ] （新增待办）真机 tetrahedral 基准回填：用 `RawLabAndroid/tests/gpu_smoke.cpp` 基础设施挂载恒等精确性 + 双方法对拍 + 计时。
