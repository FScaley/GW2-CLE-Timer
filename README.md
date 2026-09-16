# Claymore Law Event Timer (CLE)

Guild Wars 2 oyun ici event timer addon'u. World boss, meta event, convergence, ley-line anomaly ve dragonstorm zamanlamalarini expansion bazli gruplar halinde gosterir.

## Kurulum

1. [Nexus](https://raidcore.gg/Nexus) kurulu olmali.
2. `claymore-event-timer.dll` dosyasini `addons/` klasorune kopyalayin.
3. Oyunu baslatip **ALT+E** ile pencereyi acin.

Ilk calistirmada `events.json` otomatik olarak addon dizinine yazilir. Guncelleme icin `events.json`'i degistirebilirsiniz.

## Ozellikler

- **Expansion bazli gruplama**: Core Tyria, Heart of Thorns, Path of Fire, Icebrood Saga, End of Dragons, Secrets of the Obscure, Janthir Wilds, Visions of Eternity
- **5 kategori**: World Boss, Meta Event, Convergence, Ley-Line Anomaly, Dragonstorm
- **Her boss ayri satir**: 13+ world boss ayri ayri gosterilir, en yakin spawn sirali
- **Chatlink kopyalama**: Satira tiklayarak waypoint kodunu panoya kopyalayabilirsiniz
- **Per-event toggle**: Options'tan istemediginiz eventleri kapatabilirsiniz
- **GW2 temasi**: PaleGoldenrod altin vurgulu, koyu seffaf arka plan
- **UTC + yerel saat**: Baslik cubugunda her iki saat gosterilir

## Kisayollar

- **ALT+E**: Pencereyi ac/kapat
- Nexus Quick Access'te CLE ikonu

## Bilinen Sinirlamalar

- **Leyspring Hollows (Depths of Cruelty)**: Wiki event timer verisinde henuz yok (15 Eylul 2026'da cikti, 3 saatlik dongu). Wiki guncellenince eklenecek.
- **Convergence haftalik boss rotasyonu**: Hangi boss'un aktif oldugu gosterilmiyor, sadece 3 saatlik portal zamanlayicisi.
- **LW1 public instances**: Twisted Marionette, Battle for Lion's Arch, Tower of Nightmares v1'de yok. Sadece Dragonstorm mevcut.
- **Meta highlight**: Tum meta fazlari yesil "AKTIF" gosterilir (wiki verisinde gap yok). Gelecek surumde sadece boss/odul fazlari yesil olacak.
- Festival eventleri (Dragon Bash, Halloween vb.) dahil degil.

## Veri Kaynagi

Event zamanlama verisi GW2 Wiki'nin `Widget:Event_timer/data.json` v5.2 dosyasindan gelmektedir. Bu, oyun ici `/wiki et` komutunun kullandigi ayni veridir.

## Derleme

Visual Studio 2022, C++17, x64:
```
msbuild src\GW2-CLE-Timer.vcxproj /p:Configuration=Release /p:Platform=x64
```

Test:
```
cd src
cl /EHsc /std:c++17 /permissive- /DNOMINMAX /MT /utf-8 /I"..\include" test_timer.cpp core\TimerEngine.cpp core\ConfigManager.cpp
test_timer.exe
```
