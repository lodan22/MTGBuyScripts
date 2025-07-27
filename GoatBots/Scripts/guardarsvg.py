# -*- coding: utf-8 -*-
"""
Created on Wed Jun 30 12:15:10 2021

@author: Marck
"""
import requests
from bs4 import BeautifulSoup
from guardarimg import guardarimg


def guardarsvg():

    # with open("ragavan.html", "r") as html_file:
    #     content = html_file.read()
    #     soup = BeautifulSoup(content, "html.parser")
    #     html_file.close()
    url = "https://www.goatbots.com/card/ragavan-nimble-pilferer#prm"
    r = requests.get(url)
    soup = BeautifulSoup(r.content, "html.parser")

    # Busco a los monos
    # Normal A4zDSnxukUrkQg==
    # Promo A4zDSnxukUjjQw==
    # datos normal
    # Ayudita soup.select("div[id=foo] > div > div > div[class=fee] > span > span > a")
    normal = soup.find(attrs={"data-item": "A4zDSnxukUrkQg=="}).find(
        "div", class_="sell"
    )

    promo = soup.find(attrs={"data-item": "A4zDSnxukUjjQw=="}).find(
        "div", class_="sell"
    )

    IDnormal1 = str(normal).find("#")
    IDnormal2 = str(normal).find("use", IDnormal1)
    IDnormal = str(normal)[IDnormal1 : IDnormal2 - 4]

    IDpromo1 = str(promo).find("#")
    IDpromo2 = str(promo).find("use", IDpromo1)
    IDpromo = str(promo)[IDpromo1 : IDpromo2 - 4]

    # image_=soup.select(IDnormal,attrs={'data-item': "A4zDSnxukUrkQg=="},class_="sell")
    image_ = soup.select(IDnormal)
    imagen = str(image_)
    guardarimg(imagen, 2)

    # image_2 = soup.select(IDpromo,attrs={'data-item': "A4zDSnxukUjjQw=="},class_="sell")
    image_2 = soup.select(IDpromo)
    imagen2 = str(image_2)
    guardarimg(imagen2, 3)
