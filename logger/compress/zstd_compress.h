#pragma once

#include "compress/compress.h"
#include "third_party/zstd/lib/zstd.h"

namespace logger {

namespace compress {

class ZstdCompress : public Compression {
public:
    ZstdCompress();
    ~ZstdCompress() override;

    // 不能拷贝
    ZstdCompress(const ZstdCompress&) = delete;
    ZstdCompress& operator=(const ZstdCompress&) = delete;

    size_t Compress(const void* input, size_t input_size, void* output, size_t output_size) override;
    size_t CompressedBound(size_t input_size) override;
    std::string Decompress(const void* input, size_t input_size) override;
    void ResetStream() override;

private:
    ZSTD_CCtx* cctx_;  // 压缩上下文
    ZSTD_DCtx* dctx_;  // 解压上下文
};

}  // namespace compress

}  // namespace logger