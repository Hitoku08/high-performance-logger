// 定义 LogFactory，一个全局单例
#pragma once;

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace logger {

class VariadicLogHandle;

class LogFactory {
public:
    // 单例不能拷贝
    LogFactory(const LogFactory&) = delete;
    LogFactory& operator=(const LogFactory&) = delete;

    // 获取全局唯一的实例对象
    static LogFactory& Instance();

    VariadicLogHandle* GetLogHandle() const noexcept;

    void SetLogHandle(std::shared_ptr<VariadicLogHandle> log_handle);

private:
    // 单例模式，要把构造函数设置成私有的，防止外界调用
    LogFactory() = default;

    std::atomic<VariadicLogHandle*> current_{nullptr};
    std::mutex mutex_;
    std::vector<std::shared_ptr<VariadicLogHandle>> handles_;
};

}  // namespace logger