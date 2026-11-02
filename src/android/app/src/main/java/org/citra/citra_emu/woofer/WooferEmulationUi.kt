// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.woofer

import android.view.Gravity
import android.view.Menu
import android.view.MenuItem
import android.view.ViewGroup
import androidx.coordinatorlayout.widget.CoordinatorLayout
import org.citra.citra_emu.R

/**
 * Everything the emulation screen (fragments/EmulationFragment.kt) needs for
 * Woofer games, kept here so that the fragment itself changes as little as
 * possible: the dictionary panel over the game and the in-game menu item that
 * shows or hides it. The menu item only appears once a Woofer game said hello.
 */
class WooferEmulationUi {
    private var panel: WooferPanel? = null
    private var menu: Menu? = null

    private val listener = WooferBridge.Listener { type, _ ->
        if (type == "hello") {
            updateMenu()
        }
    }

    /** Called from onViewCreated: puts the panel over the game. */
    fun attach(root: CoordinatorLayout, inGameMenu: Menu) {
        val newPanel = WooferPanel(root.context)
        val lp = CoordinatorLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )
        lp.gravity = Gravity.TOP or Gravity.START
        root.addView(newPanel, lp)
        panel = newPanel
        menu = inGameMenu
        WooferBridge.addListener(listener)
        updateMenu()
    }

    /** Called from onDestroyView. */
    fun detach() {
        WooferBridge.removeListener(listener)
        panel = null
        menu = null
    }

    /** The in-game menu: true when the item was ours. */
    fun onMenuItemSelected(item: MenuItem): Boolean {
        if (item.itemId != R.id.menu_woofer_panel) {
            return false
        }
        panel?.let { it.setHiddenByUser(!it.isHiddenByUser()) }
        updateMenu()
        return true
    }

    private fun updateMenu() {
        val item = menu?.findItem(R.id.menu_woofer_panel) ?: return
        item.isVisible = WooferBridge.isActive
        val hidden = panel?.isHiddenByUser() ?: false
        item.setTitle(if (hidden) R.string.woofer_panel_show else R.string.woofer_panel_hide)
    }

    companion object {
        /** A new game starts: forget the Woofer game of before. */
        fun onEmulationStarting() = WooferBridge.reset()

        /** The emulation screen closes for good. */
        fun onEmulationFinished() = WooferBridge.reset()
    }
}
