# -*- coding: utf-8 -*-
"""verificar_std_testes.py - prova dos testes dos modulos std (S4 P3): cada std/<m>/testes.fj roda com `forja test std:<m> --json --backend x64`.

O que prova, por modulo: exit 0; resumo com falhou=0; pelo menos 1 teste; total do resumo igual ao numero de linhas de teste.
Sem o backend C (0.21.0 F4 residuo): a comparacao com o C saiu; cada modulo roda so no x64 e a contagem de testes e a esperada.
Modulo novo entra sozinho: qualquer std/<m>/testes.fj e pego pelo glob. Zero modulos = reprovado (senao "OK: 0" passaria em silencio).

Uso: python -I verificar_std_testes.py [--forja <forja.exe>] [--jobs N]
     python -I verificar_std_testes.py --selftest
Saida: uma linha por modulo; falhas com modulo e motivo; linha final "OK: <N> modulos, <T> testes" (exit 0) ou "FALHOU: ..." (exit 1).
Cada job (modulo) roda com cwd numa pasta temporaria propria (criada e removida pelo proprio job): os testes de std:file gravam no disco e os jobs correm em paralelo.
Versao: 1.3.0
Changelog: 1.3.0 - R2: o job de std:time no Windows leva FORJA_FUSO_ESPERADO (tm_gmtoff do Python em 3 instantes) no ambiente.
1.2.0 - 0.21.0 F4 residuo: sai o backend C (Backends = x64), sai a comparacao de listas c x x64 (DiferencaListas) e o selftest (b) que a testava.
           1.1.0 - S4 P7: cwd por job em pasta temporaria propria (tempfile.mkdtemp, removida no fim mesmo em falha); selftest ganha o caso (f) dois jobs nao dividem cwd e nada sobra no fim.
           1.0.0 - S4 P3: primeira versao (std:math: 3 testes nos dois backends; selftest com falso forja falhou=1, listas diferentes, exit 1 e saida fora do JSON).
Verificacao: python -I verificar_std_testes.py --selftest
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parent.parent
ForjaPadrao = PastaLanguage / "compiler" / "forja.exe"
Backends = ("x64",)
TimeoutSegundos = 600
JobsPadrao = 4


def ModulosComTestes():
    # O glob pega o arquivo, nao uma lista fixa: modulo novo entra sem editar este verificador.
    return sorted(Arquivo.parent.name for Arquivo in (PastaLanguage / "std").glob("*/testes.fj"))


# 2020-06-01, 2023-11-14, 2026-01-10 (UTC): instantes recentes; o SystemTimeToTzSpecificLocalTime aplica a tabela dinamica do Windows e o localtime do Python a regra atual,
# entao os dois so concordam onde a regra do fuso nao mudou (em 1970 e 2018 o Brasil diverge: o Windows da -02:00, o Python -03:00).
InstantesFuso = (1590969600, 1700000000, 1768000000)


def AmbienteDoModulo(Modulo):
    # std:time (R2): o teste do Windows compara local_offset_s com time.localtime().tm_gmtoff do Python do mesmo PC nos 3 instantes.
    Ambiente = dict(os.environ)
    if Modulo == "time" and os.name == "nt":
        Ambiente["FORJA_FUSO_ESPERADO"] = ",".join(str(time.localtime(Segundos).tm_gmtoff) for Segundos in InstantesFuso)
    return Ambiente


def RodarForja(Forja, Modulo, Backend, Pasta):
    # Devolve (exit, stdout). So o stdout e lido: o resumo e as linhas de teste vem ali. Pasta = cwd do job (propria, ver RodarNoJob).
    Proc = subprocess.run(
        [str(Forja), "test", f"std:{Modulo}", "--json", "--backend", Backend],
        cwd=str(Pasta), stdin=subprocess.DEVNULL, capture_output=True, timeout=TimeoutSegundos, env=AmbienteDoModulo(Modulo),
    )
    return Proc.returncode, Proc.stdout.decode("utf-8", errors="replace")


def RodarNoJob(Forja, Modulo, Backend, Rodar):
    # Cada job ganha uma pasta temporaria propria: os testes de std:file gravam no disco e os jobs correm em paralelo.
    # A pasta some no fim, mesmo quando Rodar falha.
    Pasta = tempfile.mkdtemp(prefix=f"verif_std_{Modulo}_{Backend}_")
    try:
        return Rodar(Forja, Modulo, Backend, Pasta)
    finally:
        shutil.rmtree(Pasta)


def AnalisarSaida(Texto):
    # Devolve ([(rotulo, resultado), ...], resumo). Linha que nao e JSON ou sem o campo esperado levanta ValueError.
    Testes = []
    Resumo = None
    try:
        for Linha in Texto.splitlines():
            if not Linha.strip():
                continue
            Objeto = json.loads(Linha)
            if "resumo" in Objeto:
                Resumo = Objeto["resumo"]
            else:
                Testes.append((Objeto["teste"], Objeto["resultado"]))
    except (ValueError, KeyError, TypeError) as Erro:
        raise ValueError(f"saida fora do formato --json: {Erro}") from Erro
    if not isinstance(Resumo, dict):
        raise ValueError("saida sem linha de resumo")
    return Testes, Resumo


def ConferirBackend(Forja, Modulo, Backend, Rodar):
    # Devolve (testes, falhas) de um modulo num backend; cada falha e um texto curto, o relato acrescenta modulo e backend.
    Rc, Texto = RodarNoJob(Forja, Modulo, Backend, Rodar)
    Falhas = [] if Rc == 0 else [f"exit {Rc}, esperado 0"]
    try:
        Testes, Resumo = AnalisarSaida(Texto)
    except ValueError as Erro:
        return [], Falhas + [str(Erro)]
    if Resumo.get("falhou") != 0:
        Falhas.append(f"resumo falhou={Resumo.get('falhou')}, esperado 0")
    if not Testes:
        Falhas.append("nenhum teste rodou (pede pelo menos 1)")
    if Resumo.get("total") != len(Testes):
        Falhas.append(f"resumo total={Resumo.get('total')} com {len(Testes)} linhas de teste")
    return Testes, Falhas


def RodarTodos(Forja, Modulos, Jobs, Rodar):
    # Um futuro por (modulo, backend): modulos correm em paralelo; a comparacao so comeca com tudo pronto.
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Futuros = {(Modulo, Backend): Pool.submit(ConferirBackend, Forja, Modulo, Backend, Rodar) for Modulo in Modulos for Backend in Backends}
        return {Chave: Futuro.result() for Chave, Futuro in Futuros.items()}


def AvaliarModulo(Modulo, Resultados):
    # Devolve (linhas, falhas) de um modulo: uma linha por backend (so o x64 hoje).
    Linhas = []
    Falhas = []
    for Backend in Backends:
        Testes, Erros = Resultados[Backend]
        Estado = "ok" if not Erros else "FALHOU"
        Linhas.append(f"std:{Modulo} | {Backend} | {len(Testes)} testes | {Estado}")
        Falhas += [f"std:{Modulo} {Backend}: {Erro}" for Erro in Erros]
    return Linhas, Falhas


def ExecutarVerificacao(Forja, Jobs):
    # Devolve (linhas, falhas, modulos, testes). Sem modulo nenhum ha uma falha, para que o vazio nao aprove.
    Modulos = ModulosComTestes()
    if not Modulos:
        return [], ["nenhum modulo com testes.fj em std/<m>/ (o verificador nao aprova vazio)"], 0, 0
    Resultados = RodarTodos(Forja, Modulos, Jobs, RodarForja)
    Linhas = []
    Falhas = []
    Testes = 0
    for Modulo in Modulos:
        LinhasModulo, FalhasModulo = AvaliarModulo(Modulo, {Backend: Resultados[(Modulo, Backend)] for Backend in Backends})
        Linhas += LinhasModulo
        Falhas += FalhasModulo
        Testes += len(Resultados[(Modulo, "x64")][0])
    return Linhas, Falhas, len(Modulos), Testes


# ---------- selftest ----------

def SaidaSintetica(Rotulos, Falhou=0):
    # Mesmo formato do forja test --json: uma linha por teste e a linha do resumo no fim.
    Linhas = [json.dumps({"teste": Rotulo, "resultado": "ok"}) for Rotulo in Rotulos]
    Resumo = {"total": len(Rotulos), "ok": len(Rotulos) - Falhou, "falhou": Falhou, "filtrados": 0}
    Linhas.append(json.dumps({"resumo": Resumo}))
    return "\n".join(Linhas) + "\n"


def FabricarRodar(SaidaPorBackend, Rc=0):
    # Substituto de RodarForja: responde com a saida dada por backend, sem chamar o forja.
    def Falso(Forja, Modulo, Backend, Pasta):
        return Rc, SaidaPorBackend[Backend]
    return Falso


def FalhasDoModulo(Rodar, Modulo="fake"):
    # Passa o modulo falso pelo mesmo caminho do verificador (RodarTodos + AvaliarModulo) e devolve so as falhas.
    Resultados = RodarTodos(None, [Modulo], 2, Rodar)
    return AvaliarModulo(Modulo, {Backend: Resultados[(Modulo, Backend)] for Backend in Backends})[1]


def CwdDosJobsSeparados():
    # Caso (f): 2 modulos (1 backend, x64) em paralelo; cada job recebe uma pasta propria, existente durante o job e removida depois.
    Vistas = []

    def Registrar(Forja, Modulo, Backend, Pasta):
        Vistas.append((Pasta, os.path.isdir(Pasta), os.path.abspath(Pasta) != str(PastaLanguage)))
        return 0, SaidaSintetica(["[std:fake] soma"])

    RodarTodos(None, ["fake_a", "fake_b"], 4, Registrar)
    Pastas = [Pasta for Pasta, _, _ in Vistas]
    TodasExistiam = all(Existia and Fora for _, Existia, Fora in Vistas)
    return len(Vistas) == 2 and len(set(Pastas)) == 2 and TodasExistiam and not any(os.path.exists(Pasta) for Pasta in Pastas)


def Selftest():
    Casos = []

    def Exigir(Nome, Condicao):
        Casos.append((Nome, bool(Condicao)))

    Rotulos = ["[std:fake] soma", "[std:fake] subtrai"]
    # (a) falso forja: exit 0, mas o resumo diz falhou=1; tem de reprovar pelo resumo, nao pelo exit.
    FalsoFalhou = FabricarRodar({"x64": SaidaSintetica(Rotulos, Falhou=1)})
    Exigir("(a) falso forja com resumo falhou=1 reprova", FalhasDoModulo(FalsoFalhou))
    # Controle: saida verde tem de passar, senao o verificador reprova tudo e o selftest nao prova nada.
    FalsoIgual = FabricarRodar({"x64": SaidaSintetica(Rotulos)})
    Exigir("controle: saidas iguais e verdes passam", FalhasDoModulo(FalsoIgual) == [])
    # Formato real de uma falha (forja test --json, R0003): resultado "falhou" e campos extras; so teste e resultado entram na comparacao.
    LinhaFalha = json.dumps({"teste": "expect falso", "resultado": "falhou", "codigo": "R0003", "arquivo": "tests/cli/teste_falha.fj", "linha": 6, "causa": "expect falhou: 1 == 2"})
    LinhaResumoFalha = json.dumps({"resumo": {"total": 1, "ok": 0, "falhou": 1, "filtrados": 0}})
    FalsoFalhaReal = FabricarRodar({"x64": LinhaFalha + "\n" + LinhaResumoFalha + "\n"}, Rc=1)
    Exigir("falha real do forja (resultado falhou, campos extras) reprova", FalhasDoModulo(FalsoFalhaReal))
    # Campos volateis (tempo, caminho com outra barra) nao contam: as listas comparam so teste e resultado.
    LinhaVolatil = json.dumps({"teste": "[std:fake] soma", "resultado": "ok", "ms": 9, "arquivo": "tests\\std\\x.fj"})
    LinhaResumoOk = json.dumps({"resumo": {"total": 1, "ok": 1, "falhou": 0, "filtrados": 0}})
    FalsoVolatil = FabricarRodar({"x64": LinhaVolatil + "\n" + LinhaResumoOk + "\n"})
    Exigir("campos volateis nao entram na comparacao", FalhasDoModulo(FalsoVolatil) == [])
    Exigir("exit 1 com resumo verde reprova", FalhasDoModulo(FabricarRodar({"x64": SaidaSintetica(Rotulos)}, Rc=1)))
    Exigir("saida fora do JSON reprova", FalhasDoModulo(FabricarRodar({"x64": "isto nao e json\n"})))
    Exigir("modulo sem nenhum teste reprova", FalhasDoModulo(FabricarRodar({"x64": SaidaSintetica([])})))
    Exigir("(f) dois jobs nao dividem cwd e a pasta some no fim", CwdDosJobsSeparados())
    Erros = [Nome for Nome, Ok in Casos if not Ok]
    for Nome in Erros:
        print(f"FALHOU selftest: {Nome}")
    if Erros:
        print(f"FALHOU: selftest {len(Erros)} de {len(Casos)} casos")
        return 1
    print(f"OK: selftest {len(Casos)} casos")
    return 0


def Principal():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    Args = argparse.ArgumentParser(description="Roda os testes std de cada modulo (std/<m>/testes.fj) nos 2 backends do forja e confere resumo e listas.")
    Args.add_argument("--forja", type=Path, default=ForjaPadrao, help="caminho do forja.exe (padrao: compiler/forja.exe)")
    Args.add_argument("--jobs", type=int, default=JobsPadrao, help="pares (modulo, backend) em paralelo")
    Args.add_argument("--selftest", action="store_true", help="prova o proprio verificador com casos negativos e sai")
    Dados = Args.parse_args()
    if Dados.selftest:
        return Selftest()
    if Dados.jobs < 1:
        Args.error("--jobs precisa ser >= 1")
    Forja = Dados.forja.resolve()
    if not Forja.is_file():
        Args.error(f"forja nao encontrado: {Forja}")
    Linhas, Falhas, NModulos, NTestes = ExecutarVerificacao(Forja, Dados.jobs)
    for Linha in Linhas:
        print(Linha)
    if Falhas:
        for Falha in Falhas:
            print(f"FALHOU: {Falha}")
        return 1
    print(f"OK: {NModulos} modulos, {NTestes} testes")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
