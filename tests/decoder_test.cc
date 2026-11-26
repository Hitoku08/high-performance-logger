// 文件作用：测试 M12 的解码器。
//   1. 端到端：EffectiveSink 写入 → decoder 解码 → 和原文逐条对比
//   2. 错误处理：错误私钥、无效私钥、损坏或截断的文件都要报错，而不是输出乱码
//   3. DecodeFormatter 的各个占位符
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "decode/decoder.h"
#include "effective_msg.pb.h"
#include "logger/crypt/crypt.h"
#include "logger/sinks/effective_sink.h"
#include "logger/utils/sys_util.h"

namespace {

namespace stdfs = std::filesystem;
using logger::EffectiveMsg;
using logger::EffectiveSink;
using logger::LogLevel;
using logger::LogMsg;
using logger::SourceLocation;
using logger::decode::DecodeError;
using logger::decode::DecodeFile;
using logger::decode::DecodeFormatter;

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string ReadFile(const stdfs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

void WriteFile(const stdfs::path& path, const std::string& data) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
}

class DecoderTest : public testing::Test {
protected:
    void SetUp() override {
        const std::string name = testing::UnitTest::GetInstance()->current_test_info()->name();
        dir_ = stdfs::temp_directory_path() / "logger_decoder_test" / name;
        stdfs::remove_all(dir_);
        stdfs::create_directories(dir_);
        server_ = logger::crypt::GenECDHKey();
        server_pri_hex_ = logger::crypt::BinaryKeyToHex(server_.private_key);
    }

    void TearDown() override {
        std::error_code ec;
        stdfs::remove_all(dir_, ec);
    }

    EffectiveSink::Conf MakeConf() const {
        EffectiveSink::Conf conf;
        conf.dir = dir_;
        conf.prefix = "test";
        conf.pub_key = logger::crypt::BinaryKeyToHex(server_.public_key);
        return conf;
    }

    // 写入 messages 并析构 sink（析构时 Flush），返回生成的唯一日志文件
    stdfs::path WriteLogs(const std::vector<std::string>& messages) {
        {
            EffectiveSink sink(MakeConf());
            for (const auto& message : messages) {
                sink.Log(LogMsg(SourceLocation("app.cc", 7, "Run"), LogLevel::kInfo, message));
            }
        }
        std::vector<stdfs::path> files;
        for (const auto& entry : stdfs::directory_iterator(dir_)) {
            if (entry.path().extension() == ".log") {
                files.push_back(entry.path());
            }
        }
        EXPECT_EQ(files.size(), 1u);
        return files.empty() ? stdfs::path() : files.front();
    }

    std::string Decode(const stdfs::path& file, const std::string& pri_hex, const std::string& pattern) {
        std::ostringstream out;
        DecodeFile(file, pri_hex, DecodeFormatter(pattern), out);
        return out.str();
    }

    stdfs::path dir_;
    logger::crypt::KeyPair server_;
    std::string server_pri_hex_;
};

// ---------- 端到端 ----------

TEST_F(DecoderTest, DecodesAllMessagesInOrder) {
    std::vector<std::string> expected;
    for (int i = 0; i < 500; ++i) {
        expected.push_back("line " + std::to_string(i) + " 中文日志");
    }
    const stdfs::path file = WriteLogs(expected);

    std::ostringstream out;
    const auto stats = DecodeFile(file, server_pri_hex_, DecodeFormatter("%v"), out);
    EXPECT_EQ(stats.item_count, expected.size());
    EXPECT_EQ(SplitLines(out.str()), expected);
}

TEST_F(DecoderTest, DefaultPatternContainsAllFields) {
    const stdfs::path file = WriteLogs({"hello"});
    const auto lines = SplitLines(Decode(file, server_pri_hex_, DecodeFormatter::kDefaultPattern));
    ASSERT_EQ(lines.size(), 1u);
    const std::string expected_ids = "[" + std::to_string(logger::GetProcessId()) + ":";
    EXPECT_EQ(lines[0].rfind("[I][", 0), 0u) << lines[0];
    EXPECT_NE(lines[0].find(expected_ids), std::string::npos) << lines[0];
    EXPECT_NE(lines[0].find("[app.cc:Run:7]hello"), std::string::npos) << lines[0];
}

TEST_F(DecoderTest, DecodesMultipleChunks) {
    std::vector<std::string> expected;
    {
        EffectiveSink sink(MakeConf());
        for (int round = 0; round < 3; ++round) {
            for (int i = 0; i < 100; ++i) {
                expected.push_back("round " + std::to_string(round) + " item " + std::to_string(i));
                sink.Log(LogMsg(SourceLocation("app.cc", 7, "Run"), LogLevel::kInfo, expected.back()));
            }
            sink.Flush();  // 每次 Flush 写出一个 chunk，每个 chunk 是一个独立的 zstd 流
        }
    }
    stdfs::path file;
    for (const auto& entry : stdfs::directory_iterator(dir_)) {
        if (entry.path().extension() == ".log") {
            file = entry.path();
        }
    }
    ASSERT_FALSE(file.empty());

    std::ostringstream out;
    const auto stats = DecodeFile(file, server_pri_hex_, DecodeFormatter("%v"), out);
    EXPECT_EQ(stats.chunk_count, 3u);
    EXPECT_EQ(SplitLines(out.str()), expected);
}

// ---------- 错误处理 ----------

TEST_F(DecoderTest, WrongPrivateKeyThrows) {
    const stdfs::path file = WriteLogs({"secret"});
    const auto other = logger::crypt::GenECDHKey();
    std::ostringstream out;
    EXPECT_THROW(DecodeFile(file, logger::crypt::BinaryKeyToHex(other.private_key), DecodeFormatter(), out),
                 DecodeError);
    EXPECT_TRUE(out.str().empty());  // 一条乱码都没有输出
}

TEST_F(DecoderTest, InvalidPrivateKeyHexThrows) {
    const stdfs::path file = WriteLogs({"secret"});
    std::ostringstream out;
    EXPECT_THROW(DecodeFile(file, "1234", DecodeFormatter(), out), DecodeError);
}

TEST_F(DecoderTest, NotALogFileThrows) {
    const stdfs::path file = dir_ / "garbage.log";
    WriteFile(file, std::string(1000, 'x'));
    std::ostringstream out;
    EXPECT_THROW(DecodeFile(file, server_pri_hex_, DecodeFormatter(), out), DecodeError);
}

TEST_F(DecoderTest, TruncatedFileThrows) {
    std::vector<std::string> messages(100, "some log message");
    const stdfs::path file = WriteLogs(messages);
    const std::string data = ReadFile(file);
    const stdfs::path truncated = dir_ / "truncated.bin";
    WriteFile(truncated, data.substr(0, data.size() / 2));

    std::ostringstream out;
    EXPECT_THROW(DecodeFile(truncated, server_pri_hex_, DecodeFormatter(), out), DecodeError);
}

TEST_F(DecoderTest, MissingFileThrows) {
    std::ostringstream out;
    EXPECT_THROW(DecodeFile(dir_ / "not_exist.log", server_pri_hex_, DecodeFormatter(), out), DecodeError);
}

TEST_F(DecoderTest, EmptyFileDecodesNothing) {
    const stdfs::path file = dir_ / "empty.log";
    WriteFile(file, "");
    std::ostringstream out;
    const auto stats = DecodeFile(file, server_pri_hex_, DecodeFormatter(), out);
    EXPECT_EQ(stats.item_count, 0u);
    EXPECT_TRUE(out.str().empty());
}

// ---------- DecodeFormatter ----------

EffectiveMsg MakeMsg() {
    // 用 mktime 按本地时区构造时间戳，%D 也按本地时区输出，测试结果与所在时区无关
    std::tm tm{};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 0;
    tm.tm_mday = 2;
    tm.tm_hour = 3;
    tm.tm_min = 4;
    tm.tm_sec = 5;
    tm.tm_isdst = -1;
    const int64_t seconds = static_cast<int64_t>(std::mktime(&tm));

    EffectiveMsg msg;
    msg.set_level(static_cast<int32_t>(LogLevel::kWarn));
    msg.set_timestamp(seconds * 1000 + 7);
    msg.set_pid(1234);
    msg.set_tid(5678);
    msg.set_line(42);
    msg.set_file_name("file.cc");
    msg.set_func_name("Func");
    msg.set_log_info("hello");
    return msg;
}

std::string FormatWith(const std::string& pattern, const EffectiveMsg& msg = MakeMsg()) {
    std::string dest;
    DecodeFormatter(pattern).Format(msg, &dest);
    return dest;
}

TEST(DecodeFormatter, EachPlaceholder) {
    const EffectiveMsg msg = MakeMsg();
    EXPECT_EQ(FormatWith("%l"), "W");
    EXPECT_EQ(FormatWith("%D"), "2024-01-02 03:04:05");
    EXPECT_EQ(FormatWith("%S"), "007");
    EXPECT_EQ(FormatWith("%M"), std::to_string(msg.timestamp()));
    EXPECT_EQ(FormatWith("%p"), "1234");
    EXPECT_EQ(FormatWith("%t"), "5678");
    EXPECT_EQ(FormatWith("%#"), "42");
    EXPECT_EQ(FormatWith("%F"), "file.cc");
    EXPECT_EQ(FormatWith("%f"), "Func");
    EXPECT_EQ(FormatWith("%v"), "hello");
}

TEST(DecodeFormatter, DefaultPattern) {
    EXPECT_EQ(FormatWith(DecodeFormatter::kDefaultPattern),
              "[W][2024-01-02 03:04:05:007][1234:5678][file.cc:Func:42]hello");
}

TEST(DecodeFormatter, AllLevels) {
    EffectiveMsg msg = MakeMsg();
    const std::vector<std::pair<LogLevel, std::string>> cases = {
        {LogLevel::kTrace, "T"}, {LogLevel::kDebug, "D"}, {LogLevel::kInfo, "I"},
        {LogLevel::kWarn, "W"},  {LogLevel::kError, "E"}, {LogLevel::kFatal, "F"},
    };
    for (const auto& [level, letter] : cases) {
        msg.set_level(static_cast<int32_t>(level));
        EXPECT_EQ(FormatWith("%l", msg), letter);
    }
}

TEST(DecodeFormatter, LiteralsAndSpecialCases) {
    EXPECT_EQ(FormatWith("no placeholder"), "no placeholder");
    EXPECT_EQ(FormatWith("100%%"), "100%");
    EXPECT_EQ(FormatWith("%x"), "%x");      // 不认识的占位符原样输出
    EXPECT_EQ(FormatWith("end%"), "end%");  // 末尾单独的 %
    EXPECT_EQ(FormatWith(""), "");
}

TEST(DecodeFormatter, FormatAppendsToDest) {
    std::string dest = "prefix:";
    DecodeFormatter("%v").Format(MakeMsg(), &dest);
    EXPECT_EQ(dest, "prefix:hello");
}

}  // namespace
