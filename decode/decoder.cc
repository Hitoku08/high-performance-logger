#include "decoder.h"

#include <cstring>
#include <fstream>
#include <iterator>

#include "effective_msg.pb.h"
#include "logger/compress/zstd_compress.h"
#include "logger/crypt/aes_crypt.h"
#include "logger/crypt/crypt.h"
#include "logger/sinks/effective_sink.h"

namespace logger {
namespace decode {

namespace {

using detail::ChunkHeader;
using detail::ItemHeader;

// 从 data[pos] 读出一个结构体。用 memcpy 而不是 reinterpret_cast：文件内容不保证按结构体对齐
template <typename T>
T ReadStruct(const std::string& data, size_t pos) {
    T value;
    std::memcpy(&value, data.data() + pos, sizeof(T));
    return value;
}

std::string Where(size_t pos) {
    return " (offset " + std::to_string(pos) + ")";
}

}  // namespace

DecodeStats DecodeData(const std::string& data,
                       const std::string& server_pri_key_hex,
                       const DecodeFormatter& formatter,
                       std::ostream& out) {
    const std::string server_pri = crypt::HexKeyToBinary(server_pri_key_hex);
    if (server_pri.size() != crypt::kPrivateKeyLength) {
        throw DecodeError("invalid server private key: expect " + std::to_string(crypt::kPrivateKeyLength * 2) +
                          " hex chars");
    }

    DecodeStats stats;
    std::string line;
    size_t pos = 0;
    while (pos < data.size()) {
        // ---------- chunk ----------
        if (data.size() - pos < sizeof(ChunkHeader)) {
            throw DecodeError("truncated chunk header" + Where(pos));
        }
        const ChunkHeader chunk = ReadStruct<ChunkHeader>(data, pos);
        if (chunk.magic != ChunkHeader::kMagic) {
            throw DecodeError("invalid chunk magic, not a log file or corrupted" + Where(pos));
        }
        pos += sizeof(ChunkHeader);
        if (chunk.size > data.size() - pos) {
            throw DecodeError("truncated chunk data" + Where(pos));
        }

        // 每个 chunk 可能来自不同的进程（不同的客户端密钥），并且是独立的 zstd 流
        const std::string client_pub(chunk.pub_key, crypt::kPublicKeyLength);
        const std::string shared_secret = crypt::GenECDHSharedSecret(server_pri, client_pub);
        if (shared_secret.empty()) {
            throw DecodeError("ECDH failed, invalid client public key in chunk" + Where(pos));
        }
        crypt::AESCrypt aes(shared_secret);
        compress::ZstdCompress zstd;
        ++stats.chunk_count;

        // ---------- items ----------
        const size_t chunk_end = pos + chunk.size;
        while (pos < chunk_end) {
            if (chunk_end - pos < sizeof(ItemHeader)) {
                throw DecodeError("truncated item header" + Where(pos));
            }
            const ItemHeader item = ReadStruct<ItemHeader>(data, pos);
            if (item.magic != ItemHeader::kMagic) {
                throw DecodeError("invalid item magic" + Where(pos));
            }
            pos += sizeof(ItemHeader);
            if (item.size > chunk_end - pos) {
                throw DecodeError("truncated item data" + Where(pos));
            }

            // 私钥不对时，AES 解密几乎一定因为填充错误返回空；
            // 极少数情况下填充碰巧合法，得到的也是乱码，zstd 解压或 protobuf 解析会失败。
            // 任何一步失败都报错，不输出乱码。
            const std::string compressed = aes.Decrypt(data.data() + pos, item.size);
            if (compressed.empty()) {
                throw DecodeError("decrypt failed, wrong private key?" + Where(pos));
            }
            const std::string plain = zstd.Decompress(compressed.data(), compressed.size());
            EffectiveMsg msg;
            if (plain.empty() || !msg.ParseFromString(plain)) {
                throw DecodeError("decompress or parse failed, wrong private key or corrupted data" + Where(pos));
            }
            pos += item.size;

            line.clear();
            formatter.Format(msg, &line);
            line.push_back('\n');
            out.write(line.data(), static_cast<std::streamsize>(line.size()));
            ++stats.item_count;
        }
    }
    return stats;
}

DecodeStats DecodeFile(const std::filesystem::path& log_file,
                       const std::string& server_pri_key_hex,
                       const DecodeFormatter& formatter,
                       std::ostream& out) {
    std::ifstream ifs(log_file, std::ios::binary);
    if (!ifs) {
        throw DecodeError("open " + log_file.string() + " failed");
    }
    const std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return DecodeData(data, server_pri_key_hex, formatter, out);
}

}  // namespace decode
}  // namespace logger
