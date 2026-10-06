#pragma once

#include "sony2fuji/common.h"
#include "sony2fuji/lut_parser.h"
#include <memory>

namespace sony2fuji {

enum class GpuMode {
    Off = 0,
    Auto = 1,
    Force = 2
};

struct GpuConfig {
    GpuMode mode = GpuMode::Off;
};

GpuConfig defaultGpuConfig();

ErrorCode applyLUTWithConfig(
    const std::shared_ptr<LUT3D>& lut,
    ImageData& image,
    const GpuConfig& config
);

}
