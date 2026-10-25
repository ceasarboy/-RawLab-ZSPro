package com.rawlab.android.engine

import android.os.Bundle
import android.os.SystemClock
import androidx.test.platform.app.InstrumentationRegistry
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Rule
import org.junit.Test
import org.junit.rules.Timeout
import java.io.File
import java.util.concurrent.TimeUnit
import com.rawlab.android.format.ExportFormat
import com.rawlab.android.photo.PhotoStorage
import com.rawlab.android.settings.DenoiseSettings
import com.rawlab.android.settings.EditSettings

/**
 * 导出降噪耗时诊断：同一 RAW 在降噪开/关、CPU/AUTO 与可选 LUT 下各导出一次，记录耗时与后端。
 *
 * 通过 `-e denoiseTimingEnabled true` 启用；`denoiseTimingRaw` 指定设备上的 RAW 路径，
 * `denoiseTimingLut` 可选指定 sRGB LUT（复现“带 LUT 的 CPU 管线 + 降噪”场景）。
 */
class DenoiseExportTimingTest {
    @get:Rule val budget = Timeout(1800, TimeUnit.SECONDS)

    @Test fun measureExportWithAndWithoutDenoise() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val arguments = InstrumentationRegistry.getArguments()
        android.util.Log.i("DenoiseTiming",
            "args=${arguments.size()} enabled=${arguments.getString("denoiseTimingEnabled")} " +
                "raw=${arguments.getString("denoiseTimingRaw")} lut=${arguments.getString("denoiseTimingLut")}")
        check(arguments.getString("denoiseTimingEnabled") == "true") { "Requires -e denoiseTimingEnabled true" }
        val rawPath = checkNotNull(arguments.getString("denoiseTimingRaw")) { "Missing -e denoiseTimingRaw" }
        val raw = File(rawPath)
        check(raw.isFile) { "RAW not found: $raw" }
        val lut = arguments.getString("denoiseTimingLut")?.takeIf { it.isNotBlank() }
            ?.let(::File)?.takeIf { it.isFile }
        val context = instrumentation.targetContext
        // 应用私有目录：外部 files 目录可能由 adb shell 创建，应用无写权限会导致导出报 processing error。
        val outputDir = File(context.filesDir, "denoise-timing").apply { mkdirs() }
        val previewOnly = arguments.getString("denoiseTimingPreviewOnly") == "true"
        val rows = JSONArray()
        val configurations = buildList {
            // CPU 整图降噪在 45MP 上极慢且会换页；这里只测 GPU 可用路径。
            add(Triple(null, NativeProcessor.AUTO, true))
            if (lut != null) {
                // 带 sRGB LUT 时整图走 CPU 管线，但降噪后端应由 GPU 设置决定。
                add(Triple(lut, NativeProcessor.AUTO, true))
            }
        }
        rows.put(JSONObject().put("raw", raw.name).put("rawBytes", raw.length()))
        // 与应用一致：镜头自动裁切需要镜头库，缺库时原生解码会返回处理错误。
        PhotoStorage(context).use { storage ->
            val lensDatabase = storage.lensDatabase()
            for ((look, mode, denoise) in configurations) {
                NativeProcessor(mode, lensDatabase).use { processor ->
                    val settings = EditSettings(denoise = DenoiseSettings(denoise, 40f, 46f, 50f))
                    val name = "export-${mode}-${denoise}-${look?.name ?: "none"}.jpg"
                    val output = File(outputDir, name)
                    val row = JSONObject()
                        .put("lut", look?.name ?: JSONObject.NULL)
                        .put("gpuMode", mode)
                        .put("denoise", denoise)
                    // 与编辑器一致：先预览再导出，同时暴露解码阶段的失败原因。
                    val preview = runCatching {
                        val started = SystemClock.elapsedRealtime()
                        val frame = processor.preview(raw, look, settings, 1600, false)
                        row.put("previewMs", SystemClock.elapsedRealtime() - started)
                            .put("previewBackend", frame.backend)
                            .put("previewSize", "${frame.width}x${frame.height}")
                    }
                    row.put("previewOk", preview.isSuccess)
                    if (preview.isFailure) row.put("previewError", preview.exceptionOrNull().toString())
                    if (!previewOnly) {
                        val started = SystemClock.elapsedRealtime()
                        val exported = runCatching { processor.export(raw, look, settings, output, ExportFormat.JPEG) }
                        row.put("exportMs", SystemClock.elapsedRealtime() - started)
                        if (exported.isSuccess) row.put("backend", exported.getOrThrow()).put("bytes", output.length())
                        else row.put("exportError", exported.exceptionOrNull().toString())
                    }
                    rows.put(row)
                    instrumentation.sendStatus(2, Bundle().apply { putString("stream", "DENOISE_TIMING $row\n") })
                }
            }
        }
        File(outputDir, "denoise-timing.json").writeText(rows.toString(2))
    }
}
