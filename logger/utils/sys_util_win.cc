// sys_util.h 在 Windows 下的实现。只在 Windows 构建时编译
#include "utils/sys_util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace logger {
    size_t GetPageSize() {
        SYSTEM_INFO info;   // SYSTEM_INFO是windows API提供的结构体
        ::GetSystemInfo(&info); // GetSystemInfo是windows API提供的函数
        return static_cast<size_t>(info.dwPageSize);
    }

    size_t GetProcessId(){
        return static_cast<size_t>(::GetCurrentProcessId());
    }

    size_t GetThreadId(){
        return static_cast<size_t>(::GetCurrentThreadId());
    }

    void LocalTime(std::tm* tm, const std::time_t* now) {
        // localtime_s是C语言标准库的函数，返回 0 表示成功
        if(::localtime_s(tm, now) != 0) {
            *tm = std::tm{};
        }
    }
}