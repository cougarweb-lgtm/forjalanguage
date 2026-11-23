# -*- coding: utf-8 -*-
"""Bench de desempenho do binario (2b, P18 do M2; O0b do M7): x64 release contra gcc -O0 e -O2 congelados (0.21.0 F3, D0730).

Para cada programa de tests\\nativo\\bench\\*.fj:
  1. compila com 'forja build --backend x64 --release' (x64), cronometra a compilacao (t_build, em ms) e le o tamanho
     do exe (bytes_exe); o O0 e o O2 nao se compilam aqui: sao os binarios gcc congelados (BinarioDeReferencia do
     rapido\\oraculo_congelado.py, banco selado rapido\\oraculo_congelado_ref.json), com t_build null;
  2. roda os tres uma vez e exige stdout e codigo de saida identicos entre si e iguais ao <nome>.esperado.txt;
  3. mede a mediana de 5 execucoes (1 aquecimento) de cada exe, sempre em serie; razao = tempo_x64 / tempo_O0 (e / tempo_O2).
A media geometrica das razoes e a medida do portao (meta <= 1,3 contra o -O0; contra o -O2 so se informa).
A CLI soma as estaticas (instrucoes_estaticas e spills) pela ferramenta irma diagnostico_otim.py, grava uma linha por
bench mais a *RESUMO* da rodada em benchmark\\resultados\\otim_historico.jsonl e alarma regressao acima de 5% (x64 por bench
e geomedia) contra a rodada anterior com a mesma checagem.
A pasta bench\\ nao entra no verificar_nativo (so as pastas de ListarPastas entram): a conferencia daqui e propria.

Programas (15): aabb_raio, fib_rec, fnv_hash, formato_texto, getter_vec3, lacos_estouro, lista_append,
mapa_insere_acha, matmul, matrix4_f32, nbody_f64, ordenacao, particulas_soa, transformada_aos_soa, vec4_local.

Uso como modulo (medir_nativo.py; sem CLI):
    from medir_bench import MedirTodos, ImprimirTabela
Uso como CLI:
    python medir_bench.py [--forja CAMINHO] [--bench NOME ...] [--checagem sim|nao] [--sem-historico]
                          [--historico CAMINHO] [--pasta PASTA] [--selftest]
    python medir_bench.py --contra <base.exe> [--forja CAMINHO] [--rodadas N>=5] [--sem-ir] [--bench NOME ...]
                          (M7b F0b) ABBA: intercala as rodadas A (--forja) e B (--contra); 1 linha JSON por execucao
                          no historico (tipo "contra"), com geomedias, tick, x64 por bench e, se disponivel, Ir (medir_linux --ir).
                          Ordem: rodada impar A,B; par B,A. Razao A/B = geomedia por bench das medianas; A==B deve dar 1,00 +-3%.
                          (1.3.0) Antes do ABBA, cada bench compila o EXE x64 com A e B: SHA256 igual = idêntico (razao 1,0,
                          sem cronometro, linha com identico true fora do alarme, entra na geomedia); so os diferentes vao ao ABBA.
    Sai com 0 sem alarme, 1 com ALARME, 2 em erro de uso ou de ambiente.
Depende de: medir_linux.py (--contra, so para o Ir: WSL alpine-build com perf e valgrind); sem o WSL o Ir fica null.

Versao: 1.4.0
Changelog: 1.4.0 - 0.21.0 F3: O0 e O2 viram o binario gcc congelado (D0730, BinarioDeReferencia do rapido\\oraculo_congelado.py 1.2.0);
           ErroOraculo vira ErroBench (o bench falha, nunca pula); nada de forja --modo c, forja --backend c nem gcc na medicao;
           t_build de O0 e O2 null; sem o flag --checagem no O2 (o binario congelado nao depende dele); selftest da referencia ausente.
           O FlagsGccO0 e o PastaRuntime ficam: o capturar_ref_gcc.py ainda os importa. As opcoes de linha de comando nao mudam.
           1.3.0 - --contra: bench com EXE x64 identico em A e B (SHA256 igual) nao e cronometrado; razao 1,0 marcada identico
           (linha tipo "contra" com identico true, fora do alarme) e entra na geomedia; so os diferentes vao ao ABBA;
           selftest IdenticosSemCronometro e IdenticoNaGeomedia. As opcoes antigas nao mudam.
           1.2.0 - M7b F0b: --contra (ABBA de N>=5 rodadas A/B, razao A/B, linha tipo "contra" no historico, Ir por medir_linux quando ha WSL, --sem-ir);
           selftest ABBA, razao, codegen sem formato_texto e contra fora do alarme. As opcoes antigas nao mudam.
           1.1.1 - M7a F0: ExecutarFerramentaOtim/LerEstaticas repassam --checagem da rodada ao diagnostico_otim 1.1.0 (as estaticas sim/nao saiam iguais); selftest RepasseChecagemEstaticas.
           1.1.0 - O0b do M7: historico JSONL, geomedia, tick do jogo, alarme de regressao 5%, CLI, --selftest
           e opcao Checagem preparada para o P4 (o forja ainda nao tem --checagem: padrao None).
           1.0.0 - P18 do M2: 8 programas de bench, 3 backends, mediana de 5, media geometrica e linha G6 serial.
Verificacao: python -I medir_bench.py --selftest
"""

import argparse
import contextlib
import hashlib
import inspect
import io
import json
import math
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from datetime import datetime
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[1]
PastaBench = PastaAqui / "bench"
PastaRuntime = PastaLanguage / "runtime"
PastaRapido = PastaAqui / "rapido"
sys.path.insert(0, str(PastaRapido))
import oraculo_congelado as Oraculo  # noqa: E402  (modulo irmao em rapido\, que nao importa o medir_bench)
AquecimentosBench = 1
RepeticoesBench = 5
FlagsGccO0 = ["-std=c11", "-O0", "-D__USE_MINGW_ANSI_STDIO=1", "-static", "-Wno-type-limits"]
BenchsSerial = ("particulas_soa", "nbody_f64")
Rotulos = ("x64", "O0", "O2")
VERSAO = "1.4.0"
RODADAS_CONTRA_PADRAO = 5
BENCH_FORA_CODEGEN = "formato_texto"  # runtime Ryu: fica de fora da geomedia codegen (igual ao medir_linux)
PASTA_LINUX = PastaLanguage / "tests" / "nativo" / "linux"
FORJA_PADRAO = "C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe"
HISTORICO_PADRAO = PastaLanguage / "benchmark" / "resultados" / "otim_historico.jsonl"
FERRAMENTA_OTIM = Path("C:/CLAUDE_AGI/tools/forja_otim/diagnostico_otim.py")
TIMEOUT_ESTATICA = 300
LIMITE_ALARME = 1.05
BENCHS_TICK = ("particulas_soa", "transformada_aos_soa", "aabb_raio")
NOME_RESUMO = "*RESUMO*"
VALORES_CHECAGEM = ("sim", "nao")


class ErroBench(Exception):
    """Falha de compilacao, execucao ou diferenca de saida de um bench; a mensagem diz qual e onde."""


def DividirValor(Dividendo, Divisor):
    # Divisor vem de medicao: zero vira 0.0 em vez de ZeroDivisionError (CROSS-R02).
    if Divisor == 0:
        return 0.0
    return Dividendo / Divisor


def CronometrarMs(Acao):
    Inicio = time.perf_counter()
    Acao()
    return (time.perf_counter() - Inicio) * 1000.0


def CronometrarChamada(Acao):
    # Devolve (resultado, ms): as compilacoes precisam do caminho do exe e do tempo ao mesmo tempo.
    Inicio = time.perf_counter()
    Valor = Acao()
    return Valor, (time.perf_counter() - Inicio) * 1000.0


def MedirMediana(Acao, NumAquecimentos, NumRepeticoes):
    for _ in range(NumAquecimentos):
        Acao()
    return statistics.median([CronometrarMs(Acao) for _ in range(NumRepeticoes)])


def Executar(Argumentos, Cwd):
    return subprocess.run([str(A) for A in Argumentos], capture_output=True, cwd=str(Cwd))


def ListarBenchs(Nomes=None):
    Todos = sorted(PastaBench.glob("*.fj"))
    if not Nomes:
        return Todos
    Conhecidos = {F.stem for F in Todos}
    Faltando = [N for N in Nomes if N not in Conhecidos]
    if Faltando:
        raise ErroBench(f"bench(s) inexistente(s) em {PastaBench}: {', '.join(Faltando)}")
    return [F for F in Todos if F.stem in Nomes]


def ExigirSucesso(Resultado, Fonte, Etapa):
    if Resultado.returncode != 0:
        Causa = Resultado.stderr.decode("utf-8", errors="replace").strip()[:300] or Resultado.stdout.decode("utf-8", errors="replace")[:300]
        raise ErroBench(f"{Fonte.stem}: {Etapa} falhou (exit {Resultado.returncode}): {Causa}")


def ArgumentosChecagem(Checagem):
    # --checagem so existe no forja a partir do P4; None deixa o comando igual ao de antes (padrao).
    if Checagem is None:
        return []
    if Checagem not in VALORES_CHECAGEM:
        raise ErroBench(f"checagem invalida: {Checagem!r} (use sim ou nao)")
    return ["--checagem", Checagem]


def CompilarX64(Forja, Fonte, Pasta, *, Checagem=None):
    Saida = Pasta / (Fonte.stem + "_x64.exe")
    Resultado = Executar([Forja, "build", "--backend", "x64", "--release", Fonte, "-o", Saida] + ArgumentosChecagem(Checagem), Pasta)
    ExigirSucesso(Resultado, Fonte, "forja build --backend x64 --release")
    return Saida


def BinarioCongelado(Fonte, Nivel):
    # Referencia gcc congelada (D0730): so o binario do banco selado; chave, arquivo ou sha ruim vira ErroBench, nunca pula nem recompila.
    try:
        return Oraculo.BinarioDeReferencia(Fonte, "win", Nivel)
    except Oraculo.ErroOraculo as Erro:
        raise ErroBench(f"{Fonte.stem}: {Erro}") from Erro


def CompilarO0(Forja, Fonte, Pasta):
    # Assinatura igual a das Compilar*: Forja e Pasta nao entram (o binario e o congelado); o capturar_ref_gcc ainda chama com 3 argumentos.
    return BinarioCongelado(Fonte, "O0")


def CompilarO2(Forja, Fonte, Pasta):
    # Igual ao CompilarO0: o -O2 do backend C tambem e o binario congelado, sem flag de checagem.
    return BinarioCongelado(Fonte, "O2")


def RodarUma(Exe, Pasta):
    Resultado = Executar([Exe], Pasta)
    return Resultado.stdout, Resultado.returncode


def ConferirSaidas(Exes, Fonte, Pasta):
    """Os tres exes dao stdout e codigo de saida identicos; o stdout (CRLF -> LF) tem de bater com o <nome>.esperado.txt."""
    Saidas = {Rotulo: RodarUma(Exes[Rotulo], Pasta) for Rotulo in Rotulos}
    for Rotulo in ("O0", "O2"):
        if Saidas[Rotulo] != Saidas["x64"]:
            raise ErroBench(f"{Fonte.stem}: DIFERENTE entre x64 e {Rotulo}: x64={Saidas['x64']!r} {Rotulo}={Saidas[Rotulo]!r}")
    Esperado = Fonte.with_suffix(".esperado.txt").read_bytes()
    if Saidas["x64"][0].replace(b"\r\n", b"\n") != Esperado.replace(b"\r\n", b"\n") or Saidas["x64"][1] != 0:
        raise ErroBench(f"{Fonte.stem}: saida diferente do .esperado.txt: {Saidas['x64']!r}")


def MedirBench(Forja, Fonte, Pasta, *, Checagem=None):
    """Compila o x64 (cronometrando a compilacao), pega os binarios gcc congelados O0 e O2, confere e mede um bench.

    Devolve {nome, x64, O0, O2} em ms (execucao, mediana de 5), t_build {x64} em ms (O0 e O2 ficam None: nao sao compilados aqui)
    e bytes {x64, O0, O2} com o tamanho de cada exe.
    """
    Exes, TBuild = {}, {}
    Exes["x64"], TBuild["x64"] = CronometrarChamada(lambda: CompilarX64(Forja, Fonte, Pasta, Checagem=Checagem))
    Exes["O0"] = CompilarO0(Forja, Fonte, Pasta)
    Exes["O2"] = CompilarO2(Forja, Fonte, Pasta)
    TBuild["O0"] = TBuild["O2"] = None
    ConferirSaidas(Exes, Fonte, Pasta)
    Tempos = {Rotulo: MedirMediana(lambda Exe=Exes[Rotulo]: Executar([Exe], Pasta), AquecimentosBench, RepeticoesBench) for Rotulo in Rotulos}
    Bytes = {Rotulo: Exes[Rotulo].stat().st_size for Rotulo in Rotulos}
    return {"nome": Fonte.stem, **Tempos, "t_build": TBuild, "bytes": Bytes}


def MedirTodos(Forja, Pasta, *, Checagem=None, Nomes=None, Progresso=None):
    """Um resultado por bench: o dict de MedirBench ou {nome, erro} quando algo falhou (os demais seguem).

    Nomes filtra os benches (None = todos); Progresso, se dado, recebe cada resultado logo apos a medida.
    """
    Resultados = []
    for Fonte in ListarBenchs(Nomes):
        try:
            Resultado = MedirBench(Forja, Fonte, Pasta, Checagem=Checagem)
        except ErroBench as Erro:
            Resultado = {"nome": Fonte.stem, "erro": str(Erro)}
        Resultados.append(Resultado)
        if Progresso is not None:
            Progresso(Resultado)
    return Resultados


def MediaGeometrica(Razoes):
    # Sem razoes ou com razao <= 0 (divisao por tempo zero) a media nao existe: falha explicita, nunca 0.0 (portao vazio).
    if not Razoes or any(R <= 0 for R in Razoes):
        raise ErroBench(f"media geometrica sem base valida (razoes={Razoes})")
    return math.exp(sum(math.log(R) for R in Razoes) / len(Razoes))


def RazoesContra(Resultados, Rotulo):
    return [DividirValor(R["x64"], R[Rotulo]) for R in Resultados if "erro" not in R]


def ImprimirTabela(Resultados):
    print(f"(2b) {'bench':<18} {'x64 ms':>9} {'gcc-O0 ms':>10} {'C -O2 ms':>9} {'x64/O0':>7} {'x64/O2':>7}")
    for R in Resultados:
        if "erro" in R:
            print(f"(2b) {R['nome']:<18} FALHOU: {R['erro']}")
            continue
        print(f"(2b) {R['nome']:<18} {R['x64']:>9.1f} {R['O0']:>10.1f} {R['O2']:>9.1f} {DividirValor(R['x64'], R['O0']):>7.2f} {DividirValor(R['x64'], R['O2']):>7.2f}")


def LinhaSerial(Resultados):
    Tempos = {R["nome"]: R["x64"] for R in Resultados if "erro" not in R}
    Partes = [f"{Nome}={Tempos[Nome]:.0f}" if Nome in Tempos else f"{Nome}=n/d" for Nome in BenchsSerial]
    return "G6 serial: " + " ".join(Partes)


def MediaOuNone(Razoes):
    # Sem razao nenhuma (todos os benches falharam) nao ha geomedia: null na historia, nunca 0.0.
    if not Razoes:
        return None
    return MediaGeometrica(Razoes)


def CalcularTick(Resultados):
    # Tick do jogo: soma dos tempos x64 sobre a soma dos -O2 nos 3 benches de jogo; se um faltar, null.
    Ok = {R["nome"]: R for R in Resultados if "erro" not in R}
    if any(Nome not in Ok for Nome in BENCHS_TICK):
        return None
    return DividirValor(sum(Ok[N]["x64"] for N in BENCHS_TICK), sum(Ok[N]["O2"] for N in BENCHS_TICK))


def VersaoForja(Forja, Pasta):
    Resultado = Executar([Forja, "--versao"], Pasta)
    if Resultado.returncode != 0 or not Resultado.stdout.strip():
        raise ErroBench(f"forja --versao falhou (exit {Resultado.returncode}) em {Forja}")
    return Resultado.stdout.decode("utf-8", errors="replace").strip()


def ShaCurtoForja(Forja):
    # 12 hex bastam para o historico distinguir as builds do forja; o sha inteiro fica no proprio exe.
    return hashlib.sha256(Path(Forja).read_bytes()).hexdigest()[:12]


def ShaDoArquivo(Caminho):
    # SHA256 completo: a comparacao de EXEs da bancada tem de ser exata, sem o corte de 12 hex do historico.
    return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()


def MontarMetadados(Forja, Checagem, *, Pasta, Agora):
    # Campos comuns a todas as linhas da rodada; checagem None vira "n/d" no historico.
    return {
        "data": Agora.isoformat(timespec="seconds"),
        "rodada": Agora.strftime("%Y%m%d-%H%M%S"),
        "versao_forja": VersaoForja(Forja, Pasta),
        "sha_forja": ShaCurtoForja(Forja),
        "checagem": "n/d" if Checagem is None else Checagem,
    }


def MontarLinhaBench(Resultado, Estatica, Metadados):
    # Bench que falhou sai com ok=false, o erro e os medidos em null; as estaticas valem mesmo assim.
    Base = {**Metadados, "bench": Resultado["nome"]}
    Estado = {"instrucoes_estaticas": Estatica["instrucoes_estaticas"], "spills": Estatica["spills"]}
    if "erro" in Resultado:
        Nulos = {"t_build_ms": None, "bytes_exe": None, "t_exec_ms": None, "razao_x64_o2": None, "razao_x64_o0": None}
        return {**Base, **Nulos, **Estado, "ok": False, "erro": Resultado["erro"]}
    Tempos = {Rotulo: Resultado[Rotulo] for Rotulo in Rotulos}
    Medidos = {
        "t_build_ms": Resultado["t_build"],
        "bytes_exe": Resultado["bytes"],
        "t_exec_ms": Tempos,
        "razao_x64_o2": DividirValor(Tempos["x64"], Tempos["O2"]),
        "razao_x64_o0": DividirValor(Tempos["x64"], Tempos["O0"]),
    }
    return {**Base, **Medidos, **Estado, "ok": True, "erro": None}


def MontarLinhaResumo(Resultados, Metadados):
    return {
        **Metadados,
        "bench": NOME_RESUMO,
        "geomedia_x64_o2": MediaOuNone(RazoesContra(Resultados, "O2")),
        "geomedia_x64_o0": MediaOuNone(RazoesContra(Resultados, "O0")),
        "tick_jogo_razao_o2": CalcularTick(Resultados),
    }


def MontarLinhasRodada(Resultados, Estaticas, Metadados):
    # Uma linha por bench (ok ou falha) e, por ultimo, a *RESUMO* da rodada; todas com os mesmos metadados.
    Linhas = [MontarLinhaBench(R, Estaticas[R["nome"]], Metadados) for R in Resultados]
    Linhas.append(MontarLinhaResumo(Resultados, Metadados))
    return Linhas


def ResumoDe(Linhas):
    for Linha in Linhas:
        if Linha["bench"] == NOME_RESUMO:
            return Linha
    raise ErroBench("rodada sem linha *RESUMO* (historico ou montagem incompleta)")


def SomarEstaticas(Dados, Nome):
    # Formato do diagnostico_otim.py 1.0.1: benches[].funcoes[] com instrucoes_forja e refs_pilha_forja.
    Benchs = [B for B in Dados["benches"] if B["nome"] == Nome]
    if len(Benchs) != 1:
        raise ErroBench(f"{Nome}: o JSON do diagnostico tem {len(Benchs)} bloco(s) para o bench (esperado 1)")
    Funcoes = Benchs[0]["funcoes"]
    return {
        "instrucoes_estaticas": sum(F["instrucoes_forja"] for F in Funcoes),
        "spills": sum(F["refs_pilha_forja"] for F in Funcoes),
    }


def ArgumentosFerramentaOtim(Nome, Forja, Saida, Checagem=None):
    # Checagem da rodada repassada ao diagnostico_otim (1.1.0+); sem ela as estaticas sim/nao saiam iguais.
    return [sys.executable, str(FERRAMENTA_OTIM), "--bench", Nome, "--formato", "json", "--saida", str(Saida), "--forja", str(Forja)] + ArgumentosChecagem(Checagem)


def ExecutarFerramentaOtim(Nome, Forja, Saida, Checagem=None):
    return subprocess.run(ArgumentosFerramentaOtim(Nome, Forja, Saida, Checagem), capture_output=True, timeout=TIMEOUT_ESTATICA)


def LerEstaticas(Nome, Forja, Pasta, Checagem=None):
    """instrucoes_estaticas e spills do bench pela ferramenta irma; qualquer falha vira null e aviso, nunca quebra a medida."""
    Nulo = {"instrucoes_estaticas": None, "spills": None}
    if not FERRAMENTA_OTIM.is_file():
        print(f"aviso: ferramenta {FERRAMENTA_OTIM} nao existe; estaticas de {Nome} ficam null")
        return Nulo
    Saida = Pasta / "estaticas" / Nome
    Saida.mkdir(parents=True, exist_ok=True)
    try:
        Resultado = ExecutarFerramentaOtim(Nome, Forja, Saida, Checagem)
        if Resultado.returncode != 0:
            Causa = Resultado.stderr.decode("utf-8", errors="replace").strip()[-300:]
            raise ErroBench(f"diagnostico_otim falhou (exit {Resultado.returncode}): {Causa}")
        Dados = json.loads((Saida / "diagnostico_otim.json").read_text(encoding="utf-8"))
        return SomarEstaticas(Dados, Nome)
    except (OSError, subprocess.TimeoutExpired, ErroBench, KeyError, TypeError, ValueError) as Erro:
        print(f"aviso: estaticas de {Nome} ficam null: {Erro}")
        return Nulo


def LerHistorico(Caminho):
    # Uma linha JSON por registro; linha corrompida falha com o numero dela (nao e pulada em silencio).
    if not Caminho.is_file():
        return []
    Linhas = []
    for Numero, Texto in enumerate(Caminho.read_text(encoding="utf-8").splitlines(), start=1):
        if not Texto.strip():
            continue
        try:
            Linhas.append(json.loads(Texto))
        except ValueError as Erro:
            raise ErroBench(f"historico {Caminho} linha {Numero} nao e JSON valido: {Erro}") from Erro
    return Linhas


def GravarHistorico(Caminho, Linhas):
    # So anexa: o historico e a serie de medidas, nunca se reescreve.
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    with open(Caminho, "a", encoding="utf-8", newline="\n") as Arquivo:
        for Linha in Linhas:
            Arquivo.write(json.dumps(Linha, ensure_ascii=False) + "\n")


def AcharRodadaAnterior(Historico, Checagem):
    # Ultima rodada gravada com a mesma checagem (linhas de outra checagem nao contam); lista vazia = linha de base.
    Anterior = []
    for Linha in Historico:
        if Linha.get("checagem") != Checagem:
            continue
        if Anterior and Anterior[0]["rodada"] != Linha["rodada"]:
            Anterior = []
        Anterior.append(Linha)
    return Anterior


def AlarmeBench(Linha, Antes):
    # So compara duas medidas ok; uma base x64 <= 0 nao existe e falha explicita, nunca vira alarme silencioso.
    if Antes is None or not (Linha["ok"] and Antes["ok"]):
        return None
    Agora = Linha["t_exec_ms"]["x64"]
    Base = Antes["t_exec_ms"]["x64"]
    if Base <= 0:
        raise ErroBench(f"{Linha['bench']}: t_exec_ms x64 da rodada {Antes['rodada']} invalido ({Base})")
    if Agora <= LIMITE_ALARME * Base:
        return None
    return f"ALARME {Linha['bench']}: x64 {Base:.1f} ms -> {Agora:.1f} ms (+{(DividirValor(Agora, Base) - 1.0) * 100.0:.1f}%)"


def AlarmeGeomedia(ResumoAtual, ResumoAnterior):
    Agora = ResumoAtual["geomedia_x64_o2"]
    Base = ResumoAnterior["geomedia_x64_o2"]
    if Agora is None or Base is None or Agora <= LIMITE_ALARME * Base:
        return None
    return f"ALARME geomedia: x64/O2 {Base:.3f} -> {Agora:.3f} (+{(DividirValor(Agora, Base) - 1.0) * 100.0:.1f}%)"


def BenchsOk(Linhas):
    return {L["bench"] for L in Linhas if L["bench"] != NOME_RESUMO and L["ok"]}


def CompararRodadas(Atuais, Anterior):
    # Alarmes da rodada contra a anterior (mesma checagem). Sem anterior e linha de base: nenhum alarme.
    if not Anterior:
        return []
    PorBench = {L["bench"]: L for L in Anterior}
    Alarmes = [AlarmeBench(L, PorBench.get(L["bench"])) for L in Atuais if L["bench"] != NOME_RESUMO]
    Alarmes = [A for A in Alarmes if A is not None]
    # Geomedia de subconjunto de benches nao se compara com a de todos: so compara com o mesmo conjunto ok.
    if BenchsOk(Atuais) == BenchsOk(Anterior):
        Geo = AlarmeGeomedia(ResumoDe(Atuais), ResumoDe(Anterior))
        if Geo is not None:
            Alarmes.append(Geo)
    return Alarmes


def FormatarOpcional(Valor, Casas):
    return "n/d" if Valor is None else f"{Valor:.{Casas}f}"


def ImprimirResumo(Resumo):
    print(f"geomedia x64/O2 = {FormatarOpcional(Resumo['geomedia_x64_o2'], 3)} | x64/O0 = {FormatarOpcional(Resumo['geomedia_x64_o0'], 3)}")
    print(f"tick do jogo: razao x64/O2 = {FormatarOpcional(Resumo['tick_jogo_razao_o2'], 3)}")


def ImprimirAlarmes(Alarmes, Anterior):
    if not Anterior:
        print("linha de base: sem rodada anterior com a mesma checagem, nada para comparar")
        return
    print(f"comparado com a rodada {Anterior[0]['rodada']} (sha {Anterior[0]['sha_forja']})")
    if not Alarmes:
        print(f"sem alarme: nenhuma regressao acima de {(LIMITE_ALARME - 1.0) * 100:.0f}%")
    for Alarme in Alarmes:
        print(Alarme)


def MostrarProgresso(Resultado):
    Situacao = f"FALHOU: {Resultado['erro']}" if "erro" in Resultado else "ok"
    print(f"(2b) concluido {Resultado['nome']}: {Situacao}", flush=True)


def GravarOuAvisar(Args, Linhas):
    if Args.sem_historico:
        print("historico: nao gravado (--sem-historico)")
        return
    GravarHistorico(Path(Args.historico), Linhas)
    print(f"historico: {len(Linhas)} linha(s) gravadas em {Args.historico}")


def OrdemRodada(Indice):
    # ABBA: a rodada impar mede A primeiro e a par mede B primeiro, para o drift da maquina nao cair so num lado.
    return ("A", "B") if Indice % 2 == 0 else ("B", "A")


def RazoesCodegenBench(Resultados, Rotulo):
    # Geomedia codegen: sem o formato_texto (runtime Ryu), como no medir_linux.
    return [DividirValor(R["x64"], R[Rotulo]) for R in Resultados if "erro" not in R and R["nome"] != BENCH_FORA_CODEGEN]


def MontarLinhaContra(Rotulo, Indice, Resultados, Identidade, Agora):
    # Uma execucao da forja (A ou B) numa rodada do ABBA. Sem 'checagem' e sem 'bench': o historico normal e o alarme nao a enxergam.
    Ir = Identidade.get("ir") or {}
    return {
        "tipo": "contra",
        "data": Agora.isoformat(timespec="seconds"),
        "rodada": Agora.strftime("%Y%m%d-%H%M%S"),
        "ordem": Indice,
        "rotulo": Rotulo,
        "forja": Identidade["forja"],
        "versao_forja": Identidade["versao_forja"],
        "sha_forja": Identidade["sha_forja"],
        "versao_medir_bench": VERSAO,
        "geomedia_x64_o2": MediaOuNone(RazoesContra(Resultados, "O2")),
        "geomedia_x64_o2_codegen": MediaOuNone(RazoesCodegenBench(Resultados, "O2")),
        "tick_jogo_razao_o2": CalcularTick(Resultados),
        "x64_ms": {R["nome"]: R["x64"] for R in Resultados if "erro" not in R},
        "O2_ms": {R["nome"]: R["O2"] for R in Resultados if "erro" not in R},
        "benchs_falhos": sorted(R["nome"] for R in Resultados if "erro" in R),
        "ir_geomedia_total": Ir.get("geomedia_total"),
        "ir_geomedia_codegen": Ir.get("geomedia_codegen"),
    }


def MedianasPorBench(Linhas, Rotulo):
    # Mediana das rodadas de cada bench (x64 ms) para a forja Rotulo.
    Por = {}
    for Linha in Linhas:
        if Linha["rotulo"] == Rotulo:
            for Nome, Ms in Linha["x64_ms"].items():
                Por.setdefault(Nome, []).append(Ms)
    return {Nome: statistics.median(Valores) for Nome, Valores in Por.items()}


def MontarLinhaIdentico(Nome, Sha, Identidades, Agora):
    # Bench com o mesmo EXE x64 em A e B: razao 1,0, sem cronometro. Sem 'checagem' e sem 'bench' (o nome vai em 'bench_identico'), para o alarme nao a tomar por rodada.
    return {
        "tipo": "contra",
        "identico": True,
        "bench_identico": Nome,
        "razao_ab": 1.0,
        "sha_exe_x64": Sha,
        "data": Agora.isoformat(timespec="seconds"),
        "rodada": Agora.strftime("%Y%m%d-%H%M%S"),
        "versao_medir_bench": VERSAO,
        "sha_forja_a": Identidades["A"]["sha_forja"],
        "sha_forja_b": Identidades["B"]["sha_forja"],
    }


def RazaoContra(Linhas, Identicos):
    # Razao A/B por bench (mediana de A sobre mediana de B); bench identico entra com razao 1,0 sem tempo; geomedia total e codegen (sem formato_texto).
    A, B = MedianasPorBench(Linhas, "A"), MedianasPorBench(Linhas, "B")
    Razoes = {Nome: DividirValor(A[Nome], B[Nome]) for Nome in sorted(A) if Nome in B}
    Razoes.update({Nome: 1.0 for Nome in Identicos})
    Codegen = [R for Nome, R in Razoes.items() if Nome != BENCH_FORA_CODEGEN]
    return {"total": MediaOuNone(list(Razoes.values())), "codegen": MediaOuNone(Codegen), "por_bench": Razoes}


def ImportarMedirLinux():
    # Import tardio: so o --contra com Ir precisa do medir_linux (e do WSL); sem ele o medir_bench segue igual.
    if str(PASTA_LINUX) not in sys.path:
        sys.path.insert(0, str(PASTA_LINUX))
    import medir_linux
    return medir_linux


def IrDaForja(Forja, Sha, Pasta, Cache, SemIr):
    # Ir uma vez por sha (A==B mede uma vez so). Sem WSL ou com erro: vazio e aviso; a medida de tempo segue.
    if SemIr:
        return {}
    if Sha not in Cache:
        try:
            Cache[Sha] = ImportarMedirLinux().RodarIrComTrava(Path(Forja), Pasta / "ir")
        except (RuntimeError, OSError, subprocess.TimeoutExpired) as Erro:
            print(f"aviso: Ir de {Forja} (sha {Sha}) nao medido: {Erro}")
            Cache[Sha] = {}
    return Cache[Sha]


def ShaExeX64Comum(Fonte, ForjaA, ForjaB, Pasta):
    # SHA256 do EXE x64 quando A e B geram o mesmo arquivo; None se diferem. Falha de compilacao vira None: o ABBA a reporta de novo.
    Shas = []
    try:
        for Rotulo, Forja in (("A", ForjaA), ("B", ForjaB)):
            PastaRotulo = Pasta / f"identidade_{Rotulo}"
            PastaRotulo.mkdir(parents=True, exist_ok=True)
            Shas.append(ShaDoArquivo(CompilarX64(Forja, Fonte, PastaRotulo)))
    except ErroBench as Erro:
        print(f"aviso: {Fonte.stem} sem comparacao por hash ({Erro}); entra no cronometro")
        return None
    return Shas[0] if Shas[0] == Shas[1] else None


def ClassificarBenchs(Fontes, ForjaA, ForjaB, Pasta):
    """Separa os benches: Identicos {nome: sha do EXE x64} (nao cronometrados) e Diferentes [nome] (vao ao ABBA)."""
    Identicos, Diferentes = {}, []
    for Fonte in Fontes:
        Sha = ShaExeX64Comum(Fonte, ForjaA, ForjaB, Pasta)
        if Sha is None:
            Diferentes.append(Fonte.stem)
        else:
            Identicos[Fonte.stem] = Sha
    return Identicos, Diferentes


def RodarAbba(Args, Pasta, Nomes, Identidades):
    # So os benches com EXE diferente chegam aqui; Nomes vazio nao pode ir ao MedirTodos (ListarBenchs(vazio) mediria todos).
    Forjas = {"A": Args.forja, "B": Args.contra}
    Linhas = []
    for Indice in range(Args.rodadas):
        for Rotulo in OrdemRodada(Indice):
            Resultados = MedirTodos(Forjas[Rotulo], Pasta, Nomes=Nomes)
            Linha = MontarLinhaContra(Rotulo, Indice, Resultados, Identidades[Rotulo], datetime.now())
            Linhas.append(Linha)
            print(f"(ABBA) rodada {Indice + 1}/{Args.rodadas} forja {Rotulo}: geomedia x64/O2 = {FormatarOpcional(Linha['geomedia_x64_o2'], 3)} | "
                  f"codegen = {FormatarOpcional(Linha['geomedia_x64_o2_codegen'], 3)} | falhos = {len(Linha['benchs_falhos'])}")
    return Linhas


def RodarContra(Args, Pasta):
    if not Path(Args.contra).is_absolute() or not Path(Args.contra).is_file():
        raise ErroBench(f"--contra precisa ser o caminho ABSOLUTO de um forja.exe existente: {Args.contra}")
    if Args.rodadas < 5:
        raise ErroBench(f"--rodadas {Args.rodadas}: o ABBA pede no minimo 5 rodadas")
    Forjas = {"A": Args.forja, "B": Args.contra}
    Cache = {}
    Identidades = {}
    for Rotulo, Forja in Forjas.items():
        Sha = ShaCurtoForja(Forja)
        Identidades[Rotulo] = {"forja": str(Forja), "versao_forja": VersaoForja(Forja, Pasta), "sha_forja": Sha,
                               "ir": IrDaForja(Forja, Sha, Pasta, Cache, Args.sem_ir)}
        print(f"(ABBA) forja {Rotulo}: {Identidades[Rotulo]['versao_forja']} | sha {Sha} | {Forja}")
    Fontes = ListarBenchs(Args.bench)
    Identicos, Diferentes = ClassificarBenchs(Fontes, Args.forja, Args.contra, Pasta)
    IdenticoLinhas = [MontarLinhaIdentico(Nome, Sha, Identidades, datetime.now()) for Nome, Sha in Identicos.items()]
    AbbaLinhas = RodarAbba(Args, Pasta, Diferentes, Identidades) if Diferentes else []
    if not Diferentes:
        print("(ABBA) todos os EXEs x64 sao identicos: nada a cronometrar")
    Razao = RazaoContra(AbbaLinhas, Identicos)
    print(f"contra A/B ({len(Diferentes)} cronometrado(s), {Args.rodadas} rodadas): razao geomedia total = {FormatarOpcional(Razao['total'], 3)} | "
          f"codegen = {FormatarOpcional(Razao['codegen'], 3)} (A==B deve dar 1,00 +-3%)")
    for Nome, RazaoBench in sorted(Razao["por_bench"].items()):
        Marca = " idêntico" if Nome in Identicos else ""
        print(f"(ABBA) {Nome:<18} A/B {RazaoBench:.3f}{Marca}")
    GravarOuAvisar(Args, IdenticoLinhas + AbbaLinhas)
    print(f"{len(Identicos)} de {len(Fontes)} benches idênticos (não cronometrados)")
    return 0


def RodarRodada(Args, Pasta):
    Agora = datetime.now()
    Metadados = MontarMetadados(Args.forja, Args.checagem, Pasta=Pasta, Agora=Agora)
    print(f"medir_bench {VERSAO} | forja {Metadados['versao_forja']} | sha {Metadados['sha_forja']} | checagem {Metadados['checagem']} | rodada {Metadados['rodada']}")
    Nomes = [F.stem for F in ListarBenchs(Args.bench)]
    Estaticas = {N: LerEstaticas(N, Args.forja, Pasta, Args.checagem) for N in Nomes}
    Resultados = MedirTodos(Args.forja, Pasta, Checagem=Args.checagem, Nomes=Args.bench, Progresso=MostrarProgresso)
    ImprimirTabela(Resultados)
    Linhas = MontarLinhasRodada(Resultados, Estaticas, Metadados)
    ImprimirResumo(ResumoDe(Linhas))
    Anterior = AcharRodadaAnterior(LerHistorico(Path(Args.historico)), Metadados["checagem"])
    Alarmes = CompararRodadas(Linhas, Anterior)
    ImprimirAlarmes(Alarmes, Anterior)
    GravarOuAvisar(Args, Linhas)
    return 1 if Alarmes else 0


def ConstruirParser():
    Parser = argparse.ArgumentParser(description="Bench de desempenho: x64 release contra os binarios gcc -O0 e -O2 congelados, com historico e alarme de regressao.")
    Parser.add_argument("--forja", default=FORJA_PADRAO, metavar="CAMINHO", help="forja.exe a medir (caminho absoluto)")
    Parser.add_argument("--bench", action="append", metavar="NOME", help="bench a medir (sem .fj); pode repetir")
    Parser.add_argument("--checagem", choices=VALORES_CHECAGEM, default=None, help="opcao --checagem do forja (so a partir do P4)")
    Parser.add_argument("--sem-historico", action="store_true", help="nao grava o historico (o alarme ainda le o existente)")
    Parser.add_argument("--historico", default=str(HISTORICO_PADRAO), metavar="CAMINHO", help="arquivo JSONL do historico")
    Parser.add_argument("--pasta", metavar="PASTA", help="pasta de trabalho (padrao: subpasta nova da pasta temporaria, apagada ao fim)")
    Parser.add_argument("--contra", metavar="BASE_EXE", help="forja B (caminho absoluto) em ABBA com --forja (A); grava linhas tipo contra")
    Parser.add_argument("--rodadas", type=int, default=RODADAS_CONTRA_PADRAO, metavar="N", help="rodadas do --contra (minimo 5)")
    Parser.add_argument("--sem-ir", action="store_true", help="--contra sem o Ir (medir_linux); sem WSL o Ir ja fica null")
    Parser.add_argument("--selftest", action="store_true", help="testa as funcoes com dados embutidos (nao roda o forja) e sai")
    return Parser


def Main(Argumentos=None):
    Args = ConstruirParser().parse_args(Argumentos)
    if Args.selftest:
        return 0 if Selftest() else 1
    if not Path(Args.forja).is_file():
        raise ErroBench(f"forja nao encontrado em {Args.forja} (use --forja CAMINHO)")
    Modo = RodarContra if Args.contra else RodarRodada
    if Args.pasta is not None:
        Pasta = Path(Args.pasta)
        Pasta.mkdir(parents=True, exist_ok=True)
        return Modo(Args, Pasta)
    Pasta = Path(tempfile.mkdtemp(prefix="medir_bench_"))
    try:
        return Modo(Args, Pasta)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def ResultadoEmbutido(Nome, *, X64, O0, O2):
    return {"nome": Nome, "x64": X64, "O0": O0, "O2": O2,
            "t_build": {"x64": 1.0, "O0": 2.0, "O2": 3.0}, "bytes": {"x64": 100, "O0": 200, "O2": 300}}


def RodadaEmbutida(Rodada, Checagem, *, AabbX64):
    # Duas medidas: particulas (razao 1) e aabb_raio (x64 variavel contra O2 = 50 ms).
    Resultados = [ResultadoEmbutido("particulas_soa", X64=10.0, O0=10.0, O2=10.0),
                  ResultadoEmbutido("aabb_raio", X64=AabbX64, O0=100.0, O2=50.0)]
    Metadados = {"data": "2026-11-16T20:00:00", "rodada": Rodada, "versao_forja": "forja x",
                 "sha_forja": "abcdef012345", "checagem": Checagem}
    Estaticas = {"particulas_soa": {"instrucoes_estaticas": None, "spills": None},
                 "aabb_raio": {"instrucoes_estaticas": None, "spills": None}}
    return MontarLinhasRodada(Resultados, Estaticas, Metadados)


def TesteMediaGeometrica():
    Afirmar(abs(MediaGeometrica([2.0, 8.0]) - 4.0) < 1e-9, "geomedia de 2 e 8 deveria dar 4")
    for Invalidas in ([], [0.0, 1.0]):
        try:
            MediaGeometrica(Invalidas)
        except ErroBench:
            continue
        raise AssertionError(f"geomedia aceitou razoes invalidas {Invalidas}")


def TesteDividirValorZero():
    Afirmar(DividirValor(5.0, 0) == 0.0, "divisor 0 deveria dar 0.0")
    Afirmar(DividirValor(6.0, 3) == 2.0, "6 / 3 deveria dar 2.0")


def TesteTick():
    Completa = [ResultadoEmbutido("particulas_soa", X64=10.0, O0=20.0, O2=20.0),
                ResultadoEmbutido("transformada_aos_soa", X64=30.0, O0=40.0, O2=40.0),
                ResultadoEmbutido("aabb_raio", X64=60.0, O0=60.0, O2=60.0)]
    Afirmar(abs(CalcularTick(Completa) - 100.0 / 120.0) < 1e-9, f"tick deveria ser 100/120, veio {CalcularTick(Completa)}")
    Afirmar(CalcularTick(Completa[:2]) is None, "tick sem o aabb_raio deveria ser None")
    Falhou = Completa[:2] + [{"nome": "aabb_raio", "erro": "falhou"}]
    Afirmar(CalcularTick(Falhou) is None, "tick com aabb_raio falho deveria ser None")


def TesteLinhasJsonl():
    Metadados = {"data": "2026-11-16T20:00:00", "rodada": "20261116-200000", "versao_forja": "forja x",
                 "sha_forja": "abcdef012345", "checagem": "n/d"}
    Resultados = [ResultadoEmbutido("aabb_raio", X64=60.0, O0=60.0, O2=50.0),
                  {"nome": "fib_rec", "erro": "gcc nao esta no PATH: função"}]
    Estaticas = {"aabb_raio": {"instrucoes_estaticas": 10, "spills": 2},
                 "fib_rec": {"instrucoes_estaticas": None, "spills": None}}
    Linhas = MontarLinhasRodada(Resultados, Estaticas, Metadados)
    Afirmar(len(Linhas) == 3, f"deveria haver 2 benches + RESUMO, veio {len(Linhas)}")
    Campos = {"data", "rodada", "versao_forja", "sha_forja", "checagem", "bench", "t_build_ms", "bytes_exe",
              "t_exec_ms", "razao_x64_o2", "razao_x64_o0", "instrucoes_estaticas", "spills", "ok", "erro"}
    for Linha in Linhas[:2]:
        Afirmar(set(Linha) == Campos, f"campos da linha de {Linha['bench']} diferem: {sorted(set(Linha) ^ Campos)}")
    Afirmar(Linhas[1]["ok"] is False and "função" in Linhas[1]["erro"], "bench que falhou deveria vir com ok=false e erro")
    Afirmar(Linhas[2]["bench"] == NOME_RESUMO and Linhas[2]["geomedia_x64_o2"] is not None, "RESUMO sem geomedia")
    for Linha in Linhas:
        Texto = json.dumps(Linha, ensure_ascii=False)
        Afirmar(json.loads(Texto) == Linha, f"linha nao volta do JSON: {Texto}")
    Afirmar("função" in json.dumps(Linhas[1], ensure_ascii=False), "acento deveria sair cru (ensure_ascii=False)")


def TesteSomaEstaticas():
    Dados = {"benches": [
        {"nome": "aabb_raio", "funcoes": [{"instrucoes_forja": 10, "refs_pilha_forja": 2}, {"instrucoes_forja": 5, "refs_pilha_forja": 1}]},
        {"nome": "outro", "funcoes": [{"instrucoes_forja": 99, "refs_pilha_forja": 99}]},
    ]}
    Afirmar(SomarEstaticas(Dados, "aabb_raio") == {"instrucoes_estaticas": 15, "spills": 3}, "soma de estaticas errada")


def TesteAlarmeSemAnterior():
    Afirmar(AcharRodadaAnterior([], "nao") == [], "historico vazio deveria dar linha de base")
    Atual = RodadaEmbutida("r1", "nao", AabbX64=100.0)
    Afirmar(CompararRodadas(Atual, []) == [], "sem rodada anterior nao deveria alarmar")


def TesteAlarmeAbaixoDoLimite():
    Anterior = AcharRodadaAnterior(RodadaEmbutida("r0", "nao", AabbX64=100.0), "nao")
    Atual = RodadaEmbutida("r1", "nao", AabbX64=104.0)
    Afirmar(CompararRodadas(Atual, Anterior) == [], f"+4% nao deveria alarmar: {CompararRodadas(Atual, Anterior)}")


def TesteAlarmeAcimaDoLimite():
    Anterior = AcharRodadaAnterior(RodadaEmbutida("r0", "nao", AabbX64=100.0), "nao")
    Atual = RodadaEmbutida("r1", "nao", AabbX64=106.0)
    Alarmes = CompararRodadas(Atual, Anterior)
    Esperado = ["ALARME aabb_raio: x64 100.0 ms -> 106.0 ms (+6.0%)"]
    Afirmar(Alarmes == Esperado, f"+6% deveria dar {Esperado}, veio {Alarmes}")


def TesteAlarmeChecagemDiferente():
    Historico = RodadaEmbutida("r0", "sim", AabbX64=100.0)
    Atual = RodadaEmbutida("r1", "nao", AabbX64=200.0)
    Afirmar(AcharRodadaAnterior(Historico, "nao") == [], "checagem diferente nao deveria ser comparada")
    Afirmar(CompararRodadas(Atual, AcharRodadaAnterior(Historico, "nao")) == [], "checagem diferente nao deveria alarmar")
    Afirmar(len(AcharRodadaAnterior(Historico, "sim")) == len(Historico), "a mesma checagem deveria ser achada")


def TesteRepasseChecagemEstaticas():
    # M7a F0: a checagem da rodada chega ao diagnostico_otim (bug: ExecutarFerramentaOtim nao a repassava).
    # M7b F0: bench com checagem NAO provada (lista_append: sim 260 x nao 204); particulas_soa e mapa_insere_acha ficaram iguais desde a F6b do M7a.
    Sim = ArgumentosFerramentaOtim("lista_append", "forja.exe", "saida", "sim")
    Afirmar(Sim[-2:] == ["--checagem", "sim"], f"estaticas com sim deveriam levar --checagem sim: {Sim}")
    Afirmar("--checagem" not in ArgumentosFerramentaOtim("particulas_soa", "forja.exe", "saida"), "sem checagem nao deveria haver o flag")
    if not FERRAMENTA_OTIM.is_file():
        return
    Pasta = Path(tempfile.mkdtemp(prefix="medir_bench_estaticas_"))
    try:
        Valores = {C: LerEstaticas("lista_append", FORJA_PADRAO, Pasta / C, C)["instrucoes_estaticas"] for C in ("sim", "nao")}
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    Afirmar(None not in Valores.values() and Valores["sim"] != Valores["nao"], f"instrucoes estaticas sim/nao deveriam diferir: {Valores}")


def TesteComandoChecagem():
    # Comando simulado: Executar trocado por um falso que so guarda os argumentos (nenhum forja roda).
    global Executar
    Original = Executar
    Gravados = []

    def Falso(Argumentos, Cwd):
        Gravados.append([str(A) for A in Argumentos])
        return subprocess.CompletedProcess(Argumentos, 0, b"", b"")

    Executar = Falso
    try:
        Fonte = Path("falso.fj")
        Pasta = Path(tempfile.gettempdir())
        CompilarX64("forja_falso", Fonte, Pasta, Checagem="sim")
        CompilarX64("forja_falso", Fonte, Pasta)
    finally:
        Executar = Original
    Afirmar(Gravados[0][-2:] == ["--checagem", "sim"], f"x64 com sim deveria acrescentar o flag: {Gravados[0]}")
    Afirmar("--checagem" not in Gravados[1], f"x64 sem checagem nao deveria ter o flag: {Gravados[1]}")
    Afirmar("Checagem" not in inspect.signature(CompilarO0).parameters, "CompilarO0 nao deveria receber Checagem")
    Afirmar("Checagem" not in inspect.signature(CompilarO2).parameters, "CompilarO2 nao deveria receber Checagem (binario congelado)")
    try:
        ArgumentosChecagem("talvez")
    except ErroBench:
        return
    raise AssertionError("checagem invalida deveria falhar")


def TesteReferenciaAusenteErroBench():
    # Fonte fora do banco selado (fonte nova ou editada depois da captura) falha como ErroBench nos dois niveis: nunca pula, nunca recompila (D0730).
    Pasta = Path(tempfile.mkdtemp(prefix="medir_bench_ref_"))
    try:
        Fonte = Pasta / "sem_referencia.fj"
        Fonte.write_text("// fonte sem binario congelado\n", encoding="utf-8")
        for Construtor in (CompilarO0, CompilarO2):
            try:
                Construtor(None, Fonte, Pasta)
            except ErroBench as Erro:
                Afirmar("sem_referencia" in str(Erro), f"a mensagem deveria nomear a fonte: {Erro}")
                continue
            raise AssertionError(f"{Construtor.__name__} aceitou fonte sem referencia congelada")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def LinhaContraEmbutida(Rotulo, Indice, X64Ms):
    Resultados = [{"nome": Nome, "x64": Ms, "O0": Ms, "O2": Ms, "t_build": {}, "bytes": {}} for Nome, Ms in X64Ms.items()]
    Identidade = {"forja": "forja.exe", "versao_forja": "forja x", "sha_forja": "abcdef012345", "ir": {}}
    return MontarLinhaContra(Rotulo, Indice, Resultados, Identidade, datetime(2026, 11, 17, 9, 0, 0))


def TesteOrdemAbba():
    Afirmar(OrdemRodada(0) == ("A", "B") and OrdemRodada(1) == ("B", "A"), "ABBA: a rodada 0 mede A primeiro e a 1 mede B primeiro")
    Primeiros = [OrdemRodada(I)[0] for I in range(5)]
    Afirmar(Primeiros.count("A") == 3 and Primeiros.count("B") == 2, f"5 rodadas deveriam ter 3 A primeiro: {Primeiros}")


def TesteRazaoContra():
    # A 3x mais lento que B em formato_texto: fora do codegen. Geomedia total = raiz de 3; codegen = 1.
    Linhas = [LinhaContraEmbutida("A", I, {"a": 10.0, "formato_texto": 30.0}) for I in range(5)]
    Linhas += [LinhaContraEmbutida("B", I, {"a": 10.0, "formato_texto": 10.0}) for I in range(5)]
    Razao = RazaoContra(Linhas, {})
    Afirmar(abs(Razao["total"] - 3.0 ** 0.5) < 1e-9, f"razao total deveria ser raiz de 3, veio {Razao['total']}")
    Afirmar(abs(Razao["codegen"] - 1.0) < 1e-9, f"razao codegen deveria ser 1 (sem formato_texto), veio {Razao['codegen']}")
    Iguais = [LinhaContraEmbutida(R, I, {"a": 10.0}) for R in ("A", "B") for I in range(5)]
    Afirmar(abs(RazaoContra(Iguais, {})["total"] - 1.0) < 1e-9, "A==B deveria dar razao 1,00")


def TesteContraForaDoAlarme():
    # A linha contra nao tem 'checagem' nem 'bench': a rodada normal de mesma checagem 'n/d' nao a pega como anterior.
    Contra = LinhaContraEmbutida("A", 0, {"a": 10.0})
    Afirmar("checagem" not in Contra and "bench" not in Contra and Contra["tipo"] == "contra", f"linha contra com campos errados: {sorted(Contra)}")
    Historico = [Contra] + RodadaEmbutida("r0", "n/d", AabbX64=100.0)
    Anterior = AcharRodadaAnterior(Historico, "n/d")
    Afirmar(all("bench" in L for L in Anterior) and len(Anterior) == 3, f"a rodada anterior deveria ter so as 3 linhas normais: {Anterior}")


def TesteCodegenSemFormato():
    Resultados = [ResultadoEmbutido("formato_texto", X64=5.0, O0=5.0, O2=1.0), ResultadoEmbutido("fib_rec", X64=4.0, O0=4.0, O2=2.0)]
    Afirmar(RazoesCodegenBench(Resultados, "O2") == [2.0], f"codegen deveria ignorar formato_texto: {RazoesCodegenBench(Resultados, 'O2')}")


def FabricarExecutarFalso(BenchsDiferentes):
    # Fake do Executar: grava no -o um 'EXE' que so muda entre as forjas nos benches listados; nenhum forja roda.
    def Falso(Argumentos, Cwd):
        Lista = [str(A) for A in Argumentos]
        if "--versao" in Lista:
            return subprocess.CompletedProcess(Argumentos, 0, b"forja falso 1.0\n", b"")
        Conteudo = Lista[0].encode("utf-8") if Path(Lista[5]).stem in BenchsDiferentes else b"mesmo"
        Path(Lista[-1]).write_bytes(Conteudo)
        return subprocess.CompletedProcess(Argumentos, 0, b"", b"")
    return Falso


def TesteIdenticosSemCronometro():
    # Duas copias do mesmo EXE dao identico e nao vao ao MedirTodos; o EXE diferente segue ao ABBA (5 rodadas x 2 forjas).
    global Executar, MedirTodos
    OriginalExecutar, OriginalMedir = Executar, MedirTodos
    Chamadas = []

    def FalsoMedir(Forja, Pasta, *, Checagem=None, Nomes=None, Progresso=None):
        Chamadas.append(list(Nomes))
        return [ResultadoEmbutido(N, X64=10.0, O0=10.0, O2=10.0) for N in Nomes]

    Pasta = Path(tempfile.mkdtemp(prefix="medir_bench_identico_"))
    try:
        Executar = FabricarExecutarFalso({"bench_dif"})
        Identicos, Diferentes = ClassificarBenchs([Path("bench_igual.fj"), Path("bench_dif.fj")], "forja_a", "forja_b", Pasta)
        Afirmar(list(Identicos) == ["bench_igual"] and len(Identicos["bench_igual"]) == 64, f"bench igual deveria ser identico: {Identicos}")
        Afirmar(Diferentes == ["bench_dif"], f"EXE diferente deveria seguir ao ABBA: {Diferentes}")
        ArqA, ArqB = Pasta / "forja_a.exe", Pasta / "forja_b.exe"
        ArqA.write_bytes(b"A")
        ArqB.write_bytes(b"B")
        Args = argparse.Namespace(forja=str(ArqA), contra=str(ArqB), rodadas=5, bench=None, sem_ir=True, sem_historico=True, historico="", checagem=None)
        Executar = FabricarExecutarFalso({"fib_rec"})
        MedirTodos = FalsoMedir
        with contextlib.redirect_stdout(io.StringIO()) as Saida:
            Codigo = RodarContra(Args, Pasta)
    finally:
        Executar, MedirTodos = OriginalExecutar, OriginalMedir
        shutil.rmtree(Pasta, ignore_errors=True)
    Total = len(ListarBenchs())
    Afirmar(Codigo == 0, f"--contra com um EXE diferente deveria sair 0, saiu {Codigo}")
    Afirmar(Chamadas == [["fib_rec"]] * 10, f"so fib_rec deveria ir ao ABBA (5 rodadas x 2 forjas): {Chamadas}")
    Afirmar(f"{Total - 1} de {Total} benches idênticos (não cronometrados)" in Saida.getvalue(), "faltou a contagem dos identicos no fim")


def TesteIdenticoNaGeomedia():
    # Bench identico entra na geomedia com razao 1,0 (1, 3 e 1 -> raiz cubica de 3) e nao vira linha de alarme.
    Linhas = [LinhaContraEmbutida("A", I, {"a": 10.0, "formato_texto": 30.0}) for I in range(5)]
    Linhas += [LinhaContraEmbutida("B", I, {"a": 10.0, "formato_texto": 10.0}) for I in range(5)]
    Razao = RazaoContra(Linhas, {"ident": "abcdef012345"})
    Afirmar(abs(Razao["total"] - 3.0 ** (1.0 / 3.0)) < 1e-9, f"razao total deveria ser raiz cubica de 3, veio {Razao['total']}")
    Afirmar(abs(Razao["codegen"] - 1.0) < 1e-9, f"codegen deveria ignorar formato_texto e manter o identico 1,0: {Razao['codegen']}")
    Afirmar(Razao["por_bench"]["ident"] == 1.0, "bench identico deveria ter razao 1,0")
    Linha = MontarLinhaIdentico("ident", "abcdef012345", {"A": {"sha_forja": "x"}, "B": {"sha_forja": "y"}}, datetime(2026, 11, 17, 9, 0, 0))
    Afirmar(Linha["identico"] is True and Linha["razao_ab"] == 1.0 and "checagem" not in Linha and "bench" not in Linha, f"linha identica errada: {sorted(Linha)}")
    Historico = [Linha] + RodadaEmbutida("r0", "n/d", AabbX64=100.0)
    Afirmar(len(AcharRodadaAnterior(Historico, "n/d")) == 3, "linha identica nao pode entrar na rodada anterior do alarme")


def Selftest():
    Testes = [
        ("MediaGeometrica", TesteMediaGeometrica),
        ("DividirValorZero", TesteDividirValorZero),
        ("TickDoJogo", TesteTick),
        ("LinhasJsonl", TesteLinhasJsonl),
        ("SomaEstaticas", TesteSomaEstaticas),
        ("AlarmeSemAnterior", TesteAlarmeSemAnterior),
        ("AlarmeAbaixoDoLimite", TesteAlarmeAbaixoDoLimite),
        ("AlarmeAcimaDoLimite", TesteAlarmeAcimaDoLimite),
        ("AlarmeChecagemDiferente", TesteAlarmeChecagemDiferente),
        ("ComandoChecagem", TesteComandoChecagem),
        ("ReferenciaAusenteErroBench", TesteReferenciaAusenteErroBench),
        ("RepasseChecagemEstaticas", TesteRepasseChecagemEstaticas),
        ("OrdemAbba", TesteOrdemAbba),
        ("RazaoContra", TesteRazaoContra),
        ("ContraForaDoAlarme", TesteContraForaDoAlarme),
        ("CodegenSemFormatoTexto", TesteCodegenSemFormato),
        ("IdenticosSemCronometro", TesteIdenticosSemCronometro),
        ("IdenticoNaGeomedia", TesteIdenticoNaGeomedia),
    ]
    Falhas = 0
    for Nome, Teste in Testes:
        try:
            Teste()
        except Exception as Erro:
            print(f"FALHOU {Nome}: {Erro}")
            Falhas += 1
            continue
        print(f"OK {Nome}")
    return Falhas == 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except ErroBench as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        sys.exit(2)
