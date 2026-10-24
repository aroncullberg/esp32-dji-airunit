#!/usr/bin/env python3
"""Compile/run portable protocol and rendering tests under ASan/UBSan."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
common = ["src/config.cpp", "src/msp_codec.cpp", "src/canvas.cpp",
          "src/frame.cpp", "src/engine.cpp"]
for name, extra in [("core", []), ("runtime", ["src/air_unit.cpp"]),
                    ("widgets", ["examples/steering_osd/main/widgets.cpp"])]:
    with tempfile.TemporaryDirectory(prefix="dji-air-tests-") as directory:
        binary = Path(directory) / name
        subprocess.run([
            *shlex.split(os.environ.get("CXX", "clang++")), "-std=c++17", "-Wall",
            "-Wextra", "-Werror", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-pthread", "-g", f"-I{root}/include", f"-I{root}/src", f"-I{root}/tests/host/fakes",
            f"-I{root}/examples/steering_osd/main",
            *(str(root / source) for source in [f"tests/host/{name}_test.cpp", *common, *extra]),
            "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
