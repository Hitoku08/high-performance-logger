// 文件作用：EffectiveSink 示例。把日志压缩、加密后写到当前目录下的 logger_example_out，
// 之后可以用 M12 的 decoder 加上下面的服务端私钥解码。
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#include "logger/log_handle.h"
#include "logger/sinks/effective_sink.h"

namespace {

// 服务端密钥对（与参考实现相同）。私钥只用来解码，真实项目里不能放进客户端。
constexpr const char* kServerPubKey =
    "04827405069030E26A211C973C8710E6FBE79B5CAA364AC111FB171311902277537F8852EADD17EB339EB7CD0BA2490A58CDED2C702DFC1E"
    "FC7EDB544B869F039C";
constexpr const char* kServerPriKey = "FAA5BBE9017C96BF641D19D0144661885E831B5DDF52539EF1AB4790C05E665E";

std::string RandomString(size_t length) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist('a', 'z');
    std::string str(length, '\0');
    for (char& c : str) {
        c = static_cast<char>(dist(rng));
    }
    return str;
}

}  // namespace

int main() {
    logger::EffectiveSink::Conf conf;
    conf.dir = "logger_example_put";
    conf.prefix = "loggerdemo";
    conf.pub_key = kServerPubKey;

    constexpr int kCount = 100000;
    const std::string message = RandomString(200);

    std::cout << "EffectiveSink example start, dir=" << conf.dir.string() << std::endl;
    {
        auto sink = std::make_shared<logger::EffectiveSink>(conf);
        logger::LogHandle handle(sink);

        const auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < kCount; i++) {
            handle.Log(logger::LogLevel::kInfo, logger::SourceLocation(__FILE__, __LINE__, __func__), message);
        }
        sink->Flush();
        const auto cost =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
        std::cout << kCount << " logs in " << cost.count() << " ms" << std::endl;
    }

    std::cout << "decode with server private key: " << kServerPriKey << std::endl;
    return 0;
}