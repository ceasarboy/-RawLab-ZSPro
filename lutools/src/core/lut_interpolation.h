#pragma once

#include "sony2fuji/common.h"
#include <algorithm>
#include <cmath>

namespace sony2fuji::detail {
/* 输入为 domain 归一化后的 [0,1] 或 NaN；有效表含 size^3 个 R-fast 元素，size 为 2..256。 */
static inline RGB interpolateLUT(const RGB* table, int lutSize, float r, float g, float b) {

    // 将 [0, 1] 映射到 [0, lutSize-1]
    float fr = r * (lutSize - 1);
    float fg = g * (lutSize - 1);
    float fb = b * (lutSize - 1);

    // 获取整数部分和小数部分
    int r0 = static_cast<int>(std::floor(std::max(0.0f, fr)));
    int g0 = static_cast<int>(std::floor(std::max(0.0f, fg)));
    int b0 = static_cast<int>(std::floor(std::max(0.0f, fb)));

    int r1 = std::min(r0 + 1, lutSize - 1);
    int g1 = std::min(g0 + 1, lutSize - 1);
    int b1 = std::min(b0 + 1, lutSize - 1);

    // 插值因子
    float dr = fr - r0;
    float dg = fg - g0;
    float db = fb - b0;

    /* 四面体插值：格胞按主对角线切成 6 个四面体，点落在哪个由 (dr,dg,db)
       的大小次序决定，只需 4 个角点、4 次表访问（三线性要 8 次）。沿对角面
       插值，饱和区色相不随其他通道漂移；格点处权重退化为单点，输出与表值
       逐位一致。与 GPU 路径（GLES/D3D11/Metal）保持同一约定。
       索引把 NaN 归零；权重仍使用原始值，保留 NaN 传播和负零。 */
    const int base = (b0 * lutSize + g0) * lutSize + r0;
    const int stepR = r1 - r0;
    const int stepG = (g1 - g0) * lutSize;
    const int stepB = (b1 - b0) * lutSize * lutSize;
    const RGB c000 = table[base];
    const RGB c111 = table[base + stepR + stepG + stepB];
    auto tetra = [&c000, &c111](const RGB& c1, const RGB& c2, float a, float b, float c) {
        return RGB(
            (1.0f - a) * c000.r + (a - b) * c1.r + (b - c) * c2.r + c * c111.r,
            (1.0f - a) * c000.g + (a - b) * c1.g + (b - c) * c2.g + c * c111.g,
            (1.0f - a) * c000.b + (a - b) * c1.b + (b - c) * c2.b + c * c111.b);
    };

    if (dr >= dg) {
        if (dg >= db) {
            // r >= g >= b: 000 100 110 111
            return tetra(table[base + stepR], table[base + stepR + stepG], dr, dg, db);
        }
        if (dr >= db) {
            // r >= b > g: 000 100 101 111
            return tetra(table[base + stepR], table[base + stepR + stepB], dr, db, dg);
        }
        // b > r >= g: 000 001 101 111
        return tetra(table[base + stepB], table[base + stepR + stepB], db, dr, dg);
    }
    if (dr >= db) {
        // g > r >= b: 000 010 110 111
        return tetra(table[base + stepG], table[base + stepR + stepG], dg, dr, db);
    }
    if (dg >= db) {
        // g >= b > r: 000 010 011 111
        return tetra(table[base + stepG], table[base + stepG + stepB], dg, db, dr);
    }
    // b > g > r: 000 001 011 111
    return tetra(table[base + stepB], table[base + stepG + stepB], db, dg, dr);
}
} // namespace sony2fuji::detail
