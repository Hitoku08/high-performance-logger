// 文件作用：对外的日志宏接口，使用者通常只需要包含这一个头文件。
//
// 用法：
//     auto handle = std::make_shared<logger::VariadicLogHandle>(std::make_shared<logger::ConsoleSink>());
//     EXT_LOGGER_INIT(handle);                 // 设置全局 handle
//     EXT_LOG_INFO("x={} y={}", x, y);         // 用全局 handle 写日志
//     LOG_LOGGER_INFO(other_handle, "hi");     // 用指定的 handle 写日志
//
// 编译期裁剪：定义 LOGGER_ACTIVE_LEVEL（见 log_common.h）后，低于它的宏会展开成空语句，
// 调用处不会生成任何代码，参数也不会被求值。
#pragma once

#include "log_common.h"
#include "log_factory.h"
#include "log_variadic_handle.h"

// 设置全局 handle。
#define EXT_LOGGER_INIT(handle) logger::LogFactory::Instance().SetLogHandle(handle)

// 所有日志宏共用的实现。
// 1. do { ... } while (0)：让宏在任何地方都像一条普通语句。
//    如果只写 if (...) { ... }，在 if (a) EXT_LOG_INFO("x"); else ... 里，else 会错误地和宏里的 if 配对。
// 2. 先把 handle 存进局部变量：handle 表达式只求值一次（例如 GetLogHandle() 只调用一次）。
//    auto&& 可以同时接收裸指针和 shared_ptr。
// 3. __FILE__、__LINE__、__FUNCTION__ 在调用处展开，记录的是写日志的位置，而不是这个头文件。
#define LOGGER_CALL(handle, level, ...)                                                                               \
    do {                                                                                                              \
        auto&& logger_call_handle_ = (handle);                                                                        \
        if (logger_call_handle_) {                                                                                    \
            logger_call_handle_->Log(logger::SourceLocation{__FILE__, __LINE__, __FUNCTION__}, (level), __VA_ARGS__); \
        }                                                                                                             \
    } while (0)

// 被裁剪掉的宏展开成这个空语句。同样用 do-while，保证用法和未裁剪时一致。
#define LOGGER_NOOP() \
    do {              \
    } while (0)

// 全局 handle 的简写。
#define LOGGER_GLOBAL_HANDLE() logger::LogFactory::Instance().GetLogHandle()

#if LOGGER_ACTIVE_LEVEL <= LOGGER_LEVEL_TRACE
#define LOG_LOGGER_TRACE(handle, ...) LOGGER_CALL(handle, logger::LogLevel::kTrace, __VA_ARGS__)
#define EXT_LOG_TRACE(...) LOG_LOGGER_TRACE(LOGGER_GLOBAL_HANDLE(), __VA_ARGS__)
#else
#define LOG_LOGGER_TRACE(handle, ...) LOGGER_NOOP()
#define EXT_LOG_TRACE(...) LOGGER_NOOP()
#endif

#if LOGGER_ACTIVE_LEVEL <= LOGGER_LEVEL_DEBUG
#define LOG_LOGGER_DEBUG(handle, ...) LOGGER_CALL(handle, logger::LogLevel::kDebug, __VA_ARGS__)
#define EXT_LOG_DEBUG(...) LOG_LOGGER_DEBUG(LOGGER_GLOBAL_HANDLE(), __VA_ARGS__)
#else
#define LOG_LOGGER_DEBUG(handle, ...) LOGGER_NOOP()
#define EXT_LOG_DEBUG(...) LOGGER_NOOP()
#endif

#if LOGGER_ACTIVE_LEVEL <= LOGGER_LEVEL_INFO
#define LOG_LOGGER_INFO(handle, ...) LOGGER_CALL(handle, logger::LogLevel::kInfo, __VA_ARGS__)
#define EXT_LOG_INFO(...) LOG_LOGGER_INFO(LOGGER_GLOBAL_HANDLE(), __VA_ARGS__)
#else
#define LOG_LOGGER_INFO(handle, ...) LOGGER_NOOP()
#define EXT_LOG_INFO(...) LOGGER_NOOP()
#endif

#if LOGGER_ACTIVE_LEVEL <= LOGGER_LEVEL_WARN
#define LOG_LOGGER_WARN(handle, ...) LOGGER_CALL(handle, logger::LogLevel::kWarn, __VA_ARGS__)
#define EXT_LOG_WARN(...) LOG_LOGGER_WARN(LOGGER_GLOBAL_HANDLE(), __VA_ARGS__)
#else
#define LOG_LOGGER_WARN(handle, ...) LOGGER_NOOP()
#define EXT_LOG_WARN(...) LOGGER_NOOP()
#endif

#if LOGGER_ACTIVE_LEVEL <= LOGGER_LEVEL_ERROR
#define LOG_LOGGER_ERROR(handle, ...) LOGGER_CALL(handle, logger::LogLevel::kError, __VA_ARGS__)
#define EXT_LOG_ERROR(...) LOG_LOGGER_ERROR(LOGGER_GLOBAL_HANDLE(), __VA_ARGS__)
#else
#define LOG_LOGGER_ERROR(handle, ...) LOGGER_NOOP()
#define EXT_LOG_ERROR(...) LOGGER_NOOP()
#endif

// CRITICAL 对应 LogLevel::kFatal（log_common.h 里 LOGGER_LEVEL_CRITICAL 映射到 kFatal）。
#if LOGGER_ACTIVE_LEVEL <= LOGGER_LEVEL_CRITICAL
#define LOG_LOGGER_CRITICAL(handle, ...) LOGGER_CALL(handle, logger::LogLevel::kFatal, __VA_ARGS__)
#define EXT_LOG_CRITICAL(...) LOG_LOGGER_CRITICAL(LOGGER_GLOBAL_HANDLE(), __VA_ARGS__)
#else
#define LOG_LOGGER_CRITICAL(handle, ...) LOGGER_NOOP()
#define EXT_LOG_CRITICAL(...) LOGGER_NOOP()
#endif
