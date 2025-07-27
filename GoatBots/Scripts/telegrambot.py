# -*- coding: utf-8 -*-
"""
Created on Fri Jul  2 01:37:22 2021

@author: Marck
"""
from grafica import plotear
import telegram_send


def telegram(precio1, precio2):
    mensaje = "Mono Normal a dinero1 TIX" + "\n" + "Mono Promo a dinero2 TIX"
    mensaje = mensaje.replace("dinero1", precio1)
    mensaje = mensaje.replace("dinero2", precio2)
    telegram_send.send(messages=[mensaje])
    plotear()
    with open("monos.png", "rb") as f:
        telegram_send.send(images=[f])


# import time
# while True:
#     telegram('hola', 'adios')
#     time.sleep(30.0)
