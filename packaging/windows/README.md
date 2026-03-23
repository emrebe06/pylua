# Windows Installer

Lunara icin Windows kurulum paketi uretmek icin:

```powershell
cd C:\lunara
powershell -ExecutionPolicy Bypass -File .\packaging\windows\create_installer.ps1
```

Uretilen dosya:

`C:\lunara\dist\windows\Lunara-Setup-0.1.0.exe`

Kurulum varsayilan olarak su dizine gider:

`%LOCALAPPDATA%\Programs\Lunara`

Kurulum sirasinda:

- wizard acilir
- lisans ekrani gosterilir
- kurulum klasoru secilebilir
- PATH icin checkbox vardir
- mevcut kurulum algilanir ve install/upgrade/repair modu secilir
- `install_manifest.json` ile surum bilgisi yazilir
- `bin\` PATH'e eklenir
- `LUNARA_HOME` user env var olarak yazilir
- Start Menu kisayollari olusturulur

