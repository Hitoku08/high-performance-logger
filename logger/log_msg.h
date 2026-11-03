#pragma once

#include "log_common.h"

namespace logger {

struct LogMsg {
  LogMsg(SourceLocation loc, LogLevel lvl, StringView msg);
  LogMsg(LogLevel lvl, StringView msg);

  SourceLocation location;  // 产生日志的文件名、行号、函数名
  LogLevel level;           // 日志级别
  StringView message;       // 已经格式化好的消息正文
};

}  // namespace logger