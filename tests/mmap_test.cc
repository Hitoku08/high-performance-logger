#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "logger/mmap/mmap_aux.h"

using logger::MMapAux;
namespace stdfs = std::filesystem;

class MmapTest : public testing::Test {
protected:
    // 每个测试开始前执行：建一个空的临时目录。
    void SetUp() override {
        dir_ = stdfs::temp_directory_path() / "logger_mmap_test";
        stdfs::remove_all(dir_);
        stdfs::create_directories(dir_);
        path_ = dir_ / "cache.mmap";
    }

    // 每个测试结束后执行：删除临时目录。
    void TearDown() override { stdfs::remove_all(dir_); }

    // 把 cache 里的数据转成 string，方便比较。
    static std::string ToString(const MMapAux& cache) {
        return std::string(reinterpret_cast<const char*>(cache.Data()), cache.Size());
    }

    stdfs::path dir_;
    stdfs::path path_;
};

TEST_F(MmapTest, NewFileIsEmpty) {
    MMapAux cache(path_);
    ASSERT_TRUE(cache.IsValid());
    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_TRUE(cache.Empty());
    EXPECT_TRUE(stdfs::exists(path_));
}

TEST_F(MmapTest, PushAppendsData) {
    MMapAux cache(path_);
    cache.Push("hello", 5);
    cache.Push(" world", 6);
    EXPECT_EQ(cache.Size(), 11u);
    EXPECT_FALSE(cache.Empty());
    EXPECT_EQ(ToString(cache), "hello world");
}

TEST_F(MmapTest, DataSurvivesReopen) {
    {
        MMapAux cache(path_);
        cache.Push("keep me", 7);
    }  // 析构，相当于程序退出

    MMapAux reopened(path_);  // 用同一个文件重新打开
    EXPECT_EQ(ToString(reopened), "keep me");
}

TEST_F(MmapTest, PushBeyondDefaultCapacity) {
    MMapAux cache(path_);

    // 写 1MB，超过默认的 512KB。每一块内容不同，方便检查有没有写错位置。
    std::vector<char> expected;
    for (int i = 0; i < 1024; ++i) {
        std::vector<char> block(1024, static_cast<char>(i % 256));
        cache.Push(block.data(), block.size());
        expected.insert(expected.end(), block.begin(), block.end());
    }

    ASSERT_EQ(cache.Size(), expected.size());
    EXPECT_EQ(std::memcmp(cache.Data(), expected.data(), expected.size()), 0);
}

TEST_F(MmapTest, LargeDataSurvivesReopen) {
    std::string big(800 * 1024, 'x');  // 比默认容量大
    {
        MMapAux cache(path_);
        cache.Push(big.data(), big.size());
    }
    MMapAux reopened(path_);
    EXPECT_EQ(ToString(reopened), big);
}

TEST_F(MmapTest, Clear) {
    MMapAux cache(path_);
    cache.Push("abc", 3);
    cache.Clear();
    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_TRUE(cache.Empty());

    cache.Push("xy", 2);  // 清空后从头开始写
    EXPECT_EQ(ToString(cache), "xy");
}

TEST_F(MmapTest, GetRatioGrows) {
    MMapAux cache(path_);
    EXPECT_EQ(cache.GetRatio(), 0.0);

    std::string data(100 * 1024, 'a');
    cache.Push(data.data(), data.size());
    double first = cache.GetRatio();
    EXPECT_GT(first, 0.0);
    EXPECT_LE(first, 1.0);

    cache.Push(data.data(), data.size());
    double second = cache.GetRatio();
    EXPECT_GT(second, first);
    EXPECT_LE(second, 1.0);
}

TEST_F(MmapTest, GarbageFileIsReset) {
    // 先写一个不是我们格式的文件，magic 不对。
    {
        std::ofstream ofs(path_, std::ios::binary);
        ofs << "this is not a mmap cache";
    }  // ofs 析构时关闭文件
    MMapAux cache(path_);
    ASSERT_TRUE(cache.IsValid());
    EXPECT_EQ(cache.Size(), 0u);  // 旧内容被当成无效，从头开始
}