import math
import sys
from dataclasses import dataclass


@dataclass
class Circulo:
    raio: int  # lint-ok: nome-fora-do-padrao campo do modelo de dados


@dataclass
class Retangulo:
    largura: int  # lint-ok: nome-fora-do-padrao campo do modelo de dados
    altura: int  # lint-ok: nome-fora-do-padrao campo do modelo de dados


@dataclass
class Quadrado:
    lado: int  # lint-ok: nome-fora-do-padrao campo do modelo de dados


def Area(Forma) -> float:
    if isinstance(Forma, Circulo):
        return math.pi * Forma.raio * Forma.raio
    if isinstance(Forma, Retangulo):
        return float(Forma.largura * Forma.altura)
    return float(Forma.lado * Forma.lado)


Args = sys.argv[1:]
I = 0
while I < len(Args):
    Nome = Args[I]
    if Nome == "circulo":
        Forma, I = Circulo(int(Args[I + 1])), I + 2
    elif Nome == "retangulo":
        Forma, I = Retangulo(int(Args[I + 1]), int(Args[I + 2])), I + 3
    elif Nome == "quadrado":
        Forma, I = Quadrado(int(Args[I + 1])), I + 2
    else:
        print(f"forma desconhecida: {Nome}")
        sys.exit(1)
    print(f"{Nome}: {Area(Forma):.2f}")
