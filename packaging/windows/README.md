# Windows Installer

PyLua icin Windows kurulum paketi uretmek icin:

```powershell
cd C:\pylua
powershell -ExecutionPolicy Bypass -File .\packaging\windows\create_installer.ps1
```

Uretilen dosya:

`C:\pylua\dist\windows\PyLua-Setup-0.1.0.exe`

Kurulum varsayilan olarak su dizine gider:

`%LOCALAPPDATA%\Programs\PyLua`

Kurulum sirasinda:

- wizard acilir
- lisans ekrani gosterilir
- kurulum klasoru secilebilir
- PATH icin checkbox vardir
- mevcut kurulum algilanir ve install/upgrade/repair modu secilir
- `install_manifest.json` ile surum bilgisi yazilir
- `bin\` PATH'e eklenir
- `PYLUA_HOME` user env var olarak yazilir
- Start Menu kisayollari olusturulur
