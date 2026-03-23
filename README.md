# PyLua Engine MVP

`PyLua`, `.pylua` dosyalarini dogrudan calistiran native bir dil motoru iskeletidir.

Bu ilk surum su pipeline'i gercekten calistirir:

`source -> lexer -> parser -> AST -> interpreter`

Motor Python'a transpile etmez ve `exec()` kullanmaz. Kod kendi lexer ve parser'i ile okunur, AST uzerinden native runtime'da calistirilir.

## Desteklenen dil ozellikleri

- `let` ve `const`
- `import fs`, `import json`, `import time`
- CLI komutlari: `run`, `vm`, `check`, `version`
- dosya tabanli `import lib.foo` modulleri
- `func ... end`
- `if / elseif / else / end`
- `while ... do ... end`
- `for item in items do ... end`
- `return`
- sayi, string, `true`, `false`, `nil`
- list literal: `[1, 2, 3]`
- object literal: `{name: "Latte", price: 120}`
- uye/index erisimi: `obj.name`, `obj["name"]`, `items[0]`
- degisken atama: `name = expr`
- operatorler: `+ - * / == != > >= < <= and or not`
- fonksiyon cagrisi
- yerlesik `print(...)`

## Derleme

```powershell
cmake -S . -B build
cmake --build build
```

## Calistirma

```powershell
.\build\Debug\pylua.exe .\examples\hello.pylua
.\build\Debug\pylua.exe run .\examples\hello.pylua
.\build\Debug\pylua.exe vm .\examples\vm_demo.pylua
.\build\Debug\pylua.exe check .\examples\hello.pylua
.\build\Debug\pylua.exe .\examples\catalog.pylua
.\build\Debug\pylua.exe .\examples\web_backend_demo.pylua
```

## Python uyumu

`python/pylua_bridge.py`, native motoru Python backend'lerden cagirmak icin ince bir bridge saglar.

```python
from pathlib import Path
from pylua_bridge import PyLuaBridge

root = Path(__file__).resolve().parents[1]
bridge = PyLuaBridge()
payload = bridge.run_json(root / "examples" / "web_backend_demo.pylua", cwd=root)
print(payload["framework"])
```

Bridge, script'in son satirdaki JSON ciktisini parse eder. Bu sayede Flask/FastAPI gibi Python taraflari PyLua'yi subprocess ile cagirabilir.

Shared library / embed yolu icin:

```python
from pylua_embed_ctypes import PyLuaEmbed

embed = PyLuaEmbed()
print(embed.run_file("examples/vm_demo.pylua", backend="vm"))
print(embed.run_source('print("embed ok")'))
```

Bu yol `build/Debug/pylua_embed.dll` uzerinden `ctypes` ile dogrudan embedding yapar.

## Mimari

- `include/pylua/token.hpp`: token tanimlari
- `include/pylua/lexer.hpp`: kaynak kodu tokenlara boler
- `include/pylua/ast.hpp`: AST dugumleri
- `include/pylua/parser.hpp`: recursive descent parser
- `include/pylua/runtime.hpp`: runtime value ve environment modeli
- `include/pylua/interpreter.hpp`: AST interpreter arabirimi
- `src/*.cpp`: motorun native implementasyonu

## Sonraki fazlar

- bytecode compiler
- stack-based VM
- native plugin/FFI API
