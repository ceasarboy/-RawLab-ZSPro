#pragma once

#include "sony2fuji/lut_parser.h"
#include <vector>

namespace sony2fuji {

std::vector<float> buildLutTextureDataRGBA(const LUT3D& lut);

}
