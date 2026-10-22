#pragma once

// 库内部调试用的日志宏，不是对外的日志接口（对外接口在 M5 的 logger.h 里）。
// 根目录 CMakeLists.txt 里有 add_definitions(-DENABLE_LOG=1)，所以默认是开着的。
#ifdef ENABLE_LOG

#include <chrono>

// format.h 提供 fmt::print / fmt::format；chrono.h 让 fmt 能直接输出 system_clock::time_point。
// fmt 11 里的 core.h 只是兼容层，所以这里直接包含 format.h。
#include "third_party/fmt/include/fmt/chrono.h"
#include "third_party/fmt/include/fmt/format.h"

// 输出格式：[级别] [文件:行号] 当前时间 内容
// __VA_ARGS__ 原样交给 fmt::format，用法和 fmt 相同：LOG_INFO("size={}", n);
// 格式化字符串和参数对不上时，fmt 会在编译期报错。
#define LOG_INFO(...) \
  fmt::print("[INFO] [{}:{}] {} {}\n", __FILE__, __LINE__, std::chrono::system_clock::now(), fmt::format(__VA_ARGS__))

#define LOG_DEBUG(...) \
  fmt::print("[DEBUG] [{}:{}] {} {}\n", __FILE__, __LINE__, std::chrono::system_clock::now(), fmt::format(__VA_ARGS__))

#define LOG_ERROR(...) \
  fmt::print("[ERROR] [{}:{}] {} {}\n", __FILE__, __LINE__, std::chrono::system_clock::now(), fmt::format(__VA_ARGS__))

#else

// 关闭时展开为空：调用点不产生代码，参数也不会被求值。
#define LOG_INFO(...)
#define LOG_DEBUG(...)
#define LOG_ERROR(...)

#endif