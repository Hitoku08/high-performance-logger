// 声明DefaultFormatter，把一条LogMsg格式化成可以直接阅读的一行字
// 输出格式：[2024-01-01 12:00:00] [I] [file.cc:123] [pid:tid] message
#pragma once

#include "formatter.h"

namespace logger {

class DefaultFormatter : public Formatter {
public:
    void Format(const LogMsg& msg, MemoryBuf* dest) override;
};

char LevelToChar(LogLevel level) noexcept;

}  // namespace logger