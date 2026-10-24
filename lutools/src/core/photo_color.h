#pragma once

#include "sony2fuji/common.h"
#include <array>

namespace sony2fuji {

struct HslOptions {
    std::array<float, 8> hue{};
    std::array<float, 8> saturation{};
    std::array<float, 8> lightness{};
    bool valid() const;
    bool active() const;
};

RGB applyHsl(const RGB& pixel, const HslOptions& options);
void applyHsl(ImageData& image, const HslOptions& options);
bool estimateAutoExposure(const ImageData& image, ColorSpace space, const RGB& whiteBalance, float& ev);

} // namespace sony2fuji
