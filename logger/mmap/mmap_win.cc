// 文件作用：Windows 下的文件映射。
//
// Windows 映射一个文件要三步：
//   1. CreateFileW：打开文件，得到文件句柄
//   2. CreateFileMappingW：创建“映射对象”，同时把文件扩大到 capacity 字节
//   3. MapViewOfFile：把映射对象映射到内存，得到一个指针
// 映射建立后就可以关掉前两个句柄，映射本身会一直有效，直到 UnmapViewOfFile。
#include <windows.h>

#include "mmap/mmap_aux.h"

namespace logger {

bool MMapAux::Map_(size_t capacity) {
    // OPEN_ALWAYS：文件存在就打开，不存在就创建。
    HANDLE file = CreateFileW(file_path_.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    // 64 位的大小要拆成高 32 位和低 32 位两个参数传进去。
    DWORD size_high = static_cast<DWORD>(static_cast<uint64_t>(capacity) >> 32);
    DWORD size_low = static_cast<DWORD>(capacity & 0xFFFFFFFF);
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, size_high, size_low, nullptr);
    CloseHandle(file);  // 映射对象已经引用了文件，文件句柄可以关了
    if (mapping == nullptr) {
        return false;
    }

    handle_ = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity);
    CloseHandle(mapping);  // 映射到内存后，映射对象的句柄也可以关了
    return handle_ != nullptr;
}

void MMapAux::Unmap_() {
    if (handle_ != nullptr) {
        UnmapViewOfFile(handle_);
        handle_ = nullptr;
    }
}

}  // namespace logger
