#include "log_factory.h"

#include <utility>

#include "log_variadic_handle.h"

namespace logger {

LogFactory& LogFactory::Instance() {
    // 静态局部变量只初始化一次，而且初始化是线程安全的
    static LogFactory instance;
    return instance;
}

VariadicLogHandle* LogFactory::GetLogHandle() const noexcept {
    return current_.load(std::memory_order_acquire);
}

void LogFactory::SetLogHandle(std::shared_ptr<VariadicLogHandle> log_handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    VariadicLogHandle* raw = log_handle.get();  // get方法返回裸指针
    if (log_handle) {
        handles_.push_back(std::move(log_handle));
    }
    current_.store(raw, std::memory_order_release);
}

}  // namespace logger