# Lunara Kurulum ve KullanÄ±m Rehberi

Yazar ve GeliÅŸtiren: Emre DemirbaÅŸ

Belge tarihi: 23 Mart 2026

Ä°mza:
Emre DemirbaÅŸ

## 1. Bu belge ne iÃ§in var

Bu rehber, Lunara dil motorunu baÅŸka bir Windows laptopta kurmak, Ã§alÄ±ÅŸtÄ±rmak, gÃ¼ncellemek, kaldÄ±rmak ve temel seviyede kullanmak iÃ§in hazÄ±rlanmÄ±ÅŸtÄ±r.

Bu belge Ã¶zellikle ÅŸu sorulara cevap verir:

- Lunara nedir
- Kurulum dosyasÄ± nerede
- Setup sihirbazÄ± ne yapar
- Hangi klasÃ¶re kurulur
- PATH ekleme seÃ§eneÄŸi ne iÅŸe yarar
- Upgrade ve uninstall nasÄ±l Ã§alÄ±ÅŸÄ±r
- Hangi dosyalar kurulur
- Kurulum haklarÄ± ve lisans tarafÄ± nedir
- Kurulumdan sonra ilk komutlar nelerdir

## 2. Lunara nedir

Lunara, `.lunara` dosyalarÄ±nÄ± doÄŸrudan Ã§alÄ±ÅŸtÄ±ran native bir scripting dili ve runtime motorudur.

BugÃ¼nkÃ¼ haliyle Lunara ÅŸu alanlarda kullanÄ±labilir:

- veri Ã¼retme
- JSON encode/decode
- script tabanlÄ± otomasyon
- statik site build etme
- mini web backend denemeleri
- Python iÃ§inden embed edilme
- CLI ile script Ã§alÄ±ÅŸtÄ±rma
- VM uyumluluk yolu ile daha hÄ±zlÄ± Ã§alÄ±ÅŸtÄ±rma denemeleri

## 3. Kurulum dosyasÄ±

HazÄ±r Windows kurulum dosyasÄ±:

[Lunara-Setup-0.1.0.exe](C:/lunara/dist/windows/Lunara-Setup-0.1.0.exe)

Bu dosya baÅŸka bir laptopa kopyalanÄ±p doÄŸrudan Ã§alÄ±ÅŸtÄ±rÄ±labilir.

## 4. Sistem gereksinimleri

Ã–nerilen hedef ortam:

- Windows 10 veya Windows 11
- x64 sistem
- kullanÄ±cÄ± profilinde yazma izni
- `%LOCALAPPDATA%` altÄ±nda kurulum yapabilme yetkisi

Not:

- VarsayÄ±lan kurulum yÃ¶netici yetkisi istemeden kullanÄ±cÄ± alanÄ±na kurulacak ÅŸekilde tasarlanmÄ±ÅŸtÄ±r.
- EÄŸer farklÄ± bir klasÃ¶re kuracaksan, o klasÃ¶rde yazma iznin olmasÄ± gerekir.

## 5. Setup sihirbazÄ± ne yapar

Lunara setup sihirbazÄ± aÃ§Ä±ldÄ±ÄŸÄ±nda ÅŸu akÄ±ÅŸÄ± sunar:

1. KarÅŸÄ±lama ekranÄ±
2. Lisans ekranÄ±
3. Kurulum klasÃ¶rÃ¼ seÃ§me ekranÄ±
4. `Add to PATH` seÃ§eneÄŸi
5. Mevcut sÃ¼rÃ¼m algÄ±lama
6. Kurulum tamamlama

Bu wizard artÄ±k ÅŸunlarÄ± destekler:

- Ã¶zel Lunara ikonu
- lisans kabul ekranÄ±
- kurulum klasÃ¶rÃ¼ seÃ§me
- tek tÄ±k `Add to PATH`
- mevcut kurulum iÃ§in install/upgrade/repair ayrÄ±mÄ±
- uninstall kÄ±sayolu

## 6. Kurulum sÄ±rasÄ±nda gÃ¶rÃ¼len modlar

Kurulum motoru hedef klasÃ¶rÃ¼ kontrol eder ve duruma gÃ¶re mod seÃ§er:

### Fresh install

Lunara daha Ã¶nce o klasÃ¶re kurulmamÄ±ÅŸsa uygulanÄ±r.

### Upgrade

Eski bir Lunara sÃ¼rÃ¼mÃ¼ bulunduysa ve yeni setup daha gÃ¼ncelse uygulanÄ±r.

### Repair / Reinstall

AynÄ± sÃ¼rÃ¼m zaten kuruluysa uygulanÄ±r.

### Downgrade

Daha yeni bir sÃ¼rÃ¼m kuruluysa ve daha eski setup Ã§alÄ±ÅŸtÄ±rÄ±lÄ±yorsa bu bilgi gÃ¶sterilir.

## 7. VarsayÄ±lan kurulum yolu

VarsayÄ±lan kurulum klasÃ¶rÃ¼:

`%LOCALAPPDATA%\Programs\Lunara`

Ã–rnek:

`C:\Users\KULLANICI\AppData\Local\Programs\Lunara`

## 8. Add to PATH seÃ§eneÄŸi ne yapar

Kurulum ekranÄ±ndaki `Add to PATH` seÃ§eneÄŸi iÅŸaretlenirse:

- Lunara `bin` klasÃ¶rÃ¼ kullanÄ±cÄ± PATH deÄŸiÅŸkenine eklenir
- yeni terminal aÃ§Ä±ldÄ±ÄŸÄ±nda `lunara` komutu daha rahat kullanÄ±labilir

Bu sayede Ã¶rneÄŸin ÅŸunu yazabilirsin:

```powershell
lunara version
```

EÄŸer iÅŸaretlenmezse Lunara yine kurulur, ama komutu tam dosya yoluyla Ã§aÄŸÄ±rman gerekir.

Ã–rnek:

```powershell
C:\Users\KULLANICI\AppData\Local\Programs\Lunara\bin\lunara.exe version
```

## 9. Kurulumun yazdÄ±ÄŸÄ± temel ÅŸeyler

Kurulum sÄ±rasÄ±nda genel olarak ÅŸunlar oluÅŸturulur:

- `bin\lunara.exe`
- `bin\lunara_embed.dll`
- `docs\`
- `examples\`
- `python\`
- `website\`
- `include\`
- `uninstall.ps1`
- `uninstall.cmd`
- `install_manifest.json`
- `Lunara.ico`

## 10. install_manifest.json ne iÅŸe yarar

Kurulum sonunda bir manifest dosyasÄ± oluÅŸturulur.

Bu dosya ÅŸunlarÄ± taÅŸÄ±r:

- uygulama adÄ±
- sÃ¼rÃ¼m
- kurulum klasÃ¶rÃ¼
- PATH seÃ§imi
- install mode
- varsa mevcut eski sÃ¼rÃ¼m
- kurulum zamanÄ±

Bu sayede:

- upgrade algÄ±lama
- repair ayrÄ±mÄ±
- uninstall tarafÄ±nda sÃ¼rÃ¼m bilgisi

daha dÃ¼zenli Ã§alÄ±ÅŸÄ±r.

## 11. Start Menu kÄ±sayollarÄ±

Kurulum sonrasÄ± Start Menu altÄ±nda Lunara iÃ§in kÄ±sayollar oluÅŸur:

- Lunara Guide
- Lunara CLI
- Uninstall Lunara

## 12. Kurulum haklarÄ± ve lisans notu

Bu preview sÃ¼rÃ¼m iÃ§in installer tarafÄ±nda gÃ¶rÃ¼len lisans metni ÅŸunu anlatÄ±r:

- Lunara deÄŸerlendirme ve geliÅŸtirme amacÄ±yla kullanÄ±labilir
- kendi makinelerinde kurulup Ã§alÄ±ÅŸtÄ±rÄ±labilir
- kaynak kod Ã¼zerinde yerel geliÅŸtirme yapÄ±labilir
- final production release gibi sunulmamalÄ±dÄ±r
- garanti verilmez

Bu lisans metni ÅŸu dosyada tutulur:

[LICENSE.txt](C:/lunara/packaging/windows/LICENSE.txt)

Ä°leride istenirse bu lisans:

- MIT
- Apache-2.0
- GPL
- ticari lisans

gibi bir modele Ã§evrilebilir.

## 13. Kurulum sonrasÄ± ilk kontrol

Kurulum tamamlandÄ±ktan sonra yeni bir terminal aÃ§.

Sonra ÅŸunlarÄ± dene:

```powershell
lunara version
lunara help
```

EÄŸer PATH eklemediysen tam yol ile:

```powershell
C:\Users\KULLANICI\AppData\Local\Programs\Lunara\bin\lunara.exe version
```

Beklenen Ã§Ä±ktÄ±:

```text
lunara 0.1.0
```

## 14. Ä°lk script nasÄ±l Ã§alÄ±ÅŸtÄ±rÄ±lÄ±r

Ã–rnek script:

```lunara
print("Merhaba Lunara")
```

Bunu `hello.lunara` olarak kaydettiÄŸini dÃ¼ÅŸÃ¼nelim.

Ã‡alÄ±ÅŸtÄ±rma:

```powershell
lunara hello.lunara
```

Alternatif:

```powershell
lunara run hello.lunara
```

## 15. Ã–rnek komutlar

```powershell
lunara version
lunara help
lunara check .\examples\hello.lunara
lunara run .\examples\hello.lunara
lunara vm .\examples\vm_demo.lunara
```

## 16. Python embed desteÄŸi

Lunara sadece CLI ile deÄŸil, Python iÃ§inden de kullanÄ±labilir.

Ä°lgili dosyalar:

- [lunara_embed_ctypes.py](C:/lunara/python/lunara_embed_ctypes.py)
- [lunara_bridge.py](C:/lunara/python/lunara_bridge.py)

Bu katmanlar ÅŸunlar iÃ§in uygundur:

- FastAPI
- Flask
- worker sÃ¼reÃ§leri
- backend otomasyon iÅŸleri

## 17. Web runtime notu

Lunara bugÃ¼n mini web runtime tarafÄ±nda da bazÄ± yeteneklere sahiptir:

- statik dosya sunma
- router
- middleware
- text/json response

Ã–rnek router dosyasÄ±:

[router_demo.lunara](C:/lunara/examples/router_demo.lunara)

## 18. GÃ¼venlik notlarÄ±

Kurulum ve runtime tarafÄ±nda ÅŸu temel korumalar vardÄ±r:

- path traversal bloklama
- gÃ¼venli path birleÅŸtirme
- constant-time string compare
- token Ã¼retimi
- request boyutu sÄ±nÄ±rÄ±
- method guard

Ama dÃ¼rÃ¼st not:

Bu sÃ¼rÃ¼m henÃ¼z tam production hardening seviyesinde deÄŸildir.

## 19. GPU, CUDA ve FSL notu

Lunaraâ€™da bugÃ¼n ÅŸu modÃ¼ller vardÄ±r:

- `compute`
- `cpu`
- `gpu`
- `cuda`
- `fsl`

BugÃ¼nkÃ¼ durum:

- CPU bilgisi alÄ±nabilir
- GPU/CUDA ortam algÄ±lama yapÄ±labilir
- FSL ile gÃ¼venli dosya katmanÄ± kullanÄ±labilir

Ama:

- CUDA kernel execution henÃ¼z yok
- tam GPU compute runtime henÃ¼z yok

Yani bu katmanlar ÅŸu anda â€œcapability + platform utilityâ€ seviyesindedir.

## 20. Uninstall nasÄ±l yapÄ±lÄ±r

KaldÄ±rma iÃ§in iki yol vardÄ±r.

### Start Menu Ã¼zerinden

`Uninstall Lunara` kÄ±sayolunu Ã§alÄ±ÅŸtÄ±r.

### Script ile

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Users\KULLANICI\AppData\Local\Programs\Lunara\uninstall.ps1"
```

KaldÄ±rma iÅŸlemi:

- PATH kaydÄ±nÄ± temizler
- `LUNARA_HOME` deÄŸiÅŸkenini temizler
- Start Menu kÄ±sayollarÄ±nÄ± kaldÄ±rÄ±r
- kurulum klasÃ¶rÃ¼nÃ¼ siler

## 21. Setup exe yeniden nasÄ±l Ã¼retilir

EÄŸer kendi makinen Ã¼zerinde yeni installer Ã¼retmek istersen:

```powershell
cd C:\lunara
powershell -ExecutionPolicy Bypass -File .\packaging\windows\create_installer.ps1
```

Ä°lgili Ã¼retim scripti:

[create_installer.ps1](C:/lunara/packaging/windows/create_installer.ps1)

## 22. Sorun giderme

### Sorun: `lunara` komutu bulunamÄ±yor

Sebep:

- PATH eklenmemiÅŸ olabilir
- eski terminal aÃ§Ä±k olabilir

Ã‡Ã¶zÃ¼m:

- yeni terminal aÃ§
- gerekirse tam dosya yoluyla Ã§alÄ±ÅŸtÄ±r

### Sorun: Kurulum klasÃ¶rÃ¼ne yazamÄ±yor

Sebep:

- seÃ§tiÄŸin klasÃ¶rde izin yok

Ã‡Ã¶zÃ¼m:

- varsayÄ±lan kullanÄ±cÄ± klasÃ¶rÃ¼ne kur
- ya da yazma iznin olan baÅŸka bir klasÃ¶r seÃ§

### Sorun: Setup aÃ§Ä±lÄ±yor ama kurulum olmuyor

Kontrol et:

- antivirÃ¼s script extractionâ€™Ä± engelliyor mu
- `%LOCALAPPDATA%` eriÅŸimi var mÄ±
- PowerShell policy Ã§ok kÄ±sÄ±tlÄ± mÄ±

### Sorun: Upgrade beklerken reinstall oldu

Sebep:

- eski kurulumda manifest eksik olabilir

Ã‡Ã¶zÃ¼m:

- bu sÃ¼rÃ¼mden sonra manifest yazÄ±ldÄ±ÄŸÄ± iÃ§in sonraki kurulumlar daha dÃ¼zgÃ¼n upgrade algÄ±lar

## 23. Bu rehberden sonra okunabilecek dosyalar

- [LUNARA_GUIDE.md](C:/lunara/LUNARA_GUIDE.md)
- [README.md](C:/lunara/README.md)
- [packaging/windows/README.md](C:/lunara/packaging/windows/README.md)

## 24. KÄ±sa teknik Ã¶zet

BugÃ¼n itibarÄ±yla Lunara:

- kurulabilir
- setup wizard ile gelir
- CLI iÃ§erir
- VM uyumluluk yoluna sahiptir
- Python embed desteÄŸi taÅŸÄ±r
- mini web runtime taÅŸÄ±r
- Windows Ã¼zerinde baÅŸka laptopta denenebilir durumdadÄ±r

## 25. KapanÄ±ÅŸ

Bu belge, Lunaraâ€™nÄ±n bugÃ¼nkÃ¼ kurulabilir ve kullanÄ±labilir sÃ¼rÃ¼mÃ¼ iÃ§in resmi pratik rehber olarak dÃ¼ÅŸÃ¼nÃ¼lmÃ¼ÅŸtÃ¼r.

GeliÅŸtiren:
Emre DemirbaÅŸ

Ä°mza:
Emre DemirbaÅŸ

