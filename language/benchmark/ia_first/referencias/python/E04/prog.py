import sys

with open(sys.argv[1], encoding="utf-8") as Arquivo:
    Valores = {int(Linha) for Linha in Arquivo if Linha.strip()}
for Valor in sorted(Valores):
    print(Valor)
