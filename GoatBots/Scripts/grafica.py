import matplotlib.pyplot as plt
import pandas as pd
import matplotlib.dates as md
import datetime as dt
import matplotlib.ticker as ticker
import locale
import numpy as np


def plotear():
    # plt.clf()
    with open("registro.txt", "r") as f:
        data = f.readlines()
        f.close()

    with open("datos.txt", "w") as g:
        for i in range(0, len(data)):
            if i % 2 != 0:
                g.write(data[i])
        g.close()

    X = pd.read_csv(
        "datos.txt",
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
    ax.xaxis.set_major_locator(ticker.MultipleLocator(0.3))
    plt.plot(ejex, ejeynormal, label="Mono Normal")
    plt.plot(ejex, ejeypromo, label="Mono Promo")
    plt.plot(ErrorNormalx, ErrorNormaly, "k|")
    plt.plot(ErrorPromox, ErrorPromoy, "k|", label="Out of Stock")
    plt.ylabel("TIX")
    plt.grid()
    ax.legend()
    plt.savefig("monos.png", dpi=600)
    plt.clf()


# plotear()
