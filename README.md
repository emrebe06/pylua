# Lunura Dili

Lunura Dili is a native scripting language and runtime for modern web backends, e-commerce services, data pipelines, local automation, and embeddable application scripting.

This repository contains the current Lunara runtime core: lexer, parser, AST interpreter, early VM path, web backend stack, SQLUna integration, auth/session flows, DSP helpers, and embedding bridges. The goal is simple syntax with enough runtime power to compete in real backend work instead of staying at "toy script" level.

## Why Lunura Dili

- backend-first scripting language with real web runtime features
- e-commerce and API-oriented examples out of the box
- native parser/interpreter with growing VM coverage
- SQLUna-powered SQLite workflows and auth building blocks
- embeddable runtime for host applications and automation tools
- practical developer ergonomics instead of framework bloat

## GitHub Keywords

Suggested discovery keywords for this project:

- lunura dili
- lunura language
- lunara runtime
- native scripting language
- backend scripting language
- web backend language
- ecommerce scripting language
- embeddable scripting runtime
- cpp interpreter
- parser vm language

## What Lunara Is

Lunara is not a Python transpiler and it does not call `exec()` on another host language.

This project currently provides:

- a native parser and interpreter
- a CLI runtime
- an early VM backend with compatibility fallback
- JSON, file system, time, security, compute, web, SQL, and DSP helpers
- Python embedding through a shared library and bridge layer
- a Windows installer flow

Execution path today:

`source -> lexer -> parser -> AST -> interpreter`

VM path:

`source -> lexer -> parser -> AST -> bytecode -> VM`

## Why It Exists

Lunara was built as a data-first scripting runtime for practical developer work:

- export public snapshots from admin data
- generate JSON payloads
- build static websites
- run local automation scripts
- embed scripting inside another host app
- power backend experiments and internal tools

The project aims to sit between "tiny config script" and "full production framework" with a runtime that stays understandable, hackable, and portable.

## Use Cases

- data export pipelines
- JSON compilation and validation
- local automation tools
- static site generation
- mini web services and API experiments
- auth/session-oriented backend prototypes
- e-commerce style backend services
- Python-hosted orchestration
- application scripting and embedded runtime experiments

## Language Snapshot

Supported today:

- `let`, `const`
- `func ... end`
- anonymous `func(...) ... end` lambdas
- `if / elseif / else / end`
- `while ... do ... end`
- `for item in items do ... end`
- `for index, item in list do ... end`
- `for key, value in object do ... end`
- `match / when / else / end`
- `try / catch / finally`, `throw`, `defer`
- `return`
- optional type hints on variables, params, and function returns
- list and object literals
- member and index access
- object and list mutation
- `import foo.bar as alias` for built-in, file, and package modules
- CLI commands: `run`, `vm`, `check`, `analyze`, `version`

Built-in modules available today:

- `fs`
- `json`
- `time`
- `web`
- `http`
- `sqluna`
- `payments`
- `security`
- `dsp`
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
.\build\Debug\lunara.exe analyze .\examples\analyzer_type_error_demo.lunara
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

The ctypes bridge now looks for the native embed library on both Windows and Linux builds.

## Web Runtime

Lunara includes an expanding web runtime with:

- static file serving
- router creation
- middleware support
- text and JSON responses
- simple request parsing
- `web.backend()` alias for app-style backend setup
- `web.api()` alias for FastAPI-style backend setup
- route params, cookies, CORS handling, and WebSocket routes
- SQLUna-backed auth/session flows
- bearer token support and role/permission guards
- script-level SQLite query and migration helpers for backend apps
- lightweight templating with `web.template()` and `web.render()`
- request validation middleware with `web.require_json([...])`
- payment adapter contracts through `payments`

See:

- [router_demo.lunara](/C:/pylua/examples/router_demo.lunara)
- [full_web_backend.lunara](/C:/pylua/examples/full_web_backend.lunara)
- [auth_backend.lunara](/C:/pylua/examples/auth_backend.lunara)
- [ecommerce_backend.lunara](/C:/pylua/examples/ecommerce_backend.lunara)
- [fastapi_style_backend.lunara](/C:/pylua/examples/fastapi_style_backend.lunara)
- [parser_expansion_demo.lunara](/C:/pylua/examples/parser_expansion_demo.lunara)
- [language_core_upgrade_demo.lunara](/C:/pylua/examples/language_core_upgrade_demo.lunara)
- [pattern_match_defer_demo.lunara](/C:/pylua/examples/pattern_match_defer_demo.lunara)
- [package_demo/src/main.lunara](/C:/pylua/examples/package_demo/src/main.lunara)
- [dsp_demo.lunara](/C:/pylua/examples/dsp_demo.lunara)
- [live_bridge_demo/README.md](/C:/pylua/examples/live_bridge_demo/README.md)

## Language Growth

Recent core upgrades include:

- `match` with literal, wildcard, and binding patterns
- nested list/object destructuring patterns in `match`
- `defer` with LIFO execution on normal return or exceptional exit
- runtime-enforced optional type hints for variables and functions
- generic/container type hints like `list<string>` and `object<number>`
- `lunara analyze` static diagnostics for common type mistakes
- workspace-style module resolution through `lunara.toml`, `lunara.lock`, `packages/`, and local registries
- parser recovery that reports multiple syntax issues in one pass with code-frame output

## Project Structure

- [src](/C:/pylua/src)
- [include/lunara](/C:/pylua/include/lunara)
- [examples](/C:/pylua/examples)
- [sqluna](/C:/pylua/sqluna)
- [python](/C:/pylua/python)
- [packaging/windows](/C:/pylua/packaging/windows)
- [website](/C:/pylua/website)

## Documentation

- [LUNARA_GUIDE.md](/C:/pylua/LUNARA_GUIDE.md)
- [LUNARA_KURULUM_VE_KULLANIM_REHBERI.md](/C:/pylua/LUNARA_KURULUM_VE_KULLANIM_REHBERI.md)
- [LUNARA_NEXT_STEPS.md](/C:/pylua/LUNARA_NEXT_STEPS.md)
- [packaging/windows/README.md](/C:/pylua/packaging/windows/README.md)
- [packaging/linux/README.md](/C:/pylua/packaging/linux/README.md)

## Current Status

Lunara is real and usable, but still early.

Already usable:

- CLI runtime
- installer pipeline
- file and JSON workflows
- Python embedding
- backend-oriented web apps with auth, CORS, cookies, and WebSockets
- FastAPI-style script backends with templates, middleware, and payment stubs
- SQLUna-backed SQLite flows for auth and small service data layers
- e-commerce style backend prototypes from script land
- DSP helpers for signal/data preprocessing
- VM compatibility path

Still evolving:

- full native VM coverage
- richer web server model
- package ecosystem
- plugin ABI
- production-grade runtime hardening
- full GPU/CUDA execution model

## Positioning

`Lunara is a native scripting language for data, backend services, embeddable automation, and early SQL/DSP-powered web applications.`

## Author

Created and developed by Emre Demirbas.
