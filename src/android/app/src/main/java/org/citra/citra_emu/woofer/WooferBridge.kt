// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.woofer

import android.os.Handler
import android.os.Looper
import androidx.annotation.Keep
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone
import java.util.concurrent.CopyOnWriteArrayList
import org.citra.citra_emu.utils.Log
import org.json.JSONException
import org.json.JSONObject

/**
 * Messages of Woofer games (docs/woofer-bridge/PROTOCOL.md) for the frontend.
 * Nothing here is specific to one game: every Woofer game sends the same
 * messages, so a panel built on this object works for all of them.
 */
@Keep
object WooferBridge {
    const val HOST_PANEL_VISIBLE = 1

    /** Status values of the word list, in the order the UI offers them. */
    val STATUSES = listOf("new", "learning", "known")
    const val STATUS_DELETED = "deleted"

    /** Called on the main thread. */
    fun interface Listener {
        fun onWooferMessage(type: String, message: JSONObject)
    }

    private val listeners = CopyOnWriteArrayList<Listener>()
    private val main = Handler(Looper.getMainLooper())

    /** The "hello" message of the running game, or null when it is not a Woofer game. */
    @Volatile
    var hello: JSONObject? = null
        private set

    /** The page now in the message box ("line"), or null. */
    @Volatile
    var line: JSONObject? = null
        private set

    /** True once the running game said hello: it is a Woofer game. */
    val isActive: Boolean
        get() = hello != null

    val gameId: String
        get() = hello?.optString("game", "") ?: ""

    fun addListener(listener: Listener) {
        listeners.add(listener)
    }

    fun removeListener(listener: Listener) {
        listeners.remove(listener)
    }

    /** Tells the game whether our dictionary panel is on screen. */
    fun setPanelVisible(visible: Boolean) {
        setHostFlags(if (visible) HOST_PANEL_VISIBLE else 0)
    }

    /** Forgets the running game; called when emulation starts or stops. */
    fun reset() {
        hello = null
        line = null
    }

    /** From the emulation thread (jni/woofer.cpp), one message of the game. */
    @Keep
    @JvmStatic
    fun onMessage(type: String, json: String) {
        val message = try {
            JSONObject(json)
        } catch (e: JSONException) {
            Log.warning("[WooferBridge] Ignored a \"$type\" message that is not JSON")
            return
        }
        when (type) {
            "hello" -> {
                hello = message
                line = null
            }
            "line" -> line = message
            "clear" -> line = null
        }
        main.post {
            for (listener in listeners) {
                listener.onWooferMessage(type, message)
            }
        }
    }

    // ------------------------------------------------------------------
    // Word list: sdmc:/woofer/vocab.jsonl, shared by all Woofer games
    // ------------------------------------------------------------------

    /** Key of a word: characters and pinyin (行 as xíng and as háng are two words). */
    fun key(word: JSONObject): String = word.optString("s") + "\t" + word.optString("p")

    /**
     * Saves a word of the current page.
     * @param word one item of the "w" list of a "line" message
     * @param line that "line" message, or null
     */
    fun saveWord(word: JSONObject, line: JSONObject?): Boolean {
        return try {
            val entry = JSONObject()
            entry.put("v", 1)
            entry.put("game", gameId)
            entry.put("s", word.getString("s"))
            entry.put("p", word.optString("p", ""))
            entry.put("g", word.optString("g", ""))
            if (word.has("hsk")) {
                entry.put("hsk", word.optInt("hsk"))
            }
            if (line != null) {
                entry.put("zh", line.optString("zh", ""))
                entry.put("en", line.optString("en", ""))
                entry.put("loc", line.optString("loc", ""))
            }
            entry.put("at", now())
            entry.put("status", "new")
            appendVocab(entry.toString())
        } catch (e: JSONException) {
            false
        }
    }

    /** Changes the status of a saved word (new, learning, known, deleted). */
    fun setStatus(entry: JSONObject, status: String): Boolean {
        return try {
            val copy = JSONObject(entry.toString())
            copy.put("status", status)
            copy.put("at", now())
            appendVocab(copy.toString())
        } catch (e: JSONException) {
            false
        }
    }

    /**
     * The word list of all games: per word the last line, without deleted
     * words, in the order they were first saved. Reads a file: not on the
     * main thread for long lists.
     */
    fun loadVocab(): LinkedHashMap<String, JSONObject> = parseVocab(readVocab())

    fun parseVocab(text: String): LinkedHashMap<String, JSONObject> {
        val words = LinkedHashMap<String, JSONObject>()
        for (raw in text.split('\n')) {
            val trimmed = raw.trim()
            if (trimmed.isEmpty()) {
                continue
            }
            val entry = try {
                JSONObject(trimmed)
            } catch (e: JSONException) {
                continue // a half-written line
            }
            val key = key(entry)
            if (entry.optString("status") == STATUS_DELETED) {
                words.remove(key)
            } else {
                // A later line replaces the word but keeps its place in the list.
                words[key] = entry
            }
        }
        return words
    }

    private fun now(): String {
        val format = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US)
        format.timeZone = TimeZone.getTimeZone("UTC")
        return format.format(Date())
    }

    // jni/woofer.cpp
    external fun setHostFlags(flags: Int)
    external fun lastHello(): String
    external fun lastLine(): String
    external fun appendVocab(jsonObject: String): Boolean
    external fun readVocab(): String
}
