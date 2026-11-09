// 文件作用：测试 M4 的 DefaultFormatter 和 ConsoleSink。
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "logger/formatter/default_formatter.h"
#include "logger/log_handle.h"
#include "logger/sinks/console_sink.h"
#include "logger/utils/sys_util.h"

namespace {

using logger::ConsoleSink;
using logger::DefaultFormatter;
using logger::LogLevel;
using logger::LogMsg;
using logger::MemoryBuf;
using logger::SourceLocation;

LogMsg MakeMsg(LogLevel level, logger::StringView message) {
    return LogMsg(SourceLocation("a/b/file.cc", 123, "TestFunc"), level, message);
}

std::string FormatOnce(const LogMsg& msg) {
    DefaultFormatter formatter;
    MemoryBuf buf;
    formatter.Format(msg, &buf);
    return buf;
}

class CustomFormatter : public logger::Formatter {
public:
    void Format(const LogMsg& msg, MemoryBuf* dest) override {
        dest->append("custom: ");
        dest->append(msg.message.data(), msg.message.size());
    }
};

TEST(DefaultFormatterTest, ContainsLevelLocationPidAndMessage) {
    const std::string out = FormatOnce(MakeMsg(LogLevel::kInfo, "hello world"));

    EXPECT_NE(out.find("[I]"), std::string::npos) << out;
    EXPECT_NE(out.find("[file.cc:123]"), std::string::npos) << out;
    EXPECT_NE(out.find("hello world"), std::string::npos) << out;

    const std::string pid_part = "[" + std::to_string(logger::GetProcessId()) + ":";
    EXPECT_NE(out.find(pid_part), std::string::npos) << out;
}

TEST(DefaultFormatterTest, MatchesExpectedLayout) {
    const std::string out = FormatOnce(MakeMsg(LogLevel::kWarn, "msg"));

    const std::regex pattern(R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\] \[W\] \[file\.cc:123\] \[\d+:\d+\] msg$)");
    EXPECT_TRUE(std::regex_match(out, pattern)) << out;
}

TEST(DefaultFormatterTest, EachLevelMapsToItsLetter) {
    EXPECT_EQ(logger::LevelToChar(LogLevel::kTrace), 'T');
    EXPECT_EQ(logger::LevelToChar(LogLevel::kDebug), 'D');
    EXPECT_EQ(logger::LevelToChar(LogLevel::kInfo), 'I');
    EXPECT_EQ(logger::LevelToChar(LogLevel::kWarn), 'W');
    EXPECT_EQ(logger::LevelToChar(LogLevel::kError), 'E');
    EXPECT_EQ(logger::LevelToChar(LogLevel::kFatal), 'F');
    EXPECT_EQ(logger::LevelToChar(LogLevel::kOff), '?');  // 非法级别不越界
    EXPECT_NE(FormatOnce(MakeMsg(LogLevel::kError, "m")).find("[E]"), std::string::npos);
}

TEST(ConsoleSinkTest, WritesFormattedLineToStdout) {
    ConsoleSink sink;
    testing::internal::CaptureStdout();
    sink.Log(MakeMsg(LogLevel::kInfo, "to console"));
    sink.Flush();
    const std::string out = testing::internal::GetCapturedStdout();

    EXPECT_NE(out.find("[I]"), std::string::npos) << out;
    EXPECT_NE(out.find("[file.cc:123]"), std::string::npos) << out;
    EXPECT_NE(out.find("to console"), std::string::npos) << out;
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out.back(), '\n');
}

TEST(ConsoleSinkTest, SetFormatterChangesOutput) {
    ConsoleSink sink;
    sink.SetFormatter(std::make_unique<CustomFormatter>());

    testing::internal::CaptureStdout();
    sink.Log(MakeMsg(LogLevel::kInfo, "abc"));
    sink.Flush();
    const std::string out = testing::internal::GetCapturedStdout();

    EXPECT_EQ(out, "custom: abc\n");
}

TEST(ConsoleSinkTest, NullFormatterIsIgnored) {
    ConsoleSink sink;
    sink.SetFormatter(nullptr);  // 如果指针是空，那就是默认的Default Formatter

    testing::internal::CaptureStdout();
    sink.Log(MakeMsg(LogLevel::kInfo, "still works"));
    sink.Flush();
    const std::string out = testing::internal::GetCapturedStdout();

    EXPECT_NE(out.find("[I]"), std::string::npos) << out;
    EXPECT_NE(out.find("still works"), std::string::npos) << out;
}

TEST(ConsoleSinkTest, WorksThroughLogHandle) {
    auto sink = std::make_shared<ConsoleSink>();
    logger::LogHandle handle(sink);

    testing::internal::CaptureStdout();
    handle.Log(LogLevel::kDebug, SourceLocation("/a/file.cc", 1, "f"), "dropped");
    handle.Log(LogLevel::kError, SourceLocation("/a/file.cc", 2, "f"), "kept");
    sink->Flush();
    const ::std::string out = testing::internal::GetCapturedStdout();

    EXPECT_EQ(out.find("dropped"), std::string::npos) << out;
    EXPECT_NE(out.find("kept"), std::string::npos) << out;
}

TEST(ConsoleSinkTest, ConcurrentLogsDoNotInterleave) {
    ConsoleSink sink;
    sink.SetFormatter(std::make_unique<CustomFormatter>());
    constexpr int kThreads = 4;
    constexpr int kPerThread = 200;

    testing::internal::CaptureStdout();
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; i++) {
        threads.emplace_back([&sink, kPerThread] {
            for (int j = 0; j < kPerThread; j++) {
                sink.Log(MakeMsg(LogLevel::kInfo, "0123456789"));
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    sink.Flush();
    const std::string out = testing::internal::GetCapturedStdout();

    std::string expected;
    for (int i = 0; i < kThreads * kPerThread; i++) {
        expected += "custom: 0123456789\n";
    }
    EXPECT_EQ(out, expected);
}

}  // namespace