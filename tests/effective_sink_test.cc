// 文件作用：测试 M11 的 EffectiveSink。
// 测试里按文件格式自己解析日志文件，必要时用服务端私钥解密、解压、反序列化，检查内容。
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "effective_msg.pb.h"
#include "logger/compress/zstd_compress.h"
#include "logger/crypt/aes_crypt.h"
#include "logger/crypt/crypt.h"
#include "logger/sinks/effective_sink.h"

namespace {

namespace stdfs = std::filesystem;
using logger::EffectiveSink;
using logger::LogLevel;
using logger::LogMsg;
using logger::SourceLocation;
using logger::detail::ChunkHeader;
using logger::detail::ItemHeader;

struct ScanResult {
    bool ok = true;  // 所有 magic 正确、长度不越界、能解密解析
    size_t file_count = 0;
    size_t chunk_count = 0;
    size_t item_count = 0;
    std::vector<std::string> messages;  // 传了服务端私钥时，按顺序收集每条日志的 log_info
};

std::string ReadFile(const stdfs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

// 目录下所有 .log 文件，按写入时间从旧到新排序
std::vector<stdfs::path> ListLogFiles(const stdfs::path& dir) {
    std::vector<stdfs::path> files;
    for (const auto& entry : stdfs::directory_iterator(dir)) {
        if (entry.path().extension() == ".log") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end(), [](const stdfs::path& lhs, const stdfs::path& rhs) {
        return stdfs::last_write_time(lhs) < stdfs::last_write_time(rhs);
    });
    return files;
}

size_t TotalLogSize(const stdfs::path& dir) {
    size_t total = 0;
    for (const auto& file : ListLogFiles(dir)) {
        total += static_cast<size_t>(stdfs::file_size(file));
    }
    return total;
}

// 按 [ChunkHeader][ItemHeader][密文]... 的格式遍历一个文件
void ScanFile(const stdfs::path& file, const std::string& server_pri, ScanResult* result) {
    const std::string data = ReadFile(file);
    ++result->file_count;
    size_t pos = 0;
    while (pos < data.size()) {
        ChunkHeader chunk;
        if (data.size() - pos < sizeof(chunk)) {
            result->ok = false;
            return;
        }
        std::memcpy(&chunk, data.data() + pos, sizeof(chunk));
        pos += sizeof(chunk);
        if (chunk.magic != ChunkHeader::kMagic || chunk.size > data.size() - pos) {
            result->ok = false;
            return;
        }
        ++result->chunk_count;

        // 每个 chunk 用自己的客户端公钥算共享密钥，并且是一个独立的 zstd 流
        std::unique_ptr<logger::crypt::AESCrypt> aes;
        logger::compress::ZstdCompress zstd;
        if (!server_pri.empty()) {
            const std::string client_pub(chunk.pub_key, logger::crypt::kPublicKeyLength);
            aes = std::make_unique<logger::crypt::AESCrypt>(logger::crypt::GenECDHSharedSecret(server_pri, client_pub));
        }

        const size_t chunk_end = pos + chunk.size;
        while (pos < chunk_end) {
            ItemHeader item;
            if (chunk_end - pos < sizeof(item)) {
                result->ok = false;
                return;
            }
            std::memcpy(&item, data.data() + pos, sizeof(item));
            pos += sizeof(item);
            if (item.magic != ItemHeader::kMagic || item.size > chunk_end - pos) {
                result->ok = false;
                return;
            }
            ++result->item_count;

            if (aes) {
                const std::string compressed = aes->Decrypt(data.data() + pos, item.size);
                const std::string plain = zstd.Decompress(compressed.data(), compressed.size());
                logger::EffectiveMsg msg;
                if (compressed.empty() || !msg.ParseFromString(plain)) {
                    result->ok = false;
                    return;
                }
                result->messages.push_back(msg.log_info());
            }
            pos += item.size;
        }
    }
}

ScanResult ScanDir(const stdfs::path& dir, const std::string& server_pri = "") {
    ScanResult result;
    for (const auto& file : ListLogFiles(dir)) {
        ScanFile(file, server_pri, &result);
    }
    return result;
}

std::string RandomString(std::mt19937* rng, size_t length) {
    std::uniform_int_distribution<int> dist('a', 'z');
    std::string str(length, '\0');
    for (char& c : str) {
        c = static_cast<char>(dist(*rng));
    }
    return str;
}

void LogText(EffectiveSink* sink, const std::string& text) {
    sink->Log(LogMsg(SourceLocation("file.cc", 42, "Func"), LogLevel::kInfo, text));
}

class EffectiveSinkTest : public testing::Test {
protected:
    void SetUp() override {
        const std::string name = testing::UnitTest::GetInstance()->current_test_info()->name();
        dir_ = stdfs::temp_directory_path() / "logger_effective_sink_test" / name;
        stdfs::remove_all(dir_);
        stdfs::create_directories(dir_);
        server_ = logger::crypt::GenECDHKey();
    }

    void TearDown() override {
        std::error_code ec;
        stdfs::remove_all(dir_, ec);
    }

    EffectiveSink::Conf MakeConf(const stdfs::path& dir) const {
        EffectiveSink::Conf conf;
        conf.dir = dir;
        conf.prefix = "test";
        conf.pub_key = logger::crypt::BinaryKeyToHex(server_.public_key);
        return conf;
    }

    stdfs::path dir_;
    logger::crypt::KeyPair server_;
};

TEST_F(EffectiveSinkTest, FlushWritesFileStartingWithChunkMagic) {
    EffectiveSink sink(MakeConf(dir_));
    for (int i = 0; i < 1000; ++i) {
        LogText(&sink, "message " + std::to_string(i));
    }
    sink.Flush();

    const auto files = ListLogFiles(dir_);
    ASSERT_FALSE(files.empty());
    const std::string data = ReadFile(files.front());
    ASSERT_GE(data.size(), sizeof(uint64_t));
    uint64_t magic = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    EXPECT_EQ(magic, ChunkHeader::kMagic);
}

TEST_F(EffectiveSinkTest, AllMagicsValidAndItemCountMatches) {
    EffectiveSink sink(MakeConf(dir_));
    for (int i = 0; i < 1000; ++i) {
        LogText(&sink, "message " + std::to_string(i));
    }
    sink.Flush();

    const ScanResult result = ScanDir(dir_);
    EXPECT_TRUE(result.ok);
    EXPECT_GE(result.chunk_count, 1u);
    EXPECT_EQ(result.item_count, 1000u);
}

TEST_F(EffectiveSinkTest, DecodedMessagesMatchInOrder) {
    std::vector<std::string> expected;
    {
        EffectiveSink sink(MakeConf(dir_));
        for (int i = 0; i < 1000; ++i) {
            expected.push_back("message " + std::to_string(i) + " 中文");
            LogText(&sink, expected.back());
        }
    }  // 析构时自动 Flush

    const ScanResult result = ScanDir(dir_, server_.private_key);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.messages, expected);
}

TEST_F(EffectiveSinkTest, SplitsFilesBySingleSizeWithoutLosingItems) {
    auto conf = MakeConf(dir_);
    conf.single_size = logger::megabytes{1};
    std::mt19937 rng(1);
    constexpr int kCount = 3000;  // 每条 2000 个随机字母，压缩加密后总共约 3.5MB
    {
        EffectiveSink sink(conf);
        for (int i = 0; i < kCount; ++i) {
            LogText(&sink, RandomString(&rng, 2000));
        }
    }

    const ScanResult result = ScanDir(dir_);
    EXPECT_TRUE(result.ok);
    EXPECT_GE(result.file_count, 2u);
    EXPECT_GT(result.chunk_count, result.file_count);  // 中途发生过多次交换
    EXPECT_EQ(result.item_count, static_cast<size_t>(kCount));
}

TEST_F(EffectiveSinkTest, TotalSizeLimitRemovesOldestFiles) {
    auto conf = MakeConf(dir_);
    conf.single_size = logger::megabytes{1};
    conf.total_size = logger::megabytes{2};
    std::mt19937 rng(2);
    {
        EffectiveSink sink(conf);
        for (int i = 0; i < 6000; ++i) {  // 压缩加密后约 7MB
            LogText(&sink, RandomString(&rng, 2000));
        }
    }

    // 旧文件被删掉了：剩下的总量不超过 total_size + 一个正在写的文件（single_size + 一个 chunk 的余量）
    const size_t limit = (2 + 1 + 1) * 1024 * 1024;
    EXPECT_LE(TotalLogSize(dir_), limit);
    const ScanResult result = ScanDir(dir_);
    EXPECT_TRUE(result.ok);
    EXPECT_LT(result.item_count, 6000u);
}

TEST_F(EffectiveSinkTest, LeftoverCacheIsWrittenOnNextStartup) {
    // 模拟崩溃：sink A 写入后还没 Flush，此时把它的 master_cache 复制到另一个目录，
    // 相当于进程在这一刻崩溃、mmap 里留下了数据。然后在那个目录上启动新的 sink B。
    const stdfs::path crash_dir = dir_ / "crash";
    stdfs::create_directories(crash_dir);
    std::vector<std::string> expected;
    {
        EffectiveSink sink_a(MakeConf(dir_));
        for (int i = 0; i < 100; ++i) {
            expected.push_back("before crash " + std::to_string(i));
            LogText(&sink_a, expected.back());
        }
        ASSERT_TRUE(ListLogFiles(dir_).empty());  // 数据量很小，还没交换，全部在 master 里
        stdfs::copy_file(dir_ / "master_cache", crash_dir / "master_cache");
    }

    ASSERT_TRUE(ListLogFiles(crash_dir).empty());
    {
        EffectiveSink sink_b(MakeConf(crash_dir));  // 构造时补写残留数据
        EXPECT_FALSE(ListLogFiles(crash_dir).empty());
    }

    // 残留数据是 A 的客户端密钥加密的，必须仍能用服务端私钥解开
    const ScanResult result = ScanDir(crash_dir, server_.private_key);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.messages, expected);
}

TEST_F(EffectiveSinkTest, ConcurrentLogFromMultipleThreads) {
    static constexpr int kThreads = 4;
    static constexpr int kPerThread = 2000;
    {
        EffectiveSink sink(MakeConf(dir_));
        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&sink, t] {
                std::mt19937 rng(100 + t);
                for (int i = 0; i < kPerThread; ++i) {
                    LogText(&sink, RandomString(&rng, 300));  // 总量约 2MB，期间会多次交换
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
    }

    const ScanResult result = ScanDir(dir_, server_.private_key);
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.chunk_count, 1u);
    EXPECT_EQ(result.messages.size(), static_cast<size_t>(kThreads * kPerThread));
}

TEST_F(EffectiveSinkTest, InvalidServerKeyThrows) {
    auto conf = MakeConf(dir_);
    conf.pub_key = "1234";
    EXPECT_THROW(EffectiveSink sink(conf), std::invalid_argument);
}

}  // namespace