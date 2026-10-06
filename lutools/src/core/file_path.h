#pragma once
#include <filesystem>
#include <libraw/libraw.h>
#include <string>

namespace sony2fuji {
// Public paths use UTF-8 on every platform, including the Windows C ABI.
inline int openRawFile(LibRaw& raw, const std::string& path) {
#ifdef _WIN32
    return raw.open_file(std::filesystem::u8path(path).c_str());
#else
    return raw.open_file(path.c_str());
#endif
}
}
