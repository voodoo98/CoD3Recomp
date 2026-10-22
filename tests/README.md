# Localization regression check

From the repository root in a Windows developer command prompt with clang-cl:

```bat
mkdir build\localization-tests
clang-cl /nologo /std:c++20 /EHsc /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I CoD3Host tests/localization_test.cpp CoD3Host/localization.cpp CoD3Host/settings.cpp /Fe:build/localization-tests/localization-tests.exe /Fo:build/localization-tests/ /link user32.lib
build\localization-tests\localization-tests.exe build\localization-tests
```

The check creates dummy archives and an INI in the output directory. It checks
language persistence, migration of existing Hungarian installations, the restart
boundary, missing language data, and archive routing without redirecting audio or
save files. It does not read or modify the installed game.

It also checks Hungarian weapon-swap suffixes in both directions, including
numbered models (Kar98k-ra, MP40-re, MG34-re, P38-ra), colour escapes,
punctuation, unchanged buffer length, and English-language isolation.
`COD3_EXPANDTEST=1` additionally prints two weapon-swap examples through the
running title's linked snprintf after twenty seconds, alongside the existing
button-placeholder expansion probes.
