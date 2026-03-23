# Lunara Engine MVP

`Lunara`, `.lunara` dosyalarini dogrudan calistiran native bir dil motoru iskeletidir.

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
.\build\Debug\lunara.exe .\examples\hello.lunara
.\build\Debug\lunara.exe run .\examples\hello.lunara
.\build\Debug\lunara.exe vm .\examples\vm_demo.lunara
.\build\Debug\lunara.exe check .\examples\hello.lunara
.\build\Debug\lunara.exe .\examples\catalog.lunara
.\build\Debug\lunara.exe .\examples\web_backend_demo.lunara
```

## Python uyumu

`python/lunara_bridge.py`, native motoru Python backend'lerden cagirmak icin ince bir bridge saglar.

```python
from pathlib import Path
from lunara_bridge import LunaraBridge

root = Path(__file__).resolve().parents[1]
bridge = LunaraBridge()
payload = bridge.run_json(root / "examples" / "web_backend_demo.lunara", cwd=root)
print(payload["framework"])
```

Bridge, script'in son satirdaki JSON ciktisini parse eder. Bu sayede Flask/FastAPI gibi Python taraflari Lunara'yi subprocess ile cagirabilir.

Shared library / embed yolu icin:

```python
from lunara_embed_ctypes import LunaraEmbed

embed = LunaraEmbed()
print(embed.run_file("examples/vm_demo.lunara", backend="vm"))
print(embed.run_source('print("embed ok")'))
```

Bu yol `build/Debug/lunara_embed.dll` uzerinden `ctypes` ile dogrudan embedding yapar.

## Mimari

- `include/lunara/token.hpp`: token tanimlari
- `include/lunara/lexer.hpp`: kaynak kodu tokenlara boler
- `include/lunara/ast.hpp`: AST dugumleri
- `include/lunara/parser.hpp`: recursive descent parser
- `include/lunara/runtime.hpp`: runtime value ve environment modeli
- `include/lunara/interpreter.hpp`: AST interpreter arabirimi
- `src/*.cpp`: motorun native implementasyonu

## Sonraki fazlar

- bytecode compiler
- stack-based VM
- native plugin/FFI API

