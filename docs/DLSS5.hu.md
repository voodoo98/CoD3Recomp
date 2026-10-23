# DLSS 5 a CoD3 DX11-es változatában

A telepített lánc: ReShade 6.8.0 teljes add-on támogatással → LumeniteFX Kernel
optikai mozgásbecslés → DLSS5-Feeder 1.19.0 → Deep Fried Chicken 2.0 neural rendering.
A játék nem ad natív DLSS-mozgásvektorokat: a Kernel a kész képekből becsüli őket.
Emiatt gyors mozgásnál, áttetsző felületeknél és a felületen lehetnek képhibák.

Az `nvngx_dlssnr.dll` az `E:\Games\DLSS5\v2` mappából származik, változtatás nélkül.
A Super Resolution DLL az NVIDIA hivatalos DLSS-tárából letöltött, aláírt
310.9.16-os x64-es változat. Frame Generation és Ray Reconstruction nincs telepítve.
Az MSAA ki van kapcsolva, a DLAA végzi az élsimítást. A játék felbontása 3840×2160;
a neural-rendering munkafelbontása 50%, egy menettel. Ez nem FPS-növelési ígéret:
a neural-rendering további GPU-munkát végez.

## Használat

- **Home**: ReShade és a Deep Fried Chicken beállításai.
- **End**: a neural-rendering ki-/bekapcsolása. Kikapcsolva a Feeder DLAA tovább fut.
- **F11**: a játék saját beállításai, a korábban hozzáadott nyelvválasztóval együtt.

Az eredeti ReShade preset megmarad; az új összeállítás külön `DLSS5Preset.ini`-t használ.
A magyar fordítás és a mentések nem részei ennek a telepítésnek.

## CoD3 Scene Depth

A `tools/CoD3Depth` kiegészítő még a HUD mélységet felülíró rajzolásai előtt menti
el a jelenet mélységi pufferét. A fő jelenet nem négyzetes mélységi célpontjait
követi, a négyzetes és álló árnyéktérképeket kihagyja. A mentést legalább húsz
mélységtesztelt rajzolás után, a következő `ALWAYS`/depth-write rajzolás előtt készíti.
A tényleges scissor téglalapot kivágja, így az allokáció többletsorai nem nyúlnak
bele a képbe. Ezt köti a ReShade `DEPTH` bemenetére. Jelenet nélküli menüben semleges,
távoli mélységet ad. Más pályák eltérő renderútvonalait még érdemes játék közben ellenőrizni.

Fordítás x64-es Visual Studio developer parancssorból, a
[ReShade v6.8.0 SDK](https://github.com/crosire/reshade/tree/v6.8.0/include) fejlécével:

```powershell
cmake -S tools/CoD3Depth -B build/cod3-depth -G Ninja -DRESHADE_SDK=C:/path/to/reshade/include -DCMAKE_BUILD_TYPE=Release
cmake --build build/cod3-depth
```

## Telepítés és visszaállítás

A `scripts/install_dlss5.ps1` a már ellenőrzött helyi összeállítást másolja át;
nem tölt le és nem mellékel harmadik féltől származó DLL-eket vagy shadereket.
Alapértelmezett forrás: `build/dlss5-review/runtime`, cél: `build/CoD3Host`.
Minden binárisnál ellenőrzi az x64-es PE fejlécet, így a DLL-nek nevezett HTML/TXT
fájlokat elutasítja. A játékot előbb be kell zárni.

```powershell
./scripts/install_dlss5.ps1
./scripts/install_dlss5.ps1 -Restore
```

A telepítés előtt minden felülírt fájlról másolat készül a játék mappájában,
a `dlss5-backup/<időpont>` alatt. A `dlss5-install.json` tartalmazza az eredeti
fájlok és a telepített állományok SHA-256 ellenőrzőösszegeit. A visszaállítás a
telepítés előtti teljes ReShade- és játékbeállításokat is visszahozza, tehát a
telepítés óta módosított grafikai beállítások helyére a mentett változat kerül.
Az utólag keletkezett naplókat, képernyőképeket és a biztonsági másolatot megtartja.

## Ellenőrzés

A külön hostpróbában 300/300 NGX értékelés sikerült. Saint-Lô pályán külön,
tesztmentési mappával ellenőriztük a valódi játék képét: neural feature 18 létrejött,
neural frame sikeres, változó mélység, mozgó kameránál nem nulla mozgásvektorok.
Az RTX 4090 / 617.42 illesztőprogram mellett a 4K-s álló jelenet körülbelül 100 FPS-t
adott; ez egy jelenet mérése, nem a teljes játék teljesítményígérete.
A telepítés–visszaállítás próbája az eredeti INI-fájlokat bájtra pontosan visszaadta.

Diagnosztika: a játék mellett `ReShade.log`, `dlss5-feed.log` és
`deep-fried-chicken.log`. A CoD3Depth mentett jelenetmérete a ReShade naplóban látszik.
Összehasonlító PNG-k és a próbák naplói: `build/dlss5-review`.

Források: [DLSS5-Feeder](https://github.com/jlrouzies-fr/DLSS5-Feeder),
[NVIDIA DLSS runtime](https://github.com/NVIDIA/DLSS/tree/main/lib/Windows_x86_64/rel),
[ReShade SDK](https://github.com/crosire/reshade/tree/v6.8.0/include).
