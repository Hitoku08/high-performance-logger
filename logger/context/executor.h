// 文件作用：任务调度器 Executor，以及它用到的定时器 ExecutorTimer。
//
// Executor 管理多个 runner。每个 runner 就是一个只有 1 个线程的 ThreadPool，用一个数字 tag 区分。
// 因为只有 1 个线程，同一个 runner 上的任务一定按投递顺序一个一个执行。
//
// ExecutorTimer 负责“过一段时间再做”：到时间后，把任务交给对应的 runner 去执行。
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

#include "context/thread_pool.h"

namespace logger {
namespace ctx {

using TaskRunnerTag = uint64_t;
using RepeatedTaskId = uint64_t;

constexpr uint64_t kRepeatForever = std::numeric_limits<uint64_t>::max();

// 定时器：一个后台线程 + 一个按到期时间排序的队列
class ExecutorTimer {
public:
    ExecutorTimer();
    ~ExecutorTimer();

    // delay 之后执行 task
    void PostDelayedTask(Task task, std::chrono::milliseconds delay);

private:
    using Clock = std::chrono::steady_clock;

    struct TimedTask {
        Clock::time_point when;
        Task task;

        // 优先级排序，到期最早的排队首
        bool operator>(const TimedTask& other) const { return when > other.when; }
    };

    void Run_();  // 后台线程执行的函数

    std::mutex mutex_;
    std::condition_variable cv_;
    // 这样得到的是小顶堆，到期时间最小的元素在top
    std::priority_queue<TimedTask, std::vector<TimedTask>, std::greater<TimedTask>> queue_;
    bool running_ = true;
    std::thread thread_;
};

class Executor {
public:
    Executor() = default;
    ~Executor();

    TaskRunnerTag AddTaskRunner(TaskRunnerTag tag);

    bool PostTask(TaskRunnerTag tag, Task task);

    void PostDelayedTask(TaskRunnerTag tag, Task task, std::chrono::milliseconds delay);

    RepeatedTaskId PostRepeatedTask(TaskRunnerTag tag,
                                    Task task,
                                    std::chrono::milliseconds interval,
                                    uint64_t repeat_num);

    void CancelRepeatedTask(RepeatedTaskId id);

    // 提交一个有返回值的任务
    template <class F>
    auto PostTaskAndGetResult(TaskRunnerTag tag, F f) {
        ThreadPool* runner = GetTaskRunner_(tag);
        using Result = decltype(runner->RunRetTask(std::move(f)));
        if (runner == nullptr) {
            return Result();  // 空指针
        }
        return runner->RunRetTask(std::move(f));
    }

    void WaitTaskIdle(TaskRunnerTag tag);

private:
    ThreadPool* GetTaskRunner_(TaskRunnerTag tag);

    void PostNextRepeat_(TaskRunnerTag tag,
                         Task task,
                         std::chrono::milliseconds interval,
                         uint64_t remaining,
                         RepeatedTaskId id);

    bool IsRepeatActive_(RepeatedTaskId id);

    std::mutex mutex_;  // 保护下面四个成员
    std::map<TaskRunnerTag, std::unique_ptr<ThreadPool>> runners_;
    TaskRunnerTag next_tag_ = 1;
    std::set<RepeatedTaskId> active_repeats_;
    RepeatedTaskId next_repeat_id_ = 1;

    ExecutorTimer timer_;
};

}  // namespace ctx
}  // namespace logger
