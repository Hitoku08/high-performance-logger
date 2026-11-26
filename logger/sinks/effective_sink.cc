#include "sinks/effective_sink.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include "compress/zstd_compress.h"
#include "context/context.h"
#include "crypt/aes_crypt.h"
#include "formatter/effective_formatter.h"
#include "internal_log.h"
#include "utils/file_util.h"
#include "utils/sys_util.h"

namespace logger {

namespace {

constexpr ctx::TaskRunnerTag kFileRunnerTag = 10086;  // 被占用时 NEW_TASK_RUNNER 会返回另一个 tag
constexpr double kSwapRatio = 0.8;                    // master 使用率超过它就交换

}  // namespace

EffectiveSink::EffectiveSink(Conf conf) : conf_(std::move(conf)) {
    std::error_code ec;
    std::filesystem::create_directories(conf_.dir, ec);
    if (ec) {
        throw std::runtime_error("EffectiveSink: create dir failed: " + conf_.dir.string());
    }

    // 1. 密钥：本进程临时生成一对客户端密钥，用"客户端私钥 + 服务端公钥"算出 AES 密钥。
    //    客户端私钥用完就丢，只有持有服务端私钥的人能解密。
    crypt::KeyPair client = crypt::GenECDHKey();
    const std::string shared_secret =
        crypt::GenECDHSharedSecret(client.private_key, crypt::HexKeyToBinary(conf_.pub_key));
    if (shared_secret.empty()) {
        throw std::invalid_argument("EffectiveSink: invalid server public key");
    }
    client_pub_key_ = std::move(client.public_key);
    crypt_ = std::make_unique<crypt::AESCrypt>(shared_secret);
    compress_ = std::make_unique<compress::ZstdCompress>();
    formatter_ = std::make_unique<EffectiveFormatter>();

    // 2. 双缓存
    master_cache_ = std::make_unique<MMapAux>(conf_.dir / "master_cache");
    slave_cache_ = std::make_unique<MMapAux>(conf_.dir / "slave_cache");
    if (!master_cache_->IsValid() || !slave_cache_->IsValid()) {
        throw std::runtime_error("EffectiveSink: create mmap failed in " + conf_.dir.string());
    }

    // 3. 崩溃恢复：上次进程没来得及写进文件的数据还在 mmap 里，先补写。
    //    这时还没有别的线程能访问这个对象，直接在当前线程同步写。slave 比 master 旧，先写 slave。
    if (!slave_cache_->Empty()) {
        is_slave_free_.store(false);
        CacheToFile_();
    }
    if (!master_cache_->Empty()) {
        std::swap(master_cache_, slave_cache_);
        is_slave_free_.store(false);
        CacheToFile_();
    }

    // 4. 后台 runner 和定时淘汰
    task_runner_ = NEW_TASK_RUNNER(kFileRunnerTag);
    eliminate_task_id_ =
        POST_REPEATED_TASK(task_runner_, [this] { EliminateFiles_(); }, conf_.interval, ctx::kRepeatForever);
}

EffectiveSink::~EffectiveSink() {
    // 先取消定时任务：取消之后即使定时器到期，任务也会在检查时直接返回，不会再访问 this。
    // 正在执行的那一次会被下面 Flush 里的 WAIT_TASK_IDLE 等到结束。
    EXECUTOR->CancelRepeatedTask(eliminate_task_id_);
    Flush();
}

void EffectiveSink::Log(const LogMsg& msg) {
    // 格式化不依赖共享状态，放在锁外面做
    thread_local MemoryBuf formatted;
    formatter_->Format(msg, &formatted);

    std::lock_guard<std::mutex> lock(mutex_);

    // master 从空开始写：开始一段新的 zstd 流，并先写入客户端公钥
    if (master_cache_->Empty()) {
        compress_->ResetStream();
        master_cache_->Push(client_pub_key_.data(), client_pub_key_.size());
    }

    // 先 resize 再写 data()，写入范围在 size() 之内（参考实现在 reserve 之后直接写，是未定义行为）
    compressed_buf_.resize(compress_->CompressedBound(formatted.size()));
    const size_t compressed_size =
        compress_->Compress(formatted.data(), formatted.size(), compressed_buf_.data(), compressed_buf_.size());
    if (compressed_size == 0) {
        LOG_ERROR("EffectiveSink::Log: compress failed");
        return;
    }

    crypt_->Encrypt(compressed_buf_.data(), compressed_size, encrypted_buf_);
    if (encrypted_buf_.empty()) {
        LOG_ERROR("EffectiveSink::Log: encrypt failed");
        return;
    }

    detail::ItemHeader item_header;
    item_header.size = static_cast<uint32_t>(encrypted_buf_.size());
    master_cache_->Push(&item_header, sizeof(item_header));
    master_cache_->Push(encrypted_buf_.data(), encrypted_buf_.size());

    if (master_cache_->GetRatio() > kSwapRatio) {
        SwapCache_();
    }
}

void EffectiveSink::SetFormatter(std::unique_ptr<Formatter> /*formatter*/) {}

void EffectiveSink::Flush() {
    // 1. 等后台把正在写的 slave 写完，slave 空出来
    WAIT_TASK_IDLE(task_runner_);
    // 2. 把 master 换到 slave，交给后台写
    {
        std::lock_guard<std::mutex> lock(mutex_);
        SwapCache_();
    }
    // 3. 等这次写完
    WAIT_TASK_IDLE(task_runner_);
}

void EffectiveSink::SwapCache_() {
    // "检查 slave 是否空闲"和"交换"都在 mutex_ 里完成，两个线程不会同时交换。
    // 参考实现在锁外先 load 再 store，两个线程可能都看到空闲，连续交换两次，把写满的缓存又换回 master。
    if (master_cache_->Empty() || !is_slave_free_.load(std::memory_order_acquire)) {
        return;  // slave 还在写：master 先继续写（mmap 会自动扩容），下次再交换
    }
    std::swap(master_cache_, slave_cache_);
    is_slave_free_.store(false, std::memory_order_release);
    POST_TASK(task_runner_, [this] { CacheToFile_(); });
}

void EffectiveSink::CacheToFile_() {
    if (is_slave_free_.load(std::memory_order_acquire)) {
        return;
    }

    MMapAux* cache = slave_cache_.get();
    // 只有公钥、没有 item 的缓存（比如压缩失败）不写文件
    if (cache->Size() > crypt::kPublicKeyLength) {
        detail::ChunkHeader chunk_header;
        chunk_header.size = cache->Size() - crypt::kPublicKeyLength;
        std::memcpy(chunk_header.pub_key, cache->Data(), crypt::kPublicKeyLength);

        const std::filesystem::path file_path = GetFilePath_();
        std::ofstream ofs(file_path, std::ios::binary | std::ios::app);
        ofs.write(reinterpret_cast<const char*>(&chunk_header), sizeof(chunk_header));
        ofs.write(reinterpret_cast<const char*>(cache->Data() + crypt::kPublicKeyLength),
                  static_cast<std::streamsize>(chunk_header.size));
        if (!ofs) {
            // 写失败时丢弃这个 chunk：留着的话 slave 永远不空闲，master 会无限增长
            LOG_ERROR("EffectiveSink::CacheToFile_: write {} failed, drop {} bytes", file_path.string(), cache->Size());
        }
    }

    cache->Clear();
    // 最后一步才把 slave 标记为空闲：store 之后后台不再访问 slave_cache_，Log 线程才可以交换
    is_slave_free_.store(true, std::memory_order_release);
}

std::filesystem::path EffectiveSink::GetFilePath_() {
    const size_t single_bytes = space_cast<bytes>(conf_.single_size).count();
    if (!log_file_path_.empty() && fs::GetFileSize(log_file_path_) < single_bytes) {
        return log_file_path_;
    }

    // 切新文件：{prefix}_{YYYYmmddHHMMSS}.log，同一秒内已存在时依次尝试 _1、_2…
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    LocalTime(&tm, &now);
    char time_buf[32] = {0};
    std::strftime(time_buf, sizeof(time_buf), "%Y%m%d%H%M%S", &tm);
    const std::string base_name = conf_.prefix + "_" + time_buf;

    std::filesystem::path file_path = conf_.dir / (base_name + ".log");
    for (int index = 1; std::filesystem::exists(file_path); ++index) {
        file_path = conf_.dir / (base_name + "_" + std::to_string(index) + ".log");
    }
    log_file_path_ = file_path;
    LOG_INFO("EffectiveSink: new log file {}", log_file_path_.string());

    // 每切一个新文件，总大小就可能超限，顺便淘汰一次
    EliminateFiles_();
    return log_file_path_;
}

void EffectiveSink::EliminateFiles_() {
    struct LogFile {
        std::filesystem::file_time_type write_time;
        std::filesystem::path path;
        size_t size;
    };

    // 只统计本 sink 产生的文件：{prefix}_*.log
    const std::string name_prefix = conf_.prefix + "_";
    std::vector<LogFile> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(conf_.dir, ec)) {
        const std::filesystem::path& path = entry.path();
        if (path.extension() != ".log" || path.filename().string().rfind(name_prefix, 0) != 0) {
            continue;
        }
        std::error_code time_ec;
        const auto write_time = std::filesystem::last_write_time(path, time_ec);
        if (time_ec) {
            continue;
        }
        files.push_back({write_time, path, fs::GetFileSize(path)});
    }

    // 最新的在前，累加大小，超过上限之后的文件（更旧的）全部删除
    std::sort(files.begin(), files.end(),
              [](const LogFile& lhs, const LogFile& rhs) { return lhs.write_time > rhs.write_time; });

    const size_t total_bytes = space_cast<bytes>(conf_.total_size).count();
    size_t used_bytes = 0;
    for (const LogFile& file : files) {
        used_bytes += file.size;
        if (used_bytes > total_bytes && file.path != log_file_path_) {
            LOG_INFO("EffectiveSink: remove old log file {}", file.path.string());
            std::filesystem::remove(file.path, ec);
        }
    }
}

}  // namespace logger