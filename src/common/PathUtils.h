#pragma once
// =====================================================================
// Windows 可执行文件路径工具。
// 运行时文件必须相对 exe 所在目录定位，不能依赖调用者的当前工作目录。
// =====================================================================

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace myqq {

inline std::filesystem::path ExecutablePath() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                          static_cast<DWORD>(buffer.size()));
        if (length == 0)
            throw std::runtime_error("GetModuleFileNameW failed");
        if (length < buffer.size() - 1)
            return std::filesystem::path(std::wstring(buffer.data(), length));
        buffer.resize(buffer.size() * 2);
    }
}

inline std::filesystem::path ExecutableDirectory() {
    return ExecutablePath().parent_path();
}

inline std::string PathToUtf8(const std::filesystem::path& path) {
    const std::wstring wide = path.wstring();
    if (wide.empty()) return std::string();
    int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                     static_cast<int>(wide.size()), nullptr, 0,
                                     nullptr, nullptr);
    if (length <= 0) throw std::runtime_error("WideCharToMultiByte failed");
    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                        &utf8[0], length, nullptr, nullptr);
    return utf8;
}

} // namespace myqq
