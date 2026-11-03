#pragma once

#include <cstddef>
#include <filesystem>

namespace logger {

namespace fs{

// 返回文件大小
size_t GetFileSize(const std::filesystem::path& file_path);

}   // namespace fs
}   // namespace logger