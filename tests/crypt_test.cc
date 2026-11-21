#include <string>

#include <gtest/gtest.h>

#include "logger/crypt/aes_crypt.h"
#include "logger/crypt/crypt.h"

using namespace logger::crypt;

// 参考实现 example/logger_example.cc 里的固定服务端密钥对。
static const char* kServerPrivateHex = "FAA5BBE9017C96BF641D19D0144661885E831B5DDF52539EF1AB4790C05E665E";
static const char* kServerPublicHex =
    "04827405069030E26A211C973C8710E6FBE79B5CAA364AC111FB171311902277537F8852EADD17EB339EB7CD0BA2490A58CDED2C702DFC1E"
    "FC7EDB544B869F039C";

// ---------- ECDH ----------

// 密钥长度：私钥 32 字节，公钥 65 字节且以 0x04 开头（未压缩格式）。
TEST(CryptTest, KeyLength) {
    KeyPair key = GenECDHKey();
    EXPECT_EQ(key.private_key.size(), kPrivateKeyLength);
    ASSERT_EQ(key.public_key.size(), kPublicKeyLength);
    EXPECT_EQ(static_cast<unsigned char>(key.public_key[0]), 0x04);
}

// 每次生成的密钥都不一样
TEST(CryptTest, KeysAreRandom) {
    KeyPair a = GenECDHKey();
    KeyPair b = GenECDHKey();
    EXPECT_NE(a.private_key, b.private_key);
    EXPECT_NE(a.public_key, b.public_key);
}

// 双方计算出的共享密钥相同
TEST(CryptTest, SharedSecretMatches) {
    KeyPair a = GenECDHKey();
    KeyPair b = GenECDHKey();
    std::string ab = GenECDHSharedSecret(a.private_key, b.public_key);
    std::string ba = GenECDHSharedSecret(b.private_key, a.public_key);
    EXPECT_EQ(ab, ba);
    EXPECT_EQ(ab.size(), 32u);
}

// 换一个对方，共享密钥就不同
TEST(CryptTest, DifferentPeerDifferentSecret) {
    KeyPair a = GenECDHKey();
    KeyPair b = GenECDHKey();
    KeyPair c = GenECDHKey();
    EXPECT_NE(GenECDHSharedSecret(a.private_key, b.public_key), GenECDHSharedSecret(a.private_key, c.public_key));
}

TEST(CryptTest, FixedServerKey) {
    std::string server_private = HexKeyToBinary(kServerPrivateHex);
    std::string server_public = HexKeyToBinary(kServerPublicHex);
    ASSERT_EQ(server_private.size(), kPrivateKeyLength);
    ASSERT_EQ(server_public.size(), kPublicKeyLength);

    KeyPair client = GenECDHKey();
    std::string client_side = GenECDHSharedSecret(client.private_key, server_public);
    std::string server_side = GenECDHSharedSecret(server_private, client.public_key);
    ASSERT_FALSE(client_side.empty());
    EXPECT_EQ(client_side, server_side);
    EXPECT_EQ(GenECDHSharedSecret(server_private, client.public_key), server_side);  // 再算一次，结果一样
}

TEST(CryptTest, InvalidKeyReturnsEmpty) {
    KeyPair a = GenECDHKey();
    KeyPair b = GenECDHKey();
    EXPECT_EQ(GenECDHSharedSecret("", b.public_key), "");
    EXPECT_EQ(GenECDHSharedSecret(a.private_key, ""), "");
    EXPECT_EQ(GenECDHSharedSecret(a.private_key, b.public_key.substr(0, 10)), "");
}

// ---------- 十六进制 ----------
TEST(CryptTest, HexRoundTrip) {
    KeyPair key = GenECDHKey();
    std::string hex = BinaryKeyToHex(key.public_key);
    EXPECT_EQ(hex.size(), key.public_key.size() * 2);
    EXPECT_EQ(HexKeyToBinary(hex), key.public_key);
}

TEST(CryptTest, HexKnownValue) {
    EXPECT_EQ(BinaryKeyToHex(std::string("\x01\xab\xff", 3)), "01ABFF");
    EXPECT_EQ(HexKeyToBinary("01abff"), std::string("\x01\xab\xff", 3));
    EXPECT_EQ(BinaryKeyToHex(""), "");
}

// ---------- AES ----------

TEST(AESCryptTest, RoundTrip) {
    KeyPair a = GenECDHKey();
    KeyPair b = GenECDHKey();
    AESCrypt aes(GenECDHSharedSecret(a.private_key, b.public_key));

    std::string plain = "[I] [main.cc:42] hello logger 你好";
    std::string cipher;
    aes.Encrypt(plain.data(), plain.size(), cipher);
    ASSERT_FALSE(cipher.empty());
    EXPECT_NE(cipher, plain);
    EXPECT_EQ(cipher.size() % 16u, 0u);
    EXPECT_GT(cipher.size(), plain.size());
    EXPECT_EQ(aes.Decrypt(cipher.data(), cipher.size()), plain);
}

// 服务端用自己算出的共享密钥，能解开客户端加密的数据（模拟日志库的真实用法）。
TEST(AESCryptTest, ClientEncryptServerDecrypt) {
    KeyPair server = GenECDHKey();
    KeyPair client = GenECDHKey();
    AESCrypt client_aes(GenECDHSharedSecret(client.private_key, server.public_key));
    AESCrypt server_aes(GenECDHSharedSecret(server.private_key, client.public_key));

    std::string plain(5000, 'x');
    std::string cipher;
    client_aes.Encrypt(plain.data(), plain.size(), cipher);
    EXPECT_EQ(server_aes.Decrypt(cipher.data(), cipher.size()), plain);
}

// 用错误的密钥解密，得不到原文
TEST(AESCryptTest, WrongKey) {
    AESCrypt right(std::string(32, 'a'));
    AESCrypt wrong(std::string(32, 'b'));

    std::string plain = "secret message";
    std::string cipher;
    right.Encrypt(plain.data(), plain.size(), cipher);
    EXPECT_NE(wrong.Decrypt(cipher.data(), cipher.size()), plain);
}

// 同一个 output 反复使用，每次都会先清空，不会和上一次的结果拼在一起。
TEST(AESCryptTest, OutputIsCleared) {
    AESCrypt aes(std::string(32, 'k'));
    std::string cipher;
    aes.Encrypt("first", 5, cipher);
    std::string first = cipher;
    aes.Encrypt("first", 5, cipher);
    EXPECT_EQ(first, cipher);
}

// 空输入、密钥长度不对、密文被截断：都返回空，不崩溃。
TEST(AESCryptTest, InvalidInput) {
    AESCrypt aes(std::string(32, 'k'));
    std::string cipher = "not empty";
    aes.Encrypt(nullptr, 0, cipher);
    EXPECT_TRUE(cipher.empty());
    EXPECT_EQ(aes.Decrypt(nullptr, 0), "");

    AESCrypt bad_key("short");  // 这里key的长度不对，必须是16,24或者32字节
    cipher = "not empty";
    bad_key.Encrypt("hello", 5, cipher);
    EXPECT_TRUE(cipher.empty());

    aes.Encrypt("hello world", 11, cipher);
    EXPECT_EQ(aes.Decrypt(cipher.data(), cipher.size() - 1), "");  // 长度不是16的倍数
}