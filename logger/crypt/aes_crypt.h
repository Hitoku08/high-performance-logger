// 文件作用：用 AES 实现 Crypt 接口，对每条日志做对称加密。
//
// 模式：AES-CBC，带 PKCS#7 填充（密文长度是 16 的倍数，比明文多 1~16 字节）。
// 密钥：ECDH 算出的 32 字节共享密钥，也就是 AES-256。
#pragma once

#include <string>

#include "crypt/crypt.h"

namespace logger {
namespace crypt {

class AESCrypt : public Crypt {
public:
    // key 的长度必须是 16、24 或 32 字节，否则加密和解密都会失败（返回空）。
    explicit AESCrypt(std::string key);

    void Encrypt(const void* input, size_t input_size, std::string& output) override;
    std::string Decrypt(const void* input, size_t input_size) override;

private:
    std::string key_;
};

}  // namespace crypt
}  // namespace logger
