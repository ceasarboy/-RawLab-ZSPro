# RawLab ZSPro Android v1.6.8

基于 RawLab（MIT）的增强分支首个公开发布版本。品牌更名 **RawLab ZSPro**，版本号从 v0.1.0 跳至 v1.6.8，以区分上游与分支的演进节奏。

## 本版本内容 / What's in this release

### 新功能
- **LUT 库系统**：三模式 LUT 导入（显示编码 / F-Log2 / 自动识别）、批量导入、每张 LUT 的参考场景缩略图。
- **松下 STD 底座适配**：官方 STD→V-Log 映射数据驱动的分层适配链（灰保持 1D 曲线 + look 查表），无色彩断层、无插值彩噪；导入时可选底座，长按可随时切换中性 / STD。
- **双 LUT 组合器**：A/B 双槽、独立强度、实时预览，烘焙存盘为新 LUT。
- **十项调整**：曝光 / 对比度 / 饱和度 / 色调曲线 / 锐化 / 高光 / 阴影 / 色温 / 色调 / 胶片强度。
- **缩放检视**：双指 1–8× 质心锚定缩放与拖动。

### 画质
- **四面体插值**（替代三线性）：CPU / GLES / Direct3D 11 / Metal 四条路径同步，饱和区色相不漂移；格点输出与源数据逐位一致（宿主穷举验证：4 条 256 级 ramp × 4 张适配 LUT、格点恒等 65³ 零偏差、饱和度阶梯无振荡）。

### 修复
- **LibRaw 升级 0.21.5 → 0.22.0**：修复松下 DC-S9（及同代新机型）RAW 全图渲染白底紫点的问题（旧版解压错位），相机白平衡与色彩矩阵元数据恢复完整。
- **温度白平衡兼容**：LibRaw 缺少相机矩阵的机型（如 OM-5）回退 sRGB 矩阵并锚定 6500K，色温滑杆可用；有矩阵机型保持精确路径（如 Z7 II 实测 5329K）。
- LUT id 路由修复（旧 `lut-<ts>` id 自动补 `user:` 前缀）、`inputIsFLog2` 前缀判定、组合器 busy 卡死、导入重活移出主线程等。

### 合规
- 应用内新增「开源许可」完整声明：RawLab MIT、LibRaw 0.22.0（LGPL-2.1/CDDL 双许可 + 官方源码地址与 SHA-256）、zlib 1.3.1、stb、随包松下 STD 数据的个人使用边界。
- 版本号 `versionName=1.6.8`、`versionCode=168`；应用名与权限文案更名 RawLab ZSPro。`applicationId` 保持 `com.rawlab.android`（覆盖升级现有 debug 安装，保留应用内已导入 LUT）。

## 升级 / Upgrade

- 从旧 debug 包覆盖安装：直接安装即可，应用私有数据（已导入 LUT、缩略图缓存）保留。
- 从上游官方 v0.1.0（正式签名）升级：签名不同无法覆盖，需先卸载官方版（会清除其应用内数据），再安装本包。

## 验证 / Verification

- 宿主回归：core_tests 51 PASS、white_balance_tests 12 PASS、gpu_photo_tests 10 PASS（真实 D3D11 编译，CPU/GPU max DN 0-1）、acceleration_tests 三家 RAW（Nikon Z7 II / Panasonic DC-S9 / OM-5）× 42 PASS、raw_reuse_tests 三家 × 17 PASS。
- 三张 S9 RW2 宿主渲染出图正常（凌霄花、北海白塔等）。
- Android 单元测试与 lint 在改动后全绿；真机验证（S9 RW2 出图、STD 观感、组合器）由使用者完成。

## 已知限制 / Known limitations

- 解码 S9 RW2 时 LibRaw 仍输出一条无害的 `data corrupted` 内部告警（不在像素流中）。
- RAW 解包、去马赛克与文件编码仍在 CPU；GPU 加速覆盖显示管线。
- 随包的松下 STD→V-Log 数据仅限个人使用，不得单独再分发。
