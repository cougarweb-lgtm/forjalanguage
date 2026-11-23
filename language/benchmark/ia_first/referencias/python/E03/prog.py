import sys

Contagem = {}
for Palavra in sys.argv[1:]:
    Contagem[Palavra] = Contagem.get(Palavra, 0) + 1
for Palavra, Quantas in Contagem.items():
    print(f"{Palavra}: {Quantas}")
