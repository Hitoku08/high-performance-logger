#include <gtest/gtest.h>

#include "third_party/fmt/include/fmt/format.h"

// M0：验证工具链、头文件搜索路径和测试框架本身可用。
TEST(BuildSanityTest, FmtHeaderOnlyWorks) {
  EXPECT_EQ(fmt::format("{}-{}", 42, "logger"), "42-logger");
}
