# GUI 交付与演示指南

## 1. 当前交付范围

本次交付是本机浏览器 GUI 控制面，不是最终桌面安装包。它调用真实 C++ 传输核心：

```text
React 页面 / dashboardState
          │  BeamDropApi
          ▼
     FastApiAdapter
          │ REST（权威快照）/ WebSocket（增量事件）
          ▼
FastAPI: TaskManager / ReceiverManager / EventBus
          │ NativeGateway
          ▼
    beamdrop_native（pybind11）
          ▼
 C++ app service / beamdrop_core / TCP transfer
```

边界：React feature、组件和 store 仅依赖 `BeamDropApi` 与领域 DTO；HTTP、WebSocket 和 DTO wire-format 转换只在 adapter；FastAPI route 只调用 manager，manager 只调用 `NativeGateway`，不直接绑定 pybind。

### 已交付能力

- 接收服务启动、状态展示与停止；停止仅在 native 状态实际收敛为 `stopped` 后显示成功。
- 基于本机路径的单文件发送，创建任务立即返回，传输在线程中执行。
- 任务状态、结构化错误、实时进度与运行期间任务列表。
- REST `/api/snapshot` 权威快照；页面刷新、WebSocket 重连或 sequence 缺口时从快照恢复。
- 事件 envelope v1、heartbeat、慢客户端隔离与 WebSocket 断开后任务继续。

## 2. 运行前置条件

| 项目 | Windows | Linux（已验证 Ubuntu VM） |
| --- | --- | --- |
| C++ 工具链 | MinGW / C++20 / CMake 3.20+ | GCC / C++20 / CMake 3.20+ |
| Python | 3.12+，根目录 `.venv` | 3.12+，根目录 `.venv` |
| Node | 20.19+ 或 22.12+；推荐 Node 22 | 同左；Node 18 不兼容当前 Vite 8 |
| Python 依赖 | `pybind11`、`pip install -e ".\backend[dev]"` | `pybind11`、`pip install -e './backend[dev]'` |

仅使用仓库根目录 `.venv`，不要在 `backend/` 新建第二个虚拟环境。

## 3. 启动与验证

主 README 的“GUI 控制面”章节提供 Windows / Linux 的逐条启动命令。启动后的基础健康检查：

```bash
curl -s http://127.0.0.1:8000/api/health
# {"status":"ok"}
```

浏览器打开 `http://127.0.0.1:5173`。默认 CORS 只允许 `127.0.0.1:5173` 和 `localhost:5173`；后端默认应绑定回环地址，不要把开发控制面直接暴露到局域网。

### 最小演示数据

Linux 示例：

```bash
mkdir -p /tmp/beamdrop-e2e/source /tmp/beamdrop-e2e/received
printf 'BeamDrop Linux E2E\n' > /tmp/beamdrop-e2e/source/small.txt
: > /tmp/beamdrop-e2e/source/empty.bin
printf '中文文件名内容\n' > /tmp/beamdrop-e2e/source/数据.txt
```

GUI 中启动 receiver：`127.0.0.1:9090`，保存目录 `/tmp/beamdrop-e2e/received`；随后将三个文件逐个发送到 `127.0.0.1:9090`。

完成后验证校验和：

```bash
sha256sum /tmp/beamdrop-e2e/source/small.txt /tmp/beamdrop-e2e/received/small.txt
sha256sum /tmp/beamdrop-e2e/source/empty.bin /tmp/beamdrop-e2e/received/empty.bin
sha256sum /tmp/beamdrop-e2e/source/数据.txt /tmp/beamdrop-e2e/received/数据.txt
```

每条命令的两个 hash 必须相同。将目标端口改为一个未监听端口（例如 `19091`）发送，任务必须明确进入 `failed`，而非停留在 `running`。

## 4. 验证矩阵（2026-07-15）

| 平台 | C++ / native | Backend | Frontend | GUI E2E |
| --- | --- | --- | --- | --- |
| Windows MinGW | CTest 3/3；pybind build/import | pytest 31/31 | test 10/10、lint、build | GUI 传输与 receiver start/stop 手工通过 |
| Linux Ubuntu VM | `linux-gcc-debug` CTest 2/2；`native-bindings-debug` build/import | pytest 31/31（1 条上游弃用警告） | test 10/10、build | 小文件、空文件、中文文件名、SHA256、不可达失败、刷新恢复、receiver stop 通过 |

说明：`native-bindings-debug` 为 binding build/import 预设，显式关闭 CTest；因此 Linux 的 2/2 CTest 与 pybind build/import 是两项独立证据，不能写成 Linux CTest 3/3。

Linux E2E 快照证据：三个任务为 `completed`，不可达 `127.0.0.1:19091` 的任务为 `failed`，错误码 `NETWORK_FAILURE`、消息 `connect failed: socket error 111`，receiver 最终 `stopped`。

## 5. 已知限制

- GUI 首版仅支持路径式**单文件**发送；浏览器无法安全、可靠地交付绝对路径，故不做浏览器上传到后端临时目录的复制方案。
- 接收端为单实例、串行接收；不含 UDP 自动发现、多客户端并发接收、任务历史数据库、托盘、自启动、安装器或自动更新。
- 发送取消是 C++ stop token 的协作式请求，不保证立即中断阻塞 socket；不得将“已请求取消”展示为“已取消”。
- FastAPI 控制面默认只服务本机开发使用；它不是认证、授权或公网部署方案。
- Tauri、Rust/C ABI、macOS、Android、iOS 均未交付。Tauri adapter 仅保留契约边界。

## 6. 5–7 分钟展示脚本

1. **0:00–0:45，架构**：展示上方链路，强调 GUI 不包装 CLI；它经 pybind 调用 C++ app service，REST 快照权威、WebSocket 只推增量。
2. **0:45–1:30，启动**：展示 FastAPI health 返回 `ok`，打开 GUI，说明路径输入是有意的浏览器边界。
3. **1:30–2:30，接收服务**：填写 receiver 的 host、port、保存目录，启动后确认状态 `running`；再次启动说明会收到确定错误而非创建重复监听。
4. **2:30–3:45，正常传输**：发送小文件，展示 `pending → running → completed`、进度和字节数；再发送空文件与中文文件名。
5. **3:45–4:30，完整性**：运行 `sha256sum`，展示源、接收文件 hash 一致。
6. **4:30–5:15，失败语义**：发送到未监听端口，展示结构化 `NETWORK_FAILURE` 和 `failed`，没有伪造成功或永久 running。
7. **5:15–6:00，恢复和停止**：刷新页面，展示 `/api/snapshot` 恢复任务与 receiver 状态；停止 receiver，确认最终 `stopped`。
8. **6:00–7:00，质量与边界**：展示验证矩阵；明确未实现取消立即生效、自动发现、桌面安装包或 Tauri，而非夸大跨平台能力。