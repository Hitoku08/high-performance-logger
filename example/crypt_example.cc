// 文件作用：演示日志库里加密的完整流程。
//
//   1. 服务端生成一对密钥，公钥交给客户端（写进配置），私钥自己保存。
//   2. 客户端生成一对临时密钥，用"客户端私钥 + 服务端公钥"算出共享密钥，用 AES 加密日志。
//      客户端公钥随日志一起保存。
//   3. 服务端用"服务端私钥 + 客户端公钥"算出同一个共享密钥，解密日志。
#include <iostream>
#include <string>

#include "logger/crypt/aes_crypt.h"
#include "logger/crypt/crypt.h"

using namespace logger::crypt;

int main() {
    // 1. 服务端
    KeyPair server = GenECDHKey();
    std::cout << "服务端私钥：" << BinaryKeyToHex(server.private_key) << "\n";
    std::cout << "服务端公钥：" << BinaryKeyToHex(server.public_key) << "\n";

    // 2. 客户端加密
    KeyPair client = GenECDHKey();
    std::string client_secret = GenECDHSharedSecret(client.private_key, server.public_key);
    std::cout << "客户端公钥：" << BinaryKeyToHex(client.public_key) << "\n";
    std::cout << "客户端算出的共享密钥：" << BinaryKeyToHex(client_secret) << "\n";

    AESCrypt client_aes(client_secret);
    std::string plain = "[2024-01-01 12:00:00] [I] [main.cc:42] hello logger";
    std::string cipher;
    client_aes.Encrypt(plain.data(), plain.size(), cipher);
    std::cout << "密文（" << cipher.size() << " 字节）：" << BinaryKeyToHex(cipher) << "\n\n";

    // 服务端解密
    std::string server_secret = GenECDHSharedSecret(server.private_key, client.public_key);
    std::cout << "服务端算出的共享密钥：" << BinaryKeyToHex(server_secret) << "\n";

    AESCrypt server_aes(server_secret);
    std::string decrypted = server_aes.Decrypt(cipher.data(), cipher.size());
    std::cout << "解密结果：" << decrypted << "\n";

    if (decrypted != plain) {
        std::cout << "解密失败\n";
        return 1;
    }
    std::cout << "解密成功\n";
    return 0;
}
