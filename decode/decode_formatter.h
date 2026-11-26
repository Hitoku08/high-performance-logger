// 文件作用：DecodeFormatter，按 pattern 把一条 EffectiveMsg 还原成一行文本。
//
// 占位符：
//   %l  级别字母 T D I W E F（与 DefaultFormatter 一致）
//   %D  本地日期时间 2024-01-01 12:00:00
//   %S  时间戳的毫秒部分 000~999，常写成 %D:%S 或 %D.%S
//   %M  完整的 Unix 时间戳（毫秒）
//   %p  进程 ID          %t  线程 ID
//   %F  文件名           %f  函数名          %#  行号
//   %v  日志内容
//   %%  一个 % 字符
// 其他字符原样输出；不认识的占位符（如 %x）也原样输出。
//
// 用法：
//     DecodeFormatter formatter("[%l][%D:%S][%p:%t][%F:%f:%#]%v");
//     std::string line;
//     formatter.Format(msg, &line);   // [I][2024-01-01 12:00:00:123][1234:5678][file.cc:Func:42]hello
#pragma once

#include <string>
#include <utility>

#include "effective_msg.pb.h"

namespace logger {
namespace decode {

class DecodeFormatter {
public:
    static constexpr const char* kDefaultPattern = "[%l][%D:%S][%p:%t][%F:%f:%#]%v";

    explicit DecodeFormatter(std::string pattern = kDefaultPattern) : pattern_(std::move(pattern)) {}

    void SetPattern(std::string pattern) { pattern_ = std::move(pattern); }
    const std::string& GetPattern() const { return pattern_; }

    // 把格式化结果追加到 dest 末尾（不加换行）
    void Format(const EffectiveMsg& msg, std::string* dest) const;

private:
    std::string pattern_;
};

}  // namespace decode
}  // namespace logger
