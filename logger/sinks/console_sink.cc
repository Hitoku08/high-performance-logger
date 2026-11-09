#include "console_sink.h"

#include <cstdio>
#include <utility>

#include "formatter/default_formatter.h"

namespace logger {

// ConsoleSink 的 formatter 默认是 DefaultFormatter、
ConsoleSink::ConsoleSink() : formatter_(std::make_unique<DefaultFormatter>()) {}

void ConsoleSink::Log(const LogMsg& msg) {
    std::lock_guard<std::mutex> lock(mutex_);  // 互斥访问

    buf_.clear();
    formatter_->Format(msg, &buf_);
    buf_.push_back('\n');

    // fwrite 是C语言中的的文件输出函数，把一段内存中的字节输出到文件中
    // stdout指把内存数据输出到标准输出中，即终端屏幕
    std::fwrite(buf_.data(), 1, buf_.size(), stdout);
}

void ConsoleSink::SetFormatter(std::unique_ptr<Formatter> formatter) {
    // unique_ptr 不能复制，只能移动
    if (!formatter) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    formatter_ = std::move(formatter);
}

// Flush 的作用是把缓冲区的数据直接刷新到屏幕上
void ConsoleSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fflush(stdout);
}

}  // namespace logger