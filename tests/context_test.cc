#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "logger/context/context.h"

using logger::ctx::Executor;
using logger::ctx::ThreadPool;
using namespace std::chrono_literals;  // 可以写 50ms、2s 这样的时间

// 每隔 1ms 检查一次 condition，成立就返回 true；超过 timeout 还不成立返回 false。
template <typename F>
bool WaitUntil(F condition, std::chrono::milliseconds timeout = 2s) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

// ---------- ThreadPool ----------

TEST(ThreadPoolTest, RunRetTaskReturnsValue) {
    ThreadPool pool(2);
    pool.Start();
    auto result = pool.RunRetTask([] { return 40 + 2; });
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get(), 42);
}

TEST(ThreadPoolTest, RunsAllTasks) {
    ThreadPool pool(4);
    pool.Start();
    std::atomic<int> count(0);
    for (int i = 0; i < 1000; i++) {
        pool.RunTask([&count] { ++count; });
    }
    pool.Stop();
    EXPECT_EQ(count, 1000);
}

TEST(ThreadPoolTest, RejectsTasksAfterStop) {
    ThreadPool pool(2);
    pool.Start();
    pool.Stop();
    EXPECT_FALSE(pool.RunTask([] {}));
    EXPECT_EQ(pool.RunRetTask([] { return 1; }), nullptr);
}

TEST(ThreadPoolTest, DestructorDoesNotHang) {
    std::atomic<int> count{0};
    {
        ThreadPool pool(2);
        pool.Start();
        for (int i = 0; i < 10; i++) {
            pool.RunTask([&count] { ++count; });
        }
    }
    // 调用析构函数时，会调用 Stop 函数，保证所有任务都被完成
    EXPECT_EQ(count, 10);
}

// ---------- Executor ----------

TEST(ExecutorTest, TasksRunInOrder) {
    Executor executor;
    auto runner = executor.AddTaskRunner(1);

    std::vector<int> order;  // 同一个 runner 只有一个线程，不需要加锁
    for (int i = 0; i < 100; ++i) {
        executor.PostTask(runner, [&order, i] { order.push_back(i); });
    }
    executor.WaitTaskIdle(runner);

    ASSERT_EQ(order.size(), 100u);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(order[i], i);
    }
}

TEST(ExecutorTest, ExistingTagGetsNewTag) {
    Executor executor;
    auto first = executor.AddTaskRunner(7);
    auto second = executor.AddTaskRunner(7);
    EXPECT_EQ(first, 7u);
    EXPECT_NE(first, second);
    EXPECT_TRUE(executor.PostTask(second, [] {}));  // 新的runner能正常接受任务
}

TEST(ExecutorTest, UnknownRunner) {
    Executor executor;
    EXPECT_FALSE(executor.PostTask(12345, [] {}));  // 返回 flase
    executor.WaitTaskIdle(12345);                   // 不崩溃、不卡住
}

TEST(ExecutorTest, DelayedTask) {
    Executor executor;
    auto runner = executor.AddTaskRunner(1);
    std::atomic<bool> done{false};

    executor.PostDelayedTask(runner, [&done] { done = true; }, 50ms);

    EXPECT_FALSE(done);  // 50ms 还没到
    EXPECT_TRUE(WaitUntil([&done] { return done.load(); }, 500ms));
}

TEST(ExecutorTest, RepeatedTaskRunsNTimes) {
    Executor executor;
    auto runner = executor.AddTaskRunner(1);
    std::atomic<int> count{0};

    executor.PostRepeatedTask(runner, [&count] { ++count; }, 10ms, 5);

    EXPECT_TRUE(WaitUntil([&count] { return count == 5; }));
    std::this_thread::sleep_for(100ms);  // 再等一会儿，确认没有第 6 次
    EXPECT_EQ(count, 5);
}

TEST(ExecutorTest, CancelRepeatedTask) {
    Executor executor;
    auto runner = executor.AddTaskRunner(1);
    std::atomic<int> count{0};

    auto id = executor.PostRepeatedTask(runner, [&count] { ++count; }, 5ms, logger::ctx::kRepeatForever);
    EXPECT_TRUE(WaitUntil([&count] { return count >= 3; }));

    executor.CancelRepeatedTask(id);
    executor.WaitTaskIdle(runner);  // 等正在执行的那一次结束
    int after_cancel = count;

    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(count, after_cancel);  // 取消后没有再增加
}

// ---------- Context 宏 ----------

TEST(ContextTest, Macros) {
    auto runner = NEW_TASK_RUNNER(10086);
    std::atomic<int> count{0};

    EXPECT_TRUE(POST_TASK(runner, [&count] { ++count; }));
    WAIT_TASK_IDLE(runner);
    EXPECT_EQ(count, 1);

    POST_REPEATED_TASK(runner, [&count] { ++count; }, 5ms, 3);
    EXPECT_TRUE(WaitUntil([&count] { return count == 4; }));
}