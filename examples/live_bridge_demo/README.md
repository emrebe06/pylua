# Lunara Live Bridge Demo

Bu ornek, browser tabanli frontend ile Lunara backend arasina bir kopru koyar.

Amac:

- cross-origin `fetch` akisini gostermek
- CORS preflight davranisini dogrulamak
- WebSocket ile canli mesajlasmayi gostermek
- frontend tarafinda tekrar kullanilabilir bir `LunaraBridge` sinifi vermek

## Portlar

- frontend: `http://127.0.0.1:8081`
- api: `http://127.0.0.1:8093`
- websocket: `ws://127.0.0.1:8094`

## Manuel calistirma

Uc ayri terminalde:

```powershell
.\build\Debug\lunara.exe .\examples\live_bridge_demo\frontend_server.lunara
.\build\Debug\lunara.exe .\examples\live_bridge_demo\api_server.lunara
.\build\Debug\lunara.exe .\examples\live_bridge_demo\websocket_server.lunara
```

Sonra tarayicidan su adresi ac:

`http://127.0.0.1:8081`

## Smoke test

Hazir derleme varsa su script her seyi gecici olarak ayaga kaldirir:

```powershell
powershell -ExecutionPolicy Bypass -File .\examples\live_bridge_demo\smoke_test.ps1
```

## Dosyalar

- `site/lunara_bridge.js`: frontend kopru katmani
- `site/app.js`: demo panel mantigi
- `api_server.lunara`: cross-origin API
- `websocket_server.lunara`: websocket echo runtime
- `frontend_server.lunara`: statik dosya servisi
