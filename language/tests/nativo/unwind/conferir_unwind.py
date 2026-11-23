# -*- coding: utf-8 -*-
"""Confere o unwind do backend x64 da Forja com o unwinder do Windows (P8, M1).

Para cada fixture (01_ola, m1\\chamadas, m1\\quadro_grande, m2\\array\\array_grande) e cada modo (dev, release):
  1. build --backend x64 --debug-code asm,quadros,formato (gera <exe>.debug.txt);
  2. escolhe, em cada funcao do programa, dois pontos no corpo (um no meio, outro no fim, que cai
     nos blocos frios): instrucoes depois do prologo que nao sao de epilogo;
  3. escreve a lista de pontos <RVA> <RVA do retorno esperado> <rbp salvo esperado>;
  4. roda conferir_unwind.exe (oraculo em C, compilado aqui com gcc): RtlLookupFunctionEntry e
     RtlVirtualUnwind; Rip, Rsp e Rbp desenrolados tem de bater.
Roda tambem o --autoteste do oraculo (toda entrada de .pdata, runtime inclusive, achada pelo Windows).

O formato do .debug.txt lido aqui e o documentado em nativo\\x64\\x64_listar.c.

Uso:
    python -I conferir_unwind.py [--forja <exe>]

Versao: 1.9.0
Changelog: 1.9.0 - M7a F3: a lista leva o 4o campo (bytes do rbp ao rsp real: o quadro nos pontos do corpo); o oraculo aloca a pilha falsa desse tamanho (PUSH_NONVOL do alocador le de [rsp]).
           1.8.0 - M3 P21v-c: m3\\proc_valor\\ind_args (0 a 12 argumentos, struct por valor, area de saida na chamada por 'call rax'), ind_despacho e ind_recursao entram na lista: a chamada indireta nao muda prologo nem .xdata.
           1.7.0 - M2 P17c: ConferirPdataSemOrfas (le o PE: cada RUNTIME_FUNCTION do .pdata cai inteira em .text, aponta o unwind para dentro de .xdata e vem em ordem; o ligador agora descarta secao do runtime, entrada orfa seria falha).
           1.6.0 - M2 P17b: m2\\v19b\\soa_dinamica (forja_soa_crescer: chamada de 9 argumentos, 5 na pilha; colunas e gather/scatter) e mapa_struct_valor (?V struct nos blocos de achar, valores de 40 B copiados) entram na lista.
           1.5.0 - M2 P16: m2\\v19a\\bitset_tamanhos (blocos da guarda do SHL, forja_rt_conta_bits) e matrix_f64_4x4 (produtos desenrolados: muitos vregs vivos e spill) entram na lista.
           1.4.0 - M2 P15b: m2\\formato\\format_str (forja_fmt_str_final com retorno de str de 16 B por ponteiro oculto, lista de str) e fstring_real (f32 alargado a double + formato por posicao em xmm0/edx) entram na lista.
           1.3.0 - M2 P15a: m2\\opcional\\tupla_grande (retorno de 40 B por ponteiro oculto, tupla com struct e ?T) e union (union por valor em parametro, retorno, array e lista) entram na lista.
           1.2.0 - M2 P14b: m2\\memoria\\lista_append e lista_param entram na lista (chamadas de 8 argumentos a forja_lista_crescer: area de saida na pilha; blocos de crescimento).
           1.1.0 - M2 P13b: m2\\array\\array_grande entra na lista (quadro de ~1,2 MB: UWOP_ALLOC_LARGE info 1 com sondagem de paginas).
           1.0.0 - primeira versao (P8 do M1): driver Python do oraculo conferir_unwind.c.
Verificacao: python -I conferir_unwind.py --forja ..\\..\\..\\compiler\\forja.exe
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[2]
PastaExemplos = PastaLanguage.parent / "examples"
ForjaPadrao = PastaLanguage / "compiler" / "forja.exe"
Fixtures = [
    PastaExemplos / "01_ola.fj",
    PastaLanguage / "tests" / "nativo" / "m1" / "chamadas.fj",
    PastaLanguage / "tests" / "nativo" / "m1" / "quadro_grande.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "array" / "array_grande.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "memoria" / "lista_append.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "memoria" / "lista_param.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "opcional" / "tupla_grande.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "opcional" / "union.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "formato" / "format_str.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "formato" / "fstring_real.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "v19a" / "bitset_tamanhos.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "v19a" / "matrix_f64_4x4.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "v19b" / "soa_dinamica.fj",
    PastaLanguage / "tests" / "nativo" / "m2" / "v19b" / "mapa_struct_valor.fj",
    PastaLanguage / "tests" / "nativo" / "m3" / "proc_valor" / "ind_args.fj",
    PastaLanguage / "tests" / "nativo" / "m3" / "proc_valor" / "ind_despacho.fj",
    PastaLanguage / "tests" / "nativo" / "m3" / "proc_valor" / "ind_recursao.fj",
]
Modos = ("dev", "release")
ColunaBytes = 9
LarguraBytes = 44
ColunaAsm = ColunaBytes + LarguraBytes + 2
Mnemonicos = ("mov", "movzx", "movsxd", "add", "sub", "cmp", "imul", "test", "xor", "and", "or", "neg", "call", "set")
RvaRetornoRelativo = 0x10
RbpSalvo = 0x1122334455667788
RegexFuncaoAsm = re.compile(r"^funcao (\S+) texto\+0x([0-9a-f]+)$")
RegexFuncaoQuadro = re.compile(r"^funcao (\S+) texto\+0x([0-9a-f]+) tam=0x([0-9a-f]+) quadro=(\d+) .*fpreg=(\d+) alocacao=(\d+)")
RegexTexto = re.compile(r"^secao \.text\s+rva=0x([0-9a-f]+)")
RegexResumo = re.compile(r"pontos=(\d+) ok=(\d+) falhas=(\d+)")


def LerSecoes(Arquivo):
    Secoes = {}
    Atual = None
    for Linha in Path(Arquivo).read_text(encoding="utf-8").splitlines():
        if Linha.startswith("## "):
            Atual = Linha[3:]
            Secoes[Atual] = []
        elif Atual is not None:
            Secoes[Atual].append(Linha)
    return Secoes


def LerFuncoesAsm(Linhas):
    # nome -> (inicio na .text, [(offset na funcao, texto asm)])
    Funcoes = {}
    Atual = None
    for Linha in Linhas:
        Cabecalho = RegexFuncaoAsm.match(Linha)
        if Cabecalho:
            Atual = (int(Cabecalho.group(2), 16), [])
            Funcoes[Cabecalho.group(1)] = Atual
        elif Atual is not None and Linha.startswith("  +"):
            Texto = Linha[ColunaAsm:].split("  ;")[0].strip()
            Atual[1].append((int(Linha[3:7], 16), Texto))
    return Funcoes


def LerQuadros(Linhas):
    Quadros = {}
    for Linha in Linhas:
        Achado = RegexFuncaoQuadro.match(Linha)
        if Achado:
            Quadros[Achado.group(1)] = (int(Achado.group(5)), int(Achado.group(6)), int(Achado.group(4)))
    return Quadros


def LerRvaTexto(Linhas):
    for Linha in Linhas:
        Achado = RegexTexto.match(Linha)
        if Achado:
            return int(Achado.group(1), 16)
    raise SystemExit("X0001 .debug.txt sem a secao .text no item formato")


def EhDeCorpo(Texto):
    Palavra = Texto.split(" ")[0]
    return Palavra.startswith(Mnemonicos) and not Texto.startswith("lea rsp")


def EscolherPontos(Instrucoes, FimFpreg, FimPrologo, Quadro):
    """[(offset, bytes do rbp ao rsp real)]: pontos do corpo levam o quadro inteiro; os do prologo (sondagem e pushes do alocador), 0 = rsp 0x40 abaixo do rbp."""
    # Dois pontos no corpo: o do meio e o ultimo elegivel (que cai nos blocos frios). Mais todas as instrucoes da sondagem (depois do
    # mov rbp,rsp e antes do sub rsp,N): ali o unwinder ja usa o rbp mas ainda nao conta a alocacao.
    Elegiveis = [Offset for Offset, Texto in Instrucoes if Offset >= FimPrologo and EhDeCorpo(Texto)]
    Sondagem = [Offset for Offset, _ in Instrucoes if FimFpreg <= Offset < FimPrologo]
    Pontos = set(Sondagem)
    if Elegiveis:
        Pontos |= {Elegiveis[len(Elegiveis) // 2], Elegiveis[-1]}
    return [(Offset, Quadro if Offset >= FimPrologo else 0) for Offset in sorted(Pontos)]


def MontarLista(Secoes):
    RvaTexto = LerRvaTexto(Secoes["formato"])
    Quadros = LerQuadros(Secoes["quadros"])
    Linhas = []
    Funcoes = 0
    for Nome, (Inicio, Instrucoes) in LerFuncoesAsm(Secoes["asm"]).items():
        Pontos = EscolherPontos(Instrucoes, *Quadros[Nome])
        Funcoes += 1
        for Offset, TamQuadro in Pontos:
            Linhas.append(f"{RvaTexto + Inicio + Offset:x} {RvaTexto + RvaRetornoRelativo:x} {RbpSalvo:x} {TamQuadro:x}")
    return Funcoes, Linhas


def CompilarOraculo(Pasta):
    Saida = Pasta / "conferir_unwind.exe"
    Resultado = subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                                str(PastaAqui / "conferir_unwind.c"), "-o", str(Saida)],
                               capture_output=True, text=True)
    if Resultado.returncode != 0:
        raise SystemExit(f"X0001 gcc recusou conferir_unwind.c:\n{Resultado.stderr}")
    return Saida


def Construir(Forja, Fonte, Modo, Exe):
    Comando = [str(Forja), "build", "--backend", "x64", "--debug-code", "asm,quadros,formato", str(Fonte), "-o", str(Exe)]
    if Modo == "release":
        Comando.insert(2, "--release")
    return subprocess.run(Comando, capture_output=True, text=True)


def Conferir(Oraculo, Exe, Lista):
    Resultado = subprocess.run([str(Oraculo), str(Exe), str(Lista)], capture_output=True, text=True)
    Resumo = RegexResumo.search(Resultado.stdout)
    if not Resumo:
        return 0, 0, 1, Resultado.stdout + Resultado.stderr
    Pontos, Ok, Falhas = (int(Valor) for Valor in Resumo.groups())
    return Pontos, Ok, Falhas, Resultado.stdout


def ConferirFixture(Forja, Oraculo, Fonte, Modo, Pasta):
    Exe = Pasta / f"{Fonte.stem}_{Modo}.exe"
    Resultado = Construir(Forja, Fonte, Modo, Exe)
    if Resultado.returncode != 0:
        print(f"falha {Fonte.name} {Modo}: build x64 rc={Resultado.returncode} {Resultado.stdout}{Resultado.stderr}")
        return 0, 0, 1
    Funcoes, Linhas = MontarLista(LerSecoes(str(Exe) + ".debug.txt"))
    Lista = Pasta / f"{Fonte.stem}_{Modo}.lista.txt"
    Lista.write_text("\n".join(Linhas) + "\n", encoding="utf-8")
    Pontos, Ok, Falhas, Saida = Conferir(Oraculo, Exe, Lista)
    print(f"{Fonte.stem} {Modo}: funcoes={Funcoes} pontos={Pontos} ok={Ok} falhas={Falhas}")
    if Falhas:
        print(Saida)
    Auto = subprocess.run([str(Oraculo), "--autoteste", str(Exe)], capture_output=True, text=True)
    print(f"  {Auto.stdout.strip()}")
    Entradas, Orfas = ConferirPdataSemOrfas(Exe)
    print(f"  pdata: entradas={Entradas} fora_de_secao_mantida={Orfas}")
    return Pontos, Ok, Falhas + (1 if Auto.returncode != 0 else 0) + Orfas


def U32(Dados, Posicao):
    return int.from_bytes(Dados[Posicao:Posicao + 4], "little")


def LerSecoesPe(Dados):
    # [(nome, rva, tamanho virtual, posicao no arquivo)] da tabela de secoes do PE32+ e o diretorio de excecoes (indice 3) como (rva, tamanho).
    Lfanew = U32(Dados, 0x3C)
    Quantidade = int.from_bytes(Dados[Lfanew + 6:Lfanew + 8], "little")
    TamOpcional = int.from_bytes(Dados[Lfanew + 20:Lfanew + 22], "little")
    Opcional = Lfanew + 24
    Excecoes = Opcional + 112 + 3 * 8
    Secoes = []
    for K in range(Quantidade):
        Base = Opcional + TamOpcional + K * 40
        Nome = Dados[Base:Base + 8].rstrip(b"\0").decode("ascii", "replace")
        Secoes.append((Nome, U32(Dados, Base + 12), U32(Dados, Base + 8), U32(Dados, Base + 20)))
    return Secoes, (U32(Dados, Excecoes), U32(Dados, Excecoes + 4))


def SecaoDoRva(Secoes, Rva, Tamanho):
    for Nome, Inicio, Virtual, _ in Secoes:
        if Inicio <= Rva and Rva + Tamanho <= Inicio + Virtual:
            return Nome
    return None


def ConferirPdataSemOrfas(Exe):
    # P17c: o ligador descarta secao do runtime; cada RUNTIME_FUNCTION do .pdata tem de cair inteira em .text e apontar o unwind para dentro de .xdata
    # (entrada orfa = .pdata de codigo descartado), em ordem crescente e sem sobrepor. Devolve (entradas, problemas).
    Dados = Path(Exe).read_bytes()
    Secoes, (Rva, Tamanho) = LerSecoesPe(Dados)
    Pdata = next((S for S in Secoes if S[0] == ".pdata"), None)
    if Pdata is None or Rva != Pdata[1] or Tamanho == 0 or Tamanho % 12 != 0:
        return 0, 1
    Problemas = 0
    Anterior = 0
    for K in range(Tamanho // 12):
        Inicio, Fim, Unwind = (U32(Dados, Pdata[3] + K * 12 + 4 * J) for J in range(3))
        Problemas += Fim <= Inicio or SecaoDoRva(Secoes, Inicio, Fim - Inicio) != ".text"
        Problemas += SecaoDoRva(Secoes, Unwind, 4) != ".xdata"
        Problemas += Inicio < Anterior
        Anterior = Fim
    return Tamanho // 12, Problemas


def Principal():
    Parser = argparse.ArgumentParser(description="Confere o unwind do x64 com o RtlVirtualUnwind do Windows.")
    Parser.add_argument("--forja", default=str(ForjaPadrao), help="exe da Forja (padrao: compiler\\forja.exe)")
    Args = Parser.parse_args()

    Pasta = Path(tempfile.mkdtemp(prefix="conferir_unwind_"))
    try:
        Oraculo = CompilarOraculo(Pasta)
        Total = [0, 0, 0]
        for Fonte in Fixtures:
            for Modo in Modos:
                for Indice, Valor in enumerate(ConferirFixture(Args.forja, Oraculo, Fonte, Modo, Pasta)):
                    Total[Indice] += Valor
        print(f"conferir_unwind: pontos={Total[0]} ok={Total[1]} falhas={Total[2]}")
        return 0 if Total[2] == 0 and Total[0] > 0 else 1
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Principal())
