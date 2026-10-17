#include <vector>

#include <gtest/gtest.h>

#include "logger/defer.h"
#include "logger/internal_log.h"
#include "logger/log_common.h"
#include "logger/space.h"

namespace {

TEST(SourceLocationTest, KeepsOnlyFileName) {
  EXPECT_EQ(logger::SourceLocation("/a/b/c.cc", 1, "f").file_name, "c.cc");
  EXPECT_EQ(logger::SourceLocation("C:\\a\\c.cc", 1, "f").file_name, "c.cc");
  // 混合分隔符取最后一个
  EXPECT_EQ(logger::SourceLocation("C:\\a/b\\c.cc", 1, "f").file_name, "c.cc");
  // 没有目录时原样保留
  EXPECT_EQ(logger::SourceLocation("c.cc", 1, "f").file_name, "c.cc");
}

TEST(SourceLocationTest, KeepsLineAndFunction) {
  const logger::SourceLocation loc("/a/b/c.cc", 42, "Func");
  EXPECT_EQ(loc.line, 42);
  EXPECT_EQ(loc.func_name, "Func");
}

TEST(SpaceTest, CastsBetweenUnits) {
  EXPECT_EQ(logger::space_cast<logger::bytes>(logger::megabytes{4}).count(), 4u * 1024 * 1024);

  const logger::kilobytes kb = logger::megabytes{4};  // 隐式转换走的是 space.h 里的转换构造函数
  EXPECT_EQ(kb.count(), 4u * 1024);

  const logger::megabytes mb = logger::kilobytes{2048};
  EXPECT_EQ(mb.count(), 2u);
}

TEST(SpaceTest, TruncatesInsteadOfRounding) {
  const logger::megabytes mb = logger::kilobytes{1000};  // 不足 1MB
  EXPECT_EQ(mb.count(), 0u);
}

TEST(SpaceTest, DefaultConstructedIsZero) {
  const logger::bytes b;
  EXPECT_EQ(b.count(), 0u);
}

TEST(DeferTest, RunsAtScopeExit) {
  bool ran = false;
  {
    LOG_DEFER { ran = true; };
    EXPECT_FALSE(ran);  // 还没离开作用域
  }
  EXPECT_TRUE(ran);
}

TEST(DeferTest, RunsInReverseOrder) {
  std::vector<int> order;
  {
    LOG_DEFER { order.push_back(1); };
    LOG_DEFER { order.push_back(2); };
  }
  EXPECT_EQ(order, (std::vector<int>{2, 1}));
}

TEST(LogLevelTest, ValuesMatchLevelMacros) {
  EXPECT_EQ(static_cast<int>(logger::LogLevel::kTrace), LOGGER_LEVEL_TRACE);
  EXPECT_EQ(static_cast<int>(logger::LogLevel::kDebug), LOGGER_LEVEL_DEBUG);
  EXPECT_EQ(static_cast<int>(logger::LogLevel::kInfo), LOGGER_LEVEL_INFO);
  EXPECT_EQ(static_cast<int>(logger::LogLevel::kWarn), LOGGER_LEVEL_WARN);
  EXPECT_EQ(static_cast<int>(logger::LogLevel::kError), LOGGER_LEVEL_ERROR);
  EXPECT_EQ(static_cast<int>(logger::LogLevel::kFatal), LOGGER_LEVEL_CRITICAL);
  EXPECT_EQ(static_cast<int>(logger::LogLevel::kOff), LOGGER_LEVEL_OFF);
}

#ifdef ENABLE_LOG
// internal_log.h 是往 stdout 打印的，用 gtest 自带的捕获来断言内容。
TEST(InternalLogTest, PrintsLevelAndMessage) {
  testing::internal::CaptureStdout();
  LOG_INFO("value={}", 7);
  const std::string out = testing::internal::GetCapturedStdout();
  EXPECT_NE(out.find("[INFO]"), std::string::npos);
  EXPECT_NE(out.find("value=7"), std::string::npos);
}
#endif

}  // namespace