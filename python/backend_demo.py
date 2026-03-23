from pathlib import Path

from pylua_bridge import PyLuaBridge


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    bridge = PyLuaBridge()
    payload = bridge.run_json(root / "examples" / "web_backend_demo.pylua", cwd=root)
    print(payload["framework"])
    print(payload["total_price"])
