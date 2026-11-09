// 定义 VariadicLogHandle，在 LogHandle 基础上支持 fmt 风格的可变参数日志
// 用法：handle->Log(loc, LogLevel::kInfo, "x={} y={}", x,y)
#pragma once

#include <utility>

#include "log_handle.h"

#include "third_party/fmt/include/fmt/format.h"

namespace logger {

class VariadicLogHandle : public LogHandle {
public:
    using LogHandle::LogHandle;  // 这个作用是继承父类的构造函数

    template <typename... Args>
    void Log(SourceLocation loc, LogLevel level, fmt::format_string<Args...> format, Args&&... args) {
        if (!ShouldLog_(level)) {
            return;
        }

        fmt::basic_memory_buffer<char, 256> buf;
        fmt::format_to(fmt::appender(buf), format, std::forward<Args>(args)...);

        LogHandle::Log_(LogMsg(loc, level, StringView(buf.data(), buf.size())));
    }

    template <typename... Args>
    void Log(LogLevel level, fmt::format_string<Args...> format, Args&&... args) {
        Log(SourceLocation{}, level, format, std::forward<Args>(args)...);
    }
};

}  // namespace logger