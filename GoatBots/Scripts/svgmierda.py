# -*- coding: utf-8 -*-
"""
Created on Wed Jun 30 10:46:22 2021

@author: Marck
"""

import pyvips
import re
from OCR import OCRfunction


def textitobonito(imagensvg, numero):
    pyvips.cache_set_max(0)
    image = pyvips.Image.new_from_file(imagensvg, scale=0.1, memory=False)

    if numero == 2:

        image.write_to_file(
            "imagen_2.png",
        )
        textito = OCRfunction("imagen_2.png")

    else:
        image.write_to_file("imagen_3.png")
        textito = OCRfunction("imagen_3.png")

    textito = [float(s) for s in re.findall(r"-?\d+\.?\d*", textito)]

    try:
        gotdata = textito[0]
    except IndexError:
        gotdata = "null"

    if numero == 2:

        with open(f"texto2.txt", "w") as f:
            f.write(str(gotdata))
    else:
        with open(f"texto3.txt", "w") as f:
            f.write(str(gotdata))

    return gotdata


import time

# while True:
#     textitobonito("image_2.svg", 2)
#     time.sleep(60.0)
#     print("tick")
