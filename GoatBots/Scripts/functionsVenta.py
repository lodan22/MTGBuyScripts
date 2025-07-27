import matplotlib.pyplot as plt
import pandas as pd
import matplotlib.dates as md
import datetime as dt
import matplotlib.ticker as ticker
import locale
import numpy as np
import smtplib
import telegram_send
import requests
import pyvips
import re
import pytesseract
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from bs4 import BeautifulSoup
from PIL import Image


EMAIL_ADDRESS = "lodan.asl@gmail.com"
EMAIL_PASSWORD = "onpqgsswggwghzvx"
SEND_ADDRESS1 = "jorge.romero.rs@gmail.com"
SEND_ADDRESS2 = "lodan.asl@gmail.com"


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
    print(soup)
    promo = soup.find(attrs={"data-item": "A4zDSnxukUjjQw=="}).findNext(
        "div", class_="buy"
    )
    print(promo)
    IDpromo1 = str(promo).find("#")
    IDpromo2 = str(promo).find("use", IDpromo1)
    IDpromo = str(promo)[IDpromo1 : IDpromo2 - 4]

    # image_=soup.select(IDnormal,attrs={'data-item': "A4zDSnxukUrkQg=="},class_="sell")

    # image_2 = soup.select(IDpromo,attrs={'data-item': "A4zDSnxukUjjQw=="},class_="sell")
    image_2 = soup.select(IDpromo)
    imagen2 = str(image_2)
    guardarimg(imagen2, 3)


def guardarimg(imagen, numero):

    imagen = imagen.strip("[")
    imagen = imagen.strip("]")
    imagen = imagen.replace("symbol", "svg")

    imagen = imagen[:41] + r' height="2000" width="9000"' + imagen[41:]

    # imagen=imagen[:27]+"0 0 2314 500"+imagen[40:]

    with open(f"image_{numero}.svg", "w") as f:
        f.write(imagen)


def OCRfunction(precio):

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


def plotear():

    with open("registroFON.txt", "r") as f:
        data = f.readlines()
        f.close()

    with open("datosFON.txt", "w") as g:
        for i in range(0, len(data)):
            if i % 2 != 0:
                g.write(data[i])
        g.close()

    X = pd.read_csv(
        "datosFON.txt",
        sep="   ",
        engine="python",
        header=None,
        names=["Fecha", "Hora", "Normal", "Promo"],
    )

    # print(X.filter(items=["Fecha", "Hora", "Normal"]))
    # print(X.loc[[0]])
    ErrorNormaly = []
    ErrorPromoy = []
    ErrorNormalx = []
    ErrorNormalx1 = []
    ErrorPromox = []
    ErrorPromox1 = []
    aux1 = 0
    aux2 = 0
    for i in X.index:
        if X["Normal"].values[i] == " null":

            # ErrorNormaly.append(X["Normal"].values[i])
            ErrorNormalx1.append(X["Fecha"].values[i] + " " + X["Hora"].values[i])
            ErrorNormalx.append(
                dt.datetime.strptime(ErrorNormalx1[aux1], "%Y-%m-%d %H:%M:%S")
            )
            aux1 = aux1 + 1
        if X["Promo"].values[i] == " null":

            # ErrorPromoy.append(X["Promo"].values[i])
            ErrorPromox1.append(X["Fecha"].values[i] + " " + X["Hora"].values[i])
            ErrorPromox.append(
                dt.datetime.strptime(ErrorPromox1[aux2], "%Y-%m-%d %H:%M:%S")
            )
            aux2 = aux2 + 1

    X = X.replace(" null", np.nan)
    X = X.fillna(method="ffill")
    # X = X.applymap(str)
    # X.to_csv("xgboost.txt", header=False, index=False, sep="\t", mode="w")
    X["Promo"] = X["Promo"].apply(float)
    # print(X.dtypes)
    horita = []
    ejeynormal = []
    ejeypromo = []
    ejex = []
    for i in X.index:
        horita.append(X["Fecha"].values[i] + " " + X["Hora"].values[i])
        ejex.append(dt.datetime.strptime(horita[i], "%Y-%m-%d %H:%M:%S"))
        ejeynormal.append(X["Normal"].values[i])
        ejeypromo.append(X["Promo"].values[i])

    # for i in X.index:
    #     if ejeynormal[i]=='null':
    #         eje
    # datenums = md.date2num(ejex)

    for j in range(len(ejex)):
        for i in range(len(ErrorPromox)):
            if ejex[j] == ErrorPromox[i]:
                ErrorPromoy.append(ejeypromo[j])
    for j in range(len(ejex)):
        for i in range(len(ErrorNormalx)):
            if ejex[j] == ErrorNormalx[i]:
                ErrorNormaly.append(ejeypromo[j])

    plt.subplots_adjust(bottom=0.4)
    plt.xticks(rotation=90)

    # hola = range(43)

    # plt.xticks(hola, datenums[::2], rotation="vertical")  # set divisor
    # plt.locator_params(axis="x", nbins=len(datenums / 2))  # set divisor

    ax = plt.gca()
    # xfmt = md.DateFormatter("%Y-%m-%A %H:%M:%S")
    xfmt = md.DateFormatter("%A-%d-%m %H:%M")
    locale.setlocale(locale.LC_ALL, "es_ES.utf8")
    # ax.xaxis.set_major_locator(
    #     md.HourLocator(interval=10)
    # )  # to get a tick every 15 minutes
    ax.xaxis.set_major_formatter(xfmt)
    ax.tick_params(axis="x", labelsize=5)
    ax.xaxis.set_major_locator(ticker.MultipleLocator(0.1))
    plt.plot(ejex, ejeynormal, label="FON Normal")
    plt.plot(ejex, ejeypromo, label="FON Promo")
    plt.plot(ErrorNormalx, ErrorNormaly, "k|")
    plt.plot(ErrorPromox, ErrorPromoy, "k|", label="Out of Stock")
    plt.ylabel("TIX")
    plt.grid()
    ax.legend()
    plt.savefig("FON.png", dpi=600)
    plt.clf()


def notify_user(precio1, precio2):
    with smtplib.SMTP("smtp.gmail.com", 587) as smtp:
        smtp.ehlo()
        smtp.starttls()
        smtp.ehlo()

        smtp.login(EMAIL_ADDRESS, EMAIL_PASSWORD)
        # Create the base text message.

        message = MIMEMultipart("alternative")
        message["Subject"] = "A COMPRAR MONETES"
        message["From"] = EMAIL_ADDRESS
        message["To"] = SEND_ADDRESS2

        # text = """\
        # Hi,
        # How are you?
        # Real Python has many great tutorials:
        # www.realpython.com"""
        html1 = """\
        <p>&nbsp;</p>
        <div>
        <p style="font-size: 20px;">Los monetes est&aacute;n frescorros, frescorros.</p>
        <p style="font-size: 20px;">Monete Normal&nbsp;<span style="text-decoration: underline;"> dinero1 TIX;</span> y Monete Promo <span style="text-decoration: underline;"> dinero2 TIX;</span>.<br /><br />Yo que t&uacute; ni me lo pensaba.&nbsp;</p>
        </div>
        <div>
        <h2><br /><strong><a href="https://www.goatbots.com/card/ragavan-nimble-pilferer">A comprar!!!!!</a></strong><br /><br /></h2>
        </div>
        <div>
        <p>&nbsp;</p>
        </div>
        <div>
        <p style="text-align: right;"><br /><br /><br /><em>"Metes mono, sacas tripa"</em></p>
        </div>
        """

        html1 = html1.replace("dinero1", precio1)
        html1 = html1.replace("dinero2", precio2)

        # Turn these into plain/html MIMEText objects
        # part1 = MIMEText(text, "plain")
        part2 = MIMEText(html1, "html")
        # Add HTML/plain-text parts to MIMEMultipart message
        # The email client will try to render the last part first
        # message.attach(part1)
        message.attach(part2)

        smtp.sendmail(EMAIL_ADDRESS, SEND_ADDRESS2, message.as_string())
        print("Correo enviado.")


def telegram(precio1, precio2):
    mensaje = "FON Normal a dinero1 TIX" + "\n" + "FON Promo a dinero2 TIX"
    mensaje = mensaje.replace("dinero1", precio1)
    mensaje = mensaje.replace("dinero2", precio2)
    telegram_send.send(messages=[mensaje])
    plotear()
    with open("FON.png", "rb") as f:
        telegram_send.send(images=[f])


# plotear()
