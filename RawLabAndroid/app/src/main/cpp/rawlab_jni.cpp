#include <jni.h>
#include "render_request.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace {
struct SessionDeleter {
    void operator()(sony2fuji_session* session) const { sony2fuji_session_destroy(session); }
};
using Session = std::unique_ptr<sony2fuji_session, SessionDeleter>;
// Opaque IDs keep stale Java handles from becoming native pointers.
std::mutex sessionsMutex;
std::unordered_map<jlong, Session> sessions;
jlong nextId = 1;

void throwJava(JNIEnv* env, const char* type, const char* message) {
    if (!env->ExceptionCheck()) env->ThrowNew(env->FindClass(type), message);
}
void check(sony2fuji_status status) {
    rawlab::checkStatus(status);
}
struct UtfChars {
    JNIEnv* env;
    jstring value;
    const char* chars;
    UtfChars(JNIEnv* e, jstring s) : env(e), value(s), chars(s ? e->GetStringUTFChars(s, nullptr) : nullptr) {
        if (s && !chars) throw std::bad_alloc();
    }
    ~UtfChars() { if (chars) env->ReleaseStringUTFChars(value, chars); }
};
struct Buffer {
    sony2fuji_buffer value{};
    ~Buffer() { sony2fuji_release_buffer(&value); }
};
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_rawlab_android_NativeProcessor_nativeCreate(JNIEnv* env, jobject, jint mode) {
    try {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        if (mode < SONY2FUJI_GPU_OFF || mode > SONY2FUJI_GPU_FORCE) throw std::invalid_argument("Invalid GPU mode");
        sony2fuji_session* pointer = nullptr;
        check(sony2fuji_session_create(&pointer));
        Session session(pointer);
        sony2fuji_gpu_config config{SONY2FUJI_GPU_CONFIG_VERSION, sizeof(sony2fuji_gpu_config), static_cast<sony2fuji_gpu_mode>(mode)};
        check(sony2fuji_session_set_gpu_config(pointer, &config));
        const jlong id = nextId++;
        sessions.emplace(id, std::move(session));
        return id;
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Not enough memory to create a RAW session");
        return 0;
    } catch (const std::exception& error) {
        throwJava(env, "java/lang/IllegalStateException", error.what());
        return 0;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawlab_android_NativeProcessor_nativeDestroy(JNIEnv*, jobject, jlong id) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    sessions.erase(id);
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawlab_android_NativeProcessor_nativeSetGpuMode(JNIEnv* env, jobject, jlong id, jint mode) {
    try {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = sessions.find(id);
        if (found == sessions.end() || mode < 0 || mode > 2) throw std::invalid_argument("Invalid processor or GPU mode");
        sony2fuji_gpu_config config{SONY2FUJI_GPU_CONFIG_VERSION, sizeof(config), static_cast<sony2fuji_gpu_mode>(mode)};
        check(sony2fuji_session_set_gpu_config(found->second.get(), &config));
    } catch (const std::exception& error) { throwJava(env, "java/lang/IllegalArgumentException", error.what()); }
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawlab_android_NativeProcessor_nativeSetLutMode(JNIEnv* env, jobject, jlong id, jint mode) {
    try {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = sessions.find(id);
        if (found == sessions.end() || mode < SONY2FUJI_LUT_MODE_PHOTO || mode > SONY2FUJI_LUT_MODE_FLOG2_INPUT)
            throw std::invalid_argument("Invalid processor or LUT mode");
        check(sony2fuji_session_set_lut_mode(found->second.get(), static_cast<sony2fuji_lut_mode>(mode)));
    } catch (const std::exception& error) { throwJava(env, "java/lang/IllegalArgumentException", error.what()); }
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawlab_android_NativeProcessor_nativeSetLutB(JNIEnv* env, jobject, jlong id,
    jstring path, jfloat strength) {
    try {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = sessions.find(id);
        if (found == sessions.end()) throw std::invalid_argument("Processor is closed");
        if (strength < 0 || strength > 1) throw std::invalid_argument("Invalid stacked LUT strength");
        UtfChars lut(env, path);
        check(sony2fuji_session_set_lut_b(found->second.get(), lut.chars, strength));
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Not enough memory for the stacked LUT");
    } catch (const std::invalid_argument& error) {
        throwJava(env, "java/lang/IllegalArgumentException", error.what());
    } catch (const std::exception& error) {
        throwJava(env, "java/io/IOException", error.what());
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_rawlab_android_NativeProcessor_nativeBakeLookLut(JNIEnv* env, jobject,
    jstring pathA, jfloat strengthA, jstring pathB, jfloat strengthB,
    jstring outPath, jint size) {
    try {
        if (size < 17 || size > 65 || strengthA < 0 || strengthA > 1 || strengthB < 0 || strengthB > 1)
            throw std::invalid_argument("Invalid bake settings");
        UtfChars a(env, pathA), b(env, pathB), o(env, outPath);
        check(sony2fuji_bake_look_lut(a.chars, strengthA, b.chars, strengthB, o.chars, size));
        return JNI_TRUE;
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Not enough memory to bake the LUT");
    } catch (const std::invalid_argument& error) {
        throwJava(env, "java/lang/IllegalArgumentException", error.what());
    } catch (const std::exception& error) {
        throwJava(env, "java/io/IOException", error.what());
    }
    return JNI_FALSE;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_rawlab_android_NativeProcessor_nativeRenderLutThumb(JNIEnv* env, jobject,
    jstring path) {
    // Fixed 192x128 reference scene (hue sweep + skin/sky/foliage/gray patches)
    // rendered through the full pipeline in DISPLAY mode, RGBA8 rows out.
    constexpr int kW = 192, kH = 128;
    try {
        std::vector<unsigned char> scene(size_t(kW) * kH * 3);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) {
                auto* p = &scene[(size_t(y) * kW + x) * 3];
                const float fx = x / float(kW - 1), fy = y / float(kH - 1);
                if (y < kH / 2) {
                    // hue sweep at mid-high value
                    const float h = fx * 6.0f;
                    const int i = int(h) % 6;
                    const float f = h - float(int(h));
                    const float v = 0.85f, s = 0.55f, q = v * (1 - s * f), t = v * (1 - s * (1 - f));
                    const float table[6][3] = {{v,t,0},{q,v,0},{0,v,t},{0,q,v},{t,0,v},{v,0,q}};
                    p[0] = uint8_t(table[i][0] * 255); p[1] = uint8_t(table[i][1] * 255); p[2] = uint8_t(table[i][2] * 255);
                } else if (fx < 0.45f) {
                    // gray ramp
                    const float g = 0.06f + 0.88f * (fy - 0.5f) * 2.0f * 0.5f + 0.44f * fx / 0.45f * 0.5f;
                    const float gg = std::min(1.0f, g);
                    p[0] = p[1] = p[2] = uint8_t(gg * 255);
                } else {
                    // skin, sky, foliage, red blocks
                    const float colors[4][3] = {{0.96f,0.80f,0.69f},{0.47f,0.66f,0.90f},{0.28f,0.50f,0.24f},{0.82f,0.11f,0.11f}};
                    const int i = std::min(3, int((fx - 0.45f) / 0.55f * 4));
                    const float shade = 0.75f + 0.25f * (1 - fy);
                    p[0] = uint8_t(colors[i][0] * shade * 255);
                    p[1] = uint8_t(colors[i][1] * shade * 255);
                    p[2] = uint8_t(colors[i][2] * shade * 255);
                }
            }
        sony2fuji_session* pointer = nullptr;
        check(sony2fuji_session_create(&pointer));
        Session session(pointer);
        sony2fuji_gpu_config config{SONY2FUJI_GPU_CONFIG_VERSION, sizeof(config), SONY2FUJI_GPU_OFF};
        check(sony2fuji_session_set_gpu_config(pointer, &config));
        check(sony2fuji_session_set_lut_mode(pointer, SONY2FUJI_LUT_MODE_DISPLAY));
        UtfChars lut(env, path);
        sony2fuji_request request{};
        request.version = SONY2FUJI_REQUEST_VERSION;
        request.struct_size = sizeof(request);
        request.input_type = SONY2FUJI_INPUT_BUFFER;
        request.input_pixels = scene.data();
        request.input_width = kW;
        request.input_height = kH;
        request.input_pixel_format = SONY2FUJI_PIXEL_RGB8;
        request.input_color_space = SONY2FUJI_COLOR_SRGB;
        request.input_is_linear = 0;
        request.lut_path = lut.chars;
        request.lut_strength = 1;
        request.brightness = request.contrast = request.saturation = 1;
        for (auto& v : request.wb_mul) v = 1;
        request.wb_mode = SONY2FUJI_WB_CAMERA;
        request.temperature = 6500;
        request.intent = SONY2FUJI_INTENT_FINAL;
        request.size_mode = SONY2FUJI_SIZE_NATIVE;
        request.output_target = SONY2FUJI_TARGET_BUFFER;
        request.output_format = SONY2FUJI_OUTPUT_RGBA8;
        Buffer buffer;
        check(sony2fuji_process(pointer, &request, &buffer.value));
        const auto& b = buffer.value;
        const size_t row = size_t(b.width) * 4;
        const size_t bytes = rawlab::previewByteCount(b);
        auto array = env->NewByteArray(static_cast<jsize>(bytes));
        if (!array) return nullptr;
        for (uint32_t y = 0; y < b.height && !env->ExceptionCheck(); ++y)
            env->SetByteArrayRegion(array, jsize(y * row), jsize(row),
                static_cast<const jbyte*>(b.data) + y * b.stride_bytes);
        return array;
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Not enough memory for the LUT thumbnail");
    } catch (const std::exception& error) {
        throwJava(env, "java/io/IOException", error.what());
    }
    return nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_rawlab_android_NativeProcessor_nativeAdaptStdLut(JNIEnv* env, jobject,
    jstring inPath, jstring mapPath, jstring outPath, jint size) {
    try {
        if (size < 17 || size > 65) throw std::invalid_argument("Invalid adaptation size");
        UtfChars in(env, inPath), map(env, mapPath), out(env, outPath);
        check(sony2fuji_adapt_std_lut(in.chars, map.chars, out.chars, size));
        return JNI_TRUE;
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Not enough memory to adapt the LUT");
    } catch (const std::invalid_argument& error) {
        throwJava(env, "java/lang/IllegalArgumentException", error.what());
    } catch (const std::exception& error) {
        throwJava(env, "java/io/IOException", error.what());
    }
    return JNI_FALSE;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_rawlab_android_NativeProcessor_nativeProcess(JNIEnv* env, jobject, jlong id,
    jstring input, jstring lut, jstring output, jfloat strength, jfloat exposure,
    jboolean customWb, jfloat temperature, jfloat tint,
    jfloat contrast, jfloat saturation, jfloat toneCurve, jfloat sharpening,
    jint edge, jboolean interactive, jboolean png) {
    try {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = sessions.find(id);
        if (found == sessions.end()) throw std::invalid_argument("Processor is closed");
        auto* session = found->second.get();
        UtfChars in(env, input), film(env, lut), out(env, output);
        auto request = rawlab::makeRequest(in.chars, film.chars, out.chars, strength,
            exposure, customWb, temperature, tint, contrast, saturation, toneCurve, sharpening,
            edge, png);
        check(sony2fuji_session_set_interactive_preview(session, interactive && !out.chars));
        Buffer buffer;
        check(sony2fuji_process(session, &request, &buffer.value));
        if (output) return nullptr;
        const auto& b = buffer.value;
        const size_t row = static_cast<size_t>(b.width) * 4;
        const size_t bytes = rawlab::previewByteCount(b);
        if (bytes > static_cast<size_t>(std::numeric_limits<jsize>::max()))
            throw std::runtime_error("Invalid preview buffer");
        auto pixels = env->NewByteArray(static_cast<jsize>(bytes));
        if (!pixels) return nullptr;
        for (uint32_t y = 0; y < b.height && !env->ExceptionCheck(); ++y) {
            env->SetByteArrayRegion(pixels, static_cast<jsize>(y * row), static_cast<jsize>(row),
                reinterpret_cast<const jbyte*>(b.data) + y * b.stride_bytes);
        }
        if (env->ExceptionCheck()) return nullptr;
        float kelvin = NAN, wbTint = NAN;
        sony2fuji_session_get_raw_white_balance(session, &kelvin, &wbTint);
        auto type = env->FindClass("com/rawlab/android/NativeFrame");
        if (!type) return nullptr;
        auto constructor = env->GetMethodID(type, "<init>", "(II[BFFI)V");
        if (!constructor) return nullptr;
        return env->NewObject(type, constructor, static_cast<jint>(b.width),
            static_cast<jint>(b.height), pixels, kelvin, wbTint, static_cast<jint>(sony2fuji_session_get_last_backend(session)));
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Not enough memory to develop this RAW");
    } catch (const std::invalid_argument& error) {
        throwJava(env, "java/lang/IllegalArgumentException", error.what());
    } catch (const std::exception& error) {
        throwJava(env, "java/io/IOException", error.what());
    } catch (...) {
        throwJava(env, "java/io/IOException", "RAW processing failed");
    }
    return nullptr;
}
