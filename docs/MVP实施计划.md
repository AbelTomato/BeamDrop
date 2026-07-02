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

- `ResumeAckCodec`，支持 `RESUME_ACK` payload 编解码；
- `ResumeManager`，支持 `transfer_state.json` 读取、写入、更新、查询和完成清理；
- Receiver 在 `FILE_INFO` 后计算可信 offset 并回复 `RESUME_ACK`；
- Sender 在 `FILE_INFO` 后读取 `RESUME_ACK`，从 offset 继续发送；
- CLI `serve` 读取 `transfer.enable_resume` 和 `transfer.state_file`；
- 断点续传端到端测试。

验证：

- `resume_ack_codec_tests` 覆盖 ACK 编解码和非法 payload；
- `resume_manager_tests` 覆盖状态读写、offset 更新、完成清理和越界 offset 策略；
- `single_file_transfer_tests` 覆盖 offset 大于 0 时发送端不重发前置字节；
- `directory_transfer_tests` 覆盖多文件常规 `FILE_INFO -> RESUME_ACK -> DATA` 流程；
- `resume_transfer_tests` 预置接收端部分文件和状态记录，验证再次发送完整源文件后最终内容和 SHA256 一致，且状态记录被清理；
- 全量 CTest 通过。

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
