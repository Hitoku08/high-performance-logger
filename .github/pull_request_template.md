## 模块

<!-- 例如：M3 日志核心 feature/m3-log-handle -->

## 改动内容

-

## 验证

- [ ] `python script/build_windows.py --test` 本地通过
- [ ] 新增/更新了 `tests/<模块>_test.cc`，覆盖 AGENTS.md 中该模块的验证要点
- [ ] 新增的 `.cc` 已在 `logger/CMakeLists.txt` 中登记
- [ ] 已执行 clang-format
- [ ] 只包含本模块的改动

ctest 汇总：

```
100% tests passed, 0 tests failed out of N
```

## 与参考实现的差异 / 已知问题

<!-- 如果修正了 AGENTS.md 第 7 节列出的问题，或有意偏离参考实现，在这里说明 -->
