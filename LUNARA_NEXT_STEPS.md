# Lunara Next Steps

Bu dosya GitHub vitrini icin degil, gelistirme yonu ve bir sonraki adimlari netlestirmek icin tutulur.

Yazar: Emre Demirbaş

Tarih: 23 Mart 2026

## Durum

Bugun itibariyla Lunara:

- native parser/interpreter hattina sahip
- CLI ile calisiyor
- erken asama VM uyumluluk yoluna sahip
- Python embed destegi sunuyor
- Windows installer uretebiliyor
- `.lunara` uzantisini kullaniyor
- temel web runtime, router ve middleware akisina sahip

Ama henuz erken asama bir dil/runtime projesidir.

## Geliştirmede Bir Sonraki Ana Alanlar

### 1. Dil seviyesi

- object field assignment
- list mutation API
- `break` ve `continue`
- daha zengin import modeli
- anonymous function / lambda
- pattern matching benzeri veri odakli kontrol yapilari

### 2. VM seviyesi

- `func` icin native lowering
- `import` icin native lowering
- `call` kapsamini genisletme
- object/list/loop davranislarini tam bytecode yoluna tasima
- saf interpreter fallback ihtiyacini azaltma

### 3. Web runtime seviyesi

- daha guclu request parser
- response headers kontrolu
- query/body/cookie ergonomisi
- route params
- POST/PUT/PATCH akislari
- middleware zinciri gelistirme
- basit template rendering

### 4. Paketleme ve dagitim

- repo adinin nihai karari
- release tagging
- release note sistemi
- installer ikonunu daha profesyonel hale getirme
- Inno Setup veya WiX tabanli installer’a gecis

### 5. Ekosistem

- package manager
- module registry
- test runner
- formatter
- linter
- language server

### 6. Host embedding

- Python API’yi daha ergonomik yapmak
- C ABI’yi genisletmek
- host tarafinda structured result donmek
- error / stack trace bilgisini guclendirmek

### 7. Guvenlik ve production hardening

- daha guclu sandbox stratejisi
- filesystem izin modeli
- daha iyi request limits
- config-driven security policy
- safer plugin/runtime boundaries

### 8. Compute / GPU tarafı

- capability detection’dan execution modeline gecis
- CUDA icin gercek runtime katmani
- compute API tasarimi
- parallel execution ve worker modeli

## Kisa Teknik Yol Haritasi

En mantikli gelisim sirasi:

1. VM kapsamını genişlet
2. web runtime’ı oturt
3. object/list mutasyonunu ekle
4. package/module sistemini kur
5. release/distribution katmanini profesyonellestir
6. embedding ve plugin API’yi guclendir

## Not

Bu dosya bilerek GitHub acik tanitim metni gibi yazilmadi.

Amac:

- projeyi iceriden takip etmek
- neyin bittiğini ve neyin sonraya kaldigini net tutmak
- gelistirme odagini dagitmamak
