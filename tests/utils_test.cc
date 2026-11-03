// M2 系统工具的单元测试，覆盖 sys_util、file_util 和 TimerCount。
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>

#include <gtest/gtest.h>

#ifdef _WIN32
#include <process.h>  // _getpid
#else
#include <unistd.h>  // getpid
#endif

#include "logger/defer.h"
#include "logger/utils/file_util.h"
#include "logger/utils/sys_util.h"
#include "logger/utils/timer_count.h"

namespace {

// 在系统临时目录下生成一个带进程 ID 的文件路径，避免多个测试进程之间互相覆盖。
std::filesystem::path TempFilePath(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("logger_utils_test_" + std::to_string(logger::GetProcessId()) + "_" + name);
}

// ---------- sys_util ----------

TEST(SysUtilTest, PageSizeIsPositivePowerOfTwo) {
  const size_t page_size = logger::GetPageSize();
  EXPECT_GT(page_size, 0u);
  // 2 的幂的二进制只有一位是 1，减 1 后与自身按位与必为 0。
  EXPECT_EQ(page_size & (page_size - 1), 0u);
}

TEST(SysUtilTest, ProcessIdMatchesSystemApi) {
#ifdef _WIN32
  const size_t expected = static_cast<size_t>(_getpid());
#else
  const size_t expected = static_cast<size_t>(getpid());
#endif
  EXPECT_EQ(logger::GetProcessId(), expected);
}

TEST(SysUtilTest, ThreadIdIsStableOnSameThread) {
  EXPECT_EQ(logger::GetThreadId(), logger::GetThreadId());
}

TEST(SysUtilTest, ThreadIdDiffersAcrossThreads) {
  const size_t main_id = logger::GetThreadId();
  size_t other_id = 0;
  std::thread t([&other_id] { other_id = logger::GetThreadId(); });
  t.join();
  // 主线程在整个测试期间一直存活，所以新线程不可能复用它的 ID。
  EXPECT_NE(main_id, other_id);
}

TEST(SysUtilTest, LocalTimeGivesSaneDate) {
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  logger::LocalTime(&tm, &now);
  EXPECT_GE(tm.tm_year + 1900, 2024);  // tm_year 是从 1900 年起的年数
  EXPECT_GE(tm.tm_mon, 0);
  EXPECT_LE(tm.tm_mon, 11);
  EXPECT_GE(tm.tm_mday, 1);
  EXPECT_LE(tm.tm_mday, 31);
}

// ---------- file_util ----------

TEST(FileUtilTest, FileSizeMatchesWrittenBytes) {
  const auto path = TempFilePath("size.bin");
  // 测试结束（无论成功失败）都删除临时文件。这里用到了 M1 的 LOG_DEFER。
  LOG_DEFER {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  };

  constexpr size_t kBytes = 1234;
  {
    std::ofstream out(path, std::ios::binary);
    out << std::string(kBytes, 'x');
  }  // 离开作用域时文件被关闭，数据确保已写入磁盘。

  EXPECT_EQ(logger::fs::GetFileSize(path), kBytes);
}

TEST(FileUtilTest, EmptyFileHasSizeZero) {
  const auto path = TempFilePath("empty.bin");
  LOG_DEFER {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  };
  { std::ofstream out(path, std::ios::binary); }

  EXPECT_EQ(logger::fs::GetFileSize(path), 0u);
}

TEST(FileUtilTest, MissingFileReturnsZero) {
  EXPECT_EQ(logger::fs::GetFileSize(TempFilePath("does_not_exist.bin")), 0u);
}

TEST(FileUtilTest, DirectoryReturnsZeroInsteadOfThrowing) {
  EXPECT_EQ(logger::fs::GetFileSize(std::filesystem::temp_directory_path()), 0u);
}

// ---------- TimerCount ----------

#ifdef ENABLE_LOG
// TimerCount 通过 internal_log.h 输出到 stdout，用 gtest 的捕获来断言内容。
TEST(TimerCountTest, PrintsNameAndCostOnScopeExit) {
  testing::internal::CaptureStdout();
  {
    TIMER_COUNT("sleep_task");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }  // 计时器在这里析构并输出
  const std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("sleep_task"), std::string::npos);
  EXPECT_NE(out.find("cost"), std::string::npos);
  EXPECT_NE(out.find("us"), std::string::npos);
}

TEST(TimerCountTest, TwoTimersInOneScopeCompile) {
  testing::internal::CaptureStdout();
  {
    TIMER_COUNT("first");
    TIMER_COUNT("second");  // 如果宏没有正确展开 __LINE__，这里会因为变量重名编译失败
  }
  const std::string out = testing::internal::GetCapturedStdout();

  // 后创建的先析构，所以 second 先输出。
  const size_t first_pos = out.find("first");
  const size_t second_pos = out.find("second");
  ASSERT_NE(first_pos, std::string::npos);
  ASSERT_NE(second_pos, std::string::npos);
  EXPECT_LT(second_pos, first_pos);
}
#endif

}  // namespace