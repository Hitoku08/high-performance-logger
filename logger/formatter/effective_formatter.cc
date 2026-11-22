#include "effective_formatter.h"

#include <chrono>

#include "effective_msg.pb.h"
#include "utils/sys_util.h"

namespace logger {

void EffectiveFormatter::Format(const LogMsg& msg, MemoryBuf* dest) {
    thread_local EffectiveMsg effective_msg;  // 每个线程独一份，不需要竞争

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    effective_msg.set_level(static_cast<int32_t>(msg.level));
    effective_msg.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    effective_msg.set_pid(static_cast<uint32_t>(GetProcessId()));
    effective_msg.set_tid(static_cast<uint32_t>(GetThreadId()));
    effective_msg.set_line(msg.location.line);  // line的类型就是int32_t
    effective_msg.set_file_name(msg.location.file_name.data(), msg.location.file_name.size());
    effective_msg.set_func_name(msg.location.func_name.data(), msg.location.func_name.size());
    effective_msg.set_log_info(msg.message.data(), msg.message.size());

    const size_t len = effective_msg.ByteSizeLong();  // 返回序列化之后占多少字节
    dest->resize(len);
    if (len > 0) {
        effective_msg.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(dest->data()));
    }
}

}  // namespace logger