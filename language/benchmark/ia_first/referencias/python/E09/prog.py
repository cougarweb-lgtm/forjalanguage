import math
import sys

with open(sys.argv[1], encoding="utf-8") as Arquivo:
    Valores = [int(Linha) for Linha in Arquivo if Linha.strip()]
Media = sum(Valores) / len(Valores)
Variancia = sum((V - Media) ** 2 for V in Valores) / len(Valores)
print(f"media={Media:.2f} desvio={math.sqrt(Variancia):.2f}")
