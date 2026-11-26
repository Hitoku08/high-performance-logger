#include "decode_formatter.h"

#include <cstdio>
#include <ctime>
#include <string>

#include "logger/formatter/default_formatter.h"
#include "logger/utils/sys_util.h"

namespace logger {
namespace decode {

namespace {

// 毫秒时间戳 → "2024-01-01 12:00:00"（本地时间）
void AppendDateTime(int64_t timestamp_ms, std::string* dest) {
    const std::time_t seconds = static_cast<std::time_t>(timestamp_ms / 1000);
    std::tm tm{};
    LocalTime(&tm, &seconds);  // 线程安全，参考实现用的 std::localtime 不是
    char buf[32];
    const size_t len = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    dest->append(buf, len);
}

// 毫秒部分，固定 3 位：5 → "005"
void AppendMilliseconds(int64_t timestamp_ms, std::string* dest) {
    char buf[8];
    const int len = std::snprintf(buf, sizeof(buf), "%03d", static_cast<int>(timestamp_ms % 1000));
    dest->append(buf, len);
}

}  // namespace

void DecodeFormatter::Format(const EffectiveMsg& msg, std::string* dest) const {
    // 逐个字符扫描 pattern：普通字符原样追加，遇到 % 就看下一个字符是什么占位符
    for (size_t i = 0; i < pattern_.size(); ++i) {
        const char ch = pattern_[i];
        if (ch != '%' || i + 1 == pattern_.size()) {
            dest->push_back(ch);  // 普通字符，或者末尾单独一个 %
            continue;
        }

        const char flag = pattern_[++i];
        switch (flag) {
            case 'l':
                dest->push_back(LevelToChar(static_cast<LogLevel>(msg.level())));
                break;
            case 'D':
                AppendDateTime(msg.timestamp(), dest);
                break;
            case 'S':
                AppendMilliseconds(msg.timestamp(), dest);
                break;
            case 'M':
                dest->append(std::to_string(msg.timestamp()));
                break;
            case 'p':
                dest->append(std::to_string(msg.pid()));
                break;
            case 't':
                dest->append(std::to_string(msg.tid()));
                break;
            case '#':
                dest->append(std::to_string(msg.line()));
                break;
            case 'F':
                dest->append(msg.file_name());
                break;
            case 'f':
                dest->append(msg.func_name());
                break;
            case 'v':
                dest->append(msg.log_info());
                break;
            case '%':
                dest->push_back('%');
                break;
            default:  // 不认识的占位符原样输出，方便发现 pattern 写错了
                dest->push_back('%');
                dest->push_back(flag);
                break;
        }
    }
}

}  // namespace decode
}  // namespace logger
