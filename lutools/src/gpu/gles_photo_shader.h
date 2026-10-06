#pragma once

namespace sony2fuji {
inline constexpr const char* kPhotoComputeShader = R"glsl(#version 310 es
precision highp float;
precision highp int;
layout(local_size_x = 128) in;
layout(std430, binding = 0) readonly buffer Source { float src[]; };
layout(std430, binding = 1) writeonly buffer Destination { float dst[]; };
layout(binding = 0) uniform highp sampler3D film;
layout(binding = 1) uniform highp sampler3D filmB;
uniform uvec2 sourceSize;
uniform uvec2 destinationSize;
uniform uint sourceStartRow;
uniform uint destinationStartRow;
uniform uint pixelCount;
uniform uint outputOffset;
uniform int stage;
uniform bool useLut;
uniform bool lutOnDisplay;
uniform bool useLutB;
uniform int lutSize;
uniform int lutSizeB;
uniform float strengthB;
uniform vec3 domainMinB;
uniform vec3 domainMaxB;
uniform mat3 toSrgb;
uniform mat3 toFGamut;
uniform vec3 exposureWB;
uniform float strength;
uniform vec3 domainMin;
uniform vec3 domainMax;
uniform vec4 tone; // contrast, saturation, highlights, shadows
uniform float curve;

vec3 readPixel(uvec2 xy) {
    uint i = ((xy.y - sourceStartRow) * sourceSize.x + xy.x) * 3u;
    return vec3(src[i], src[i + 1u], src[i + 2u]);
}
float neutral(float x) {
    if (x <= 0.0) return 0.0;
    const float gray = 0.1845;
    float linear = 1.0 / (1.0 + (1.0 - gray) / gray * pow(gray / x, 1.5));
    return linear <= 0.0031308 ? 12.92 * linear : 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
}
float flog2(float x) {
    return clamp(x < 0.00088899597 ? 8.799461 * x + 0.092864 :
        log(x * 5.555556 + 0.064829) / log(10.0) * 0.245281 + 0.384316, 0.0, 1.0);
}
// 四面体插值：a >= b >= c 为单元内三个分数坐标的降序。格胞按主对角线切成 6 个
// 四面体，点落在哪个由三分量大小次序决定，只需 4 个角点。相比三线性（8 采样、
// 逐轴独立加权），沿对角面插值，饱和区色相不随亮度轴漂移。
vec3 tetra(vec3 c0, vec3 c1, vec3 c2, vec3 c3, float a, float b, float c) {
    return (1.0 - a) * c0 + (a - b) * c1 + (b - c) * c2 + c * c3;
}
vec3 lookup(vec3 x) {
    vec3 p = clamp((x - domainMin) / (domainMax - domainMin), 0.0, 1.0) * float(lutSize - 1);
    ivec3 low = ivec3(floor(p));
    ivec3 high = min(low + 1, ivec3(lutSize - 1));
    vec3 t = p - vec3(low);
    if (t.x >= t.y) {
        if (t.y >= t.z)      // x >= y >= z: 000 100 110 111
            return tetra(texelFetch(film, low, 0).rgb,
                texelFetch(film, ivec3(high.x, low.y, low.z), 0).rgb,
                texelFetch(film, ivec3(high.x, high.y, low.z), 0).rgb,
                texelFetch(film, high, 0).rgb, t.x, t.y, t.z);
        else if (t.x >= t.z) // x >= z > y: 000 100 101 111
            return tetra(texelFetch(film, low, 0).rgb,
                texelFetch(film, ivec3(high.x, low.y, low.z), 0).rgb,
                texelFetch(film, ivec3(high.x, low.y, high.z), 0).rgb,
                texelFetch(film, high, 0).rgb, t.x, t.z, t.y);
        // z > x >= y: 000 001 101 111
        return tetra(texelFetch(film, low, 0).rgb,
            texelFetch(film, ivec3(low.x, low.y, high.z), 0).rgb,
            texelFetch(film, ivec3(high.x, low.y, high.z), 0).rgb,
            texelFetch(film, high, 0).rgb, t.z, t.x, t.y);
    }
    if (t.x >= t.z)          // y > x >= z: 000 010 110 111
        return tetra(texelFetch(film, low, 0).rgb,
            texelFetch(film, ivec3(low.x, high.y, low.z), 0).rgb,
            texelFetch(film, ivec3(high.x, high.y, low.z), 0).rgb,
            texelFetch(film, high, 0).rgb, t.y, t.x, t.z);
    if (t.y >= t.z)          // y >= z > x: 000 010 011 111
        return tetra(texelFetch(film, low, 0).rgb,
            texelFetch(film, ivec3(low.x, high.y, low.z), 0).rgb,
            texelFetch(film, ivec3(low.x, high.y, high.z), 0).rgb,
            texelFetch(film, high, 0).rgb, t.y, t.z, t.x);
    // z > y > x: 000 001 011 111
    return tetra(texelFetch(film, low, 0).rgb,
        texelFetch(film, ivec3(low.x, low.y, high.z), 0).rgb,
        texelFetch(film, ivec3(low.x, high.y, high.z), 0).rgb,
        texelFetch(film, high, 0).rgb, t.z, t.y, t.x);
}
vec3 lookupB(vec3 x) {
    vec3 p = clamp((x - domainMinB) / (domainMaxB - domainMinB), 0.0, 1.0) * float(lutSizeB - 1);
    ivec3 low = ivec3(floor(p));
    ivec3 high = min(low + 1, ivec3(lutSizeB - 1));
    vec3 t = p - vec3(low);
    if (t.x >= t.y) {
        if (t.y >= t.z)      // x >= y >= z
            return tetra(texelFetch(filmB, low, 0).rgb,
                texelFetch(filmB, ivec3(high.x, low.y, low.z), 0).rgb,
                texelFetch(filmB, ivec3(high.x, high.y, low.z), 0).rgb,
                texelFetch(filmB, high, 0).rgb, t.x, t.y, t.z);
        else if (t.x >= t.z) // x >= z > y
            return tetra(texelFetch(filmB, low, 0).rgb,
                texelFetch(filmB, ivec3(high.x, low.y, low.z), 0).rgb,
                texelFetch(filmB, ivec3(high.x, low.y, high.z), 0).rgb,
                texelFetch(filmB, high, 0).rgb, t.x, t.z, t.y);
        // z > x >= y
        return tetra(texelFetch(filmB, low, 0).rgb,
            texelFetch(filmB, ivec3(low.x, low.y, high.z), 0).rgb,
            texelFetch(filmB, ivec3(high.x, low.y, high.z), 0).rgb,
            texelFetch(filmB, high, 0).rgb, t.z, t.x, t.y);
    }
    if (t.x >= t.z)          // y > x >= z
        return tetra(texelFetch(filmB, low, 0).rgb,
            texelFetch(filmB, ivec3(low.x, high.y, low.z), 0).rgb,
            texelFetch(filmB, ivec3(high.x, high.y, low.z), 0).rgb,
            texelFetch(filmB, high, 0).rgb, t.y, t.x, t.z);
    if (t.y >= t.z)          // y >= z > x
        return tetra(texelFetch(filmB, low, 0).rgb,
            texelFetch(filmB, ivec3(low.x, high.y, low.z), 0).rgb,
            texelFetch(filmB, ivec3(low.x, high.y, high.z), 0).rgb,
            texelFetch(filmB, high, 0).rgb, t.y, t.z, t.x);
    // z > y > x
    return tetra(texelFetch(filmB, low, 0).rgb,
        texelFetch(filmB, ivec3(low.x, low.y, high.z), 0).rgb,
        texelFetch(filmB, ivec3(low.x, high.y, high.z), 0).rgb,
        texelFetch(filmB, high, 0).rgb, t.z, t.y, t.x);
}
float toneCurve(float x) {
    float s = sign(curve) * pow(abs(curve), 1.2);
    vec3 points = clamp(vec3(0.25 - s * 0.2, 0.5 + s * 0.05, 0.75 + s * 0.2), 0.0, 1.0);
    x = clamp(x, 0.0, 1.0);
    if (x <= 0.25) return x * 4.0 * points.x;
    if (x <= 0.5) return mix(points.x, points.y, (x - 0.25) * 4.0);
    if (x <= 0.75) return mix(points.y, points.z, (x - 0.5) * 4.0);
    return mix(points.z, 1.0, (x - 0.75) * 4.0);
}
vec3 evaluate(vec3 inputColor) {
    vec3 linear = (toSrgb * inputColor) * exposureWB;
    vec3 pixel = vec3(neutral(linear.r), neutral(linear.g), neutral(linear.b));
    if (useLut) {
        if (lutOnDisplay) {
            pixel = mix(pixel, lookup(pixel), strength);
        } else {
            vec3 fgamut = toFGamut * linear;
            pixel = mix(pixel, lookup(vec3(flog2(fgamut.r), flog2(fgamut.g), flog2(fgamut.b))), strength);
        }
    }
    // Look stacking: the second slot consumes the blended display encoding.
    if (useLutB) pixel = mix(pixel, lookupB(pixel), strengthB);
    if (tone.z != 0.0 || tone.w != 0.0) {
        float luminance = dot(pixel, vec3(0.2126, 0.7152, 0.0722));
        float adjusted = luminance;
        if (tone.w != 0.0) adjusted = pow(clamp(adjusted, 0.0, 1.0), 1.0 - tone.w * 0.5);
        if (tone.z != 0.0) adjusted = 1.0 - pow(1.0 - clamp(adjusted, 0.0, 1.0), 1.0 - tone.z * 0.5);
        pixel = luminance > 0.0 ? pixel * (adjusted / luminance) : vec3(adjusted);
    }
    if (curve != 0.0) pixel = vec3(toneCurve(pixel.r), toneCurve(pixel.g), toneCurve(pixel.b));
    if (tone.x != 1.0 || tone.y != 1.0) {
        pixel = (pixel - 0.5) * tone.x + 0.5;
        float luminance = dot(pixel, vec3(0.2126, 0.7152, 0.0722));
        pixel = vec3(luminance) + (pixel - luminance) * tone.y;
    }
    return pixel;
}
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= pixelCount) return;
    uvec2 xy = uvec2(i % destinationSize.x, destinationStartRow + i / destinationSize.x);
    vec3 value;
    if (stage == 1 || (stage == 2 && all(equal(sourceSize, destinationSize)))) {
        value = evaluate(readPixel(xy));
    } else {
        vec2 scale = vec2(destinationSize.x == 1u ? 0.0 : float(sourceSize.x - 1u) / float(destinationSize.x - 1u),
            destinationSize.y == 1u ? 0.0 : float(sourceSize.y - 1u) / float(destinationSize.y - 1u));
        vec2 p = min(vec2(xy) * scale, vec2(sourceSize - 1u));
        uvec2 low = uvec2(floor(p));
        uvec2 high = min(low + 1u, sourceSize - 1u);
        vec2 t = p - vec2(low);
        vec3 a = readPixel(low), b = readPixel(uvec2(high.x, low.y));
        vec3 c = readPixel(uvec2(low.x, high.y)), d = readPixel(high);
        // FINAL resizes display output; PREVIEW resizes linear input first.
        if (stage == 2) { a = evaluate(a); b = evaluate(b); c = evaluate(c); d = evaluate(d); }
        value = mix(mix(a, b, t.x), mix(c, d, t.x), t.y);
        if (stage == 3) value = evaluate(value);
    }
    uint outIndex = (outputOffset + i) * 3u;
    dst[outIndex] = value.r; dst[outIndex + 1u] = value.g; dst[outIndex + 2u] = value.b;
}
)glsl";
}
