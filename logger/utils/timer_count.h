// RAII 风格的计时器，用来快速测量一段代码的耗时。
//
// 用法：在想计时的作用域开头写一行
//     TIMER_COUNT("load config");
// 离开作用域时，会通过 internal_log.h 的 LOG_INFO 输出：
//     [INFO] [文件:行号] 时间 load config cost 1234 us
//
// 这是库内部排查性能用的工具。ENABLE_LOG 没定义时，LOG_INFO 展开为空，计时器不输出任何东西。
#pragma once
#include <chrono>
#include <string_view>

#include "internal_log.h"

namespace logger {

class TimerCount {
public:
explicit TimerCount(std::string_view info) : info_(info), start_(std::chrono::steady_clock::now()) {}

//禁用拷贝构造，因为两份相同的计时没有意义
TimerCount(const TimerCount&) = delete;
TimerCount& operator=(const TimerCount&) = delete;

~TimerCount() {
    const auto cost = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_);
    LOG_INFO("{} cost {} us", info_, cost.count());
    static_cast<void>(cost);
}

private:
std::string_view info_;
std::chrono::steady_clock::time_point start_;
};
}   // namespace logger

#define LOGGER_TIMER_CONCAT_INNER(a, b) a##b
#define LOGGER_TIMER_CONCAT(a, b) LOGGER_TIMER_CONCAT_INNER(a, b)

// 展开后形如：logger::TimerCount timer_count_42("load config")
#define TIMER_COUNT(info) logger::TimerCount LOGGER_TIMER_CONCAT(timer_count_, __LINE__)(info)