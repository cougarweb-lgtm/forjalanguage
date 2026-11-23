#!/usr/bin/env python3
"""verificar_dwarf.py - DWARF 4 do PE e do ELF: dumps sem aviso e tabela de linhas (M6 P64, D-M6-10).

Versao: 0.3.1
Changelog: 0.3.1 - main (validacao do P68): o controle do gcc volta a passar: LinhasDoDwarf (0.2.0) passou a devolver as linhas de TODOS os arquivos da CU e o gabarito.c puxa linhas de headers do sistema (extras 117.. no windows, 2.. no linux); o controle filtra por gabarito.c.
           0.3.0 - P68: --forja tambem no alvo windows (PE com --simbolos padrao; dumps por objdump do WinLibs; tabela x JSON igual ao do ELF).
           0.2.0 - P66: --forja <forja.exe> (alvo linux): cada fixture do gdb vira ELF com --simbolos padrao; dumps sem aviso e tabela DWARF == JSON do --debug-code linhas
                   (um arquivo por fixture, off relativo ao .text, comparacao pelo nome do arquivo).
           0.1.0 - P64: controle positivo com gcc -g -gdwarf-4 (MinGW -> PE, dumps por objdump; alpine -> ELF, dumps por readelf e objdump).
                   Oraculo fixo: gabarito\\linhas_gcc.txt (linhas obrigatorias e permitidas). Comparador da tabela com o JSON do
                   --debug-code linhas no formato do D-M6-2 ({"linhas": [{"off", "arquivo", "linha"}]}, off relativo ao .text);
                   o comparador e testado com a propria tabela do gcc (igual passa; uma linha trocada falha).
Uso: python verificar_dwarf.py                       (controle positivo do gcc, alvos windows e linux)
     python verificar_dwarf.py --alvo windows --exe <exe> --json <linhas.json>   (tabela do exe x JSON do --debug-code)
Depende de: objdump e readelf (WinLibs no Windows; alpine-build no Linux); gcc via verificar_gdb.py.
Saida: 0 PASSOU; 1 FALHOU; 2 ferramenta ausente; 3 controle do gcc FALHOU.
"""
import argparse
import json
import re
import sys
import tempfile
from pathlib import Path

from verificar_gdb import (FIXTURES, GABARITO, WSL_DISTRO, ChecarFerramentas, CompilarGabarito, Executar, FerramentaAusente,
                           ParaWsl)
from verificar_linhas import ProcsDoDump, RodarBuild

NOME_ARQUIVO_FONTE = "gabarito.c"
REGEX_LINHA_DECODIFICADA = re.compile(r"^(\S+)\s+(\d+)\s+0x([0-9a-fA-F]+)")
REGEX_VMA_TEXT_PE = re.compile(r"^\s*\d+\s+\.text\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)")
REGEX_VMA_TEXT_ELF = re.compile(r"\.text\s+\S+\s+([0-9a-fA-F]+)")
DUMPS_POR_ALVO = {
    "windows": [("objdump", ["--dwarf=info"]), ("objdump", ["--dwarf=line"]), ("objdump", ["--dwarf=frames"])],
    "linux": [("readelf", ["--debug-dump=info"]), ("readelf", ["--debug-dump=line"]), ("readelf", ["--debug-dump=frames"]),
              ("objdump", ["--dwarf=info"])],
}


def Comando(Alvo: str, Ferramenta: str, Argumentos: list, Exe: Path) -> list:
    if Alvo == "windows":
        return [Ferramenta, *Argumentos, Exe.as_posix()]
    return ["wsl", "-d", WSL_DISTRO, "--", Ferramenta, *Argumentos, ParaWsl(Exe)]


def SemAvisos(Saida: str) -> tuple:
    """Dump limpo: nenhuma linha com Warning ou Error (criterio do contrato P64 (e))."""
    for Linha in Saida.splitlines():
        if "Warning" in Linha or "Error" in Linha:
            return False, Linha.strip()[:160]
    return True, ""


def LerOraculo() -> tuple:
    """Le gabarito\\linhas_gcc.txt: (obrigatorias, permitidas) como conjuntos de numeros de linha."""
    Obrigatorias, Permitidas, Secao = set(), set(), None
    for Linha in (GABARITO / "linhas_gcc.txt").read_text(encoding="utf-8").splitlines():
        if Linha.startswith("OBRIGATORIAS"):
            Secao = Obrigatorias
        elif Linha.startswith("PERMITIDAS"):
            Secao = Permitidas
        elif Linha.strip().isdigit() and Secao is not None:
            Secao.add(int(Linha))
    return Obrigatorias, Permitidas


def VmaDoText(Alvo: str, Exe: Path) -> int:
    """Endereco do .text no arquivo (para o off relativo do D-M6-2)."""
    if Alvo == "windows":
        Saida, Regex = Executar(Comando(Alvo, "objdump", ["-h"], Exe)), REGEX_VMA_TEXT_PE
    else:
        Saida, Regex = Executar(Comando(Alvo, "readelf", ["-SW"], Exe)), REGEX_VMA_TEXT_ELF
    for Linha in Saida.splitlines():
        Casou = Regex.search(Linha)
        if Casou:
            return int(Casou.group(1), 16)
    raise RuntimeError(f"secao .text nao achada em {Exe.name}")


def ColapsarLinhas(Linhas: list) -> list:
    """D-M6-2 grava o par so quando a linha muda: colapsa repeticoes consecutivas da mesma linha."""
    Resultado, Anterior = [], None
    for Linha in sorted(Linhas, key=lambda Registro: Registro["off"]):
        if Anterior is None or Linha["linha"] != Anterior:
            Resultado.append(Linha)
            Anterior = Linha["linha"]
    return Resultado


def LinhasDoDwarf(Alvo: str, Exe: Path) -> list:
    """Tabela de linhas decodificada de todos os arquivos da CU no formato D-M6-2 (off relativo ao .text; arquivo = nome sem pasta)."""
    Ferramenta = "objdump" if Alvo == "windows" else "readelf"
    Argumentos = ["--dwarf=decodedline"] if Alvo == "windows" else ["--debug-dump=decodedline"]
    Saida = Executar(Comando(Alvo, Ferramenta, Argumentos, Exe))
    Base = VmaDoText(Alvo, Exe)
    Linhas = []
    for Linha in Saida.splitlines():
        Casou = REGEX_LINHA_DECODIFICADA.match(Linha)
        if Casou:
            Linhas.append({"off": int(Casou.group(3), 16) - Base, "arquivo": Path(Casou.group(1)).name, "linha": int(Casou.group(2))})
    return ColapsarLinhas(Linhas)


def LinhasDoJsonForja(Registros: list) -> list:
    """JSON do --debug-code linhas (um registro por proc, de todos os modulos) no formato da tabela DWARF: off relativo ao .text do programa."""
    Linhas = [{"off": Linha["off"], "arquivo": Path(Linha["arquivo"]).name, "linha": Linha["linha"]}
              for Dump in ProcsDoDump(Registros).values() for Linha in Dump["linhas"]]
    return ColapsarLinhas(Linhas)


def VerificarForja(Forja: Path, Pasta: Path, Alvo: str) -> list:
    """P66/P68 (--forja): cada fixture do gdb vira exe/ELF com --simbolos padrao; dumps sem aviso e tabela DWARF igual ao JSON do --debug-code linhas."""
    Resultados = []
    for Fonte in sorted(FIXTURES.glob("*.fj")):
        try:
            Registros = RodarBuild(Forja, Fonte, Alvo, Pasta)
        except RuntimeError as Erro:
            Resultados.append(("FALHOU", Fonte.stem, "build", str(Erro)))
            continue
        Exe = Pasta / f"{Fonte.stem}_{Alvo}.exe"
        Passou, Detalhe = VerificarDumps(Alvo, Exe)
        Resultados.append(("PASSOU" if Passou else "FALHOU", Fonte.stem, "dumps sem aviso", Detalhe))
        Atual = LinhasDoDwarf(Alvo, Exe)
        Passou, Detalhe = CompararTabelas(Atual, LinhasDoJsonForja(Registros))
        Resultados.append(("PASSOU" if Passou else "FALHOU", Fonte.stem, "tabela DWARF x JSON", Detalhe or f"{len(Atual)} registros"))
    return Resultados


def CarregarJson(Caminho: Path) -> list:
    """JSON do --debug-code linhas (D-M6-2): {"linhas": [{"off", "arquivo", "linha"}]}."""
    Dados = json.loads(Caminho.read_text(encoding="utf-8"))
    return ColapsarLinhas(Dados["linhas"])


def CompararTabelas(Atual: list, Referencia: list) -> tuple:
    """Igualdade por posicao de (off, arquivo, linha) depois de colapsar. Devolve (ok, detalhe)."""
    if len(Atual) != len(Referencia):
        return False, f"tamanhos diferentes: tabela={len(Atual)} referencia={len(Referencia)}"
    for Indice, (Feito, Esperado) in enumerate(zip(Atual, Referencia)):
        if Feito != Esperado:
            return False, f"diferenca no registro {Indice}: tabela={Feito} referencia={Esperado}"
    return True, ""


def ValidarOraculo(Linhas: list, Obrigatorias: set, Permitidas: set) -> tuple:
    """Toda linha obrigatoria aparece e nenhuma linha fora de obrigatorias+permitidas aparece."""
    Presentes = {Linha["linha"] for Linha in Linhas}
    Faltam = sorted(Obrigatorias - Presentes)
    Extras = sorted(Presentes - (Obrigatorias | Permitidas))
    if Faltam or Extras:
        return False, f"faltam={Faltam} extras={Extras}"
    return True, ""


def VerificarDumps(Alvo: str, Exe: Path) -> tuple:
    for Ferramenta, Argumentos in DUMPS_POR_ALVO[Alvo]:
        Limpo, Trecho = SemAvisos(Executar(Comando(Alvo, Ferramenta, Argumentos, Exe)))
        if not Limpo:
            return False, f"{Ferramenta} {' '.join(Argumentos)}: {Trecho}"
    return True, f"{len(DUMPS_POR_ALVO[Alvo])} dumps sem Warning/Error"


def VerificarAutoTesteComparador(Alvo: str, Linhas: list, Tmp: Path) -> tuple:
    """Comparador x JSON: a tabela gravada em JSON tem de casar consigo mesma, e uma linha trocada tem de falhar."""
    Caminho = Tmp / f"linhas_{Alvo}.json"
    Caminho.write_text(json.dumps({"linhas": Linhas}), encoding="utf-8")
    Igual, _ = CompararTabelas(Linhas, CarregarJson(Caminho))
    Trocada = [dict(Linhas[0], linha=Linhas[0]["linha"] + 1)] + Linhas[1:]
    Diferente, _ = CompararTabelas(Trocada, CarregarJson(Caminho))
    if Igual and not Diferente:
        return True, "JSON igual passa e linha trocada falha"
    return False, f"igual={Igual} trocada_falhou={not Diferente}"


def VerificarAlvo(Alvo: str, Tmp: Path, Obrigatorias: set, Permitidas: set) -> list:
    """Lista de (estado, alvo, verificacao, detalhe) do controle positivo de um alvo."""
    try:
        Exe = CompilarGabarito(Alvo, Tmp)
    except RuntimeError as Erro:
        return [("FALHOU", Alvo, "build do gabarito", str(Erro))]
    Resultados = []
    Passou, Detalhe = VerificarDumps(Alvo, Exe)
    Resultados.append(("PASSOU" if Passou else "FALHOU", Alvo, "dumps sem aviso", Detalhe))
    Linhas = [Linha for Linha in LinhasDoDwarf(Alvo, Exe) if Linha["arquivo"] == "gabarito.c"]  # headers do sistema entram na CU do gcc: so o fonte do gabarito vale
    Passou, Detalhe = ValidarOraculo(Linhas, Obrigatorias, Permitidas)
    Resultados.append(("PASSOU" if Passou else "FALHOU", Alvo, "linhas x oraculo gcc", Detalhe or f"{len(Linhas)} registros"))
    Passou, Detalhe = VerificarAutoTesteComparador(Alvo, Linhas, Tmp)
    Resultados.append(("PASSOU" if Passou else "FALHOU", Alvo, "comparador x JSON", Detalhe))
    return Resultados


def ImprimirResultados(Resultados: list) -> int:
    for Estado, Alvo, Verificacao, Detalhe in Resultados:
        print(f"{Estado} | {Alvo} | {Verificacao} | {Detalhe}")
    return 0 if all(Estado == "PASSOU" for Estado, *_ in Resultados) else 1


def RodarControle(Alvos: list) -> int:
    Obrigatorias, Permitidas = LerOraculo()
    DirTemp = Path(tempfile.mkdtemp(prefix="p64_dwarf_"))
    Resultados = []
    for Alvo in Alvos:
        Resultados += VerificarAlvo(Alvo, DirTemp, Obrigatorias, Permitidas)
    Codigo = ImprimirResultados(Resultados)
    return 3 if any(Estado == "FALHOU" and Verificacao == "build do gabarito" for Estado, _, Verificacao, _ in Resultados) else Codigo


def RodarComparacao(Alvo: str, Exe: Path, Json: Path) -> int:
    Atual = LinhasDoDwarf(Alvo, Exe)
    Passou, Detalhe = CompararTabelas(Atual, CarregarJson(Json))
    return ImprimirResultados([("PASSOU" if Passou else "FALHOU", Alvo, "tabela x JSON", Detalhe or f"{len(Atual)} registros")])


def MontarArgumentos() -> argparse.Namespace:
    Parser = argparse.ArgumentParser(description="DWARF 4 do PE e do ELF (M6 P64)")
    Parser.add_argument("--alvo", choices=["windows", "linux", "ambos"], default="ambos")
    Parser.add_argument("--exe", type=Path, default=None, help="exe com DWARF (modo comparacao com --json)")
    Parser.add_argument("--json", type=Path, default=None, help="JSON do --debug-code linhas (D-M6-2)")
    Parser.add_argument("--forja", type=Path, default=None, help="forja.exe: fixtures do gdb com DWARF (P66 ELF; P68 PE: --alvo windows ou linux)")
    return Parser.parse_args()


def RodarForja(Argumentos: argparse.Namespace) -> int:
    if Argumentos.alvo == "ambos":
        print("--forja pede um alvo: --alvo windows ou --alvo linux", file=sys.stderr)
        return 2
    ChecarFerramentas([Argumentos.alvo])
    return ImprimirResultados(VerificarForja(Argumentos.forja.resolve(), Path(tempfile.mkdtemp(prefix="p66_dwarf_")), Argumentos.alvo))


def Principal() -> int:
    Argumentos = MontarArgumentos()
    if Argumentos.forja is not None:
        try:
            return RodarForja(Argumentos)
        except FerramentaAusente as Erro:
            print(f"FERRAMENTA AUSENTE: {Erro}", file=sys.stderr)
            return 2
    Alvos = ["windows", "linux"] if Argumentos.alvo == "ambos" else [Argumentos.alvo]
    try:
        ChecarFerramentas(Alvos)
        if Argumentos.json is not None:
            return RodarComparacao(Argumentos.alvo, Argumentos.exe, Argumentos.json)
        return RodarControle(Alvos)
    except FerramentaAusente as Erro:
        print(f"FERRAMENTA AUSENTE: {Erro}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(Principal())
