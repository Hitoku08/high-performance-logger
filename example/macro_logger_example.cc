#include <memory>
#include <string>

#include "logger/logger.h"
#include "logger/sinks/console_sink.h"

int main() {
    // 1. 创建输出目的地和handle，并设置为全局 handle
    auto sink = std::make_shared<logger::ConsoleSink>();
    auto handle = std::make_shared<logger::VariadicLogHandle>(sink);

    EXT_LOGGER_INIT(handle);

    EXT_LOG_TRACE("this trace is filted");
    EXT_LOG_DEBUG("this debug is filted");
    EXT_LOG_INFO("hello {}", "logger");
    EXT_LOG_WARN("disk usage {}%", 85);
    EXT_LOG_ERROR("open {} failed, code={}", std::string("config.json"), -1);
    EXT_LOG_CRITICAL("pi is about {:2f}", 3.14159);

    handle->SetLevel(logger::LogLevel::kDebug);
    EXT_LOG_DEBUG("new bug is visible");

    LOG_LOGGER_INFO(handle, "logged through an explicit handle");

    sink->Flush();

    return 0;
}