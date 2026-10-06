#pragma once

#include "sony2fuji/lut_parser.h"
#include <memory>
#include <string>

namespace sony2fuji {

// Builds the "RawLab neutral display -> Panasonic STD" input-adaptation cube
// from the official STD_to_VLOG mapping, then resamples `lut` (a look whose
// input domain is the STD direct-output rendering) through it. The result is
// a display-encoding look LUT that reads correctly on our neutral baseline:
//   adapted(v) = lut( Minv( vlogEncode( neutralDecode(v) ) ) )
// where Minv is the numerical inverse of the STD->V-Log map. See
// docs/research/verification/ for the V-Log anchors and the official mapping
// source (Lumix Lab assets, models/mergeLut).
std::shared_ptr<LUT3D> adaptStdLut(const LUT3D& lut, const LUT3D& stdToVlog, int outSize);

} // namespace sony2fuji
