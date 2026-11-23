import sys

Nome = sys.argv[1]
Linhas = ["n,quadrado,cubo"] + [f"{N},{N * N},{N * N * N}" for N in range(1, 6)]
with open(Nome, "w", encoding="utf-8", newline="\n") as Arquivo:
    Arquivo.write("\n".join(Linhas) + "\n")
print(f"gravado: {Nome} (5 linhas)")
