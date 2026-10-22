# A Call of Duty 3 magyar fordításának ellenőrzése

2026. október 22. – Az 5718 bejegyzést tartalmazó `strings.hu.json` átnézve az angol referencia alapján. **1450 bejegyzés javult**, az ismétlődő előfordulásokat is beleértve. A javított fordítást a játékba telepítettem.

## A fontosabb javítások

- Katonai terminológia: szanitéc, utász/műszaki katona, golyószóró, párhuzamosított géppuska, forgó-tolózár, töltőléc, kumulatív gránát, tarack, aknavető, féllánctalpas, páncélvadász, harccsoport, irányzó, töltőkezelő és segédvezető.
- Rendfokozatok: például a corporal tizedes, a sergeant őrmester, a private first class őrvezető; a korábbi téves törzsőrmesteri és tizedesi címek javítva.
- Harcászati parancsok: oldalbiztosítás, bekerítés, célmegjelölés, lefogótűz, fedezés, visszavonulás. A „boost” gyalogos helyzetben felsegítés, a Panzerschreck páncéltörő rakétavető.
- A „Jerry”, „Bosch” és „Kraut” az ellenségre utaló katonai szlengként „fricc” vagy „német”. A kanadai Leslie Baron vezetékneve megmarad Baron alakban; a Fekete Báró elnevezése külön kezelve.
- A veteráninterjúk több hosszabb, elcsúszott fordítási szakasza a hozzá tartozó angol mondatokhoz került vissza. A páncélozatlan járművek korábbi „puhafedelű” fordítása és a harckocsiszemélyzet szakszavai javítva.
- Több hiányzó vagy félrefordított menüszöveg, küldetéscél és idiomatikus fordulat javult. Például a „Smoke 'em if you got 'em” cigarettázást jelent, nem füstgránátdobást. Az amerikai emeletszámozás a magyar földszint/első emelet rendszerhez igazítva.

A bombákról szóló párbeszéd „collapsible circuit” kifejezése értelmező fordítást kapott: „megszakításérzékeny gyújtóáramkör”. A terminológia ellenőrzéséhez használt források: [ATF USBDC jelentés, „Collapsing Circuit” kapcsolótípus](https://www.atf.gov/media/14621/download), [a lengyel katonai egyetem könyvtárában elérhető szakanyag](https://pbio.akademia.mil.pl/wp-content/scans/usb2/usb2/20191007_przekazanie_001/03_PDF/73825.pdf). A játék eredeti történeti és fegyvertörténeti állításait nem ellenőriztem teljes körűen.

## Ellenőrzések és telepítés

- Az összes `id`, `t`, `en` és `files` mező egyezik az érintetlen `strings.en.json` referenciával. Csak a magyar szövegek módosultak.
- Minden gombjelölés, formátumparaméter, színkód és sortörésszám ellenőrizve. A szögletes zárójelbe írt interjú-megjegyzés fordítható szöveg, nem gombjelölés.
- 5690 kitöltött fordítás. A fennmaradó 28 bejegyzés eredetileg is csak szóközt tartalmaz; nincs üresen hagyott, érdemi angol szöveg.
- Az ő/ű/Ő/Ű megőrzése elkészült a csomagolóban és a natív futtatóban. A `-HungarianAccents` kapcsoló a megfelelő betűhelyeket használja; a három eredeti betűkészlet javítása a kisebb mip-szintekre is kiterjed. Mind az 5690 nem üres fordítás veszteség nélkül kódolható, a Saint-Lô név ô betűje is megmarad.
- A korábbi telepítés 29 eredeti mentésének és 29 telepített fájljának SHA-256 ellenőrzése sikeres volt az újracsomagolás előtt.
- 29 archívum újracsomagolva és telepítve, 9406 szövegelőfordulás behelyettesítve.
- A telepített archívumok **mind a 9436 rekordja** az eredeti angol archívumokból és a javított JSON-ból számolt elvárt szöveggel összehasonlítva: **0 eltérés**. Mind az 5690 fordítás megtalálható a játékban. A telepítés utáni eredeti és magyar fájlhash-ek is egyeznek a manifeszttel.
- A `build/CoD3Host/CoD3.exe` elindult és pályát töltött. A magyar szünetmenü szemrevételezve: az ellenőrzőponttal kapcsolatos hosszabb feliratok is elférnek. A próbaindítás folyamata később bezárult, ezért a játékot a munka végén újra elindítottam, és ellenőriztem, hogy válaszol. Teljes hadjárat-végigjátszás és minden felirat játék közbeni képi ellenőrzése nem történt.

## Mentések és javításjegyzék

- [Az átnézés előtti magyar fordítás](review/strings.hu.before.json)
- [Tételes javításjegyzék: angol szöveg, korábbi és javított magyar szöveg](review/changes.json)
- [A beépített rekordok ellenőrzésének összesítése](review/installed-verification.json)
- [Képernyőkép a futó játék magyar szünetmenüjéről](review/game-pause.png)
- [Az ő/ű/Ő/Ű játékbeli próbája az eredeti betűkészletekkel](review/hu-glyphs-in-game.png). Külön próbatelepítésben a menü két sora helyére került a kis- és nagybetűs tesztszöveg; a rendes fordításban a menü eredeti magyar feliratai maradtak.

Az eredeti angol archívumok `build/CoD3Host/game/**/*.cod.orig` néven megmaradtak; a telepítést a `game/hu_install.json` rögzíti.

A fegyvercsere jelzése „Tartsd nyomva …-t a … csere …” alakot használ. A célfegyver
„-ra/-re” ragját a futtató a konkrét fegyvernév behelyettesítése után választja ki:
például Kar98k-ra, MP40-re, MG34-re és P38-ra. A JSON-sablonban szereplő alapértelmezett
ragot ilyenkor helyesbíti, a szövegpuffer hosszának változtatása nélkül. A rifleman
megnevezés a felhasználó választása szerint „gyalogos” lett a „puskás” helyett.

Újratelepítés a javított forrásból:

```powershell
.\scripts\pack_subtitles.ps1 -Strings localization\strings.hu.json -HungarianAccents -Install
```

Visszaállítás az eredeti angol archívumokra:

```powershell
.\scripts\pack_subtitles.ps1 -Restore
```
