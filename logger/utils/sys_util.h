// 文件作用：声明与操作系统相关的小工具函数（页大小、进程 ID、线程 ID、本地时间）。
// 后续模块的使用方：
//   M4 DefaultFormatter / M10 EffectiveFormatter：GetProcessId、GetThreadId、LocalTime
//   M7 mmap 缓存：GetPageSize（映射长度要按页对齐）
#pragma once

#include <cstddef>
#include <ctime>

namespace logger {

size_t GetPageSize();   // 单位通常是字节

size_t GetProcessId();  // 返回当前进程ID

size_t GetThreadId();

// time_t是时间戳，通常用于表示从 Unix 时间纪元（1970 年 1 月 1 日 UTC）开始经过的秒数。输入参数
// tm是时间结构体，包含年月日时分秒，输出参数
void LocalTime(std::tm* tm, const std::time_t* now);

}   // namespace logger