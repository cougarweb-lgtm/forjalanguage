import sys

CAPACIDADE = 4


class Fila:
    """Fila circular: vetor fixo de CAPACIDADE posicoes reaproveitado."""

    def __init__(self) -> None:
        self.Itens = [0] * CAPACIDADE
        self.Inicio = 0
        self.Tamanho = 0

    def Enfileirar(self, Valor: int) -> bool:
        if self.Tamanho == CAPACIDADE:
            return False
        self.Itens[(self.Inicio + self.Tamanho) % CAPACIDADE] = Valor
        self.Tamanho += 1
        return True

    def Desenfileirar(self):
        if self.Tamanho == 0:
            return None
        Valor = self.Itens[self.Inicio]
        self.Inicio = (self.Inicio + 1) % CAPACIDADE
        self.Tamanho -= 1
        return Valor


F = Fila()
for Arg in sys.argv[1:]:
    if Arg == "pop":
        V = F.Desenfileirar()
        print("pop vazia" if V is None else f"pop {V}")
    else:
        print(f"push {Arg} {'ok' if F.Enfileirar(int(Arg)) else 'cheia'}")
print(f"tamanho={F.Tamanho}")
