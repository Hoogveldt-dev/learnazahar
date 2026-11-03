// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.woofer

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.res.ColorStateList
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.RippleDrawable
import android.text.TextUtils
import android.util.TypedValue
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.preference.PreferenceManager
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import org.citra.citra_emu.R
import org.json.JSONArray
import org.json.JSONObject

/**
 * The dictionary panel over the game. It shows the page the Woofer game has in
 * its message box: words to tap with pinyin, meaning and HSK level,
 * pronunciation, saving to the shared word list and the English. It only
 * appears for a game that said hello, and it knows nothing about any one game.
 *
 * The panel floats: drag it by its title bar to where it hides the least (the
 * place is kept per orientation), tap the title to fold it, and use the in-game
 * menu to hide it altogether. Without text in the message box it shrinks to
 * its title bar.
 */
@SuppressLint("ViewConstructor")
class WooferPanel(context: Context) :
    LinearLayout(context),
    WooferBridge.Listener {
    private val prefs = PreferenceManager.getDefaultSharedPreferences(context)

    private val title: TextView
    private val toggle: TextView
    private val body: ScrollView
    private val hint: TextView
    private val sentence: WrapLayout
    private val english: TextView
    private val detail: LinearLayout
    private val word: TextView
    private val pinyin: TextView
    private val hsk: TextView
    private val meaning: TextView
    private val actions: LinearLayout
    private val save: TextView
    private val englishButton: TextView
    private val pinyinButton: TextView

    private val saved = HashSet<String>()
    private var speech: WooferSpeech? = null
    private var line: JSONObject? = null
    private var selected = -1
    private var collapsed = prefs.getBoolean(PREF_COLLAPSED, false)
    private var showPinyin = prefs.getBoolean(PREF_PINYIN, false)
    private var hiddenByUser = prefs.getBoolean(PREF_HIDDEN, false)
    private var dragging = false

    init {
        orientation = VERTICAL
        background = rounded(BG, dp(10).toFloat())
        elevation = dp(6).toFloat()
        isClickable = true // touches on the panel do not reach the game
        val pad = dp(8)
        setPadding(pad, dp(4), pad, pad)

        val header = LinearLayout(context)
        header.gravity = Gravity.CENTER_VERTICAL
        header.minimumHeight = dp(36)
        title = text(13f, SOFT)
        title.setSingleLine(true)
        title.ellipsize = TextUtils.TruncateAt.END
        title.text = context.getString(R.string.woofer_panel_title_default)
        header.addView(title, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f))
        val words = pill(context.getString(R.string.woofer_words_short))
        words.setOnClickListener { openWordList() }
        header.addView(words, marginStart(words, dp(6)))
        toggle = pill("")
        toggle.setOnClickListener { setCollapsed(!collapsed) }
        header.addView(toggle, marginStart(toggle, dp(6)))
        addView(header)
        setUpDragging(header)

        val content = LinearLayout(context)
        content.orientation = VERTICAL
        hint = text(14f, SOFT)
        hint.text = context.getString(R.string.woofer_panel_waiting)
        hint.setPadding(0, dp(2), 0, dp(4))
        content.addView(hint)
        sentence = WrapLayout(context, dp(4))
        sentence.setPadding(0, dp(4), 0, 0)
        content.addView(sentence)
        english = text(14f, SOFT)
        english.setPadding(0, dp(6), 0, 0)
        content.addView(english)

        detail = LinearLayout(context)
        detail.orientation = VERTICAL
        detail.setPadding(0, dp(8), 0, dp(4))
        val top = LinearLayout(context)
        top.gravity = Gravity.BOTTOM
        word = text(28f, ACCENT)
        top.addView(word)
        pinyin = text(19f, FG)
        pinyin.setPadding(dp(10), 0, 0, dp(3))
        top.addView(pinyin, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f))
        hsk = text(12f, SOFT)
        hsk.setPadding(dp(6), 0, 0, dp(4))
        top.addView(hsk)
        detail.addView(top)
        meaning = text(15f, FG)
        detail.addView(meaning)
        content.addView(detail)

        actions = LinearLayout(context)
        actions.setPadding(0, dp(6), 0, 0)
        val speakWord = pill(context.getString(R.string.woofer_speak_word))
        speakWord.setOnClickListener { speak(selectedWord(), "s") }
        val speakLine = pill(context.getString(R.string.woofer_speak_line))
        speakLine.setOnClickListener { speak(line, "zh") }
        save = pill(context.getString(R.string.woofer_save))
        save.setOnClickListener { saveSelected() }
        pinyinButton = pill(context.getString(R.string.woofer_pinyin))
        pinyinButton.setOnClickListener { setShowPinyin(!showPinyin) }
        englishButton = pill(context.getString(R.string.woofer_english))
        englishButton.setOnClickListener { setShowEnglish(english.visibility != VISIBLE) }
        val buttons = listOf(speakWord, speakLine, save, pinyinButton, englishButton)
        for ((i, b) in buttons.withIndex()) {
            val lp = LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
            if (i > 0) {
                lp.marginStart = dp(4)
            }
            b.gravity = Gravity.CENTER
            actions.addView(b, lp)
        }
        content.addView(actions)

        body = ScrollView(context)
        body.isVerticalScrollBarEnabled = true
        body.addView(content)
        addView(body, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))

        english.visibility = if (prefs.getBoolean(PREF_ENGLISH, false)) VISIBLE else GONE
        visibility = GONE
        applyCollapsed()
        showLine(null)
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        speech = WooferSpeech(context) { _ ->
            Toast.makeText(context, R.string.woofer_no_voice, Toast.LENGTH_LONG).show()
        }
        WooferBridge.addListener(this)
        WooferBridge.hello?.let { onHello(it) }
        WooferBridge.line?.let { showLine(it) }
    }

    override fun onDetachedFromWindow() {
        WooferBridge.removeListener(this)
        WooferBridge.setPanelVisible(false)
        speech?.shutdown()
        speech = null
        super.onDetachedFromWindow()
    }

    override fun onWooferMessage(type: String, message: JSONObject) {
        when (type) {
            "hello" -> onHello(message)

            "line" -> showLine(message)

            "clear" -> showLine(null)

            "saved" -> {
                saved.add(WooferBridge.key(message))
                updateDetail()
            }
        }
    }

    /** Shows or hides the panel for the rest of this and later games (in-game menu). */
    fun setHiddenByUser(hidden: Boolean) {
        hiddenByUser = hidden
        prefs.edit().putBoolean(PREF_HIDDEN, hidden).apply()
        updateVisibility()
    }

    fun isHiddenByUser(): Boolean = hiddenByUser

    private fun onHello(hello: JSONObject) {
        title.text = hello.optString("title", hello.optString("game"))
        speech?.setLanguage(hello.optString("lang"))
        saved.clear()
        // The word list is a file (perhaps behind Android's storage framework): read it aside.
        Thread {
            val keys = WooferBridge.loadVocab().keys
            post {
                saved.addAll(keys)
                updateDetail()
            }
        }.start()
        updateVisibility()
    }

    private fun updateVisibility() {
        visibility = if (WooferBridge.isActive && !hiddenByUser) VISIBLE else GONE
        updateHostFlags()
    }

    private fun updateHostFlags() {
        WooferBridge.setPanelVisible(visibility == VISIBLE && !collapsed)
    }

    private fun setCollapsed(value: Boolean) {
        collapsed = value
        prefs.edit().putBoolean(PREF_COLLAPSED, value).apply()
        applyCollapsed()
    }

    private fun applyCollapsed() {
        body.visibility = if (collapsed) GONE else VISIBLE
        toggle.text = if (collapsed) "+" else "–"
        toggle.contentDescription = context.getString(
            if (collapsed) R.string.woofer_unfold else R.string.woofer_fold
        )
        updateHostFlags()
    }

    // ------------------------------------------------------------------
    // Content
    // ------------------------------------------------------------------

    private fun showLine(newLine: JSONObject?) {
        line = newLine
        selected = -1
        sentence.removeAllViews()
        val words = newLine?.optJSONArray("w")
        if (words != null) {
            for (i in 0 until words.length()) {
                val w = words.optJSONObject(i) ?: continue
                sentence.addView(chip(i, w))
            }
        } else if (newLine != null) {
            // A game may leave out "w"; then the page is shown as it is.
            val plain = text(20f, FG)
            plain.text = newLine.optString("zh")
            sentence.addView(plain)
        }
        english.text = newLine?.optString("en") ?: ""
        val hasLine = newLine != null
        hint.visibility = if (hasLine) GONE else VISIBLE
        sentence.visibility = if (hasLine) VISIBLE else GONE
        detail.visibility = if (hasLine) VISIBLE else GONE
        actions.visibility = if (hasLine) VISIBLE else GONE
        english.visibility =
            if (hasLine && prefs.getBoolean(PREF_ENGLISH, false) && english.text.isNotEmpty()) {
                VISIBLE
            } else {
                GONE
            }
        body.scrollTo(0, 0)
        updateDetail()
    }

    private fun chip(index: Int, w: JSONObject): View {
        val box = LinearLayout(context)
        box.orientation = VERTICAL
        box.gravity = Gravity.CENTER_HORIZONTAL
        val p = w.optString("p")
        val top = text(11f, SOFT)
        top.text = p
        top.tag = TAG_PINYIN
        top.visibility = if (showPinyin && p.isNotEmpty()) VISIBLE else GONE
        box.addView(top)
        val characters = text(22f, FG)
        characters.text = w.optString("s")
        box.addView(characters)
        if (p.isNotEmpty()) {
            box.background = rounded(CHIP, dp(4).toFloat())
            box.setPadding(dp(5), dp(1), dp(5), dp(2))
            box.setOnClickListener { select(index) }
            box.setOnLongClickListener {
                select(index)
                saveSelected()
                true
            }
        } else {
            box.setPadding(0, dp(1), 0, dp(2))
        }
        return box
    }

    private fun select(index: Int) {
        selected = index
        for (i in 0 until sentence.childCount) {
            val chip = sentence.getChildAt(i)
            if (chip.background != null) {
                chip.background = rounded(if (i == index) CHIP_SELECTED else CHIP, dp(4).toFloat())
            }
        }
        updateDetail()
        speak(selectedWord(), "s")
    }

    private fun selectedWord(): JSONObject? {
        val words: JSONArray = line?.optJSONArray("w") ?: return null
        return if (selected >= 0) words.optJSONObject(selected) else null
    }

    private fun updateDetail() {
        val w = selectedWord()
        word.text = w?.optString("s") ?: ""
        pinyin.text = w?.optString("p")
            ?: if (line != null) context.getString(R.string.woofer_tap_a_word) else ""
        meaning.text = w?.optString("g") ?: ""
        meaning.visibility = if (meaning.text.isEmpty()) GONE else VISIBLE
        val level = w?.optInt("hsk", 0) ?: 0
        hsk.text = when {
            level <= 0 -> ""
            level > 6 -> "HSK 7-9"
            else -> "HSK $level"
        }
        val isSaved = w != null && saved.contains(WooferBridge.key(w))
        enable(save, w != null && !isSaved)
        save.text = context.getString(if (isSaved) R.string.woofer_saved else R.string.woofer_save)
        enable(englishButton, line?.optString("en")?.isNotEmpty() == true)
        pinyinButton.isSelected = showPinyin
        pinyinButton.alpha = if (showPinyin) 1f else 0.75f
    }

    private fun setShowEnglish(show: Boolean) {
        prefs.edit().putBoolean(PREF_ENGLISH, show).apply()
        english.visibility = if (show && english.text.isNotEmpty()) VISIBLE else GONE
    }

    private fun setShowPinyin(show: Boolean) {
        showPinyin = show
        prefs.edit().putBoolean(PREF_PINYIN, show).apply()
        for (i in 0 until sentence.childCount) {
            val top = sentence.getChildAt(i).findViewWithTag<TextView>(TAG_PINYIN) ?: continue
            top.visibility = if (show && top.text.isNotEmpty()) VISIBLE else GONE
        }
        updateDetail()
    }

    private fun saveSelected() {
        val w = selectedWord() ?: return
        if (saved.contains(WooferBridge.key(w))) {
            return
        }
        val current = line
        Thread {
            val ok = WooferBridge.saveWord(w, current)
            post {
                if (ok) {
                    saved.add(WooferBridge.key(w))
                    updateDetail()
                } else {
                    Toast.makeText(context, R.string.woofer_save_failed, Toast.LENGTH_SHORT).show()
                }
            }
        }.start()
    }

    private fun speak(source: JSONObject?, field: String) {
        source?.let { speech?.speak(it.optString(field)) }
    }

    private fun openWordList() {
        context.startActivity(Intent(context, WooferVocabActivity::class.java))
    }

    // ------------------------------------------------------------------
    // Size and place
    // ------------------------------------------------------------------

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val parentView = parent as? View
        val parentWidth = parentView?.width?.takeIf { it > 0 }
            ?: MeasureSpec.getSize(widthMeasureSpec)
        val parentHeight = parentView?.height?.takeIf { it > 0 }
            ?: MeasureSpec.getSize(heightMeasureSpec)
        val width = if (isLandscape()) {
            min(dp(380), (parentWidth * 0.42f).toInt())
        } else {
            min(dp(520), parentWidth - dp(16))
        }
        val height = (parentHeight * MAX_HEIGHT).toInt()
        super.onMeasure(
            MeasureSpec.makeMeasureSpec(max(width, dp(200)), MeasureSpec.EXACTLY),
            MeasureSpec.makeMeasureSpec(max(height, dp(80)), MeasureSpec.AT_MOST)
        )
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        super.onLayout(changed, l, t, r, b)
        if (!dragging) {
            applyPosition()
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setUpDragging(handle: View) {
        val slop = ViewConfiguration.get(context).scaledTouchSlop
        var downX = 0f
        var downY = 0f
        var startX = 0f
        var startY = 0f
        handle.setOnTouchListener { _, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    downX = event.rawX
                    downY = event.rawY
                    startX = x
                    startY = y
                    dragging = false
                }

                MotionEvent.ACTION_MOVE -> {
                    val dx = event.rawX - downX
                    val dy = event.rawY - downY
                    if (!dragging && (abs(dx) > slop || abs(dy) > slop)) {
                        dragging = true
                    }
                    if (dragging) {
                        x = clamp(startX + dx, maxX())
                        y = clamp(startY + dy, maxY())
                    }
                }

                MotionEvent.ACTION_UP -> {
                    if (dragging) {
                        savePosition()
                        dragging = false
                    } else {
                        setCollapsed(!collapsed) // a tap on the title folds the panel
                    }
                }

                MotionEvent.ACTION_CANCEL -> dragging = false
            }
            true
        }
    }

    private fun maxX(): Float = max(0, ((parent as? View)?.width ?: 0) - width).toFloat()
    private fun maxY(): Float = max(0, ((parent as? View)?.height ?: 0) - height).toFloat()

    private fun clamp(value: Float, maximum: Float) = min(max(value, 0f), maximum)

    private fun positionKey(axis: String) =
        "woofer_panel_${axis}_" + if (isLandscape()) "landscape" else "portrait"

    /** Places the panel at its kept place, as a fraction of the free room. */
    private fun applyPosition() {
        val fx = prefs.getFloat(positionKey("x"), if (isLandscape()) 1f else 0.5f)
        val fy = prefs.getFloat(positionKey("y"), if (isLandscape()) 0f else 0.5f)
        // x and y are relative to the parent; the layout puts the panel at its top left.
        translationX = 0f
        translationY = 0f
        x = clamp(fx * maxX(), maxX())
        y = clamp(fy * maxY(), maxY())
    }

    private fun savePosition() {
        val fx = if (maxX() > 0) x / maxX() else 0f
        val fy = if (maxY() > 0) y / maxY() else 0f
        prefs.edit()
            .putFloat(positionKey("x"), clamp(fx, 1f))
            .putFloat(positionKey("y"), clamp(fy, 1f))
            .apply()
    }

    private fun isLandscape() =
        resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

    // ------------------------------------------------------------------
    // Views
    // ------------------------------------------------------------------

    private fun dp(value: Int): Int = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP,
        value.toFloat(),
        resources.displayMetrics
    ).toInt()

    private fun rounded(colour: Int, radius: Float): GradientDrawable {
        val d = GradientDrawable()
        d.setColor(colour)
        d.cornerRadius = radius
        return d
    }

    private fun text(sp: Float, colour: Int): TextView {
        val t = TextView(context)
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp)
        t.setTextColor(colour)
        t.typeface = Typeface.DEFAULT
        return t
    }

    /** A small button that looks the same in every theme. */
    private fun pill(label: String): TextView {
        val b = text(13f, FG)
        b.text = label
        b.setSingleLine(true)
        b.setPadding(dp(10), dp(6), dp(10), dp(6))
        b.background = RippleDrawable(
            ColorStateList.valueOf(RIPPLE),
            rounded(CHIP, dp(14).toFloat()),
            null
        )
        b.isClickable = true
        b.isFocusable = true
        return b
    }

    private fun marginStart(view: View, margin: Int): LayoutParams {
        val lp = LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT)
        lp.marginStart = margin
        view.minimumWidth = dp(36)
        return lp
    }

    private fun enable(view: View, enabled: Boolean) {
        view.isEnabled = enabled
        view.alpha = if (enabled) 1f else 0.4f
    }

    companion object {
        private const val BG = -0x1ff0ebe8 // 0xE00F1418
        private const val FG = Color.WHITE
        private const val SOFT = -0x4f4738 // 0xFFB0B8C8
        private const val ACCENT = -0x3fc0 // 0xFFFFC040
        private const val CHIP = -0xd9cfc4 // 0xFF26303C
        private const val CHIP_SELECTED = -0xc5b59a // 0xFF3A4A66
        private const val RIPPLE = 0x40FFFFFF
        private const val TAG_PINYIN = "pinyin"
        private const val MAX_HEIGHT = 0.55f

        private const val PREF_COLLAPSED = "woofer_panel_collapsed"
        private const val PREF_HIDDEN = "woofer_panel_hidden"
        private const val PREF_PINYIN = "woofer_panel_pinyin"
        private const val PREF_ENGLISH = "woofer_panel_english"
    }
}
