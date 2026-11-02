// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <mutex>
#include "core/woofer/bridge.h"

namespace Woofer {

namespace {

std::mutex g_mutex;
Handler g_handler;
std::string g_last_hello;
std::string g_last_line;
std::atomic<u32> g_host_flags{0};

constexpr std::string_view CLEAR_MESSAGE = R"({"t":"clear"})";

std::string_view Trim(std::string_view s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == '\0' ||
                          s.back() == ' ')) {
        s.remove_suffix(1);
    }
    return s;
}

/// Skips a JSON string starting at s[i] == '"'; returns the index after it, or npos.
std::size_t SkipString(std::string_view s, std::size_t i) {
    for (++i; i < s.size(); ++i) {
        if (s[i] == '\\') {
            ++i;
        } else if (s[i] == '"') {
            return i + 1;
        }
    }
    return std::string_view::npos;
}

std::size_t SkipSpace(std::string_view s, std::size_t i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
        ++i;
    }
    return i;
}

/// Calls the handler outside the lock, so that it may call back into the bridge.
void Notify(const Handler& handler, std::string_view type, std::string_view json) {
    if (handler) {
        handler(type, json);
    }
}

} // namespace

std::string_view MessageType(std::string_view json) {
    int depth = 0;
    bool expect_key = false;
    for (std::size_t i = 0; i < json.size();) {
        const char c = json[i];
        if (c == '"') {
            const std::size_t end = SkipString(json, i);
            if (end == std::string_view::npos) {
                return {};
            }
            if (depth == 1 && expect_key) {
                const std::string_view key = json.substr(i + 1, end - i - 2);
                std::size_t j = SkipSpace(json, end);
                if (key == "t" && j < json.size() && json[j] == ':') {
                    j = SkipSpace(json, j + 1);
                    if (j < json.size() && json[j] == '"') {
                        const std::size_t vend = SkipString(json, j);
                        if (vend == std::string_view::npos) {
                            return {};
                        }
                        return json.substr(j + 1, vend - j - 2);
                    }
                    return {};
                }
            }
            expect_key = false;
            i = end;
            continue;
        }
        if (c == '{' || c == '[') {
            ++depth;
            expect_key = (c == '{');
        } else if (c == '}' || c == ']') {
            --depth;
        } else if (c == ',') {
            expect_key = (depth == 1);
        }
        ++i;
    }
    return {};
}

bool HandleDebugString(std::string_view text) {
    text = Trim(text);
    if (text.substr(0, 4) != "WOOF") {
        return false;
    }
    std::size_t i = 4;
    s64 version = 0;
    while (i < text.size() && i < 8 && text[i] >= '0' && text[i] <= '9') {
        version = version * 10 + (text[i] - '0');
        ++i;
    }
    if (i == 4 || i >= text.size() || text[i] != ' ') {
        return false; // "WOOF" at the start of an ordinary log line
    }
    if (version != PROTOCOL_VERSION) {
        return true; // a bridge message we do not understand
    }
    const std::string_view json = text.substr(i + 1);
    if (json.size() > MAX_MESSAGE || json.size() < 2 || json.front() != '{' ||
        json.back() != '}') {
        return true;
    }
    const std::string_view type = MessageType(json);
    if (type.empty()) {
        return true;
    }

    Handler handler;
    {
        std::lock_guard lock{g_mutex};
        if (type == "hello") {
            g_last_hello = json;
            g_last_line.clear();
        } else if (type == "line") {
            g_last_line = json;
        } else if (type == "clear") {
            g_last_line.clear();
        }
        handler = g_handler;
    }
    Notify(handler, type, json);
    return true;
}

s64 SystemInfo(s32 param) {
    switch (static_cast<InfoParam>(param)) {
    case InfoParam::Version:
        return PROTOCOL_VERSION;
    case InfoParam::HostFlags:
        return g_host_flags.load();
    default:
        return 0;
    }
}

void SetHandler(Handler handler) {
    std::lock_guard lock{g_mutex};
    g_handler = std::move(handler);
}

void SetHostFlags(u32 flags) {
    g_host_flags.store(flags);
}

u32 GetHostFlags() {
    return g_host_flags.load();
}

std::string LastHello() {
    std::lock_guard lock{g_mutex};
    return g_last_hello;
}

std::string LastLine() {
    std::lock_guard lock{g_mutex};
    return g_last_line;
}

void Reset() {
    std::lock_guard lock{g_mutex};
    g_last_hello.clear();
    g_last_line.clear();
}

void OnStateLoaded() {
    Handler handler;
    {
        std::lock_guard lock{g_mutex};
        if (g_last_hello.empty()) {
            return; // not a Woofer game
        }
        g_last_line.clear();
        handler = g_handler;
    }
    Notify(handler, "clear", CLEAR_MESSAGE);
}

} // namespace Woofer
