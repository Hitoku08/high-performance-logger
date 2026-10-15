# CLAUDE.md

本仓库的完整说明（项目背景、构建验证、M0–M12 开发路线、Git/PR 流程、编码规范、协作规则）在 AGENTS.md 中，开始任何工作前先阅读它：

@AGENTS.md

## 最重要的几条

- 这是一个**从零重写 C++17 日志库的学习项目**，代码由开发者本人写。默认只讲解、评审、给测试建议、排查问题；开发者明确要求时才直接写实现。
- 参考实现在 `../logger-tutorial`，只读，不要整文件复制。
- 每个模块一个分支 `feature/mN-<名字>`，完成后开 PR 合并到 `main`；不要直接提交到 `main`，不要擅自合并 PR。
- 验证命令（Windows）：`python script/build_windows.py --test`，Linux 用 `build_linux.py`。退出码为 0 才算通过，必须实际运行后再下结论。
- 新增 `.cc` 要在 `logger/CMakeLists.txt` 中取消对应 `# Mx` 行注释；测试文件放在 `tests/<模块>_test.cc`，会被自动收录。
- 不要修改 `logger/third_party/` 和 `script/bin/`。
- 代码风格遵循根目录 `.clang-format`（Chromium，列宽 120）和 AGENTS.md 第 6 节的命名规则。

## 常用命令

```powershell
python script/build_windows.py --test            # 构建并运行全部测试
python script/build_windows.py --clean --test    # 清空后全量构建
cd build/x64-Debug; ctest -C Debug -R <名字> --output-on-failure   # 只跑部分测试
git switch -c feature/mN-<名字>                   # 开始一个新模块
```
