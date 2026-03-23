# Lunara

Lunara is a native scripting language and runtime for data pipelines, local automation, lightweight web backends, and embeddable application scripting.

It runs `.lunara` files directly through its own lexer, parser, AST runtime, CLI, and early VM compatibility layer. Lunara is designed to feel small, practical, and readable while staying close to real runtime control.

## What Lunara Is

Lunara is not a Python transpiler and it does not call `exec()` on another host language.

This project currently provides:

- a native parser and interpreter
- a CLI runtime
- an early VM backend with compatibility fallback
- JSON, file system, time, security, compute, and web helpers
- Python embedding through a shared library and bridge layer
- a Windows installer flow

Current execution path:

`source -> lexer -> parser -> AST -> interpreter`

VM path:

`source -> lexer -> parser -> AST -> bytecode -> VM`

## Why It Exists

Lunara was built as a data-first scripting runtime for practical developer work:

- export public snapshots from admin data
- generate JSON payloads
- build static websites
- run local automation scripts
- act as a small embedded language inside another host app
- power lightweight backend or tooling experiments

The goal is to sit between “tiny config script” and “full production framework” with a runtime that is understandable, hackable, and portable.

## Use Cases

- data export pipelines
- JSON compilation and validation
- local automation tools
- static site generation
- mini web services and API experiments
- Python-hosted orchestration
- application scripting and embedded runtime experiments

## Language Snapshot

Supported today:

- `let`, `const`
- `func ... end`
- `if / elseif / else / end`
- `while ... do ... end`
- `for item in items do ... end`
- `return`
- `list` and `object` literals
- member and index access
- `import` for built-in and file-based modules
- CLI commands: `run`, `vm`, `check`, `version`

Built-in modules available today:

- `fs`
- `json`
- `time`
- `web`
- `http`
- `security`
- `compute`
- `cpu`
- `gpu`
- `cuda`
- `fsl`

## Quick Start

### Build

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

### Run

```powershell
.\build\Debug\lunara.exe help
.\build\Debug\lunara.exe .\examples\hello.lunara
.\build\Debug\lunara.exe vm .\examples\vm_demo.lunara
.\build\Debug\lunara.exe check .\examples\router_demo.lunara
```

### Windows Installer

```powershell
powershell -ExecutionPolicy Bypass -File .\packaging\windows\create_installer.ps1
```

Installer output:

`dist\windows\Lunara-Setup-0.1.0.exe`

## Example

```lunara
import json
import time

let payload = {
    generated_at: time.utc(),
    products: [
        {id: 1, name: "Latte", price: 120},
        {id: 2, name: "Mocha", price: 135}
    ]
}

print(json.encode(payload))
```

## Python Embedding

Lunara can be called from Python through:

- [lunara_bridge.py](/C:/pylua/python/lunara_bridge.py)
- [lunara_embed_ctypes.py](/C:/pylua/python/lunara_embed_ctypes.py)

Example:

```python
from pathlib import Path
from lunara_embed_ctypes import LunaraEmbed

root = Path(__file__).resolve().parents[1]
embed = LunaraEmbed(root / "build" / "Debug" / "lunara_embed.dll")

print(embed.run_file(root / "examples" / "vm_demo.lunara", backend="vm"))
```

This makes Lunara useful in:

- FastAPI
- Flask
- worker pipelines
- local build tooling
- admin-side automation

## Web Runtime

Lunara includes an early web runtime with:

- static file serving
- router creation
- middleware support
- text and JSON responses
- simple request parsing

See:

- [router_demo.lunara](/C:/pylua/examples/router_demo.lunara)
- [website/README.md](/C:/pylua/website/README.md)

## Project Structure

- [src](/C:/pylua/src)
- [include/lunara](/C:/pylua/include/lunara)
- [examples](/C:/pylua/examples)
- [python](/C:/pylua/python)
- [packaging/windows](/C:/pylua/packaging/windows)
- [website](/C:/pylua/website)

## Documentation

- [LUNARA_GUIDE.md](/C:/pylua/LUNARA_GUIDE.md)
- [LUNARA_KURULUM_VE_KULLANIM_REHBERI.md](/C:/pylua/LUNARA_KURULUM_VE_KULLANIM_REHBERI.md)
- [packaging/windows/README.md](/C:/pylua/packaging/windows/README.md)

## Current Status

Lunara is real and working, but still early.

What is already usable:

- CLI runtime
- installer pipeline
- file and JSON workflows
- Python embedding
- static and small web runtime experiments
- VM compatibility path

What is still evolving:

- full native VM coverage
- richer web server model
- object/list mutation ergonomics
- package ecosystem
- plugin ABI
- production-grade runtime hardening
- full GPU/CUDA execution model

## Positioning

The most accurate short description today is:

`Lunara is a data-first native scripting language with early web runtime support, Python embedding, and a practical CLI-to-installer workflow.`

## Author

Created and developed by Emre Demirbaş.
