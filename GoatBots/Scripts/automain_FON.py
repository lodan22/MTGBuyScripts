# -*- coding: utf-8 -*-
"""
Created on Wed Jun 30 13:24:39 2021

@author: Marck
"""

import os

# from guardarsvg import guardarsvg
# from svgmierda import textitobonito
# from correitogoat import notify_user
import time
from datetime import datetime, date

# from telegrambot import telegram
from functions import *

starttime = time.time()
compra = 70


while True:

    now = datetime.now()
    today = date.today()
    current_time = now.strftime("%H:%M:%S")
    fecha = str(today)
    guardarsvg()

    enviarccorreo = 0

    precio_normal = textitobonito("image_2.svg", 2)
    precio_promo = textitobonito("image_3.svg", 3)

    try:
        float(precio_promo)
    except ValueError:
        try:
            float(precio_normal)

        except ValueError:
            print("cagada")

    else:
        print("Promo " + str(precio_promo) + " TIX")
        try:
            float(precio_normal)

        except ValueError:
            if float(precio_promo) < compra:

                enviarccorreo = enviarccorreo + 1

        else:
            if float(precio_normal) < compra or float(precio_promo) < compra:

                enviarccorreo = enviarccorreo + 1

    try:
        float(precio_normal)

    except ValueError:
        print("cagada")

    else:
        print("Normal " + str(precio_normal) + " TIX")
        if float(precio_normal) < compra:

            enviarccorreo = enviarccorreo + 1

    if enviarccorreo > 0:
        telegram(str(precio_normal), str(precio_promo))
        # notify_user(str(precio_normal), str(precio_promo))

    with open("registroFON.txt", "a") as f:
        f.write(
            "\n"
            + fecha
            + "   "
            + current_time
            + "   "
            + str(precio_normal)
            + "    "
            + str(precio_promo)
            + "\n"
        )
        f.close()

    print("tick" + "   " + current_time)
    del precio_normal
    del precio_promo
    # os.remove("image_2.svg")
    # os.remove("image_3.svg")
    # os.remove("imagen_2.png")
    # os.remove("imagen_3.png")

    time.sleep(900.0 - ((time.time() - starttime) % 60.0))
