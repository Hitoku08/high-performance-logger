// 文件作用：线程池。开几个线程，它们不停地从一个任务队列里取任务来执行。
//
// 用法：
//     ThreadPool pool(4);   // 4 个线程
//     pool.Start();
//     pool.RunTask([] { DoWork(); });
//     auto result = pool.RunRetTask([] { return 42; });
//     int value = result->get();  // 等任务执行完，拿到 42
//     pool.Stop();
#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace logger {
namespace ctx {

using Task = std::function<void()>;

class ThreadPool {
public:
    explicit ThreadPool(int thread_count);
    ~ThreadPool();  // 自动调用 Stop

    // 启动线程。已经启动过返回 false。
    bool Start();

    // 不再接受新任务，等队列里剩下的任务执行完，然后结束所有线程。
    void Stop();

    // 提交一个任务到任务队列。线程池没在运行时返回 false。
    bool RunTask(Task task);

    // 提交一个有返回值的任务到任务队列，通过返回的 future 拿结果。线程池没在运行时返回空指针。
    template <typename F>
    auto RunRetTask(F f) {
        // 这里的 f 是一个可调用对象，invoke_result_t<F>是返回值类型
        using R = std::invoke_result_t<F>;  // F 的返回值类型

        // packaged_task 会把 f 的返回值存进 future，调用方用 future->get() 取出来。
        auto task = std::make_shared<std::packaged_task<R()>>(std::move(f));
        auto future = std::make_shared<std::future<R>>(task->get_future());

        // 投递一个普通任务，在里面执行 packaged_task。
        if (!RunTask([task] { (*task)(); })) {
            future.reset();  // 没投递成功，返回空指针
        }
        return future;
    }

private:
    void WorkerLoop_();  // 每个线程执行的函数

    int thread_count_;
    std::vector<std::thread> workers_;

    std::mutex mutex_;            // 保护 tasks_ 和 running_
    std::condition_variable cv_;  // 来了新任务、或者要停止时，用它叫醒线程
    std::queue<Task> tasks_;
    bool running_ = false;
};

}  // namespace ctx
}  // namespace logger
