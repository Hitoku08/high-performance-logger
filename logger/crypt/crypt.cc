// 文件作用：实现 ECDH 密钥生成、共享密钥计算和十六进制转换（使用 cryptopp 库）。
//
// cryptopp 的写法说明：
//   - SecByteBlock 是 cryptopp 的字节数组，用完会自动清零，适合放密钥。
//   - HexEncoder / StringSink 这类对象可以串成"管道"：数据从 Put 进去，经过编码，最后落到 string 里。
//     new 出来的 StringSink 交给 HexEncoder 后由它负责释放，不需要我们 delete。
#include "crypt/crypt.h"

#include "cryptopp/eccrypto.h"
#include "cryptopp/filters.h"
#include "cryptopp/hex.h"
#include "cryptopp/oids.h"
#include "cryptopp/osrng.h"

namespace logger {
namespace crypt {

using CryptoPP::byte;  // 这里的byte就是 unsigned char

using ECDH = CryptoPP::ECDH<CryptoPP::ECP>::Domain;

// 把 const char* 转换成 const unsigned char*
static const byte* ToBytes(const std::string& s) {
    return reinterpret_cast<const byte*>(s.data());
}

static std::string ToString(const CryptoPP::SecByteBlock& block) {
    return std::string(reinterpret_cast<const char*>(block.data()), block.size());
}

KeyPair GenECDHKey() {
    CryptoPP::AutoSeededRandomPool rng;  // 随机数生成器，从操作系统获取随机种子
    ECDH dh(CryptoPP::ASN1::secp256r1());

    CryptoPP::SecByteBlock private_key(dh.PrivateKeyLength());
    CryptoPP::SecByteBlock public_key(dh.PublicKeyLength());
    dh.GenerateKeyPair(rng, private_key, public_key);

    return KeyPair{ToString(private_key), ToString(public_key)};
}

std::string GenECDHSharedSecret(const std::string& private_key, const std::string& other_public_key) {
    ECDH dh(CryptoPP::ASN1::secp256r1());

    // 长度必须对。cryptopp 的 Agree 不检查长度，传短了会越界读内存。
    if (private_key.size() != dh.PrivateKeyLength() || other_public_key.size() != dh.PublicKeyLength()) {
        return "";
    }

    CryptoPP::SecByteBlock shared(dh.AgreedValueLength());
    // 最后一个参数 true：检查对方公钥是不是曲线上合法的点，防止拿伪造的公钥来套密钥。
    if (!dh.Agree(shared, ToBytes(private_key), ToBytes(other_public_key), true)) {
        return "";
    }
    return ToString(shared);
}

std::string BinaryKeyToHex(const std::string& binary_key) {
    std::string hex;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(hex));  // 编码结果写入 hex，默认大写
    encoder.Put(ToBytes(binary_key), binary_key.size());
    encoder.MessageEnd();  // 告诉编码器数据结束了，把剩余内容全部输出
    return hex;
}

std::string HexKeyToBinary(const std::string& hex_key) {
    std::string binary;
    CryptoPP::HexDecoder decoder(new CryptoPP::StringSink(binary));
    decoder.Put(ToBytes(hex_key), hex_key.size());
    decoder.MessageEnd();
    return binary;
}
}  // namespace crypt
}  // namespace logger