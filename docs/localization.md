# Translating the game's text

The English text (subtitles, menus, objectives, hints, loading lines) lives in a string bank
at the end of every `.cod` archive. Two scripts take it out and put it back; no Python needed,
only Windows PowerShell 5.1.

```powershell
.\scripts\extract_subtitles.ps1               # -> localization\strings.json  (-ChunkSize 500 splits it)
# fill in the "hu" fields
.\scripts\pack_subtitles.ps1                  # -> localization\game_hu\...   (the game folder is untouched)
.\scripts\pack_subtitles.ps1 -Install         # copies them over the game, originals kept as *.cod.orig
.\scripts\pack_subtitles.ps1 -Restore         # puts the originals back
```

`-Game <folder>` (default `build\CoD3Host\game`), `-Scope sp|mp|all` work on both.

An entry:

```json
{"id": "14b1c781", "t": "5.8|9.9|", "en": "we had the armored unit up front.", "hu": "", "files": ["sp/saint_lo/saint_lo.cod"]}
```

* Fill in `hu`; empty means "leave English". `id`, `t`, `en` must stay as they are (the packer finds the string by them).
* `t` is a subtitle's timing (`start|end|` or `duration|`); the packer puts it back in front.
* Inside a subtitle `|` is a line break; `\n`, `[VERB_ATTACK]`-style button tags and `^1` colour codes must be kept.
* A string used in several archives is listed once, `files` says where.
* The text uses Windows-1252. Add `-HungarianAccents` when packing with the updated host to preserve ő/ű/Ő/Ű using its matching font patch; without this switch they fall back to ö/ü/Ö/Ü. Install the generated `hu_install.json` alongside the archives so the host enables the matching glyphs.
* Packing rebuilds the bank, so it may grow; the archive and its header are updated.
  Only the bank at the end of the file is used by the engine; older copies in the middle
  of some level archives are left alone.
