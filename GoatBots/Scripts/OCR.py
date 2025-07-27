# -*- coding: utf-8 -*-
"""
Created on Tue Jun 29 17:36:12 2021

@author: Marck
"""


def OCRfunction(precio):
    # Importamos la libreria Pillow
    from PIL import Image

    # Importamos Pytesseract
    import pytesseract

    pytesseract.pytesseract.tesseract_cmd = (
        r"C:\Users\Marck\AppData\Local\Programs\Tesseract-OCR\tesseract"
    )
    # Abrimos la imagen
    im = Image.open(precio)

    # Utilizamos el método "image_to_string"
    # Le pasamos como argumento la imagen abierta con Pillow
    texto = pytesseract.image_to_string(im, lang="eng", config="--psm 7 --oem 1")

    # Mostramos el resultado
    return texto


# a = OCRfunction("imagen_2.png")

# print(str(a))
