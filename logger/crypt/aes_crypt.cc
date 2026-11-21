#include "crypt/aes_crypt.h"

#include <utility>

#include "cryptopp/aes.h"
#include "cryptopp/filters.h"
#include "cryptopp/modes.h"

namespace logger {
namespace crypt {

using CryptoPP::byte;

// CryptoPP::AES::BLOCKSIZE：编译器长两个16，这是AES的分组大小
static const byte kIV[CryptoPP::AES::BLOCKSIZE] = {'d', 'a', 'd', '0', 'c', '0', '0', '1',
                                                   '2', '3', '4', '0', '0', '8', '0', 'a'};

AESCrypt::AESCrypt(std::string key) : key_(std::move(key)) {}

// 用 AES-CBC 模式加密一段任意二进制数据，并把密文写进 output
void AESCrypt::Encrypt(const void* input, size_t input_size, std::string& output) {
    // 这个很重要，把密文先清空
    output.clear();
    if (input == nullptr || input_size == 0) {
        return;
    }
    try {
        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption aes;
        aes.SetKeyWithIV(reinterpret_cast<const byte*>(key_.data()), key_.size(), kIV);

        // 第三个参数 true：立刻把全部输入送进管道处理完。
        CryptoPP::ArraySource(static_cast<const byte*>(input), input_size, true,
                              new CryptoPP::StreamTransformationFilter(aes, new CryptoPP::StringSink(output)));
    } catch (const CryptoPP::Exception&) {
        output.clear();  // 可能已经写了一半，清掉
    }
}

std::string AESCrypt::Decrypt(const void* input, size_t input_size) {
    std::string output;
    if (input == nullptr || input_size == 0) {
        return output;
    }
    try {
        CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption aes;
        aes.SetKeyWithIV(reinterpret_cast<const byte*>(key_.data()), key_.size(), kIV);

        CryptoPP::ArraySource(static_cast<const byte*>(input), input_size, true,
                              new CryptoPP::StreamTransformationFilter(aes, new CryptoPP::StringSink(output)));
    } catch (const CryptoPP::Exception&) {
        // 最常见的情况：密钥不对，解出来的填充不合法，cryptopp 抛 InvalidCiphertext。
        output.clear();
    }
    return output;
}
}  // namespace crypt

}  // namespace logger