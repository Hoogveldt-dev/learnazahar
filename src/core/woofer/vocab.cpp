// Copyright 2026 Hoogveldt-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include "common/file_util.h"
#include "core/woofer/vocab.h"

namespace Woofer {

namespace {
std::mutex g_vocab_mutex;
}

std::string VocabPath() {
    return FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) + "woofer/vocab.jsonl";
}

bool AppendVocab(std::string_view json_object) {
    if (json_object.size() < 2 || json_object.front() != '{' || json_object.back() != '}' ||
        json_object.find('\n') != std::string_view::npos ||
        json_object.find('\r') != std::string_view::npos) {
        return false;
    }
    std::lock_guard lock{g_vocab_mutex};
    const std::string path = VocabPath();
    if (!FileUtil::CreateFullPath(path)) {
        return false;
    }
    // One write per line: a power cut can only cost the last line.
    std::string line{json_object};
    line += '\n';
    FileUtil::IOFile file(path, "ab");
    return file.IsOpen() && file.WriteBytes(line.data(), line.size()) == line.size();
}

std::string ReadVocab() {
    std::lock_guard lock{g_vocab_mutex};
    std::string text;
    if (FileUtil::Exists(VocabPath())) {
        FileUtil::ReadFileToString(true, VocabPath(), text);
    }
    return text;
}

} // namespace Woofer
