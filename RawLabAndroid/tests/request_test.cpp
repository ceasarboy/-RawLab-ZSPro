#include "render_request.h"
#include <cassert>
#include <cmath>

// Only status text is linked here; these tests exercise adapter validation.
extern "C" const char* sony2fuji_status_message(sony2fuji_status) { return "test status"; }

int main() {
    auto preview = rawlab::makeRequest("input.dng", "film.cube", nullptr, 0.8f, 1.5f, false, 4200, 10,
        1.0f, 1.0f, 0.0f, 0.0f, 1600, false);
    assert(preview.version == SONY2FUJI_REQUEST_VERSION);
    assert(preview.struct_size == sizeof(sony2fuji_request));
    assert(preview.brightness == 1 && preview.contrast == 1 && preview.saturation == 1);
    assert(preview.tone_curve == 0 && preview.sharpening == 0);
    assert(preview.wb_mode == SONY2FUJI_WB_CAMERA);
    assert(preview.temperature == 6500 && preview.tint == 0);
    assert(preview.wb_mul[1] == 1);
    assert(preview.intent == SONY2FUJI_INTENT_PREVIEW);
    assert(preview.preview_long_edge == 1600);
    assert(preview.output_format == SONY2FUJI_OUTPUT_RGBA8);
    assert(preview.exposure_ev == 1.5f);
    auto exported = rawlab::makeRequest("input.dng", nullptr, "out.png", 1, 0, true, 4800, -10,
        1.2f, 0.9f, 0.4f, 0.5f, 1000, true);
    assert(exported.intent == SONY2FUJI_INTENT_FINAL);
    assert(exported.size_mode == SONY2FUJI_SIZE_NATIVE);
    assert(exported.preview_long_edge == 0);
    assert(exported.output_target == SONY2FUJI_TARGET_FILE);
    assert(exported.output_format == SONY2FUJI_OUTPUT_PNG);
    assert(exported.wb_mode == SONY2FUJI_WB_TEMPERATURE);
    assert(exported.temperature == 4800 && exported.tint == -10);
    assert(exported.lut_strength == 0);
    assert(exported.contrast == 1.2f && exported.saturation == 0.9f);
    assert(exported.tone_curve == 0.4f && exported.sharpening == 0.5f);
    // Zero strength collapses a provided LUT to none, leaving the request safe to render.
    auto noLut = rawlab::makeRequest("input.dng", "film.cube", nullptr, 0.0f, 0, false, 6500, 0,
        1.0f, 1.0f, 0.0f, 0.0f, 1600, false);
    assert(noLut.lut_strength == 0);
    bool rejected = false;
    try { rawlab::makeRequest("in", nullptr, nullptr, 1, NAN, false, 6500, 0,
        1.0f, 1.0f, 0.0f, 0.0f, 1600, false); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { rawlab::makeRequest("in", nullptr, nullptr, 1, 0, false, 6500, 0,
        2.5f, 1.0f, 0.0f, 0.0f, 1600, false); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { rawlab::makeRequest("in", nullptr, nullptr, 1, 0, false, 6500, 0,
        1.0f, 1.0f, 0.0f, 2.5f, 1600, false); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { rawlab::makeRequest("in", nullptr, nullptr, 1, 0, false, 6500, 0,
        1.0f, 1.0f, 1.5f, 0.0f, 1600, false); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    bool outOfMemory = false;
    try { rawlab::checkStatus(SONY2FUJI_STATUS_OUT_OF_MEMORY); }
    catch (const std::bad_alloc&) { outOfMemory = true; }
    assert(outOfMemory);
    char pixel = 0;
    sony2fuji_buffer padded{&pixel, 19, 2, 2, 12, SONY2FUJI_PIXEL_RGBA8};
    bool truncated = false;
    try { rawlab::previewByteCount(padded); }
    catch (const std::runtime_error&) { truncated = true; }
    assert(truncated);
    padded.size_bytes = 20;
    assert(rawlab::previewByteCount(padded) == 16);
}
