# Plan de MigraciÃ³n: Tesseract â†’ Windows.Media.OCR

## Objetivo
Reemplazar el motor OCR (Tesseract/pytesseract) por **Windows.Media.OCR** (WinOCR nativo) manteniendo toda la lÃ³gica de negocio: extracciÃ³n de frames, zona de recorte, fuzzy matching, renombrado y logging.

---

## Cambios TÃ©cnicos

### 1. Dependencias
- **Eliminar**: `pytesseract`, `pillow` (solo para conversiÃ³n a PIL en OCR)
- **AÃ±adir**: `windows-media-ocr` (o usar WinRT nativo `windows.media.ocr`)
- **Mantener**: `opencv-python` para extracciÃ³n de frames

def frame_to_softwarebitmap(frame_bgr: np.ndarray) -> SoftwareBitmap:
    """Convierte frame OpenCV (BGR) a SoftwareBitmap BGRA8 para WinOCR."""
    h, w = frame_bgr.shape[:2]
    # BGR -> BGRA
    bgra = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2BGRA)
    # Crear SoftwareBitmap y copiar buffer
    sb = SoftwareBitmap(BitmapPixelFormat.BGRA8, w, h, BitmapAlphaMode.PREMULTIPLIED)
    sb.copy_from_buffer(bgra.flatten())
    return sb

### 3. Motor OCR WinOCR
```python
# InicializaciÃ³n Ãºnica (reutilizar engine)
engine = OcrEngine.try_create_from_user_profile_languages()  # o .try_create_from_language(Language("en"))

async def ocr_softwarebitmap(sb: SoftwareBitmap) -> str:
    result = await engine.recognize_async(sb)
    # Unir lÃ­neas con salto de lÃ­nea (similar a pytesseract output)
    return "\n".join(line.text for line in result.lines)
```

### 4. IntegraciÃ³n en `extraer_texto_de_frame`
- Recortar zona (igual que ahora: `ZONA_DESDE`/`ZONA_HASTA`)
- Escalar 2x (Lanczos) para mejorar OCR en texto pequeÃ±o â†’ `cv2.resize`
- Convertir a SoftwareBitmap
- Llamar `engine.recognize_async` (async) â†’ requiere `asyncio.run` o loop

### 5. Punto de atenciÃ³n: Async
WinOCR es **async** (`recognize_async`). Opciones:
- **OpciÃ³n A**: `asyncio.run(ocr_softwarebitmap(sb))` en cada frame (simple, crea loop cada vez)
- **OpciÃ³n B**: Crear loop global al inicio, reutilizar (mÃ¡s eficiente)
- **OpciÃ³n C**: Wrapper sÃ­ncrono con `loop.run_until_complete`

RecomendaciÃ³n: **OpciÃ³n B** â€” crear event loop una vez en `procesar_carpeta`, pasarlo a `buscar_texto_en_rango`.

---


| Archivo | Cambios |
|---------|---------|
| `renombrar_videos.py` | Reemplazar imports, `extraer_texto_de_frame`, inicializaciÃ³n OCR, `buscar_texto_en_rango` para async |
| `requirements.txt` (nuevo) | `opencv-python`, `windows-media-ocr` |

---

## ValidaciÃ³n

1. **Test unitario rÃ¡pido**: Probar OCR en 1 frame conocido â†’ comparar salida vs Tesseract
2. **Test integraciÃ³n**: Procesar 3-5 videos de muestra â†’ verificar renombrado correcto
3. **Benchmark**: Medir tiempo/video antes/despuÃ©s (esperado: 3-5x mÃ¡s rÃ¡pido)
4. **Edge cases**: Video sin texto, video corrupto, nombre ya correcto, colisiÃ³n nombres

---


| Riesgo | MitigaciÃ³n |
|--------|------------|
| WinOCR no disponible en Windows versiÃ³n antigua | Verificar `OcrEngine.is_supported()` al inicio; fallback a Tesseract si falla |
| Async rompe flujo sÃ­ncrono actual | Encapsular en helper sÃ­ncrono con loop dedicado |
| Diferente salida de texto (salto de lÃ­nea, espacios) | Normalizar igual que antes (`replace("\n", " ").strip()`) |
| Idioma OCR (juego en inglÃ©s vs SO en espaÃ±ol) | Forzar `Language("en")` en `try_create_from_language` |

---
## Pasos de ImplementaciÃ³n

1. Crear `requirements.txt` con dependencias nuevas
2. Escribir helper `winocr_helper.py`: `frame_to_softwarebitmap`, `OcrEngine` singleton, `recognize_sync`
3. Modificar `renombrar_videos.py`:
   - Reemplazar imports y detecciÃ³n Tesseract
   - Nueva `extraer_texto_de_frame` usando WinOCR
   - Adaptar `buscar_texto_en_rango` para recibir/usar loop async
   - Mantener `corregir_con_fuzzy`, `encontrar_linea_con_clave`, logging, renombrado sin cambios
4. Probar con videos de muestra
5. Documentar uso (README o docstring)


## Fuera de Alcance

- Cambiar zona de recorte (usuario confirmÃ³ misma zona)
- Cambiar lÃ³gica fuzzy matching / naming convention
- GUI / CLI avanzada
- Procesamiento paralelo de videos (se puede aÃ±adir despuÃ©s)
