// 文件作用：解码逻辑。按 EffectiveSink 写入的格式反向还原日志文件：
//
//   文件： [ ChunkHeader | ItemHeader | 密文 | ItemHeader | 密文 | ... ][ ChunkHeader | ... ] ...
//   每个 chunk：用 "服务端私钥 + ChunkHeader 里的客户端公钥" 做 ECDH 得到 AES 密钥，并新建一个 zstd 解压流
//   每个 item： AES 解密 → zstd 解压 → EffectiveMsg 反序列化 → DecodeFormatter 格式化成一行
//
// 解码逻辑单独放在这里（编译成静态库 decoder_core），decoder 程序和单元测试都链接它。
#pragma once

#include <cstddef>
#include <filesystem>
#include <ostream>
#include <stdexcept>
#include <string>

#include "decode_formatter.h"

namespace logger {
namespace decode {

// 文件格式不对、私钥不对、解密或解压失败时抛出
class DecodeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct DecodeStats {
    size_t chunk_count = 0;
    size_t item_count = 0;
};

// 解码一段日志文件内容，每条日志格式化后写一行到 out。
// server_pri_key_hex：服务端私钥（十六进制）。
// 出错时抛 DecodeError；此时 out 里可能已经写了出错位置之前的内容。
DecodeStats DecodeData(const std::string& data,
                       const std::string& server_pri_key_hex,
                       const DecodeFormatter& formatter,
                       std::ostream& out);

// 读取整个文件后调用 DecodeData。文件打不开时也抛 DecodeError。
DecodeStats DecodeFile(const std::filesystem::path& log_file,
                       const std::string& server_pri_key_hex,
                       const DecodeFormatter& formatter,
                       std::ostream& out);

}  // namespace decode
}  // namespace logger
