// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.woofer

import android.content.Context
import android.speech.tts.TextToSpeech
import java.util.Locale

/**
 * Pronunciation with Android's text to speech, in the language the game teaches.
 * @param onUnavailable called (once) when the phone has no voice for that language
 */
class WooferSpeech(context: Context, private val onUnavailable: (Locale) -> Unit = {}) {
    private var tts: TextToSpeech? = null
    private var initialised = false
    private var available = false
    private var warned = false
    private var pending: String? = null
    private var locale: Locale = Locale.SIMPLIFIED_CHINESE

    init {
        tts = TextToSpeech(context.applicationContext) { status ->
            initialised = status == TextToSpeech.SUCCESS
            if (initialised) {
                applyLocale()
                pending?.let { if (available) speak(it) }
            }
            pending = null
        }
    }

    /** The language from the game's "hello" (BCP 47, e.g. zh-Hans). */
    fun setLanguage(tag: String?) {
        if (tag.isNullOrEmpty()) {
            return
        }
        val wanted = localeFor(tag)
        if (wanted != locale) {
            locale = wanted
            warned = false
            if (initialised) {
                applyLocale()
            }
        }
    }

    fun speak(text: String?) {
        val engine = tts ?: return
        if (text.isNullOrEmpty()) {
            return
        }
        if (!initialised) {
            pending = text
            return
        }
        if (!available) {
            if (!warned) {
                warned = true
                onUnavailable(locale)
            }
            return
        }
        engine.speak(text, TextToSpeech.QUEUE_FLUSH, null, "woofer")
    }

    fun shutdown() {
        tts?.shutdown()
        tts = null
        initialised = false
        available = false
    }

    private fun applyLocale() {
        val result = tts?.setLanguage(locale) ?: TextToSpeech.LANG_NOT_SUPPORTED
        available = result != TextToSpeech.LANG_MISSING_DATA &&
            result != TextToSpeech.LANG_NOT_SUPPORTED
    }

    companion object {
        /** zh, zh-Hans and zh-CN speak Mandarin as in mainland China; zh-Hant as in Taiwan. */
        fun localeFor(tag: String): Locale = when {
            tag.startsWith("zh-Hant") || tag == "zh-TW" -> Locale.TRADITIONAL_CHINESE
            tag.startsWith("zh") -> Locale.SIMPLIFIED_CHINESE
            else -> Locale.forLanguageTag(tag)
        }
    }
}
