"""Run the FastAPI host against a local MinGW pybind build on Windows."""

from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

import uvicorn


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
PYBIND_DIRECTORY = REPOSITORY_ROOT / "build" / "windows-mingw-python-debug" / "bindings" / "python"


def find_mingw_bin_directory() -> Path | None:
    configured = os.environ.get("BEAMDROP_MINGW_BIN")
    if configured:
        return Path(configured)
    compiler = shutil.which("g++.exe")
    return Path(compiler).parent if compiler else None


def main() -> None:
    if not PYBIND_DIRECTORY.exists():
        raise SystemExit(f"missing native module directory: {PYBIND_DIRECTORY}")
    mingw_bin_directory = find_mingw_bin_directory()
    if mingw_bin_directory is None or not mingw_bin_directory.exists():
        raise SystemExit("missing MinGW runtime directory; set BEAMDROP_MINGW_BIN or add g++.exe to PATH")

    os.add_dll_directory(str(mingw_bin_directory))
    sys.path.insert(0, str(PYBIND_DIRECTORY))

    from app.main import create_app

    uvicorn.run(create_app(), host="127.0.0.1", port=8000)


if __name__ == "__main__":
    main()