// 文件作用：decoder 命令行工具。
//
// 用法：decoder <log_file> <server_pri_key_hex> <output_file> [pattern]
//   pattern 可省略，默认 "[%l][%D:%S][%p:%t][%F:%f:%#]%v"，占位符见 decode_formatter.h
//
// 解码失败时删除输出文件并返回 1，不留下只解了一半的结果。
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "decoder.h"

int main(int argc, char* argv[]) {
    if (argc != 4 && argc != 5) {
        std::cerr << "Usage: decoder <log_file> <server_pri_key_hex> <output_file> [pattern]\n"
                  << "  default pattern: " << logger::decode::DecodeFormatter::kDefaultPattern << std::endl;
        return 1;
    }
    const std::filesystem::path log_file = argv[1];
    const std::string server_pri_key = argv[2];
    const std::filesystem::path output_file = argv[3];
    const logger::decode::DecodeFormatter formatter(argc == 5 ? argv[4]
                                                              : logger::decode::DecodeFormatter::kDefaultPattern);

    std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Decode failed: open " << output_file.string() << " failed" << std::endl;
        return 1;
    }

    try {
        const auto stats = logger::decode::DecodeFile(log_file, server_pri_key, formatter, out);
        out.close();
        if (!out) {
            throw logger::decode::DecodeError("write " + output_file.string() + " failed");
        }
        std::cout << "Decoded " << stats.item_count << " logs in " << stats.chunk_count << " chunks -> "
                  << output_file.string() << std::endl;
    } catch (const std::exception& e) {
        out.close();
        std::error_code ec;
        std::filesystem::remove(output_file, ec);
        std::cerr << "Decode failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
