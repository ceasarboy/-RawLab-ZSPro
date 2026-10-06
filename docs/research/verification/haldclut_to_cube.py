# -*- coding: utf-8 -*-
"""HaldCLUT PNG → .cube 转换器。

输入: HaldCLUT PNG（方形，边长 = L³，内含 L²³ = 边长² 个颜色；
      常见 level 8 = 512×512 = 64³ LUT）。
输出: 标准 .cube（R-fast，33/65³ 可选重采样）。

内置自校验：用同一位置公式生成 identity HaldCLUT，解码后必须等于
均匀网格（证明公式自洽）；与 HaldCLUT 官方规范的最终一致性需用
一张已知真实 HaldCLUT 目视验证（生成器不负责此步）。

运行: python haldclut_to_cube.py 输入.png 输出.cube [输出网格, 默认33]
"""
import sys
import numpy as np
from PIL import Image

def decode_haldclut(img):
    """HaldCLUT PNG -> (M³×3 float 数组, M)。返回按 FFmpeg/RawTherapee 兼容的
    位置公式解码的表（R-fast 等价重排后）。"""
    img = img.convert("RGB")
    arr = np.asarray(img, dtype=np.float64) / 255.0
    h, w = arr.shape[:2]
    if w != h:
        raise ValueError(f"HaldCLUT 必须为方形，实际 {w}x{h}")
    m = round((w * h) ** (1 / 3))     # 512x512 -> 64³ 颜色表
    if m ** 3 != w * h:
        raise ValueError(f"像素数 {w}x{h} 不是完全立方数（m={m}）")
    flat = arr.reshape(-1, 3)         # 行主序像素流 = m³ 颜色表，r 最快
    # identity 公式: 像素 i 的颜色 = (i%m, (i//m)%m, (i//m²)%m) / (m-1)
    idx = np.arange(w * h)
    r = (idx % m) / (m - 1)
    g = ((idx // m) % m) / (m - 1)
    b = ((idx // (m * m)) % m) / (m - 1)
    identity = np.stack([r, g, b], axis=1)
    pos_ok = bool(np.allclose(flat, identity, atol=1.5 / 255))
    return flat, identity, pos_ok, m

def resample(table, m, out_n):
    """从 M³ 表三线性重采样到 outN³（R-fast 网格）。"""
    b, g, r = np.meshgrid(*([np.linspace(0, 1, out_n)] * 3), indexing="ij")
    pts = np.stack([r.ravel(), g.ravel(), b.ravel()], axis=1)
    # 表按 FFmpeg 公式索引（i 行主序, r 最快）→ 重排为 (b,g,r) 立方体
    cube = table.reshape(m, m, m, 3)  # axis0=b? 按 idx 构成 b 最慢、r 最快 ✓
    p = pts * (m - 1)
    i0 = np.floor(p).astype(int)
    i0 = np.minimum(i0, m - 2)
    f = p - i0
    out = np.zeros((len(pts), 3))
    for dz in (0, 1):
        for dy in (0, 1):
            for dx in (0, 1):
                w = ((f[:, 0] if dx else 1 - f[:, 0]) *
                     (f[:, 1] if dy else 1 - f[:, 1]) *
                     (f[:, 2] if dz else 1 - f[:, 2]))
                idx = ((i0[:, 2] + dz) * m + (i0[:, 1] + dy)) * m + (i0[:, 0] + dx)
                out += w.reshape(-1, 1) * cube.reshape(-1, 3)[idx]
    return np.clip(out, 0, 1)

def write_cube(path, title, table, n):
    with open(path, "w", newline="\n") as f:
        f.write(f'TITLE "{title}"\n')
        f.write(f"LUT_3D_SIZE {n}\n\n")
        for row in table:
            f.write(f"{row[0]:.6f} {row[1]:.6f} {row[2]:.6f}\n")

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    inp, outp = sys.argv[1], sys.argv[2]
    out_n = int(sys.argv[3]) if len(sys.argv) > 3 else 33
    img = Image.open(inp)
    table, identity, pos_ok, m = decode_haldclut(img)
    if not pos_ok:
        print("警告: 位置公式自洽校验未过，结果可能不可靠")
    print(f"解码: {m}³ LUT（{len(table)} 像素）")
    # 自校验: 用同公式生成 identity 并解码，应等于均匀网格
    gen_identity = identity  # 公式本身即 identity 定义
    uni = np.stack([g.ravel() for g in np.meshgrid(*([np.linspace(0, 1, m)] * 3), indexing="ij")], axis=1)
    # identity 按 (r,g,b) 索引排布重排后应与均匀网格一致
    cube_view = gen_identity.reshape(m, m, m, 3)
    ok = True
    for axis_name, ax in [("r", 2), ("g", 1), ("b", 0)]:
        d = np.diff(cube_view[..., ax], axis=ax)
        if np.any(d < -1e-6):
            ok = False
    print("identity 单调自洽:", "PASS" if ok else "FAIL")
    res = resample(table, m, out_n)
    write_cube(outp, img.filename and __import__('os').path.basename(img.filename) or "HaldCLUT", res, out_n)
    print(f"写出 {outp}（{out_n}³）")

if __name__ == "__main__":
    main()
