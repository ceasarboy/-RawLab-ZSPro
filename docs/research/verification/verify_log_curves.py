# -*- coding: utf-8 -*-
"""验证③：RawLab 拟内置的 log 解码函数 vs colour-science 官方实现 + 厂商数据表锚点对拍。

公式来源：docs/research/01-color-science-lut-math.md §1（各厂商白皮书）。
分支结构（log 主体 + 线性趾部 + 负值余量）参照 colour-science 0.4.7 实现
（panasonic_v_log.py CONSTANTS_VLOG cut2=0.181；sony.py 分支点 CV 171.2102946929）。
锚点：10-bit 码值（full range, 0-1023）。
运行：python verify_log_curves.py
"""
import numpy as np
from colour.models.rgb.transfer_functions.sony import log_encoding_SLog3, log_decoding_SLog3
from colour.models.rgb.transfer_functions.panasonic_v_log import log_encoding_VLog, log_decoding_VLog
from colour.models.rgb.transfer_functions.fujifilm_f_log import log_decoding_FLog

def slog3_decode(codes):
    """S-Log3 逆函数（S-Gamut3 白皮书）。CV≥171.21 走 log，以下走线性趾（含负值余量）。"""
    cv = np.asarray(codes, dtype=float)
    return np.where(
        cv >= 171.2102946929,
        (10 ** ((cv - 420.0) / 261.5)) * 0.19 - 0.01,
        (cv - 95.0) * 0.01125 / (171.2102946929 - 95.0),
    )

def vlog_decode(codes):
    """V-Log 逆函数（VARICAM V-Log/V-Gamut 文档）。y<0.181 走线性趾，以下 log。"""
    y = np.asarray(codes, dtype=float) / 1023.0
    return np.where(y < 0.181, (y - 0.125) / 5.6, 10 ** ((y - 0.598206) / 0.241514) - 0.00873)

def flog2_encode(x):
    """F-Log2 正函数（Fujifilm F-Log2 Data Sheet Ver.1.0，log 主体段）。"""
    x = np.asarray(x, dtype=float)
    y = 0.245281 * np.log10(5.555556 * x + 0.064829) + 0.384316
    return np.clip(y, 0.0, 1.0) * 1023.0

def flog2_decode(codes):
    """F-Log2 逆函数（log 主体段；零点由公式定义于 CV≈94.9≈95）。"""
    y = np.asarray(codes, dtype=float) / 1023.0
    return (10 ** ((y - 0.384316) / 0.245281) - 0.064829) / 5.555556

results = []

def check(name, value, target=None, tol=None):
    if target is None:
        results.append((name, f"{value}"))
    else:
        ok = "PASS" if abs(value - target) <= tol else "FAIL"
        results.append((name, f"{value:.3e} vs 目标 {target:g} (容差 {tol:g}) -> {ok}"))

# ---------- 1. 与 colour-science 全域对拍（CV 0..1023，200k 点） ----------
cv = np.linspace(0.0, 1023.0, 200001)
for tag, ours, ref in [
    ("S-Log3", slog3_decode(cv), log_decoding_SLog3(cv / 1023.0)),
    ("V-Log", vlog_decode(cv), log_decoding_VLog(cv / 1023.0)),
]:
    d = np.abs(ours - ref)
    check(f"{tag} 解码 vs colour-science 全域", f"max|Δ|={d.max():.3e} mean={d.mean():.3e} @CV{cv[d.argmax()]:.1f}")

# 编码->解码 roundtrip（colour 自身基线自洽性）
lin = np.linspace(0.0, 1.0, 200001)
for tag, enc, dec in [
    ("S-Log3", log_encoding_SLog3, log_decoding_SLog3),
    ("V-Log", log_encoding_VLog, log_decoding_VLog),
]:
    d = np.abs(dec(enc(lin)) - lin)
    check(f"{tag} colour 编码->解码 roundtrip（基线自洽）", f"max|Δ|={d.max():.3e}")

# ---------- 2. F-Log2 roundtrip + 与 F-Log 区分 ----------
x = np.linspace(0.0, 1.0, 200001)
d = np.abs(flog2_decode(flog2_encode(x)) - x)
check("F-Log2 自实现 roundtrip", f"max|Δ|={d.max():.3e}（log 段；负值余量由 clamp 承担）")
check("区分 F-Log2 与 F-Log @CV400", f"FLog2={flog2_decode(np.array([400.0]))[0]:.6f} FLog={log_decoding_FLog(np.array([400.0/1023.0]))[0]:.6f}（应显著不同）")

# ---------- 3. 官方锚点（10-bit full range 码值） ----------
anchors = [
    ("S-Log3 0%",  95.0,  0.0,  1e-9),
    ("S-Log3 18%", 420.0, 0.18, 1e-9),
    ("S-Log3 90%", 598.0, 0.9,  1e-3),   # colour 同样解出 0.90084，属 Sony 常数定义的"90%白"
    ("V-Log 0%",   127.875, 0.0,  1e-9),  # 官方零点 y=0.125 精确对应 CV 127.875；"128" 为数据表舍入值
    ("V-Log 18%",  433.0, 0.18, 1e-4),
    ("F-Log2 0%",  95.0,  0.0,  1e-6),   # 厂商常数按 6 位小数舍入所致
    ("F-Log2 18%", 400.0, 0.18, 1e-6),
    ("F-Log2 90%", 570.0, 0.9,  1e-3),
]
for tag, code, expect, tol in anchors:
    fn = {"S": slog3_decode, "V": vlog_decode, "F": flog2_decode}[tag[0]]
    val = float(fn(np.array([code]))[0])
    check(f"锚点 {tag} @CV{code:g}", val, expect, tol)

# ---------- 4. 趾部连续性（分支点两侧无跳变） ----------
check("S-Log3 趾部连续性 @CV171.2102946929", float(np.ptp(slog3_decode(np.array([171.2102946929 - 1e-3, 171.2102946929 + 1e-3])))), 0.0, 1e-6)
check("V-Log 趾部连续性 @CV185.163(=0.181*1023)", float(np.ptp(vlog_decode(np.array([0.181 * 1023 - 1e-3, 0.181 * 1023 + 1e-3])))), 0.0, 1e-6)

print(f"{'项目':<44} 结果")
print("-" * 110)
for name, res in results:
    print(f"{name:<44} {res}")
