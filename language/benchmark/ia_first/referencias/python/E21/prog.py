import sys

Total = int(sys.argv[1])
Triangulo = []
for K in range(Total):
    Linha = [1] * (K + 1)
    for J in range(1, K):
        Linha[J] = Triangulo[K - 1][J - 1] + Triangulo[K - 1][J]
    Triangulo.append(Linha)
for Linha in Triangulo:
    print(" ".join(str(V) for V in Linha))
