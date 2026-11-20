
from winocr_helper import create_winocr_engine, winocr_available

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# CONFIGURACIÃ“N â€” editar segÃºn necesidad
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# Franja vertical donde aparece el texto del modo (calibrada con calibrar_zona.py)
#   ZONA_DESDE: 0.0 = tope de pantalla
#   ZONA_HASTA: fraccion de la altura total
ZONA_DESDE = 0.13
ZONA_HASTA = 0.15

# Factor de escalado para mejorar OCR en texto pequeÃ±o (2x = doble resoluciÃ³n)
ESCALA_OCR = 2
    "Attack and Defend - Trench Lines",
    "Attack and Defend - Trainwreck",
    "Attack and Defend - Monument",
    "Attack and Defend - Aftershock",
    "Attack and Defend - Akh Canal",
    "Attack and Defend - The Mog",
# EXTRACCION DE FRAMES Y BUSQUEDA
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

def extraer_texto_de_frame(frame, ocr_engine) -> str:
    """
    Recorta la zona de interÃ©s, escala y ejecuta WinOCR.
    Retorna texto con saltos de lÃ­nea (compatibilidad con lÃ³gica fuzzy existente).
    """
    alto, ancho = frame.shape[:2]
    recorte = frame[int(alto * ZONA_DESDE) : int(alto * ZONA_HASTA), :]

    if ESCALA_OCR > 1:
        recorte = cv2.resize(
            interpolation=cv2.INTER_LANCZOS4
        )

    return ocr_engine.recognize(recorte)


def buscar_texto_en_rango(ruta_video: str, log: logging.Logger, ocr_engine):
    cap = cv2.VideoCapture(ruta_video)
    if not cap.isOpened():
        log.error(f"No se pudo abrir el video: {ruta_video}")
            log.debug(f"  Segundo {segundo}s: no se pudo leer el frame.")
            continue

        texto = extraer_texto_de_frame(frame, ocr_engine)

        # Intento 1: fuzzy match contra lista de modos conocidos
        nombre_fuzzy = corregir_con_fuzzy(texto)

            texto_encontrado, segundo_exitoso = buscar_texto_en_rango(ruta_original, log, ocr_engine)

            if not texto_encontrado:
                log.warning("  OMITIDO: sin texto identificable.")
                sin_texto += 1
    log.info(f"  Ya correctos   : {ya_correctos}")
    log.info(f"  Sin resultado  : {sin_texto}")
    log.info(f"  Log guardado en: {log_path}")
    log.info("=" * 60)


if __name__ == "__main__":
    carpeta = sys.argv[1] if len(sys.argv) > 1 else CARPETA
    carpeta = os.path.abspath(carpeta)
    procesar_carpeta(carpeta)
