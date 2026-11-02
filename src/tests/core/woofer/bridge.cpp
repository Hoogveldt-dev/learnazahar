// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Unit tests of the emulator side of the Woofer bridge (docs/woofer-bridge/PROTOCOL.md).

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "core/woofer/bridge.h"

namespace {
struct Received {
    std::vector<std::string> types;
    std::vector<std::string> bodies;
};

Woofer::Handler Recorder(Received& got) {
    return [&got](std::string_view type, std::string_view json) {
        got.types.emplace_back(type);
        got.bodies.emplace_back(json);
    };
}
} // namespace

TEST_CASE("Woofer::MessageType", "[core][woofer]") {
    REQUIRE(Woofer::MessageType(R"({"t":"line","h":"00ff"})") == "line");
    REQUIRE(Woofer::MessageType(R"({ "h" : "x", "t" : "hello" })") == "hello");
    // a nested "t" is not the message type
    REQUIRE(Woofer::MessageType(R"({"w":[{"t":"no"}],"t":"line"})") == "line");
    REQUIRE(Woofer::MessageType(R"({"o":{"t":"no"},"t":"clear"})") == "clear");
    REQUIRE(Woofer::MessageType(R"({"s":"\"t\":\"no\"","t":"saved"})") == "saved");
    REQUIRE(Woofer::MessageType(R"({"x":"t"})").empty());
    REQUIRE(Woofer::MessageType(R"({"t":1})").empty());
    REQUIRE(Woofer::MessageType(R"({"t":"unterminated)").empty());
    REQUIRE(Woofer::MessageType("").empty());
}

TEST_CASE("Woofer::HandleDebugString", "[core][woofer]") {
    Received got;
    Woofer::Reset();
    Woofer::SetHandler(Recorder(got));

    // ordinary log lines are not ours
    REQUIRE_FALSE(Woofer::HandleDebugString("cjk: 7000 dialogue texts"));
    REQUIRE_FALSE(Woofer::HandleDebugString("WOOFER says hi"));
    REQUIRE_FALSE(Woofer::HandleDebugString("WOOF"));
    REQUIRE_FALSE(Woofer::HandleDebugString(""));

    // ours, but not understood: consumed without a callback
    REQUIRE(Woofer::HandleDebugString(R"(WOOF2 {"t":"line"})"));
    REQUIRE(Woofer::HandleDebugString("WOOF1 not json"));
    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"no":"type"})"));
    REQUIRE(Woofer::HandleDebugString("WOOF1 {" + std::string(Woofer::MAX_MESSAGE, ' ') + "}"));
    REQUIRE(got.types.empty());

    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"t":"hello","game":"test-zh"})"));
    // a trailing line break or NUL (C string written with its terminator) is fine
    REQUIRE(
        Woofer::HandleDebugString(std::string("WOOF1 {\"t\":\"line\",\"zh\":\"\xE4\xBD\xA0\"}\n")));
    REQUIRE(Woofer::HandleDebugString(std::string("WOOF1 {\"t\":\"future-kind\"}\0", 26)));
    REQUIRE(got.types == std::vector<std::string>{"hello", "line", "future-kind"});
    REQUIRE(got.bodies[1] == "{\"t\":\"line\",\"zh\":\"\xE4\xBD\xA0\"}");
    REQUIRE(Woofer::LastHello() == R"({"t":"hello","game":"test-zh"})");
    REQUIRE(Woofer::LastLine() == got.bodies[1]);

    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"t":"clear"})"));
    REQUIRE(Woofer::LastLine().empty());

    // a new hello forgets the page of the game before
    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"t":"line","zh":"x"})"));
    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"t":"hello","game":"other-zh"})"));
    REQUIRE(Woofer::LastLine().empty());

    Woofer::SetHandler({});
    Woofer::Reset();
    REQUIRE(Woofer::LastHello().empty());
}

TEST_CASE("Woofer handler may call back into the bridge", "[core][woofer]") {
    std::string seen;
    Woofer::Reset();
    Woofer::SetHandler([&seen](std::string_view, std::string_view) {
        seen = Woofer::LastLine(); // must not dead-lock
        Woofer::SetHostFlags(Woofer::HOST_PANEL_VISIBLE);
    });
    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"t":"line","zh":"y"})"));
    REQUIRE(seen == R"({"t":"line","zh":"y"})");
    REQUIRE(Woofer::GetHostFlags() == Woofer::HOST_PANEL_VISIBLE);
    Woofer::SetHandler({});
    Woofer::SetHostFlags(0);
    Woofer::Reset();
}

TEST_CASE("Woofer::OnStateLoaded", "[core][woofer]") {
    Received got;
    Woofer::Reset();
    Woofer::SetHandler(Recorder(got));

    // not a Woofer game: nothing happens
    Woofer::OnStateLoaded();
    REQUIRE(got.types.empty());

    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"t":"hello","game":"test-zh"})"));
    REQUIRE(Woofer::HandleDebugString(R"(WOOF1 {"t":"line","zh":"z"})"));
    Woofer::OnStateLoaded();
    REQUIRE(got.types == std::vector<std::string>{"hello", "line", "clear"});
    REQUIRE(got.bodies[2] == R"({"t":"clear"})");
    REQUIRE(Woofer::LastLine().empty());
    REQUIRE_FALSE(Woofer::LastHello().empty()); // still the same game

    Woofer::SetHandler({});
    Woofer::Reset();
}

TEST_CASE("Woofer::SystemInfo", "[core][woofer]") {
    using Woofer::InfoParam;
    REQUIRE(Woofer::SYSTEM_INFO_TYPE == 0x574F4F46); // 'WOOF'
    REQUIRE(Woofer::SystemInfo(static_cast<s32>(InfoParam::Version)) == Woofer::PROTOCOL_VERSION);
    Woofer::SetHostFlags(Woofer::HOST_PANEL_VISIBLE);
    REQUIRE(Woofer::SystemInfo(static_cast<s32>(InfoParam::HostFlags)) == 1);
    Woofer::SetHostFlags(0);
    REQUIRE(Woofer::SystemInfo(static_cast<s32>(InfoParam::HostFlags)) == 0);
    REQUIRE(Woofer::SystemInfo(99) == 0);
    REQUIRE(Woofer::SystemInfo(-1) == 0);
}
