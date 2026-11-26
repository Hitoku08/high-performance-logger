// 文件作用：高性能落盘 Sink。
//
// 数据流（Log 在调用线程上执行，只有写文件交给后台 runner）：
//   LogMsg → EffectiveFormatter(protobuf) → zstd 流式压缩 → AES 加密 → [ItemHeader + 密文] 追加到 master mmap
//   master 使用率 > 80% 或 Flush 时，master 与 slave 交换；后台 runner 把 slave 写成 [ChunkHeader + 数据]
//   追加到日志文件
//
// mmap 缓存的布局（每个缓存从空开始写时，先写入本进程的客户端公钥）：
//   [ 65 字节客户端公钥 | ItemHeader | 密文 | ItemHeader | 密文 | ... ]
//   公钥跟着缓存一起落在 mmap 文件里，进程崩溃后下次启动补写残留数据时，仍能拿到加密这些数据时用的公钥。
//
// 日志文件的布局（decoder 按这个格式解析，改动必须同步到 M12）：
//   [ ChunkHeader | ItemHeader | 密文 | ItemHeader | 密文 | ... ][ ChunkHeader | ... ] ...
//   每个 chunk 是一个独立的 zstd 流：解压时每个 chunk 都要从第一个 item 开始按顺序解压。
//
// 注意：一个目录只能给一个 EffectiveSink 使用（master_cache / slave_cache 文件名是固定的）。
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include "compress/compress.h"
#include "context/executor.h"
#include "crypt/crypt.h"
#include "mmap/mmap_aux.h"
#include "sinks/sink.h"
#include "space.h"

namespace logger {

namespace detail {

// 一个日志块chuck的头部
struct ChunkHeader {
    static constexpr uint64_t kMagic = 0xdeadbeefdada1100;
    uint64_t magic = kMagic;
    uint64_t size = 0;
    char pub_key[128] = {};  // 客户端公钥，前65字节有效
};

struct ItemHeader {
    static constexpr uint32_t kMagic = 0xbe5fba11;
    uint32_t magic = kMagic;
    uint32_t size = 0;
};

static_assert(sizeof(ChunkHeader) == 144, "ChunkHeader layout changed");
static_assert(sizeof(ItemHeader) == 8, "ItemHeader layout changed");

}  // namespace detail

class EffectiveSink final : public LogSink {
public:
    // 配置 configuration
    struct Conf {
        std::filesystem::path dir;
        std::string prefix;   // 文件名：{prefix}_{YYYYmmddHHMMSS}.log，同一秒重名时加 _1、_2…
        std::string pub_key;  // 服务端公钥
        std::chrono::minutes interval{5};
        megabytes single_size{4};
        megabytes total_size{100};
    };

    explicit EffectiveSink(Conf conf);

    ~EffectiveSink() override;

    EffectiveSink(const EffectiveSink&) = delete;
    EffectiveSink& operator=(const EffectiveSink&) = delete;

    void Log(const LogMsg& msg) override;

    void SetFormatter(std::unique_ptr<Formatter> formatter) override;

    void Flush() override;

private:
    void SwapCache_();

    // 把 slave 写进日志文件并清空。在后台 runner 上执行（构造函数里补写残留数据时在当前线程执行）
    void CacheToFile_();

    std::filesystem::path GetFilePath_();

    void EliminateFiles_();

    Conf conf_;

    // 保护 Log 用到的所有状态：formatter_、compress_、crypt_、两个缓冲区，以及 master/slave 的交换
    std::mutex mutex_;
    std::unique_ptr<Formatter> formatter_;
    std::unique_ptr<compress::Compression> compress_;
    std::unique_ptr<crypt::Crypt> crypt_;
    std::string client_pub_key_;
    std::string compressed_buf_;
    std::string encrypted_buf_;

    std::unique_ptr<MMapAux> master_cache_;
    std::unique_ptr<MMapAux> slave_cache_;
    // true：slave 是空的，可以交换。false：slave 正在等待或正在写文件，此时后台 runner 独占 slave_cache_
    std::atomic<bool> is_slave_free_{true};

    std::filesystem::path log_file_path_;

    ctx::TaskRunnerTag task_runner_ = 0;
    ctx::RepeatedTaskId eliminate_task_id_ = 0;
};

}  // namespace logger