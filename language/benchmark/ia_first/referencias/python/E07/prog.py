import sys


def Valido(Texto: str) -> bool:
    Digitos = Texto[1:] if Texto.startswith("-") else Texto
    return Digitos.isascii() and Digitos.isdigit() and len(Digitos) <= 15


if len(sys.argv) < 2:
    print("erro: informe um numero")
    sys.exit(2)
if not Valido(sys.argv[1]):
    print(f"invalido: {sys.argv[1]}")
    sys.exit(1)
print(int(sys.argv[1]) * 2)
