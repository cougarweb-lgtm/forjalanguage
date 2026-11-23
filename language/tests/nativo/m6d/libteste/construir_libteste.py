"""construir_libteste.py - refaz as bibliotecas estaticas de teste da contabilidade por lib (M6d, P92a).

Gera libteste_a.a e libteste_b.a em COFF (gcc MinGW UCRT, Windows) e em ELF (gcc do alpine-build, WSL), com `ar rcsD`
(deterministico: sem data no arquivo). Grava SHA256.txt com o hash de cada .a e, com --conferir, compara o que foi
refeito com o versionado, sem tocar nos arquivos versionados.

Versao: 0.1.1
Changelog:
  0.1.1 - M6d P92a: nomes Python em CamelCase (regra PY-R01); sem mudanca de comportamento.
  0.1.0 - M6d P92a: primeira versao. COFF por gcc no PATH; ELF por `wsl -d alpine-build` com timeout (teto de 180 s por lib).
Uso: python tests\\nativo\\m6d\\libteste\\construir_libteste.py [--conferir]
Depende de: stdlib do Python; gcc e ar no PATH (Windows) e a distro WSL alpine-build com gcc e ar.
"""

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

PastaLibteste = Path(__file__).resolve().parent
LibsDeTeste = (("libteste_a.a", "lt_a.c"), ("libteste_b.a", "lt_b.c"))
FlagsCompilar = ["-std=gnu11", "-O1", "-c"]
TetoSegundosWsl = 180


def CaminhoWsl(Caminho):
    """C:\\x\\y -> /mnt/c/x/y (o WSL monta a unidade em /mnt)."""
    Texto = str(Caminho).replace("\\", "/")
    return "/mnt/" + Texto[0].lower() + Texto[2:]


def ConstruirWindows(PastaSaida):
    """COFF: gcc MinGW no PATH, um .o temporario por fonte, empacotado em .a."""
    with tempfile.TemporaryDirectory() as Tmp:
        for NomeA, Fonte in LibsDeTeste:
            Objeto = Path(Tmp) / (Path(Fonte).stem + ".o")
            subprocess.run(["gcc", *FlagsCompilar, str(PastaLibteste / Fonte), "-o", str(Objeto)], check=True, timeout=TetoSegundosWsl)
            subprocess.run(["ar", "rcsD", str(PastaSaida / NomeA), str(Objeto)], check=True, timeout=TetoSegundosWsl)


def ConstruirLinux(PastaSaida):
    """ELF: a mesma fonte no alpine-build (musl). Um wsl por biblioteca, com teto de tempo; o tmp some no fim do comando."""
    for NomeA, Fonte in LibsDeTeste:
        Comando = (
            f"T=$(mktemp -d) && gcc {' '.join(FlagsCompilar)} '{CaminhoWsl(PastaLibteste / Fonte)}' -o \"$T/o.o\" "
            f"&& ar rcsD '{CaminhoWsl(PastaSaida / NomeA)}' \"$T/o.o\" && rm -rf \"$T\""
        )
        subprocess.run(["wsl", "-d", "alpine-build", "--", "sh", "-c", Comando], check=True, timeout=TetoSegundosWsl)


def Sha256Arquivo(Caminho):
    return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()


def ListarHashes(Pasta):
    """Hash dos .a de uma pasta-raiz (COFF na raiz, ELF em linux\\)."""
    Linhas = []
    for PastaRel in ("", "linux"):
        for NomeA, _ in LibsDeTeste:
            Caminho = Pasta / PastaRel / NomeA
            Rel = (PastaRel + "/" if PastaRel else "") + NomeA
            Linhas.append(f"{Sha256Arquivo(Caminho)}  {Rel}")
    return Linhas


def ConstruirTudo(PastaSaida):
    PastaSaida.mkdir(parents=True, exist_ok=True)
    (PastaSaida / "linux").mkdir(parents=True, exist_ok=True)
    ConstruirWindows(PastaSaida)
    ConstruirLinux(PastaSaida / "linux")


def Conferir():
    """Refaz numa pasta temporaria e compara o sha256 com o versionado. Devolve 0 se bateu tudo."""
    with tempfile.TemporaryDirectory() as Tmp:
        PastaNova = Path(Tmp)
        ConstruirTudo(PastaNova)
        Divergencias = [Item for Item in ListarHashes(PastaNova) if Item not in ListarHashes(PastaLibteste)]
    for Linha in Divergencias:
        print("DIVERGE " + Linha)
    print("conferir: " + ("OK, sha256 igual ao versionado" if not Divergencias else f"{len(Divergencias)} divergencia(s)"))
    return 0 if not Divergencias else 1


def Principal(Argumentos):
    if "--conferir" in Argumentos:
        return Conferir()
    ConstruirTudo(PastaLibteste)
    (PastaLibteste / "SHA256.txt").write_text("\n".join(ListarHashes(PastaLibteste)) + "\n", encoding="utf-8")
    print("\n".join(ListarHashes(PastaLibteste)))
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
