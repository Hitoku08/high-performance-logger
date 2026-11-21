// 文件作用：加密模块的公共部分。
//
// 1. ECDH 密钥交换：客户端和服务端各有一对密钥（私钥 + 公钥），
//    用"自己的私钥 + 对方的公钥"能算出同一个共享密钥，而共享密钥本身从不在网络或文件里出现。
// 2. 密钥的二进制 <-> 十六进制字符串转换，方便把密钥写进配置或命令行。
// 3. Crypt 接口：对称加密（AESCrypt 实现它）。
//
// 日志库里的用法：
//   - 服务端事先生成一对密钥，把公钥写进 EffectiveSink 的配置，私钥自己保存。
//   - EffectiveSink 启动时生成一对临时密钥，用"临时私钥 + 服务端公钥"算出共享密钥，用它做 AES 加密；
//     临时公钥写进日志文件的 ChunkHeader。
//   - 解码器用"服务端私钥 + 文件里的临时公钥"算出同一个共享密钥，再解密。
#pragma once

#include <cstddef>
#include <string>

namespace logger {
namespace crypt {

struct KeyPair {
    std::string private_key;  // 私钥，32字节
    std::string public_key;   // 公钥，65字节
};

constexpr size_t kPrivateKeyLength = 32;
constexpr size_t kPublicKeyLength = 65;

KeyPair GenECDHKey();

std::string GenECDHSharedSecret(const std::string& private_key, const std::string& other_public_key);

std::string BinaryKeyToHex(const std::string& binary_key);

std::string HexKeyToBinary(const std::string& hex_key);

class Crypt {
public:
    virtual ~Crypt() = default;

    virtual void Encrypt(const void* input, size_t input_size, std::string& output) = 0;

    virtual std::string Decrypt(const void* input, size_t input_size) = 0;
};
}  // namespace crypt

}  // namespace logger