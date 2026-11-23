# -*- coding: utf-8 -*-
"""Congela (ou confere, sem gcc) as DLLs da Forja: lista ordenada de exports e texto do .h, por fixture e modo.

Fixtures e de onde vem cada uma:
  tests\\cli\\cli_casos_ffi.py FIXTURES_DLL (linhas 451-452): biblioteca.fj, biblioteca_tipos.fj, biblioteca_numericos.fj,
      biblioteca_ponteiros.fj, biblioteca_listas.fj, biblioteca_arrays.fj, biblioteca_texto.fj, biblioteca_soma.fj, dll_com_foreign.fj
  tests\\nativo\\m4\\poda\\verificar_poda.py (linha 144): export_dll.fj (dev e release)
  tests\\runtime\\threads_t0\\verificar_threads_t0.py (linha 118): dll_threads.fj (build --release; congelado tambem em dev)
  Ficaram de fora, de proposito: m3\\dll\\realoca.fj e preserva.fj (sintaxe que a Forja nao aceita; nao ha verificador que os build em --modo dll).

Modos: dev (sem --release) e release (--release). Build: forja build --modo dll --backend c|x64 [--release] <fonte> -o <dll>.
Lado C: gaveta\\g1\\forja_g1.exe (so em --congelar). Lado x64: language\\compiler\\forja.exe (sempre).
Leitor do PE: tests\\nativo\\perfil\\exportacoes_pe.LerExportacoes (RVA crescente), o mesmo nos dois lados. .h: texto exato ao lado da DLL.
Banco: tests\\nativo\\rapido\\oraculo_congelado_g1_dll.json (classe "dll"); chave = ChaveDe(Fonte, "padrao", Args=(Modo, "exports+h"), Modo=Modo, Classe="dll").
--congelar grava so onde C == x64; C recusa = pulado (motivo); diferenca ou x64 recusado = nada gravado.
--contra-congelado: NAO usa forja_g1 nem gcc; so o x64 atual contra o JSON; chave ausente = diferente (FALHA).
Com banco selado (oraculo_congelado.py --selar) o cabecalho nao olha o gcc: e assim que a conferencia roda sem gcc no PATH.

Uso:
    python -I congelar_dll.py --congelar [--banco <json>]
    python -I congelar_dll.py --contra-congelado [--banco <json>]
    python -I congelar_dll.py --selftest
Verificacao: python -I congelar_dll.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 - 0.21.0 F1 P3 (classe dll): primeira versao (--congelar, --contra-congelado, --selftest; --banco para o controle negativo).
"""

import argparse
import struct
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaNativo = PastaAqui.parent
PastaLanguage = PastaAqui.parents[2]
sys.path.insert(0, str(PastaAqui))  # oraculo_congelado (0.21.0 F1)
sys.path.insert(0, str(PastaNativo / "perfil"))  # exportacoes_pe: leitor do export directory do PE64 (reaproveitado)
import oraculo_congelado as Oraculo  # noqa: E402
import exportacoes_pe as Exportacoes  # noqa: E402

CAMINHO_JSON_DLL = PastaAqui / "oraculo_congelado_g1_dll.json"
FORJA_X64 = PastaLanguage / "compiler" / "forja.exe"
PastaCli = PastaLanguage / "tests" / "cli"
FIXTURES_DLL = [PastaCli / Nome for Nome in (
    "biblioteca.fj", "biblioteca_tipos.fj", "biblioteca_numericos.fj", "biblioteca_ponteiros.fj", "biblioteca_listas.fj",
    "biblioteca_arrays.fj", "biblioteca_texto.fj", "biblioteca_soma.fj", "dll_com_foreign.fj")]
FIXTURES_DLL += [PastaNativo / "m4" / "poda" / "export_dll.fj", PastaLanguage / "tests" / "runtime" / "threads_t0" / "dll_threads.fj"]
MODOS_DLL = {"dev": [], "release": ["--release"]}
ClasseDll = "dll"
Checagem = "padrao"
TimeoutBuild = 120


def ItensDoCongelamento():
    """(fonte, modo) de cada fixture de DLL, em dev e release, na ordem da lista."""
    return [(Fonte, Modo) for Fonte in FIXTURES_DLL for Modo in MODOS_DLL]


def ChaveDoItem(Fonte, Modo):
    return Oraculo.ChaveDe(Fonte, Checagem, Args=(Modo, "exports+h"), Modo=Modo, Classe=ClasseDll)


def RegistroPulado(Motivo):
    return {"invalido": False, "origem": Oraculo.FORJA_G1.name, "pulado": True, "motivo": Motivo, "exit": 0, "stdout": "", "stderr": []}


def RegistroDll(Impressao):
    return {"invalido": False, "origem": Oraculo.FORJA_G1.name, "exit": 0, "stdout": "", "stderr": [],
            "exports": Impressao["exports"], "h": Impressao["h"]}


def BuildarDll(Forja, Backend, Modo, Fonte, Temp):
    """(DLL, .h, motivo): motivo vazio = o build passou; senao o exit e o comeco do texto do erro."""
    Dll = Temp / f"{Fonte.stem}_{Backend}_{Modo}.dll"
    Comando = [str(Forja), "build", "--modo", "dll", "--backend", Backend, *MODOS_DLL[Modo], str(Fonte), "-o", str(Dll)]
    Rodada = subprocess.run(Comando, capture_output=True, timeout=TimeoutBuild)
    if Rodada.returncode != 0 or not Dll.is_file():
        Texto = (Rodada.stderr + Rodada.stdout).decode("utf-8", errors="replace").strip()
        return None, None, f"exit {Rodada.returncode}: {Texto[:200]}"
    return Dll, Dll.with_suffix(".h"), ""


def LerImpressao(Dll, H):
    """(impressao, erro): exports na ordem do RVA (leitor do PE) e o texto exato do .h; erro = o que nao se leu."""
    try:
        Nomes = Exportacoes.LerExportacoes(Dll)[1]
        return {"exports": Nomes, "h": H.read_bytes().decode("utf-8")}, ""
    except (SystemExit, OSError, ValueError, struct.error) as Erro:
        return None, f"nao leu a DLL ou o .h: {Erro}"


def ImpressaoDoX64(Modo, Fonte, Temp):
    """(impressao, erro) do forja atual com backend x64; recusa do x64 conta como erro."""
    Dll, H, Recusa = BuildarDll(FORJA_X64, "x64", Modo, Fonte, Temp)
    if Recusa:
        return None, f"x64 recusou ({Recusa})"
    return LerImpressao(Dll, H)


def DiferencaDeLados(Esperado, ErroEsperado, Obtido, ErroObtido):
    """Texto do que difere ('' = iguais). Erro de leitura ou recusa de um dos lados conta como diferenca."""
    if ErroEsperado or ErroObtido:
        return "; ".join(Erro for Erro in (ErroEsperado, ErroObtido) if Erro)
    Diferencas = []
    if Esperado["exports"] != Obtido["exports"]:
        Diferencas.append("lista de exports")
    if Esperado["h"] != Obtido["h"]:
        Diferencas.append("texto do .h")
    return " e ".join(Diferencas)


def CongelarItem(Banco, Fonte, Modo, Temp):
    """(situacao, texto): grava so onde o C produz o que o x64 tem de igualar; C recusa = pulado; diferenca = nada gravado."""
    Rotulo = f"{Fonte.name} {Modo}"
    Dll, H, Recusa = BuildarDll(Oraculo.FORJA_G1, "c", Modo, Fonte, Temp)
    if Recusa:
        Banco.Registrar(ChaveDoItem(Fonte, Modo), RegistroPulado(Recusa))
        return "pulado", f"{Rotulo}: C recusou ({Recusa})"
    ImpC, ErroC = LerImpressao(Dll, H)
    ImpX, ErroX = ImpressaoDoX64(Modo, Fonte, Temp)
    Diferenca = DiferencaDeLados(ImpC, ErroC, ImpX, ErroX)
    if Diferenca:
        return "divergente", f"{Rotulo}: {Diferenca}"
    Banco.Registrar(ChaveDoItem(Fonte, Modo), RegistroDll(ImpC))
    return "gravado", Rotulo


def CongelarTudo(Caminho):
    """--congelar: zera o JSON e grava o lado C onde ele iguala o x64, item a item (banco selado nao se recongela)."""
    if not Oraculo.FORJA_G1.is_file():
        raise Oraculo.ErroOraculo(f"forja de referencia ausente: {Oraculo.FORJA_G1}")
    Banco = Oraculo.BancoOraculo(Caminho, Oraculo.FORJA_G1)
    if Banco.Selado:
        raise Oraculo.ErroOraculo(f"{Caminho.name} esta selado: nao se recongela")
    Banco.Itens.clear()
    Banco.Cabecalho = Oraculo.CabecalhoAtual(Banco.Referencia)
    Contagem = Counter()
    with tempfile.TemporaryDirectory(prefix="congelar_dll_") as Temp:
        for Fonte, Modo in ItensDoCongelamento():
            Situacao, Texto = CongelarItem(Banco, Fonte, Modo, Path(Temp))
            Contagem[Situacao] += 1
            print(f"{Situacao}: {Texto}", flush=True)
    Banco.Salvar()
    print(f"congelar: {Contagem['gravado']} gravados, {Contagem['pulado']} pulados, "
          f"{Contagem['divergente']} divergencias C != x64 ({Caminho.name})")
    return 0


def ConferirItem(Banco, Fonte, Modo, Temp):
    """(situacao, texto) de um item contra o congelado: x64 atual, sem gcc; chave ausente = diferente (FALHA)."""
    Rotulo = f"{Fonte.name} {Modo}"
    Registro = Banco.Itens.get(ChaveDoItem(Fonte, Modo))
    if Registro is None:
        return "diferente", f"{Rotulo}: chave ausente no congelado (FALHA)"
    if Registro.get("pulado"):
        return "pulado", f"{Rotulo}: {Registro['motivo']}"
    ImpX, ErroX = ImpressaoDoX64(Modo, Fonte, Temp)
    Diferenca = DiferencaDeLados(Registro, "", ImpX, ErroX)
    if Diferenca:
        return "diferente", f"{Rotulo}: {Diferenca}"
    return "igual", Rotulo


def ConferirContraCongelado(Caminho):
    """--contra-congelado: sem gcc e sem forja_g1; so o x64 atual contra o JSON. Cabecalho nao selado olha o gcc: falha sem ele."""
    Banco = Oraculo.BancoOraculo(Caminho, Oraculo.FORJA_G1)
    Banco.ExigirCabecalho()
    Contagem = Counter()
    Inicio = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="contra_dll_") as Temp:
        for Fonte, Modo in ItensDoCongelamento():
            Situacao, Texto = ConferirItem(Banco, Fonte, Modo, Path(Temp))
            Contagem[Situacao] += 1
            print(f"{Situacao}: {Texto}", flush=True)
    Segundos = time.perf_counter() - Inicio
    print(f"contra-congelado: {Contagem['igual']} iguais, {Contagem['diferente']} diferentes, "
          f"{Contagem['pulado']} pulados, {Segundos:.1f} s ({Caminho.name})")
    return 0 if Contagem["diferente"] == 0 else 1


def ConferirSelftestLeitura():
    """Leitor de exports e comparacao, sem compilar nada."""
    Falhas = []
    Nomes = Exportacoes.LerExportacoes(Exportacoes.DllDoSistema("ntdll.dll"))[1]
    if "RtlAllocateHeap" not in Nomes:
        Falhas.append("leitor de exports: ntdll sem RtlAllocateHeap")
    Base = {"exports": ["a", "b"], "h": "x\n"}
    if DiferencaDeLados(Base, "", {"exports": ["a", "b"], "h": "x\n"}, ""):
        Falhas.append("iguais deram diferenca")
    if "lista de exports" not in DiferencaDeLados(Base, "", {"exports": ["b", "a"], "h": "x\n"}, ""):
        Falhas.append("ordem trocada nao deu diferenca de exports")
    if "texto do .h" not in DiferencaDeLados(Base, "", {"exports": ["a", "b"], "h": "x \n"}, ""):
        Falhas.append("espaco no .h nao deu diferenca")
    if not DiferencaDeLados(Base, "", None, "x64 recusou (teste)"):
        Falhas.append("recusa do x64 nao virou diferenca")
    return Falhas


def ConferirSelftestChaves(Temp):
    """Modo e classe entram na chave; pulado e chave ausente saem sem compilar."""
    Falhas = []
    Fonte = FIXTURES_DLL[0]
    Dev, Release = ChaveDoItem(Fonte, "dev"), ChaveDoItem(Fonte, "release")
    if Dev == Release or Dev == Oraculo.ChaveDe(Fonte, Checagem, Args=("dev", "exports+h"), Modo="dev"):
        Falhas.append("modo ou classe nao entraram na chave")
    Banco = Oraculo.BancoOraculo(Temp / "vazio.json", Oraculo.FORJA_G1)
    if ConferirItem(Banco, Fonte, "dev", Temp)[0] != "diferente":
        Falhas.append("chave ausente nao deu FALHA")
    Banco.Itens[Dev] = RegistroPulado("teste")
    if ConferirItem(Banco, Fonte, "dev", Temp)[0] != "pulado":
        Falhas.append("registro pulado nao saiu como pulado")
    return Falhas


def Selftest():
    with tempfile.TemporaryDirectory(prefix="congelar_dll_st_") as Temp:
        Falhas = ConferirSelftestLeitura() + ConferirSelftestChaves(Path(Temp))
    print("selftest: " + ("OK" if not Falhas else "; ".join(Falhas)))
    return 0 if not Falhas else 1


def Principal():
    Parser = argparse.ArgumentParser(description="Congela e confere as DLLs da Forja (exports + .h) sem gcc na conferencia.")
    Parser.add_argument("--congelar", action="store_true")
    Parser.add_argument("--contra-congelado", action="store_true")
    Parser.add_argument("--selftest", action="store_true")
    Parser.add_argument("--banco", type=Path, default=CAMINHO_JSON_DLL,
                        help="JSON do congelado (padrao: oraculo_congelado_g1_dll.json); o controle negativo aponta para uma copia")
    Args = Parser.parse_args()
    if Args.selftest:
        return Selftest()
    if Args.congelar and Args.contra_congelado:
        print("--congelar e --contra-congelado sao excludentes")
        return 2
    if Args.congelar:
        return CongelarTudo(Args.banco)
    if Args.contra_congelado:
        return ConferirContraCongelado(Args.banco)
    Parser.print_help()
    return 2


if __name__ == "__main__":
    try:
        sys.exit(Principal())
    except Oraculo.ErroOraculo as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        sys.exit(2)
