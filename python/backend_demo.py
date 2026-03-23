from pathlib import Path

from lunara_bridge import LunaraBridge


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    bridge = LunaraBridge()
    payload = bridge.run_json(root / "examples" / "web_backend_demo.lunara", cwd=root)
    print(payload["framework"])
    print(payload["total_price"])

