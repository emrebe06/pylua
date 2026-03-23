# Lunara Website

Bu klasor, Lunara ile uretilen yerel bir statik website ornegidir.

## Uretim

Kok klasorden calistir:

```powershell
.\build\Debug\lunara.exe .\website\build_site.lunara
```

Script su dosyalari uretir:

- `website/dist/index.html`
- `website/dist/styles.css`
- `website/dist/app.js`
- `website/dist/site_data.json`

## Lokal canli tutma

Siteyi yerelde ayakta tutmak icin:

```powershell
.\build\Debug\lunara.exe .\website\serve_site.lunara
```

Sonra tarayicidan `http://127.0.0.1:8080` adresini ac.

## Not

Site, veri katmanini `website/lib/site_content.lunara` dosyasindan alir. Yani tasarim statik, icerik Lunara modulu uzerinden gelir.

