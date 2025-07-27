# -*- coding: utf-8 -*-
"""
Created on Wed Jun 30 13:30:08 2021

@author: Marck
"""


import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

EMAIL_ADDRESS = "lodan.asl@gmail.com"
EMAIL_PASSWORD = "onpqgsswggwghzvx"
SEND_ADDRESS1 = "jorge.romero.rs@gmail.com"
SEND_ADDRESS2 = "lodan.asl@gmail.com"


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
