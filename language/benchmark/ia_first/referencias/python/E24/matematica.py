def quadrado(n: int) -> int:  # lint-ok: nome-fora-do-padrao nome da funcao exigido pelo enunciado E24
    return n * n


def soma_quadrados(a: int, b: int) -> int:  # lint-ok: nome-fora-do-padrao nome da funcao exigido pelo enunciado E24
    return quadrado(a) + quadrado(b)
