#include "default_formatter.h"

#include <ctime>
#include <iterator>

#include "third_party/fmt/include/fmt/format.h"
#include "utils/sys_util.h"

namespace logger {

char LevelToChar(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::kTrace:
            return 'T';
        case LogLevel::kDebug:
            return 'D';
        case LogLevel::kInfo:
            return 'I';
        case LogLevel::kWarn:
            return 'W';
        case LogLevel::kError:
            return 'E';
        case LogLevel::kFatal:
            return 'F';
        default:
            return '?';
    }
}

void DefaultFormatter::Format(const LogMsg& msg, MemoryBuf* dest) {
    // 1. 取当前时间，并转化成本地时间
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    LocalTime(&tm, &now);  // 这是utils中定义的函数，把now的时间转化到tm里面去

    // 2. 把时间格式化成 "2024-01-01 12:00:00"
    char time_buf[32];
    // std::strftime的意思是 string format time
    const size_t time_len = std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm);

    // 3. 用 fmt 一次性拼出整行，直接追加到 dest 末尾。dest是一个string类型指针
    // [{}] [{}] [{}:{}] [{}:{}] {}是模版，[2024-11-03 10:13:20] [I] [file.cc:123] [1234:5678] hello
    fmt::format_to(std::back_inserter(*dest), "[{}] [{}] [{}:{}] [{}:{}] {}", fmt::string_view(time_buf, time_len),
                   LevelToChar(msg.level), msg.location.file_name, msg.location.line, GetProcessId(), GetThreadId(),
                   msg.message);
}

}  // namespace logger
