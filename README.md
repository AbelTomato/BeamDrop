# BeamDrop / 邻光

BeamDrop（邻光）是一个面向局域网的轻量文件传输工具，聚焦在同一局域网内通过命令行完成可靠的文件投递。

## 项目定位

BeamDrop 不以“复刻 AirDrop”为目标，而是提供一个清晰、可维护的局域网文件传输核心：

- 单文件发送
- 多路径发送
- 文件夹递归发送
- TCP 传输
- 项目内 Packet 协议
- 文件内容分块传输
- SHA256 校验

## 技术栈

- C++20
- CMake
- MinGW / GCC
- nlohmann/json
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

Windows / MinGW：

```bash
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw
```

运行测试：

```bash
ctest --test-dir build-mingw --output-on-failure
```

## 使用与配置

### 启动接收端

```bash
build-mingw\beamdrop.exe serve --host 127.0.0.1 --port 9090 --save-dir received
```

参数：

| 参数         | 默认值     | 说明         |
| ------------ | ---------- | ------------ |
| `--host`     | `0.0.0.0`  | 监听地址     |
| `--port`     | `9090`     | 监听端口     |
| `--save-dir` | `received` | 文件保存目录 |

### 发送单文件

```bash
build-mingw\beamdrop.exe send .\README.md --to 127.0.0.1:9090
```

### 发送目录

```bash
build-mingw\beamdrop.exe send .\docs --to 127.0.0.1:9090
```

### 一次发送多个路径

```bash
build-mingw\beamdrop.exe send .\README.md .\docs --to 127.0.0.1:9090
```

### 配置文件示例

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
