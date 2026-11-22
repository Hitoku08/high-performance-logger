// 文件作用：演示 zstd 流式压缩的效果。
//
// 把同样的 1000 条日志用两种方式压缩，比较总大小：
//   1. 每条单独压缩（每条前都调用 ResetStream）
//   2. 流式压缩（只在开头调用一次 ResetStream）
// 然后把流式压缩的结果逐条解压，检查是否和原文一致。
#include <cstdio>
#include <string>
#include <vector>

#include "logger/compress/zstd_compress.h"

int main() {
    std::vector<std::string> logs;
    for (int i = 0; i < 1000; i++) {
        logs.push_back("[2024-01-01 12:00:00] [I] [effective_sink.cc:123] [1234:5678] user " + std::to_string(i) +
                       " logged in");
    }

    logger::compress::ZstdCompress zstd;
    size_t original = 0;
    size_t separate = 0;
    size_t streamed = 0;
    std::vector<std::string> compressed;

    // 1. 每条单独压缩
    for (const std::string& log : logs) {
        zstd.ResetStream();
        std::string out(zstd.CompressedBound(log.size()), '\0');
        separate += zstd.Compress(log.data(), log.size(), &out[0], out.size());
        original += log.size();
    }

    // 2. 流式压缩
    zstd.ResetStream();
    for (const std::string& log : logs) {
        std::string out(zstd.CompressedBound(log.size()), '\0');
        out.resize(zstd.Compress(log.data(), log.size(), &out[0], out.size()));
        streamed += out.size();
        compressed.push_back(out);
    }

    std::printf("原始大小:       %zu 字节\n", original);
    std::printf("每条单独压缩:   %zu 字节\n", separate);
    std::printf("流式压缩:       %zu 字节\n", streamed);

    // 按顺序解压
    for (size_t i = 0; i < logs.size(); i++) {
        if (zstd.Decompress(compressed[i].data(), compressed[i].size()) != logs[i]) {
            std::printf("第 %zu 条解压结果不一致\n", i);
            return 1;
        }
    }
    std::printf("全部 %zu 条解压正确\n", logs.size());
    return 0;
}
