from pathlib import Path

from pylua_embed_ctypes import PyLuaEmbed


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    embed = PyLuaEmbed()
    print(embed.run_file(root / "examples" / "vm_demo.pylua", backend="vm"))
    print(embed.run_source('print("embed ok")', backend="interpreter"))
