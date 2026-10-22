#include "utils/file_util.h"

#include <system_error>

namespace logger{
namespace fs{

size_t GetFileSize(const std::filesystem::path& file_path) {
    std::error_code ec;
    // 下面语句可以确保文件路径存在，而且是普通文件。否则就返回 0 
    if(!std::filesystem::is_regular_file(file_path, ec)) {
        return 0;
    }
    const std::uintmax_t size = std::filesystem::file_size(file_path, ec);
    // 如果获取文件大小失败，就会触发下面条件语句
    if(ec) {
        return 0;
    }    
    return static_cast<size_t>(size);
}

}
}