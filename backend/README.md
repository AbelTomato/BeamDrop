# Slice 5：FastAPI REST 控制面

当前目录提供 FastAPI 宿主应用层与 REST 权威快照 API；WebSocket 增量事件属于 Slice 6。

## 当前模块

- `app/core/native_gateway.py`：隔离 pybind11 DTO 和异常，提供稳定的 Python 应用层端口。
- `app/core/task_manager.py`：拥有单文件发送任务的内存快照与状态机。
- `app/core/receiver_manager.py`：拥有单例接收服务的生命周期快照，并串行化 start/stop 操作。
- `app/core/fake_native_gateway.py`：用于单元测试的确定性 native 替身。
- `app/schemas/wire.py`：Pydantic JSON DTO 与领域 DTO 的显式转换。
- `app/main.py`：可注入 `NativeGateway` 的 FastAPI 应用工厂与 REST routes。

## 边界

- `TaskManager` 只能依赖 `NativeGateway`，不能导入 `beamdrop_native`。
- `run()` 必须使用 `asyncio.to_thread()` 运行阻塞的 `gateway.send()`。
- native 进度回调来自 worker thread；更新共享任务快照时必须加锁。
- 状态机只允许：`pending -> running -> completed` 或 `pending -> running -> failed`。
- 终态不能被迟到进度或重复执行覆盖；不要实现取消状态。
- `get()` / `snapshots()` 返回不可变 DTO，不暴露内部可修改容器。
- `ReceiverManager` 不承诺 `stop()` 同步达到 `stopped`：native 服务可能真实返回 `stopping`。
- API routes 只能调用 `TaskManager` / `ReceiverManager`，不得直接 import `beamdrop_native`。
- `POST /api/transfers/send` 返回 `202` 和 task ID，不等待传输结束；`GET /api/snapshot` 是权威状态。
- `POST /api/receiver/start` 和 `/stop` 经 `asyncio.to_thread()` 调用可能阻塞的 native 生命周期操作。
- 所有 4xx 错误统一为 `{"error":{"code", "message", "details"}}`，不泄漏 Pydantic 或日志文本结构。
- CORS 仅允许 Vite 开发地址 `http://127.0.0.1:5173` 与 `http://localhost:5173`。
- WebSocket 事件、重连和 sequence 仍属于 Slice 6–7。

## 测试环境

请在 `backend/` 建立 Python 3.12+ 虚拟环境后安装开发依赖：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

## REST 接口

```text
GET  /api/health
GET  /api/snapshot
POST /api/transfers/send
POST /api/receiver/start
POST /api/receiver/stop
GET  /api/tasks/{task_id}
```

## Windows 开发启动

先使用 `windows-mingw-python-debug` 预设构建 `beamdrop_native`，再从仓库根目录运行：

```powershell
backend\.venv\Scripts\python.exe backend\run_windows_dev.py
```

该脚本从 `PATH` 中的 `g++.exe` 推导 MinGW runtime DLL 目录；不能自动发现时设置 `BEAMDROP_MINGW_BIN`。它仅解决当前 Windows MinGW 开发环境的 `.pyd` 与运行时 DLL 搜索路径；正式打包启动方式将在后续交付切片确定。