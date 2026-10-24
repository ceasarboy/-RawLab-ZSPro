#include "sony2fuji/ffi/sony2fuji_c.h"
#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    for (auto mode : {SONY2FUJI_GPU_OFF, SONY2FUJI_GPU_FORCE}) {
        sony2fuji_session* session = nullptr;
        if (sony2fuji_session_create(&session) != SONY2FUJI_STATUS_OK) return 1;
        sony2fuji_gpu_config config{SONY2FUJI_GPU_CONFIG_VERSION, sizeof(config), mode};
        sony2fuji_session_set_gpu_config(session, &config);
        sony2fuji_request r{};
        r.version = SONY2FUJI_REQUEST_VERSION; r.struct_size = sizeof(r);
        r.input_type = SONY2FUJI_INPUT_RAW; r.input_path = argv[1];
        r.wb_mode = SONY2FUJI_WB_CAMERA; r.temperature = 6500;
        r.brightness = r.contrast = r.saturation = r.lut_strength = 1;
        r.intent = SONY2FUJI_INTENT_PREVIEW; r.preview_long_edge = 2000;
        r.size_mode = SONY2FUJI_SIZE_NATIVE;
        r.output_target = SONY2FUJI_TARGET_BUFFER; r.output_format = SONY2FUJI_OUTPUT_RGBA8;
        auto run = [&](const char* label, bool film) {
            r.lut_path = film ? argv[2] : nullptr;
            sony2fuji_buffer buffer{};
            const auto start = std::chrono::steady_clock::now();
            const auto status = sony2fuji_process(session, &r, &buffer);
            const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count();
            std::cout << (mode == SONY2FUJI_GPU_OFF ? "CPU " : "Metal ") << label << ' '
                      << ms << " ms status=" << status << " backend="
                      << sony2fuji_session_get_last_backend(session) << std::endl;
            sony2fuji_release_buffer(&buffer);
            if (status != SONY2FUJI_STATUS_OK) std::exit(1);
        };
        run("cold-neutral", false); run("first-film", true);
        for (int i=0; i<3; ++i) {
            r.exposure_ev = .1f*i;
            run("cached-neutral", false); run("cached-film", true);
        }
        r.wb_mode = SONY2FUJI_WB_TEMPERATURE;
        r.temperature = 6000; run("exact-6000-neutral", false); run("exact-6000-film", true);
        r.temperature = 8000; run("exact-8000-neutral", false); run("exact-8000-film", true);
        sony2fuji_session_set_interactive_preview(session, 1);
        r.preview_long_edge = 1000;
        for (float kelvin : {5000.f, 7000.f, 9000.f}) {
            r.temperature = kelvin;
            run("interactive-WB-neutral", false); run("interactive-WB-film", true);
        }
        sony2fuji_session_set_interactive_preview(session, 0);
        r.preview_long_edge = 2000;
        run("release-exact-neutral", false); run("release-exact-film", true);
        sony2fuji_session_destroy(session);
    }
}
