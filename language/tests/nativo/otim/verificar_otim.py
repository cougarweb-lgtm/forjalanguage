# -*- coding: utf-8 -*-
"""Runner diferencial dos passes de otimizacao da BIR (M7a F0): passe ligado x desligado (--sem-otim tudo), com bissecao.

Para cada alvo (casos de tests\\lowering amostrados de forma deterministica + os benches de tests\\nativo\\bench) compila
'forja build --backend x64 --release' com a opcao padrao e com '--sem-otim tudo':
  - sem passe registrado (F0): o exe tem de sair IGUAL byte a byte;
  - com passes: roda os dois exes e exige o mesmo stdout e o mesmo codigo de saida; quando diverge, a bissecao acha o menor
    prefixo da lista de passes ligados que ja diverge (o ultimo passe do prefixo e o culpado).
--passe <nome> liga so aquele passe (os outros ficam em --sem-otim) contra o desligado.
Os passes validos vem da propria mensagem de erro do forja ('--sem-otim: passo desconhecido ... (validos: tudo, a, b)').

Uso:
    python verificar_otim.py --forja C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe [--amostra 40] [--passe NOME] [--jobs N]
    python verificar_otim.py --forja C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe --contra <forja_base.exe> [--amostra 40] [--jobs N]
    python verificar_otim.py --selftest
Saida final: "OK: N/N alvos iguais" (ou "FALHOU: ..."), exit 0 verde, 1 com divergencia, 2 em erro de uso.
--contra: compila os mesmos alvos com o forja novo e com o forja BASE congelado (amostra + benches em --release; os benches tambem em dev) e
exige exe IGUAL byte a byte (SHA256); prova que uma fatia neutra (ex.: M7a F2) nao mudou nenhum byte.

--bisseccao <arquivo.fj> [--checagem sim|nao]: compila ligado e com '--sem-otim tudo', e se o stdout/stderr/exit divergem acha o MENOR conjunto de passes
cujo desligamento elimina a divergencia (um passe ou uma combinacao); o diferencial normal tambem imprime esse conjunto quando falha.
--determinismo [--amostra N]: 2 builds release do mesmo alvo com --checagem sim e com --checagem nao; os bytes (com a versao do forja e o GUID mascarados) tem de ser iguais.

--semente N: numero da FATIA do rodizio (D0696): gira a amostra do tests\\lowering (--amostra) em N posicoes; 0 = a amostra de sempre. Fatia 0..3 cobre 4 conjuntos diferentes.

Versao: 1.10.0
Changelog: 1.10.0 - C1-B (origem): fixtures do passe origem (tests\\nativo\\otim\\origem\\: aceite e recusa) entram em ListarAlvos.
           1.9.0 - C1-A (curto): fixtures do passe curto (tests\\nativo\\otim\\curto\\: aceite e recusa) entram em ListarAlvos.
           1.8.1 -portao LG do G3: --contra mascara a versao (mascara_versao) tambem quando o forja novo e o base tem versoes diferentes, sem precisar de --checagem-*; antes o dev dos benches falhava por SHA so por "Forja Native <versao>", StringFileInfo e GUID do RSDS (.text igual). 1.8.0 - S4 (costura): fixtures do passe costura (tests\\nativo\\otim\\costura\\: aceite e recusa) entram em ListarAlvos.
           1.7.0 - M7b F2b: fixtures da guarda de retorno antecipado (tests\\nativo\\otim\\guarda\\: aceite e recusa) entram em ListarAlvos.
           1.6.1 - P3: a mascara da versao do forja saiu para mascara_versao.py (HashSemVersao/BytesSemVersao: so as ancoras; versao de `forja --versao`); SemVersaoDoForja e VERSOES_FORJA saem daqui.
           1.6.0 - M7b R4a: --sem-passe-base (com --contra --sem-passe: o passe tambem desligado no forja base).
           1.5.0- D0696: --semente N (numero da fatia; AmostrarCasos/ListarAlvos ganham Deslocamento/Semente, 0 = comportamento anterior).
           1.4.0 - M7a F9: --bisseccao (menor conjunto de passes culpado, funcao MenorConjuntoCulpado), --determinismo (2 builds byte a byte, sim e nao), selftest da bissecao com passe culpado simulado.
           1.3.0- M7a F9a: --checagem-padrao e --checagem-nao (com --contra; o padrao do release virou sim). 1.2.0 - M7a F3: --sem-passe (com --contra); --release-muda (com --contra: release pode diferir do base com stdout/exit iguais; dev e o modo antigo seguem byte a byte).
           1.1.0 - M7a F2: --contra <forja_base.exe> (SHA256 do exe novo x base em release e, nos benches, tambem em dev) e seu selftest.
           1.0.0 - M7a F0: primeira versao (amostra deterministica, 14 benches, --passe, bissecao, --selftest).
Verificacao: python -I verificar_otim.py --selftest
"""

import argparse
import concurrent.futures
import filecmp
import hashlib
import itertools
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaTestes = PastaAqui.parents[1]
PastaLowering = PastaTestes / "lowering"
PastaBench = PastaTestes / "nativo" / "bench"
PastaGuarda = PastaAqui / "guarda"
PastaCostura = PastaAqui / "costura"
PastaCurto = PastaAqui / "curto"
PastaOrigem = PastaAqui / "origem"
FORJA_PADRAO = "C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe"
AMOSTRA_PADRAO = 40
TIMEOUT_BUILD = 120
TIMEOUT_EXEC = 60
JOBS_PADRAO = 4
PASSE_INEXISTENTE = "__passe_inexistente__"
RE_VALIDOS = re.compile(r"validos: tudo, (.*)\)")
sys.path.insert(0, str(PastaAqui))  # mascara_versao (P3) importa tambem com python -I
import mascara_versao as Mascara  # noqa: E402


class ErroOtim(Exception):
    pass


def AmostrarCasos(Casos, Quantidade, Deslocamento=0):
    """Quantidade casos espalhados por igual na lista ordenada (sem aleatoriedade: o mesmo resultado em toda rodada); Deslocamento = fatia do rodizio."""
    Casos = sorted(Casos)
    if Quantidade >= len(Casos):
        return Casos
    return [Casos[((K * len(Casos)) // Quantidade + Deslocamento) % len(Casos)] for K in range(Quantidade)]


def ListarAlvos(Amostra, Semente=0):
    Lowering = [str(C) for C in PastaLowering.glob("*.fj")]
    Benches = sorted(str(C) for C in PastaBench.glob("*.fj"))
    Guarda = sorted(str(C) for C in PastaGuarda.glob("*.fj"))  # F2b: aceite e recusa da guarda de retorno antecipado
    Costura = sorted(str(C) for C in PastaCostura.glob("*.fj"))  # S4: aceite e recusa do passe costura (jump threading)
    Curto = sorted(str(C) for C in PastaCurto.glob("*.fj"))  # C1-A: aceite e recusa do passe curto (curto-circuito and/or)
    Origem = sorted(str(C) for C in PastaOrigem.glob("*.fj"))  # C1-B: aceite e recusa do passe origem (copia de agregado lido so por cargas)
    return AmostrarCasos(Lowering, Amostra, Semente) + Benches + Guarda + Costura + Curto + Origem


def LerPassesDaMensagem(Texto):
    """Lista de passes registrados a partir do texto de erro do forja; [] quando 'nenhum passe registrado'."""
    Achado = RE_VALIDOS.search(Texto)
    if Achado is None:
        raise ErroOtim(f"nao entendi a lista de passes na mensagem do forja: {Texto.strip()[:200]}")
    if "nenhum passe registrado" in Achado.group(1):
        return []
    return [N.strip() for N in Achado.group(1).split(",") if N.strip()]


def Executar(Comando, Timeout):
    return subprocess.run(Comando, capture_output=True, timeout=Timeout)


def DescobrirPasses(Forja, Fonte):
    Resultado = Executar([Forja, "build", "--backend", "x64", "--release", Fonte, "-o", os.devnull, "--sem-otim", PASSE_INEXISTENTE], TIMEOUT_BUILD)
    if Resultado.returncode != 2:
        raise ErroOtim(f"o forja deveria recusar um passo inventado com exit 2, deu {Resultado.returncode}")
    return LerPassesDaMensagem(Resultado.stderr.decode("utf-8", errors="replace"))


def Compilar(Forja, Fonte, Saida, SemOtim, Release=True, Checagem=None):
    Comando = [Forja, "build", "--backend", "x64"] + (["--release"] if Release else []) + [Fonte, "-o", str(Saida)]
    if Checagem and Release:
        Comando += ["--checagem", Checagem]
    if SemOtim:
        Comando += ["--sem-otim", ",".join(SemOtim)]
    Resultado = Executar(Comando, TIMEOUT_BUILD)
    if Resultado.returncode != 0:
        Causa = (Resultado.stderr + Resultado.stdout).decode("utf-8", errors="replace").strip()[:300]
        raise ErroOtim(f"{Path(Fonte).name}: build falhou (exit {Resultado.returncode}): {Causa}")


def Rodar(Exe):
    try:
        Resultado = Executar([str(Exe)], TIMEOUT_EXEC)
    except subprocess.TimeoutExpired:
        return ("timeout", b"")
    return (Resultado.returncode, Resultado.stdout)


def Diverge(Forja, Fonte, Pasta, Desligados):
    """Compila com esses passes desligados e compara o comportamento com o build 'tudo desligado'."""
    Base = Pasta / "base.exe"
    Teste = Pasta / "teste.exe"
    Compilar(Forja, Fonte, Base, ["tudo"])
    Compilar(Forja, Fonte, Teste, Desligados)
    return Rodar(Base) != Rodar(Teste)


def MenorPrefixoQueDiverge(Passes, DivergeCom):
    """Menor k tal que ligar so os k primeiros passes ja diverge; DivergeCom(Ligados) -> bool. None se nenhum prefixo diverge."""
    for K in range(1, len(Passes) + 1):
        if DivergeCom(Passes[:K]):
            return K
    return None


def Bissectar(Forja, Fonte, Pasta, Passes):
    def DivergeCom(Ligados):
        return Diverge(Forja, Fonte, Pasta, [P for P in Passes if P not in Ligados])

    K = MenorPrefixoQueDiverge(Passes, DivergeCom)
    return None if K is None else Passes[K - 1]


def MenorConjuntoCulpado(Passes, DivergeCom):
    """Menor conjunto de passes cujo DESLIGAMENTO elimina a divergencia; DivergeCom(Desligados) -> bool (True = ainda diverge).
    Tamanho 1 primeiro (um passe culpado), depois pares, trios...; [] quando nenhum subconjunto resolve."""
    for Tamanho in range(1, len(Passes) + 1):
        for Conjunto in itertools.combinations(Passes, Tamanho):
            if not DivergeCom(list(Conjunto)):
                return list(Conjunto)
    return []


def NormalizarStderr(Bruto):
    """stderr comparavel: do JSON de erro de execucao fica so (codigo, linha, causa); texto sem JSON fica como veio."""
    Partes = []
    for Linha in Bruto.decode("utf-8", errors="replace").splitlines():
        try:
            Dados = json.loads(Linha)
            Partes.append((Dados.get("codigo"), Dados.get("linha"), Dados.get("causa")))
        except ValueError:
            Partes.append(Linha.strip())
    return tuple(Partes)


def RodarCompleto(Exe):
    """(exit, stdout, stderr normalizado); ('timeout', b'', ()) quando passa do limite."""
    try:
        Resultado = Executar([str(Exe)], TIMEOUT_EXEC)
    except subprocess.TimeoutExpired:
        return ("timeout", b"", ())
    return (Resultado.returncode, Resultado.stdout, NormalizarStderr(Resultado.stderr))


def BisseccaoPasses(Forja, Fonte, Pasta, Passes, Checagem=None, Rodar=RodarCompleto):
    """None se ligado e 'tudo desligado' se comportam igual (stdout, stderr normalizado, exit); senao o menor conjunto de passes culpado.
    Rodar(exe) -> tupla comparavel (injetavel para o selftest)."""
    Base, Teste = Pasta / "bis_base.exe", Pasta / "bis_teste.exe"
    Compilar(Forja, Fonte, Base, ["tudo"], True, Checagem)
    Esperado = Rodar(Base)
    Compilar(Forja, Fonte, Teste, [], True, Checagem)
    if Rodar(Teste) == Esperado:
        return None

    def DivergeCom(Desligados):
        Compilar(Forja, Fonte, Teste, Desligados, True, Checagem)
        return Rodar(Teste) != Esperado

    return MenorConjuntoCulpado(Passes, DivergeCom)


def VerificarAlvo(Forja, Fonte, Passes, PasseUnico):
    """None se o alvo passa; senao o texto da falha."""
    with tempfile.TemporaryDirectory(prefix="verificar_otim_") as Temp:
        Pasta = Path(Temp)
        Padrao, Desligado = Pasta / "padrao.exe", Pasta / "tudo.exe"
        Ligados = [PasseUnico] if PasseUnico else Passes
        Compilar(Forja, Fonte, Padrao, [P for P in Passes if P not in Ligados])
        Compilar(Forja, Fonte, Desligado, ["tudo"])
        if not Passes:
            return None if filecmp.cmp(Padrao, Desligado, shallow=False) else "sem passe registrado e o exe difere do --sem-otim tudo"
        if Rodar(Padrao) == Rodar(Desligado):
            return None
        Culpado = Bissectar(Forja, Fonte, Pasta, Ligados)
        Conjunto = MenorConjuntoCulpado(Ligados, lambda D: Diverge(Forja, Fonte, Pasta, D))
        return f"stdout/exit divergem; passe culpado: {Culpado}; menor conjunto cujo desligamento resolve: {', '.join(Conjunto) or 'nenhum'}"


def Sha256Do(Arquivo, Versoes=None):
    """SHA256 do arquivo; com Versoes (Mascara.VersoesDoForja) o hash sai sem os bytes de versao do forja (mascara_versao.HashSemVersao)."""
    if not Versoes:
        return hashlib.sha256(Path(Arquivo).read_bytes()).hexdigest()
    return Mascara.HashSemVersao(Arquivo, Versoes)


def ListarAlvosContra(Amostra, Semente=0):
    """(fonte, release): amostra + benches em release, e os benches tambem em dev."""
    Alvos = ListarAlvos(Amostra, Semente)
    Benches = [A for A in Alvos if Path(A).parent == PastaBench]
    return [(A, True) for A in Alvos] + [(A, False) for A in Benches]


SemPasseNoBase = [False]  # --sem-passe-base: o --sem-passe vale tambem no forja base (base que ja tem o passe)


def VerificarContraAlvo(Forja, Base, Fonte, Release, ReleaseMuda=False, SemOtim=None, Checagem=(None, None)):
    """None se o exe do forja novo e o do base tem o mesmo SHA256; senao o texto da falha. Checagem = (--checagem do novo, --checagem do base) no release (F9a).
    ReleaseMuda (fatia que muda o codigo do release): o release PODE diferir do base, mas stdout e exit tem de ser iguais; o dev segue exigindo SHA igual."""
    with tempfile.TemporaryDirectory(prefix="verificar_otim_contra_") as Temp:
        Saida = Path(Temp) / "alvo.exe"  # o caminho de saida entra no exe dev: os dois builds usam o MESMO, um depois do outro
        Novo = Path(Temp) / "novo.exe"
        Compilar(Forja, Fonte, Saida, SemOtim if Release else [], Release, Checagem[0])
        VersoesDiferem = Mascara.VersaoDoForjaUnica(str(Forja)) != Mascara.VersaoDoForjaUnica(str(Base))
        Versoes = Mascara.VersoesDoForja(Forja, Base) if Checagem != (None, None) or VersoesDiferem else None  # P3: versao do forja novo e do base saem do hash
        HashNovo = Sha256Do(Saida, Versoes)
        if Release and ReleaseMuda:
            Saida.replace(Novo)
        Compilar(Base, Fonte, Saida, SemOtim if Release and SemPasseNoBase[0] else [], Release, Checagem[1])
        if HashNovo == Sha256Do(Saida, Versoes):
            return None
        if Release and ReleaseMuda:
            return None if Rodar(Novo) == Rodar(Saida) else "release difere do base e stdout/exit divergem"
        return "exe difere do base (SHA256)"


def ChecagemDoContra(Args):
    """(novo, base): --checagem-padrao = novo sem flag x base com `sim` (o padrao novo e sim); --checagem-nao = novo com `nao` x base sem flag (o padrao antigo era nao)."""
    if Args.checagem_padrao:
        return (None, "sim")
    return ("nao", None) if Args.checagem_nao else (None, None)


def PrincipalContra(Args):
    for Caminho in (Args.forja, Args.contra):
        if not os.path.isabs(Caminho) or not os.path.isfile(Caminho):
            raise ErroOtim(f"--forja e --contra precisam ser caminhos absolutos de arquivos existentes: {Caminho}")
    Alvos = ListarAlvosContra(Args.amostra, Args.semente)
    print(f"verificar_otim {VERSAO} --contra | alvos: {len(Alvos)} (release + dev dos benches)")
    with concurrent.futures.ThreadPoolExecutor(max_workers=Args.jobs) as Pool:
        Futuros = [((A, R), Pool.submit(VerificarContraAlvo, Args.forja, Args.contra, A, R, Args.release_muda, [Args.sem_passe] if Args.sem_passe else None, ChecagemDoContra(Args))) for A, R in Alvos]
        Falhas = [(f"{Path(A).name} [{'release' if R else 'dev'}]", F.result()) for (A, R), F in Futuros if F.result() is not None]
    for Nome, Causa in Falhas:
        print(f"FALHOU {Nome}: {Causa}")
    Total = len(Alvos)
    print(f"{'OK' if not Falhas else 'FALHOU'}: {Total - len(Falhas)}/{Total} alvos iguais ao base")
    return 0 if not Falhas else 1


def VerificarDeterminismoAlvo(Forja, Fonte):
    """None se 2 builds release do mesmo alvo (com --checagem sim e com nao) saem iguais byte a byte (versao do forja e GUID mascarados)."""
    with tempfile.TemporaryDirectory(prefix="verificar_otim_det_") as Temp:
        for Checagem in ("sim", "nao"):
            Bytes = []
            for Rodada in ("a", "b"):
                Saida = Path(Temp) / Rodada / "alvo.exe"
                Saida.parent.mkdir(exist_ok=True)
                Compilar(Forja, Fonte, Saida, [], True, Checagem)
                Bytes.append(Mascara.BytesSemVersao(Saida, Mascara.VersoesDoForja(Forja)))
            if Bytes[0] != Bytes[1]:
                return f"2 builds --checagem {Checagem} saem diferentes (primeiro byte diferente em {next(K for K, (X, Y) in enumerate(zip(*Bytes)) if X != Y) if len(Bytes[0]) == len(Bytes[1]) else 'tamanho'})"
    return None


def PrincipalDeterminismo(Args):
    Alvos = ListarAlvos(Args.amostra, Args.semente)
    print(f"verificar_otim {VERSAO} --determinismo | alvos: {len(Alvos)} (2 builds x checagem sim/nao)")
    with concurrent.futures.ThreadPoolExecutor(max_workers=Args.jobs) as Pool:
        Futuros = [(A, Pool.submit(VerificarDeterminismoAlvo, Args.forja, A)) for A in Alvos]
        Falhas = [(Path(A).name, F.result()) for A, F in Futuros if F.result() is not None]
    for Nome, Causa in Falhas:
        print(f"FALHOU {Nome}: {Causa}")
    print(f"{'OK' if not Falhas else 'FALHOU'}: {len(Alvos) - len(Falhas)}/{len(Alvos)} alvos deterministas")
    return 0 if not Falhas else 1


def PrincipalBisseccao(Args):
    Fonte = os.path.abspath(Args.bisseccao)
    if not os.path.isfile(Fonte):
        raise ErroOtim(f"--bisseccao: arquivo nao encontrado: {Fonte}")
    Passes = DescobrirPasses(Args.forja, Fonte)
    with tempfile.TemporaryDirectory(prefix="verificar_otim_bis_") as Temp:
        Conjunto = BisseccaoPasses(Args.forja, Fonte, Path(Temp), Passes, Args.checagem)
    if Conjunto is None:
        print(f"OK: {Path(Fonte).name} sem divergencia (passes: {', '.join(Passes)})")
        return 0
    print(f"FALHOU: {Path(Fonte).name} diverge; passe(s) culpado(s): {', '.join(Conjunto) or 'nenhum subconjunto resolve'}")
    return 1


def Principal(Args):
    Forja = Args.forja
    if not os.path.isabs(Forja) or not os.path.isfile(Forja):
        raise ErroOtim(f"--forja precisa ser caminho absoluto de um arquivo existente: {Forja}")
    if Args.bisseccao:
        return PrincipalBisseccao(Args)
    if Args.determinismo:
        return PrincipalDeterminismo(Args)
    if Args.contra:
        return PrincipalContra(Args)
    Alvos = ListarAlvos(Args.amostra, Args.semente)
    Passes = DescobrirPasses(Forja, Alvos[0])
    if Args.passe and Args.passe not in Passes:
        raise ErroOtim(f"--passe {Args.passe}: passe desconhecido (registrados: {', '.join(Passes) or 'nenhum'})")
    print(f"verificar_otim {VERSAO} | passes: {', '.join(Passes) or 'nenhum'} | alvos: {len(Alvos)}")
    with concurrent.futures.ThreadPoolExecutor(max_workers=Args.jobs) as Pool:
        Futuros = [(A, Pool.submit(VerificarAlvo, Forja, A, Passes, Args.passe)) for A in Alvos]
        Falhas = [(Path(A).name, F.result()) for A, F in Futuros if F.result() is not None]
    for Nome, Causa in Falhas:
        print(f"FALHOU {Nome}: {Causa}")
    Total = len(Alvos)
    print(f"{'OK' if not Falhas else 'FALHOU'}: {Total - len(Falhas)}/{Total} alvos iguais")
    return 0 if not Falhas else 1


VERSAO = "1.8.1"


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def TesteAmostraDeterministica():
    Casos = [f"c{K:03d}.fj" for K in range(392)]
    A, B = AmostrarCasos(Casos, 40), AmostrarCasos(list(reversed(Casos)), 40)
    Afirmar(A == B and len(A) == 40 and len(set(A)) == 40, "a amostra deveria ser igual, de 40 casos distintos")
    Afirmar(AmostrarCasos(Casos[:5], 40) == sorted(Casos[:5]), "amostra maior que a lista devolve a lista toda")


def TesteAlvosDaLista():
    Alvos = ListarAlvos(AMOSTRA_PADRAO)
    Afirmar(len(Alvos) == AMOSTRA_PADRAO + 14, f"esperava 40 casos + 14 benches, vieram {len(Alvos)}")
    Afirmar(sum(1 for A in Alvos if Path(A).parent == PastaBench) == 14, "os 14 benches deveriam entrar")


def TesteListaDePasses():
    Afirmar(LerPassesDaMensagem("forja: --sem-otim: passo desconhecido 'x' (validos: tudo, nenhum passe registrado)") == [], "tabela vazia = []")
    Afirmar(LerPassesDaMensagem("forja: --sem-otim: passo desconhecido 'x' (validos: tudo, a, b)") == ["a", "b"], "dois passes")
    try:
        LerPassesDaMensagem("outra coisa")
    except ErroOtim:
        return
    raise AssertionError("mensagem fora do formato deveria falhar")


def TesteBissecao():
    Passes = ["a", "b", "c", "d"]
    Afirmar(MenorPrefixoQueDiverge(Passes, lambda L: "c" in L) == 3, "o culpado c fecha o prefixo de tamanho 3")
    Afirmar(MenorPrefixoQueDiverge(Passes, lambda L: False) is None, "sem divergencia = None")
    Afirmar(MenorPrefixoQueDiverge(Passes, lambda L: "a" in L) == 1, "o culpado a fecha o prefixo 1")


def TesteConjuntoCulpado():
    Passes = ["checagem", "copia_arg", "alocador", "isel"]
    Afirmar(MenorConjuntoCulpado(Passes, lambda D: "alocador" not in D) == ["alocador"], "culpado unico: alocador")
    Afirmar(MenorConjuntoCulpado(Passes, lambda D: not ("copia_arg" in D and "isel" in D)) == ["copia_arg", "isel"], "combinacao copia_arg+isel")
    Afirmar(MenorConjuntoCulpado(Passes, lambda D: True) == [], "nenhum subconjunto resolve = []")
    Afirmar(NormalizarStderr(b'{"codigo": "R0006", "linha": 7, "causa": "x", "arquivo": "a.fj"}\n') == (("R0006", 7, "x"),), "stderr JSON normalizado")


def TesteBisseccaoReal():
    """Passe culpado injetado: um forja falso (script) so adultera o exe quando o passe 'alocador' esta ligado; a bissecao tem de apontar so ele."""
    with tempfile.TemporaryDirectory(prefix="verificar_otim_bis_st_") as Temp:
        Falso = Path(Temp) / "forja_falso.py"
        Falso.write_text(FORJA_FALSO, encoding="utf-8")
        Bat = Path(Temp) / "forja_falso.bat"
        Bat.write_text(f'@"{sys.executable}" "{Falso}" %*\r\n', encoding="ascii")
        Fonte = Path(Temp) / "a.fj"
        Fonte.write_text("proc main() {\n}\n", encoding="utf-8")
        Texto = lambda Exe: (0, Path(Exe).read_bytes(), ())  # o "exe" do forja falso e um texto: sua saida e o conteudo
        Conjunto = BisseccaoPasses(str(Bat), str(Fonte), Path(Temp), ["checagem", "copia_arg", "alocador", "isel"], None, Texto)
        Afirmar(Conjunto == ["alocador"], f"a bissecao deveria apontar alocador, apontou {Conjunto}")
        Afirmar(BisseccaoPasses(str(Bat), str(Fonte), Path(Temp), ["checagem", "copia_arg"], None, Texto) == [], "sem o passe culpado na lista nenhum subconjunto resolve")


FORJA_FALSO = """import sys
a = sys.argv
saida = a[a.index("-o") + 1]
desl = a[a.index("--sem-otim") + 1].split(",") if "--sem-otim" in a else []
ruim = "tudo" not in desl and "alocador" not in desl
open(saida, "w").write("import sys\\nprint(%s)\\n" % ("2" if ruim else "1"))
"""


def TesteForjaReal():
    Forja = FORJA_PADRAO
    Afirmar(os.path.isfile(Forja), f"forja nao encontrado: {Forja}")
    Passes = DescobrirPasses(Forja, str(PastaBench / "particulas_soa.fj"))
    Afirmar(isinstance(Passes, list), "a lista de passes deveria ser uma lista")
    Falha = VerificarAlvo(Forja, str(PastaBench / "fib_rec.fj"), Passes, None)
    Afirmar(Falha is None, f"fib_rec deveria passar: {Falha}")


def TesteContra():
    Alvos = ListarAlvosContra(AMOSTRA_PADRAO)
    Afirmar(len(Alvos) == AMOSTRA_PADRAO + 14 + 14, f"esperava 54 em release + 14 em dev, vieram {len(Alvos)}")
    Afirmar(sum(1 for _, R in Alvos if not R) == 14, "so os 14 benches entram em dev")
    Forja = FORJA_PADRAO
    Afirmar(os.path.isfile(Forja), f"forja nao encontrado: {Forja}")
    Fonte = str(PastaBench / "fib_rec.fj")
    Falha = VerificarContraAlvo(Forja, Forja, Fonte, True) or VerificarContraAlvo(Forja, Forja, Fonte, False)
    Afirmar(Falha is None, f"o forja contra ele mesmo deveria dar exe igual: {Falha}")
    with tempfile.TemporaryDirectory(prefix="verificar_otim_st_") as Temp:
        A, B = Path(Temp) / "a.bin", Path(Temp) / "b.bin"
        A.write_bytes(b"abc")
        B.write_bytes(b"abd")
        Afirmar(Sha256Do(A) != Sha256Do(B), "bytes diferentes deveriam dar SHA256 diferente")


def Selftest():
    Testes = [("AmostraDeterministica", TesteAmostraDeterministica), ("AlvosDaLista", TesteAlvosDaLista), ("ListaDePasses", TesteListaDePasses),
              ("Bissecao", TesteBissecao), ("ConjuntoCulpado", TesteConjuntoCulpado), ("BisseccaoReal", TesteBisseccaoReal), ("ForjaReal", TesteForjaReal), ("Contra", TesteContra)]
    Falhas = 0
    for Nome, Teste in Testes:
        try:
            Teste()
        except Exception as Erro:
            print(f"FALHOU {Nome}: {Erro}")
            Falhas += 1
            continue
        print(f"OK {Nome}")
    print(f"selftest: {len(Testes) - Falhas}/{len(Testes)}")
    return Falhas == 0


def Main():
    Parser = argparse.ArgumentParser(description="Diferencial dos passes de otimizacao da BIR (passe ligado x --sem-otim tudo).")
    Parser.add_argument("--forja", default=FORJA_PADRAO, help="forja.exe (caminho absoluto)")
    Parser.add_argument("--amostra", type=int, default=AMOSTRA_PADRAO, help="quantos casos do tests\\lowering entram (alem dos benches)")
    Parser.add_argument("--semente", type=int, default=0, help="numero da fatia do rodizio: gira a amostra do lowering (0 = a de sempre)")
    Parser.add_argument("--passe", default=None, help="liga so este passe contra o desligado")
    Parser.add_argument("--jobs", type=int, default=JOBS_PADRAO)
    Parser.add_argument("--release-muda", action="store_true", help="com --contra: fatia que muda o release (F3+); o release pode diferir do base mas stdout/exit tem de ser iguais; o dev segue igual byte a byte")
    Parser.add_argument("--sem-passe", default=None, help="com --contra: compila o forja novo com --sem-otim <passe> (sem --release-muda: exe igual ao base byte a byte)")
    Parser.add_argument("--sem-passe-base", action="store_true", help="com --contra --sem-passe: desliga o passe tambem no forja base (base que ja tem o passe; M7b R4a)")
    Parser.add_argument("--checagem-padrao", action="store_true", help="com --contra (F9a): o forja novo sem flag contra o base com --checagem sim (release)")
    Parser.add_argument("--checagem-nao", action="store_true", help="com --contra (F9a): o forja novo com --checagem nao contra o base sem flag (release; o padrao do base era nao)")
    Parser.add_argument("--contra", default=None, help="forja BASE congelado (caminho absoluto): exige exe igual byte a byte")
    Parser.add_argument("--bisseccao", default=None, metavar="ARQUIVO.FJ", help="acha o menor conjunto de passes culpado por uma divergencia ligado x --sem-otim tudo")
    Parser.add_argument("--checagem", default=None, choices=("sim", "nao"), help="com --bisseccao: --checagem do release")
    Parser.add_argument("--determinismo", action="store_true", help="2 builds release (checagem sim e nao) de cada alvo: bytes iguais")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    SemPasseNoBase[0] = Args.sem_passe_base
    if Args.selftest:
        return 0 if Selftest() else 1
    return Principal(Args)


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except ErroOtim as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        sys.exit(2)
