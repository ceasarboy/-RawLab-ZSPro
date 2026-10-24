#include "RawMetadataBridge.h"

#include <string>

#include "sony2fuji/raw_processor.h"

RawDimensions rawlab_get_raw_dimensions(const char *path) {
    RawDimensions dims = {0, 0};
    if (!path || path[0] == '\0') {
        return dims;
    }

    sony2fuji::RAWProcessor processor;
    sony2fuji::ErrorCode result = processor.loadFile(std::string(path));
    if (result != sony2fuji::ErrorCode::Success) {
        return dims;
    }

    dims.width = static_cast<uint32_t>(processor.getWidth());
    dims.height = static_cast<uint32_t>(processor.getHeight());
    return dims;
}
