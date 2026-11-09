// 定义 LogSink 接口，sink 代表日志的一个输出目的地
// sink决定“写到哪里”，比如屏幕或者文件
#pragma once

#include <memory>

#include "formatter/formatter.h"
#include "log_common.h"
#include "log_msg.h"

namespace logger {

class LogSink {
public:
    virtual ~LogSink() = default;

    virtual void Log(const LogMsg& msg) = 0;

    // 替换formatter
    virtual void SetFormatter(std::unique_ptr<Formatter> formatter) = 0;

    // 把缓冲重的 数据刷出去
    virtual void Flush() {}
};

}  // namespace logger