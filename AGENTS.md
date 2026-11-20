# AGENTS.md - Delta Force Clipper

## Arquitectura y Stack Real (NO CAMBIAR)
- **Plugin principal:** C++ / CMake / Qt6 para OBS Studio 32.2.2 (Windows x64) ubicado en `src/`.
- **Detector OCR:** Python (`winocr_helper.py` y `tools/killfeed_detector/`).
- **Dashboard y scripts:** `dashboard/` y scripts de compilación/release en `scripts/`.
- **Build y Empaquetado:** `Build-DeltaForceClipper.cmd`, `scripts/setup-compilation.ps1` y `Nueva release.cmd` (genera ZIP con CPack).

## Reglas Estrictas de Desarrollo
- **Alcance mínimo:** Resuelve los problemas modificando únicamente los archivos estrictamente necesarios. Nunca reescribas módulos enteros ni compiles código en otro lenguaje (prohibido migrar a Rust, Go, C#, etc.).
- **No tocar la carpeta `.git` ni el sistema de versiones:** Nunca borres `.git`, ni ejecutes `git push`, `git reset`, `git clean` o `git checkout -f`.
- **Detente ante bloqueos:** Si intentas arreglar un fallo 2 veces y persiste, DETENTE, explica el error exacto y espera instrucciones antes de probar un tercer enfoque.