// 声明 EffectiveFormatter：把一条 LogMsg 序列化成 protobuf 二进制（EffectiveMsg），供 EffectiveSink 压缩、加密后落盘。
// 与 DefaultFormatter 不同，Format 会覆盖 dest 原有内容：一次调用只产出一条消息的完整二进制。
#pragma once

#include "formatter.h"

namespace logger {

class EffectiveFormatter : public Formatter {
public:
    void Format(const LogMsg& msg, MemoryBuf* dest) override;
};

}  // namespace logger