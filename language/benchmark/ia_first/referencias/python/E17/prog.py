import sys
from enum import Enum


class Estado(Enum):
    VERDE = 1
    AMARELO = 2
    VERMELHO = 3
    PISCANTE = 4


PROXIMO = {Estado.VERDE: Estado.AMARELO, Estado.AMARELO: Estado.VERMELHO,
           Estado.VERMELHO: Estado.VERDE, Estado.PISCANTE: Estado.PISCANTE}


def Transicao(Atual: Estado, Evento: str):
    """Novo estado, ou None se o evento e invalido."""
    if Evento == "tick":
        return PROXIMO[Atual]
    if Evento == "pane":
        return Estado.PISCANTE
    if Evento == "ok":
        return Estado.VERMELHO if Atual == Estado.PISCANTE else Atual
    return None


E = Estado.VERDE
print("inicio: VERDE")
for Evento in sys.argv[1:]:
    Novo = Transicao(E, Evento)
    if Novo is None:
        print(f"{Evento}: evento invalido")
    else:
        E = Novo
        print(f"{Evento}: {E.name}")
