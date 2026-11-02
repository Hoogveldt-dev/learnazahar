// Copyright 2026 Woofer-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// End-to-end test of the Woofer bridge: the game side (port/woofer_bridge.c of
// PokemonWoofer, copied unchanged into game/) talks to the emulator side
// (src/core/woofer) the way it does in the emulator. The two system calls are
// replaced by what SVC::GetSystemInfo and SVC::OutputDebugString in
// src/core/hle/kernel/svc.cpp do with them.

#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <json.hpp>
#include "common/file_util.h"
#include "core/woofer/bridge.h"
#include "core/woofer/vocab.h"

extern "C" {
#include "woofer_bridge.h"
}

using nlohmann::json;

namespace {

/// What the kernel answers: a real 3DS, an emulator without the bridge, or WooferAzahar.
enum class Host { Real3DS, OtherEmulator, Woofer };
Host g_host = Host::Woofer;

std::vector<std::string> g_types;
std::vector<json> g_messages;

/// SVC::GetSystemInfo: Azahar answers unknown types with 0 and success; a real 3DS fails.
int KernelGetSystemInfo(int64_t* out, uint32_t type, int32_t param) {
    switch (g_host) {
    case Host::Real3DS:
        return -1;
    case Host::OtherEmulator:
        *out = 0;
        return 0;
    case Host::Woofer:
        *out = type == Woofer::SYSTEM_INFO_TYPE ? Woofer::SystemInfo(param) : 0;
        return 0;
    }
    return -1;
}

/// SVC::OutputDebugString: the kernel copies exactly `length` bytes out of guest memory.
void KernelOutputDebugString(const char* text, size_t length) {
    const std::string string(text, length);
    Woofer::HandleDebugString(string);
}

void Boot(Host host) {
    g_host = host;
    g_types.clear();
    g_messages.clear();
    Woofer::Reset();
    Woofer::SetHostFlags(0);
    gWooferSystemInfo = KernelGetSystemInfo;
    gWooferOutput = KernelOutputDebugString;
    Woofer::SetHandler([](std::string_view type, std::string_view body) {
        g_types.emplace_back(type);
        g_messages.push_back(json::parse(body)); // every message must be valid JSON
    });
}

void Shutdown() {
    Woofer::SetHandler({});
    Woofer::Reset();
    Woofer::SetHostFlags(0);
}

/// A page as PokemonWoofer's port/woofer_dict.c sends it.
int SendLine(WooferMsg* m) {
    Woofer_Begin(m, "line");
    Woofer_Hex64(m, "h", 0x6b8a1f0c9e2d4a17ULL);
    Woofer_Str(m, "zh", "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8C\xE5\xB0\x8F\xE6\x99\xB4\xEF\xBC\x81");
    Woofer_Str(m, "en", "Hello, \"May\"!\nWelcome.");
    Woofer_Str(m, "loc", "LITTLEROOT_TOWN");
    Woofer_ArrayBegin(m, "w");
    Woofer_ObjectBegin(m, nullptr);
    Woofer_Str(m, "s", "\xE4\xBD\xA0\xE5\xA5\xBD"); // 你好
    Woofer_Str(m, "p", "n\xC7\x90 h\xC7\x8Eo");     // nǐ hǎo
    Woofer_Str(m, "g", "hello");
    Woofer_Int(m, "hsk", 1);
    Woofer_ObjectEnd(m);
    Woofer_ObjectBegin(m, nullptr);
    Woofer_Str(m, "s", "\xEF\xBC\x8C"); // ，
    Woofer_ObjectEnd(m);
    Woofer_ObjectBegin(m, nullptr);
    Woofer_Str(m, "s", "\xE5\xB0\x8F\xE6\x99\xB4"); // 小晴 (a filled-in name)
    Woofer_ObjectEnd(m);
    Woofer_ObjectBegin(m, nullptr);
    Woofer_Str(m, "s", "\xEF\xBC\x81"); // ！
    Woofer_ObjectEnd(m);
    Woofer_ArrayEnd(m);
    return Woofer_Send(m);
}

} // namespace

TEST_CASE("Woofer game and emulator: handshake and hello", "[core][woofer][e2e]") {
    Boot(Host::Woofer);
    REQUIRE(Woofer_Init("pokeemerald-zh", "PokemonWoofer", "zh-Hans", "en", "0.1.0") == 1);
    REQUIRE(Woofer_Active());

    REQUIRE(g_types == std::vector<std::string>{"hello"});
    const json& hello = g_messages[0];
    CHECK(hello["game"] == "pokeemerald-zh");
    CHECK(hello["title"] == "PokemonWoofer");
    CHECK(hello["lang"] == "zh-Hans");
    CHECK(hello["src"] == "en");
    CHECK(hello["build"] == "0.1.0");
    CHECK(json::parse(Woofer::LastHello()) == hello);

    // the game reads the host flags anew on every call
    CHECK(Woofer_HostFlags() == 0);
    Woofer::SetHostFlags(Woofer::HOST_PANEL_VISIBLE);
    CHECK(Woofer_HostFlags() == WOOFER_HOST_PANEL_VISIBLE);
    Shutdown();
}

TEST_CASE("Woofer game and emulator: line, clear and saved", "[core][woofer][e2e]") {
    static WooferMsg m;
    Boot(Host::Woofer);
    REQUIRE(Woofer_Init("pokeemerald-zh", "PokemonWoofer", "zh-Hans", "en", nullptr) == 1);

    REQUIRE(SendLine(&m) == 1);
    REQUIRE(g_types.size() == 2);
    REQUIRE(g_types[1] == "line");
    const json& line = g_messages[1];
    CHECK(line["h"] == "6b8a1f0c9e2d4a17");
    CHECK(line["en"] == "Hello, \"May\"!\nWelcome.");
    CHECK(line["loc"] == "LITTLEROOT_TOWN");
    REQUIRE(line["w"].size() == 4);
    CHECK(line["w"][0]["p"] == "n\xC7\x90 h\xC7\x8Eo");
    CHECK(line["w"][0]["hsk"] == 1);
    CHECK_FALSE(line["w"][1].contains("p")); // punctuation: not a word

    // the pieces of "w" together are the page
    std::string joined;
    for (const auto& piece : line["w"]) {
        joined += piece["s"].get<std::string>();
    }
    CHECK(joined == line["zh"].get<std::string>());
    CHECK(json::parse(Woofer::LastLine()) == line);

    REQUIRE(Woofer_SendClear() == 1);
    CHECK(g_types.back() == "clear");
    CHECK(Woofer::LastLine().empty());

    REQUIRE(Woofer_SendSaved("\xE4\xBD\xA0\xE5\xA5\xBD", "n\xC7\x90 h\xC7\x8Eo") == 1);
    CHECK(g_types.back() == "saved");
    CHECK(g_messages.back()["s"] == "\xE4\xBD\xA0\xE5\xA5\xBD");

    // a message the game cannot finish is not sent at all
    const std::size_t before = g_types.size();
    Woofer_Begin(&m, "line");
    Woofer_ArrayBegin(&m, "w");
    CHECK(Woofer_Send(&m) == 0);
    CHECK(g_types.size() == before);

    // a savestate load keeps the game but forgets the page
    REQUIRE(SendLine(&m) == 1);
    Woofer::OnStateLoaded();
    CHECK(g_types.back() == "clear");
    CHECK(Woofer::LastLine().empty());
    CHECK_FALSE(Woofer::LastHello().empty());
    Shutdown();
}

TEST_CASE("Woofer game without the bridge sends nothing", "[core][woofer][e2e]") {
    static WooferMsg m;
    for (const Host host : {Host::Real3DS, Host::OtherEmulator}) {
        Boot(host);
        Woofer::SetHostFlags(Woofer::HOST_PANEL_VISIBLE);
        CHECK(Woofer_Init("pokeemerald-zh", "PokemonWoofer", "zh-Hans", "en", "0.1.0") == 0);
        CHECK_FALSE(Woofer_Active());
        CHECK(SendLine(&m) == 0);
        CHECK(Woofer_SendClear() == 0);
        CHECK(Woofer_HostFlags() == 0);
        CHECK(g_types.empty());
        CHECK(Woofer::LastHello().empty());
        Shutdown();
    }
}

TEST_CASE("Woofer game and emulator share vocab.jsonl", "[core][woofer][e2e]") {
    static WooferMsg m;
    namespace fs = std::filesystem;
    const fs::path sdmc = fs::temp_directory_path() / "woofer-azahar-test-sdmc";
    fs::remove_all(sdmc);
    fs::create_directories(sdmc);
    const std::string old_sdmc = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
    FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, sdmc.string());

    Boot(Host::Woofer);
    REQUIRE(Woofer_Init("pokeemerald-zh", "PokemonWoofer", "zh-Hans", "en", "0.1.0") == 1);
    CHECK(Woofer::ReadVocab().empty());

    // the game writes sdmc:/woofer/vocab.jsonl, which is SDMCDir/woofer/vocab.jsonl here
    const std::string game_dir = (sdmc / "woofer").string();
    gWooferVocabDir = game_dir.c_str();
    Woofer_BeginVocab(&m);
    Woofer_Str(&m, "s", "\xE9\x93\xB6\xE8\xA1\x8C"); // 银行
    Woofer_Str(&m, "p", "y\xC3\xADnh\xC3\xA1ng");
    Woofer_Str(&m, "g", "bank");
    Woofer_Str(&m, "status", "new");
    REQUIRE(Woofer_SaveVocab(&m) == 1);
    gWooferVocabDir = "sdmc:/woofer";

    // the emulator reads it and adds its own line
    REQUIRE(fs::path(Woofer::VocabPath()) == sdmc / "woofer" / "vocab.jsonl");
    const json from_game = json::parse(Woofer::ReadVocab());
    CHECK(from_game["v"] == 1);
    CHECK(from_game["game"] == "pokeemerald-zh");
    CHECK(from_game["g"] == "bank");

    json update = from_game;
    update["status"] = "learning";
    REQUIRE(Woofer::AppendVocab(update.dump()));
    CHECK_FALSE(Woofer::AppendVocab("{\"s\":\"a\nb\"}")); // one object per line
    CHECK_FALSE(Woofer::AppendVocab("not json"));

    std::istringstream lines(Woofer::ReadVocab());
    std::vector<json> entries;
    for (std::string raw; std::getline(lines, raw);) {
        entries.push_back(json::parse(raw));
    }
    REQUIRE(entries.size() == 2);
    CHECK(entries[0]["status"] == "new");
    CHECK(entries[1]["status"] == "learning"); // the last line of a word counts
    CHECK(entries[1]["s"] == entries[0]["s"]);

    Shutdown();
    FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, old_sdmc);
    fs::remove_all(sdmc);
}
