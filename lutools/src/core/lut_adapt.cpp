#include "sony2fuji/lut_adapt.h"
#include "sony2fuji/vlog.h"
#include "sony2fuji/lut_applicator.h"
#include "core/photo_rendering.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sony2fuji {

namespace {

constexpr float kCut1 = 0.01f;   // V-Log encode toe bound (reflection)
constexpr float kCut2 = 0.181f;  // V-Log decode branch (code)
constexpr float kB = 0.00873f;
constexpr float kC = 0.241514f;
constexpr float kD = 0.598206f;

RGB clamp01(RGB v) {
    return RGB(std::clamp(v.r, 0.0f, 1.0f), std::clamp(v.g, 0.0f, 1.0f),
        std::clamp(v.b, 0.0f, 1.0f));
}

float comp(const RGB& v, int axis) {
    return axis == 0 ? v.r : axis == 1 ? v.g : v.b;
}

void setComp(RGB& v, int axis, float x) {
    (&v.r)[axis] = x;
}

} // namespace

float vlogEncode(float linear) {
    linear = std::clamp(linear, 0.0f, 1.0f);
    return linear < kCut1 ? 5.6f * linear + 0.125f
        : kC * std::log10(linear + kB) + kD;
}

float vlogDecode(float code) {
    code = std::clamp(code, 0.0f, 1.0f);
    return code < kCut2 ? (code - 0.125f) / 5.6f
        : std::pow(10.0f, (code - kD) / kC) - kB;
}

float neutralDisplayDecode(float display) {
    if (display <= 0) return 0;
    // sRGB inverse EOTF, then the log-logistic view-curve inverse of
    // neutralDisplay() (photo_rendering.h): L = 1/(1 + k*(g/L0)^1.5) solved
    // for L0. Middle gray is invariant by construction.
    const float l0 = display <= 0.04045f ? display / 12.92f
        : std::pow((display + 0.055f) / 1.055f, 2.4f);
    if (l0 >= 1) return 1;
    constexpr float g = 0.1845f;
    const float k = (1 - g) / g;
    return g / std::pow((1.0f / l0 - 1.0f) / k, 2.0f / 3.0f);
}

namespace {

// One Newton solve of map(s) = target. Returns residual after the run.
float solvePoint(const LUTApplicator& map, RGB target, RGB& s, int iterations) {
    // Levenberg-Marquardt projection: for targets outside the map's attainable
    // output set (gamut overflow), the exact system has no solution; LM
    // converges to the nearest attainable point CONTINUOUSLY in the target,
    // which per-point hard clamping cannot.
    auto residual = [&](const RGB& v) {
        const RGB e = clamp01(map.apply(v));
        return std::sqrt(std::pow(target.r - e.r, 2) + std::pow(target.g - e.g, 2)
            + std::pow(target.b - e.b, 2));
    };
    float lambda = 1e-3f;
    float bestNorm = residual(s);
    for (int it = 0; it < 16; ++it) {
        const RGB e = clamp01(map.apply(s));
        const RGB err(target.r - e.r, target.g - e.g, target.b - e.b);
        if (std::abs(err.r) + std::abs(err.g) + std::abs(err.b) < 3e-4f) break;
        const float h = 1.0f / 128.0f;
        float J[3][3];
        for (int axis = 0; axis < 3; ++axis) {
            RGB sp = s;
            setComp(sp, axis, std::clamp(comp(s, axis) + h, 0.0f, 1.0f));
            const RGB ep = clamp01(map.apply(sp));
            J[0][axis] = (ep.r - e.r) / h;
            J[1][axis] = (ep.g - e.g) / h;
            J[2][axis] = (ep.b - e.b) / h;
        }
        // Normal equations with damping: (JtJ + lambda*I) delta = Jt * err.
        float JtJ[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                JtJ[i][j] = J[0][i] * J[0][j] + J[1][i] * J[1][j] + J[2][i] * J[2][j];
        for (int i = 0; i < 3; ++i) JtJ[i][i] *= 1.0f + lambda;
        const float det = JtJ[0][0] * (JtJ[1][1] * JtJ[2][2] - JtJ[1][2] * JtJ[2][1])
            - JtJ[0][1] * (JtJ[1][0] * JtJ[2][2] - JtJ[1][2] * JtJ[2][0])
            + JtJ[0][2] * (JtJ[1][0] * JtJ[2][1] - JtJ[1][1] * JtJ[2][0]);
        if (std::abs(det) < 1e-12f) break;
        const float JtE[3] = { J[0][0] * err.r + J[1][0] * err.g + J[2][0] * err.b,
            J[0][1] * err.r + J[1][1] * err.g + J[2][1] * err.b,
            J[0][2] * err.r + J[1][2] * err.g + J[2][2] * err.b };
        const float dx = (JtE[0] * (JtJ[1][1] * JtJ[2][2] - JtJ[1][2] * JtJ[2][1])
            - JtE[1] * (JtJ[0][1] * JtJ[2][2] - JtJ[0][2] * JtJ[2][1])
            + JtE[2] * (JtJ[0][1] * JtJ[1][2] - JtJ[0][2] * JtJ[1][1])) / det;
        const float dy = (JtE[0] * (JtJ[1][2] * JtJ[2][0] - JtJ[1][0] * JtJ[2][2])
            - JtE[1] * (JtJ[0][2] * JtJ[2][0] - JtJ[0][0] * JtJ[2][2])
            + JtE[2] * (JtJ[0][0] * JtJ[1][2] - JtJ[0][2] * JtJ[1][0])) / det;
        const float dz = (JtE[0] * (JtJ[1][0] * JtJ[2][1] - JtJ[1][1] * JtJ[2][0])
            - JtE[1] * (JtJ[0][0] * JtJ[2][1] - JtJ[0][1] * JtJ[2][0])
            + JtE[2] * (JtJ[0][0] * JtJ[1][1] - JtJ[0][1] * JtJ[1][0])) / det;
        RGB candidate = clamp01(RGB(s.r + std::clamp(dx, -0.25f, 0.25f),
            s.g + std::clamp(dy, -0.25f, 0.25f),
            s.b + std::clamp(dz, -0.25f, 0.25f)));
        const float candidateNorm = residual(candidate);
        if (candidateNorm < bestNorm) {
            s = candidate;
            bestNorm = candidateNorm;
            lambda = std::max(lambda * 0.5f, 1e-6f);
            if (bestNorm < 3e-4f) break;
        } else {
            lambda *= 4.0f;
            if (lambda > 1e3f) break;
        }
    }
    return bestNorm;
}

} // namespace

std::shared_ptr<LUT3D> adaptStdLut(const LUT3D& lut, const LUT3D& stdToVlog, int outSize) {
    if (!lut.isValid() || stdToVlog.getSize() < 2 || outSize < 17 || outSize > 65) return nullptr;
    const LUTApplicator look(std::make_shared<LUT3D>(lut));

    // 分层链（实验 A/B 定论，见 docs/research/05 报告 §10）：
    //   adapted(v) = look( stdDisplay( neutralDecode(v) ) )
    // 其中 stdDisplay 是"场景线性 → STD 直出显示值"的 1D 曲线，从官方
    // STD_to_VLOG 映射灰轴反演得到：M(g,g,g) 是 STD 显示值 g 的 V-Log 码，
    // vlogDecode(M_gray) = 场景线性，故 (L, g) 对即 STD 显示响应曲线。
    // 该曲线 1D 单调（灰轴已验证），适配格点天然连续，无逐点 3D 逆噪声。
    const int mn = stdToVlog.getSize();
    std::vector<double> linAxis;   // 场景线性（vlogDecode(M_gray)）
    std::vector<double> stdAxis;   // STD 显示值（g）
    for (int i = 0; i < mn; ++i) {
        const double g = double(i) / (mn - 1);
        const LUTApplicator mapApp(std::make_shared<LUT3D>(stdToVlog));
        const RGB m = clamp01(mapApp.apply(RGB(float(g), float(g), float(g))));
        linAxis.push_back(vlogDecode((m.r + m.g + m.b) / 3.0));
        stdAxis.push_back(g);
    }
    // 反转成 lin -> std 的 1D 表（等距 lin 轴，逐点线性内插；linAxis 单调递增）
    const int curveN = 256;
    std::vector<double> stdCurve(curveN);
    for (int i = 0; i < curveN; ++i) {
        const double L = double(i) / (curveN - 1);
        // 二分查找 linAxis 中 L 的位置（linAxis 单调）
        int lo = 0, hi = mn - 1;
        while (hi - lo > 1) {
            const int mid = (lo + hi) / 2;
            if (linAxis[mid] < L) lo = mid; else hi = mid;
        }
        const double t = std::clamp((L - linAxis[lo]) / std::max(linAxis[hi] - linAxis[lo], 1e-9), 0.0, 1.0);
        stdCurve[i] = stdAxis[lo] + (stdAxis[hi] - stdAxis[lo]) * t;
    }
    auto stdDisplay = [&](double lin) {
        lin = std::clamp(lin, 0.0, 1.0);
        const double p = lin * (curveN - 1);
        const int i0 = std::min(int(p), curveN - 2);
        const double t = p - i0;
        return stdCurve[i0] + (stdCurve[i0 + 1] - stdCurve[i0]) * t;
    };

    std::vector<RGB> data(static_cast<size_t>(outSize) * outSize * outSize);
    for (int b = 0; b < outSize; ++b)
        for (int g = 0; g < outSize; ++g)
            for (int r = 0; r < outSize; ++r) {
                const RGB v(float(r) / (outSize - 1), float(g) / (outSize - 1), float(b) / (outSize - 1));
                const RGB s(std::clamp(float(stdDisplay(neutralDisplayDecode(v.r))), 0.0f, 1.0f),
                    std::clamp(float(stdDisplay(neutralDisplayDecode(v.g))), 0.0f, 1.0f),
                    std::clamp(float(stdDisplay(neutralDisplayDecode(v.b))), 0.0f, 1.0f));
                data[(static_cast<size_t>(b) * outSize + g) * outSize + r] = clamp01(look.apply(s));
            }
    return LUT3D::fromData(outSize, std::move(data), "STD base adapted via layered tone (RawLab)");
}

} // namespace sony2fuji
