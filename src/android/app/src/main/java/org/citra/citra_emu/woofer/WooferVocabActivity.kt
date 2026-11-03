// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.woofer

import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updateLayoutParams
import androidx.core.view.updatePadding
import androidx.core.widget.doAfterTextChanged
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.snackbar.Snackbar
import java.security.MessageDigest
import java.util.Locale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.citra.citra_emu.R
import org.citra.citra_emu.databinding.ActivityWooferVocabBinding
import org.citra.citra_emu.databinding.ListItemWooferWordBinding
import org.citra.citra_emu.utils.ThemeUtil
import org.json.JSONObject

/**
 * The word list of all Woofer games (sdmc:/woofer/vocab.jsonl): search, filter
 * by status, tap to hear a word, long press to set the status (new, learning,
 * known) or delete, and export to a tab-separated file that Anki imports.
 */
class WooferVocabActivity : AppCompatActivity() {
    private lateinit var binding: ActivityWooferVocabBinding
    private val all = ArrayList<JSONObject>()
    private val shown = ArrayList<JSONObject>()
    private val adapter = WordAdapter()
    private var speech: WooferSpeech? = null

    private val exporter =
        registerForActivityResult(ActivityResultContracts.CreateDocument(TSV_MIME)) { uri ->
            if (uri != null) {
                export(uri)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeUtil.setTheme(this)
        super.onCreate(savedInstanceState)
        binding = ActivityWooferVocabBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _, insets ->
            val bars = insets.getInsets(
                WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout()
            )
            binding.appbarWooferVocab.updatePadding(
                top = bars.top,
                left = bars.left,
                right = bars.right
            )
            binding.listWords.updatePadding(
                left = bars.left + dp(12),
                right = bars.right + dp(12),
                bottom = bars.bottom + dp(96)
            )
            binding.buttonExport.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                bottomMargin = bars.bottom + dp(16)
                rightMargin = bars.right + dp(16)
            }
            insets
        }

        binding.toolbarWooferVocab.setNavigationOnClickListener { finish() }
        binding.listWords.layoutManager = LinearLayoutManager(this)
        binding.listWords.adapter = adapter
        binding.search.doAfterTextChanged { filter() }
        binding.statusFilter.setOnCheckedStateChangeListener { _, _ -> filter() }
        binding.buttonExport.setOnClickListener { exporter.launch(EXPORT_NAME) }

        speech = WooferSpeech(this) { _ ->
            Toast.makeText(this, R.string.woofer_no_voice, Toast.LENGTH_LONG).show()
        }
        speech?.setLanguage(WooferBridge.hello?.optString("lang") ?: "zh-Hans")
        reload()
    }

    override fun onDestroy() {
        speech?.shutdown()
        speech = null
        super.onDestroy()
    }

    private fun reload() {
        lifecycleScope.launch {
            val words = withContext(Dispatchers.IO) { WooferBridge.loadVocab().values.toList() }
            all.clear()
            all.addAll(words)
            filter()
        }
    }

    private fun statusFilter(): String? = when (binding.statusFilter.checkedChipId) {
        R.id.filter_new -> "new"
        R.id.filter_learning -> "learning"
        R.id.filter_known -> "known"
        else -> null
    }

    private fun filter() {
        val query = binding.search.text?.toString()?.trim()?.lowercase(Locale.ROOT) ?: ""
        val status = statusFilter()
        shown.clear()
        for (e in all.asReversed()) { // newest first
            if (status != null && e.optString("status", "new") != status) {
                continue
            }
            if (query.isNotEmpty()) {
                val haystack = listOf("s", "p", "g", "zh", "en")
                    .joinToString("\n") { e.optString(it) }
                    .lowercase(Locale.ROOT)
                if (!haystack.contains(query) && !stripTones(haystack).contains(query)) {
                    continue
                }
            }
            shown.add(e)
        }
        adapter.notifyDataSetChanged()
        binding.toolbarWooferVocab.subtitle =
            resources.getQuantityString(R.plurals.woofer_word_count, all.size, all.size)
        binding.textEmpty.visibility = if (shown.isEmpty()) View.VISIBLE else View.GONE
        binding.textEmpty.setText(
            if (all.isEmpty()) R.string.woofer_words_empty else R.string.woofer_words_no_match
        )
    }

    private fun askStatus(entry: JSONObject) {
        val labels = arrayOf(
            getString(R.string.woofer_status_new),
            getString(R.string.woofer_status_learning),
            getString(R.string.woofer_status_known),
            getString(R.string.woofer_delete)
        )
        val values = WooferBridge.STATUSES + WooferBridge.STATUS_DELETED
        MaterialAlertDialogBuilder(this)
            .setTitle(entry.optString("s") + "  " + entry.optString("p"))
            .setItems(labels) { _, which -> changeStatus(entry, values[which]) }
            .show()
    }

    private fun changeStatus(entry: JSONObject, status: String) {
        val previous = entry.optString("status", "new")
        lifecycleScope.launch {
            val ok = withContext(Dispatchers.IO) { WooferBridge.setStatus(entry, status) }
            if (!ok) {
                Toast.makeText(
                    this@WooferVocabActivity,
                    R.string.woofer_save_failed,
                    Toast.LENGTH_SHORT
                ).show()
                return@launch
            }
            reload()
            if (status == WooferBridge.STATUS_DELETED) {
                Snackbar.make(binding.root, R.string.woofer_deleted, Snackbar.LENGTH_LONG)
                    .setAction(R.string.woofer_undo) {
                        lifecycleScope.launch {
                            withContext(Dispatchers.IO) { WooferBridge.setStatus(entry, previous) }
                            reload()
                        }
                    }
                    .show()
            }
        }
    }

    private fun export(uri: Uri) {
        val entries = ArrayList(all)
        lifecycleScope.launch {
            val ok = withContext(Dispatchers.IO) {
                try {
                    contentResolver.openOutputStream(uri)?.use {
                        it.write(toTsv(entries).toByteArray(Charsets.UTF_8))
                        true
                    } ?: false
                } catch (e: Exception) {
                    false
                }
            }
            val message = if (ok) {
                resources.getQuantityString(R.plurals.woofer_exported, entries.size, entries.size)
            } else {
                getString(R.string.woofer_export_failed)
            }
            Toast.makeText(this@WooferVocabActivity, message, Toast.LENGTH_LONG).show()
        }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private inner class WordAdapter : RecyclerView.Adapter<WordAdapter.Holder>() {
        inner class Holder(val item: ListItemWooferWordBinding) : RecyclerView.ViewHolder(item.root)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder = Holder(
            ListItemWooferWordBinding.inflate(
                LayoutInflater.from(parent.context),
                parent,
                false
            )
        )

        override fun getItemCount(): Int = shown.size

        override fun onBindViewHolder(holder: Holder, position: Int) {
            val e = shown[position]
            val item = holder.item
            item.textCharacters.text = e.optString("s")
            item.textPinyin.text = e.optString("p")
            item.textMeaning.text = e.optString("g")
            item.textMeaning.visibility =
                if (e.optString("g").isEmpty()) View.GONE else View.VISIBLE
            item.textStatus.text = statusLabel(e.optString("status", "new"))
            val sentence = e.optString("zh")
            val source = listOf(e.optString("loc"), e.optString("game")).filter { it.isNotEmpty() }
            item.textSentence.text = (listOf(sentence) + source).filter { it.isNotEmpty() }
                .joinToString("  ·  ")
            item.textSentence.visibility =
                if (item.textSentence.text.isEmpty()) View.GONE else View.VISIBLE
            item.root.setOnClickListener { speech?.speak(e.optString("s")) }
            item.root.setOnLongClickListener {
                askStatus(e)
                true
            }
        }
    }

    private fun statusLabel(status: String): String = getString(
        when (status) {
            "learning" -> R.string.woofer_status_learning
            "known" -> R.string.woofer_status_known
            else -> R.string.woofer_status_new
        }
    )

    companion object {
        private const val TSV_MIME = "text/tab-separated-values"
        private const val EXPORT_NAME = "woofer-words.tsv"

        /** "yínháng" to "yinhang", so that a search without tone marks finds words too. */
        fun stripTones(text: String): String =
            java.text.Normalizer.normalize(text, java.text.Normalizer.Form.NFD)
                .replace(Regex("\\p{Mn}+"), "")

        /**
         * Anki import file: the header tells Anki (2.1.55 and later) the separator, the
         * field names, the tag column and a stable id per word, so that a later export
         * updates the notes of an earlier one instead of adding them twice.
         */
        fun toTsv(entries: List<JSONObject>): String {
            val sb = StringBuilder()
            sb.append("#separator:tab\n")
            sb.append("#html:false\n")
            sb.append(
                "#columns:Id\tCharacters\tPinyin\tMeaning\tSentence\tEnglish\tPlace\tGame\tStatus\tHSK\tTags\n"
            )
            sb.append("#guid column:1\n")
            sb.append("#tags column:11\n")
            for (e in entries) {
                val status = e.optString("status", "new")
                val game = e.optString("game")
                val hsk = e.optInt("hsk", 0)
                val tags = listOf("woofer", game, status).filter { it.isNotEmpty() }
                    .joinToString(" ") { it.replace(' ', '_') }
                val fields = listOf(
                    guid(WooferBridge.key(e)),
                    e.optString("s"),
                    e.optString("p"),
                    e.optString("g"),
                    e.optString("zh"),
                    e.optString("en"),
                    e.optString("loc"),
                    game,
                    status,
                    if (hsk > 0) hsk.toString() else "",
                    tags
                )
                sb.append(fields.joinToString("\t") { clean(it) })
                sb.append('\n')
            }
            return sb.toString()
        }

        private fun clean(field: String): String =
            field.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ')

        private fun guid(key: String): String {
            val digest = MessageDigest.getInstance("SHA-1").digest(key.toByteArray(Charsets.UTF_8))
            return "woofer-" + digest.take(10).joinToString("") { "%02x".format(it) }
        }
    }
}
