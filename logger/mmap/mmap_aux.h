// 文件作用：mmap 缓存。把一个磁盘文件映射到内存，像操作普通内存一样往里追加数据。
//
// 为什么用 mmap：写进映射内存的数据由操作系统负责同步到文件。
// 即使程序突然崩溃，已经写进去的数据也还在文件里，下次启动时可以读出来补写到日志，日志不会丢。
//
// 文件布局：
//     [ MmapHeader（magic + size） | 数据 ...... | 未使用的空间 ...... ]
//     |<------------------------ capacity_（按页对齐）------------------->|
//
// 用法：
//     MMapAux cache("D:/logs/cache.mmap");
//     cache.Push("hello", 5);
//     cache.Size();   // 5
//     cache.Data();   // 指向 "hello"
//     cache.Clear();  // Size 变回 0
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace logger {

class MMapAux {
public:
    explicit MMapAux(std::filesystem::path file_path);
    ~MMapAux();

    // 映射不能拷贝
    MMapAux(const MMapAux&) = delete;
    MMapAux& operator=(const MMapAux&) = delete;

    // 映射是否成功
    bool IsValid() const { return handle_ != nullptr; }

    // 数据的起始地址
    uint8_t* Data() const;

    // 已经写入的数据字节数
    size_t Size() const;

    bool Empty() const { return Size() == 0; }

    // 在末尾追加 size 字节
    void Push(const void* data, size_t size);

    void Clear();

    double GetRatio() const;

private:
    struct MmapHeader {
        static constexpr uint32_t kMagic = 0xdeadbeef;  // 判断文件是不是我们写的
        uint32_t magic;
        uint32_t size;
    };

    MmapHeader* Header_() const { return static_cast<MmapHeader*>(handle_); }

    // 保证容量最少有 need 字节，相当于 vector 的reserve
    void EnsureCapacity_(size_t need);

    // 下面两个函数和操作系统有关， 在 mmap_win.cc 中实现
    bool Map_(size_t capacity);  // 把文件扩大到 capacity 字节并映射到内存，成功后 handle_ 指向映射的起始地址。
    void Unmap_();               // 解除映射，handle_ 变成 nullptr

    std::filesystem::path file_path_;
    void* handle_ = nullptr;  // 映射的起始地址
    size_t capacity_ = 0;
};

}  // namespace logger
