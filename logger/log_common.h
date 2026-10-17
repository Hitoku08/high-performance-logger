#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// 这些宏定义了日志的严重程度
#define LOGGER_LEVEL_TRACE 0
#define LOGGER_LEVEL_DEBUG 1
#define LOGGER_LEVEL_INFO  2
#define LOGGER_LEVEL_WARN  3
#define LOGGER_LEVEL_ERROR 4
#define LOGGER_LEVEL_CRITICAL 5
#define LOGGER_LEVEL_OFF 6

#ifndef LOGGER_ACTIVE_LEVEL
#define LOGGER_ACTIVE_LEVEL LOGGER_LEVEL_TRACE
#endif

namespace logger {

using StringView = std::string_view;
using MemoryBuf = std::string;

// 这里的枚举必须要和上面的宏定义一一对应
enum class LogLevel {
    kTrace = LOGGER_LEVEL_TRACE,
    kDebug = LOGGER_LEVEL_DEBUG,
    kInfo = LOGGER_LEVEL_INFO,
    kWarn = LOGGER_LEVEL_WARN,
    kError = LOGGER_LEVEL_ERROR,
    kFatal = LOGGER_LEVEL_CRITICAL,
    kOff = LOGGER_LEVEL_OFF,
};

struct SourceLocation {
    constexpr SourceLocation() = default;
    // std::string_view只负责记录字符串的长度和地址
    SourceLocation(StringView file_name_in, int32_t line_in, StringView func_name_in) 
        : file_name(file_name_in), line(line_in), func_name(func_name_in) {
        // rfind是字符串从后往前找第一个字符。所以后面也要比较两个斜杠的位置，取后面一个
        const size_t slash = file_name.rfind('/');
        const size_t backslash = file_name.rfind('\\');
        size_t pos = StringView::npos;  // npos指的是无效的位置
        if (slash != StringView::npos) {
            pos = slash;
        }
        if(backslash != StringView::npos && (pos == StringView::npos || backslash > pos)) {
            pos = backslash;
        }
        // 截取斜杠之后的位置作为文件名的起点
        if(pos != StringView::npos) {
            file_name = file_name.substr(pos +1);
        }
    }
    StringView file_name;
    int32_t line{0};
    StringView func_name;
};

}