# Slice 4：FastAPI 宿主应用层

当前目录提供 FastAPI 宿主的应用层基础，不包含 HTTP 路由；REST API 属于 Slice 5。

## 当前模块

- `app/core/native_gateway.py`：隔离 pybind11 DTO 和异常，提供稳定的 Python 应用层端口。
- `app/core/task_manager.py`：拥有单文件发送任务的内存快照与状态机。
- `app/core/receiver_manager.py`：拥有单例接收服务的生命周期快照，并串行化 start/stop 操作。
- `app/core/fake_native_gateway.py`：用于单元测试的确定性 native 替身。

## 边界

- `TaskManager` 只能依赖 `NativeGateway`，不能导入 `beamdrop_native`。
- `run()` 必须使用 `asyncio.to_thread()` 运行阻塞的 `gateway.send()`。
- native 进度回调来自 worker thread；更新共享任务快照时必须加锁。
- 状态机只允许：`pending -> running -> completed` 或 `pending -> running -> failed`。
- 终态不能被迟到进度或重复执行覆盖；不要实现取消状态。
- `get()` / `snapshots()` 返回不可变 DTO，不暴露内部可修改容器。
- `ReceiverManager` 不承诺 `stop()` 同步达到 `stopped`：native 服务可能真实返回 `stopping`。
- HTTP 路由、Pydantic wire DTO、REST 快照和 WebSocket 事件不属于本目录当前实现，分别在 Slice 5–7 完成。

## 测试环境

主机默认 Python 3.14 没有安装 pytest。请在 `backend/` 建立 Python 3.12+ 虚拟环境后安装开发依赖：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -e ".[dev]"
.\.venv\Scripts\python.exe -m pytest -q
```

当前依赖中没有 FastAPI 或 Pydantic；它们将在 Slice 5 引入，避免本练习被 HTTP wire model 干扰。