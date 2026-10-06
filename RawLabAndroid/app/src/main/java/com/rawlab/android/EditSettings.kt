package com.rawlab.android

data class EditSettings(
    val film: String = "neutral",
    val strength: Float = 1f,
    val exposure: Float = 0f,
    val customWb: Boolean = false,
    val temperature: Float = 6500f,
    val tint: Float = 0f,
    val contrast: Float = 1f,
    val saturation: Float = 1f,
    val toneCurve: Float = 0f,
    val sharpening: Float = 0f,
    val filmB: String? = null,
    val strengthB: Float = 0f,
) {
    init {
        require(strength.isFinite() && strength in 0f..1f)
        require(exposure.isFinite() && exposure in -5f..5f)
        require(temperature.isFinite() && temperature in 2000f..50000f)
        require(tint.isFinite() && tint in -150f..150f)
        require(contrast.isFinite() && contrast in 0f..2f)
        require(saturation.isFinite() && saturation in 0f..2f)
        require(toneCurve.isFinite() && toneCurve in -1f..1f)
        require(sharpening.isFinite() && sharpening in 0f..2f)
        require(strengthB.isFinite() && strengthB in 0f..1f)
    }
    fun reset() = EditSettings(film = film)
}

data class Film(val id: String, val name: String, val file: String?) {
    companion object {
        val all = listOf(
            Film("neutral", "中性", null),
            Film("provia", "PROVIA", "PROVIA"), Film("velvia", "Velvia", "Velvia"),
            Film("astia", "ASTIA", "ASTIA"), Film("classic-chrome", "CLASSIC CHROME", "CLASSIC-CHROME"),
            Film("classic-neg", "CLASSIC Neg.", "CLASSIC-Neg."), Film("reala-ace", "REALA ACE", "REALA-ACE"),
            Film("pro-neg-std", "PRO Neg. Std", "PRO-Neg.Std"), Film("eterna", "ETERNA", "ETERNA"),
            Film("eterna-bb", "ETERNA BLEACH BYPASS", "ETERNA-BB"), Film("acros", "ACROS", "ACROS"),
        )
    }
}
