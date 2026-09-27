# BMW E90 kommunikációs kutatás

Aktuális tervezési korlát: a bekötés helye szabadon választható; a felhasználó a LILYGO panel hardveres módosítása nélkül szeretné teljesíteni az adatmegjelenítést és az ülésautomatikát. Az ülés alatti K-CAN nem kötelező bekötési pont.

Dátum: 2026-09-27. Céljármű: 2007-es E90, N52B25, iDrive nélkül. Autóoldali hardver: LILYGO T-CAN485, a fotón XY_32_CAN+RS485 V1.1 / 2024-4-25. Kormány: a repository README-jében azonosított ESP32-S3 panel.

Ez forráskutatás és műszaki elemzés, nem a saját autón végzett mérés. Gyártási hónap, váltó és az ülésmodul pontos változata még nem ismert. Egyetlen itt szereplő CAN-küldési funkció sincs a saját autón igazolva.

## Eredmény

A kijelzéshez és az automatika bemeneteihez használható nyilvános kiindulópontok léteznek. A memória 1/2 CAN-on történő előhívására az átnézett forrásokban nem találtam igazolt, teljes E90 parancssorozatot. Az ülésvezérlés ezért külön megvalósíthatósági feladat marad.

## Buszok és hardver

A BMW E90 oktatási anyag a K-CAN-t 100 kbit/s, a PT-CAN-t és az F-CAN-t 500 kbit/s hálózatként írja le. A JBE kapcsolja össze a K-CAN, PT-CAN és diagnosztikai hálózatokat. A K-CAN lezárása a résztvevő modulokban elosztott; nem alkalmazható rá automatikusan a nagysebességű CAN két végi 120 ohmos lezárási sémája. [BMW: Voltage Supply & Bus Systems, 22–26. nyomtatott oldal](https://www.e90post.com/forums/attachment.php?attachmentid=1584131)

A BMW WDS eltérő K-CAN jelszinteket és hibatűrő, egyvezetéken is működő adóvevőket ír le. A LILYGO dokumentált SN65HVD231 alkatrésze ISO 11898-2 nagysebességű CAN-adóvevő. Következtetés: az ESP32 megtartható, de a gyári CAN-csatlakozót nem tekintjük megfelelő K-CAN illesztésnek. A 100 kbit/s beállítás nem változtatja meg az elektromos fizikai réteget. [BMW WDS tükör](https://wds.psjr.org/en/e90/zinfo/DAA0701FB-656135001.htm), [LILYGO](https://github.com/Xinyuan-LilyGO/T-CAN485), [TI SN65HVD231](https://www.ti.com/product/SN65HVD231)

A TJA1055 egy hibatűrő, legfeljebb 125 kBd sebességű illesztőre példa. Konkrét kapcsoláshoz a változat, ESP32 felőli jelszintek, táp, lezáró/bias hálózat, ébresztés és a meglévő adóvevő leválasztása is ellenőrzendő. Ez nem kész bekötési javaslat. [NXP adatlap](https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf)

## Mit jelentenek a visszafejtett adatbázisok?

A DBC-ben a `BO_` üzenetazonosítót, hosszt és névleges küldőt ír le, az `SG_` pedig bithelyet, szélességet, bájtsorrendet és átszámítást. Egy üzenetnév önmagában nem dekódolás és nem végrehajtható parancs.

A letöltött nyers fájlok szöveges szerkezeti számlálása:

| Adatbázis | Felismert BO_ blokkok | Legalább egy SG_ mezővel |
|---|---:|---:|
| bmwcd/opendbc-BMW-E8x-E9x | 325 | 32 |
| nberlette/bmw-dbc | 66 | 26 |
| Maseg535 összevont DBC | 386 | 111 |

Ez nem teljes DBC-validálás és nem hitelességi pontszám: számláló vagy ismeretlen mező is SG_. A források részben közös eredetűek, ezért egyezésük nem feltétlenül független megerősítés. A vizsgált ágak változhatnak. [bmwcd nyers DBC](https://raw.githubusercontent.com/bmwcd/opendbc-BMW-E8x-E9x/master/bmw_e9x_e8x.dbc), [nberlette nyers DBC](https://raw.githubusercontent.com/nberlette/bmw-dbc/main/src/bmw-e90.dbc), [összevont nyers DBC](https://raw.githubusercontent.com/Maseg535/E90-and-e8x-DBC-mega-merge-project/main/bmw_e9x_e8x1_merged.dbc)

## Elsőként vizsgálandó adatok

| CAN ID | Rendeltetés / nyom | Értékelés |
|---|---|---|
| 0x0AA | Fordulatszám, gázpedál | Fordulatszámhoz működő KOMBI példakód is van |
| 0x130 | CAS kapocs-/gyújtásállapot | Ismert üzenet; mezők és hossz forrásonként eltérnek |
| 0x1A0 | Sebesség és mozgás | DBC-mezők léteznek, skálázás és érvényesség ellenőrzendő |
| 0x1D0 | Motoradatok / hőmérsékletek | Dekódolási jelöltek vannak, N52-n mérendő |
| 0x1D6 | Multifunkciós kormánygombok | Bitmezők és külön felhasználói projekt létezik |
| 0x0E2 / 0x0E6 / 0x0EA / 0x0EE | Ajtó/zár állapotok | Az ajtók hozzárendelése ellentmondásos |
| 0x1E1 / 0x2FC | Összesített ajtóállapot-jelöltek | Fizikai ajtónyitás és zárállapot külön ellenőrizendő |
| 0x34F | Kézifék | Kiegészítő feltétel; nem helyettesíti az állóhelyzetet |
| 0x3B4 | Feszültség | Nyilvános skálázások eltérnek |

A bmwcd projekt külön figyelmeztet, hogy az adatbázis nem rendeli minden üzenethez hozzá a fizikai buszt. Az ülés alatti K-CAN-on való jelenlétet ezért mérni kell; a gateway nem átlátszó másolat minden hálózat között. [Projektleírás](https://github.com/bmwcd/opendbc-BMW-E8x-E9x)

### Fordulatszám: konkrétan értelmezhető példa

A KOMBI-t vezérlő példakód a 0x0AA keret 4. és 5. bájtjába a fordulatszám négyszeresét teszi, kis bájttal kezdve. A bájtszámozás itt nullától indul:

`rpm = (B4 + 256 * B5) / 4`

Például 3000 rpm nyers értéke 12000 = 0x2EE0, tehát B4=0xE0, B5=0x2E. Ez a LED-fordulatszámjelzés jó kiindulópontja. A régebbi DBC-k 1-es szorzója és előjeles mezője ezzel nem egyezik; változtatás nélküli használatuk hibás kijelzést okozhat. [Működő KOMBI példakód](https://gist.github.com/gzoller/6c7395789ae377de68c4007477e9406b)

### Hőmérséklet és ajtó: feltételes jelöltek

Az összevont DBC 0x1D0-n B0−48 °C hűtőfolyadék- és B1−48 °C olajhőmérsékletet ír le. A saját N52-n a jel jelenlétét, invalid értékeit és diagnosztikai adattal való egyezését igazolni kell; a képlet önmagában nem bizonyít mért olajhőmérsékletet.

Ugyanez a forrás 0x0EA-t vezetőajtóként, B3=0xFC/0xFD értékeket csukott/nyitott állapotként jelöl, míg a nberlette adatbázis 0x0E2-t nevezi bal első ajtónak. A 0x2FC kommentje a fizikai nyitás és zárállapot összekeverésére is figyelmeztet. Ajtónkénti teszt szükséges. [Összevont DBC](https://raw.githubusercontent.com/Maseg535/E90-and-e8x-DBC-mega-merge-project/main/bmw_e9x_e8x1_merged.dbc), [nberlette DBC](https://raw.githubusercontent.com/nberlette/bmw-dbc/main/src/bmw-e90.dbc)

## Az ülésmemória működése és a hiányzó láncszem

A BMW leírásában a memóriás vezetőülés SMFA modulja közvetlenül olvassa az ülés- és memóriagombokat. A pozíciót a motorok Hall-jeleiből követi, a memóriahelyeket helyben tárolja. K-CAN-on többek között a külső tükrök memóriakéréseit továbbítja; a tükörpozíciókat az FRM tárolja. [BMW: General Vehicle Electrical II, 65–67. nyomtatott oldal](https://bimmerpost.com/forums/attachment.php?attachmentid=1584135&d=1488745477)

Ebből az következik, hogy a gombnyomással együtt megjelenő keret lehet az SMFA által küldött tükörkérés vagy állapotjelentés. Visszaküldése nem bizonyítja, hogy az ülés saját memóriahelyét is előhívja.

| ID | Nyilvános elnevezés | Használhatóság |
|---|---|---|
| 0x1F3 | Operation seat memory FA | Kutatási nyom |
| 0x20B | Memory adjustment | Küldőként SM_FA is szerepel; irány tisztázandó |
| 0x232 | Status FAS | Lehetséges visszajelzés |
| 0x3DA / 0x3DB | Memory positions CKM configuration/status | Konfigurációs nyom, nem igazolt recall-parancs |

A bmwcd és az összevont DBC e bejegyzései nem tartalmaznak SG_ mezőket. Nincs belőlük megállapítható teljes 1/2 kiválasztás, végrehajtási sorrend, időzítés, számláló vagy megszakítás. A MorGuux lista több modellváltozat küldőit is felsorolja, ezért az E90-re való megfelelés sem automatikus. [KCAN jegyzetek](https://gist.github.com/MorGuux/1f93228d5dde65fc7f81d78ddf405f99), [bmwcd DBC](https://raw.githubusercontent.com/bmwcd/opendbc-BMW-E8x-E9x/master/bmw_e9x_e8x.dbc)

Három vizsgálható megoldás: igazolt natív CAN-kérés; célzott SMFA diagnosztikai funkció, ha támogatott; vagy a gyári 1/2 gomb elektromos utánzása megfelelő külön illesztéssel. Az utóbbi megváltoztatja a kizárólag CAN-os bekötési elképzelést. Egyik megoldást sem tekintjük még bizonyítottnak.

## Passzív adatvétel és diagnosztika

A periodikus járműüzenetek vételéhez nem kell lekérdezést küldeni. A diagnosztika címzett kérés-válasz kommunikáció, saját időzítéssel, szállítási protokollal és szükség esetén munkamenettel. Egy nyers CAN-adóvevő önmagában nem helyettesít diagnosztikai protokollkezelőt.

Az EdiabasLib nyílt forrású megvalósítása D-CAN, BMW-FAST, KWP-2000 és más BMW protokollokat támogat, valamint PRG/GRP állományokat értelmez. Referencia a diagnosztikai réteghez, de nem kész ESP32 firmware és nem bizonyít SMFA memória-előhívási szolgáltatást. [EdiabasLib](https://github.com/uholeschak/ediabaslib)

A BMW 2007-es modellfrissítési anyaga több sorozatra 2007 márciusi/szeptemberi D-CAN átállást ír le, motorfüggően. Ezért az évszám önmagában nem dönti el a saját autó diagnosztikai interfészét. A K-line és a K-CAN külön hálózat. [BMW Service Information, D-CAN rész](https://5series.net/forums/attachments/e60-discussion-2/new-here-read-before-you-begin-148359/04_e60-e61-model-update-164775d1539352923)

## Használható projektek és korlátaik

- [veikkos/e90-can-cluster](https://github.com/veikkos/e90-can-cluster): műszeregység-padi meghajtás. Hasznos a keretek működésének ellenőrzéséhez, de egyetlen műszeregység sikeres meghajtása nem igazolja a teljes autó elektromos kompatibilitását.
- [BlueGenieBMW](https://github.com/BMW-E8x-E9x/BlueGenieBMW): CAN-alapú kormánygomb-kezelés Bluetooth-audio projekthez. Referencia a gyári gombokhoz, nem a kínai TFT panel két saját gombjához.
- [E46 Seat Memory K-Bus](https://github.com/handro123/E46_Seat_Memory_K-Bus): más generáció és más busz; az E90 CAN-kódjába közvetlenül nem átvehető.

## Javasolt rendszerfelosztás és igazolási sorrend

Saját tervezési következtetés:

1. A LILYGO CAN-kezelője időbélyeggel rögzíti a kereteket. Induláskor listen-only mód: nincs adás és nincs ACK-küldés.
2. Az E90-profil csak ellenőrzött mezőket emel járműállapottá. Minden adat mellett érvényesség és frissesség szerepel; a hiányzó sebesség nem nulla sebesség.
3. A BLE-réteg normalizált adatokat továbbít. A kormánykijelző nem függ a BMW CAN-ID-ktől.
4. Az ülésállapotgép külön modul. A kérések engedélyezése állóhelyzethez, friss kapocs- és ajtóállapothoz kötött. A motorleállás, gyújtáslevétel és buszalvás külön állapot.
5. Az ülésvezérlő adapter csak igazolt módszerrel kerülhet engedélyezésre. A CAN hardveres ACK nem jelenti az üléskérés elfogadását vagy a mozgás befejezését.

Mérések sorrendje: hardverillesztés ellenőrzése; címkézett passzív felvételek ajtó/gyújtás/indítás/1-es és 2-es memória műveletekről; jelöltek összevetése; lejátszásos dekóderteszt; kijelzés; csak utána az ülésparancs és megállítás ellenőrzése. Az alvást és nyugalmi fogyasztást külön vizsgálni kell. A motorjárást fordulatszámmal is össze kell vetni, nem elegendő egy bizonytalan nevű IgnitionOn bit.

A következő döntéshez szükséges adatok: gyártási hónap, váltó típusa, az ülés gyári/utólagos eredete és SMFA azonosítója. A kutatás nem módosította sem az autót, sem az eredeti firmware-backupot.

## Kiegészítő ellenőrzés: valóban plug-and-play a T-CAN485?

A felhasználó külön kérésére ellenőrzött állítás: a gyári T-CAN485 közvetlenül megfelelő-e a 2007-es E90 ülésmoduljának K-CAN hálózatára?

**Válasz:** gyári kialakításában nem tekinthető plug-and-play K-CAN interfésznek. Ez nem azt jelenti, hogy semmilyen BMW-modullal nem kommunikálhat, vagy hogy egyetlen keretet sem vehet. A BMW nagysebességű PT-CAN hálózatához a dokumentált adóvevő fizikai rétege megfelelő típusú; az üléshez tervezett K-CAN más eset. A konkrét panelen jelszintet, lezárást és kommunikációt nem mértünk.

### Gyártói bizonyítéklánc

1. A LILYGO README CAN-alkatrészként SN65HVD231-et ad meg. A V1.1 példányon a pontos beültetést nem igazoltuk méréssel; revízióspecifikus teljes kapcsolási rajzot a vizsgált anyagok között nem találtam. [Gyártói nyers README](https://raw.githubusercontent.com/Xinyuan-LilyGO/T-CAN485/arduino-esp32-libs_v3.0.1/README.md)
2. A TI adatlap az alkatrészt ISO 11898-2 nagysebességű CAN-adóvevőként specifikálja. Az alacsonyabb bitsebesség és a slope-control üzemmód nem teszi ISO 11898-3 hibatűrő adóvevővé. [TI adatlap, 1. és 4. oldal](https://www.ti.com/lit/ds/symlink/sn65hvd231.pdf)
3. A BMW WDS a K-CAN-nál hibatűrő működést, a PT-CAN-tól eltérő jelszinteket és eltérő lezárási viselkedést ír le. [BMW WDS](https://wds.psjr.org/en/e90/zinfo/DAA0701FB-656135001.htm)
4. Az NXP saját műszaki támogatása kimondja, hogy a két fizikai réteg nem kompatibilis: nemcsak a lezárás, hanem a CANH/CANL feszültségszintek is különböznek. [NXP TechSupport, 2019-04-02/03](https://community.nxp.com/t5/Other-NXP-Products/Loop-back-connection-between-High-speed-CAN-transceiver-and-Low/m-p/872693/highlight/true)

### Miért vannak mégis működő példák?

- A [floeplala F20 KOMBI projekt](https://github.com/floeplala/BMW-F20-KOMBI-Cluster-Bench-testing) ténylegesen T-CAN485-öt használ, de F20 műszeregységgel és F45-ről gyűjtött adatokkal. Ez nem E90 K-CAN kompatibilitási teszt.
- A [veikkos E90 KOMBI projekt](https://github.com/veikkos/e90-can-cluster) SN65HVD230-at is példaként említ, és a CAN-adapter 120 ohmos lezárásának eltávolítását írja elő. Ez valódi ellenpélda a túl erős „ilyen chippel soha nem működhet” állításra. A projekt azonban próbapadi műszeregységről szól, nem teljes autós hálózat, buszalvás és hibamódok minősítéséről.
- Egy [E90 próbapad készítője](https://e46canbus.blogspot.com/2016/12/e90-test-bench.html) ezzel szemben az MCP2551-gyel tapasztalt megbízhatatlan K-CAN működésről számol be, amelyet TJA1055 használatával oldott meg. Ez személyes mérnöki tapasztalat, nem gyártói minősítés, de összhangban van a fizikai réteg eltérésével.

Műszaki értelmezés: a differenciális vevő egyes körülmények között megkülönböztetheti a másik fizikai réteg logikai állapotait. Ettől a meghajtás, buszterhelés, időzítés és hibatűrés még nem válik kompatibilissé. A konkrét sikeres próbák okát hullámformák és kapcsolások nélkül nem lehet biztosan megállapítani. Passzív vétel sikere sem bizonyít kétirányú kompatibilitást; normál CAN-módban a vezérlő ACK-val is ad, külön alkalmazási küldés nélkül.

### Következmény a saját rendszerhez

| Cél | Döntés |
|---|---|
| Ülés alatti K-CAN, gyári T-CAN485 csatlakozó | Nem igazolt és nem megfelelő plug-and-play tervezési alap |
| K-CAN megfelelő külső hibatűrő illesztővel | Megtartható LILYGO/ESP32 alap, de hardveres illesztés szükséges |
| PT-CAN | Az adóvevő megfelelő fizikai osztályú; bekötés, lezárás, táp és firmware még tervezendő |
| OBD diagnosztika | A tényleges D-CAN/K-line változat és protokoll dönti el; nem helyettesíti az ülés K-CAN kapcsolatát |

A megfelelő K-CAN adóvevő nem egyszerű sorkapocs mögé kötött passzív átalakító: az ESP32 TX/RX oldalához kell illeszteni, vagy aktív átjáró szükséges. Az alaplapi adóvevő és a kivezetések figyelembevételével kell megtervezni; kész bekötést a mostani fotóból nem állítunk elő. Nincs még indok új board vásárlására, de a kizárólag gyári sorkapcsos K-CAN bekötés ígéretét nem támasztják alá a források.

## Szabadon választott bekötési pont, változatlan panel

A PT-CAN a dokumentált CAN-adóvevőhöz illő busz, ezért a boardot nem indokolt általánosan BMW-hez alkalmatlannak minősíteni. A teljes cél teljesülése viszont még nincs igazolva: az SMFA elérhetősége, memória-előhívási szolgáltatása, a szükséges ajtóállapot és leállítás utáni működés a választott bekötési pont felől külön vizsgálandó. A gateway jelenléte nem bizonyít tetszőleges kerettovábbítást. D-CAN-os autón az OBD felőli diagnosztika szintén vizsgálandó irány, de az SMFA elérhetősége még nem bizonyít memóriahely-választási funkciót.

Új hardverbizonyíték: a gyártói README Project hivatkozásán elérhető teljes kapcsolási rajzot letöltöttem, és a CAN+RS485 oldalt képként is ellenőriztem. Az U1 SN65HVD231 mellett RY2=120R közvetlenül a CANH/CANL közé kapcsolódik, kapcsoló nélkül. A rajz dátuma 2026-01-23; a felhasználó paneljén 2024-04-25/V1.1 látszik, tehát a beültetés egyezése nem tekinthető bizonyítottnak. [Gyártói kapcsolási rajz](https://github.com/Xinyuan-LilyGO/T-CAN485/blob/arduino-esp32-libs_v3.0.1/project/T-CAN485.pdf)

Ha a saját példányon is fix 120 ohmos CAN-lezárás van, meglévő, két 120 ohmos végellenállással lezárt PT-CAN-ra harmadik lezárást tenne: ideális eredő 40 ohm a korábbi 60 helyett. Egy másik párhuzamos rácsatlakozási pont ezt nem oldja meg. Ebből nem következik biztos kommunikációképtelenség, de feltétel nélküli, módosításmentes bekötés nem ígérhető. A konkrét beültetés tisztázása szükséges a végső hardverdöntéshez.
