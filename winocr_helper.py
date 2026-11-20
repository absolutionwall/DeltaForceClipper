    def recognize(self, frame_bgr: np.ndarray) -> str:
        """
        Ejecuta OCR en un frame OpenCV (BGR).
        Retorna texto con saltos de lÃ­nea (compatibles con pytesseract output).
        """
        sb = self._frame_to_softwarebitmap(frame_bgr)
        return self._loop.run_until_complete(self._recognize_async(sb))
