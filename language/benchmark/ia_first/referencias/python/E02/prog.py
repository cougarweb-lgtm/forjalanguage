import sys

Soma = 0
with open(sys.argv[1], encoding="utf-8") as Arquivo:
    for Linha in Arquivo:
        if Linha.strip():
            Soma += int(Linha)
print(Soma)
