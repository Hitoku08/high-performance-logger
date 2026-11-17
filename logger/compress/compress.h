// 文件作用：压缩算法的公共接口。zstd 和 zlib 两种实现都继承它，EffectiveSink 只认这个接口。
//
// “流式压缩”是什么意思：
//   普通压缩每次都从零开始，每条日志单独压缩，压缩率很低（日志很短，没什么可复用的）。
//   流式压缩会记住之前压过的内容，后面的日志里出现相同的片段（比如相同的文件名、相同的格式），
//   就只记一个“和前面第 N 个字节开始的那段一样”，压缩率高很多。
//   代价是解压时也必须从同一个起点开始、按同样的顺序一条一条解压。
//
// 用法：
//     ZstdCompress c;
//     c.ResetStream();                                   // 开始一段新的压缩流
//     std::string out(c.CompressedBound(n), '\0');       // 准备足够大的输出空间
//     size_t len = c.Compress(data, n, out.data(), out.size());
//     out.resize(len);
//     std::string back = c.Decompress(out.data(), out.size());  // 解压
#pragma once

#include <cstddef>
#include <string>

namespace logger {

namespace compress {

class Compression {
public:
    virtual ~Compression() = default;

    // 压缩 input，结果写进 output，返回写了多少字节。失败或输入为空时返回 0。
    virtual size_t Compress(const void* input, size_t input_size, void* output, size_t output_size) = 0;

    // 压缩 input_size 字节最坏需要多大的输出空间
    virtual size_t CompressedBound(size_t input_size) = 0;

    // 解压一条 Compress 的输出，失败或者输入为空时返回空字符串
    virtual std::string Decompress(const void* input, size_t input_size) = 0;

    // 开始一段新的压缩流，之后压缩的数据不再参考之前的内容
    virtual void ResetStream() = 0;
};
}  // namespace compress

}  // namespace logger