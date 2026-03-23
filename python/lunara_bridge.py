from __future__ import annotations

import json
import subprocess
from pathlib import Path


class LunaraBridge:
    def __init__(self, engine_path: str | Path | None = None) -> None:
        base = Path(__file__).resolve().parents[1]
        candidates = [
            base / "build" / "Debug" / "lunara.exe",
            base / "build" / "Release" / "lunara.exe",
            base / "build" / "lunara.exe",
        ]
        self.engine_path = Path(engine_path) if engine_path else next((path for path in candidates if path.exists()), candidates[0])

    def run(self, script_path: str | Path, cwd: str | Path | None = None) -> str:
        script = Path(script_path)
        working_dir = Path(cwd) if cwd else script.resolve().parents[0]
        result = subprocess.run(
            [str(self.engine_path), str(script)],
            cwd=str(working_dir),
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout

    def run_json(self, script_path: str | Path, cwd: str | Path | None = None) -> dict:
        output = self.run(script_path, cwd=cwd)
        lines = [line.strip() for line in output.splitlines() if line.strip()]
        if not lines:
            raise ValueError("Lunara script did not emit any output")
        return json.loads(lines[-1])


__all__ = ["LunaraBridge"]

