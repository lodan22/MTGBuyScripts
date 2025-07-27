import matplotlib.pyplot as plt
import pandas as pd
import matplotlib.dates as md
import datetime as dt
import matplotlib.ticker as ticker
import locale
import numpy as np
import requests
from bs4 import BeautifulSoup
import lxml.html as html

# main > section > div > main > section > div > div > div > div:nth-child(3) > div > table > tbody > tr:nth-child(1) > td:nth-child(1) > div > div > a > span

# def guardarsvg():

# with open("lv1.html", "r") as html_file:
#     content = html_file.read()
#     soup = BeautifulSoup(content, "html.parser")
#     html_file.close()
url = "https://poe.ninja/challenge/skill-gems?level=1&corrupted=No"
r = requests.get(url)
soup = BeautifulSoup(r.content, "html.parser")
# soup = BeautifulSoup(r.content, "html.parser")

# Gemas
cosas = "//tbody/tr[1]/td[1]/div/div//a/span/text()"
nombre = '//*[@id="main"]/section/div/main/section/div/div/div/div[3]/div/table/tbody/tr[1]/td[1]/div/div/a/span'
# print(r.status_code)
# home = r.content
# parser = html.fromstring(home)
nombre_url = soup.find_element_by_xpath(nombre).get_attribute("wholeText")

print(len(nombre_url))
print(nombre_url)

# # Ayudita soup.select("div[id=foo] > div > div > div[class=fee] > span > span > a")

# normal = soup.find(attrs={"data-item": "A4zDSnxtkUXkQw=="}).find(
#     "div", class_="sell"
# )

# promo = soup.find(attrs={"data-item": "A4zDSnxukUjiSw=="}).find(
#     "div", class_="sell"
# )

# IDnormal1 = str(normal).find("#")
# IDnormal2 = str(normal).find("use", IDnormal1)
# IDnormal = str(normal)[IDnormal1 : IDnormal2 - 4]

# IDpromo1 = str(promo).find("#")
# IDpromo2 = str(promo).find("use", IDpromo1)
# IDpromo = str(promo)[IDpromo1 : IDpromo2 - 4]

# # image_=soup.select(IDnormal,attrs={'data-item': "A4zDSnxukUrkQg=="},class_="sell")
# image_ = soup.select(IDnormal)
# imagen = str(image_)
# guardarimg(imagen, 2)

# # image_2 = soup.select(IDpromo,attrs={'data-item': "A4zDSnxukUjjQw=="},class_="sell")
# image_2 = soup.select(IDpromo)
# imagen2 = str(image_2)
# guardarimg(imagen2, 3)
