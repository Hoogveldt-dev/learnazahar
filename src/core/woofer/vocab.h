// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// The word list shared by all Woofer games and the emulator:
// sdmc:/woofer/vocab.jsonl (docs/woofer-bridge/PROTOCOL.md, section 4).

#pragma once

#include <string>
#include <string_view>

namespace Woofer {

/// Host path of sdmc:/woofer/vocab.jsonl.
std::string VocabPath();

/// Appends one JSON object as one line. Refuses text with a line break.
bool AppendVocab(std::string_view json_object);

/// The whole file, empty when there is none.
std::string ReadVocab();

} // namespace Woofer
