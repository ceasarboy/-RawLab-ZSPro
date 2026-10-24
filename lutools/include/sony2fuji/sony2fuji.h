#pragma once

/**
 * @file sony2fuji.h
 * @brief Sony to Fuji LUT Tool - 主头文件
 *
 * 这个库可以将 Sony 相机的 RAW 文件应用富士 F-Log2 LUT
 *
 * 基本使用流程:
 * 1. 使用 RAWProcessor 加载和处理 Sony RAW 文件
 * 2. 使用 ColorConverter 将色彩空间转换到 Fuji F-Gamut
 * 3. 使用 LUTParser 加载富士 LUT 文件
 * 4. 使用 LUTApplicator 应用 LUT 到图像
 * 5. 使用 ImageEncoder 保存结果
 *
 * 示例代码:
 * @code
 * #include <sony2fuji/sony2fuji.h>
 *
 * using namespace sony2fuji;
 *
 * // 1. 加载 RAW 文件
 * RAWProcessor processor;
 * processor.loadFile("input.ARW");
 *
 * RAWProcessOptions options;
 * options.outputLinear = true;
 * ImageData image;
 * processor.process(options, image);
 *
 * // 2. 色彩空间转换
 * ColorConverter converter;
 * converter.convertImage(image,
 *     ColorSpace::SonyNative,
 *     ColorSpace::FujiFilm_FGamut);
 *
 * // 3. 加载和应用 LUT
 * auto lut = LUTParser::loadLUT("ETERNA_BT709.cube");
 * LUTApplicator applicator(lut);
 * applicator.applyToImage(image);
 *
 * // 4. 保存结果
 * ImageEncoder::saveJPEG(image, "output.jpg", 95);
 * @endcode
 */

// 核心类型和定义
#include "sony2fuji/common.h"

// RAW 文件处理
#include "sony2fuji/raw_processor.h"

// 色彩空间转换
#include "sony2fuji/color_converter.h"

// LUT 处理
#include "sony2fuji/lut_parser.h"
#include "sony2fuji/lut_applicator.h"
#include "sony2fuji/gpu/lut_gpu.h"

namespace sony2fuji {

/**
 * @brief 版本信息
 */
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

/**
 * @brief 获取版本字符串
 */
inline std::string getVersionString() {
    return std::to_string(VERSION_MAJOR) + "." +
           std::to_string(VERSION_MINOR) + "." +
           std::to_string(VERSION_PATCH);
}

} // namespace sony2fuji
