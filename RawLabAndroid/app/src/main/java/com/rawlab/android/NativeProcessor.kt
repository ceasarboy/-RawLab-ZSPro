package com.rawlab.android

import android.graphics.Bitmap
import java.io.File
import java.nio.ByteBuffer

class NativeFrame(val width: Int, val height: Int, val pixels: ByteArray, val temperature: Float, val tint: Float, val backend: Int) {
    fun bitmap(): Bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888).also {
        it.copyPixelsFromBuffer(ByteBuffer.wrap(pixels))
    }
}

class NativeProcessor(mode: Int = AUTO) : AutoCloseable {
    private var handle = nativeCreate(mode).also { check(it != 0L) }

    @Synchronized
    fun setGpuMode(mode: Int) {
        check(handle != 0L)
        require(mode in CPU..FORCE)
        nativeSetGpuMode(handle, mode)
    }

    @Synchronized
    fun setLutB(path: File?, strength: Float) {
        check(handle != 0L)
        require(strength.isFinite() && strength in 0f..1f)
        nativeSetLutB(handle, path?.path, strength)
    }

    @Synchronized
    fun setLutMode(mode: Int) {
        check(handle != 0L)
        require(mode in LUT_PHOTO..LUT_FLOG2_INPUT)
        nativeSetLutMode(handle, mode)
    }

    /** Renders a fixed reference scene through the LUT (DISPLAY mode) for library thumbnails. */
    @Synchronized
    fun renderLutThumb(path: String): Bitmap? = try {
        val bytes = nativeRenderLutThumb(path) ?: return null
        val bmp = Bitmap.createBitmap(THUMB_W, THUMB_H, Bitmap.Config.ARGB_8888)
        bmp.copyPixelsFromBuffer(java.nio.ByteBuffer.wrap(bytes))
        bmp
    } catch (_: Throwable) { null }

    @Synchronized
    fun bakeLookLut(pathA: String, strengthA: Float, pathB: String?, strengthB: Float, outPath: String, size: Int = 65): Boolean {
        require(strengthA in 0f..1f && strengthB in 0f..1f && size in 17..65)
        return nativeBakeLookLut(pathA, strengthA, pathB, strengthB, outPath, size)
    }

    @Synchronized
    fun adaptStdLut(inPath: String, mapPath: String, outPath: String, size: Int = 65): Boolean {
        require(size in 17..65)
        return nativeAdaptStdLut(inPath, mapPath, outPath, size)
    }

    @Synchronized
    fun preview(input: File, lut: File?, settings: EditSettings, edge: Int, interactive: Boolean): NativeFrame {
        check(handle != 0L) { "Processor is closed" }
        return checkNotNull(nativeProcess(handle, input.path, lut?.path, null, settings.strength,
            settings.exposure, settings.customWb, settings.temperature, settings.tint,
            settings.contrast, settings.saturation, settings.toneCurve, settings.sharpening,
            edge, interactive, false))
    }

    @Synchronized
    fun export(input: File, lut: File?, settings: EditSettings, output: File, png: Boolean) {
        check(handle != 0L) { "Processor is closed" }
        require(input.canonicalPath != output.canonicalPath)
        nativeProcess(handle, input.path, lut?.path, output.path, settings.strength,
            settings.exposure, settings.customWb, settings.temperature, settings.tint,
            settings.contrast, settings.saturation, settings.toneCurve, settings.sharpening,
            0, false, png)
    }

    @Synchronized
    override fun close() {
        if (handle != 0L) { nativeDestroy(handle); handle = 0L }
    }

    private external fun nativeCreate(mode: Int): Long
    private external fun nativeSetGpuMode(handle: Long, mode: Int)
    private external fun nativeSetLutMode(handle: Long, mode: Int)
    private external fun nativeSetLutB(handle: Long, path: String?, strength: Float)
    private external fun nativeRenderLutThumb(path: String): ByteArray?
    private external fun nativeBakeLookLut(pathA: String, strengthA: Float, pathB: String?, strengthB: Float, outPath: String, size: Int): Boolean
    private external fun nativeAdaptStdLut(inPath: String, mapPath: String, outPath: String, size: Int): Boolean
    private external fun nativeDestroy(handle: Long)
    private external fun nativeProcess(handle: Long, input: String, lut: String?, output: String?,
        strength: Float, exposure: Float, customWb: Boolean, temperature: Float, tint: Float,
        contrast: Float, saturation: Float, toneCurve: Float, sharpening: Float,
        edge: Int, interactive: Boolean, png: Boolean): NativeFrame?

    companion object {
        const val THUMB_W = 192
        const val THUMB_H = 128
        const val CPU = 0
        const val AUTO = 1
        const val FORCE = 2
        // Mirrors sony2fuji_lut_mode: how the loaded LUT expects its input encoded.
        const val LUT_PHOTO = 0        // Built-in film contract (F-Log2 to film outputs).
        const val LUT_DISPLAY = 1      // Generic look LUT on the neutral display encoding.
        const val LUT_FLOG2_INPUT = 2  // Technical conversion consuming F-Gamut/F-Log2.
        init { System.loadLibrary("rawlab-jni") }
    }
}
