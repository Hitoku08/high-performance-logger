#include "context/executor.h"

namespace logger {
namespace ctx {

// ========== ExecutorTimer ==========
ExecutorTimer::ExecutorTimer() : thread_([this] { Run_(); }) {}

ExecutorTimer::~ExecutorTimer() {
    {
        std::lock_guard<std::mutex> lock_(mutex_);
        running_ = false;
    }
    cv_.notify_all();
    thread_.join();
}

void ExecutorTimer::PostDelayedTask(Task task, std::chrono::milliseconds delay) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({Clock::now() + delay, std::move(task)});
    }
    cv_.notify_one();
}

void ExecutorTimer::Run_() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (running_) {
        if (queue_.empty()) {
            cv_.wait(lock);
            continue;
        }
        if (Clock::now() < queue_.top().when) {
            // 这里的wait_until()的第二个参数是引用传递，但是在线程阻塞期间
            // 其他的线程可能会改变queue_，导致queue_底层的vector扩容。这时候引用就悬空了
            // 所以使用局部变量
            auto when = queue_.top().when;
            cv_.wait_until(lock, when);  // 阻塞直到队头任务到期
            continue;
        }

        Task task = queue_.top().task;
        queue_.pop();

        lock.unlock();  // 执行任务时解锁
        task();
        lock.lock();
    }
}

// ========== Executor ==========

Executor::~Executor() {
    // 先停止所有 runner（会把排队的任务执行完）。
    // 这里不加锁：Stop 时 runner 里的任务可能要调用 PostTask，PostTask 要拿 mutex_，加锁就会互相等待。
    // 析构时不会再有人添加 runner，runners_ 不会被修改，只读不加锁是安全的。
    for (auto& entry : runners_) {
        entry.second->Stop();
    }
    // 函数结束后，成员被销毁：timer_ 先销毁，然后才是 runners_。
}

TaskRunnerTag Executor::AddTaskRunner(TaskRunnerTag tag) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (runners_.count(tag) > 0) {
        tag = next_tag_++;  // 被占用了，换一个
    }
    auto runner = std::make_unique<ThreadPool>(1);  // 只有 1 个线程，保证任务按顺序执行
    runner->Start();
    runners_[tag] = std::move(runner);
    return tag;
}

// 把任务交给 runner 执行。runner 不存在时返回 false。
bool Executor::PostTask(TaskRunnerTag tag, Task task) {
    ThreadPool* runner = GetTaskRunner_(tag);
    if (runner == nullptr) {
        return false;
    }
    return runner->RunTask(std::move(task));
}

// delay 之后把任务交给 runner 执行。
void Executor::PostDelayedTask(TaskRunnerTag tag, Task task, std::chrono::milliseconds delay) {
    // 定时器到期时，并不直接执行 task，而是把它交给 runner 去执行。
    timer_.PostDelayedTask([this, tag, task] { PostTask(tag, task); }, delay);
}

RepeatedTaskId Executor::PostRepeatedTask(TaskRunnerTag tag,
                                          Task task,
                                          std::chrono::milliseconds interval,
                                          uint64_t repeat_num) {
    RepeatedTaskId id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        id = next_repeat_id_++;
        active_repeats_.insert(id);
    }
    if (repeat_num > 0) {
        PostNextRepeat_(tag, task, interval, repeat_num, id);
    }
    return id;
}

void Executor::PostNextRepeat_(TaskRunnerTag tag,
                               Task task,
                               std::chrono::milliseconds interval,
                               uint64_t remaining,
                               RepeatedTaskId id) {
    // 投递一个延迟任务，它执行时做三件事：
    //   1. 检查有没有被取消
    //   2. 执行一次 task
    //   3. 还有剩余次数就再投递下一次
    PostDelayedTask(
        tag,
        [this, tag, task, interval, remaining, id] {
            if (!IsRepeatActive_(id)) {
                return;  // 已经被取消
            }
            task();
            if (remaining > 1) {
                PostNextRepeat_(tag, task, interval, remaining - 1, id);
            } else {
                CancelRepeatedTask(id);  // 最后一次执行完，从集合里删掉
            }
        },
        interval);
}

void Executor::CancelRepeatedTask(RepeatedTaskId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_repeats_.erase(id);
}

bool Executor::IsRepeatActive_(RepeatedTaskId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_repeats_.count(id) > 0;
}

// 阻塞等待，直到这个 runner 上已经投递的任务全部执行完
// 放一个空任务到队尾，等待空任务执行
void Executor::WaitTaskIdle(TaskRunnerTag tag) {
    auto done = PostTaskAndGetResult(tag, [] {});
    if (done != nullptr) {
        done->wait();
    }
}

ThreadPool* Executor::GetTaskRunner_(TaskRunnerTag tag) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = runners_.find(tag);
    if (it == runners_.end()) {
        return nullptr;
    }
    return it->second.get();
}

}  // namespace ctx
}  // namespace logger