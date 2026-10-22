# CoD3 magyarítás: szövegek kinyerése és visszacsomagolása

A játék összes angol szövege (feliratok, menük, küldetéscélok, tippek, betöltőképernyő-sorok)
a `.cod` archívumok végén lévő szövegblokkban van. Két script veszi ki és teszi vissza őket.
Csak Windows PowerShell 5.1 kell hozzá, Python nem.

## Fájlok

| Fájl | Mi ez |
| --- | --- |
| `localization/strings.en.json` | **Az eredeti angol szövegek mentése.** Ehhez ne nyúlj, referencia. |
| `localization/strings.json` | Az eredeti, üres `hu` mezőkkel kinyert munkasablon. |
| `localization/strings.hu.json` | A magyar fordítás: ezt kell szerkeszteni. A 2026. október 22-i nyelvi és katonai terminológiai ellenőrzés javításait tartalmazza. |
| `localization/REVIEW.hu.md` | Az ellenőrzés eredménye, a javításjegyzék és a biztonsági mentés elérhetősége. |
| `scripts/extract_subtitles.ps1` | Kinyeri a szövegeket a játékból. |
| `scripts/pack_subtitles.ps1` | Visszacsomagolja a fordítást a játék fájljaiba. |
| `scripts/CoD3Loc.cs` | A két script közös kódja. |

Tartalom: 29 `.cod` fájl (SP és MP), 9436 szöveg, ebből 5718 egyedi (az ismétlődők egyszer szerepelnek).

## Használat

```powershell
# 1. Kinyerés (felülírja a strings.json-t, ezért a fordítás megkezdése után ne futtasd újra!)
.\scripts\extract_subtitles.ps1
#    több fájlra bontva, pl. 500 bejegyzésenként: -ChunkSize 500
#    csak az SP vagy MP: -Scope sp | mp

# 2. Fordítás: a strings.hu.json "hu" mezőinek szerkesztése

# 3. Visszacsomagolás: az átírt .cod fájlok a localization\game_hu\ mappába kerülnek,
#    a játék mappájához nem nyúl
.\scripts\pack_subtitles.ps1 -Strings localization\strings.hu.json -HungarianAccents

# 4. Telepítés a játékba (az eredetiek *.cod.orig néven megmaradnak)
.\scripts\pack_subtitles.ps1 -Strings localization\strings.hu.json -HungarianAccents -Install

# Visszaállítás az eredetire
.\scripts\pack_subtitles.ps1 -Restore
```

Mindkét script `-Game <mappa>` kapcsolóval más játékmappát is megkap (alapértelmezés: `build\CoD3Host\game`).
A csomagoló több `strings_001.json`, `strings_002.json`, ... fájlt is egyben beolvas, ha mappát kap (`-Strings <mappa>`).

## Egy bejegyzés

```json
{"id": "14b1c781", "t": "5.8|9.9|", "en": "we had the armored unit up front.", "hu": "", "files": ["sp/saint_lo/saint_lo.cod"]}
```

- **`hu`**: ide kerül a magyar szöveg. Ha üres, a szöveg angol marad.
- **`id`, `t`, `en`**: ne változtasd. A csomagoló ezek alapján találja meg a szöveget.
- **`t`**: a felirat időzítése (`kezdés|vég|` vagy `időtartam|`). A csomagoló teszi vissza a szöveg elé.
- **`files`**: melyik archívumokban szerepel a szöveg.

## Szabályok a fordításhoz

- A `|` jel a feliratban sortörés, tartsd meg.
- Maradjon meg a `\n`, a gombcímkék (`[VERB_ATTACK]`, `[USE_RIGHT_STICK]` stb.) és a színkódok (`^1` stb.).
- A játék nem tördeli át a hosszú sort (kilógott a képernyőről), ezért a magyar szöveg hossza az angoléhoz közeli legyen.
- A `-HungarianAccents` kapcsolóval az **ő, ű, Ő, Ű** külön karakterként kerül a csomagba. Ehhez az új `CoD3.exe` betűkészlet-javítása kell; a kapcsoló nélkül a korábbi ö/ü helyettesítés működik.

## Hogyan működik

A blokk a fájl végén van, hash-táblával és rekordokkal. Visszacsomagoláskor a blokkot újraépíti a script
(a megosztott szövegeket is külön rekordba írja), így hosszabb szöveg esetén a blokk és a fájl nő,
a fejlécben lévő méreteket a script frissíti. Néhány pálya fájljában a blokk régebbi másolata a fájl
közepén is megvan, ezeket nem módosítja, mert a játék a fájl végén lévőt használja.
Kipróbálva a Saint-Lô pályán: a megnövelt blokkal a pálya betöltődik, és az új szöveg megjelenik.

## Telepítés és visszaállítás angolra

```powershell
scripts\pack_subtitles.ps1 -Strings localization\strings.hu.json -HungarianAccents -Install
scripts\pack_subtitles.ps1 -Restore   # vissza angolra (az .orig fájlok visszakerülnek, a hu_install.json törlődik)
```

Az `-Install` mindig az eredeti angol fájlokból építkezik (előbb visszaállít), ezért tetszőleges
számú alkalommal futtatható. A `game\hu_install.json` felsorolja az érintett fájlokat az eredeti
és a magyar változat SHA-256 hash-ével – ez használható a Cod3 Hun installerhez.

## Magyar hosszú ékezetek

Az F11 → Gameplay → Text language választóval az angol és a magyar játékbeli szöveg között lehet váltani. Az Apply után újra kell indítani a játékot. A választás a `CoD3Recomp.ini` `text_language = en` / `hu` beállításába kerül; a menüket, küldetéscélokat és feliratokat együtt váltja. Az angol mód az eredeti `.cod.orig` archívumokat olvassa, a magyar mód a telepített fordítást; egyik változatot sem írja felül.

A `CoD3Host/hungarian_fonts.cpp` a játék három eredeti bitmap betűkészletében készíti elő a hosszú ékezeteket, a kisebb mip-szinteken is. Az eredeti betűtesteket és ékezetvonalakat használja. A javítás a `hu_install.json` `font_encoding: "hu-double-acute-v1"` jelölése esetén aktív, az eredeti atlaszblokkok ellenőrzése után.

A négy magyar betű a D5/F5/DB/FB bájtot kapja; az egyéb Windows-1252 karakterek, például a Saint-Lô név ô betűje megmaradnak. A csomagoló ütközésként elutasítja a javításhoz lefoglalt Õ/õ/Û/û/Ò/ò/Ù/ù karaktereket a magyar szövegben.
