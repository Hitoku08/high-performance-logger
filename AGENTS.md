# AGENTS.md

本文件给在本仓库中工作的 AI 编码助手（Cursor、Claude Code、Codex 等）和开发者本人阅读，描述项目背景、构建验证方式、开发路线和协作规则。

## 1. 项目背景

`logger-dev` 是一个**从零开始重写**的高性能 C++17 日志库，用于学习。参考实现位于同级目录 `../logger-tutorial`（克隆自 `CppTrainingHub/logger-tutorial`），本仓库只保留了其构建系统（CMake + Python 脚本）和第三方依赖，所有 `logger/`、`example/`、`decode/` 下的业务源码都由开发者按模块逐步编写。

最终功能：

- 同步日志接口 + `fmt` 风格格式化宏（`EXT_LOG_INFO("x={}", x)`）
- 控制台输出（`ConsoleSink`）
- 高性能落盘（`EffectiveSink`）：protobuf 序列化 → zstd 流式压缩 → ECDH + AES 加密 → mmap 双缓存 → 后台线程写文件 → 按大小切分、按总量淘汰
- 离线解码工具 `decoder`：解密、解压、反序列化，按 pattern 还原成文本

## 2. 目录结构

```
logger-dev/
├── CMakeLists.txt            # 根构建脚本，BUILD_STL_TEST=ON 时启用 tests/
├── logger/
│   ├── CMakeLists.txt        # 按模块登记源文件（注释里标了 M2、M3…）
│   ├── third_party/          # 第三方库，禁止修改
│   └── (待开发的模块源码)
├── example/CMakeLists.txt    # 自动构建已存在的 *_example.cc
├── decode/CMakeLists.txt     # decode.cc 存在时才构建 decoder
├── tests/
│   ├── CMakeLists.txt        # 自动收录 tests/*_test.cc，每个文件一个测试程序
│   └── build_sanity_test.cc  # M0 骨架自检
├── script/
│   ├── build_windows.py
│   ├── build_linux.py
│   └── bin/protoc(.exe)      # protobuf 21.8 编译器
├── AGENTS.md / CLAUDE.md
└── .github/pull_request_template.md
```

第三方库版本：zstd 1.5.6、zlib 1.2.13、protobuf 21.8、cryptopp 8.9.0、fmt 11.0.2、googletest 1.15.2。

## 3. 构建与验证

Windows（Visual Studio 2022，x64）：

```powershell
python script/build_windows.py            # 只构建
python script/build_windows.py --test     # 构建 + 运行全部单元测试（每个模块完成后必须执行）
python script/build_windows.py --clean --test   # 清空 build/ 后全量构建并测试
python script/build_windows.py --config Release --test
```

Linux：把 `build_windows.py` 换成 `build_linux.py`，参数相同。

构建产物在 `build/x64-Debug/`（Linux 为 `build/Debug/`）。单独运行测试：

```powershell
cd build/x64-Debug
ctest -C Debug --output-on-failure            # 全部
ctest -C Debug -R LogHandle --output-on-failure   # 只跑名字匹配的测试
```

说明：

- 原项目没有任何单元测试，`--test` 参数也没有真正生效。本仓库新增了 GoogleTest，并让 `--test` 在编译完成后自动执行 `ctest`，**脚本退出码非 0 即视为验证失败**。
- `logger/proto/*.proto` 存在时，脚本会先用 `script/bin/protoc` 生成 `effective_msg.pb.h/.cc` 到构建目录；不存在时自动跳过。
- 第一次构建要编译 protobuf 和 cryptopp，耗时较长；之后是增量构建。
- 新增 `tests/xxx_test.cc` 后重新运行脚本即可被收录，不需要改 `tests/CMakeLists.txt`。

## 4. 开发路线

模块按依赖顺序排列，**必须按顺序开发**，后面的模块依赖前面的模块。每个模块对应一个分支、一个 PR。

| 编号 | 分支 | 内容 | 依赖 |
|---|---|---|---|
| M0 | `main`（初始提交） | 构建骨架、测试框架 | — |
| M1 | `feature/m1-common` | 公共类型与工具头文件 | M0 |
| M2 | `feature/m2-utils` | 系统工具（页大小、进程/线程 ID、本地时间、文件大小） | M1 |
| M3 | `feature/m3-log-handle` | 日志消息、Formatter/Sink 接口、LogHandle | M1 |
| M4 | `feature/m4-console-sink` | 默认格式化器 + 控制台输出 | M2 M3 |
| M5 | `feature/m5-macro-api` | 可变参数日志、全局工厂、日志宏 | M4 |
| M6 | `feature/m6-context` | 线程池、任务执行器、定时/重复任务 | M1 |
| M7 | `feature/m7-mmap` | 跨平台 mmap 缓存 | M2 |
| M8 | `feature/m8-compress` | zstd / zlib 流式压缩 | M1 |
| M9 | `feature/m9-crypt` | ECDH 密钥交换 + AES 加解密 | M1 |
| M10 | `feature/m10-effective-formatter` | protobuf 消息定义 + 二进制格式化器 | M2 M3 |
| M11 | `feature/m11-effective-sink` | 高性能落盘 Sink | M6–M10 |
| M12 | `feature/m12-decoder` | 解码工具 + 端到端验证 | M11 |

每个模块的通用完成步骤：

1. 从最新的 `main` 拉出分支。
2. 编写源码；若有 `.cc`，在 `logger/CMakeLists.txt` 中取消对应 `# Mx` 行的注释。
3. 编写 `tests/<模块>_test.cc`，覆盖下文列出的验证要点。
4. 运行 `python script/build_windows.py --test`，全部通过。
5. 用 `.clang-format` 格式化改动过的文件。
6. 提交、推送、开 PR，合并到 `main`。

### M1 公共基础 `feature/m1-common`

文件：`logger/log_common.h`、`logger/space.h`、`logger/defer.h`、`logger/internal_log.h`

- `log_common.h`：`LogLevel` 枚举（Trace…Off，对应 `LOGGER_LEVEL_*` 宏）、`LOGGER_ACTIVE_LEVEL`、`StringView`/`MemoryBuf` 别名、`SourceLocation`（构造时只保留文件名，兼容 `/` 和 `\`）
- `space.h`：仿 `std::chrono::duration` 的容量类型 `space<Rep, Ratio>`，提供 `bytes/kilobytes/megabytes…` 与 `space_cast`
- `defer.h`：`ExecuteOnScopeExit` 与 `LOG_DEFER { ... };` 宏
- `internal_log.h`：库内部调试用的 `LOG_INFO/LOG_DEBUG/LOG_ERROR`（`ENABLE_LOG` 控制）

CMake：纯头文件，无需改动。

验证 `tests/common_test.cc`：

- `SourceLocation("/a/b/c.cc", 1, "f").file_name == "c.cc"`，Windows 路径 `"C:\\a\\c.cc"` 同样得到 `"c.cc"`
- `space_cast<bytes>(megabytes{4}).count() == 4 * 1024 * 1024`；`kilobytes` 与 `megabytes` 相互转换正确
- `LOG_DEFER` 在作用域结束时执行，且多个 defer 按逆序执行
- `LogLevel` 的数值与 `LOGGER_LEVEL_*` 宏一致

### M2 系统工具 `feature/m2-utils`

文件：`logger/utils/sys_util.h`、`sys_util.cc`、`sys_util_win.cc`、`sys_util_linux.cc`、`file_util.h`、`file_util.cc`、`timer_count.h`

- `GetPageSize()`、`GetProcessId()`、`GetThreadId()`、`LocalTime()`（Windows 用 `localtime_s`，Linux 用 `localtime_r`）
- `fs::GetFileSize()`：文件不存在返回 0
- `TimerCount`：RAII 计时，`TIMER_COUNT(info)` 宏

CMake：取消 `UTIL_SRCS` 中 `# M2` 行注释。

验证 `tests/utils_test.cc`：

- 页大小 > 0 且为 2 的幂
- `GetProcessId()` 与 `_getpid()` / `getpid()` 一致
- 两个不同线程的 `GetThreadId()` 不同，同一线程多次调用相同
- `LocalTime` 得到的年份 ≥ 2024
- 写入 N 字节的临时文件后 `GetFileSize == N`；不存在的文件返回 0

### M3 日志核心 `feature/m3-log-handle`

文件：`logger/log_msg.h/.cc`、`logger/formatter/formatter.h`、`logger/sinks/sink.h`、`logger/log_handle.h/.cc`

- `LogMsg`：位置、级别、消息（`StringView`，不拥有内存）
- `Formatter` 接口：`Format(const LogMsg&, MemoryBuf*)`
- `LogSink` 接口：`Log`、`SetFormatter`、`Flush`
- `LogHandle`：持有多个 sink，默认级别 `kInfo`，`ShouldLog_` 过滤后分发到所有 sink；级别用 `std::atomic` 保存

CMake：取消 `CORE_SRCS` 中 `# M3` 行注释（`log_msg.cc log_handle.cc`）。

验证 `tests/log_handle_test.cc`（写一个记录收到消息的 `MockSink`）：

- 默认级别为 `kInfo`：`kDebug` 被丢弃，`kInfo`/`kError` 被分发
- `SetLevel(kTrace)` 后所有级别都能通过，`SetLevel(kOff)` 后全部丢弃
- 两个 sink 都收到同一条消息，消息内容、级别、行号原样传递
- 空 sink 列表时调用 `Log` 不崩溃

### M4 控制台输出 `feature/m4-console-sink`

文件：`logger/formatter/default_formatter.h/.cc`、`logger/sinks/console_sink.h/.cc`

- `DefaultFormatter` 输出格式：`[2024-01-01 12:00:00] [I] [file.cc:123] [pid:tid] message`
- `ConsoleSink`：默认使用 `DefaultFormatter`，可通过 `SetFormatter` 替换，输出到 stdout

CMake：取消 `FORMATTER_SRCS` 和 `SINK_SRCS` 中 `# M4` 行注释。

验证 `tests/console_sink_test.cc`：

- `DefaultFormatter` 的输出包含 `[I]`、`file.cc:123`、当前进程 ID 和原消息
- 每个级别映射到正确的字母 `T D I W E F`
- 用 `testing::internal::CaptureStdout()` 捕获 `ConsoleSink` 输出并断言内容
- `SetFormatter` 换成自定义 formatter 后输出随之改变

### M5 宏 API `feature/m5-macro-api`

文件：`logger/log_variadic_handle.h`、`logger/log_factory.h/.cc`、`logger/logger.h`；示例 `example/macro_logger_example.cc`

- `VariadicLogHandle`：模板 `Log(loc, lvl, fmt::format_string<Args...>, Args&&...)`，**先判断级别再格式化**，用 `fmt::basic_memory_buffer<char, 256>` 避免堆分配
- `LogFactory`：单例，保存全局 `VariadicLogHandle`
- `logger.h`：`EXT_LOGGER_INIT(handle)`、`EXT_LOG_TRACE…EXT_LOG_CRITICAL`、`LOG_LOGGER_xxx(handle, ...)`，受 `LOGGER_ACTIVE_LEVEL` 编译期裁剪

CMake：取消 `CORE_SRCS` 中 `# M5` 行注释（`log_factory.cc`）。示例会被自动构建。

验证 `tests/macro_api_test.cc`：

- `EXT_LOG_INFO("a={} b={}", 1, "x")` 经 MockSink 收到 `"a=1 b=x"`，且 `file_name`/`line`/`func_name` 正确
- 级别被过滤时参数**不会被格式化**（传入一个格式化时会计数的自定义类型来断言）
- 未调用 `EXT_LOGGER_INIT` 时（handle 为空）调用宏不崩溃
- 运行 `macro_logger_example` 能看到控制台输出

### M6 任务调度 `feature/m6-context`

文件：`logger/context/thread_pool.h/.cc`、`executor.h/.cc`、`context.h/.cc`

- `ThreadPool`：`Start/Stop`、`RunTask`、`RunRetTask`（返回 `shared_ptr<future>`）
- `Executor`：按 `TaskRunnerTag` 管理单线程 runner（保证同一 runner 内任务串行）；`ExecutorTimer` 用优先队列实现 `PostDelayedTask`、`PostRepeatedTask`、`CancelRepeatedTask`
- `Context`：全局单例 + `NEW_TASK_RUNNER`、`POST_TASK`、`WAIT_TASK_IDLE`、`POST_REPEATED_TASK` 宏

CMake：取消 `CONTEXT_SRCS` 中 `# M6` 行注释。

验证 `tests/context_test.cc`（避免依赖精确时间，用宽松上下限）：

- `RunRetTask` 返回值正确；1000 个任务全部执行（原子计数）
- 同一 runner 上的任务按投递顺序串行执行
- 申请已存在的 tag 时返回一个新的、不冲突的 tag
- `PostDelayedTask(50ms)`：50ms 前未执行，500ms 内执行
- `PostRepeatedTask(10ms, 5)` 最终恰好执行 5 次；`CancelRepeatedTask` 后不再执行
- `Stop()` 后 `RunTask` 不再接受任务，析构不死锁

### M7 mmap 缓存 `feature/m7-mmap`

文件：`logger/mmap/mmap_aux.h/.cc`、`mmap_win.cc`、`mmap_linux.cc`

- 文件头 `MmapHeader{magic, size}`，打开已有文件时 magic 正确则**保留旧数据**（进程崩溃后日志不丢的关键）
- 默认容量 512KB，按页对齐；`Push` 时自动扩容
- `GetRatio()` = 已用 / 可用容量，`EffectiveSink` 据此判断何时落盘

CMake：取消 `MMAP_SRCS` 中 `# M7` 行注释。

验证 `tests/mmap_test.cc`（使用临时目录，测试结束删除）：

- 新文件 `Size()==0`、`Empty()`；`Push` 后 `Size` 与 `Data()` 内容正确
- 析构后用同一路径重新打开，数据仍在（持久化）
- `Push` 超过 512KB 的数据自动扩容且内容完整
- `Clear()` 后 `Size()==0`；`GetRatio()` 在 0~1 之间并随写入增长

### M8 压缩 `feature/m8-compress`

文件：`logger/compress/compress.h`、`zstd_compress.h/.cc`、`zlib_compress.h/.cc`；示例 `example/zstd_example.cc`

- `Compression` 接口：`Compress`、`CompressedBound`、`Decompress`、`ResetStream`
- zstd 使用**流式压缩**（`ZSTD_compressStream2` + `ZSTD_e_flush`），同一个 chunk 内的多条日志共享字典上下文，压缩率更高；解压端遇到 zstd 帧头时重置解压上下文

CMake：取消 `COMPRESS_SRCS` 中 `# M8` 行注释。

验证 `tests/compress_test.cc`（zstd 与 zlib 各一组）：

- 单条数据压缩后解压与原文一致
- `ResetStream` 后连续压缩 N 条，按同样顺序逐条解压，每条都还原
- 重复度高的数据压缩后明显变小
- 空输入返回 0 / 空串，不崩溃
- 超过 10KB 的单条数据也能正确往返（参考实现在这里有缺陷，见第 7 节）

### M9 加密 `feature/m9-crypt`

文件：`logger/crypt/crypt.h/.cc`、`aes_crypt.h/.cc`；示例 `example/crypt_example.cc`

- `GenECDHKey()` 返回 `(私钥, 公钥)` 二进制；`GenECDHSharedSecret(私钥, 对方公钥)`
- `BinaryKeyToHex` / `HexKeyToBinary`
- `AESCrypt`：用共享密钥构造，`Encrypt` / `Decrypt`

CMake：取消 `CRYPT_SRCS` 中 `# M9` 行注释。

验证 `tests/crypt_test.cc`：

- 双方各自生成密钥对，`Shared(a_pri, b_pub) == Shared(b_pri, a_pub)`
- 公钥长度为 65 字节（未压缩点格式，`ChunkHeader` 依赖此长度）
- Hex 往返一致，且 Hex 长度是二进制的 2 倍
- AES 加密后与明文不同，解密后与明文一致；用错误密钥解密得不到原文
- 用参考实现中的固定密钥对（见 `../logger-tutorial/example/logger_example.cc`）能算出确定的共享密钥

### M10 二进制格式化 `feature/m10-effective-formatter`

文件：`logger/proto/effective_msg.proto`、`logger/formatter/effective_formatter.h/.cc`

- `EffectiveMsg`：level、timestamp(ms)、pid、tid、line、file_name、func_name、log_info；`optimize_for = LITE_RUNTIME`
- `EffectiveFormatter::Format` 把 `LogMsg` 序列化为 protobuf 二进制写入 `dest`

CMake：取消 `FORMATTER_SRCS` 和 `PROTO_SRCS` 中 `# M10` 行注释。构建脚本会自动调用 protoc。

验证 `tests/effective_formatter_test.cc`：

- `Format` 后用 `EffectiveMsg::ParseFromString` 反序列化，各字段与输入一致
- `pid` 等于当前进程 ID，`timestamp` 与当前时间相差不超过数秒
- 中文、空消息、超长消息都能往返

### M11 高性能落盘 `feature/m11-effective-sink`

文件：`logger/sinks/effective_sink.h/.cc`；示例 `example/logger_example.cc`

数据流：`LogMsg → EffectiveFormatter → zstd → AES → [ItemHeader + 数据] 写入 master mmap`；master 使用率 > 80% 时与 slave 交换，后台 runner 把 slave 写成 `[ChunkHeader(含客户端公钥) + 数据]` 追加到日志文件。

- 文件名 `{prefix}_{YYYYmmddHHMMSS}.log`，超过 `single_size` 切新文件
- 定时任务按 `interval` 执行淘汰，总大小超过 `total_size` 时删除最旧的文件
- 启动时若 master/slave 缓存非空（上次崩溃残留），先补写到文件

CMake：取消 `SINK_SRCS` 中 `# M11` 行注释。

验证 `tests/effective_sink_test.cc`（目录用临时目录，公钥用 M9 现生成的密钥对）：

- 写 1000 条后 `Flush()`，目录下生成 `.log` 文件，文件开头 8 字节等于 `ChunkHeader::kMagic`
- 遍历文件，所有 chunk 和 item 的 magic 都正确，item 数量等于写入条数
- `single_size` 设小（如 1MB）并写入大量数据后，会生成多个文件
- 模拟崩溃：写入后不 Flush 直接构造新的 `EffectiveSink`，残留数据被补写到文件
- 运行 `logger_example`（先把输出目录改成相对路径或临时目录）

### M12 解码器 `feature/m12-decoder`

文件：`decode/decode.cc`、`decode/decode_formatter.h/.cc`

- `DecodeFormatter`：支持 pattern `%l %D %S %M %p %t %# %F %f %v`，如 `[%l][%D:%S][%p:%t][%F:%f:%#]%v`
- `decoder <log_file> <server_pri_key_hex> <output_file>`：逐 chunk 读取客户端公钥 → ECDH 得到共享密钥 → 逐 item 解密、解压、反序列化、格式化

CMake：`decode/CMakeLists.txt` 在 `decode.cc` 存在时自动构建。为了让解码逻辑可测试，建议在本模块把 `decode_formatter.cc` 和解码函数拆成静态库 `decoder_core`，`decoder` 和测试都链接它（需要在 `tests/CMakeLists.txt` 里为该测试额外链接 `decoder_core`）。

验证 `tests/decoder_test.cc`（端到端）：

- 生成服务端密钥对 → 用服务端公钥配置 `EffectiveSink` 写入 N 条已知内容 → `Flush` → 用服务端私钥解码 → 解码文本中按顺序包含全部 N 条原文
- 用错误私钥解码时报错退出，而不是输出乱码
- `DecodeFormatter` 各占位符输出正确
- 手动：运行 `logger_example` 后用 `decoder` 解码，打开输出文件检查

## 5. Git 与 PR 工作流

分支：

- `main`：始终保持可构建、测试全绿，不直接在上面提交
- 模块分支：`feature/mN-<名字>`（见第 4 节表格）；修复用 `fix/<描述>`，其他杂项用 `chore/<描述>`

每个模块的流程：

```powershell
git switch main
git pull
git switch -c feature/m1-common
# ……开发、编写测试……
python script/build_windows.py --test
git add -A
git commit -m "feat(common): add LogLevel, SourceLocation, space and defer"
git push -u origin feature/m1-common
# 在 GitHub 网页上开 PR（或安装 gh 后：gh pr create --base main --fill）
```

提交信息使用 Conventional Commits：`feat(模块): ...`、`test(模块): ...`、`fix(模块): ...`、`refactor(模块): ...`、`build: ...`、`docs: ...`。

PR 合并标准（与 `.github/pull_request_template.md` 一致）：

- 只包含本模块的改动，不顺手改其他模块
- `python script/build_windows.py --test` 在本地通过（贴出 ctest 汇总行）
- 新增代码有对应测试，覆盖第 4 节列出的验证要点
- 已执行 clang-format
- 推荐使用 Squash merge，合并后删除远端分支，本地 `git switch main && git pull`

## 6. 编码规范

- C++17；`#pragma once`；所有代码在 `namespace logger`（子模块用 `logger::ctx`、`logger::crypt`、`logger::compress`、`logger::fs`）
- 格式：根目录 `.clang-format`（Chromium 风格，列宽 120）
- 命名：类型和函数 `PascalCase`；变量 `snake_case`；成员变量尾随下划线 `level_`；私有辅助函数尾随下划线 `ShouldLog_()`；常量 `kName`；宏全大写
- include 路径相对 `logger/` 目录：`#include "sinks/sink.h"`；在 example、decode、tests 中用 `#include "logger/sinks/sink.h"`
- 平台相关代码放在 `*_win.cc` / `*_linux.cc`，由 CMake 按平台选择，不在通用代码里写大段 `#ifdef`
- 热路径（`Log` 调用链）避免堆分配和不必要的拷贝；跨线程共享状态必须有锁或原子变量

## 7. 参考实现中的已知问题

以下问题存在于 `../logger-tutorial`，重写时可以修正，并在 PR 描述里说明：

- `ZstdCompress::Decompress` 和 `EffectiveSink::Log` 在 `std::string::reserve` 之后直接往 `data()` 写数据，超出 `size()` 属于未定义行为，且解压输出固定上限 10KB，超长日志会被截断。应改为 `resize` 或循环扩容
- `ThreadPool` / `Executor` 使用 `std::result_of_t`，在 C++17 已弃用、C++20 移除，可改用 `std::invoke_result_t`
- `TimerCount` 析构时计算了耗时却没有输出，`TIMER_COUNT` 宏被定义为空
- `ExecutorContext::GetNextRunnerTag` 使用函数内 static 计数器，多个 `Executor` 实例会共享
- `GetTaskRunner` 找不到 tag 时返回 `nullptr`，`PostTask` 没有判空
- `example/logger_example.cc` 把输出目录写死为 `D:/logger`

## 8. 给 AI 助手的协作规则

- **开发者本人在学习，代码由开发者写。** 默认只做讲解、设计评审、代码审查、写测试建议和排查问题；只有开发者明确要求时才直接写模块实现代码。
- 参考实现 `../logger-tutorial` 只读，可以查阅并引用其设计，但不要整文件复制到本仓库。
- 一次只处理当前分支对应的模块；发现其他模块的问题时记录下来告诉开发者，不要顺手修改。
- 不要修改 `logger/third_party/` 和 `script/bin/`。
- 声称"完成"或"通过"之前，必须实际运行 `python script/build_windows.py --test` 并确认退出码为 0；不要只凭推断。
- 不要直接向 `main` 提交或强推；不要在未经确认的情况下合并 PR。
- 新增 `.cc` 时记得同步 `logger/CMakeLists.txt`；新增测试文件命名为 `tests/<模块>_test.cc`。
