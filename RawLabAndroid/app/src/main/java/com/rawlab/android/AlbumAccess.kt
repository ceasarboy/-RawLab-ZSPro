package com.rawlab.android

object AlbumAccess {
    enum class Level { NONE, PARTIAL, FULL }
    fun permissions(api: Int): List<String> = when {
        api >= 34 -> listOf("android.permission.READ_MEDIA_IMAGES", "android.permission.READ_MEDIA_VISUAL_USER_SELECTED")
        api >= 33 -> listOf("android.permission.READ_MEDIA_IMAGES")
        else -> listOf("android.permission.READ_EXTERNAL_STORAGE")
    }
    fun level(api: Int, granted: Set<String>): Level = when {
        permissions(api).first() in granted -> Level.FULL
        api >= 34 && "android.permission.READ_MEDIA_VISUAL_USER_SELECTED" in granted -> Level.PARTIAL
        else -> Level.NONE
    }
}

object RawFiles {
    private val extensions = setOf("dng", "arw", "arq", "srf", "sr2", "cr2", "cr3", "crw", "nef", "nrw", "raf", "orf", "rw2", "pef", "srw", "3fr", "iiq", "rwl", "mos", "mrw", "raw", "kdc", "dcr", "erf", "x3f")
    private val mimes = setOf("image/x-adobe-dng", "image/x-sony-arw", "image/x-canon-cr2", "image/x-canon-cr3", "image/x-nikon-nef", "image/x-fuji-raf", "image/x-panasonic-rw2", "image/x-olympus-orf", "image/x-pentax-pef")
    fun accepts(name: String, mime: String?) = name.substringAfterLast('.', "").lowercase() in extensions || mime?.lowercase() in mimes
}
