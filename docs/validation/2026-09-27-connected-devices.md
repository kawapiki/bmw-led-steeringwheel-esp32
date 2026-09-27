# Két eszköz olvasási ellenőrzése – 2026-09-27

## Eredmény

Mindkét eszköz elérhető a számítógépről, a ROM letöltési kapcsolat és a flash kiolvasása/ellenőrzése működik. Firmware-írás, törlés, eFuse-módosítás vagy CAN-parancs nem történt. Az esptool átmeneti segédprogramot töltött RAM-ba; a vizsgálatok resetet is végeztek.

| Eszköz | Port ezen a gépen | Kiolvasott azonosítás | Flash |
|---|---|---|---|
| Kormány | COM6 | ESP32-S3 QFN56 v0.2, két mag, 240 MHz, 8 MB embedded PSRAM AP_3v3, 40 MHz kristály | Winbond EF4019, 32 MiB, quad flash, 3,3 V |
| LILYGO | COM7 | ESP32-D0WDQ6-V3 v3.1, két mag, 240 MHz, 40 MHz kristály; CH9102 USB–soros adapter | Winbond EF4016, 4 MiB |

A COM-számok csatlakoztatástól/géptől függhetnek. A PSRAM fenti adata chipazonosítás, nem futtatott RAM-teszt.

## Meglévő firmware és eltérések

LILYGO: az alkalmazás metaadata és egy nyolcmásodperces bootnapló egyaránt az `esp32_wifi_extender` projektet, `1` verziót és ESP-IDF `v6.0.2` verziót jelzi; fordítási idő: 2026-07-30 15:10:57. A firmware nem a saját gateway-alkalmazásunk.

LILYGO partíciók: NVS 0x9000/0x6000, PHY 0xF000/0x1000, factory alkalmazás 0x10000/0x100000. Nincs otadata vagy két OTA-slot. A saját gateway első USB-telepítéséhez külön, 4 MiB-hoz méretezett A/B partíciós terv szükséges; a kormány 6 MiB-os alkalmazáshelyei ide nem másolhatók át.

A teljes flash-összehasonlítás mindkét eszközön először eltérést mutatott. A bootloaderben egy munkameneten belül frissen kiolvasott konfigurációs tartománnyal korrigált helyi másolat viszont a teljes fizikai flash esptool MD5 digestjével egyezett:

| Eszköz | Különböző bájtok | Érintett 4 KiB-os szektorok | Következtetés |
|---|---:|---|---|
| LILYGO | 5980 | 0xB000, 0xD000, 0xE000 | Az első mostani mentés óta csak NVS változott; a többi tartalom egyezik |
| Kormány | 7952 | 0xB000, 0xD000, 0xE000 | A korábbi eredeti mentéshez képest csak NVS változott; a többi tartalom egyezik |

Ez tartalmi összehasonlítás, nem kriptográfiai eredethitelesítés. A futó eredeti firmware az újraindításkor módosíthatja az NVS-t. A mentés egy adott időpont állapotát őrzi, nem azt állítja, hogy később is bájtra azonos marad.

## Mentések

A változatlan eredeti kormánymentés továbbra is `back/steering-wheel-original.bin`, SHA-256:
`c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45`.

Új, helyi, Gitből kizárt pillanatmentések:

- `back/local/lilygo-verified-20260927.bin`: 4194304 bájt; SHA-256 `a15517044001ae4204357b0851d5c447eb5e6adc56678f8171bd91b8de4cbded`.
- `back/local/wheel-verified-20260927.bin`: 33554432 bájt; SHA-256 `d6d548bb21751683ed925eb6de457b2ad048eb3894e80a3864fb546b797e4f43`.

Ezek eszközspecifikus beállításokat tartalmazhatnak, ezért nem publikálandók automatikusan. Az aktív fejlesztői worktree-ben vannak; archiválása előtt az ignorált mentéseket külön meg kell őrizni.

## USB-megfigyelés

A COM6 többször Windows 31-es hibával megtagadta a megnyitást reset után, miközben az eszközkezelő OK állapotot jelzett. Fizikai USB-újracsatlakoztatás után a kommunikáció helyreállt. A Windows-eszköz újraindítását az operációs rendszer hozzáférésmegtagadással elutasította. A pontos ok még nem bizonyított; nem tekintjük stabilnak az ismételt automatikus reset/újracsatlakozást.

COM7-en a kiolvasás, digestellenőrzés és a reset utáni bootnapló sikeres volt.

## Amit ez még nem igazol

A saját firmware nem került egyik eszközre sem. A saját BLE recovery szolgáltatás, Wi-Fi credential-megosztás, páros OTA, rollback, TFT/DMA, LED/gomb/motor/szenzor működés és UI-válaszidő még nincs hardveren igazolva. A két csatlakoztatott eszköz lehetővé teszi ezek későbbi tesztjét, de előbb a megfelelő firmware-komponenseket és konkrét első-telepítési tervet kell elkészíteni.
