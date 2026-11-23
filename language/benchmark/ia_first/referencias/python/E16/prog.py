import sys

with open(sys.argv[1], encoding="utf-8") as Arquivo:
    Valores = [int(Linha) for Linha in Arquivo if Linha.strip()]
Alvo = int(sys.argv[2])
Baixo, Alto = 0, len(Valores) - 1
Achado = -1
while Baixo <= Alto:
    Meio = (Baixo + Alto) // 2
    if Valores[Meio] == Alvo:
        Achado = Meio
        break
    if Valores[Meio] < Alvo:
        Baixo = Meio + 1
    else:
        Alto = Meio - 1
print(Achado)
