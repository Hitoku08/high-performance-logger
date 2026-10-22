#pragma once

#include <functional>
#include <utility>

namespace logger{

class ExecuteOnScopeExit {
public:
    ExecuteOnScopeExit() = default;

    // // 这里的移动构造函数没有形参，是因为函数体中不会用到形参。C++函数中，参数名可以省略，但是参数类型不能省略
    // ExecuteOnScopeExit(ExecuteOnScopeExit&&) = default;
    // ExecuteOnScopeExit& operator=(ExecuteOnScopeExit&&) = default;

    // 这里的移动构造需要清空源对象的回调，所以需要手动实现
    ExecuteOnScopeExit(ExecuteOnScopeExit&& other) noexcept : func_(other.func_) {
        other.func_ = nullptr;
    }
    ExecuteOnScopeExit& operator=(ExecuteOnScopeExit&& other) {
        if(this != &other) {
            func_ = other.func_;
            other.func_ = nullptr;
        }
        return *this;
    }

    // 删除拷贝构造与拷贝赋值运算符
    ExecuteOnScopeExit(const ExecuteOnScopeExit&) = delete;
    ExecuteOnScopeExit& operator=(const ExecuteOnScopeExit&) = delete;

    // 完美转发
    template <typename F>
    ExecuteOnScopeExit(F&& func) : func_(std::forward<F>(func)) {}

    // noexcept 表示不抛出异常
    ~ExecuteOnScopeExit() noexcept{
        if(func_){
            func_();
        }
    }
private:
    // void()表示无参数、无返回值
    std::function<void()> func_;
};

} // namespace

#define LOGGER_DEFER_CONCAT_INNER(a, b) a##b
#define LOGGER_DEFER_CONCAT(a, b) LOGGER_DEFER_CONCAT_INNER(a, b)

#define LOG_DEFER logger::ExecuteOnScopeExit LOGGER_DEFER_CONCAT(defer_, __LINE__) = [&]()