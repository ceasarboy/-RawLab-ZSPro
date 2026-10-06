package com.rawlab.android

import android.app.Application
import android.graphics.Bitmap
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.io.File

enum class Operation { NONE, IMPORT, PICK_EXPORT, EXPORT, IMPORT_LUT }
data class LutComposeState(
    val lutA: String? = null,
    val strengthA: Float = 1f,
    val lutB: String? = null,
    val strengthB: Float = 0.8f,
    val preview: android.graphics.Bitmap? = null,
    val busy: Boolean = false,
)
data class PreviewPair(val neutral: Bitmap, val result: Bitmap, val temperature: Float, val tint: Float)
data class LutChoice(val file: File?, val mode: Int?)
data class EditorState(
    val photo: ImportedPhoto? = null,
    val edits: EditSettings = EditSettings(),
    val preview: PreviewPair? = null,
    val operation: Operation = Operation.NONE,
    val rendering: Boolean = false,
    val exact: Boolean = false,
    val error: String? = null,
    val message: String? = null,
    val gpuEnabled: Boolean = true,
    val userLuts: List<UserLut> = emptyList(),
    val baseBusy: String? = null,
) {
    val canExport get() = photo != null && preview != null && exact && !rendering && operation == Operation.NONE
    val controlsEnabled get() = photo != null && operation == Operation.NONE
}

private sealed interface Work {
    data class Import(val uri: Uri, val gpuMode: Int) : Work
    data class ComposePreview(val edits: EditSettings, val gpuMode: Int) : Work
    data class Preview(val photo: ImportedPhoto, val edits: EditSettings, val interactive: Boolean,
        val gpuMode: Int, val importing: Boolean = false) : Work
    data class Export(val photo: ImportedPhoto, val edits: EditSettings, val destination: Uri?, val png: Boolean, val gpuMode: Int) : Work
}
private sealed interface WorkResult {
    data class Preview(val request: Work.Preview, val pair: PreviewPair) : WorkResult
    data class ComposePreview(val request: Work.ComposePreview, val bitmap: android.graphics.Bitmap) : WorkResult
    data object Export : WorkResult
}

class EditorViewModel(application: Application) : AndroidViewModel(application) {
    val storage = PhotoStorage(application)
    val luts = UserLutStore(application)
    private var processor: NativeProcessor? = null
    private var currentRevision = 0L
    private val mutable = MutableStateFlow(EditorState(userLuts = luts.all()))
    val state = mutable.asStateFlow()
    private val composeMutable = MutableStateFlow(LutComposeState())
    val compose = composeMutable.asStateFlow()
    private var disposed = false
    private val queue = RenderQueue<Work, WorkResult>(::perform, { revision, result ->
        viewModelScope.launch {
            if (!disposed && revision == currentRevision) accept(result)
        }
    }, { processor?.close(); storage.close() })

    private fun engine() = processor ?: NativeProcessor().also { processor = it }

    /** Resolves the selected film id to a LUT file and its declared input encoding. */
    private fun lutChoice(filmId: String): LutChoice = when {
        filmId.startsWith("user:") -> luts.find(filmId)?.let { LutChoice(it.file, it.mode) } ?: LutChoice(null, null)
        else -> LutChoice(storage.filmPath(filmId), NativeProcessor.LUT_PHOTO)
    }

    private fun applyLut(native: NativeProcessor, choice: LutChoice) {
        // Session-level declaration: every render states how its LUT input is encoded.
        native.setLutMode(choice.mode ?: NativeProcessor.LUT_PHOTO)
    }

    private fun applyLutB(native: NativeProcessor, edits: EditSettings) {
        val file = edits.filmB?.let { luts.find(it)?.file }
        native.setLutB(file, if (file == null) 0f else edits.strengthB)
    }

    /** Switches a user LUT between the neutral base and the Panasonic STD base. */
    fun setLutBase(id: String, std: Boolean) {
        val current = mutable.value
        val lut = luts.find(id) ?: return
        if (std == lut.stdBase || current.baseBusy != null) return
        mutable.value = current.copy(baseBusy = id,
            message = getApplication<Application>().getString(
                if (std) R.string.base_adapting else R.string.base_restoring))
        viewModelScope.launch {
            var success = false
            val processor = NativeProcessor()
            try {
                val adaptedFile = File(lut.file.parentFile, "adapted-${id.removePrefix("user:")}.cube")
                val mapFile = stdMapFile()
                if (std) {
                    val original = lut.stdOriginal ?: lut.file
                    success = processor.adaptStdLut(original.path, mapFile.path, adaptedFile.path, 65)
                    if (success) luts.setBase(id, true, adaptedFile)
                } else {
                    luts.restoreOriginal(id)
                    success = true
                }
            } catch (error: Throwable) {
                success = false
            } finally {
                processor.close()
            }
            mutable.value = mutable.value.copy(userLuts = luts.all(), baseBusy = null,
                message = getApplication<Application>().getString(
                    if (success) R.string.base_done else R.string.base_failed))
            if (success && current.edits.film == id && current.photo != null) edit(current.edits)
        }
    }

    private fun perform(work: Work): WorkResult = when (work) {
        is Work.Import -> {
            val photo = storage.import(work.uri)
            try { perform(Work.Preview(photo, EditSettings(), false, work.gpuMode, importing = true)) }
            catch (error: Throwable) { photo.file.delete(); throw error }
        }
        is Work.ComposePreview -> {
            val photo = mutable.value.photo ?: return WorkResult.ComposePreview(work, work.edits.let { android.graphics.Bitmap.createBitmap(8, 8, android.graphics.Bitmap.Config.ARGB_8888) })
            val native = engine()
            native.setGpuMode(work.gpuMode)
            val choice = lutChoice(work.edits.film)
            applyLut(native, choice)
            applyLutB(native, work.edits)
            val frame = native.preview(photo.file, choice.file, work.edits, 800, false)
            WorkResult.ComposePreview(work, frame.bitmap())
        }
        is Work.Preview -> {
            val edge = if (work.interactive) 1000 else 1600
            val native = engine()
            native.setGpuMode(work.gpuMode)
            val choice = lutChoice(work.edits.film)
            applyLut(native, choice)
            applyLutB(native, work.edits)
            val neutral = native.preview(work.photo.file, null, work.edits, edge, work.interactive)
            val film = if (choice.file == null || work.edits.strength == 0f) neutral
                else native.preview(work.photo.file, choice.file, work.edits, edge, work.interactive)
            val neutralBitmap = neutral.bitmap()
            WorkResult.Preview(work, PreviewPair(neutralBitmap,
                if (neutral === film) neutralBitmap else film.bitmap(), neutral.temperature, neutral.tint))
        }
        is Work.Export -> {
            val output = storage.temporaryOutput(work.png)
            try {
                engine().setGpuMode(work.gpuMode)
                val choice = lutChoice(work.edits.film)
                applyLut(engine(), choice)
                applyLutB(engine(), work.edits)
                engine().export(work.photo.file, choice.file, work.edits, output, work.png)
                if (work.destination == null) storage.saveAlbum(output, work.png)
                else storage.saveDocument(output, work.destination, work.photo.uri)
            } finally { output.delete() }
            WorkResult.Export
        }
    }

    private fun accept(result: Result<WorkResult>) {
        result.fold({ value ->
            when (value) {
                is WorkResult.ComposePreview -> composeMutable.value =
                    composeMutable.value.copy(preview = value.bitmap, busy = false)
                is WorkResult.Preview -> {
                    val request = value.request
                    if (request.importing) {
                        mutable.value.photo?.file?.takeIf { it != request.photo.file }?.delete()
                    }
                    val settings = if (!request.edits.customWb && value.pair.temperature.isFinite())
                        request.edits.copy(temperature = value.pair.temperature.coerceIn(2000f, 50000f), tint = value.pair.tint.coerceIn(-150f, 150f))
                    else request.edits
                    mutable.value = mutable.value.copy(photo = request.photo, edits = settings, preview = value.pair,
                        operation = Operation.NONE, rendering = false, exact = !request.interactive, error = null)
                }
                WorkResult.Export -> mutable.value = mutable.value.copy(operation = Operation.NONE, message = text(R.string.export_done))
            }
        }, { error ->
            composeMutable.value = composeMutable.value.copy(busy = false)
            mutable.value = mutable.value.copy(operation = Operation.NONE, rendering = false,
                exact = mutable.value.exact,
                error = failure(error))
        })
    }

    fun importPhoto(uri: Uri?) {
        if (uri == null || mutable.value.operation != Operation.NONE) return
        mutable.value = mutable.value.copy(operation = Operation.IMPORT, rendering = true, error = null)
        currentRevision = queue.submit(Work.Import(uri, gpuMode()))
    }

    fun edit(edits: EditSettings, interactive: Boolean = false) {
        val current = mutable.value
        if (!current.controlsEnabled) return
        mutable.value = current.copy(edits = edits, rendering = true, exact = false, error = null)
        currentRevision = queue.submit(Work.Preview(current.photo!!, edits, interactive, gpuMode()))
    }

    fun retry() { edit(mutable.value.edits) }
    fun reset() { edit(mutable.value.edits.reset()) }

    // base: "auto" = 按文件标注嗅探；"std" = 全部按松下 STD 底座适配；"neutral" = 全部按中性
    fun importLuts(uris: List<Uri>, base: String = "auto") {
        if (uris.isEmpty()) return
        viewModelScope.launch {
            // Heavy native work (adaptation, thumbnails) must leave the main thread.
            val result = kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.Default) {
                var ok = 0
            var adaptedCount = 0
            var failed = 0
            val thumbProcessor = NativeProcessor()
            try {
                for (uri in uris) {
                    var imported: UserLut? = null
                    try {
                        imported = luts.import(uri, null)
                        if (imported == null) failed++
                    } catch (error: Throwable) {
                        failed++
                    }
                    val lut = imported ?: continue
                    try {
                        val wantsAdapt = base == "std" || (base == "auto" && lut.stdBase)
                        if (wantsAdapt) {
                            val mapFile = stdMapFile()
                            val adaptedFile = File(lut.file.parentFile, "adapted-${lut.id.removePrefix("user:")}.cube")
                            if (thumbProcessor.adaptStdLut(lut.file.path, mapFile.path, adaptedFile.path, 65)) {
                                luts.setBase(lut.id, true, adaptedFile)
                                adaptedCount++
                            } else failed++
                        }
                        // Render the thumb from the CURRENT file (adapted when applicable).
                        val current = luts.find(lut.id) ?: lut
                        thumbProcessor.renderLutThumb(current.file.path)?.let { bmp ->
                            val png = java.io.ByteArrayOutputStream()
                            bmp.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, png)
                            luts.attachThumb(lut.id, png.toByteArray())
                        }
                        ok++
                    } catch (error: Throwable) {
                        failed++
                    }
                }
                } finally {
                    thumbProcessor.close()
                }
                Triple(ok, adaptedCount, failed)
            }
            val (ok, adaptedCount, failed) = result
            mutable.value = mutable.value.copy(userLuts = luts.all(),
                message = getApplication<Application>().getString(R.string.lut_import_done, ok, adaptedCount, failed))
        }
    }

    private fun stdMapFile(): File {
        val cached = File(getApplication<Application>().cacheDir, "STD_to_VLOG_2nd.cube")
        if (!cached.exists() || cached.length() == 0L) {
            getApplication<Application>().assets.open("std/STD_to_VLOG_2nd.cube").use { input ->
                cached.outputStream().use { input.copyTo(it) }
            }
        }
        return cached
    }

    fun deleteLut(id: String) {
        luts.delete(id)
        val current = mutable.value
        val edits = if (current.edits.film == id) current.edits.copy(film = "neutral") else current.edits
        mutable.value = current.copy(userLuts = luts.all(), edits = edits)
        if (current.edits.film != edits.film && current.photo != null) edit(edits)
    }

    fun setUserLutMode(id: String, mode: Int) {
        require(mode == NativeProcessor.LUT_DISPLAY || mode == NativeProcessor.LUT_FLOG2_INPUT)
        luts.setMode(id, mode)
        val current = mutable.value
        mutable.value = current.copy(userLuts = luts.all())
        if (current.edits.film == id && current.photo != null) edit(current.edits)
    }

    private fun gpuMode() = if (mutable.value.gpuEnabled) NativeProcessor.AUTO else NativeProcessor.CPU
    fun setGpuEnabled(enabled: Boolean) {
        if (mutable.value.operation != Operation.NONE) return
        mutable.value = mutable.value.copy(gpuEnabled = enabled)
        if (mutable.value.photo != null) edit(mutable.value.edits)
    }

    fun beginExport(): Boolean {
        if (!mutable.value.canExport) return false
        mutable.value = mutable.value.copy(operation = Operation.PICK_EXPORT, error = null)
        return true
    }
    fun cancelExport() {
        if (mutable.value.operation == Operation.PICK_EXPORT) mutable.value = mutable.value.copy(operation = Operation.NONE)
    }
    fun export(destination: Uri?, png: Boolean) {
        val current = mutable.value
        if (current.operation != Operation.PICK_EXPORT || current.photo == null) return
        mutable.value = current.copy(operation = Operation.EXPORT)
        currentRevision = queue.submit(Work.Export(current.photo, current.edits, destination, png, gpuMode()))
    }
    // ---- LUT composer ----
    fun composeRender(lutA: String?, strengthA: Float, lutB: String?, strengthB: Float) {
        val current = mutable.value
        if (current.photo == null) return
        val edits = current.edits.copy(
            film = lutA ?: "neutral", strength = strengthA,
            filmB = lutB, strengthB = if (lutB == null) 0f else strengthB)
        composeMutable.value = composeMutable.value.copy(busy = true)
        currentRevision = queue.submit(Work.ComposePreview(edits, gpuMode()))
    }

    fun saveComposedLut(lutA: String?, strengthA: Float, lutB: String?, strengthB: Float, name: String): String? {
        val a = luts.find(lutA ?: return null) ?: return null
        val b = lutB?.let { luts.find(it) }
        val outFile = File(getApplication<Application>().cacheDir, "baked-${System.currentTimeMillis()}.cube")
        val processor = NativeProcessor()
        val ok = try {
            processor.bakeLookLut(a.file.path, strengthA, b?.file?.path,
                if (b == null) 0f else strengthB, outFile.path, 65)
        } finally {
            processor.close()
        }
        if (!ok) { outFile.delete(); return null }
        val saved = luts.registerFile(outFile, name)
        outFile.delete()
        composeMutable.value = LutComposeState()
        mutable.value = mutable.value.copy(userLuts = luts.all(),
            edits = mutable.value.edits.copy(film = saved.id))
        if (mutable.value.photo != null) edit(mutable.value.edits)
        return saved.name
    }

    fun dismissMessage() { mutable.value = mutable.value.copy(message = null) }
    private fun text(id: Int) = getApplication<Application>().getString(id)
    private fun failure(error: Throwable): String = if (error is OutOfMemoryError) text(R.string.memory_error)
        else text(R.string.process_error) + "\n" + (error.message ?: error.javaClass.simpleName)

    override fun onCleared() {
        disposed = true
        queue.close()
    }
}
