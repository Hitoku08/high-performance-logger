// 声明 ConsoleSink，把日志格式化后输出到 stdout
// 默认使用default formatter
#pragma once

#include <memory>
#include <mutex>

#include "sink.h"

namespace logger {

class ConsoleSink : public LogSink {
public:
    ConsoleSink();
    ~ConsoleSink() override = default;

    // 持有互斥锁，不能拷贝
    ConsoleSink(const ConsoleSink&) = delete;
    ConsoleSink& operator=(const ConsoleSink&) = delete;

    void Log(const LogMsg& msg) override;

    void SetFormatter(std::unique_ptr<Formatter> formatter) override;

    void Flush() override;

private:
    std::mutex mutex_;
    std::unique_ptr<Formatter> formatter_;
    MemoryBuf buf_;
};

}  // namespace logger