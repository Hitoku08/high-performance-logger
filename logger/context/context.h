// 文件作用：全局唯一的 Context，里面放一个 Executor，再提供几个宏，让任何地方都能直接用。
//
// 用法：
//     auto runner = NEW_TASK_RUNNER(1);
//     POST_TASK(runner, [] { WriteFile(); });
//     WAIT_TASK_IDLE(runner);
//     POST_REPEATED_TASK(runner, [] { Clean(); }, std::chrono::seconds(10), logger::ctx::kRepeatForever);
#pragma once

#include "context/executor.h"

namespace logger {
namespace ctx {

class Context {
public:
    static Context* GetInstance();

    Executor* GetExecutor() { return &executor_; }

    TaskRunnerTag NewTaskRunner(TaskRunnerTag tag) { return executor_.AddTaskRunner(tag); }

private:
    Context() = default;  // 构造函数私有，外面只能通过 GetInstance 拿到

    Executor executor_;
};

}  // namespace ctx
}  // namespace logger

#define CONTEXT logger::ctx::Context::GetInstance()
#define EXECUTOR CONTEXT->GetExecutor()

#define NEW_TASK_RUNNER(tag) CONTEXT->NewTaskRunner(tag)
#define POST_TASK(tag, task) EXECUTOR->PostTask(tag, task)
#define WAIT_TASK_IDLE(tag) EXECUTOR->WaitTaskIdle(tag)
#define POST_REPEATED_TASK(tag, task, interval, repeat_num) EXECUTOR->PostRepeatedTask(tag, task, interval, repeat_num)
