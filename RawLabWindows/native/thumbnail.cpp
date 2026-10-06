#include "sony2fuji/ffi/sony2fuji_c.h"
#include "core/file_path.h"
#include <cstdlib>
#include <cstring>
#include <memory>

// Windows-only helper. Extracts embedded data without unpacking/demosaicing RAW.
// kind=1: JPEG bytes, kind=2: RGB8 pixels. flip is LibRaw's orientation.
extern "C" __declspec(dllexport) int rawlab_thumbnail(
    const char* path, sony2fuji_buffer* output, int* kind, int* flip) {
    if (!path || !output || !kind || !flip) return 1;
    *output = {};
    try {
        auto raw = std::make_unique<LibRaw>();
        if (sony2fuji::openRawFile(*raw,path) || raw->unpack_thumb()) return 3;
        std::unique_ptr<libraw_processed_image_t, decltype(&LibRaw::dcraw_clear_mem)>
            thumbnail(raw->dcraw_make_mem_thumb(), LibRaw::dcraw_clear_mem);
        if (!thumbnail) return 2;
        *kind = thumbnail->type == LIBRAW_IMAGE_JPEG ? 1 : 2;
        if (*kind == 2 && (thumbnail->type != LIBRAW_IMAGE_BITMAP ||
            thumbnail->bits != 8 || thumbnail->colors != 3)) return 2;
        if (thumbnail->data_size > 64 * 1024 * 1024) return 2;
        output->data = std::malloc(thumbnail->data_size);
        if (!output->data) return 5;
        std::memcpy(output->data, thumbnail->data, thumbnail->data_size);
        output->size_bytes = thumbnail->data_size;
        output->width = thumbnail->width; output->height = thumbnail->height;
        output->stride_bytes = thumbnail->width * 3;
        output->pixel_format = SONY2FUJI_PIXEL_RGB8;
        *flip = raw->imgdata.sizes.flip;
        return 0;
    } catch (...) { sony2fuji_release_buffer(output); return 4; }
}
