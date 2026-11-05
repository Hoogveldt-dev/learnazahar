// Copyright 2026 Hoogveldt-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// The Woofer bridge: messages from Woofer games to the frontend.
// The protocol is in docs/woofer-bridge/PROTOCOL.md. The core knows nothing
// about any particular game; it only recognises "WOOF<version> <json>" lines
// written with svcOutputDebugString and answers the handshake through
// svcGetSystemInfo.

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include "common/common_types.h"

namespace Woofer {

/// Highest protocol version this emulator understands.
constexpr s64 PROTOCOL_VERSION = 1;

/// svcGetSystemInfo type for the handshake: 'WOOF'.
constexpr u32 SYSTEM_INFO_TYPE = 0x574F4F46;

/// svcGetSystemInfo params for SYSTEM_INFO_TYPE.
enum class InfoParam : s32 {
    Version = 0,   ///< highest protocol version (PROTOCOL_VERSION)
    HostFlags = 1, ///< HostFlag bits
};

/// Bits returned for InfoParam::HostFlags.
enum HostFlag : u32 {
    HOST_PANEL_VISIBLE = 1u << 0,
};

/// Largest message accepted (the game keeps its messages under 8 KiB).
constexpr std::size_t MAX_MESSAGE = 64 * 1024;

/// Receives the type and the JSON object of every accepted message, on the emulation thread.
using Handler = std::function<void(std::string_view type, std::string_view json)>;

/**
 * Looks at one debug string from the game (svcOutputDebugString).
 * @returns true when it is a bridge message (handled or ignored), false for an
 *          ordinary log line.
 */
bool HandleDebugString(std::string_view text);

/// Answer to svcGetSystemInfo(SYSTEM_INFO_TYPE, param).
s64 SystemInfo(s32 param);

/// Sets the frontend's handler; an empty handler removes it.
void SetHandler(Handler handler);

/// Sets the HostFlag bits the game can read.
void SetHostFlags(u32 flags);
u32 GetHostFlags();

/// The last "hello" and "line" messages (JSON), empty when none yet.
std::string LastHello();
std::string LastLine();

/// Forgets the last messages; called when emulation starts or stops.
void Reset();

/**
 * Called when a savestate is loaded. The game stays the same, so "hello" is
 * kept, but the page in the message box is not known any more: the last line
 * is forgotten and the handler receives {"t":"clear"}.
 */
void OnStateLoaded();

/// The value of the top-level string key "t" of a JSON object, or empty.
std::string_view MessageType(std::string_view json);

} // namespace Woofer
