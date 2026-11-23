"""Casos de cli do build: erros de build, #line e FORJA_ARQ no C gerado, build --modo c (foreign, array, memoria, v1.7, modulos), DLL sem export e --modo invalido.

Importado pelo motor verificar_cli.py (que monta a lista CASOS na ordem original); as funcoes comuns vem de la.

Versao: 1.5.0
Changelog: 1.5.0 - 0.21.0 F4 passo 3: saem CasoForjaCcInvalido, CasoLinhaNoC e as 3 sondas do gcc (backend C aposentado); os casos --modo c viram run no x64 (foreign, alias, array, memoria, v1.7, v1.8); +1 caso (CasoX0003: --backend c, --modo c com e sem -o e com --release, --prova-layout = X0003 exit 2).
           1.4.0 - 0.21.0 F3: CasoPadraoReleaseEhC vira CasoPadraoReleaseEhX64 (build --release sem --backend e x64, com a assinatura Forja Native).
           1.3.0 - Portao Z b1: +1 caso (CasoBuildDivisaoRealZeroE0420: 1.0 / 0.0, 0.0 / 0.0 e -1.0 / 0.0 em f32 e f64 = exit 1 com E0420 e a mensagem com math.inf()/math.nan()).
           1.2.0 - P-IA1-LG: +3 casos da sonda de versao do gcc (CasoSondaGcc16/Gcc15/GccIlegivel; gcc_falso.c feito pelo gcc real): flag -fdisable-tree-widening_mul so com gcc >= 16 e a nota fora do stderr.
           1.1.1 - M4 P25b: CasoForjaCcInvalido leva --backend c explicito (FORJA_CC e do gcc; o x64 nao o usa).
           1.1.0 - M4 P25b: +2 casos (CasoPadraoDevEhX64: build sem --backend em dev grava o exe com a assinatura Forja Native; CasoPadraoReleaseEhC: --release sem --backend segue no gcc, sem a assinatura).
           1.0.0 - P9a: casos movidos do verificar_cli.py 1.21.0 sem mudar nenhum.
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import os
import shutil
import tempfile

from verificar_cli import ExemploOla, ConferirExecucao, ConferirJsonCodigo, LinhaJson, Rodar

ASSINATURA_X64 = b"Forja Native"   # string de identificacao do exe do backend x64 no .rdata (verificar_pe item 7)


def CasoBuildComErro(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["build", "erro_tipo.fj", "-o", str(PastaSaidas / "erro.exe")])
    return ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "E0301")


def CasoBuildDivisaoRealZeroE0420(Forja, PastaSaidas):
    """Divisao real por zero constante nao dobra: o build sai 1 com E0420 e a mensagem que manda usar math.inf()/math.nan()."""
    Problemas = []
    Pasta = tempfile.mkdtemp(prefix="cli_divzero_")
    try:
        for Tipo in ("f32", "f64"):
            for Expressao in ("1.0 / 0.0", "0.0 / 0.0", "-1.0 / 0.0"):
                Arquivo = os.path.join(Pasta, "div_zero.fj")
                with open(Arquivo, "w", encoding="utf-8", newline="\n") as Fonte:
                    Fonte.write(f"proc main() {{\n    let _r: {Tipo} = {Expressao}\n    print(1)\n}}\n")
                Rodada = Rodar(Forja, ["build", Arquivo, "-o", os.path.join(Pasta, "div_zero.exe")])
                Rotulo = f"{Tipo} {Expressao}"
                Problemas += [f"{Rotulo}: {P}" for P in ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "E0420")]
                if "use math.inf()/math.nan()" not in Rodada.stdout:
                    Problemas.append(f"{Rotulo}: a mensagem do E0420 nao trouxe o conselho de math.inf()/math.nan()")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoDllSemExport(Forja, PastaSaidas):
    """DLL sem nenhum 'pub export proc' = B0003 (diagnostico, exit 1) e nenhuma DLL."""
    Dll = PastaSaidas / "vazia.dll"
    Rodada = Rodar(Forja, ["build", "--modo", "dll", "sem_teste.fj", "-o", str(Dll)])
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stderr, "B0003")
    if Dll.exists():
        Problemas.append("gerou DLL mesmo sem 'pub export proc'")
    return Problemas


def CasoModoInvalido(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["build", "--modo", "lib", "biblioteca.fj", "-o", str(PastaSaidas / "x.dll")])
    return ConferirExecucao(Rodada, 2)


def CasoForeignRun(Forja, PastaSaidas):
    """0.21.0 F4 passo 3 (era o .c com rotulo e comentario): foreign_libm.fj roda no x64 e imprime 12 (sqrt de 144) e 1024 (pow de 2 e 10)."""
    Rodada = Rodar(Forja, ["run", "foreign_libm.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    if Rodada.stdout.replace("\r\n", "\n") != "12\n1024\n":
        Problemas.append(f"stdout {Rodada.stdout!r}, esperado '12\\n1024\\n'")
    return Problemas


def CasoForeignAlias(Forja, PastaSaidas):
    """v1.10 (A6): proc c_remove = "remove"(...) no x64: run imprime -1 (o rotulo do simbolo C ficava no .c do backend C, aposentado)."""
    return ConferirExecucao(Rodar(Forja, ["run", "foreign_alias.fj"]), 0, "-1")


def CasoArrayRun(Forja, PastaSaidas):
    """0.21.0 F4 passo 3 (era o .c do array): array_ast.fj roda no x64 e imprime 12 (soma 1+2+3 mais c[0] = 4, mais t[0] = 2)."""
    return ConferirExecucao(Rodar(Forja, ["run", "array_ast.fj"]), 0, "12")


def CasoMemoriaRun(Forja, PastaSaidas):
    """0.21.0 F4 passo 3 (era o .c da memoria): memoria_modo_c.fj roda no x64 e imprime 1 (l[0] depois do append)."""
    return ConferirExecucao(Rodar(Forja, ["run", "memoria_modo_c.fj"]), 0, "1")


def CasoV17Run(Forja, PastaSaidas):
    """0.21.0 F4 passo 3 (era o .c com opcional, union, tupla e fmt): v17.fj roda no x64 e imprime '3 1-2' e 'x'."""
    Execucao = Rodar(Forja, ["run", "v17.fj"])
    Problemas = ConferirExecucao(Execucao, 0)
    if Execucao.stdout.replace("\r\n", "\n") != "3 1-2\nx\n":
        Problemas.append(f"stdout {Execucao.stdout!r}, esperado '3 1-2\\nx\\n'")
    return Problemas


def CasoModulosRun(Forja, PastaSaidas):
    """0.21.0 F4 passo 3 (era o .c com #line do modulo): modulos_panico.fj imprime 5 e o panic do modulo sai 101 com R0003 em mods/falha_cli.fj, linha 3."""
    Rodada = Rodar(Forja, ["run", "modulos_panico.fj"])
    Problemas = ConferirExecucao(Rodada, 101, "5")
    Dados = LinhaJson(Rodada.stderr) or {}
    Arquivo = str(Dados.get("arquivo", "")).replace("\\", "/")
    if Dados.get("codigo") != "R0003" or not Arquivo.endswith("mods/falha_cli.fj") or Dados.get("linha") != 3:
        Problemas.append(f"stderr sem R0003 em mods/falha_cli.fj linha 3: {Rodada.stderr.strip()[:160]!r}")
    return Problemas


def ConstruirOlaSemBackend(Forja, Saida, Opcoes):
    """build do 01_ola SEM --backend (so as Opcoes dadas); devolve (problemas, o exe contem a assinatura do x64)."""
    Rodada = Rodar(Forja, ["build"] + Opcoes + [str(ExemploOla), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Saida.exists():
        return Problemas + ["build nao gravou o exe"], False
    return Problemas, ASSINATURA_X64 in Saida.read_bytes()


def CasoPadraoDevEhX64(Forja, PastaSaidas):
    """M4 P25b (D-M4-8): build em dev sem --backend usa o x64; o exe traz 'Forja Native' no .rdata (o do gcc nao)."""
    Problemas, TemAssinatura = ConstruirOlaSemBackend(Forja, PastaSaidas / "padrao_dev.exe", [])
    if not TemAssinatura:
        Problemas.append("o exe de dev sem --backend nao traz a assinatura Forja Native (nao veio do x64)")
    return Problemas


def CasoPadraoReleaseEhX64(Forja, PastaSaidas):
    """0.21.0 F3: build --release sem --backend usa o x64 (o backend C saiu): o exe traz 'Forja Native' no .rdata."""
    Problemas, TemAssinatura = ConstruirOlaSemBackend(Forja, PastaSaidas / "padrao_release.exe", ["--release"])
    if not TemAssinatura:
        Problemas.append("o exe de --release sem --backend nao traz a assinatura Forja Native (o padrao e x64)")
    return Problemas


def CasoX0003(Forja, PastaSaidas):
    """0.21.0 F4 passo 3: backend C e opcoes C (--backend c, --modo c com e sem -o e com --release, --prova-layout) saem com X0003 em UMA linha JSON no stderr, exit 2 e forma_certa preenchida."""
    Pedidos = [
        ["build", "--backend", "c", "sem_teste.fj", "-o", str(PastaSaidas / "x0003_backend.exe")],
        ["build", "--modo", "c", "sem_teste.fj", "-o", str(PastaSaidas / "x0003_modo.c")],
        ["build", "--modo", "c", "sem_teste.fj"],
        ["build", "--modo", "c", "--release", "sem_teste.fj", "-o", str(PastaSaidas / "x0003_release.c")],
        ["build", "--prova-layout", str(PastaSaidas / "x0003_prova.c"), "sem_teste.fj"],
    ]
    Problemas = []
    for Pedido in Pedidos:
        Rodada = Rodar(Forja, Pedido)
        Rotulo = " ".join(Pedido[:3])
        Problemas += [f"{Rotulo}: {P}" for P in ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(Rodada.stderr, "X0003")]
        if not (LinhaJson(Rodada.stderr) or {}).get("forma_certa"):
            Problemas.append(f"{Rotulo}: X0003 sem forma_certa")
    return Problemas
