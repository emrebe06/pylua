from pathlib import Path

from lunara_embed_ctypes import LunaraEmbed


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    embed = LunaraEmbed()
    print(embed.run_file(root / "examples" / "vm_demo.lunara", backend="vm"))
    print(embed.run_source('print("embed ok")', backend="interpreter"))

