# MVP 实施计划

## 1. 目标

完成 BeamDrop C++ CLI MVP，实现局域网 TCP 文件传输、文件夹递归发送、进度显示、断点续传、SHA256 校验、配置和日志。

## 2. 实施切片

### Step 1：工程初始化

交付：

- CMake 项目骨架；
- `src/`、`include/`、`tests/`、`docs/` 目录；
- README；
- 配置示例；
- CI 占位。

验证：

- CMake 能配置并构建占位程序。

### Step 2：协议层

交付：

- `PacketHeader`；
- `PacketType`；
- `Packet`；
- `Serializer`；
- `PacketParser`。

单元测试：

- header 编解码；
- magic 校验；
- version 校验；
- length 校验；
- checksum 校验。

### Step 3：网络层

交付：

- `TcpServer`；
- `TcpClient`；
- 阻塞式读写 MVP。

集成测试：

- client 连接 server；
- 发送 HELLO；
- server 能解析 packet。

### Step 4：单文件传输

交付：

- `Sender`；
- `Receiver`；
- 文件 SHA256；
- 保存目录处理。

验证：

- 发送单文件后，接收文件内容一致；
- SHA256 一致。

### Step 5：多文件与文件夹

交付：

- `FileScanner`；
- 相对路径生成；
- 目录递归；
- 接收端目录创建。

验证：

- 发送目录后，接收目录结构一致；
- 多文件 hash 全部一致。

### Step 6：进度与日志

交付：

- `ProgressReporter`；
- `Logger`；
- 控制台进度；
- 文件日志。

验证：

- 传输过程中显示速度、当前文件进度、总进度；
- 日志文件记录传输开始、结束、错误。

### Step 7：断点续传

交付：

- `ResumeManager`；
- `transfer_state.json`；
- `RESUME_REQ` / `RESUME_ACK` 流程。

验证：

- 中断大文件传输；
- 再次发送时从已有 offset 继续；
- 最终 hash 一致。

### Step 8：文档回填

交付：

- README 使用说明；
- 协议设计更新；
- CLI 示例更新；
- 测试说明。

## 3. 验收标准

MVP 通过条件：

1. `beamdrop serve` 可启动接收端；
2. `beamdrop send` 可发送文件和目录；
3. 文件内容和 SHA256 校验一致；
4. 中断后可续传；
5. 有日志；
6. 有配置文件；
7. 有单元测试和端到端测试；
8. 文档能支撑演示和答辩。
