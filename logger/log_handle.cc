#include "log_handle.h"

#include <utility>

#include "sinks/sink.h"

namespace logger {

LogHandle::LogHandle(LogSinkPtrList sinks) {
    sinks_.reserve(sinks.size());
    for (const LogSinkPtr& sink : sinks) {
        AddSink_(sink);
    }
}

LogHandle::LogHandle(LogSinkPtr sink) {
    AddSink_(std::move(sink));
}

void LogHandle::SetLevel(LogLevel level) {
    level_.store(level, std::memory_order_relaxed);
}

LogLevel LogHandle::GetLevel() const {
    return level_.load(std::memory_order_relaxed);
}

void LogHandle::Log(LogLevel level, SourceLocation loc, StringView message) {
    if (!ShouldLog_(level)) {
        return;
    }
    Log_(LogMsg(loc, level, message));
}

void LogHandle::Log_(const LogMsg& log_msg) {
    for (const LogSinkPtr& sink : sinks_) {
        sink->Log(log_msg);
    }
}

void LogHandle::AddSink_(LogSinkPtr sink) {
    if (sink) {
        sinks_.push_back(std::move(sink));
    }
}

}  // namespace logger