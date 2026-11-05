// Copyright 2026 Hoogveldt-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.woofer

import android.content.Context
import android.view.View
import android.view.ViewGroup
import kotlin.math.max

/** Children side by side, wrapping to the next line like text; each line on a common baseline. */
class WrapLayout(context: Context, private val gap: Int) : ViewGroup(context) {
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val maxWidth = MeasureSpec.getSize(widthMeasureSpec) - paddingLeft - paddingRight
        var x = 0
        var y = 0
        var lineHeight = 0
        var widest = 0
        for (i in 0 until childCount) {
            val child = getChildAt(i)
            if (child.visibility == View.GONE) {
                continue
            }
            child.measure(
                MeasureSpec.makeMeasureSpec(max(maxWidth, 0), MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
            )
            val w = child.measuredWidth
            val h = child.measuredHeight
            if (x > 0 && x + w > maxWidth) {
                x = 0
                y += lineHeight + gap
                lineHeight = 0
            }
            x += w + gap
            widest = max(widest, x - gap)
            lineHeight = max(lineHeight, h)
        }
        setMeasuredDimension(
            resolveSize(widest + paddingLeft + paddingRight, widthMeasureSpec),
            resolveSize(y + lineHeight + paddingTop + paddingBottom, heightMeasureSpec)
        )
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        val maxWidth = r - l - paddingLeft - paddingRight
        var x = 0
        var y = 0
        var lineHeight = 0
        var lineStart = 0

        // Children of one line are bottom-aligned, so words with and without pinyin line up.
        fun placeLine(end: Int) {
            var cx = 0
            for (j in lineStart until end) {
                val child = getChildAt(j)
                if (child.visibility == View.GONE) {
                    continue
                }
                val w = child.measuredWidth
                val h = child.measuredHeight
                val top = paddingTop + y + lineHeight - h
                child.layout(paddingLeft + cx, top, paddingLeft + cx + w, top + h)
                cx += w + gap
            }
        }
        for (i in 0 until childCount) {
            val child = getChildAt(i)
            if (child.visibility == View.GONE) {
                continue
            }
            val w = child.measuredWidth
            if (x > 0 && x + w > maxWidth) {
                placeLine(i)
                y += lineHeight + gap
                x = 0
                lineHeight = 0
                lineStart = i
            }
            x += w + gap
            lineHeight = max(lineHeight, child.measuredHeight)
        }
        placeLine(childCount)
    }
}
