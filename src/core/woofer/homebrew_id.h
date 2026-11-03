// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// A title id for a homebrew game (a .3dsx), which has none of its own.
// Azahar keys savestates, cheats and the like on the title id; without one
// every homebrew game shares id 0 and the frontends hide the savestates.

#pragma once

#include <span>
#include <string_view>
#include "common/common_types.h"

namespace Woofer {

/// High bits of every id from HomebrewTitleId; no real title uses them.
constexpr u64 HOMEBREW_ID_PREFIX = 0xFF57'0000'0000'0000; // 0xFF, 'W'

/**
 * A stable id for a homebrew game: from the English titles of its SMDH when it
 * has one (the same after a rebuild and wherever the file is), else from the
 * file name (also inside an Android content:// URI).
 * @param smdh the icon data of the game (AppLoader::ReadIcon), may be empty
 * @param path the path or URI of the game file
 */
u64 HomebrewTitleId(std::span<const u8> smdh, std::string_view path);

} // namespace Woofer
