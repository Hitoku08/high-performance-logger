// 定义formatter接口，负责把LogMsg转换成最终要输出的字节。
// formatter决定“长什么样”，比如转化成文本或者protobuf二进制
#pragma once

#include "log_common.h"
#include "log_msg.h"

namespace logger {

class Formatter {
public:
    virtual ~Formatter() = default;
    virtual void Format(const LogMsg& msg, MemoryBuf* dest) = 0;
};

}  // namespace logger