# Lunara Next Steps

Bu dosya GitHub vitrini icin degil, gelistirme yonu ve sonraki adimlari net tutmak icin tutulur.

Yazar: Emre Demirbas

Tarih: 25 Mart 2026

## Durum

Bugun itibariyla Lunara:

- native parser/interpreter hattina sahip
- CLI ile calisiyor
- erken asama VM uyumluluk yoluna sahip
- Python embed destegi sunuyor
- Windows installer uretebiliyor
- `.lunara` uzantisini kullaniyor
- gelisen web runtime, router, middleware ve websocket akisina sahip
- SQLUna tabanli auth/store ve backend ornekleri tasiyor
- script seviyesi SQLite query ve migration yardimcilari sunuyor
- ilk `dsp` stdlib genislemesini almis durumda

Ama halen buyume asamasinda bir dil/runtime projesidir.

## Gelistirmede Bir Sonraki Ana Alanlar

### 1. Dil seviyesi

- `break` ve `continue`
- anonymous function / lambda
- pattern matching benzeri veri odakli kontrol yapilari
- daha zengin import modeli
- standart kutuphane ergonomisini buyutme

### 2. VM seviyesi

- `func` icin native lowering
- `import` icin native lowering
- `call` kapsamini genisletme
- object/list/loop davranislarini tam bytecode yoluna tasima
- saf interpreter fallback ihtiyacini azaltma

### 3. Web runtime seviyesi

- auth/session/role middleware'i standart kutuphane seviyesinde olgunlastirma
- signed cookie ve bearer token akislarini daha iyi paketleme
- template/rendering katmanini buyutme
- multipart/form-data ve upload destegi
- background job / queue primitive'leri
- e-ticaret odakli backend kaliplari: catalog, cart, order, payment adapterleri

### 4. Ekosistem

- SQLUna icin daha genel Lunara query/repository API
- package manager
- module registry
- test runner
- formatter
- linter
- language server

### 5. Host embedding

- Python API'yi daha ergonomik yapmak
- C ABI'yi genisletmek
- host tarafinda structured result donmek
- error / stack trace bilgisini guclendirmek

### 6. Guvenlik ve production hardening

- daha guclu sandbox stratejisi
- filesystem izin modeli
- daha iyi request limits
- config-driven security policy
- safer plugin/runtime boundaries

### 7. Compute / GPU

- capability detection'dan execution modeline gecis
- CUDA icin gercek runtime katmani
- compute API tasarimi
- parallel execution ve worker modeli

### 8. DSP / data compute

- `dsp` modulunu filtreleme, transform ve streaming operatorleri ile buyutme
- audio/event stream pipeline primitive'leri
- realtime analytics ve feature extraction helper'lari
- sayisal sinyal isleme ve feature engineering'i backend verisiyle birlestirme

## Kisa Teknik Yol Haritasi

En mantikli gelisim sirasi:

1. web/backend katmanini uretim mantigina yaklastir
2. SQLUna script API'sini buyut
3. VM kapsamini genislet
4. package/module sistemini kur
5. DSP/compute tarafini derinlestir
6. release/distribution ve embedding katmanini profesyonellestir

## Not

Bu dosya bilerek acik tanitim metni gibi yazilmadi.

Amac:

- projeyi iceriden takip etmek
- neyin bittigini ve neyin sonraya kaldigini net tutmak
- gelistirme odagini dagitmamak
