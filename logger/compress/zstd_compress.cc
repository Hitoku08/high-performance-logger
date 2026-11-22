// 文件作用：实现 ZstdCompress。
#include "compress/zstd_compress.h"

#include <cstring>

namespace logger {
namespace compress {

// 每个 zstd 帧（一段压缩流）都以这 4 个字节开头。
static constexpr unsigned char kZstdMagic[4] = {0x28, 0xb5, 0x2f, 0xfd};

// 判断 input 是不是一段新压缩流的开头。
static bool IsFrameStart(const void* input, size_t input_size) {
    return input_size >= sizeof(kZstdMagic) && std::memcmp(input, kZstdMagic, sizeof(kZstdMagic)) == 0;
}

ZstdCompress::ZstdCompress() : cctx_(ZSTD_createCCtx()), dctx_(ZSTD_createDCtx()) {
    // 压缩级别 1~22，越大压得越小但越慢。5 是速度和压缩率的折中。
    ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, 5);
}

ZstdCompress::~ZstdCompress() {
    ZSTD_freeCCtx(cctx_);  // 传 nullptr 也是安全的
    ZSTD_freeDCtx(dctx_);
}

void ZstdCompress::ResetStream() {
    // 丢掉之前记住的内容，下一次 Compress 会开始一个新的帧（以 kZstdMagic 开头）。
    ZSTD_CCtx_reset(cctx_, ZSTD_reset_session_only);
}

size_t ZstdCompress::CompressedBound(size_t input_size) {
    return ZSTD_compressBound(input_size);
}

size_t ZstdCompress::Compress(const void* input, size_t input_size, void* output, size_t output_size) {
    if (input == nullptr || input_size == 0 || output == nullptr) {
        return 0;
    }
    ZSTD_inBuffer in = {input, input_size, 0};
    ZSTD_outBuffer out = {output, output_size, 0};

    // ZSTD_e_flush：把这条数据压完并全部输出，但不结束这个帧。
    // 下一条数据还在同一个帧里，可以参考这一条的内容，这就是“流式”。
    size_t remaining = ZSTD_compressStream2(cctx_, &out, &in, ZSTD_e_flush);

    // 出错，或者 remaining 不为 0（输出空间不够，还有数据没写出来），都算失败。
    if (ZSTD_isError(remaining) || remaining != 0) {
        return 0;
    }
    return out.pos;  // pos 是实际写入的字节数
}

std::string ZstdCompress::Decompress(const void* input, size_t input_size) {
    if (input == nullptr || input_size == 0) {
        return "";
    }
    // 遇到新帧的开头，说明压缩端调用过 ResetStream，解压端也要从头开始。
    if (IsFrameStart(input, input_size)) {
        ZSTD_DCtx_reset(dctx_, ZSTD_reset_session_only);
    }

    std::string result;
    char buffer[16 * 1024];  // 每次最多解压出 16KB，放进 buffer 后追加到 result
    ZSTD_inBuffer in = {input, input_size, 0};
    bool buffer_full = false;

    // 循环到“输入用完”并且“上一次 buffer 没写满”为止。
    // buffer 写满了说明 zstd 可能还有数据没吐出来，要再调用一次。
    // 这样多长的数据都能完整解压，没有长度上限。
    do {
        ZSTD_outBuffer out = {buffer, sizeof(buffer), 0};
        size_t ret = ZSTD_decompressStream(dctx_, &out, &in);
        if (ZSTD_isError(ret)) {
            return "";
        }
        result.append(buffer, out.pos);
        buffer_full = (out.pos == out.size);
    } while (in.pos < in.size || buffer_full);

    return result;
}

}  // namespace compress
}  // namespace logger
