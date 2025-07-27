from selenium import webdriver
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options
import time
import pandas as pd


nivel_1 = "https://poe.ninja/challenge/skill-gems?level=1&quality=20&corrupted=No"
nivel_5 = "https://poe.ninja/challenge/skill-gems?level=5&corrupted=No"
nivel_20 = "https://poe.ninja/challenge/skill-gems?level=20&corrupted=No"


def get_options():

    options = Options()
    # options.add_experimental_option("detach", True)
    options.add_experimental_option("excludeSwitches", ["enable-logging"])
    return options


def gemitas(putitolink):
    driver_path = "C:\\Users\\Marck\Downloads\\chromedriver_win32\\chromedriver.exe"
    driver = webdriver.Chrome(executable_path=driver_path, options=get_options())

    driver.get(putitolink)
    WebDriverWait(driver, 10)

    gemas = driver.find_elements(
        by=By.XPATH,
        value="//tbody/tr/td/div/div//a/span",
    )

    # print(gemas[1].text)

    precios = driver.find_elements(
        by=By.XPATH,
        value="//tbody/tr/td[@class='sorted sorted-desc css-aolo3q']/span",
    )

    # print(precios[0].text)

    monedas = driver.find_elements(
        by=By.XPATH,
        value="//tbody/tr/td[@class='sorted sorted-desc css-aolo3q']/span[@title]",
    )

    # print(monedas[0].get_attribute("title"))
    for i in range(len(gemas)):
        gemas[i] = gemas[i].text
        precios[i] = precios[i].text
        monedas[i] = monedas[i].get_attribute("title")
        total = []
        total.append(gemas)
        total.append(precios)
        total.append(monedas)
    return total


# print(gemitas(nivel_1)[2])
df = pd.DataFrame()
for j in gemitas(nivel_1):
    df_uno = pd.DataFrame(j)
    df = pd.concat([df, df_uno])

df.head()
