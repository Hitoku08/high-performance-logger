// 文件作用：用 zlib 实现的流式压缩。作为 zstd 的备选，接口完全一样。
#pragma once

#include "compress/compress.h"
#include "third_party/zlib/include/zlib.h"

namespace logger {
namespace compress {

class ZlibCompress : public Compression {
public:
    ZlibCompress();  // 构造时就准备好压缩流，不调用 ResetStream 也能直接压缩
    ~ZlibCompress() override;

    // z_stream 里有 zlib 内部分配的内存，不能拷贝。
    ZlibCompress(const ZlibCompress&) = delete;
    ZlibCompress& operator=(const ZlibCompress&) = delete;

    size_t Compress(const void* input, size_t input_size, void* output, size_t output_size) override;
    size_t CompressedBound(size_t input_size) override;
    std::string Decompress(const void* input, size_t input_size) override;
    void ResetStream() override;

private:
    void ResetDecompressStream_();

    z_stream deflate_stream_{};   // 压缩用
    z_stream inflate_stream_{};   // 解压用
    bool deflate_ready_ = false;  // deflate_stream_ 是否初始化成功
    bool inflate_ready_ = false;  // inflate_stream_ 是否初始化成功
};

}  // namespace compress
}  // namespace logger
