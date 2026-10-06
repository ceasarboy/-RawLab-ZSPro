#pragma once

namespace sony2fuji {

// Panasonic V-Log transfer function (VARICAM V-Log/V-Gamut technical
// documentation), normalized scene reflection [0, 1] <-> normalized code
// [0, 1]. Anchor values verified against colour-science 0.4.7:
// encode(0.18) = 0.4233, decode(0.125) = 0, toe branch point y = 0.181
// (see docs/research/verification/log-decode-verification.md).

// Scene reflection -> V-Log code. Inputs outside [0, 1] clamp; the toe
// segment below 0.01 extends linearly to hold negatives in codecs, which
// this scalar form does not need.
float vlogEncode(float linear);

// V-Log code -> scene reflection. Branch point y = 0.181 (not the toe's
// 0.125 offset): values below it all ride the linear toe segment.
float vlogDecode(float code);

// Inverse of the pipeline's neutralDisplay(): sRGB decode followed by the
// log-logistic view-curve inverse. Neutral-encoded display value -> scene
// linear. Defined in lut_adapt.cpp next to its only consumer.
float neutralDisplayDecode(float display);

} // namespace sony2fuji
