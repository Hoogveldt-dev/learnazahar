// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// The title id of a homebrew game, for its savestates (core/woofer/homebrew_id.h).

#include <cstring>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "core/woofer/homebrew_id.h"

namespace {
/// An SMDH with an English short title (UTF-16).
std::vector<u8> Smdh(const char* title) {
    std::vector<u8> smdh(0x36C0, 0);
    std::memcpy(smdh.data(), "SMDH", 4);
    for (std::size_t i = 0; title[i] != '\0'; ++i) {
        smdh[0x8 + 0x200 + 2 * i] = static_cast<u8>(title[i]);
    }
    return smdh;
}
} // namespace

TEST_CASE("Homebrew id: from the SMDH title, the same wherever the file is", "[woofer]") {
    const auto smdh = Smdh("PokemonWoofer");
    const u64 a = Woofer::HomebrewTitleId(smdh, "/sdcard/games/PokemonWoofer.3dsx");
    const u64 b = Woofer::HomebrewTitleId(smdh, "/elsewhere/renamed.3dsx");
    CHECK(a == b);
    CHECK((a & 0xFFFF'0000'0000'0000ULL) == Woofer::HOMEBREW_ID_PREFIX);
    CHECK(a != Woofer::HomebrewTitleId(Smdh("Another game"), "/sdcard/games/PokemonWoofer.3dsx"));
}

TEST_CASE("Homebrew id: from the file name without an SMDH", "[woofer]") {
    const u64 plain = Woofer::HomebrewTitleId({}, "/sdcard/3ds/PokemonWoofer.3dsx");
    // An Android content URI and another folder or case: the same name, the same id.
    CHECK(plain == Woofer::HomebrewTitleId(
                       {}, "content://com.android.externalstorage.documents/tree/primary%3AGames/"
                           "document/primary%3AGames%2FPokemonWoofer.3dsx"));
    CHECK(plain == Woofer::HomebrewTitleId({}, "C:\\games\\pokemonwoofer.3DSX"));
    CHECK(plain != Woofer::HomebrewTitleId({}, "/sdcard/3ds/Other.3dsx"));
    // An SMDH without a title counts as none.
    CHECK(plain == Woofer::HomebrewTitleId(Smdh(""), "/x/PokemonWoofer.3dsx"));
    CHECK((plain & 0xFFFF'0000'0000'0000ULL) == Woofer::HOMEBREW_ID_PREFIX);
}
