#include "sony2fuji/ffi/sony2fuji_c.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: sony2fuji_example input.RAW film.cube output.png\n";
        return 2;
    }
    sony2fuji_request request{};
    request.version=SONY2FUJI_REQUEST_VERSION; request.struct_size=sizeof(request);
    request.input_type=SONY2FUJI_INPUT_RAW; request.input_path=argv[1];
    request.lut_path=argv[2]; request.lut_strength=1;
    request.wb_mode=SONY2FUJI_WB_CAMERA;
    request.brightness=request.contrast=request.saturation=1;
    request.temperature=6500;
    request.intent=SONY2FUJI_INTENT_FINAL; request.size_mode=SONY2FUJI_SIZE_NATIVE;
    request.output_target=SONY2FUJI_TARGET_FILE; request.output_format=SONY2FUJI_OUTPUT_PNG;
    request.output_path=argv[3];
    sony2fuji_session* session=nullptr;
    auto result=sony2fuji_session_create(&session);
    if (result==SONY2FUJI_STATUS_OK) result=sony2fuji_process(session,&request,nullptr);
    sony2fuji_session_destroy(session);
    std::cout << sony2fuji_status_message(result) << '\n';
    return result==SONY2FUJI_STATUS_OK ? 0 : 1;
}
