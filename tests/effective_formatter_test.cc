#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "effective_msg.pb.h"
#include "logger/formatter/effective_formatter.h"
#include "logger/utils/sys_util.h"

using logger::EffectiveFormatter;
using logger::EffectiveMsg;
using logger::LogLevel;
using logger::LogMsg;
using logger::MemoryBuf;
using logger::SourceLocation;

EffectiveMsg FormatAndParse(const LogMsg& msg) {
    // 序列化->反序列化
    EffectiveFormatter formatter;
    MemoryBuf buf;
    formatter.Format(msg, &buf);
    EffectiveMsg parsed;
    EXPECT_TRUE(parsed.ParseFromString(buf));  // 反序列化
    return parsed;
}

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

TEST(EffectiveFormatter, FieldsRoundTrip) {
    const LogMsg msg(SourceLocation("a/b/file.cc", 123, "TestFunc"), LogLevel::kWarn, "hello world");
    const EffectiveMsg parsed = FormatAndParse(msg);
    EXPECT_EQ(parsed.level(), static_cast<int32_t>(LogLevel::kWarn));
    EXPECT_EQ(parsed.line(), 123);
    EXPECT_EQ(parsed.file_name(), "file.cc");
    EXPECT_EQ(parsed.func_name(), "TestFunc");
    EXPECT_EQ(parsed.log_info(), "hello world");
}

TEST(EffectiveFormatter, AllLevelsRoundTrip) {
    for (LogLevel level :
         {LogLevel::kTrace, LogLevel::kDebug, LogLevel::kInfo, LogLevel::kWarn, LogLevel::kError, LogLevel::kFatal}) {
        const EffectiveMsg parsed = FormatAndParse(LogMsg(SourceLocation("f.cc", 1, "f"), level, "x"));
        EXPECT_EQ(parsed.level(), static_cast<int32_t>(level));
    }
}

TEST(EffectiveFormatter, PidTidAndTimestamp) {
    const int64_t before = NowMs();
    const EffectiveMsg parsed = FormatAndParse(LogMsg(SourceLocation("f.cc", 1, "f"), LogLevel::kInfo, "x"));
    const int64_t after = NowMs();

    EXPECT_EQ(parsed.pid(), static_cast<uint32_t>(logger::GetProcessId()));
    EXPECT_EQ(parsed.tid(), static_cast<uint32_t>(logger::GetThreadId()));
    // 允许数秒误差
    EXPECT_GE(parsed.timestamp(), before - 5000);
    EXPECT_LE(parsed.timestamp(), after + 5000);
}

TEST(EffectiveFormatter, DifferentThreadHasItsOwnTid) {
    uint32_t other_tid = 0;
    uint32_t expected_tid = 0;
    std::thread t([&] {
        expected_tid = static_cast<uint32_t>(logger::GetThreadId());
        other_tid = FormatAndParse(LogMsg(SourceLocation("f.cc", 1, "f"), LogLevel::kInfo, "x")).tid();
    });
    t.join();
    EXPECT_EQ(other_tid, expected_tid);
    EXPECT_NE(other_tid, static_cast<uint32_t>(logger::GetThreadId()));
}

TEST(EffectiveFormatter, EmptyMessage) {
    const EffectiveMsg parsed = FormatAndParse(LogMsg(SourceLocation(), LogLevel::kInfo, ""));
    EXPECT_EQ(parsed.log_info(), "");
    EXPECT_EQ(parsed.file_name(), "");
    EXPECT_EQ(parsed.line(), 0);
}

TEST(EffectiveFormatter, LongMessage) {
    const std::string text(1024 * 1024, 'a');
    const EffectiveMsg parsed = FormatAndParse(LogMsg(SourceLocation("f.cc", 1, "f"), LogLevel::kInfo, text));
    EXPECT_EQ(parsed.log_info(), text);
}

// Format 覆盖 dest 而不是追加；同一线程连续格式化时，短消息不会残留上一条长消息的内容
TEST(EffectiveFormatter, OverwritesDestAndDoesNotLeakPreviousMessage) {
    EffectiveFormatter formatter;
    MemoryBuf buf = "garbage";
    formatter.Format(LogMsg(SourceLocation("long_name.cc", 99, "LongFunc"), LogLevel::kError, "a long message"), &buf);
    formatter.Format(LogMsg(SourceLocation("s.cc", 1, "f"), LogLevel::kInfo, "s"), &buf);

    EffectiveMsg parsed;
    ASSERT_TRUE(parsed.ParseFromString(buf));
    EXPECT_EQ(parsed.log_info(), "s");
    EXPECT_EQ(parsed.file_name(), "s.cc");
    EXPECT_EQ(parsed.func_name(), "f");
    EXPECT_EQ(parsed.line(), 1);
    EXPECT_EQ(parsed.level(), static_cast<int32_t>(LogLevel::kInfo));
}