#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "logger/log_handle.h"
#include "logger/sinks/sink.h"

namespace {

using logger::LogHandle;
using logger::LogLevel;
using logger::LogSinkPtr;
using logger::LogSinkPtrList;
using logger::SourceLocation;

// MockSink 记录的一条日志。
// 所有内容都拷贝成 std::string，因为 LogMsg 里的视图在 Log 返回后就可能失效。
struct RecordedMsg {
    std::string file_name;
    int32_t line;
    std::string func_name;
    LogLevel level;
    std::string message;
};

// 测试替身：不真正输出，只把收到的日志记下来，供断言使用。
class MockSink : public logger::LogSink {
public:
    void Log(const logger::LogMsg& msg) override {
        records.push_back({std::string(msg.location.file_name), msg.location.line, std::string(msg.location.func_name),
                           msg.level, std::string(msg.message)});
    }

    void SetFormatter(std::unique_ptr<logger::Formatter> /*formatter*/) override {}

    std::vector<RecordedMsg> records;
};

// 各测试共用的源码位置。
SourceLocation TestLocation() {
    return SourceLocation("/a/b/test.cc", 42, "TestFunc");
}

TEST(LogHandleTest, DefaultLevelIsInfo) {
    auto sink = std::make_shared<MockSink>();
    LogHandle handle(sink);

    EXPECT_EQ(handle.GetLevel(), LogLevel::kInfo);

    handle.Log(LogLevel::kDebug, TestLocation(), "debug");
    handle.Log(LogLevel::kInfo, TestLocation(), "info");
    handle.Log(LogLevel::kError, TestLocation(), "error");

    // kDebug 被丢弃，剩下两条按顺序到达。
    ASSERT_EQ(sink->records.size(), 2u);
    EXPECT_EQ(sink->records[0].message, "info");
    EXPECT_EQ(sink->records[1].message, "error");
}

TEST(LogHandleTest, TraceLevelLetsEverythingThrough) {
    auto sink = std::make_shared<MockSink>();
    LogHandle handle(sink);
    handle.SetLevel(LogLevel::kTrace);

    handle.Log(LogLevel::kTrace, TestLocation(), "m");
    handle.Log(LogLevel::kDebug, TestLocation(), "m");
    handle.Log(LogLevel::kInfo, TestLocation(), "m");
    handle.Log(LogLevel::kWarn, TestLocation(), "m");
    handle.Log(LogLevel::kError, TestLocation(), "m");
    handle.Log(LogLevel::kFatal, TestLocation(), "m");

    EXPECT_EQ(sink->records.size(), 6u);
}

TEST(LogHandletest, OffLevelDropsEverything) {
    auto sink = std::make_shared<MockSink>();
    LogHandle handle(sink);
    // level设成kOff会导致所有日志都会被丢弃
    handle.SetLevel(LogLevel::kOff);
    handle.Log(LogLevel::kFatal, TestLocation(), "fatal");
    handle.Log(LogLevel::kOff, TestLocation(), "off");

    EXPECT_TRUE(sink->records.empty());
}

TEST(LogHandletest, AllSinksReceiveTheSameMessage) {
    auto sink_a = std::make_shared<MockSink>();
    auto sink_b = std::make_shared<MockSink>();
    LogHandle handle{sink_a, sink_b};

    handle.Log(LogLevel::kWarn, TestLocation(), "hello");

    for (const auto& sink : {sink_a, sink_b}) {
        ASSERT_EQ(sink->records.size(), 1u);
        const RecordedMsg& record = sink->records[0];
        EXPECT_EQ(record.message, "hello");
        EXPECT_EQ(record.level, LogLevel::kWarn);
        EXPECT_EQ(record.line, 42);
        EXPECT_EQ(record.func_name, "TestFunc");
        EXPECT_EQ(record.file_name, "test.cc");  // SourceLocation只保留文件名
    }
}

TEST(LogHandleTest, NullSinkIsIgnored) {
    auto sink = std::make_shared<MockSink>();
    LogHandle handle{LogSinkPtr{}, sink};

    handle.Log(LogLevel::kInfo, TestLocation(), "hello");

    EXPECT_EQ(sink->records.size(), 1u);
}

}  // namespace