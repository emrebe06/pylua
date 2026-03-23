# PyLua Kurulum ve Kullanım Rehberi

Yazar ve Geliştiren: Emre Demirbaş

Belge tarihi: 23 Mart 2026

İmza:
Emre Demirbaş

## 1. Bu belge ne için var

Bu rehber, PyLua dil motorunu başka bir Windows laptopta kurmak, çalıştırmak, güncellemek, kaldırmak ve temel seviyede kullanmak için hazırlanmıştır.

Bu belge özellikle şu sorulara cevap verir:

- PyLua nedir
- Kurulum dosyası nerede
- Setup sihirbazı ne yapar
- Hangi klasöre kurulur
- PATH ekleme seçeneği ne işe yarar
- Upgrade ve uninstall nasıl çalışır
- Hangi dosyalar kurulur
- Kurulum hakları ve lisans tarafı nedir
- Kurulumdan sonra ilk komutlar nelerdir

## 2. PyLua nedir

PyLua, `.pylua` dosyalarını doğrudan çalıştıran native bir scripting dili ve runtime motorudur.

Bugünkü haliyle PyLua şu alanlarda kullanılabilir:

- veri üretme
- JSON encode/decode
- script tabanlı otomasyon
- statik site build etme
- mini web backend denemeleri
- Python içinden embed edilme
- CLI ile script çalıştırma
- VM uyumluluk yolu ile daha hızlı çalıştırma denemeleri

## 3. Kurulum dosyası

Hazır Windows kurulum dosyası:

[PyLua-Setup-0.1.0.exe](C:/pylua/dist/windows/PyLua-Setup-0.1.0.exe)

Bu dosya başka bir laptopa kopyalanıp doğrudan çalıştırılabilir.

## 4. Sistem gereksinimleri

Önerilen hedef ortam:

- Windows 10 veya Windows 11
- x64 sistem
- kullanıcı profilinde yazma izni
- `%LOCALAPPDATA%` altında kurulum yapabilme yetkisi

Not:

- Varsayılan kurulum yönetici yetkisi istemeden kullanıcı alanına kurulacak şekilde tasarlanmıştır.
- Eğer farklı bir klasöre kuracaksan, o klasörde yazma iznin olması gerekir.

## 5. Setup sihirbazı ne yapar

PyLua setup sihirbazı açıldığında şu akışı sunar:

1. Karşılama ekranı
2. Lisans ekranı
3. Kurulum klasörü seçme ekranı
4. `Add to PATH` seçeneği
5. Mevcut sürüm algılama
6. Kurulum tamamlama

Bu wizard artık şunları destekler:

- özel PyLua ikonu
- lisans kabul ekranı
- kurulum klasörü seçme
- tek tık `Add to PATH`
- mevcut kurulum için install/upgrade/repair ayrımı
- uninstall kısayolu

## 6. Kurulum sırasında görülen modlar

Kurulum motoru hedef klasörü kontrol eder ve duruma göre mod seçer:

### Fresh install

PyLua daha önce o klasöre kurulmamışsa uygulanır.

### Upgrade

Eski bir PyLua sürümü bulunduysa ve yeni setup daha güncelse uygulanır.

### Repair / Reinstall

Aynı sürüm zaten kuruluysa uygulanır.

### Downgrade

Daha yeni bir sürüm kuruluysa ve daha eski setup çalıştırılıyorsa bu bilgi gösterilir.

## 7. Varsayılan kurulum yolu

Varsayılan kurulum klasörü:

`%LOCALAPPDATA%\Programs\PyLua`

Örnek:

`C:\Users\KULLANICI\AppData\Local\Programs\PyLua`

## 8. Add to PATH seçeneği ne yapar

Kurulum ekranındaki `Add to PATH` seçeneği işaretlenirse:

- PyLua `bin` klasörü kullanıcı PATH değişkenine eklenir
- yeni terminal açıldığında `pylua` komutu daha rahat kullanılabilir

Bu sayede örneğin şunu yazabilirsin:

```powershell
pylua version
```

Eğer işaretlenmezse PyLua yine kurulur, ama komutu tam dosya yoluyla çağırman gerekir.

Örnek:

```powershell
C:\Users\KULLANICI\AppData\Local\Programs\PyLua\bin\pylua.exe version
```

## 9. Kurulumun yazdığı temel şeyler

Kurulum sırasında genel olarak şunlar oluşturulur:

- `bin\pylua.exe`
- `bin\pylua_embed.dll`
- `docs\`
- `examples\`
- `python\`
- `website\`
- `include\`
- `uninstall.ps1`
- `uninstall.cmd`
- `install_manifest.json`
- `PyLua.ico`

## 10. install_manifest.json ne işe yarar

Kurulum sonunda bir manifest dosyası oluşturulur.

Bu dosya şunları taşır:

- uygulama adı
- sürüm
- kurulum klasörü
- PATH seçimi
- install mode
- varsa mevcut eski sürüm
- kurulum zamanı

Bu sayede:

- upgrade algılama
- repair ayrımı
- uninstall tarafında sürüm bilgisi

daha düzenli çalışır.

## 11. Start Menu kısayolları

Kurulum sonrası Start Menu altında PyLua için kısayollar oluşur:

- PyLua Guide
- PyLua CLI
- Uninstall PyLua

## 12. Kurulum hakları ve lisans notu

Bu preview sürüm için installer tarafında görülen lisans metni şunu anlatır:

- PyLua değerlendirme ve geliştirme amacıyla kullanılabilir
- kendi makinelerinde kurulup çalıştırılabilir
- kaynak kod üzerinde yerel geliştirme yapılabilir
- final production release gibi sunulmamalıdır
- garanti verilmez

Bu lisans metni şu dosyada tutulur:

[LICENSE.txt](C:/pylua/packaging/windows/LICENSE.txt)

İleride istenirse bu lisans:

- MIT
- Apache-2.0
- GPL
- ticari lisans

gibi bir modele çevrilebilir.

## 13. Kurulum sonrası ilk kontrol

Kurulum tamamlandıktan sonra yeni bir terminal aç.

Sonra şunları dene:

```powershell
pylua version
pylua help
```

Eğer PATH eklemediysen tam yol ile:

```powershell
C:\Users\KULLANICI\AppData\Local\Programs\PyLua\bin\pylua.exe version
```

Beklenen çıktı:

```text
pylua 0.1.0
```

## 14. İlk script nasıl çalıştırılır

Örnek script:

```pylua
print("Merhaba PyLua")
```

Bunu `hello.pylua` olarak kaydettiğini düşünelim.

Çalıştırma:

```powershell
pylua hello.pylua
```

Alternatif:

```powershell
pylua run hello.pylua
```

## 15. Örnek komutlar

```powershell
pylua version
pylua help
pylua check .\examples\hello.pylua
pylua run .\examples\hello.pylua
pylua vm .\examples\vm_demo.pylua
```

## 16. Python embed desteği

PyLua sadece CLI ile değil, Python içinden de kullanılabilir.

İlgili dosyalar:

- [pylua_embed_ctypes.py](C:/pylua/python/pylua_embed_ctypes.py)
- [pylua_bridge.py](C:/pylua/python/pylua_bridge.py)

Bu katmanlar şunlar için uygundur:

- FastAPI
- Flask
- worker süreçleri
- backend otomasyon işleri

## 17. Web runtime notu

PyLua bugün mini web runtime tarafında da bazı yeteneklere sahiptir:

- statik dosya sunma
- router
- middleware
- text/json response

Örnek router dosyası:

[router_demo.pylua](C:/pylua/examples/router_demo.pylua)

## 18. Güvenlik notları

Kurulum ve runtime tarafında şu temel korumalar vardır:

- path traversal bloklama
- güvenli path birleştirme
- constant-time string compare
- token üretimi
- request boyutu sınırı
- method guard

Ama dürüst not:

Bu sürüm henüz tam production hardening seviyesinde değildir.

## 19. GPU, CUDA ve FSL notu

PyLua’da bugün şu modüller vardır:

- `compute`
- `cpu`
- `gpu`
- `cuda`
- `fsl`

Bugünkü durum:

- CPU bilgisi alınabilir
- GPU/CUDA ortam algılama yapılabilir
- FSL ile güvenli dosya katmanı kullanılabilir

Ama:

- CUDA kernel execution henüz yok
- tam GPU compute runtime henüz yok

Yani bu katmanlar şu anda “capability + platform utility” seviyesindedir.

## 20. Uninstall nasıl yapılır

Kaldırma için iki yol vardır.

### Start Menu üzerinden

`Uninstall PyLua` kısayolunu çalıştır.

### Script ile

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Users\KULLANICI\AppData\Local\Programs\PyLua\uninstall.ps1"
```

Kaldırma işlemi:

- PATH kaydını temizler
- `PYLUA_HOME` değişkenini temizler
- Start Menu kısayollarını kaldırır
- kurulum klasörünü siler

## 21. Setup exe yeniden nasıl üretilir

Eğer kendi makinen üzerinde yeni installer üretmek istersen:

```powershell
cd C:\pylua
powershell -ExecutionPolicy Bypass -File .\packaging\windows\create_installer.ps1
```

İlgili üretim scripti:

[create_installer.ps1](C:/pylua/packaging/windows/create_installer.ps1)

## 22. Sorun giderme

### Sorun: `pylua` komutu bulunamıyor

Sebep:

- PATH eklenmemiş olabilir
- eski terminal açık olabilir

Çözüm:

- yeni terminal aç
- gerekirse tam dosya yoluyla çalıştır

### Sorun: Kurulum klasörüne yazamıyor

Sebep:

- seçtiğin klasörde izin yok

Çözüm:

- varsayılan kullanıcı klasörüne kur
- ya da yazma iznin olan başka bir klasör seç

### Sorun: Setup açılıyor ama kurulum olmuyor

Kontrol et:

- antivirüs script extraction’ı engelliyor mu
- `%LOCALAPPDATA%` erişimi var mı
- PowerShell policy çok kısıtlı mı

### Sorun: Upgrade beklerken reinstall oldu

Sebep:

- eski kurulumda manifest eksik olabilir

Çözüm:

- bu sürümden sonra manifest yazıldığı için sonraki kurulumlar daha düzgün upgrade algılar

## 23. Bu rehberden sonra okunabilecek dosyalar

- [PYLUA_GUIDE.md](C:/pylua/PYLUA_GUIDE.md)
- [README.md](C:/pylua/README.md)
- [packaging/windows/README.md](C:/pylua/packaging/windows/README.md)

## 24. Kısa teknik özet

Bugün itibarıyla PyLua:

- kurulabilir
- setup wizard ile gelir
- CLI içerir
- VM uyumluluk yoluna sahiptir
- Python embed desteği taşır
- mini web runtime taşır
- Windows üzerinde başka laptopta denenebilir durumdadır

## 25. Kapanış

Bu belge, PyLua’nın bugünkü kurulabilir ve kullanılabilir sürümü için resmi pratik rehber olarak düşünülmüştür.

Geliştiren:
Emre Demirbaş

İmza:
Emre Demirbaş
