#include "log_msg.h"

namespace logger {

LogMsg::LogMsg(SourceLocation loc, LogLevel lvl, StringView msg) : location(loc), level(lvl), message(msg) {}

// 委托构造，委托给主构造函数
LogMsg::LogMsg(LogLevel lvl, StringView msg) : LogMsg(SourceLocation{}, lvl, msg) {}

}  // namespace logger