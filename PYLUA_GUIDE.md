# PyLua Guide

Yazar: Emre Demirbaş

PyLua, `.pylua` dosyalarini dogrudan calistiran native bir scripting dili ve runtime motorudur. Bu repo icindeki surum; veri uretme, JSON pipeline, yerel web backend, statik site build etme ve Python icinden embed edilme senaryolari icin tasarlanmistir.

Bu guide sana su 6 seyi verir:

1. nasil derlenir ve calistirilir
2. dilin temel sozdizimi
3. stdlib modulleri
4. web/router/middleware modeli
5. Python embedding yolu
6. bugun ne eksik, sonra ne gelmeli

## 1. Hizli Baslangic

```powershell
cd C:\pylua
cmake -S . -B build
cmake --build build --config Debug
```

Temel kullanim:

```powershell
.\build\Debug\pylua.exe help
.\build\Debug\pylua.exe .\examples\hello.pylua
.\build\Debug\pylua.exe run .\examples\hello.pylua
.\build\Debug\pylua.exe vm .\examples\vm_demo.pylua
.\build\Debug\pylua.exe check .\examples\hello.pylua
```

## 2. CLI Komutlari

- `pylua <script.pylua>`: varsayilan interpreter backend ile calistirir
- `pylua run <script.pylua>`: interpreter backend
- `pylua vm <script.pylua>`: VM backend
- `pylua check <script.pylua>`: parse kontrolu yapar
- `pylua help`: yardim basar
- `pylua version`: surum basar

Not:

- `vm` backend artik daha fazla seyi kabul eder.
- Saf bytecode destegi olmayan yerlerde uyumluluk icin interpreter fallback kullanir.
- Yani bugun `vm` pratikte daha kullanisli, ama henuz tam production-grade bytecode runtime degil.

## 3. Windows Kurulum Exe

PyLua icin Windows setup exe uretmek icin:

```powershell
cd C:\pylua
powershell -ExecutionPolicy Bypass -File .\packaging\windows\create_installer.ps1
```

Uretilen dosya:

`C:\pylua\dist\windows\PyLua-Setup-0.1.0.exe`

Bu paket:

- Release build uretir
- self-contained MSVC runtime ile gelir
- `%LOCALAPPDATA%\Programs\PyLua` altina kurulur
- wizard ile lisans ekrani gosterir
- kurulum klasoru secme sayfasi sunar
- `Add to PATH` checkbox'i sunar
- mevcut kurulumu algilayip install/upgrade/repair davranisi uygular
- `bin` klasorunu user PATH'e ekler
- `PYLUA_HOME` ortam degiskenini yazar

## 4. Proje Yapisi

Tipik bir PyLua klasoru:

```text
my_app/
  lib/
    helpers.pylua
  app.pylua
  data/
    payload.json
```

Modul import:

```pylua
import lib.helpers
```

PyLua su yollar uzerinden modul arar:

- script klasoru altinda goreli dosya
- script klasoru altinda `lib/`
- mevcut calisma klasoru
- mevcut calisma klasoru altinda `lib/`

## 5. Temel Sozdizimi

### Degisken

```pylua
let name = "North Ember"
let price = 120
const city = "Istanbul"
```

### Fonksiyon

```pylua
func greet(name)
    return "Merhaba " + name
end
```

### Kosul

```pylua
if price > 100 then
    print("premium")
elseif price > 50 then
    print("mid")
else
    print("entry")
end
```

### While

```pylua
let i = 0

while i < 3 do
    print(i)
    i = i + 1
end
```

### For In

```pylua
let items = ["Latte", "Mocha", "Filter"]

for item in items do
    print(item)
end
```

Object uzerinde `for key in obj` su an key iterasyonu yapar.

## 6. Veri Tipleri

Bugun kullanabildigin ana tipler:

- `number`
- `string`
- `bool`
- `nil`
- `list`
- `object`
- `function`

### List

```pylua
let drinks = ["Latte", "Mocha", "Espresso"]
print(drinks[0])
```

### Object

```pylua
let cafe = {
    name: "North Ember",
    city: "Istanbul",
    open: true
}

print(cafe.name)
print(cafe["city"])
```

Not:

- `obj.name` eksik field icin hata verir
- `obj["missing"]` eksik key icin `nil` dondurur

## 7. Operatorler

Desteklenen operatorler:

- `+`
- `-`
- `*`
- `/`
- `==`
- `!=`
- `>`
- `>=`
- `<`
- `<=`
- `and`
- `or`
- `not`

## 8. JSON Ile Calisma

`json.decode` ve `json.read` artik dogrudan mevcut.

```pylua
import json

let raw = "{\"name\":\"North Ember\",\"open\":true}"
let parsed = json.decode(raw)
print(parsed.name)

json.write("build/out/site.json", parsed)
let loaded = json.read("build/out/site.json")
print(loaded["name"])
```

Python uyumlu alias'ler de var:

- `json.dumps`
- `json.loads`
- `json.dump`
- `json.load`

## 9. Stdlib Modulleri

### `fs`

```pylua
import fs

if not fs.exists("build/out") then
    fs.mkdir("build/out", true)
end

fs.write_text("build/out/hello.txt", "Merhaba")
print(fs.read_text("build/out/hello.txt"))
```

Fonksiyonlar:

- `fs.exists(path)`
- `fs.mkdir(path, deep)`
- `fs.read_text(path)`
- `fs.write_text(path, text)`

### `json`

Fonksiyonlar:

- `json.encode(value)`
- `json.decode(text)`
- `json.write(path, value)`
- `json.read(path)`
- `json.dumps(value)`
- `json.loads(text)`
- `json.dump(path, value)`
- `json.load(path)`

### `time`

```pylua
import time

print(time.now())
print(time.utc())
```

### `security`

Bu modul runtime tarafinda guvenlik yardimcilari verir.

Fonksiyonlar:

- `security.constant_time_equals(a, b)`
- `security.safe_join(root, child)`
- `security.issue_token(length)`

Ornek:

```pylua
import security

let token = security.issue_token(24)
print(token)
print(security.constant_time_equals("a", "a"))
```

### `compute`

Genel hesaplama kapasitesi bilgisi verir.

Fonksiyonlar:

- `compute.capabilities()`
- `compute.backend()`

Ornek:

```pylua
import compute

let caps = compute.capabilities()
print(caps.preferred_backend)
print(caps.cpu_threads)
```

### `cpu`, `gpu`, `cuda`

Bu moduller bugun tam bir tensor/runtime katmani degil. Simdilik kapasite ve ortam algilama katmani sunarlar.

- `cpu.info()`
- `gpu.info()`
- `cuda.available()`
- `cuda.info()`

Not:

- CUDA destegi bugun best-effort detection seviyesinde
- yani toolkit/driver var mi yok mu sorusunu cevaplar
- henuz kernel calistiran bir GPU compute ABI yok

### `fsl`

Bu repoda `fsl` kismini `File System Layer` olarak ele aldim. Guvenli path ve veri dosyasi yardimcilari icin kullanilir.

Fonksiyonlar:

- `fsl.safe_join(root, child)`
- `fsl.read_text(path)`
- `fsl.write_text(path, text)`
- `fsl.read_json(path)`
- `fsl.write_json(path, value)`

## 10. Web Runtime

PyLua artik sadece statik dosya sunmakla kalmiyor. Mini bir web runtime da var.

### Statik Sunum

```pylua
import web

web.serve_static("website/dist", 8080)
```

### HTTP Response

`web.response`, `web.text`, `web.json` ile response uretebilirsin.

```pylua
import web

func hello(ctx)
    return web.text(200, "merhaba")
end
```

```pylua
import web

func hello(ctx)
    return web.json({ok: true, route: ctx.request.path}, 200)
end
```

### Router

```pylua
import web
import time

let app = web.router()

func health(ctx)
    return web.json({
        status: "ok",
        now: time.utc()
    }, 200)
end

app.get("/health", health)
app.listen(8092)
```

### Request Modeli

Handler'a gelen `ctx.request` alanlari:

- `method`
- `target`
- `path`
- `version`
- `body`
- `headers`
- `query`
- `json`

`request.json`, `Content-Type: application/json` geldiginde otomatik parse edilmeye calisir.

### Middleware

Middleware bir callable alir ve `ctx` uzerinden calisir.

- `nil` donerse sonraki middleware veya route handler devam eder
- response donerse istek orada sonlanir

Ornek:

```pylua
import web
import security

let app = web.router()
const api_key = "demo-key"

func auth(ctx)
    let provided = ctx.request.headers["x-api-key"]
    if not security.constant_time_equals(provided, api_key) then
        return web.json({error: "unauthorized"}, 401)
    end
    return nil
end

func info(ctx)
    return web.json({route: ctx.request.path}, 200)
end

app.use(auth)
app.get("/info", info)
app.listen(8092)
```

### HTTP Alias

`import http` su an `web` modulunun alias'i gibi davranir.

## 11. Python Embed

PyLua iki yolla Python tarafina baglanabilir:

### Subprocess Bridge

[pylua_bridge.py](/C:/pylua/python/pylua_bridge.py)

### Native DLL + `ctypes`

[pylua_embed_ctypes.py](/C:/pylua/python/pylua_embed_ctypes.py)

Ornek:

```python
from pathlib import Path
from pylua_embed_ctypes import PyLuaEmbed

root = Path(r"C:\pylua")
embed = PyLuaEmbed(root / "build" / "Debug" / "pylua_embed.dll")

print(embed.run_file(root / "examples" / "vm_demo.pylua", backend="vm"))
print(embed.run_source('print("embed ok")'))
```

Bu yol FastAPI, Flask ya da queue-worker tarafindan PyLua scripti cagirabilsin diye var.

## 12. Ornekler

Hazir ornekler:

- [hello.pylua](/C:/pylua/examples/hello.pylua)
- [vm_demo.pylua](/C:/pylua/examples/vm_demo.pylua)
- [vm_data_demo.pylua](/C:/pylua/examples/vm_data_demo.pylua)
- [router_demo.pylua](/C:/pylua/examples/router_demo.pylua)
- [platform_demo.pylua](/C:/pylua/examples/platform_demo.pylua)
- [catalog.pylua](/C:/pylua/examples/catalog.pylua)

## 13. Guvenlik Katmani Bugun Ne Sagliyor

Bugun runtime seviyesinde gelen korumalar:

- path traversal bloklama
- `security.safe_join`
- `security.constant_time_equals`
- token uretimi
- router/static server icin method kisitlama
- request boyutu limiti
- default olarak kapali response handling

Bu iyi bir taban ama tam production hardening degil.

## 14. Bugun Hala Eksik Olanlar

En onemli eksikler:

- tam bytecode VM kapsami
- user function ve import icin native VM lowering
- object field assignment
- list mutation API
- query/body parser'in daha zengin hale gelmesi
- async I/O
- plugin ABI
- package manager
- test runner
- formatter
- linter
- LSP
- gercek GPU compute ABI
- CUDA kernel execution

## 15. Yarinin Mantikli Devami

Bence en dogru sonraki adimlar:

1. VM backend'de `func/import/call` lowering
2. `web.post` icin body parsing ve response header kontrolu
3. object/list mutation
4. package registry
5. host embedding API'sini daha karli hale getirmek
6. GPU/CUDA tarafini capability detection'dan execution modeline tasimak

## 16. Kisa Tarif

Bugunku haliyle PyLua'yi en dogru su cumle anlatir:

`PyLua is a data-first native scripting language with an early web runtime, VM compatibility path, and Python embedding support.`
