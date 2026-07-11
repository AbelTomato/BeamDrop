# GUI 界面开发实施计划

## 1. 目标与约束

### 1.1 近期目标：大作业 GUI

在现有 C++ CLI MVP 和 app service 基础上，完成一个可展示的本机 GUI 控制面。GUI 第一阶段采用：

```text
React + TypeScript
        |
   BeamDropApi 抽象
        |
 FastApiAdapter（本期实际实现）
        |
REST + WebSocket
        |
FastAPI TaskManager / EventBus
        |
pybind11
        |
C++ beamdrop_core / app service
```

本期展示目标：

- 启动和停止接收服务。
- 创建单文件发送任务。
- 展示接收服务状态、任务状态和实时传输进度。
- 页面刷新或 WebSocket 重连后，通过 REST 快照恢复权威状态。
- 完成 Windows 端到端演示。
- 保持 Windows / Linux 构建边界清晰，不引入 Windows 专属业务逻辑。

### 1.2 长期目标

长期不把 FastAPI + React 作为唯一桌面终态，而是采用“双宿主、同一前端领域层”的路线：

```text
React pages/components/store
                |
        BeamDropApi interface
                |
          TauriAdapter
                |
     Tauri command + event
                |
         Rust safe wrapper
                |
         stable C ABI
                |
beamdrop_core + app service
```

长期目标：

- React 页面可从 FastAPI 宿主迁移至 Tauri v2，不重写页面、store 和领域模型。
- FastAPI 保留为浏览器控制面、自动化 API 和未来 Agent 编排入口。
- 正式桌面客户端最终通过 Tauri / Rust / C ABI 直接调用 C++ core，不要求 Python 后端常驻。
- 后续扩展 macOS；移动端单独处理沙盒文件、局域网权限和后台生命周期。

### 1.3 本期非目标

以下内容不进入本周大作业 GUI 交付范围：

- Tauri 业务接入、Rust-C++ FFI、C ABI。
- 托盘、自启动、安装器、自动更新。
- UDP 自动发现、多客户端并发接收。
- 完整任务持久化和历史数据库。
- Android / iOS 构建。
- 在 C++ 取消链路未闭环前提供虚假的“取消成功”。
- 浏览器上传文件到本机后端再发送。

## 2. 模块边界

### 2.1 本期模块结构

```text
frontend/
  React pages/components/store
                |
        BeamDropApi interface
                |
         FastApiAdapter
                |
       REST + WebSocket
                |
backend/
  API schemas / TaskManager / EventBus / NativeGateway
                |
bindings/python/
  pybind11 thin adapter
                |
beamdrop_core + app service
```

### 2.2 依赖规则

1. React 页面、组件和 store 只能依赖 `BeamDropApi` 和共享领域类型。
2. `fetch`、WebSocket 和 Tauri API 只能出现在对应 adapter 中。
3. FastAPI API 层不能直接调用 pybind，必须经过 `NativeGateway`。
4. pybind 只适配 app service，不绑定裸 `Sender`、`Receiver`、socket 或协议对象。
5. C++ core 不感知 HTTP、WebSocket、Python、Rust、Tauri 和前端任务 ID。
6. REST 快照是权威状态；WebSocket 只负责低延迟增量事件。
7. 传输协议、续传格式和 SHA256 校验逻辑只属于 C++ core。

### 2.3 CMake 与多平台构建策略

当前 CMake target 边界按宿主分层，而不是按平台复制业务逻辑：

```text
beamdrop_core (STATIC, PIC, C++ app service / transfer / protocol)
  ├─ beamdrop CLI
  ├─ beamdrop_unit_tests / beamdrop_integration_tests
  ├─ beamdrop_native (pybind11，本期后续切片)
  └─ beamdrop_c_api (稳定 C ABI，Tauri / Rust 路线后续切片)
```

约束：

- `beamdrop_core` 继续保持不感知 HTTP、WebSocket、Python、Rust、Tauri 和前端任务 ID。
- `beamdrop_core` 使用静态库并开启 `POSITION_INDEPENDENT_CODE`，保证 Linux 下可被 pybind11 `MODULE` 复用。
- Windows socket 依赖只通过 CMake 平台分支链接 `ws2_32`，不要把平台库链接散落到各宿主 target。
- Python binding 与未来 Tauri C ABI 是并列宿主适配层；Tauri 不依赖 pybind，FastAPI 不成为桌面客户端的必需常驻后端。
- C ABI 共享库必须单独定义导出符号、句柄生命周期、错误码和内存释放规则，不直接把 C++ 类 ABI 暴露给 Rust。
- macOS 后续通过 preset/toolchain、签名、公证和本地网络权限处理；不要在 core 业务代码中提前写 macOS 特例。
- Android / iOS 后续需要新的文件源抽象，不能把桌面 `std::filesystem::path` 直接视为移动端 API。

构建选项：

| 选项                    | 默认值 | 用途                         |
| ----------------------- | ------ | ---------------------------- |
| `BEAMDROP_BUILD_CLI`    | `ON`   | 构建 CLI                     |
| `BEAMDROP_BUILD_TESTS`  | `ON`   | 构建 CTest 测试 suite        |
| `BEAMDROP_BUILD_PYTHON` | `OFF`  | 构建 pybind11 native binding |

增量构建策略：测试目标已合并为 unit / integration 两个 suite，避免 `beamdrop_core` 更新后每个测试可执行文件重复链接。头文件变更导致依赖 `.cpp` 重编译是正确行为；后续优化重点是降低头文件传播范围、使用前置声明或 PImpl、引入编译缓存，而不是隐藏依赖关系。

### 2.4 建议目录规划

```text
bindings/
  python/
    CMakeLists.txt
    beamdrop_pybind.cpp
    tests/

backend/
  pyproject.toml
  app/
    main.py
    api/
    core/
      native_gateway.py
      task_manager.py
      receiver_manager.py
      event_bus.py
    schemas/
    websocket/
  tests/

frontend/
  package.json
  src/
    app/
    components/
    features/
      send/
      receiver/
      tasks/
    platform/
      BeamDropApi.ts
      createBeamDropApi.ts
      fastapi/FastApiAdapter.ts
      tauri/TauriAdapter.ts
    shared/
      contracts.ts
      errors.ts

desktop/                 # 可选：Tauri 壳验证，非本期必交付
```

## 3. 跨宿主领域契约

### 3.1 前端端口

React 功能层统一依赖：

```ts
interface BeamDropApi {
  getSnapshot(): Promise<AppSnapshot>;
  send(request: SendRequest): Promise<TaskCreated>;
  startReceiver(request: StartReceiverRequest): Promise<ReceiverSnapshot>;
  stopReceiver(): Promise<ReceiverSnapshot>;
  subscribe(listener: (event: BeamDropEvent) => void): () => void;
}
```

### 3.2 领域类型

本期至少定义以下共享领域类型：

- `SendRequest`
- `TaskCreated`
- `TaskSnapshot`
- `ReceiverSnapshot`
- `AppSnapshot`
- `TransferProgress`
- `BeamDropEvent`
- `ErrorPayload`

TypeScript、Pydantic、未来 Rust serde 和 C++ DTO 保持语义一致，但不追求跨语言共享源码。

### 3.3 事件 envelope v1

```json
{
  "schema_version": 1,
  "event_id": "...",
  "sequence": 42,
  "type": "transfer.progress",
  "task_id": "...",
  "timestamp": "2026-07-11T14:00:00+08:00",
  "payload": {}
}
```

约束：

- `schema_version` 用于长期兼容。
- `sequence` 用于发现事件遗漏。
- WebSocket 事件不是权威状态；REST 快照是权威状态。
- task ID 属于宿主任务管理层，不进入 C++ transfer/protocol 层。
- 前端领域请求使用“文件来源引用”概念，避免把浏览器 `File` 永久写入核心模型。

## 4. 实施切片

### Slice 0：基线确认与范围冻结

#### 目标

确认当前 C++ app service、构建方式和测试基线，冻结本期功能边界，避免 GUI 开发期间同时改动协议层。

#### 交付

- 明确 GUI 只调用 app service。
- 记录 Windows / Linux 构建命令和当前测试数量。
- 确认本期单文件发送和串行接收范围。
- 确认不在本期修改 Packet 协议和续传格式。

#### 验收标准

- CMake 配置和构建成功。
- 现有 CTest 全部通过。
- app service 单文件 loopback 集成测试通过。
- CLI 行为无回归。

### Slice 1：GUI 所需 C++ 应用服务契约闭环

#### 目标

使 GUI 可以通过稳定的 C++ 服务级 API 完成发送和接收，不依赖 CLI 入口或控制台文本。

#### 范围

- 校验 `SendRequest`、`ReceiveRequest`、`SendResult`、`ReceiveResult`、`TransferProgress` 和结构化错误。
- 建立最小接收服务生命周期：`start/status/stop`。
- 进度回调只输出领域数据，不输出 UI 文本。
- 明确线程和生命周期所有权。

#### 排除

- 不在此切片实现 pybind、HTTP 或 GUI。
- 不强行实现尚未设计完成的阻塞 socket 中断。
- 若 `stop` 只能在安全边界生效，状态和文档必须如实表达，不得报告为立即停止。

#### 验收标准

- C++ 测试可直接调用发送服务完成单文件传输。
- 接收生命周期测试覆盖：未启动、启动成功、重复启动、状态查询、停止、重复停止。
- 进度事件至少覆盖开始、传输中、文件完成和任务完成所需字段。
- 错误通过结构化错误码表达，不要求上层解析日志字符串。
- 所有已有 CTest 继续通过。

### Slice 2：pybind11 薄绑定

#### 目标

将 C++ app service 暴露给 Python，同时保持 Python 不承担传输协议和会话编排。

#### 交付

- `bindings/python/CMakeLists.txt`。
- `beamdrop_native` 扩展模块。
- 请求、结果、进度和结构化异常转换。
- 长任务释放 GIL；调用 Python 回调前恢复 GIL。
- UTF-8 路径边界处理。

#### 排除

- 不直接暴露 `Sender`、`Receiver`、`TcpServer`。
- 不让 Python 拼装 manifest 或 Packet。
- 不把 Python 对象长期保存在无明确生命周期的 C++ 全局状态中。

#### 验收标准

- Python 可成功 `import beamdrop_native`。
- 合法请求能调用 C++ app service。
- 非法请求转换为稳定、可识别的 Python 异常。
- 单文件 loopback 冒烟测试能收到结构化进度。
- Python 回调抛出异常时进程不崩溃，任务得到明确失败结果。
- Windows 验证通过；Linux 至少完成模块构建和 import 验证。

### Slice 3：跨宿主领域契约与前端端口

#### 目标

先定义 React 所依赖的稳定端口，保证 FastAPI 与未来 Tauri 可以互换，而不重写页面和 store。

#### 交付

- TypeScript 领域类型。
- `BeamDropApi` interface。
- `FastApiAdapter` 接口位置。
- `TauriAdapter` 预留位置和契约说明；本期不要求实现。
- 事件 envelope v1。

#### 验收标准

- 前端可使用 mock `BeamDropApi` 完成类型检查。
- React 组件源码中不存在直接 `fetch`、`WebSocket` 或 Tauri `invoke` 调用。
- FastAPI DTO 与 TypeScript DTO 的字段、可空性和枚举值有明确对照。
- 未知事件类型可被忽略或记录，不导致页面崩溃。
- `schema_version != 1` 时 adapter 给出明确兼容性错误。

### Slice 4：FastAPI NativeGateway 与任务状态机

#### 目标

在 HTTP 层和 pybind 之间建立可替换的应用层，统一管理后台任务、接收服务和状态快照。

#### 模块边界

```text
API routes -> TaskManager / ReceiverManager
                    |
              NativeGateway interface
                    |
          PybindNativeGateway implementation
```

#### 交付

- `NativeGateway` 抽象及 pybind 实现。
- 可用于测试的 fake gateway。
- `TaskManager` 内存态任务仓库。
- `ReceiverManager` 接收生命周期管理。
- 后台任务在线程池执行，不阻塞 asyncio event loop。

基础状态机：

```text
pending -> running -> completed
pending -> running -> failed
```

只有 C++ 取消闭环完成后才加入：

```text
running -> canceling -> canceled
```

#### 验收标准

- 使用 fake gateway 可独立测试任务状态机，无需真实 socket。
- 后台任务执行期间 FastAPI 健康检查仍可响应。
- 同一任务只进入一个终态。
- native 异常被转换为结构化 `failed` 状态。
- 任务快照更新具备线程安全保护。
- 重复启动接收服务得到确定错误，不创建重复监听实例。

### Slice 5：REST 权威快照 API

#### 目标

提供命令入口和可恢复的权威状态快照，避免前端依赖 WebSocket 消息历史推导当前状态。

#### 接口

```text
GET  /api/health
GET  /api/snapshot
POST /api/transfers/send
POST /api/receiver/start
POST /api/receiver/stop
```

可选调试接口：

```text
GET /api/tasks/{task_id}
```

#### 安全边界

- 默认只监听 `127.0.0.1`。
- CORS 仅允许已配置的 Vite 开发地址。
- 路径、host、port、chunk size 均执行服务端校验。
- 不允许前端指定任意日志文件或后端内部状态路径。

#### 验收标准

- 请求验证失败返回稳定的 4xx 和结构化错误体。
- 创建任务立即返回 task ID，不等待传输结束。
- `/api/snapshot` 同时返回任务和接收服务状态。
- 不存在的任务返回 404。
- 后端启动、任务运行和任务完成时快照均与实际状态一致。
- FastAPI pytest 覆盖成功、校验失败、native 失败和重复启动接收场景。

### Slice 6：WebSocket 增量事件通道

#### 目标

实时推送任务与接收状态，同时保证连接失败不会影响真实传输任务。

#### 交付

- `EventBus`。
- `GET /ws/events`。
- heartbeat。
- 事件：
  - `task.created`
  - `task.started`
  - `transfer.progress`
  - `task.completed`
  - `task.failed`
  - `receiver.started`
  - `receiver.stopped`
  - `error`
  - `heartbeat`
- 单调递增 `sequence`。

#### 边界

- EventBus 不保存业务权威状态。
- 慢客户端不得阻塞 pybind 回调或传输线程。
- 进度允许合并或节流；终态事件不得丢弃。
- WebSocket 断开不取消任务。

#### 验收标准

- 连接后可收到 heartbeat。
- 一次成功任务可观察到创建、开始、进度和完成事件。
- 失败任务收到明确终态事件。
- 客户端主动断开后后台任务继续运行并可从 REST 快照查询。
- 重连后前端可先通过快照恢复，再继续消费新事件。
- 慢客户端测试不会阻塞任务执行线程。

### Slice 7：FastApiAdapter 与状态恢复

#### 目标

将 REST / WebSocket 的所有宿主细节封装在 adapter 内，为以后替换为 TauriAdapter 建立真实迁移边界。

#### 交付

- `FastApiAdapter`。
- REST 命令调用。
- WebSocket 连接、重连、退避和取消订阅。
- DTO 校验与错误归一化。
- 初始化及重连流程：先拉快照，再消费事件；发现 sequence 缺口时重新拉快照。

#### 验收标准

- mock HTTP / WS 下 adapter 可独立测试。
- 后端离线时返回统一 `BeamDropError`，而不是泄漏 `TypeError: fetch failed`。
- WebSocket 重连不会产生重复订阅和重复事件处理。
- sequence 缺失触发一次快照刷新。
- adapter 销毁后不保留定时器或 WebSocket 连接。
- React 其余模块不感知 API URL、WebSocket URL 或 JSON envelope 解析细节。

### Slice 8：React 单页展示 GUI

#### 目标

完成可演示、状态完整但不过度扩张的单页控制面。

#### 页面组成

- 顶部状态栏：后端连接状态、接收服务状态。
- 发送区：本机路径、目标 host、port、发送按钮。
- 接收区：监听 host、port、保存目录、启动/停止按钮。
- 当前任务：总进度、当前文件进度、字节数、状态、错误。
- 任务列表：本次运行期间任务。

#### 边界

- 页面不解析日志文本推断状态。
- 页面不直接访问网络和 native 能力。
- 首版不做复杂路由、主题系统、历史数据库和配置编辑器。
- 普通浏览器无法可靠提供绝对文件路径，因此展示版明确采用路径输入或受控后端选择；不实现上传到后端临时目录的伪文件选择。

#### 验收标准

- 使用 mock adapter 时可以独立演示成功、失败、离线、空状态。
- 使用 FastApiAdapter 时可创建真实发送任务。
- 进度事件驱动 UI 更新，无正常轮询。
- 刷新页面后可从快照恢复正在运行或已完成任务。
- 后端断开时页面显示离线，不崩溃、不清空最后快照。
- 表单校验阻止空路径、非法端口和空 host。
- 前端测试通过，生产构建成功。

### Slice 9：真实端到端演示闭环

#### 目标

证明 GUI 调用的是真实 C++ 传输核心，而不是 mock、上传接口或 CLI 文本包装。

#### 必须通过的验收场景

1. 同机 loopback 单文件传输。
2. 空文件传输。
3. 小文件传输。
4. 中文文件名传输。
5. 目标不可达时明确失败。
6. WebSocket 中断后任务继续，重连后通过快照恢复。
7. 接收文件 SHA256 与源文件一致。
8. 原有 CLI 和 CTest 无回归。

#### 平台验收

- Windows：完整端到端流程必须通过。
- Linux：CMake / CTest、pybind 构建/import、backend pytest、frontend build 必须通过；有可用环境时完成 loopback E2E。
- 不将“代码理论跨平台”写成“已完成跨平台验证”。

#### 完成标准

- 一条明确命令启动后端。
- 一条明确命令启动前端。
- 从干净目录按 README 可复现。
- 展示样例和故障兜底录屏准备完成。

### Slice 10：文档、展示与质量门禁

#### 目标

使项目可以被评审者理解、构建和复现，并准确区分当前能力与长期规划。

#### 交付

- 更新 README：架构、构建、启动、测试、演示步骤。
- 更新 GUI 实施计划的实际完成状态。
- 架构图和模块职责说明。
- Windows / Linux 验证矩阵。
- 已知限制：浏览器路径选择、取消语义、串行接收、无自动发现、无桌面安装包。
- 5–7 分钟展示脚本。

#### 验收标准

- 文档中的命令在干净环境可执行。
- 所有已完成能力有测试或手工验收证据。
- 所有未完成能力明确标为后续计划。
- 不宣称已完成 Tauri、macOS 或移动端支持。
- 完成交付前统一运行 CTest、binding tests、backend pytest、frontend tests/build 和 E2E。

## 5. 可选切片：Tauri v2 最小宿主验证

该切片只能在 Slice 0–10 的核心门禁完成后开始，不属于本期必交付路径。

### 目标

验证同一个 React 构建产物能够运行在 Tauri 窗口中，并确认未来 adapter 替换路径成立。

### 交付

- `desktop/` Tauri v2 工程。
- 加载现有 React frontend。
- 原生文件 dialog。
- `app_info` command / event 冒烟验证。
- `TauriAdapter` 可编译骨架，不实现真实传输。

### 验收标准

- Windows Tauri 窗口成功启动并加载 React UI。
- 原生 dialog 可返回本机路径。
- React 页面仍只依赖 `BeamDropApi`。
- 不启动 FastAPI 时，未实现的传输命令显示明确“不支持”，不伪装成功。
- Tauri 代码不侵入 `frontend/features` 和 C++ core。

## 6. 切片依赖与执行顺序

```text
Slice 0 基线
   |
Slice 1 C++ app service
   |
Slice 2 pybind
   |
Slice 3 跨宿主契约
   |
Slice 4 TaskManager / NativeGateway
   +------------+
   |            |
Slice 5 REST   Slice 6 WebSocket
   |            |
   +------+-----+
          |
Slice 7 FastApiAdapter
          |
Slice 8 React GUI
          |
Slice 9 E2E
          |
Slice 10 文档与交付
```

不建议先画完整 React 页面再补 adapter；那会导致页面直接绑定 FastAPI，破坏未来 Tauri 迁移边界。

## 7. 风险与降级策略

### 风险 1：接收服务生命周期未闭环

优先修复 C++ 生命周期。若展示期限内无法安全停止阻塞监听，降级为 GUI 发送 + 独立 CLI 接收，并在界面/文档中标注接收控制未完成。不能伪造 stop 状态。

### 风险 2：浏览器无法选择本机绝对路径

本期使用明确路径输入或受控后端文件选择。禁止浏览器上传到同机后端临时目录后再发送，因为这会产生无意义复制且无法迁移到 Tauri。

### 风险 3：pybind 回调、GIL 与 asyncio 线程混用

pybind 回调不得直接操作 asyncio 对象；通过线程安全队列或 `loop.call_soon_threadsafe` 交给后端事件循环。

### 风险 4：WebSocket 事件丢失

事件不是权威状态。前端初始化、重连和 sequence 缺口时统一拉取 REST 快照。

### 风险 5：范围失控

只有 Slice 9 的 E2E 门禁完成后才能做 Tauri 壳、视觉动画或额外页面。核心闭环未完成时，不添加自动发现、托盘、取消和历史数据库。

## 8. 展示后演进切片

后续另立计划，不混入本周交付：

1. 稳定 C ABI：C++ 句柄、请求、快照、回调、内存释放和错误码。
2. Rust safe wrapper：隔离 unsafe FFI。
3. TauriAdapter：以 command/event 实现 `BeamDropApi`。
4. 桌面能力：原生 dialog、托盘、通知、应用目录、安装器。
5. macOS：构建、签名、公证、本地网络权限。
6. 移动文件源抽象：从仅路径扩展为 path / URI / file descriptor / stream。
7. Android / iOS 生命周期：沙盒、局域网权限、后台限制和中断恢复。

FastAPI 不会被废弃，而是保留为 Web / Agent 控制宿主；Tauri 是正式桌面宿主。两者共享领域契约和 React 功能层，但不强行共享底层运行时。