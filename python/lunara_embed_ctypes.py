from __future__ import annotations

import ctypes
from pathlib import Path


class LunaraEmbed:
    def __init__(self, dll_path: str | Path | None = None) -> None:
        root = Path(__file__).resolve().parents[1]
        default_path = root / "build" / "Debug" / "lunara_embed.dll"
        self.dll_path = Path(dll_path) if dll_path else default_path
        self.lib = ctypes.CDLL(str(self.dll_path))

        self.lib.lunara_run_file.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
        self.lib.lunara_run_file.restype = ctypes.c_void_p

        self.lib.lunara_run_source.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
        self.lib.lunara_run_source.restype = ctypes.c_void_p

        self.lib.lunara_string_free.argtypes = [ctypes.c_void_p]
        self.lib.lunara_string_free.restype = None

    def _consume(self, pointer: int) -> str:
        if not pointer:
            return ""
        try:
            return ctypes.cast(pointer, ctypes.c_char_p).value.decode("utf-8")
        finally:
            self.lib.lunara_string_free(pointer)

    def run_file(self, script_path: str | Path, backend: str = "interpreter") -> str:
        exit_code = ctypes.c_int()
        result = self.lib.lunara_run_file(str(script_path).encode("utf-8"), backend.encode("utf-8"), ctypes.byref(exit_code))
        text = self._consume(result)
        if exit_code.value != 0:
            raise RuntimeError(text)
        return text

    def run_source(self, source: str, virtual_path: str = "<memory>", backend: str = "interpreter") -> str:
        exit_code = ctypes.c_int()
        result = self.lib.lunara_run_source(
            source.encode("utf-8"),
            virtual_path.encode("utf-8"),
            backend.encode("utf-8"),
            ctypes.byref(exit_code),
        )
        text = self._consume(result)
        if exit_code.value != 0:
            raise RuntimeError(text)
        return text


__all__ = ["LunaraEmbed"]

