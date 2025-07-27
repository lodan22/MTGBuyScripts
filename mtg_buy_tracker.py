#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import json
import csv
import time
import datetime
import asyncio
import requests
import pandas as pd
import matplotlib.pyplot as plt
from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from bs4 import BeautifulSoup
from telegram import Bot
from telegram.error import TimedOut
from telegram.request import HTTPXRequest
import numpy as np  
# ——— Carga de configuración desde config.json ———
CONFIG_FILE = os.getenv('CONFIG_FILE', 'config.json')
try:
    with open(CONFIG_FILE, encoding='utf-8') as f:
        config = json.load(f)
except Exception as e:
    print(f"[ERROR] No se pudo cargar {CONFIG_FILE}: {e}")
    exit(1)

# ——— Flags desde config.json ———
SEND_TELEGRAM = config.get('send_telegram', True)
DEBUG         = config.get('debug', False)

def debug(msg: str):
    if DEBUG:
        print(f"[DEBUG] {msg}", flush=True)

def info(msg: str):
    print(f"[INFO] {msg}", flush=True)

# ——— Parámetros desde config.json ———
TOKEN_BOT1       = config['telegram_bot1_token']
TOKEN_BOT2       = config['telegram_bot2_token']
CARDTRADER_TOKEN = config['cardtrader_token']
TOP_N            = config['top_n']
CHAT_ID          = config['chat_id']

# ——— Rutas y constantes ———
JOBS_CONFIG      = os.getenv('JOBS_CONFIG', 'jobs.json')
HISTORY_CSV      = os.getenv('HISTORY_CSV', 'history.csv')
API_BASE_CT      = 'https://api.cardtrader.com/api/v2'
CM_TIMEOUT       = 10      # segundos
RATE_LIMIT_DELAY = 1.1     # entre llamadas a la API

# ——— Telegram setup ———
trequest = HTTPXRequest(
    connection_pool_size=20,
    connect_timeout=20.0,
    read_timeout=60.0,
    write_timeout=60.0,
    pool_timeout=20.0
)
bot1 = Bot(token=TOKEN_BOT1, request=trequest)
bot2 = Bot(token=TOKEN_BOT2, request=trequest)

# ——— CardTrader session ———
session_ct = requests.Session()
session_ct.headers.update({'Authorization': f'Bearer {CARDTRADER_TOKEN}'})

# ——— País → bandera ———
COUNTRY_FLAGS = {
    "Alemania":        "🇩🇪", "Germany":           "🇩🇪",
    "Bélgica":         "🇧🇪", "Belgium":           "🇧🇪",
    "Bulgaria":        "🇧🇬",
    "Chipre":          "🇨🇾", "Cyprus":            "🇨🇾",
    "Croacia":         "🇭🇷", "Croatia":           "🇭🇷",
    "Dinamarca":       "🇩🇰", "Denmark":           "🇩🇰",
    "Eslovaquia":      "🇸🇰", "Slovakia":          "🇸🇰",
    "Eslovenia":       "🇸🇮", "Slovenia":          "🇸🇮",
    "España":          "🇪🇸", "Spain":             "🇪🇸",
    "Estonia":         "🇪🇪",
    "Finlandia":       "🇫🇮", "Finland":           "🇫🇮",
    "Francia":         "🇫🇷", "France":            "🇫🇷",
    "Grecia":          "🇬🇷", "Greece":            "🇬🇷",
    "Hungría":         "🇭🇺", "Hungary":           "🇭🇺",
    "Irlanda":         "🇮🇪", "Ireland":           "🇮🇪",
    "Islandia":        "🇮🇸", "Iceland":           "🇮🇸",
    "Italia":          "🇮🇹", "Italy":             "🇮🇹",
    "Japón":           "🇯🇵", "Japan":             "🇯🇵",
    "Letonia":         "🇱🇻", "Latvia":            "🇱🇻",
    "Liechtenstein":   "🇱🇮",
    "Lituania":        "🇱🇹", "Lithuania":         "🇱🇹",
    "Luxemburgo":      "🇱🇺", "Luxembourg":        "🇱🇺",
    "Malta":           "🇲🇹",
    "Noruega":         "🇳🇴", "Norway":            "🇳🇴",
    "Países Bajos":    "🇳🇱", "Netherlands":       "🇳🇱",
    "Polonia":         "🇵🇱", "Poland":            "🇵🇱",
    "Portugal":        "🇵🇹",
    "Reino Unido":     "🇬🇧", "United Kingdom":    "🇬🇧",
    "República Checa": "🇨🇿", "Czech Republic":    "🇨🇿",
    "Rumania":         "🇷🇴", "Romania":           "🇷🇴",
    "Singapur":        "🇸🇬", "Singapore":         "🇸🇬",
    "Suecia":          "🇸🇪", "Sweden":            "🇸🇪",
    "Suiza":           "🇨🇭", "Switzerland":       "🇨🇭",
    "Austria":         "🇦🇹"
}

# ——— Helpers de scraping y API ———
def parse_price(s: str) -> float | None:
    num = re.sub(r"[^\d,\.]", "", s).replace(",", ".")
    try:    return float(num)
    except: return None

def get_top_offers_selenium(url: str, top_n: int = TOP_N) -> list[dict]:
    opts = Options()
    opts.add_argument("--headless"); opts.add_argument("--disable-gpu")
    opts.add_argument("--no-sandbox"); opts.add_argument("user-agent=Mozilla/5.0")
    driver = webdriver.Chrome(options=opts)
    try:
        driver.get(url)
        soup = BeautifulSoup(driver.page_source, "html.parser")
    finally:
        driver.quit()

    raw = soup.find("title").text.split("|")[0].strip() if soup.find("title") else "—"
    name = re.sub(r" - MTG Singles$", "", raw).replace('"', "").replace("'", "")

    rows = soup.select("div.article-row:not(.ehcm-article-row-disabled)")
    if not rows:
        raise RuntimeError("No se encontraron ofertas activas.")

    offers = []
    for row in rows[:top_n]:
        pt = row.select_one("div.price-container span.color-primary")
        price_txt = pt.text.strip() if pt else "—"
        price     = parse_price(price_txt)
        seller_el = row.select_one("span.seller-name a")
        seller    = seller_el.text.strip() if seller_el else "—"
        loc_el    = row.select_one("span[aria-label*='Item location']")
        country   = loc_el['aria-label'].split(":",1)[1].strip() if loc_el else "—"
        sales_el  = row.select_one("span.sell-count")
        sales     = (sales_el['data-bs-original-title'].split()[0]
                     if sales_el and 'data-bs-original-title' in sales_el.attrs else "0")
        qty_el    = row.select_one("div.amount-container span.item-count")
        quantity  = qty_el.text.strip() if qty_el else "—"
        offers.append({
            "article":   name,
            "seller":    seller,
            "quantity":  quantity,
            "price_txt": price_txt,
            "price":     price,
            "country":   country,
            "sales":     sales,
            "url":       url,
        })
    return offers

def get_cardtrader_zero_lowest_price(blueprint_id: int,
                                     foil="true",
                                     language="en",
                                     condition="Near Mint") -> float | None:
    params = {"blueprint_id": blueprint_id,
              "foil": foil,
              "language": language}
    if condition:
        params["condition"] = condition
    debug(f"CT Zero req params: {params}")
    resp = session_ct.get(f"{API_BASE_CT}/marketplace/products",
                          params=params, timeout=CM_TIMEOUT)
    debug(f"CT HTTP {resp.status_code}")
    resp.raise_for_status()
    data = resp.json().get(str(blueprint_id), [])
    zero = [p for p in data if p.get('user',{}).get('can_sell_via_hub')]
    if not zero:
        return None
    cents = min(p['price']['cents'] for p in zero)
    price = cents / 100.0
    debug(f"CT Zero min = {price:.2f} €")
    return price

def ensure_history():
    if not os.path.exists(HISTORY_CSV):
        with open(HISTORY_CSV, 'w', newline='', encoding='utf-8') as f:
            csv.writer(f).writerow(
                ['article','timestamp','cm_price','ct_price','target']
            )

def record_history(article, ts, cm, ct, tgt):
    ensure_history()
    with open(HISTORY_CSV, 'a', newline='', encoding='utf-8') as f:
        csv.writer(f).writerow([
            article,
            ts,
            f"{cm:.2f}" if cm is not None else "",
            f"{ct:.2f}" if ct is not None else "",
            f"{tgt:.2f}"
        ])

def ascii_table(offers: list[dict]) -> str:
    cols = ["Seller", "Qty", "Price", "Country", "Sales"]
    rows = []
    for o in offers:
        flag = COUNTRY_FLAGS.get(o["country"], "")
        country = f"{flag} {o['country']}"
        rows.append([o["seller"], o["quantity"], o["price_txt"], country, o["sales"]])
    widths = [len(c) for c in cols]
    for r in rows:
        for i, cell in enumerate(r):
            widths[i] = max(widths[i], len(str(cell)))
    sep = "+" + "+".join("-"*(w+2) for w in widths) + "+"
    header = "| " + " | ".join(cols[i].ljust(widths[i]) for i in range(len(cols))) + " |"
    lines = [sep, header, sep]
    for r in rows:
        line = "| " + " | ".join(str(r[i]).ljust(widths[i]) for i in range(len(r))) + " |"
        lines.append(line)
    lines.append(sep)
    return "\n".join(lines)

async def send_telegram_message(bot: Bot, message: str, parse_mode: str = 'Markdown'):
    try:
        await bot.send_message(chat_id=CHAT_ID, text=message, parse_mode=parse_mode)
    except TimedOut:
        debug("Timeout enviando mensaje, reintentando...")
        await bot.send_message(chat_id=CHAT_ID, text=message, parse_mode=parse_mode)

async def send_telegram_photo(bot: Bot, path: str, caption: str = None, parse_mode: str = 'Markdown'):
    try:
        with open(path, 'rb') as f:
            await bot.send_photo(chat_id=CHAT_ID, photo=f, caption=caption, parse_mode=parse_mode)
    except TimedOut:
        debug(f"Timeout enviando foto {path}, reintentando...")
        with open(path, 'rb') as f:
            await bot.send_photo(chat_id=CHAT_ID, photo=f, caption=caption, parse_mode=parse_mode)

async def send_offers_table(bot: Bot, offers: list[dict], title: str,
                            cm_min: float | None, ct_min: float | None, target: float):
    table = ascii_table(offers)
    diff_str = ""
    if cm_min is not None and ct_min is not None:
        diff = ct_min - cm_min
        pct = (diff / cm_min * 100) if cm_min else 0
        sign = "+" if diff >= 0 else "-"
        diff_str = (
            f"\nCM Min: {cm_min:.2f} €\n"
            f"CT Zero: {ct_min:.2f} €\n"
            f"Diferencia: {sign}{abs(diff):.2f} € ({sign}{abs(pct):.1f}%)"
        )
    payload = f"{title}\n```{table}```{diff_str}"
    if SEND_TELEGRAM:
        await send_telegram_message(bot, payload)
    else:
        print(payload)



def plot_history() -> str:
    # Lee el histórico
    df = pd.read_csv(HISTORY_CSV, parse_dates=["timestamp"])
    
    # Figura más amplia
    fig, ax = plt.subplots(figsize=(16, 8))
    fig.patch.set_facecolor('#2b2b2b')
    ax.set_facecolor('#3c3c3c')
    ax.grid(True, linestyle='--', color='#555555')
    
    cmap = plt.get_cmap('tab10')
    jitter_scale = 0.20  # Desviación estándar del jitter (ajústalo a tu gusto)

    for i, (article, grp) in enumerate(df.groupby('article')):
        grp = grp.sort_values('timestamp')
        ts = grp['timestamp']
        color = cmap(i)

        # Un mismo jitter (scalar) para todos los puntos CM de este artículo
        jitter_cm = np.random.normal(loc=0, scale=jitter_scale)
        y_cm = grp['cm_price'] + jitter_cm
        ax.plot(
            ts, y_cm,
            linestyle='-', linewidth=1,
            marker='o', markersize=4,
            markerfacecolor=color, markeredgecolor='white',
            label=f"{article} CM",
            color=color
        )

        # Un mismo jitter distinto para todos los puntos CT de este artículo
        jitter_ct = np.random.normal(loc=0, scale=jitter_scale)
        y_ct = grp['ct_price'] + jitter_ct
        ax.plot(
            ts, y_ct,
            linestyle='--', linewidth=1,
            marker='s', markersize=4,
            markerfacecolor=color, markeredgecolor='white',
            label=f"{article} CT",
            color=color
        )

        # Línea de precio target sin jitter
        target = grp['target'].iloc[0]
        ax.axhline(
            y=target, linestyle=':', linewidth=1,
            color=color, alpha=0.8,
            label=f"{article} target"
        )

    # Etiquetas y estilo de ejes
    ax.set_xlabel('Tiempo', color='white')
    ax.set_ylabel('Precio (€)', color='white')
    ax.tick_params(axis='x', rotation=45, colors='lightgray')
    ax.tick_params(axis='y', colors='lightgray')

    # Leyenda fuera de la gráfica
    leg = ax.legend(
        bbox_to_anchor=(1.02, 1),
        loc='upper left',
        borderaxespad=0.,
        fontsize='small',
        frameon=False
    )
    for text in leg.get_texts():
        text.set_color('white')

    plt.tight_layout() # deja espacio a la derecha para la leyenda
    filename = 'price_trend.png'
    plt.savefig(filename, dpi=150, facecolor=fig.get_facecolor())
    plt.close(fig)
    return filename



async def check_offers():
    try:
        jobs = json.load(open(JOBS_CONFIG, encoding='utf-8'))
    except Exception as e:
        info(f"Error cargando jobs: {e}")
        return

    now = datetime.datetime.now().isoformat()
    summary = {}

    for job in jobs:
        url            = job.get('url_cardmarket')
        blueprint_id   = job.get('blueprint_id')
        target_price   = job.get('target_price')
        notify         = job.get('notify', True)
        alias          = job.get('alias') or None
        target_country = job.get('target_country')
        alert_only     = job.get('alert_only', False)

        try:
            offers = get_top_offers_selenium(url)
        except Exception as e:
            info(f"Error scraping {url}: {e}")
            continue

        name   = alias or offers[0]['article']
        prices = [o['price'] for o in offers if o['price'] is not None]
        cm_min = min(prices) if prices else None

        ct_min = None
        if blueprint_id:
            ct_min = get_cardtrader_zero_lowest_price(blueprint_id)
            time.sleep(RATE_LIMIT_DELAY)

        if notify and cm_min is not None and cm_min <= target_price:
            title = f"✅ [{name}]({url}) ha alcanzado {cm_min:.2f} € ≤ {target_price:.2f} €"
            await send_offers_table(bot1, offers, title, cm_min, ct_min, target_price)

        if not alert_only:
            record_history(name, now, cm_min, ct_min, target_price)
            summary[name] = (offers, cm_min, ct_min, target_price)

    for name, (offers, cm_min, ct_min, tgt) in summary.items():
        title  = f"📊 *{name}* (objetivo: {tgt:.2f} €)"
        await send_offers_table(bot2, offers, title, cm_min, ct_min, tgt)

    img = plot_history()
    caption = "📈 Tendencia de precios"
    if SEND_TELEGRAM:
        await send_telegram_photo(bot2, img, caption)
    else:
        print(f"[GRAPH] guardado en {img}")

def main():
    try:
        loop = asyncio.get_event_loop()
    except RuntimeError:
        loop = None

    if loop and loop.is_running():
        asyncio.ensure_future(check_offers())
    else:
        asyncio.run(check_offers())

if __name__ == "__main__":
    main()
