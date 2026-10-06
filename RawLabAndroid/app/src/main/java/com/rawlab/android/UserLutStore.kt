package com.rawlab.android

import android.content.Context
import android.net.Uri
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/** A user-imported .cube LUT with its declared input encoding (NativeProcessor.LUT_*). */
data class UserLut(val id: String, val name: String, val file: File, val mode: Int,
    val thumb: File? = null, val stdBase: Boolean = false,
    /** Untouched import copy, kept when the file was adapted to the STD base. */
    val stdOriginal: File? = null)

/**
 * User LUT library under filesDir/user-luts. Imports copy the document into app
 * storage (session-only edits still never rewrite originals) and sniff the
 * #Gamma contract comment to suggest the input encoding, per the research
 * contract: a LUT without a declared encoding is applied on the neutral
 * display encoding; F-Log2-input LUTs must be declared as such.
 */
class UserLutStore(private val context: Context) {
    private val directory = File(context.filesDir, "user-luts")
    private val indexFile = File(directory, "index.json")

    @Synchronized
    fun all(): List<UserLut> {
        if (!indexFile.exists()) return emptyList()
        return runCatching {
            val array = JSONArray(indexFile.readText())
            (0 until array.length()).mapNotNull { i ->
                val item = array.getJSONObject(i)
                val file = File(item.getString("file"))
                if (!file.exists()) return@mapNotNull null
                // Legacy debug imports stored bare "lut-<ts>" ids; the editor routes
                // on the "user:" prefix, so normalize on read.
                val id = item.getString("id").let { if (it.startsWith("user:")) it else "user:$it" }
                val thumb = if (item.has("thumb")) File(item.getString("thumb")) else null
                val stdOriginal = if (item.has("stdOriginal")) File(item.getString("stdOriginal")) else null
                UserLut(id, item.getString("name"), file, item.getInt("mode"), thumb?.takeIf { it.exists() },
                    item.optBoolean("stdBase"), stdOriginal?.takeIf { it.exists() })
            }
        }.getOrDefault(emptyList())
    }

    /** Copies the document and returns the imported LUT, or null if the uri is unreadable. */
    @Synchronized
    fun import(uri: Uri, name: String?): UserLut? {
        directory.mkdirs()
        val id = "user:lut-${System.currentTimeMillis()}"
        val destination = File(directory, "${id.removePrefix("user:")}.cube")
        val header = StringBuilder()
        try {
            val input = context.contentResolver.openInputStream(uri) ?: return null
            input.use { stream ->
                destination.outputStream().use { output ->
                    val buffer = ByteArray(64 * 1024)
                    var first = true
                    while (true) {
                        val read = stream.read(buffer)
                        if (read <= 0) break
                        if (first) {
                            // Keep the first 4 KiB for contract sniffing before writing on.
                            header.append(String(buffer, 0, minOf(read, 4096), Charsets.ISO_8859_1))
                            first = false
                        }
                        output.write(buffer, 0, read)
                    }
                }
            }
        } catch (error: Throwable) {
            destination.delete()
            throw error
        }
        if (destination.length() == 0L) { destination.delete(); return null }
        val mode = if (FLOG2_HEADER.matcher(header).find()) NativeProcessor.LUT_FLOG2_INPUT
            else NativeProcessor.LUT_DISPLAY
        // Panasonic STD-based look: header declares the STD photo-style base.
        val stdBase = STD_HEADER.matcher(header).find()
        // SAF lastPathSegment looks like "primary:test-kit/portra-warm.cube";
        // keep only the base file name for the picker label.
        val display = (name ?: uri.lastPathSegment ?: "LUT")
            .substringAfterLast('/').removeSuffix(".cube")
            .ifBlank { "LUT" }.take(40)
        val list = all() + UserLut(id, display, destination, mode, null, stdBase, destination)
        writeIndex(list)
        return list.last()
    }

    /** Registers an existing file (e.g. a baked LUT) into the library. */
    @Synchronized
    fun registerFile(file: File, name: String): UserLut {
        val id = "user:lut-${System.currentTimeMillis()}"
        val destination = File(directory, "${id.removePrefix("user:")}.cube")
        file.inputStream().use { input -> destination.outputStream().use { input.copyTo(it) } }
        val list = all() + UserLut(id, name.take(40).ifBlank { "LUT" }, destination,
            NativeProcessor.LUT_DISPLAY)
        writeIndex(list)
        return list.last()
    }

    /** Attaches a rendered thumbnail (PNG bytes) to a library entry. */
    @Synchronized
    fun attachThumb(id: String, png: ByteArray) {
        val target = all().firstOrNull { it.id == id } ?: return
        val thumbFile = File(directory, "thumb-${id.removePrefix("user:")}.png")
        thumbFile.outputStream().use { it.write(png) }
        val list = all().map { if (it.id == id) it.copy(thumb = thumbFile) else it }
        writeIndex(list)
    }

    /** Re-points the entry at adapted/original content after a base switch. */
    @Synchronized
    fun setBase(id: String, std: Boolean, adaptedFile: File) {
        val current = all().firstOrNull { it.id == id } ?: return
        val original = current.stdOriginal ?: current.file.also {
            it.copyTo(File(directory, "orig-${id.removePrefix("user:")}.cube"), overwrite = true)
        }
        val list = all().map {
            if (it.id == id) it.copy(file = adaptedFile, stdBase = std, stdOriginal = original) else it
        }
        writeIndex(list)
    }

    /** Restores the untouched import copy (neutral base). */
    @Synchronized
    fun restoreOriginal(id: String) {
        val current = all().firstOrNull { it.id == id } ?: return
        val original = current.stdOriginal ?: return
        original.copyTo(current.file, overwrite = true)
        val list = all().map { if (it.id == id) it.copy(stdBase = false) else it }
        writeIndex(list)
    }

    @Synchronized
    fun setMode(id: String, mode: Int) {
        val list = all().map { if (it.id == id) it.copy(mode = mode) else it }
        writeIndex(list)
    }

    @Synchronized
    fun delete(id: String) {
        val target = all().firstOrNull { it.id == id } ?: return
        target.file.delete()
        writeIndex(all().filterNot { it.id == id })
    }

    fun find(id: String): UserLut? = all().firstOrNull { it.id == id }

    private fun writeIndex(list: List<UserLut>) {
        directory.mkdirs()
        val array = JSONArray()
        for (lut in list) {
            val item = JSONObject().put("id", lut.id).put("name", lut.name)
                .put("file", lut.file.path).put("mode", lut.mode)
            if (lut.thumb != null) item.put("thumb", lut.thumb.path)
            if (lut.stdBase) item.put("stdBase", true)
            if (lut.stdOriginal != null) item.put("stdOriginal", lut.stdOriginal.path)
            array.put(item)
        }
        val temp = File(directory, "index.json.tmp")
        temp.writeText(array.toString())
        if (!temp.renameTo(indexFile)) {
            indexFile.delete()
            if (!temp.renameTo(indexFile)) temp.delete()
        }
    }

    companion object {
        // "#Gamma:F-Log2 to ..." / "#Gamma: FLog2C..." style contract comments.
        private val FLOG2_HEADER = Regex("(?i)#\\s*Gamma\\s*:\\s*F-?Log2\\b").toPattern()
        // Panasonic STD photo-style base declaration (look LUTs authored on STD).
        private val STD_HEADER = Regex("(?i)#\\s*LUMIXPHOTOSTYLE\\s*:?\\s*STD\\b").toPattern()
    }
}
