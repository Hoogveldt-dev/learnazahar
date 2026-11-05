// Copyright 2026 Hoogveldt-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.woofer

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** The word list rules of docs/woofer-bridge/PROTOCOL.md, section 4, and the Anki export. */
class WooferWordListTest {
    private fun entry(s: String, p: String, g: String): JSONObject = JSONObject()
        .put("v", 1)
        .put("game", "pokeemerald-zh")
        .put("s", s)
        .put("p", p)
        .put("g", g)
        .put("status", "new")

    private val bank = entry("银行", "yínháng", "bank")
        .put("zh", "我去银行。")
        .put("en", "I go to the bank.")
        .put("loc", "PETALBURG")
        .put("hsk", 3)
        .toString()
    private val walk = entry("行", "xíng", "to walk").toString()
    private val row = entry("行", "háng", "row").toString()

    @Test
    fun lastLinePerWordCounts() {
        val learning = JSONObject(bank).put("status", "learning").toString()
        val words = WooferBridge.parseVocab(listOf(bank, walk, learning).joinToString("\n"))
        assertEquals(2, words.size)
        // a later line changes the word but not its place in the list
        assertEquals(listOf("银行\tyínháng", "行\txíng"), words.keys.toList())
        assertEquals("learning", words["银行\tyínháng"]!!.getString("status"))
    }

    @Test
    fun sameCharactersWithOtherPinyinAreTwoWords() {
        val words = WooferBridge.parseVocab("$walk\n$row\n")
        assertEquals(2, words.size)
    }

    @Test
    fun deletedWordsAndBrokenLinesAreLeftOut() {
        val deleted = JSONObject(walk).put("status", "deleted").toString()
        val text = "$bank\n$walk\n{\"v\":1,\"s\":\"半\n\n$deleted\n"
        val words = WooferBridge.parseVocab(text)
        assertEquals(listOf("银行\tyínháng"), words.keys.toList())
        // saved again after deleting: back, at the end
        val again = WooferBridge.parseVocab("$text$walk\n")
        assertEquals(listOf("银行\tyínháng", "行\txíng"), again.keys.toList())
    }

    @Test
    fun ankiExport() {
        val tsv = WooferVocabActivity.toTsv(
            listOf(JSONObject(bank), JSONObject(walk).put("g", "to walk\tgo"))
        )
        val lines = tsv.trimEnd('\n').split('\n')
        assertEquals("#separator:tab", lines[0])
        assertEquals("#html:false", lines[1])
        assertTrue(lines[2].startsWith("#columns:Id\tCharacters\tPinyin"))
        assertEquals("#guid column:1", lines[3])
        assertEquals("#tags column:11", lines[4])
        assertEquals(7, lines.size)

        val first = lines[5].split('\t')
        assertEquals(11, first.size)
        assertTrue(first[0].startsWith("woofer-"))
        assertEquals(
            listOf("银行", "yínháng", "bank", "我去银行。", "I go to the bank."),
            first.subList(1, 6)
        )
        assertEquals("3", first[9])
        assertEquals("woofer pokeemerald-zh new", first[10])

        // a tab inside a field does not shift the columns
        val second = lines[6].split('\t')
        assertEquals(11, second.size)
        assertEquals("to walk go", second[3])
        assertEquals("", second[9])

        // the id depends on characters and pinyin only, so a later export updates the note
        val later = WooferVocabActivity.toTsv(listOf(JSONObject(bank).put("status", "known")))
        assertEquals(first[0], later.trimEnd('\n').split('\n')[5].split('\t')[0])
        val other = WooferVocabActivity.toTsv(listOf(JSONObject(row)))
        assertFalse(other.contains(second[0]))
    }

    @Test
    fun searchWithoutToneMarks() {
        assertEquals("yinhang", WooferVocabActivity.stripTones("yínháng"))
        assertEquals("nu lu", WooferVocabActivity.stripTones("nǚ lǜ"))
        assertEquals("银行", WooferVocabActivity.stripTones("银行"))
    }

    @Test
    fun speechLanguage() {
        assertEquals(java.util.Locale.SIMPLIFIED_CHINESE, WooferSpeech.localeFor("zh-Hans"))
        assertEquals(java.util.Locale.SIMPLIFIED_CHINESE, WooferSpeech.localeFor("zh"))
        assertEquals(java.util.Locale.TRADITIONAL_CHINESE, WooferSpeech.localeFor("zh-Hant"))
        assertEquals("ja", WooferSpeech.localeFor("ja").language)
    }
}
