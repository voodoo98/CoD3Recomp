# Grafikai és hangjavítások átvétele

A javítások a [legoliamneeson-féle grafikai és hangcsomagból](https://github.com/hovg-N8/CoD3Recomp/pull/2)
származnak, a hovg-N8 fork `a3a93cffe5c9e866df44cbbf6dbb85e18587691e`
állapotából. A forrás eltérő alapra épül, ezért az átvétel a saját egyjátékos
futtatóhoz igazított változtatáscsomag, nem a teljes fork beolvasztása.

## Átvett működés

- Gyorsabb textúrakibontás, textúrapool és staging feltöltés; a menet közben
  változó textúrák és geometriapufferek pontosabb frissítése.
- Élesebb árnyékszűrés, MSAA melletti simább növényzetszélek és állítható
  füst-/effektfelbontás. A meglévő külön árnyékfelbontás megmarad; explicit
  árnyékfelbontás esetén az effektméretezés nem szorozza meg azt még egyszer.
- A felületekre rajzolt részletek mélységi eltolása, valamint a találati
  nyomok és nyomjelzők hibás indexeltolásának javítása. A vertexprogram
  adja hozzá az eltolást, a D3D draw base vertex értéke nulla.
- PNG/DDS textúracsomagok és PNG textúrakimentés. A kimentés alapból ki van
  kapcsolva; új textúraállományokat ez a commit nem tartalmaz. A fájlok az
  exe melletti `textures/dump` és `textures/replace` könyvtárakba kerülnek.
  A kapcsolók Apply után a már gyorsítótárazott textúrákat is frissítik.
- Opcionális scRGB HDR, 16 bites lebegőpontos rendercélokkal, RenoDX és Prism
  tónusleképezéssel. F11 → HDR, illetve RenoDX; a HDR alapból kikapcsolva.
  A játék és a kezelőfelület fényereje külön állítható. A szükséges shaderek
  be vannak építve az exe-be; a külső shaderkönyvtárak MIT-licence megmarad.
- A filmek megjelenítése a konzol TV-gammagörbéje nélkül, a HDR bloom és HUD
  fényerejének javítása, valamint a resolved képek piros/kék csatornáinak
  helyreállítása a megfelelő fetch swizzle esetén.
- Stereo/5.1 hangkimenet, automatikus választással és F11 → Controls → Speakers
  beállítással. A hangerő alapértéke 100%, így a korábbi beállításfájlok
  betöltése nem halkítja le a játékot.
- Közös erősítést használó hanglimiter a 16 bites mintákra alakítás előtt:
  gyors halkítás túlvezérléskor, körülbelül 200 ms-os visszaállás. A limiter
  alapból működik; `COD3_NOLIMITER=1` a korábbi levágást kéri.
- Pontosabb képkockakorlát, GPU-swapra ébresztett várakozás, gyorsabb
  konstansírás és shaderfelismerés; külön kapcsolható optimalizációk és
  renderelő-ellenőrzések az F11 menüben.

## Illesztés a saját ághoz

Multiplayer-futtató, hálózati kód, bot, kaszt, játékmód, fegyver-, mozgás-
vagy kontroller-játékmenetmódosítás nem került át. A magyarítás, nyelvválasztás,
egérkezelés, ADS-beállítások, kilépés és a meglévő DLSS5-fájlok megmaradnak.

A saját GPU-s mélység-visszaírás újabb a fork CPU-n konvertáló útvonalánál,
ezért azt megtartjuk. A fork ehhez tartozó CPU-s workerje és kapcsolója nem
került át. A shaderciklus javítása már az alapágban is szerepelt; az azt
helyettesítő régi, képernyőhatást letiltó kerülőmegoldást nem használjuk.

A forrásban fontosabb commitok:

| Terület | Forráscommitok |
|---|---|
| Képminőség, feltöltés és frame pacing | `4d2d8c6`, `5c4fb9e`, `3b64492`, `85ab7b7`, `f24233c` részben |
| Dinamikus geometria és ellenőrzések | `2bcb5d4`, `b32044e`, `2ce5fc3`, `289345c`, `b532bcc` részben, `bc5055b`, `c91235a`, `77de7fd`, `19e2070` |
| Filmek, HDR és textúrapack kezelőfelület | `fdf0b36`, `8d01f8d`, `b65630f`, `b2deac8`, `64c68b0` |
| Felületi részletek és lövedéknyomok | `e47acc1`, `0f6a6ba` |
| Hanglimiter és hangdiagnosztika | `fa15677`, `5c4fb9e` |
| Színcsatornák | `689c18e` |

A textúrapack betöltője a fork korábbi állapotában is szerepelt; csak annak
általános textúrakezelő fájljait és a szükséges renderelőhívásokat vettük át.

## Ellenőrzés

- A Windows CoD3 cél fordítása és linkelése sikeres.
- A limiter tesztje: plafon, csatornaarány, visszaállás, stereo hangerő,
  némítás, bypass és nem véges minták.
- Beállítások: régi INI betöltése, új hang-/grafikai értékek mentése és
  visszaolvasása, HDR-csúszkák, Preview mentés nélkül, érvénytelen értékek.
- A korábbi magyarítási, ragozási és nyelvválasztási regresszióteszt sikeres.
- Saint-Lô pályabetöltés és képernyőkép; Forest pálya SDR és HDR módban,
  lövés/célzás és szabályos kilépés. A HDR-shaderek futás közben is sikeresen
  fordultak. Windows alatt a kijelző HDR-je a teszt idején ki volt kapcsolva:
  az scRGB renderút működését ellenőriztük, fizikai HDR-megjelenítést nem.
- A textúrakimentés 366 PNG-t készített. Három saját, kimentett textúra
  visszatöltése a cserehelyről sikeres volt. Az SDR-próba teljes
  effektfelbontással futott.
- A rögzített hangok 48 kHz-esek, a stereo és a hatcsatornás felvétel is
  tartalmaz hangmintákat. A fizikai hangsugárzók elrendezését ez nem igazolja.

A nyers tesztfájlok és képek az ignorált `build/fork-review/validation` és
`build/fork-review/runtime` könyvtárakban maradnak; játékadatot nem commitolunk.

## További grafikai kapcsolók és számlálójavítás

Az F11 → Graphics lapra a fork `settings.cpp` és `overlay.cpp` fájljai alapján
négy további beállítás került: Fog, Draw distance, Particle effects és
Dynamic lights. A korábbi INI alapértékei megtartják a játék saját ködjét,
automatikus látótávolságát, összes effektjét és dinamikus fényeit. Apply után
újraindítás nélkül kerülnek a játék parancspufferébe a `r_fog`, `r_zfar`,
`fx_enable`, `fx_cullscale` és `r_dynamiclight` változók értékei. A beállításokat
az INI megőrzi. A részecske-effektek mennyisége és a már meglévő
effektfelbontás külön kapcsoló.

A [b532bcc számlálójavítás](https://github.com/hovg-N8/CoD3Recomp/commit/b532bcc0b0)
megmaradt része is átkerült a meglévő egyjátékos/közös naplózási pontokra.
A korlátozott diagnosztikai számlálók a határ elérése után nem növekednek
tovább, így nem fordulnak át, és az atomi inkrementálás sem terheli tovább
az érintett hívásokat. A futási statisztikák számlálói nem változnak.
Az inputfájl ütközésénél a saját gombnyomási időbélyeg-kezelés megmaradt;
a fork további kontrolleres naplózását nem vettük át.

Az A/V beállításteszt az új mezők alapértékeit, mentését, határértékeit,
a játékparancsok egyszeri kiküldését, az effektek kikapcsolását és az eredeti
grafikai állapot visszaállítását is ellenőrzi.
A Forest pálya 90 másodperces próbakörében a négy eltérő tesztbeállítás
parancsai is bekerültek a játék pufferébe; a futás szabályosan, 0-s kóddal
lépett ki. A Windows host fordítása és a korábbi regressziótesztek sikeresek.
