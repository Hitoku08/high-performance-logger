// 定义 LogHandle，它是日志的分发中心
// LogHandle 负责：1. 按级别过滤日志。2. 把通过过滤的日志分发给所有 sink
#pragma once

#include <atomic>
#include <initializer_list>
#include <memory>
#include <vector>

#include "log_common.h"
#include "log_msg.h"

namespace logger {

class LogSink;

// 共享指针：一个 sink 可能被多个 LogHandle 共用
using LogSinkPtr = std::shared_ptr<LogSink>;
using LogSinkPtrList = std::initializer_list<LogSinkPtr>;

class LogHandle {
public:
    // 用法：LogHandle handle {sink_a, sink_b}
    explicit LogHandle(LogSinkPtrList sinks);
    // 用法：LogHandle handle(sink)
    explicit LogHandle(LogSinkPtr sink);

    virtual ~LogHandle() = default;

    // 删除拷贝构造，因为原子成员本身不可拷贝
    LogHandle(const LogHandle&) = delete;
    LogHandle& operator=(const LogHandle&) = delete;

    void SetLevel(LogLevel level);
    LogLevel GetLevel() const;

    void Log(LogLevel level, SourceLocation loc, StringView message);

protected:
    // 保护的成员变量，派生类成员函数可以访问。但是私有成员变量，派生类就不可访问
    // 保护和私有成员变量在类外都不可被访问
    bool ShouldLog_(LogLevel level) const noexcept {
        const LogLevel current = level_.load(std::memory_order_relaxed);
        return level != LogLevel::kOff && level >= current && !sinks_.empty();
    }

    void Log_(const LogMsg& log_msg);

private:
    void AddSink_(LogSinkPtr sink);
    std::atomic<LogLevel> level_{LogLevel::kInfo};  // 默认只输出kInfo及以上
    std::vector<LogSinkPtr> sinks_;
};
}  // namespace logger