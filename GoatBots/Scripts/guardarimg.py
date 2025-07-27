# -*- coding: utf-8 -*-
"""
Created on Wed Jun 30 13:12:29 2021

@author: Marck
"""

def guardarimg(imagen,numero):



    imagen=imagen.strip('[')
    imagen=imagen.strip(']')
    imagen=imagen.replace('symbol','svg')
    
    imagen=imagen[:41]+r' height="2000" width="9000"'+imagen[41:]
    
    # imagen=imagen[:27]+"0 0 2314 500"+imagen[40:]
    
    
    
    with open(f'image_{numero}.svg', 'w') as f:
        f.write(imagen)