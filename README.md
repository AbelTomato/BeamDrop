# BeamDrop / 邻光

本机 GUI 演示：React → FastAPI → pybind11 → C++ 传输核心。

## 演示配置

| 项目 | 值 |
| --- | --- |
| GUI | `http://127.0.0.1:5173` |
| 后端 | `http://127.0.0.1:8000` |
| 接收地址 | `127.0.0.1:9090` |
| Linux 接收目录 | `/tmp/beamdrop-e2e/received` |
| Node | 20.19+，推荐 Node 22 |
| Python | 3.12+ |

## Windows：构建并启动 GUI

在仓库根目录执行：

```powershell
# 首次初始化 Python 环境
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install pybind11
.\.venv\Scripts\python.exe -m pip install -e ".\backend[dev]"

# 构建 pybind 模块
cmake --preset windows-mingw-python-debug
cmake --build --preset windows-mingw-python-debug --parallel

# 安装前端依赖
npm --prefix frontend ci
```

打开两个终端：

```powershell
# 终端 A：后端
.\.venv\Scripts\python.exe backend\run_windows_dev.py
```

```powershell
# 终端 B：前端
npm --prefix frontend run dev -- --host 127.0.0.1 --port 5173
```

浏览器访问 `http://127.0.0.1:5173`。

## Linux：构建并启动 GUI

在仓库根目录执行：

```bash
# 首次初始化 Python 环境
python3.12 -m venv .venv
./.venv/bin/python -m pip install pybind11
./.venv/bin/python -m pip install -e './backend[dev]'

# 构建 C++ 与 pybind 模块
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --parallel
cmake --preset native-bindings-debug
cmake --build --preset native-bindings-debug --parallel

# 安装前端依赖
npm --prefix frontend ci
```

打开两个终端：

```bash
# 终端 A：后端
export PYTHONPATH="$PWD/build/native-bindings-debug/bindings/python${PYTHONPATH:+:$PYTHONPATH}"
./.venv/bin/python -m uvicorn app.main:create_app --factory --host 127.0.0.1 --port 8000
```

```bash
# 终端 B：前端
npm --prefix frontend run dev -- --host 127.0.0.1 --port 5173
```

## GUI 演示步骤

1. 在“接收服务”填写 `127.0.0.1`、`9090` 与保存目录，点击“启动接收”。
2. 在“发送文件或文件夹”填写待发送文件或文件夹的**绝对路径**、目标 `127.0.0.1`、端口 `9090`，点击“发送”。文件夹会递归传输其中的常规文件，保留完整目录结构，并在接收目录下创建所选文件夹的最外层目录。
3. 观察任务状态和进度变为 `completed`。
4. 停止接收服务，确认最终状态为 `stopped`。

> 文件夹传输行为：保留所选文件夹的最外层目录及所有空目录；符号链接会被拒绝；接收目录中已存在同名的目标文件或文件夹时，任务会失败而不会覆盖已有内容。目录传输使用协议 v2，因此发送端与接收端必须同时更新到当前版本。

Linux 测试文件与 SHA256 验证：

```bash
mkdir -p /tmp/beamdrop-e2e/source /tmp/beamdrop-e2e/received
printf 'BeamDrop Linux E2E\n' > /tmp/beamdrop-e2e/source/small.txt
: > /tmp/beamdrop-e2e/source/empty.bin
printf '中文文件名内容\n' > /tmp/beamdrop-e2e/source/数据.txt

sha256sum /tmp/beamdrop-e2e/source/small.txt /tmp/beamdrop-e2e/received/small.txt
sha256sum /tmp/beamdrop-e2e/source/empty.bin /tmp/beamdrop-e2e/received/empty.bin
sha256sum /tmp/beamdrop-e2e/source/数据.txt /tmp/beamdrop-e2e/received/数据.txt
```

## 测试命令

```powershell
ctest --preset windows-mingw-debug --output-on-failure
.\.venv\Scripts\python.exe -m pytest backend\tests -q
npm --prefix frontend test
npm --prefix frontend run lint
npm --prefix frontend run build
```