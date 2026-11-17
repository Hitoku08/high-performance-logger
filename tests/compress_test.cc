// 文件作用：测试 M8 的 ZstdCompress 和 ZlibCompress。
//
// 两种实现接口相同，所以测试逻辑写成接收 Compression& 的函数，zstd 和 zlib 各调用一遍。
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "logger/compress/zlib_compress.h"
#include "logger/compress/zstd_compress.h"

using logger::compress::Compression;
using logger::compress::ZlibCompress;
using logger::compress::ZstdCompress;

static std::string CompressOne(Compression& c, const std::string& input) {
    std::string out(c.CompressedBound(input.size()), '\0');
    size_t len = c.Compress(input.data(), input.size(), &out[0], out.size());
    out.resize(len);
    return out;
}
static void CheckRoundTrip(Compression& c) {
    c.ResetStream();
    std::string input = "[I] [main.cc:42] hello logger";
    std::string compressed = CompressOne(c, input);
    ASSERT_FALSE(compressed.empty());
    EXPECT_EQ(c.Decompress(compressed.data(), compressed.size()), input);
}

// 一个流里连续压缩多条，再按顺序逐条解压，每条都能还原。
static void CheckStream(Compression& c) {
    std::vector<std::string> inputs;
    for (int i = 0; i < 100; i++) {
        inputs.push_back("[I] [effective_sink.cc:" + std::to_string(i) + "] message number " + std::to_string(i));
    }
    c.ResetStream();
    std::vector<std::string> compressed;
    for (const std::string& s : inputs) {
        compressed.push_back(CompressOne(c, s));
    }
    for (size_t i = 0; i < inputs.size(); i++) {
        EXPECT_EQ(c.Decompress(compressed[i].data(), compressed[i].size()), inputs[i]) << "第" << i << "条";
    }
}

// ResetStream 后开启新的流，解压端能自动识别并从头开始
static void CheckTwoStreams(Compression& c) {
    c.ResetStream();
    std::string a = CompressOne(c, "first stream");
    c.ResetStream();
    std::string b = CompressOne(c, "second stream");

    EXPECT_EQ(c.Decompress(a.data(), a.size()), "first stream");
    EXPECT_EQ(c.Decompress(b.data(), b.size()), "second stream");
}

// 重复度高的数据压缩后明显变小
static void CheckRatio(Compression& c) {
    c.ResetStream();
    std::string input;
    for (int i = 0; i < 1000; i++) {
        input += "the same log line again and again\n";
    }
    std::string compressed = CompressOne(c, input);
    ASSERT_FALSE(compressed.empty());
    EXPECT_LT(compressed.size(), input.size() / 10);  // 小于原来的1/10
}

// 空输入：压缩返回 0，解压返回空串
static void CheckEmpty(Compression& c) {
    char out[64];
    EXPECT_EQ(c.Compress("", 0, out, sizeof(out)), 0u);
    EXPECT_EQ(c.Compress(nullptr, 10, out, sizeof(out)), 0u);
    EXPECT_EQ(c.Decompress("", 0), "");
    EXPECT_EQ(c.Decompress(nullptr, 10), "");
}

// 超过10KB的数据也能完整往返
static void CheckLarge(Compression& c) {
    c.ResetStream();
    std::string input;
    for (int i = 0; i < 200000; i++) {
        input += static_cast<char>('a' + (i * 7919) % 26);  // 200KB
    }
    std::string compressed = CompressOne(c, input);
    ASSERT_FALSE(compressed.empty());
    EXPECT_EQ(c.Decompress(compressed.data(), compressed.size()), input);
}

// 输出空间不够时返回 0
static void CheckOutputTooSmall(Compression& c) {
    c.ResetStream();
    std::string input(1000, 'x');
    char out[4];
    EXPECT_EQ(c.Compress(input.data(), input.size(), out, sizeof(out)), 0u);
}

// ---------- zstd ----------

TEST(ZstdCompressTest, RoundTrip) {
    ZstdCompress c;
    CheckRoundTrip(c);
}
TEST(ZstdCompressTest, Stream) {
    ZstdCompress c;
    CheckStream(c);
}
TEST(ZstdCompressTest, TwoStreams) {
    ZstdCompress c;
    CheckTwoStreams(c);
}
TEST(ZstdCompressTest, Ratio) {
    ZstdCompress c;
    CheckRatio(c);
}
TEST(ZstdCompressTest, Empty) {
    ZstdCompress c;
    CheckEmpty(c);
}
TEST(ZstdCompressTest, Large) {
    ZstdCompress c;
    CheckLarge(c);
}
TEST(ZstdCompressTest, OutputTooSmall) {
    ZstdCompress c;
    CheckOutputTooSmall(c);
}

// ---------- zlib ----------

TEST(ZlibCompressTest, RoundTrip) {
    ZlibCompress c;
    CheckRoundTrip(c);
}
TEST(ZlibCompressTest, Stream) {
    ZlibCompress c;
    CheckStream(c);
}
TEST(ZlibCompressTest, TwoStreams) {
    ZlibCompress c;
    CheckTwoStreams(c);
}
TEST(ZlibCompressTest, Ratio) {
    ZlibCompress c;
    CheckRatio(c);
}
TEST(ZlibCompressTest, Empty) {
    ZlibCompress c;
    CheckEmpty(c);
}
TEST(ZlibCompressTest, Large) {
    ZlibCompress c;
    CheckLarge(c);
}
TEST(ZlibCompressTest, OutputTooSmall) {
    ZlibCompress c;
    CheckOutputTooSmall(c);
}

// 不调用 ResetStream 也能直接用（参考实现的 zlib 必须先 ResetStream，否则压缩返回 0）。
TEST(ZlibCompressTest, WorksWithoutReset) {
    ZlibCompress c;
    std::string compressed = CompressOne(c, "no reset");
    ASSERT_FALSE(compressed.empty());
    EXPECT_EQ(c.Decompress(compressed.data(), compressed.size()), "no reset");
}