# BeamDrop / 邻光

BeamDrop（邻光）是一个面向局域网的轻量文件传输工具，聚焦在同一局域网内通过命令行完成可靠的文件投递。

当前状态：C++ CLI 局域网文件传输 MVP 已完成。当前版本支持在局域网内通过命令行发送单文件、多文件和目录，并包含分块传输、断点续传、SHA256 校验、配置加载、基础进度输出和文件日志。

## 项目定位

BeamDrop 不以“复刻 AirDrop”为目标，而是提供一个清晰、可维护的局域网文件传输核心：

- 单文件发送
- 多路径发送
- 文件夹递归发送
- TCP 传输
- 项目内 Packet 协议
- HELLO manifest 会话头
- FILE_INFO JSON payload
- RESUME_ACK offset 协商
- 文件内容分块传输
- 断点续传状态文件
- SHA256 校验

## 技术栈

- C++20
- CMake
- MinGW / GCC
- 项目内 SHA256 实现
- CTest

## 目录结构

```text
src/
  app/
  cli/
  network/
  protocol/
  transfer/
  filesystem/
  config/
  logger/
  utils/

include/beamdrop/
  app/
  cli/
  network/
  protocol/
  transfer/
  filesystem/
  config/
  logger/
  utils/

tests/
  unit/
  integration/

docs/
config/
scripts/
cmake/
```

## 构建

推荐使用 `CMakePresets.json` 固化平台构建目录和选项。

### Windows / MinGW

```bash
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
```

运行测试：

```bash
ctest --preset windows-mingw-debug
```

旧命令仍可使用：

```bash
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```

### Linux / GCC

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
```

运行测试：

```bash
ctest --preset linux-gcc-debug
```

旧命令仍可使用：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

### 构建选项

| 选项                    | 默认值 | 说明                         |
| ----------------------- | ------ | ---------------------------- |
| `BEAMDROP_BUILD_CLI`    | `ON`   | 构建 `beamdrop` CLI          |
| `BEAMDROP_BUILD_TESTS`  | `ON`   | 构建 CTest 测试 suite        |
| `BEAMDROP_BUILD_PYTHON` | `OFF`  | 构建后续 pybind11 native 模块 |

当前 CTest 由两个测试可执行文件组成：`beamdrop_unit_tests` 和 `beamdrop_integration_tests`。各测试源文件仍保持原有独立 `main()` 形式，但在 CMake 中编译为 object 并由 suite runner 调用。这样在 `beamdrop_core` 变化时只需要重链接 CLI、unit suite 和 integration suite，避免每个测试目标重复链接。

新增或修改被广泛包含的头文件仍会触发相关 `.cpp` 重编译，这是 C++ 依赖模型的正常行为。若后续头文件改动频繁，可继续通过减少头文件包含、前置声明、PImpl、预编译头或 `ccache` / `sccache` 降低重编译成本；不要通过隐藏头文件依赖来规避正确的增量构建。

## 使用与配置

以下示例按平台区分：

- Windows 使用 `build-mingw\beamdrop.exe` 和反斜杠路径。
- Linux 使用 `./build/beamdrop` 和正斜杠路径。

### 启动接收端

Windows：

```bash
build-mingw\beamdrop.exe serve --config config\beamdrop.example.json --host 127.0.0.1 --port 9090 --save-dir received --log-file logs\beamdrop.log
```

Linux：

```bash
./build/beamdrop serve --config config/beamdrop.example.json --host 127.0.0.1 --port 9090 --save-dir received --log-file logs/beamdrop.log
```

`serve` 启动后会持续监听，可连续接收多次 `send`。当前退出方式为 Ctrl+C 或终止进程。

参数：

| 参数         | 默认值              | 说明                                   |
| ------------ | ------------------- | -------------------------------------- |
| `--host`     | `0.0.0.0`           | 监听地址                               |
| `--port`     | `9090`              | 监听端口                               |
| `--save-dir` | `received`          | 文件保存目录                           |
| `--log-file` | `logs/beamdrop.log` | 日志文件路径                           |
| `--config`   | 无                  | 配置文件路径，可读取续传配置和日志配置 |

### 生成和查看配置

生成默认配置：

Windows：

```bash
build-mingw\beamdrop.exe config init --path config\beamdrop.json
```

Linux：

```bash
./build/beamdrop config init --path config/beamdrop.json
```

查看配置：

Windows：

```bash
build-mingw\beamdrop.exe config show --path config\beamdrop.json
```

Linux：

```bash
./build/beamdrop config show --path config/beamdrop.json
```

`config init` 会创建父目录并写入默认配置；`config show` 会读取指定配置文件并输出规范化后的 JSON。

### 发送单文件

Windows：

```bash
build-mingw\beamdrop.exe send .\README.md --to 127.0.0.1:9090 --config config\beamdrop.example.json --chunk-size 65536 --log-file logs\beamdrop.log
```

Linux：

```bash
./build/beamdrop send ./README.md --to 127.0.0.1:9090 --config config/beamdrop.example.json --chunk-size 65536 --log-file logs/beamdrop.log
```

### 发送目录

Windows：

```bash
build-mingw\beamdrop.exe send .\docs --to 127.0.0.1:9090
```

Linux：

```bash
./build/beamdrop send ./docs --to 127.0.0.1:9090
```

### 一次发送多个路径

Windows：

```bash
build-mingw\beamdrop.exe send .\README.md .\docs --to 127.0.0.1:9090
```

Linux：

```bash
./build/beamdrop send ./README.md ./docs --to 127.0.0.1:9090
```

### 配置文件示例

当前 `serve` / `send` 已支持通过 `--config` 读取配置文件。命令行显式参数会覆盖已加载配置；`send --chunk-size` 会覆盖 `transfer.chunk_size`。

`serve` 会读取 `transfer.enable_resume` 和 `transfer.state_file`。开启续传后，接收端会根据状态文件和本地已存在文件计算可信 offset；发送端收到 `RESUME_ACK` 后从该 offset 继续发送。状态记录绑定 `relative_path + size + sha256`，传输完成且校验通过后会清理对应记录。

当前传输会话以 `HELLO` packet 开始，payload 使用 `beamdrop-manifest-v1` 文本 manifest，包含文件数量和总字节数；随后每个文件统一使用 `FILE_INFO -> RESUME_ACK -> DATA -> FILE_END`，最后用 `FINISH` 结束。

示例配置位于：

```text
config/beamdrop.example.json
```

内容示例：

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 9090,
    "save_dir": "./received"
  },
  "transfer": {
    "chunk_size": 1048576,
    "enable_resume": true,
    "verify_sha256": true,
    "state_file": "./transfer_state.json"
  },
  "log": {
    "level": "info",
    "file": "./logs/beamdrop.log"
  }
}
```

## 设计文档

核心设计文档位于 `docs/`。
