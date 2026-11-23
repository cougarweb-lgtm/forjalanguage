# conferir_semente.py - confere (ou grava) o SHA256SUMS.txt da semente da Forja (F3 do G3, D-G3-5/D-G3-6).
# Versao 0.1.3 | 2026-11-19
# Changelog: 0.1.3 - LEIAME.txt (documento, nao artefato) fora da lista; 0.1.2 - inclui a copia de trabalho forja_fj\libfadec_envelope.a (tem de ser igual a da semente); 0.1.1 - CamelCase (PY-R01); 0.1.0 - cria; modo conferir (padrao, exit 0 = tudo igual, 1 = divergencia) e --gravar.
# Reversao: apagar SHA256SUMS.txt e rodar --gravar de novo com a semente certa (so ao trocar a semente de proposito).
# Cobre: forja_semente.exe, libfadec_envelope.a, fadec_envelope.o, rt\*.o e um resumo de uma linha da arvore ..\semente_c.
# Verificacao: python conferir_semente.py   (exit 0 = "0 divergencias")
import hashlib
import os
import sys

RAIZ = os.path.dirname(os.path.abspath(__file__))
SUMS = os.path.join(RAIZ, "SHA256SUMS.txt")
SEMENTE_C = os.path.join(os.path.dirname(RAIZ), "semente_c")
COPIA_FADEC = os.path.join(os.path.dirname(RAIZ), "forja_fj", "libfadec_envelope.a")
IGNORADOS = ("SHA256SUMS.txt", "conferir_semente.py", "LEIAME.txt")


def Sha256Arquivo(Caminho):
    Hash = hashlib.sha256()
    with open(Caminho, "rb") as Arq:
        for Bloco in iter(lambda: Arq.read(1 << 20), b""):
            Hash.update(Bloco)
    return Hash.hexdigest()


def ListarSemente():
    Achados = []
    for Pasta, _, Arquivos in os.walk(RAIZ):
        for Nome in Arquivos:
            if Nome in IGNORADOS or Nome.endswith(".pyc"):
                continue
            Achados.append(os.path.relpath(os.path.join(Pasta, Nome), RAIZ).replace("\\", "/"))
    return sorted(Achados)


def ResumirSementeC():
    Hash = hashlib.sha256()
    Qtd = 0
    for Pasta, _, Arquivos in sorted(os.walk(SEMENTE_C)):
        for Nome in sorted(Arquivos):
            Caminho = os.path.join(Pasta, Nome)
            Rel = os.path.relpath(Caminho, SEMENTE_C).replace("\\", "/")
            Hash.update(Rel.encode("utf-8") + b"\0" + Sha256Arquivo(Caminho).encode("ascii") + b"\n")
            Qtd += 1
    return Hash.hexdigest(), Qtd


def Calcular():
    Linhas = {Rel: Sha256Arquivo(os.path.join(RAIZ, Rel)) for Rel in ListarSemente()}
    Resumo, Qtd = ResumirSementeC()
    Linhas["semente_c/(%d arquivos)" % Qtd] = Resumo
    Linhas["../forja_fj/libfadec_envelope.a (copia de trabalho)"] = Sha256Arquivo(COPIA_FADEC)
    return Linhas


def Gravar():
    with open(SUMS, "w", encoding="utf-8", newline="\n") as Arq:
        for Rel, Valor in sorted(Calcular().items()):
            Arq.write("%s  %s\n" % (Valor, Rel))
    print("gravado", SUMS)
    return 0


def Conferir():
    if not os.path.isfile(SUMS):
        print("SEM SHA256SUMS.txt (rode --gravar)")
        return 1
    Esperado = {}
    with open(SUMS, encoding="utf-8") as Arq:
        for Linha in Arq:
            Valor, Rel = Linha.rstrip("\n").split("  ", 1)
            Esperado[Rel] = Valor
    Atual = Calcular()
    Ruins = 0
    for Rel in sorted(set(Esperado) | set(Atual)):
        if Esperado.get(Rel) != Atual.get(Rel):
            print("DIFERE:", Rel, "esperado", Esperado.get(Rel), "atual", Atual.get(Rel))
            Ruins += 1
    print("semente: %d itens, %d divergencias" % (len(Atual), Ruins))
    return 1 if Ruins else 0


if __name__ == "__main__":
    sys.exit(Gravar() if "--gravar" in sys.argv else Conferir())
