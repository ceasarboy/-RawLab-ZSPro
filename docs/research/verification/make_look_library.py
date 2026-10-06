# -*- coding: utf-8 -*-
"""RawLab 中性基线风格库生成器。

所有 look 定义在"中性显示编码"（log-logistic 视图曲线 + sRGB）上，
与 App 的 DISPLAY 模式输入契约一致。全部通过主对角单调检查。

运行: python make_look_library.py [输出目录]
"""
import numpy as np
import os, sys

N = 65

def s2l(x):
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)

def l2s(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * x ** (1 / 2.4) - 0.055)

def lgg(x, lift, gamma, gain):
    x = np.clip(x, 0.0, 1.0)
    return np.clip(lift + (gain - lift) * np.power(x, 1.0 / gamma), 0.0, 1.0)

def sat(rgb, amount):
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722]).reshape(3, 1)
    return lum + (rgb - lum) * amount

M = lambda *v: np.array(v).reshape(3, 3)

LOOKS = {
    "kodak-portra": ("Portra 暖调人像", lambda x:
        sat(np.stack([lgg(c, l, gm, gn) for c, (l, gm, gn) in zip(
            l2s(np.clip(s2l(x) @ M(1.06, 0.02, -0.05, 0.0, 0.97, 0.02, -0.03, 0.0, 1.02).T, 0, 1)).T,
            [(0.04, 0.90, 0.965), (0.05, 0.92, 0.96), (0.07, 0.96, 0.955)])], axis=1), 0.86)),

    "eterna-cold": ("Eterna 冷调电影", lambda x:
        sat(np.stack([lgg(c, 0.02, 1.04, 0.985) for c in
            l2s(np.clip(s2l(x) @ M(0.97, 0.03, 0.02, 0.01, 0.99, 0.0, 0.02, 0.01, 0.96).T, 0, 1)).T],
            axis=1), 0.80)),

    "teal-orange": ("青橙电影", lambda x:
        sat(np.stack([lgg(c, 0.0, gm, 1.0) for c, gm in zip(
            l2s(np.clip(s2l(x) @ M(1.10, -0.04, -0.06, -0.03, 1.00, -0.02, -0.05, -0.01, 1.09).T, 0, 1)).T,
            (0.92, 1.0, 1.14))], axis=1), 1.12)),

    "bleach-bypass": ("漂银高对比", lambda x:
        np.clip(0.35 * x + 0.65 * sat(np.stack([lgg(c, 0.0, 0.62, 1.02) for c in x.T], axis=1), 0.55), 0, 1)),

    "bw-classic": ("经典黑白", lambda x:
        np.repeat(lgg(l2s(np.clip(s2l(x) @ np.array([0.10, 0.20, 0.10]) / 0.40, 0, 1)), 0.01, 0.95, 0.99).reshape(-1, 1), 3, axis=1)),

    "bw-high": ("高对比黑白", lambda x:
        np.repeat(lgg(l2s(np.clip(s2l(x) @ np.array([0.12, 0.22, 0.06]) / 0.40, 0, 1)), 0.0, 0.72, 1.0).reshape(-1, 1), 3, axis=1)),

    "vintage-fade": ("复古褪色", lambda x:
        sat(np.stack([lgg(c, l, gm, gn) for c, (l, gm, gn) in zip(
            l2s(np.clip(s2l(x) @ M(1.02, 0.04, -0.02, -0.01, 1.00, 0.02, 0.03, -0.02, 0.92).T, 0, 1)).T,
            [(0.06, 1.02, 0.96), (0.07, 1.00, 0.95), (0.05, 0.94, 0.90)])], axis=1), 0.78)),

    "landscape-punch": ("风光通透", lambda x:
        sat(np.stack([lgg(c, 0.0, gm, 1.0) for c, gm in zip(
            l2s(np.clip(s2l(x) @ M(1.05, -0.02, -0.03, -0.02, 1.03, -0.01, -0.02, -0.01, 1.06).T, 0, 1)).T,
            (0.94, 1.02, 1.08))], axis=1), 1.18)),

    "portrait-soft": ("人像柔粉", lambda x:
        sat(np.stack([lgg(c, 0.03, g_, 0.98) for c, g_ in zip(
            l2s(np.clip(s2l(x) @ M(1.03, 0.02, -0.01, 0.0, 0.98, 0.03, 0.0, 0.02, 1.0).T, 0, 1)).T,
            (1.06, 1.02, 1.0))], axis=1), 0.95)),

    "cross-process": ("反转片交叉冲洗", lambda x:
        np.stack([lgg(x[:, 0], 0.02, 0.85, 1.01),
                  lgg(x[:, 1], 0.015, 1.00, 1.0),
                  lgg(x[:, 2], 0.05, 1.18, 0.98)], axis=1)),

    "cinematic-shadow": ("夜色冷调", lambda x:
        sat(np.stack([lgg(c, l, gm, gn) for c, (l, gm, gn) in zip(
            l2s(np.clip(s2l(x) @ M(0.97, 0.0, 0.05, 0.0, 0.98, 0.0, 0.04, 0.0, 0.97).T, 0, 1)).T,
            [(0.02, 1.06, 0.98), (0.025, 1.02, 0.98), (0.06, 0.98, 0.985)])], axis=1), 0.92)),

    "sunset-gold": ("暮色金", lambda x:
        sat(np.stack([lgg(c, 0.0, gm, gn) for c, (gm, gn) in zip(
            l2s(np.clip(s2l(x) @ M(1.09, 0.0, -0.04, 0.01, 1.0, 0.0, -0.02, 0.0, 0.94).T, 0, 1)).T,
            [(0.98, 1.0), (1.0, 1.0), (1.08, 0.995)])], axis=1), 1.06)),
}

def write_cube(path, title, table):
    with open(path, "w", newline="\n") as f:
        f.write(f'TITLE "{title}"\n')
        f.write("# RawLab neutral-baseline starter look (display encoding).\n")
        f.write(f"LUT_3D_SIZE {N}\n\n")
        for row in table:
            f.write(f"{row[0]:.6f} {row[1]:.6f} {row[2]:.6f}\n")

def mono(rgb):
    t = rgb.reshape(N, N, N, 3)
    tol = -1e-3
    return (np.all(np.diff(t[:, :, :, 0], axis=2) >= tol) and
            np.all(np.diff(t[:, :, :, 1], axis=1) >= tol) and
            np.all(np.diff(t[:, :, :, 2], axis=0) >= tol))

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    b, g, r = np.meshgrid(*([np.linspace(0, 1, N)] * 3), indexing="ij")
    grid = np.stack([r.ravel(), g.ravel(), b.ravel()], axis=1)
    ramp = np.stack([np.linspace(0, 1, 1024)] * 3, axis=1)

    ok = 0
    for key, (cn, fn) in LOOKS.items():
        table = np.clip(fn(grid), 0, 1)
        if not mono(table):
            print(f"SKIP {key}: 主单调检查失败")
            continue
        write_cube(os.path.join(out, f"{key}.cube"), cn, table)
        drift = np.abs(fn(ramp) - ramp).max()
        ok += 1
        print(f"{key}: OK (中性斜率最大漂移 {drift:.4f})")
    print(f"生成 {ok}/{len(LOOKS)} -> {os.path.abspath(out)}")

if __name__ == "__main__":
    main()
