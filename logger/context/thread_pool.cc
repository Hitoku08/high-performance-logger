#include "context/thread_pool.h"

namespace logger {
namespace ctx {

ThreadPool::ThreadPool(int thread_count) : thread_count_(thread_count) {}

ThreadPool::~ThreadPool() {
    Stop();
}

bool ThreadPool::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return false;
    }
    running_ = true;
    for (int i = 0; i < thread_count_; i++) {
        workers_.emplace_back([this] { WorkerLoop_(); });
    }
    return true;
}

void ThreadPool::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    cv_.notify_all();  // 唤醒所有等待线程

    // 等待所有线程退出
    for (std::thread& t : workers_) {
        t.join();
    }
    workers_.clear();
}

// RunTask作用是把任务加到任务队列中，然后随机唤醒一个工作线程
bool ThreadPool::RunTask(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return false;
        }
        tasks_.push(std::move(task));
    }
    cv_.notify_one();  // 唤醒一个线程来执行
    return true;
}

void ThreadPool::WorkerLoop_() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // 阻塞，直到任务队列不为空或者线程池要关闭
            cv_.wait(lock, [this] { return !tasks_.empty() || !running_; });

            // 如果任务队列为空，就说明线程池要关闭
            if (tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();  // 只有取任务时需要锁，执行任务不需要加锁，否则会大大浪费效率，就不是并发执行了
    }
}
}  // namespace ctx
}  // namespace logger