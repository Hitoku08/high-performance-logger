#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "logger/logger.h"
#include "logger/sinks/sink.h"

namespace {

using logger::LogLevel;
using logger::VariadicLogHandle;

// MockSink 记录的一条日志。内容都拷贝成 std::string，因为 LogMsg 里的视图在 Log 返回后就失效了。
struct RecordedMsg {
    std::string file_name;
    int32_t line;
    std::string func_name;
    LogLevel level;
    std::string message;
};

// 测试替身：只记录收到的日志，不真正输出。
class MockSink : public logger::LogSink {
public:
    void Log(const logger::LogMsg& msg) override {
        records.push_back({std::string(msg.location.file_name), msg.location.line, std::string(msg.location.func_name),
                           msg.level, std::string(msg.message)});
    }

    void SetFormatter(std::unique_ptr<logger::Formatter> /*formatter*/) override {}

    std::vector<RecordedMsg> records;
};

// 统计被格式化了多少次的类型，用来证明“被过滤的日志不会格式化参数”。
int g_format_count = 0;
struct Counted {};

}  // namespace

template <>
struct fmt::formatter<Counted> : fmt::formatter<std::string_view> {
    auto format(const Counted& /*value*/, fmt::format_context& ctx) const {
        ++g_format_count;
        return fmt::formatter<std::string_view>::format("counted", ctx);
    }
};

namespace {

class MacroApiTest : public testing::Test {
protected:
    void SetUp() override {
        sink_ = std::make_shared<MockSink>();
        handle_ = std::make_shared<VariadicLogHandle>(sink_);
        EXT_LOGGER_INIT(handle_);
        g_format_count = 0;
    }

    void TearDown() override { EXT_LOGGER_INIT(nullptr); }

    std::shared_ptr<MockSink> sink_;
    std::shared_ptr<VariadicLogHandle> handle_;
};

TEST_F(MacroApiTest, FormatsArgumentsAndRecordsLocation) {
    EXT_LOG_INFO("a={} b={}", 1, "x");
    const int expected_line = __LINE__ - 1;  // 上一行就是写日志的位置

    ASSERT_EQ(sink_->records.size(), 1u);
    const RecordedMsg& record = sink_->records[0];
    EXPECT_EQ(record.message, "a=1 b=x");
    EXPECT_EQ(record.level, LogLevel::kInfo);
    EXPECT_EQ(record.file_name, "macro_api_test.cc");  // 只保留文件名，不含目录
    EXPECT_EQ(record.line, expected_line);
    // 不同编译器的 __FUNCTION__ 不一样：GCC 是 "TestBody"，MSVC 会带上类名前缀，所以只检查包含关系。
    EXPECT_NE(record.func_name.find("TestBody"), std::string::npos) << record.func_name;
}

TEST_F(MacroApiTest, EachMacroUsesItsLevel) {
    handle_->SetLevel(LogLevel::kTrace);

    EXT_LOG_TRACE("m");
    EXT_LOG_DEBUG("m");
    EXT_LOG_INFO("m");
    EXT_LOG_WARN("m");
    EXT_LOG_ERROR("m");
    EXT_LOG_CRITICAL("m");

    const std::vector<LogLevel> expected = {LogLevel::kTrace, LogLevel::kDebug, LogLevel::kInfo,
                                            LogLevel::kWarn,  LogLevel::kError, LogLevel::kFatal};
    ASSERT_EQ(sink_->records.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(sink_->records[i].level, expected[i]) << "index " << i;
    }
}

TEST_F(MacroApiTest, FilteredLogDoesNotFormatArguments) {
    // 默认级别是 kInfo，DEBUG 会被过滤：Counted 不应该被格式化。
    EXT_LOG_DEBUG("{}", Counted{});
    EXPECT_EQ(g_format_count, 0);
    EXPECT_TRUE(sink_->records.empty());

    // INFO 能通过：Counted 被格式化一次。
    EXT_LOG_INFO("{}", Counted{});
    EXPECT_EQ(g_format_count, 1);
    ASSERT_EQ(sink_->records.size(), 1u);
    EXPECT_EQ(sink_->records[0].message, "counted");
}

TEST_F(MacroApiTest, NoGlobalHandleDoesNotCrash) {
    EXT_LOGGER_INIT(nullptr);
    EXT_LOG_INFO("nobody listens {}", 1);
    EXT_LOG_CRITICAL("still fine");
    EXPECT_TRUE(sink_->records.empty());
}

TEST_F(MacroApiTest, ExplicitHandleMacro) {
    auto other_sink = std::make_shared<MockSink>();
    auto other_handle = std::make_shared<VariadicLogHandle>(other_sink);

    LOG_LOGGER_WARN(other_handle, "to {}", "other");

    // 只有指定的 handle 收到，全局 handle 没有收到。
    ASSERT_EQ(other_sink->records.size(), 1u);
    EXPECT_EQ(other_sink->records[0].message, "to other");
    EXPECT_TRUE(sink_->records.empty());
}

TEST_F(MacroApiTest, MacroIsSafeInIfElseWithoutBraces) {
    bool else_ran = false;
    const bool condition = false;
    // 如果宏只是 if (...) { ... }，这里的 else 会和宏里的 if 配对，else_ran 就不会变成 true。
    if (condition)
        EXT_LOG_INFO("not here");
    else
        else_ran = true;

    EXPECT_TRUE(else_ran);
    EXPECT_TRUE(sink_->records.empty());
}

TEST_F(MacroApiTest, LongMessageIsNotTruncated) {
    // 超过栈上 256 字节缓冲的消息，fmt 会自动扩容，内容必须完整。
    const std::string long_text(1000, 'x');
    EXT_LOG_INFO("{}", long_text);

    ASSERT_EQ(sink_->records.size(), 1u);
    EXPECT_EQ(sink_->records[0].message, long_text);
}

TEST_F(MacroApiTest, LogWithoutLocation) {
    handle_->Log(LogLevel::kError, "code={}", 42);

    ASSERT_EQ(sink_->records.size(), 1u);
    EXPECT_EQ(sink_->records[0].message, "code=42");
    EXPECT_EQ(sink_->records[0].line, 0);  // 没有源码位置时行号为 0
    EXPECT_TRUE(sink_->records[0].file_name.empty());
}

}  // namespace