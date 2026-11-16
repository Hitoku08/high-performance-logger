#include "context/context.h"

namespace logger {
namespace ctx {

Context* Context::GetInstance() {
    // 用 new 创建，并且故意不 delete。
    // 原因：程序退出时，全局对象的销毁顺序不好控制。如果 Context 先没了，
    // 别的全局对象销毁时再用它就会出错。不 delete 就没这个问题，进程退出时系统会回收。
    static Context* instance = new Context();
    return instance;
}

}  // namespace ctx
}  // namespace logger
