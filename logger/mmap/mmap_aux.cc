#include "mmap_aux.h"

#include <algorithm>
#include <cstring>

#include "utils/file_util.h"
#include "utils/sys_util.h"

namespace logger {

static constexpr size_t kDefaultCapacity = 512 * 1024;  // 默认 512KB

// 把 size 向上取整到页大小的整数倍。比如页大小 4096 时，5000 -> 8192。
static size_t AlignToPage(size_t size) {
    size_t page_size = GetPageSize();  // 返回操作系内存页面大小，一般是 4KB
    return (size + page_size - 1) / page_size * page_size;
}

MMapAux::MMapAux(std::filesystem::path file_path) : file_path_(std::move(file_path)) {
    size_t capacity = std::max(fs::GetFileSize(file_path_), kDefaultCapacity);
    capacity = AlignToPage(capacity);
    if (!Map_(capacity)) {
        return;
    }
    capacity_ = capacity;

    MmapHeader* header = Header_();
    size_t max_size = capacity_ - sizeof(MmapHeader);

    // header->magic != MmapHeader::kMagic 的时候，说明文件是新文件或者是不认识的文件
    // header->size > max_size 的时候说明文件有可能损坏
    if (header->magic != MmapHeader::kMagic || header->size > max_size) {
        header->magic = MmapHeader::kMagic;
        header->size = 0;
    }
}

MMapAux::~MMapAux() {
    Unmap_();
}

uint8_t* MMapAux::Data() const {
    if (!IsValid()) {
        return nullptr;
    }
    // 跳过文件头
    return static_cast<uint8_t*>(handle_) + sizeof(MmapHeader);
}

size_t MMapAux::Size() const {
    if (!IsValid()) {
        return 0;
    }
    return Header_()->size;
}

void MMapAux::Push(const void* data, size_t size) {
    if (!IsValid() || size == 0) {
        return;
    }
    size_t new_size = Size() + size;
    EnsureCapacity_(sizeof(MmapHeader) + new_size);
    if (!IsValid()) {
        return;  // 扩容失败
    }
    // void* memcpy(void* dest, const void* src, std::size_t count);
    // 从 src 拷贝 count字节的数据到 dest，返回dest
    std::memcpy(Data() + Size(), data, size);
    Header_()->size = static_cast<uint32_t>(new_size);
}

void MMapAux::Clear() {
    if (!IsValid()) {
        return;
    }
    Header_()->size = 0;
}

double MMapAux::GetRatio() const {
    if (!IsValid()) {
        return 0.0;
    }
    return static_cast<double>(Size()) / (capacity_ - sizeof(MmapHeader));
}

void MMapAux::EnsureCapacity_(size_t need) {
    if (need <= capacity_) {
        return;
    }
    // 每次至少翻倍
    size_t new_capacity = AlignToPage(std::max(need, capacity_ * 2));

    // 先解除旧映射
    Unmap_();
    if (Map_(new_capacity)) {
        capacity_ = new_capacity;
    } else {
        capacity_ = 0;  // 如果失败， handle_ 是 nullptr
    }
}
}  // namespace logger