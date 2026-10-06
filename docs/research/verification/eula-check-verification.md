# 验证① 记录：富士/松下官方 LUT 包许可核对

日期：2026-10-03。核对样本存于 `F:\rawlab\eula-check\`（原始 ZIP 未改动）。

## 样本

| 厂商 | 文件 | 来源 |
|---|---|---|
| Fujifilm | `gfx100II-3d-lut-v100.zip`（15 文件：F-Log/F-Log2/F-Log2C × ETERNA/ETERNA-BB/WDR/FLog 技术转换，33³） | fujifilm-x.com/en-us/support/download/lut/ |
| Fujifilm | `x-h2s-3d-lut-v100.zip`（同构 15 文件） | 同上 |
| Panasonic | `VLog_to_V709_forV35_EN.zip`（.cube 33³ / .txt Quantel / .vlt 相机体载入三格式，2015-2016 版） | av.jpn.support.panasonic.com/support/global/cs/dsc/download/lut/index.html |

## 结论：包内均无独立 EULA，许可由站点级条款覆盖

1. **富士包**：仅含 LUT `.cube` 与 overview PDF（F-Log2_LUT_overview_Ver.1.1E.pdf），PDF 全文只讲用途与类型，**无许可条款**。`.cube` 文件头仅 5 行元信息（`#FUJIFILM Corporation`、`#Gamma:F-Log2 to ETERNA`、`#Gamut:F-Gamut to ITU-R BT.709`、版本号）——**编码声明在文件头里，这对我们 P0 的导入器是好消息：这类 LUT 可自动识别**。
2. **富士站点条款**（fujifilm.com/terms_of_use/，原文核对）："download content **for personal, noncommercial purposes**… users are prohibited from **modifying or altering**, etc., content downloaded or printed out from the Website."
3. **松下包**：仅 Readme 一句技术说明（"Output is in legal range only"），无 EULA。`.cube` 头只有一行 `# Vlog_to_V709_forV35_ver100.cube by panasonic`（无编码声明——导入器需按文件名/用户确认）。
4. **松下站点条款**（av.jpn.support.panasonic.com/support/global/cs/terms-of-use.html，2022-04-01 版，原文核对）："Duplication, public transmission, **distribution, modification**, deletion, or reproduction of Content … **is prohibited** … except for printing or storage **for personal Use**."

## 对 RawLab 的裁定（确认综合论证的许可红线）

- **不可随应用分发**两家官方 LUT：站点条款明确禁止复制/分发/修改，且授权范围仅"个人使用"。04 报告"默认不可再分发"的保守判断成立。
- **可行路径**：官方 LUT 仅作为（a）**离线拟合参照**（开发机私有，产品不含）、（b）导入器对用户自备文件的**自动识别目标**（富士 `#Gamma:#Gamut` 头格式应写入解析器；松下按文件名模式识别）。
- **意外收获**：富士官方已经发布**以胶片模拟命名的风格 LUT**（`FLog2_FGamut_to_ETERNA_BT.709` 等，2025-11 更新）——意味着"P2 首批富士风格包"可以引导用户从官方免费获取自备文件后导入，规避授权问题且色彩权威。
- **链接勘误**（修正 02 报告 §5 与 04 报告 §5 中的松下链接）：VariCam LUT Library 旧页已 404；现行官方页为 [av.jpn.support.panasonic.com/support/global/cs/dsc/download/lut/index.html](https://av.jpn.support.panasonic.com/support/global/cs/dsc/download/lut/index.html)（GH5 系起的相机 V-Log LUT）与 [pro-av.panasonic.net/en/cinema_camera_varicam_eva/support/lut/](https://pro-av.panasonic.net/en/cinema_camera_varicam_eva/support/lut/)（电影机）。

## 复现

`F:\rawlab\eula-check\` 内含原始 ZIP 与页面快照（pana_dsc.html / pana_terms.html / fuji_terms.html / fuji_lut_page.html）。
