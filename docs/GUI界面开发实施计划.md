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
  subscribe(
    listener: (event: BeamDropEvent) => void,
    onSnapshot?: (snapshot: AppSnapshot) => void,
  ): () => void;
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

## 实施状态（2026-07-15）

### Slice 5：REST 权威快照 API — 已完成

已完成：

- 新增 FastAPI 应用工厂、Pydantic wire DTO 与 `GET /api/health`、`GET /api/snapshot`、`POST /api/transfers/send`、`POST /api/receiver/start`、`POST /api/receiver/stop`、`GET /api/tasks/{task_id}`。
- REST DTO 严格按 `docs/FastAPI-TypeScript DTO对照.md` 使用 snake_case；接收服务 `state_file` 由后端按 `save_dir` 派生，不对前端暴露内部状态路径。
- 创建发送任务立即返回 `202` 和 task ID；任务执行、接收服务 start/stop 均不阻塞 asyncio event loop。
- `/api/snapshot` 同时返回 tasks 与 receiver 的权威内存快照；不存在任务返回结构化 `404`。
- 校验错误、重复接收服务启动等 4xx 使用稳定 `{"error":{"code", "message", "details"}}` 结构；CORS 仅允许配置的 Vite 开发地址。
- backend REST/API 测试 23/23 通过，覆盖成功、校验失败、native 失败、重复启动、404 与 CORS；真实 Uvicorn `GET /api/health` Windows smoke 返回 `{"status":"ok"}`。

后续切片：

- EventBus、实时事件、heartbeat 和 WebSocket 断线语义已在 Slice 6 实现。
- Windows MinGW 开发启动使用 `backend/run_windows_dev.py` 注入 native `.pyd` 与运行时 DLL 搜索路径（可通过 `BEAMDROP_MINGW_BIN` 显式指定）；正式交付启动方式将在 Slice 10 固化。

### Slice 6：WebSocket 增量事件通道 — 已完成

已完成：

- 新增线程安全 `EventBus` 与 `GET /ws/events`；事件使用 envelope v1，包含 schema version、UUID event ID、进程内单调 sequence、类型、task ID、时间戳和 JSON payload。
- `TaskManager` 发布 `task.created`、`task.started`、`transfer.progress`、`task.completed` 和 `task.failed`；`ReceiverManager` 发布 `receiver.started`、`receiver.stopped` 及失败 `error` 事件。
- 每个 WebSocket 客户端使用独立有界队列。发布线程以 `loop.call_soon_threadsafe()` 投递，不阻塞 native 回调或传输线程；慢客户端下可丢弃进度，终态事件优先保留。
- 实现 heartbeat；WebSocket 断开仅注销订阅，不影响已创建的后台任务。EventBus 不保存权威业务状态，重连后仍必须先拉 REST snapshot。
- backend pytest 27/27 通过，覆盖任务生命周期事件、sequence、heartbeat、客户端断开后任务完成与慢订阅者终态保留。

后续切片：

- FastApiAdapter 的 WebSocket 重连、退避、未知事件处理、schema compatibility error 与 sequence 缺口 snapshot 刷新属于 Slice 7。

### Slice 7：FastApiAdapter 与状态恢复 — 已完成

已完成：

- 实现 `FastApiAdapter`，将 REST snake_case DTO 映射为前端 camelCase 领域模型；发送、启动/停止接收服务与权威 snapshot 均只通过 adapter 调用。
- 网络不可达统一映射为 `BeamDropError(BACKEND_UNAVAILABLE)`；后端结构化 HTTP 错误保留稳定 code/message/details，不向功能层泄漏原始 fetch 错误。
- 实现 WebSocket 连接、指数退避重连、unsubscribe 清理连接与定时器；连接成功和 sequence 缺口时均重新拉取 snapshot，并通过可选 `onSnapshot` 回调交给 store。
- 事件 envelope 严格验证 `schema_version`，不兼容版本转换为 `INCOMPATIBLE_EVENT_SCHEMA`；未知事件保留原 payload，允许调用方忽略或记录。
- `TransferProgress.totalBytes`、`transferredBytes` 按后端实际契约允许为 `null`，覆盖 C++ 初始进度未提供总量的情况。
- 接收服务 WebSocket 事件统一使用 `ReceiverSnapshotDto` 扁平 DTO，保持与 REST `receiver` 字段同形，避免泄露后端内部嵌套 request。
- 已知事件采用精确类型解析；未来未知事件保留原 payload，由功能层忽略或记录，不会被错误按 task/receiver DTO 解析。
- 新增 `tsx --test` 前端测试命令，使用 Node 原生 test runner；不依赖当前 Node 24 下无法初始化的 Vitest 运行时。

已验证：
- `npm --prefix frontend test`：10/10 通过，覆盖 REST DTO/请求映射、离线与结构化 HTTP 错误、schema 拒绝、初始空总量进度、receiver WebSocket DTO、未知 task-like 事件、连接/sequence 缺口 snapshot 刷新和 unsubscribe。
- `npm --prefix frontend run build` 通过。
- `.venv\Scripts\python.exe -m pytest backend\tests -q`：30/30 通过（1 条现有 Starlette/httpx 弃用警告）。

后续切片：

- Slice 8 负责将 adapter 的 snapshot/event 回调接入 React store 和单页 GUI；页面、组件不得直接调用 fetch 或 WebSocket。

### Slice 4：FastAPI NativeGateway 与任务状态机 — 已完成

已完成：

- 新增 backend 内存领域 DTO：`StartReceiverRequest`、`ReceiverSnapshot`。
- 实现 `NativeGateway` 发送与接收服务契约，提供 `PybindNativeGateway` 与可测试的 `FakeNativeGateway`。
- `TaskManager` 支持 `pending -> running -> completed/failed`，使用锁保护任务快照；长任务经 `asyncio.to_thread()` 执行，进度和终态互不覆盖。
- 取消状态仅在 native 层明确返回 `CANCELLED` 后进入 `canceled`；取消请求与传输成功竞态时以 native 成功结果为准，进入 `completed`，不伪造取消成功。
- `PybindNativeGateway` 的 cancel watcher 在发送结束后退出并 join，不因成功任务累积后台 daemon 线程。
- 接收服务 native `stop()` 为异步请求确认：`ReceiverManager` 在 worker thread 中轮询 native `status()` 至多 2 秒，仅实际收敛为 `stopped` 后才返回 REST snapshot 并发布 `receiver.stopped`；超时转换为结构化 `STOP_TIMEOUT` failed 状态，避免 GUI 永久卡在 `stopping`。
- `ReceiverManager` 管理单一接收服务快照，并串行化 start/stop；重复启动返回确定错误，不会创建第二个监听实例；停止已停止服务为幂等操作。
- backend pytest 覆盖发送任务成功、native 失败、未知异常、进度、重复执行、接收服务生命周期与并发 start/stop；共 15/15 通过。
- Windows MinGW 下构建 `beamdrop_native` 成功；真实 `PybindNativeGateway` receiver smoke 验证 `running -> stopping`，符合 C++ 异步 stop 语义。

后续切片：

- FastAPI 应用、Pydantic DTO 与 REST 快照已在 Slice 5 实现；WebSocket 事件仍属于 Slice 6–7。
- Slice 5 的 API route 已在 worker thread 调用可能阻塞的 `ReceiverManager.start/stop`；后续 API route 必须保持该边界。
- Windows MinGW 的 `.pyd` 手工运行时需通过 `os.add_dll_directory()` 指定 MinGW runtime DLL 目录；正式启动脚本应处理该开发环境依赖。

### Slice 3：跨宿主领域契约与前端端口 — 已完成

已完成：

- 在 `frontend/src/platform/BeamDropApi.ts` 定义共享领域类型、`BeamDropApi` 端口、事件 envelope v1 与未知事件识别逻辑。
- 提供 `FastApiAdapter`、`TauriAdapter` 的宿主边界骨架；两者当前明确返回 `HOST_NOT_IMPLEMENTED`，不伪造网络或原生传输已实现。
- 定义 `IncompatibleEventSchemaError`：`schemaVersion !== 1` 时 adapter 给出明确兼容性错误。
- 新增 `MockBeamDropApi` 和 TypeScript 契约 typecheck fixture，验证 mock 能满足端口、未来未知事件不会被当作已知事件处理。
- 新增 `docs/FastAPI-TypeScript DTO对照.md`，冻结 FastAPI JSON 与 TypeScript 领域 DTO 的字段、可空性、枚举和命名转换规则；后端尚未实现部分已明确标记。

已验证：

- `npm --prefix frontend run build` 通过。
- `npm --prefix frontend run lint` 通过。
- React 模板源码未引入直接 `fetch`、`WebSocket` 或 Tauri `invoke` 调用；宿主细节只允许在后续 adapter 实现内出现。

待验证/未完成：

- FastAPI/Pydantic DTO、真实 REST 和 WebSocket 传输已分别在 Slice 5–6 实现；FastApiAdapter 的 wire-format 解析、重连与 sequence 恢复测试仍属于 Slice 7。

### Slice 2：pybind11 薄绑定 — 已完成

已完成：

- 构建 `beamdrop_native` pybind11 扩展模块。
- 暴露 `ErrorCode`、传输方向/阶段、`ServiceError`、`SendResult`、`TransferProgress` 等只读领域 DTO。
- `ServiceResult<T>` 失败统一转换为带稳定 `ErrorCode` 的 `BeamDropError`，上层无需解析错误文本。
- 暴露 `send(..., on_progress=None)`；发送长任务释放 GIL，进度回调前重新获取 GIL。
- Python 回调异常转换为 `BeamDropError(ErrorCode.INTERNAL_ERROR)`，不会导致进程崩溃。
- 暴露持久 `ReceiverService` 的 `start/status/stop`，用于 app service 层的真实 loopback。
- 新增 pytest：中文文件名 loopback、进度阶段、SHA256、回调异常和非法回调参数。

已验证：

- Windows Debug 模块构建与 import 成功。
- pytest 通过：同进程 loopback、中文文件名 `数据.bin`、SHA256、完整发送进度、回调异常映射和参数校验。
- CTest 通过：`beamdrop_unit_tests`、`beamdrop_integration_tests`、`beamdrop_python_binding_tests` 共 3/3。
- Linux：使用 Python 3.12 venv 构建并成功 import `beamdrop_native`；`linux-gcc-debug` CTest 为 unit/integration 2/2 通过。由于 `native-bindings-debug` 为 binding 构建/import 目的关闭测试，Linux binding 验证不误记为 CTest 第 3 项。

待验证/未完成：

- 无。Slice 2 验收完成；CLI `serve` 生命周期回归已修复并通过 Windows MinGW 常驻监听冒烟验证，可进入 Slice 3 跨宿主领域契约与前端端口。

### Slice 8：React 单页展示 GUI — 已完成（2026-07-15）

已完成：

- 使用 Tailwind CSS v4 和本地 shadcn/ui 兼容组件实现单页控制面，使用 Lucide 图标。
- 提供后端与接收服务状态栏、路径式单文件发送、接收服务启停、任务列表、实时进度、结构化错误和空状态。
- feature 组件只接收领域类型和回调，未直接调用 `fetch`、`WebSocket` 或 Tauri API。
- 新增无 React 依赖的 `dashboardState`：REST snapshot 是权威状态，事件只做增量更新，未知事件安全忽略。
- 离线状态保留最后快照；表单阻止空路径、空 host、非法端口和空保存目录。

已验证：

- `npm --prefix frontend test`：10/10 通过，覆盖 adapter 与 snapshot/event 状态归约。
- `npm --prefix frontend run lint`：0 errors / 0 warnings。
- `npm --prefix frontend run build`：TypeScript 检查和 Vite 生产构建成功。
- Vite 开发服务 `http://127.0.0.1:5173/` 返回 HTTP 200。

### Slice 9：真实端到端演示闭环 — 已完成（2026-07-15）

已验证：

- Windows：已完成 GUI 实际传输与接收服务启停手工验证；自动化门禁为 Windows CTest 3/3、backend pytest 31/31、frontend test 10/10 与 production build 通过。
- Linux（Ubuntu 虚拟机，Python 3.12、Node 22）：`ctest --preset linux-gcc-debug` 为 2/2 通过；`native-bindings-debug` 成功构建 `beamdrop_native`，并由 Python 3.12 成功 import；backend pytest 31/31 通过（1 条上游弃用警告）；frontend test 10/10 与 production build 通过。
- Linux 同机真实 GUI loopback：FastAPI、Vite、pybind 和 C++ core 完整链路成功传输 `small.txt`（19 B）、`empty.bin`（0 B）和中文文件名 `数据.txt`（22 B）。
- SHA256：小文件为 `44793931e7c1d73284f917456854559a9aedd15f1bff6c09a2890a278fc37314`、空文件为标准空 SHA256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`、中文文件为 `be0cc1c62f7d5afdf6e1b2b78d0281603cf07328cc6b762a6dfd4a8a67bec17c`；三组源文件与接收文件均一致。
- 不可达目标 `127.0.0.1:19091` 明确进入 `failed`，快照记录 `NETWORK_FAILURE` 与 `connect failed: socket error 111`，未遗留 `running` 任务。
- `/api/snapshot` 返回三个 completed、一个 failed 任务，接收服务最终为 `stopped`；页面刷新/重连通过 REST 权威快照恢复任务及接收服务状态。

说明：Linux 的 Python binding 使用独立 `native-bindings-debug` preset；该 preset 为构建/import 目的关闭 CTest。因此 Linux CTest 2/2 与 binding 构建/import 为两项独立验证，不将其误记为 3/3 CTest。

### Slice 10：文档、展示与质量门禁 — 已完成（2026-07-15）

已完成：

- 更新根 `README.md`：明确 GUI 架构、Windows/Linux 的 pybind、FastAPI、Vite 启动步骤与统一质量门禁命令。
- 新增 `docs/GUI交付与演示指南.md`：记录实际模块边界、运行前置条件、Windows/Linux 验证矩阵、最小 E2E 操作、已知限制与 5–7 分钟展示脚本。
- 如实区分 Windows 与 Linux 验证事实；不将 Linux binding build/import 误记为 CTest，也不宣称已完成 Tauri、macOS 或移动端。

最终本机质量门禁：

- `ctest --preset windows-mingw-debug --output-on-failure`：3/3 通过。
- `.venv\Scripts\python.exe -m pytest backend\tests -q`：31/31 通过（1 条 Starlette/httpx 上游弃用警告）。
- `npm --prefix frontend test`：10/10 通过；`npm --prefix frontend run lint`：0 errors / 0 warnings；`npm --prefix frontend run build`：通过。