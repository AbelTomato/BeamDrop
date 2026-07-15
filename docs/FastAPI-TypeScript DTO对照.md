# FastAPI / TypeScript DTO 对照（Slice 3）

状态：前端领域契约已冻结为 v1；Slice 5 已实现 Pydantic REST DTO 与 snake_case JSON wire format。WebSocket envelope 的实际传输和 adapter 解析仍未实现。

命名规则：JSON DTO 使用 `snake_case`，前端领域模型使用 `camelCase`。`FastApiAdapter` 在 Slice 7 负责命名转换、结构校验和错误归一化；React 功能层不得感知 JSON wire format。

## 请求与快照

| 语义 | FastAPI JSON 字段 | TypeScript 字段 | 可空性 / 枚举 |
| --- | --- | --- | --- |
| 发送源路径 | `source_path` | `SendRequest.sourcePath` | 非空字符串 |
| 目标主机 | `target_host` | `SendRequest.targetHost` | 非空字符串 |
| 目标端口 | `target_port` | `SendRequest.targetPort` | 整数，1–65535 |
| 分块大小 | `chunk_size` | `SendRequest.chunkSize` | 可选正整数 |
| 接收监听主机 | `host` | `StartReceiverRequest.host` | 非空字符串 |
| 接收监听端口 | `port` | `StartReceiverRequest.port` | 整数，1–65535 |
| 接收保存目录 | `save_dir` | `StartReceiverRequest.saveDir` | 非空字符串 |
| 创建任务 ID | `task_id` | `TaskCreated.taskId` | 非空字符串 |
| 创建时间 | `created_at` | `TaskCreated.createdAt` | ISO-8601 字符串 |
| 任务状态 | `state` | `TaskSnapshot.state` | `pending` / `running` / `completed` / `failed` |
| 任务开始、完成时间 | `started_at`、`finished_at` | `startedAt`、`finishedAt` | 可为 `null` |
| 任务进度 | `progress` | `TaskSnapshot.progress` | 可为 `null` |
| 任务错误 | `error` | `TaskSnapshot.error` | 可为 `null` |
| 接收服务状态 | `state` | `ReceiverSnapshot.state` | `stopped` / `starting` / `running` / `stopping` / `failed` |
| 接收地址、端口、目录 | `host`、`port`、`save_dir` | `host`、`port`、`saveDir` | 未启动时均为 `null` |
| 接收服务错误 | `last_error` | `lastError` | 可为 `null` |
| 快照任务列表 | `tasks` | `AppSnapshot.tasks` | 数组，允许为空 |
| 快照采样时间 | `snapshot_at` | `AppSnapshot.snapshotAt` | ISO-8601 字符串 |

## 进度与错误

| 语义 | FastAPI JSON 字段 | TypeScript 字段 | 枚举 / 约束 |
| --- | --- | --- | --- |
| 传输方向 | `direction` | `TransferProgress.direction` | `send` / `receive` |
| 阶段 | `stage` | `TransferProgress.stage` | `task_started` / `transferring` / `file_completed` / `task_completed` |
| 文件位置 | `file_index`、`file_count` | `fileIndex`、`fileCount` | 非负整数；index 从 1 开始 |
| 路径 | `relative_path` | `relativePath` | 字符串 |
| 当前文件字节 | `current_file_bytes`、`current_file_total_bytes` | `currentFileBytes`、`currentFileTotalBytes` | 非负整数 |
| 任务总字节 | `total_bytes`、`transferred_bytes` | `totalBytes`、`transferredBytes` | 非负整数 |
| 结构化错误 | `code`、`message`、`details` | `ErrorPayload` | `details` 可缺省；不得要求上层解析日志文本 |

## WebSocket envelope v1

| FastAPI JSON 字段 | TypeScript 字段 | 约束 |
| --- | --- | --- |
| `schema_version` | `schemaVersion` | 必须为 `1`；其他版本由 adapter 抛出 `INCOMPATIBLE_EVENT_SCHEMA` |
| `event_id` | `eventId` | 非空字符串 |
| `sequence` | `sequence` | 非负、单调递增整数 |
| `type` | `type` | 已知事件或未来未知字符串 |
| `task_id` | `taskId` | 任务事件为字符串，其余事件为 `null` |
| `timestamp` | `timestamp` | ISO-8601 字符串 |
| `payload` | `payload` | 按事件类型决定；未知事件保留原 payload，页面可忽略或记录 |

已知事件：`task.created`、`task.started`、`transfer.progress`、`task.completed`、`task.failed`、`receiver.started`、`receiver.stopped`、`error`、`heartbeat`。

## 后端落地要求

- Slice 5 已按此表实现 Pydantic DTO 与 REST route 测试；`StartReceiverRequest.state_file` 是后端从 `save_dir` 派生的内部字段，不出现在 wire DTO。
- Slice 7 在实际实现 REST/WS 前必须补充 wire DTO 到本领域模型的运行时解析测试。
- REST 快照是权威状态；WebSocket 只能提供增量事件，不能作为恢复状态的唯一来源。