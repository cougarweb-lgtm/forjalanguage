"""verificar_estatica.py - oraculo do P23d (M3b): os membros que a selecao de estaticas puxa de libz.a e libcjson.a sao os que o ld puxa.

Para cada conjunto: as raizes (simbolos foreign que o programa chama) viram um programa C minimo que so as referencia; o gcc liga com -Wl,-Map e a secao "Archive member
included to satisfy reference by file (symbol)" do mapa diz quais membros do .a o ld puxou. O mesmo conjunto de raizes vai para `teste_nativo.exe estatica <lib> -- <raizes>`
(EstaticaSelecionar) e os dois conjuntos de membros tem de ser IGUAIS. Membro a mais ou a menos e falha, com o membro e o simbolo (o do mapa) no relato.

Conjuntos do contrato: compressao_basico (libz.a; raizes = os procs de zlib.fj que vendor\\compression\\compression.fj chama) e json_basico (libcjson.a; os de cjson.fj que
vendor\\json\\json.fj chama). Estendidos: compressao_todos e json_todos (TODOS os procs foreign do modulo: puxa os membros do gz*, mais bordas).

Uso:   python -I verificar_estatica.py [--teste-nativo <abs>\\teste_nativo.exe] [--vendor <abs>\\language\\vendor]
Saida: uma linha por conjunto e o resumo "conjuntos iguais ao mapa do ld: N/N (contrato 2/2)"; exit 0 so se todos iguais.
Versao: 0.2.1
Changelog: 0.2.1 - IA1-L3c2: vendor\\compressao\\ passou a vendor\\compression\\ (compression.fj).
           0.2.0 - M3b P23e: conjunto fx_coff (libfx_coff.a de m3\\estatica\\libs; as raizes sao os procs de fx_coff.fj): a selecao com COMDAT, fraco e comum tambem iguala o mapa do ld.
           0.1.0 - M3b P23d: primeira versao.
Depende de: gcc no PATH (so para gerar o mapa; a selecao em si e o teste_nativo.exe).
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

RaizDoScript = Path(__file__).resolve().parent
PastaLanguage = RaizDoScript.parent.parent
PastaFxCoff = RaizDoScript / "m3" / "estatica" / "libs"
ProcForeign = re.compile(r'^\s*(?:pub\s+)?proc\s+(\w+)\s*(?:=\s*"(\w+)")?\s*\(', re.MULTILINE)
LinhaMembro = re.compile(r"^(\S.*?)\(([^()\s][^()]*)\)\s*(?:\S.*)?$")
FimDaSecao = ("Discarded input sections", "Memory Configuration", "Allocating common symbols", "Linker script and memory map")


def Procs(Fonte: Path) -> dict:
    """nome do proc na Forja -> simbolo do C (o alias depois do '=' quando existe)."""
    Texto = Fonte.read_text(encoding="utf-8")
    return {Nome: (Alias or Nome) for Nome, Alias in ProcForeign.findall(Texto)}


def Usados(Wrapper: Path, Apelido: str, Foreign: dict) -> list:
    """Os simbolos C dos procs foreign que o modulo wrapper chama (apelido.nome), em ordem alfabetica."""
    Texto = Wrapper.read_text(encoding="utf-8")
    Nomes = {N for N in re.findall(rf"\b{Apelido}\.(\w+)", Texto) if N in Foreign}
    return sorted(Foreign[N] for N in Nomes)


def Conjuntos(Vendor: Path) -> list:
    """(nome, biblioteca, raizes) de cada conjunto."""
    Zlib = Procs(Vendor / "compression" / "zlib.fj")
    Cjson = Procs(Vendor / "json" / "cjson.fj")
    return [
        ("compressao_basico", Vendor / "libz.a", Usados(Vendor / "compression" / "compression.fj", "zl", Zlib)),
        ("json_basico", Vendor / "libcjson.a", Usados(Vendor / "json" / "json.fj", "cj", Cjson)),
        ("compressao_todos", Vendor / "libz.a", sorted(Zlib.values())),
        ("json_todos", Vendor / "libcjson.a", sorted(Cjson.values())),
        ("fx_coff", PastaFxCoff / "libfx_coff.a", sorted(V for N, V in Procs(PastaFxCoff.parent / "fx_coff.fj").items() if N != "main")),
    ]


def MembrosDoMapa(Mapa: str, Biblioteca: Path) -> dict:
    """membro -> simbolo que o ld disse ter pedido, so da biblioteca dada, lendo a secao 'Archive member included'."""
    Achados = {}
    Dentro = False
    Atual = None
    for Linha in Mapa.splitlines():
        if Linha.startswith("Archive member included"):
            Dentro = True
            continue
        if Dentro and Linha.startswith(FimDaSecao):
            break
        if not Dentro or not Linha.strip():
            continue
        if Linha[0] != " ":
            Casou = LinhaMembro.match(Linha)
            Atual = None
            if Casou and Path(Casou.group(1).replace("\\", "/")).name == Biblioteca.name:
                Atual = Casou.group(2)
                Achados[Atual] = ""
        if Atual is not None and "(" in Linha and Linha.rstrip().endswith(")") and Achados[Atual] == "":
            Achados[Atual] = Linha.rstrip().rsplit("(", 1)[1][:-1]
    return Achados


def MapaDoLd(Pasta: Path, Nome: str, Biblioteca: Path, Raizes: list) -> str:
    """Liga um programa C que so referencia as raizes e devolve o mapa do ld."""
    Declaracoes = "".join(f"extern char {R}[];\n" for R in Raizes)
    Tabela = ", ".join(Raizes)
    Fonte = Pasta / f"{Nome}.c"
    Fonte.write_text(f"{Declaracoes}void *const referencias[] = {{{Tabela}}};\nint main(void) {{ return referencias[0] == 0; }}\n", encoding="utf-8")
    MapaArq = Pasta / f"{Nome}.map"
    Saida = subprocess.run(["gcc", "-O0", str(Fonte), str(Biblioteca), f"-Wl,-Map,{MapaArq}", "-o", str(Pasta / f"{Nome}.exe")], capture_output=True, text=True)
    if Saida.returncode != 0:
        raise RuntimeError(f"gcc falhou em {Nome}: {Saida.stderr[:600]}")
    return MapaArq.read_text(encoding="utf-8", errors="replace")


def MembrosNossos(TesteNativo: Path, Biblioteca: Path, Raizes: list) -> tuple:
    """(membros, pendentes) que EstaticaSelecionar devolve, na ordem em que o teste_nativo.exe os imprime."""
    Saida = subprocess.run([str(TesteNativo), "estatica", str(Biblioteca), "--", *Raizes], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if Saida.returncode != 0:
        raise RuntimeError(f"teste_nativo estatica saiu com {Saida.returncode}: {Saida.stderr[:600]}")
    Membros = [L[len("membro "):].rsplit("(", 1)[1][:-1] for L in Saida.stdout.splitlines() if L.startswith("membro ")]
    Pendentes = [L[len("pendente "):] for L in Saida.stdout.splitlines() if L.startswith("pendente ")]
    return Membros, Pendentes


def Comparar(Nome: str, Ld: dict, Nossos: list) -> bool:
    """Imprime a linha do conjunto; falso se ha membro a mais ou a menos (com o simbolo do mapa para o que falta)."""
    AMenos = sorted(set(Ld) - set(Nossos))
    AMais = sorted(set(Nossos) - set(Ld))
    Repetidos = len(Nossos) - len(set(Nossos))
    if not AMenos and not AMais and not Repetidos:
        print(f"ok {Nome}: {len(Nossos)} membros, iguais ao mapa do ld")
        return True
    print(f"FALHOU {Nome}: ld {len(Ld)} membros, nosso {len(Nossos)}; repetidos {Repetidos}")
    for M in AMenos:
        print(f"  membro a MENOS: {M} (o ld puxou para o simbolo '{Ld[M]}')")
    for M in AMais:
        print(f"  membro a MAIS: {M} (o ld nao puxou)")
    return False


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--teste-nativo", default=str(PastaLanguage / "compiler" / "teste_nativo.exe"))
    Analisador.add_argument("--vendor", default=str(PastaLanguage / "vendor"))
    Args = Analisador.parse_args()
    TesteNativo, Vendor = Path(Args.teste_nativo).resolve(), Path(Args.vendor).resolve()
    Iguais = 0
    Todos = Conjuntos(Vendor)
    with tempfile.TemporaryDirectory(prefix="forja_estatica_") as Pasta:
        for Nome, Biblioteca, Raizes in Todos:
            Ld = MembrosDoMapa(MapaDoLd(Path(Pasta), Nome, Biblioteca, Raizes), Biblioteca)
            Nossos, Pendentes = MembrosNossos(TesteNativo, Biblioteca, Raizes)
            Iguais += Comparar(Nome, Ld, Nossos)
            print(f"   raizes {len(Raizes)}, pendentes (sem dono na estatica) {len(Pendentes)}: {' '.join(Pendentes[:6])}{' ...' if len(Pendentes) > 6 else ''}")
    print(f"conjuntos iguais ao mapa do ld: {Iguais}/{len(Todos)} (contrato: compressao_basico, json_basico e fx_coff)")
    return 0 if Iguais == len(Todos) else 1


if __name__ == "__main__":
    sys.exit(Principal())
