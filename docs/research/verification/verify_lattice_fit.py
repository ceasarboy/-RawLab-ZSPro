# -*- coding: utf-8 -*-
"""验证②：参照对拟合 3D LUT 的方法对比实验。

对比三种拟合器在同一组"中性参照对"上的精度/平滑性/单调性：
  A. 3x3+偏置 最小二乘（基线，OCIO ociolutmatsolve 同型）
  B. Finlayson 2015 多项式（colour-science 实现，04 报告推荐的快速基线）
  C. lattice regression + 平滑正则（Gupta CIC2009 主线，本实验自实现）

真值：解析"风格变换"T = 3x3 通道混合 ∘ 逐通道 gamma（光滑、单调、点态，
即合法的 3D LUT 可表达对象）。训练/测试分离；λ 在验证集上选。
运行：python verify_lattice_fit.py
"""
import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spla
import colour

rng = np.random.default_rng(20261003)
K = 17  # lattice 每轴节点数

# ---------- 真值风格变换 ----------
M_GT = np.array([[0.95, 0.06, 0.00],
                 [0.03, 0.93, 0.04],
                 [0.00, 0.09, 0.91]])
GAMMA = np.array([1.06, 0.94, 1.13])
LIFT  = np.array([0.015, 0.000, 0.030])

def ground_truth(x):
    x = np.clip(x, 0.0, 1.0)
    return np.clip(M_GT @ (np.power(x, GAMMA) + LIFT).T, 0.0, 1.0).T

# ---------- 数据 ----------
def train_points():
    grid = np.stack(np.meshgrid(*[np.linspace(0, 1, 12)] * 3, indexing="ij"), -1).reshape(-1, 3)
    rnd = rng.uniform(0, 1, (8000, 3))
    return np.vstack([grid, rnd])

def val_points():
    return rng.uniform(0, 1, (3000, 3))

def test_points():
    rnd = rng.uniform(0, 1, (20000, 3))
    return rnd

def colorchecker24():
    """ColorChecker 24 色卡（BabelColor 平均实测值）→ sRGB 编码值。"""
    cc = colour.SDS_COLOURCHECKERS["BabelColor Average"]
    xyz = np.array([colour.sd_to_XYZ(sd).reshape(3) for sd in cc.values()])
    rgb = colour.XYZ_to_sRGB(xyz / 100.0, apply_cctf_encoding=True)
    return np.clip(rgb, 0, 1)

XYZ_W = colour.CCS_ILLUMINANTS["CIE 1931 2 Degree Standard Observer"]["D50"]

def de2000(rgb1, rgb2):
    l1 = colour.XYZ_to_Lab(colour.sRGB_to_XYZ(np.clip(rgb1, 0, 1)), illuminant=XYZ_W)
    l2 = colour.XYZ_to_Lab(colour.sRGB_to_XYZ(np.clip(rgb2, 0, 1)), illuminant=XYZ_W)
    return colour.difference.delta_E_CIE2000(l1, l2)

# ---------- 三线性权重（拟合与求值共用） ----------
def trilinear_matrix(points, K):
    """N×K³ 稀疏三线性权重矩阵（输入 [0,1]³，节点 i/(K-1)）。"""
    p = np.clip(points, 0, 1) * (K - 1)
    i0 = np.floor(p).astype(int)
    i0 = np.minimum(i0, K - 2)
    f = p - i0
    n = len(p)
    rows, cols, vals = [], [], []
    for dz in (0, 1):
        for dy in (0, 1):
            for dx in (0, 1):
                idx = ((i0[:, 2] + dz) * K + (i0[:, 1] + dy)) * K + (i0[:, 0] + dx)
                w = ((f[:, 0] if dx else 1 - f[:, 0]) *
                     (f[:, 1] if dy else 1 - f[:, 1]) *
                     (f[:, 2] if dz else 1 - f[:, 2]))
                rows.append(np.arange(n)); cols.append(idx); vals.append(w)
    return sp.coo_matrix((np.concatenate(vals), (np.concatenate(rows), np.concatenate(cols))),
                         shape=(n, K ** 3)).tocsr()

def trilinear_apply(lattice, points, K):
    A = trilinear_matrix(points, K)
    return np.stack([A @ lattice[:, c] for c in range(3)], axis=1)

# ---------- C. lattice 回归（平滑正则，Gupta CIC2009 型） ----------
def second_difference(K):
    """沿三轴的二阶差分算子（每通道独立作用）。"""
    triples = []
    for axis in range(3):
        o1, o2 = [ax for ax in range(3) if ax != axis]
        for u in range(K):
            for v in range(K):
                for c in range(K - 2):
                    idx = [0, 0, 0]; idx[o1] = u; idx[o2] = v
                    ns = []
                    for d in (c, c + 1, c + 2):
                        idx[axis] = d
                        ns.append(((idx[2]) * K + idx[1]) * K + idx[0])
                    triples.append(tuple(ns))
    t = np.array(triples)
    n_rows = len(t)
    rows = np.concatenate([np.arange(n_rows)] * 3)
    cols = t.ravel()
    vals = np.tile(np.array([1.0, -2.0, 1.0]), n_rows)
    return sp.coo_matrix((vals, (rows, cols)), shape=(n_rows, K ** 3)).tocsr()

def fit_lattice(points, targets, K, lam):
    A = trilinear_matrix(points, K)
    D = second_difference(K)
    AtA = (A.T @ A).tocsc()
    DtD = (D.T @ D).tocsc()
    scale = AtA.diagonal().max()
    lhs = (AtA + lam * scale * DtD).tocsc()
    rhs = A.T @ targets
    sol = spla.spsolve(lhs, rhs)
    return np.stack([sol[:, c] for c in range(3)], axis=1)

# ---------- A/B. 矩阵与多项式 ----------
def fit_matrix(points, targets):
    X = np.hstack([points, np.ones((len(points), 1))])
    return np.linalg.lstsq(X, targets, rcond=None)[0]

def apply_matrix(mdl, pts):
    return np.clip(np.hstack([pts, np.ones((len(pts), 1))]) @ mdl, 0, 1)

def fit_finlayson(points, targets, degree=3):
    P = finlayson_expansion(points, degree)
    return np.linalg.lstsq(P, targets, rcond=None)[0]

from colour.utilities import tstack
def finlayson_expansion(rgb, degree=3):
    """Finlayson 2015 式向量多项式展开（deg3: 20 项，与 colour-science 实现同构）。"""
    r, g, b = rgb[:, 0], rgb[:, 1], rgb[:, 2]
    terms = [np.ones_like(r), r, g, b, r * g, g * b, r * b, r ** 2, g ** 2, b ** 2,
             r * g * b, r ** 2 * g, r ** 2 * b, g ** 2 * r, g ** 2 * b, b ** 2 * r, b ** 2 * g,
             r ** 3, g ** 3, b ** 3]
    return tstack(terms)

# ---------- 实验 ----------
Xtr = train_points(); Ytr = ground_truth(Xtr)
Xval = val_points(); Yval = ground_truth(Xval)
Xte = test_points();  Yte = ground_truth(Xte)
CC = colorchecker24(); YCC = ground_truth(CC)

models = {}

models["A 3x3+offset"] = ("matrix", fit_matrix(Xtr, Ytr))

lattice_best, best = None, None
for lam in (0.0, 1e-4, 1e-3, 1e-2, 1e-1):
    L = fit_lattice(Xtr, Ytr, K, lam)
    dval = de2000(Yval, trilinear_apply(L, Xval, K)).mean()
    print(f"  [λ={lam:<5}] val meanΔE00 = {dval:.4f}")
    if best is None or dval < best:
        best, lattice_best = dval, (L, lam)
models["C lattice(17³)+smooth"] = ("lattice", lattice_best)
print(f"  [lattice] 选中 λ={lattice_best[1]}")

# B1: 普通多项式 deg3（Vandermonde 型 20 项，自实现展开）
P_tr = finlayson_expansion(Xtr)
models["B1 poly-deg3(20项,自实现)"] = ("poly", np.linalg.lstsq(P_tr, Ytr, rcond=None)[0])

# B2: colour-science 官方 Finlayson 2015 根多项式（root-polynomial，中性保持）
models["B2 Finlayson2015根多项式(colour)"] = ("rootpoly", (Ytr, Xtr))

print(f"\n{'方法':<24} {'随机20k meanΔE00':>16} {'maxΔE00':>9} {'色卡24 meanΔE00':>16} {'中性带单调viol':>14} {'中性带max2nd-diff':>17}")
print("-" * 100)

t = np.linspace(0, 1, 1024)
neutral = np.stack([t, t, t], axis=1)

def evaluate(name, pred_te, pred_cc, pred_neutral):
    d_te = de2000(Yte, pred_te)
    d_cc = de2000(YCC, pred_cc)
    dv = pred_neutral
    viol = sum(int((np.diff(dv[:, c]) < -1e-6).sum()) for c in range(3))
    second = np.abs(np.diff(dv, 2, axis=0)).max()
    print(f"{name:<24} {d_te.mean():>16.4f} {d_te.max():>9.3f} {d_cc.mean():>16.4f} {viol:>14d} {second:>17.4f}")

# 真值自身（基线应全 0）
evaluate("ground truth", Yte, YCC, ground_truth(neutral))

for name, (kind, mdl) in models.items():
    if kind == "matrix":
        evaluate(name, apply_matrix(mdl, Xte), apply_matrix(mdl, CC), apply_matrix(mdl, neutral))
    elif kind == "poly":
        evaluate(name, finlayson_expansion(Xte) @ mdl, finlayson_expansion(CC) @ mdl, finlayson_expansion(neutral) @ mdl)
    elif kind == "rootpoly":
        ap = lambda q: np.clip(colour.colour_correction(q, mdl[0], mdl[1], method="Finlayson 2015", degree=3), 0, 1)
        evaluate(name, ap(Xte), ap(CC), ap(neutral))
    else:
        L, _ = mdl
        evaluate(name, trilinear_apply(L, Xte, K), trilinear_apply(L, CC, K), trilinear_apply(L, neutral, K))

# 蓝色 hue ramp 方向性检查（04 报告翻车点 #1）
hue = np.stack([np.zeros(512), np.linspace(0, 0.6, 512), np.linspace(0.1, 1.0, 512)], axis=1)
print("\n蓝色 ramp 上各方法输出 B-R 差的极值（检验 hue twist / 越界表现）:")
for name, (kind, mdl) in models.items():
    if kind == "matrix": p = apply_matrix(mdl, hue)
    elif kind == "poly": p = finlayson_expansion(hue) @ mdl
    elif kind == "rootpoly": p = np.clip(colour.colour_correction(hue, mdl[0], mdl[1], method="Finlayson 2015", degree=3), 0, 1)
    else:
        L, _ = mdl; p = trilinear_apply(L, hue, K)
    print(f"  {name:<28} R∈[{p[:,0].min():.3f},{p[:,0].max():.3f}]  B-R∈[{(p[:,2]-p[:,0]).min():.3f},{(p[:,2]-p[:,0]).max():.3f}]")
