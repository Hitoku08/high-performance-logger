// 文件作用：实现 ZlibCompress。
//
// zlib 里压缩叫 deflate，解压叫 inflate。
#include "compress/zlib_compress.h"

namespace logger {
namespace compress {

// 判断 input 是不是一段新压缩流的开头。
// zlib 流的前两个字节是固定的头：第一个字节是 0x78，第二个字节由压缩级别决定。
static bool IsStreamStart(const void* input, size_t input_size) {
    if (input_size < 2) {
        return false;
    }
    const unsigned char* p = static_cast<const unsigned char*>(input);
    return p[0] == 0x78 && (p[1] == 0x01 || p[1] == 0x5e || p[1] == 0x9c || p[1] == 0xda);
}

ZlibCompress::ZlibCompress() {
    ResetStream();
}

ZlibCompress::~ZlibCompress() {
    if (deflate_ready_) {
        deflateEnd(&deflate_stream_);  // 释放 zlib 内部分配的内存
    }
    if (inflate_ready_) {
        inflateEnd(&inflate_stream_);
    }
}

void ZlibCompress::ResetStream() {
    if (deflate_ready_) {
        deflateEnd(&deflate_stream_);
    }
    deflate_stream_ = z_stream{};  // 清零。zalloc/zfree 为空表示用 zlib 默认的内存分配
    deflate_ready_ = deflateInit(&deflate_stream_, Z_BEST_COMPRESSION) == Z_OK;
}

void ZlibCompress::ResetDecompressStream_() {
    if (inflate_ready_) {
        inflateEnd(&inflate_stream_);
    }
    inflate_stream_ = z_stream{};
    inflate_ready_ = inflateInit(&inflate_stream_) == Z_OK;
}

size_t ZlibCompress::CompressedBound(size_t input_size) {
    // compressBound 是一次性压缩的上限；流式压缩每次 flush 还会多出几个字节，再留 16 字节余量。
    return compressBound(static_cast<uLong>(input_size)) + 16;
}

size_t ZlibCompress::Compress(const void* input, size_t input_size, void* output, size_t output_size) {
    if (input == nullptr || input_size == 0 || output == nullptr || !deflate_ready_) {
        return 0;
    }
    // zlib 的接口不带 const，只能强转。它不会修改输入。
    deflate_stream_.next_in = static_cast<Bytef*>(const_cast<void*>(input));
    deflate_stream_.avail_in = static_cast<uInt>(input_size);
    deflate_stream_.next_out = static_cast<Bytef*>(output);
    deflate_stream_.avail_out = static_cast<uInt>(output_size);

    // Z_SYNC_FLUSH：把这条数据压完并全部输出，但不结束压缩流（和 zstd 的 ZSTD_e_flush 一样）。
    int ret = deflate(&deflate_stream_, Z_SYNC_FLUSH);

    // 输入没用完，或者输出空间被用光（可能还有数据没写出来），都算失败。
    if (ret != Z_OK || deflate_stream_.avail_in != 0 || deflate_stream_.avail_out == 0) {
        return 0;
    }
    return output_size - deflate_stream_.avail_out;  // 用掉的输出空间就是压缩后的长度
}

std::string ZlibCompress::Decompress(const void* input, size_t input_size) {
    if (input == nullptr || input_size == 0) {
        return "";
    }
    if (IsStreamStart(input, input_size)) {
        ResetDecompressStream_();
    }
    if (!inflate_ready_) {
        return "";  // 还没遇到过流的开头，没法解压
    }

    inflate_stream_.next_in = static_cast<Bytef*>(const_cast<void*>(input));
    inflate_stream_.avail_in = static_cast<uInt>(input_size);

    std::string result;
    char buffer[16 * 1024];
    // 和 zstd 一样：输入还没用完，或者上次 buffer 写满了，就继续。
    do {
        inflate_stream_.next_out = reinterpret_cast<Bytef*>(buffer);
        inflate_stream_.avail_out = sizeof(buffer);
        int ret = inflate(&inflate_stream_, Z_SYNC_FLUSH);
        if (ret == Z_BUF_ERROR) {
            break;  // 没有更多可以输出的了，不是错误
        }
        if (ret != Z_OK && ret != Z_STREAM_END) {
            return "";  // 数据坏了
        }
        result.append(buffer, sizeof(buffer) - inflate_stream_.avail_out);
    } while (inflate_stream_.avail_in > 0 || inflate_stream_.avail_out == 0);

    return result;
}

}  // namespace compress
}  // namespace logger
