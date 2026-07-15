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

## 测试命令

```powershell
ctest --preset windows-mingw-debug --output-on-failure
.\.venv\Scripts\python.exe -m pytest backend\tests -q
npm --prefix frontend test
npm --prefix frontend run lint
npm --prefix frontend run build
```