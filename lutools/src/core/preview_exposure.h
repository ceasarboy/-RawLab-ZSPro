#pragma once

#include <string>
#include <vector>

namespace sony2fuji {

// Both inputs contain linear-light sRGB luminance samples, including black.
float estimatePreviewExposureEV(std::vector<float> raw, std::vector<float> preview, bool* matched=nullptr);
std::vector<float> loadPreviewLuminance(const std::string& path);

} // namespace sony2fuji
