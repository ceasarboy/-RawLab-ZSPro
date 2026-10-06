#include "sony2fuji/lut_applicator.h"
#include <algorithm>
#include <cmath>
#include <iostream>

#ifdef _OPENMP
namespace {
constexpr size_t kParallelThreshold = 1u << 16;
}
#endif

namespace sony2fuji {

namespace {
// (dr,dg,db) 的 6 种大小次序 -> 四面体编号；与 shader 端查找表保持同一约定
inline int interpCase(float dr, float dg, float db) {
    if (dr >= dg) {
        if (dg >= db) return 0;   // r >= g >= b
        if (dr >= db) return 1;   // r >= b > g
        return 2;                 // b > r >= g
    }
    if (dr >= db) return 3;       // g > r >= b
    if (dg >= db) return 4;       // g >= b > r
    return 5;                     // b > g > r
}

inline RGB tetra(const RGB& c0, const RGB& c1, const RGB& c2, const RGB& c3,
    float a, float b, float c) {
    const float w0 = 1.0f - a;
    const float w1 = a - b;
    const float w2 = b - c;
    const float w3 = c;
    return RGB{w0 * c0.r + w1 * c1.r + w2 * c2.r + w3 * c3.r,
        w0 * c0.g + w1 * c1.g + w2 * c2.g + w3 * c3.g,
        w0 * c0.b + w1 * c1.b + w2 * c2.b + w3 * c3.b};
}
} // namespace

LUTApplicator::LUTApplicator(std::shared_ptr<LUT3D> lut)
    : lut_(std::move(lut)) {
}

RGB LUTApplicator::apply(const RGB& input) const {
    if (!lut_ || !lut_->isValid()) {
        return input;
    }

    // 将输入值限制在 [0, 1] 范围内
    const auto low = lut_->domainMin();
    const auto high = lut_->domainMax();
    float r = std::clamp((input.r-low.r)/(high.r-low.r), 0.0f, 1.0f);
    float g = std::clamp((input.g-low.g)/(high.g-low.g), 0.0f, 1.0f);
    float b = std::clamp((input.b-low.b)/(high.b-low.b), 0.0f, 1.0f);

    // 应用四面体插值
    return tetrahedralInterpolate(r, g, b);
}

ErrorCode LUTApplicator::applyToImage(ImageData& image) const {
    if (!lut_ || !lut_->isValid()) {
        return ErrorCode::InvalidFormat;
    }

    if (image.pixels.empty()) {
        return ErrorCode::ProcessingError;
    }

    const size_t pixel_count = image.pixels.size();

    // 对每个像素应用 LUT
#ifdef _OPENMP
#pragma omp parallel for if (pixel_count >= kParallelThreshold)
#endif
    for (size_t i = 0; i < pixel_count; ++i) {
        image.pixels[i] = apply(image.pixels[i]);
    }

    return ErrorCode::Success;
}

ErrorCode LUTApplicator::applyToImage(const ImageData& input, ImageData& output) const {
    if (!lut_ || !lut_->isValid()) {
        return ErrorCode::InvalidFormat;
    }

    if (input.pixels.empty()) {
        return ErrorCode::ProcessingError;
    }

    // 确保输出大小正确
    output.width = input.width;
    output.height = input.height;
    output.pixels.resize(input.pixels.size());

    // 应用 LUT
    const size_t pixel_count = input.pixels.size();
#ifdef _OPENMP
#pragma omp parallel for if (pixel_count >= kParallelThreshold)
#endif
    for (size_t i = 0; i < pixel_count; ++i) {
        output.pixels[i] = apply(input.pixels[i]);
    }

    return ErrorCode::Success;
}

RGB LUTApplicator::tetrahedralInterpolate(float r, float g, float b) const {
    int lutSize = lut_->getSize();

    // 将 [0, 1] 映射到 [0, lutSize-1]
    float fr = r * (lutSize - 1);
    float fg = g * (lutSize - 1);
    float fb = b * (lutSize - 1);

    // 获取整数部分和小数部分
    int r0 = static_cast<int>(std::floor(fr));
    int g0 = static_cast<int>(std::floor(fg));
    int b0 = static_cast<int>(std::floor(fb));

    int r1 = std::min(r0 + 1, lutSize - 1);
    int g1 = std::min(g0 + 1, lutSize - 1);
    int b1 = std::min(b0 + 1, lutSize - 1);

    // 插值因子
    float dr = fr - r0;
    float dg = fg - g0;
    float db = fb - b0;

    // 四面体插值：格胞按主对角线分为 6 个四面体，由 (dr,dg,db) 的次序取
    // 含点的那个，只采样 4 个角点。相比三线性（8 点、逐轴独立加权），
    // 饱和区色相不随其他通道漂移，与 GPU 路径保持一致。
    switch (interpCase(dr, dg, db)) {
    case 0: // r >= g >= b: 000 100 110 111
        return tetra(lut_->getValue(r0, g0, b0), lut_->getValue(r1, g0, b0),
            lut_->getValue(r1, g1, b0), lut_->getValue(r1, g1, b1), dr, dg, db);
    case 1: // r >= b > g: 000 100 101 111
        return tetra(lut_->getValue(r0, g0, b0), lut_->getValue(r1, g0, b0),
            lut_->getValue(r1, g0, b1), lut_->getValue(r1, g1, b1), dr, db, dg);
    case 2: // b > r >= g: 000 001 101 111
        return tetra(lut_->getValue(r0, g0, b0), lut_->getValue(r0, g0, b1),
            lut_->getValue(r1, g0, b1), lut_->getValue(r1, g1, b1), db, dr, dg);
    case 3: // g > r >= b: 000 010 110 111
        return tetra(lut_->getValue(r0, g0, b0), lut_->getValue(r0, g1, b0),
            lut_->getValue(r1, g1, b0), lut_->getValue(r1, g1, b1), dg, dr, db);
    case 4: // g >= b > r: 000 010 011 111
        return tetra(lut_->getValue(r0, g0, b0), lut_->getValue(r0, g1, b0),
            lut_->getValue(r0, g1, b1), lut_->getValue(r1, g1, b1), dg, db, dr);
    default: // b > g > r: 000 001 011 111
        return tetra(lut_->getValue(r0, g0, b0), lut_->getValue(r0, g0, b1),
            lut_->getValue(r0, g1, b1), lut_->getValue(r1, g1, b1), db, dg, dr);
    }
}

} // namespace sony2fuji
