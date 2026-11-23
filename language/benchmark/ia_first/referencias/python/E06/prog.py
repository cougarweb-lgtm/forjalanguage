import sys

if len(sys.argv) < 2:
    print("erro: informe o arquivo")
    sys.exit(2)
try:
    with open(sys.argv[1], "rb") as Arquivo:
        Dados = Arquivo.read()
except FileNotFoundError:
    print(f"erro: arquivo nao encontrado: {sys.argv[1]}")
    sys.exit(2)
Linhas = Dados.count(b"\n")
if Dados and not Dados.endswith(b"\n"):
    Linhas += 1
print(Linhas)
