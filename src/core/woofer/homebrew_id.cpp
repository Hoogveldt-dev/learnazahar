// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <string>
#include "core/woofer/homebrew_id.h"

namespace Woofer {

namespace {

// SMDH: "SMDH", version, reserved, then 16 titles of 0x200 bytes each
// (short title 0x80, long title 0x100, publisher 0x80, all UTF-16).
constexpr std::size_t SMDH_TITLES = 0x8;
constexpr std::size_t SMDH_TITLE_SIZE = 0x200;
constexpr std::size_t SMDH_ENGLISH = 1;

u64 Fnv1a(std::span<const u8> data, u64 hash = 0xCBF29CE484222325ULL) {
    for (const u8 b : data) {
        hash = (hash ^ b) * 0x100000001B3ULL;
    }
    return hash;
}

/// The file name: after the last '/' or '\', and in a content:// URI after the last "%2F".
std::string_view FileName(std::string_view path) {
    const auto slash = path.find_last_of("/\\");
    if (slash != std::string_view::npos) {
        path.remove_prefix(slash + 1);
    }
    for (std::size_t i = path.size(); i >= 3; --i) {
        const std::string_view tail = path.substr(i - 3, 3);
        if (tail[0] == '%' && tail[1] == '2' && (tail[2] == 'F' || tail[2] == 'f')) {
            path.remove_prefix(i);
            break;
        }
    }
    return path;
}

} // namespace

u64 HomebrewTitleId(std::span<const u8> smdh, std::string_view path) {
    u64 hash = 0;
    const std::size_t at = SMDH_TITLES + SMDH_ENGLISH * SMDH_TITLE_SIZE;
    if (smdh.size() >= at + SMDH_TITLE_SIZE && std::equal(smdh.begin(), smdh.begin() + 4, "SMDH")) {
        const auto title = smdh.subspan(at, SMDH_TITLE_SIZE);
        if (std::any_of(title.begin(), title.end(), [](u8 b) { return b != 0; })) {
            hash = Fnv1a(title);
        }
    }
    if (hash == 0) {
        std::string name{FileName(path)};
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        hash = Fnv1a({reinterpret_cast<const u8*>(name.data()), name.size()});
    }
    return HOMEBREW_ID_PREFIX | (hash & 0x0000'FFFF'FFFF'FFFFULL);
}

} // namespace Woofer
