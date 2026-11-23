"""Mede o orquestrador de projeto (forja build --projeto) e prova o aceite A1..A7 do P22-O3 com os projetos gerados "largo" e "proj250".

A1 determinismo (sha256 de todo arquivo da saida e as colunas nome/chave/hash_interface do estado iguais entre jobs=1 e 3 rodadas de jobs=12; programa gerado = esperado),
A2 sobrecarga do orquestrador contra um script Python que roda os mesmos "forja build" em sequencia, A3 ganho do largo (j1/j6, j1/j12), A4 ganho do proj250 (j1/padrao),
A5 no-op, A6 recompilacoes por tipo de mudanca (corpo de 1 dll, struct da base, asset), A7 largo em jobs padrao contra o monolito x64. Mediana de 7 amostras depois de 2 aquecimentos, perf_counter, rodizio entre as configuracoes (a deriva da maquina pega todas igual), projetos e
saidas em %TEMP%. O ganho de DIVIDIR (monolito x largo j1) e o de PARALELIZAR (j1 x jN no mesmo projeto) saem em linhas separadas, nunca somados.
Ruido: a ocupacao da CPU (GetSystemTimes) e lida antes de cada rodada; acima de --ruido-limite % em media a linha sai marcada RUIDOSO e o aceite de tempo fica "nao conclusivo".
Desde a 0.4.0 a medicao e ROBUSTA AO RUIDO POR DESENHO (nao exige repouso): A2 em PARES alternados (script, orquestrador j1; ordem ABBA a cada par) e o criterio usa a
MEDIANA DAS RAZOES POR PAR t_orq/t_script (a razao das medianas e os pares ficam no JSON); o ruido de fundo cai igual nos dois lados do par. Desde a 0.5.0 o A3 e o A3N: o teto
e o PISO FISICO da rodada jN medido com os intervalos reais do FORJA_ORQ_TEMPOS (campo ini_us): piso_jN = serial_nao_sobreposto + max(caminho_critico, trabalho_paralelo / N_efetivo(N))
+ vida do exe final; trabalho e caminho critico vem de uma rodada j1 (sem contencao; a contencao ja esta no N efetivo, contar na rodada jN seria contar 2x);
serial_nao_sobreposto = parede - uniao dos intervalos [ini, ini+us] das vidas dos filhos; N_efetivo(N) = speedup(N) = N * t(1) / t(N) de um microteste CPU-bound
proprio (N processos python -I com laco fixo; mediana de CALIB_RODADAS rodadas alternadas) - uma maquina de 6 nucleos com SMT nao entrega 12x. A3N passa se 0,8 <= piso_jN / t(jN)
<= 1,05; acima de 1,05 o modelo esta errado (conta menos do que a maquina fez) e FALHA "modelo", nunca passa. A8: mudanca abaixo de 1 s = parede <= 1000 ms nos cenarios incrementais
(corpo de 1 dll no largo e no proj250; struct da base no proj250) e no build monolitico do corpus_200000.
Conclusivo = IQR/mediana <= 0,15 (dispersao do par em A2; das configs envolvidas nos demais); senao 'dispersao_alta' (relata, nao falha a suite por isso).

Versao: 0.6.0
Changelog: 0.6.0 - 0.21.0 F3: sai MedirBackendC (amostra informativa do backend C por produto) e o campo backend_c do JSON, porque o backend C e aposentado na Forja 0.21.0 (--backend c vira X0003; decisao Opus D0730); ImprimirTempos perde a linha backend C.
           0.5.0 - M4 P24a2: MedirAmdahl -> MedirPiso (rodada j1 com FORJA_ORQ_TEMPOS = BaseDeJ1: mediana por dll de trabalho e critico sem contencao; depois j6/j12 com os
           intervalos reais; piso = serial(jN) + max(critico_j1, trabalho_j1 / N_ef) + exe(jN); resumo ganha trabalho_j1_ms, critico_j1_ms, t_j1_ms e contencao = trabalho_jN / trabalho_j1), AvaliarA3 -> AvaliarA3N (0,8 <= razao <= 1,05), AvaliarA8 novo;
           JSON ganha "piso" e "a8" (sai "amdahl"); sai SerialDaRodada/TetoDeAmdahl/ETAPAS_FORA_DO_SERIAL; selftest com 4 casos de intervalos sinteticos (sobrepostos, disjuntos,
           filho unico, modelo errado).
           0.4.0 - A2 pareado/ABBA (mediana das razoes por par, 15 pares por padrao, --amostras muda), calibracao de N efetivo e teto de Amdahl com ele, conclusivo por dispersao
           (IQR/mediana <= 0,15) em A2/A3/A4/A7, ruido_fundo_pct (antes/depois) so informativo, --selftest cobre ABBA, razoes, N efetivo, teto e decisao.
           0.3.0 - --exigir-repouso: a ocupacao NAO e mais medida durante a amostra (o proprio forja j6/j12 satura os nucleos e toda amostra paralela saia ruidosa); mede o fundo
           numa janela de SENTINELA_S s ANTES (forja parado) e noutra DEPOIS (forja ja terminou) e refaz a amostra (ate 3x) se qualquer uma passar de 5% (um terceiro
           processo ativo aparece nas duas). A3 passa a ser >= 0.8 x o teto de Amdahl MEDIDO (MedirAmdahl: serial = etapas do orquestrador + exe final num j1 com
           FORJA_ORQ_TEMPOS; teto_j6/teto_j12/teto_infinito gravados em tempos.amdahl, 3 casas) no lugar de 3.5x/4.0x fixos.
           0.2.0 - limpeza com 5 tentativas e aviso em stderr; NucleosFisicos sem leitura devolve None (JSON null); contagem_estavel entra em A5 e A6;
           AvaliarIncrementais por tabela; ContextoMedicao (dataclass) no lugar dos parametros repetidos; --amostras >= 1; --exigir-repouso (espera
           CPU <= 5% antes de cada grupo de amostras, refaz amostra ruidosa ate 3x, plano de energia e dispersao no JSON; estouro sai com 2 e
           resultado nao_conclusivo); --tempos-orq (FORJA_ORQ_TEMPOS, mediana por etapa em tempos_orq); --selftest.
           0.1.0 - M3 P22-O3: primeira versao (A1..A7, rodizio, ruido, JSON em tests\\projeto\\resultados).
Uso: python -I medir_projeto.py --forja <COPIA do forja.exe> --tudo        (--so a1 | --so tempos; --amostras N; --aquecimentos N; --manter; --exigir-repouso; --tempos-orq)
Verificacao: python -I medir_projeto.py --selftest
             python -I medir_projeto.py --forja <copia> --so a1
"""

import argparse
import ctypes
import hashlib
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from collections import namedtuple
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Optional

sys.dont_write_bytecode = True
PASTA_TESTES = Path(__file__).resolve().parent
sys.path.insert(0, str(PASTA_TESTES))
import gerar_projeto  # noqa: E402  (lint-ok: import depois do sys.path, gerador irmao)

RAIZ_LINGUAGEM = PASTA_TESTES.parent.parent
CORPUS_200000 = RAIZ_LINGUAGEM / "benchmark" / "corpus" / "gerado" / "corpus_200000.fj"
ESPERADO_200000 = RAIZ_LINGUAGEM / "benchmark" / "corpus" / "gerado" / "corpus_200000.esperado.txt"
TIMEOUT = 900
AMOSTRAS = 7
AQUECIMENTOS = 2
RUIDO_LIMITE = 10.0                  # % de CPU ocupada por outros, em media, para a linha deixar de ser RUIDOSO
JOBS_VARRIDOS = (1, 2, 4, 6, 12)
EXECUTAVEL = {"largo": "jogo.exe", "proj250": "app.exe"}
MONOLITO_DE_HOJE_MS = 2567
META_J1_PADRAO, META_NOOP_MS, META_SOBRECARGA = 2.0, 50.0, 1.05
FRACAO_PISO = 0.8                    # A3N: piso_jN / t(jN) >= esta fracao
LIMITE_MODELO = 1.05                 # A3N: piso_jN / t(jN) acima disso = o modelo conta menos do que a maquina fez (FALHA "modelo")
META_A8_MS = 1000.0                  # A8: mudanca abaixo de 1 s
EXE_DO_LARGO = "jogo"
LIMITE_J12_CONTRA_J6 = 1.05
Resultado = namedtuple("Resultado", "codigo saida erro ms")
VERSAO = "0.6.0"
PARES_A2 = 15                        # A2: pares (script, orquestrador j1) medidos; --amostras muda
LIMITE_DISPERSAO = 0.15              # IQR/mediana maximo para a medicao ser conclusiva
CALIB_NS = (1, 2, 4, 6, 8, 12)       # processos simultaneos da calibracao de N efetivo
CALIB_RODADAS = 5
CALIB_ITERACOES = 8_000_000          # ~0,5 s de CPU por processo
CODIGO_CALIB = f"s = 0\nfor i in range({CALIB_ITERACOES}):\n    s += i * i\n"
# apagar bin\ e .forja_cache\ falha quando o antivirus segura um arquivo: espera 0,2 s, 0,4 s, 0,8 s e 1,6 s entre as 5 tentativas
TENTATIVAS_LIMPEZA = 5
ESPERA_LIMPEZA_S = 0.2
# --exigir-repouso: a CPU precisa ficar <= REPOUSO_PCT por REPOUSO_JANELA_S seguidos (uma leitura a cada REPOUSO_PASSO_S), no maximo REPOUSO_LIMITE_S
REPOUSO_PCT = 5.0
REPOUSO_JANELA_S = 3.0
REPOUSO_PASSO_S = 0.25
REPOUSO_LIMITE_S = 600.0
# amostra ruidosa (CPU de fundo acima de RUIDO_LIMITE) e refeita no maximo estas vezes
REFACOES_MAXIMAS = 3
SENTINELA_S = 1.0                    # janela curta antes e depois de cada amostra (forja parado) para detectar terceiros


class NaoConclusivo(RuntimeError):
    """Medicao sem repouso (--exigir-repouso estourou o limite): o Main sai com codigo 2 e grava 'resultado': 'nao_conclusivo' no JSON."""


@dataclass
class ContextoMedicao:
    """Opcoes e caminhos comuns a uma execucao inteira; as funcoes de medicao recebem este objeto em vez de repetir os parametros.
    Refeitas acumula o ruido (%) de cada tentativa refeita por --exigir-repouso."""
    Forja: Path
    Trabalho: Path
    Amostras: int
    Aquecimentos: int
    ExigirRepouso: bool = False
    TemposOrq: Optional[Path] = None
    Refeitas: list = field(default_factory=list)
    Pares: int = PARES_A2


class Filetime(ctypes.Structure):
    _fields_ = [("baixo", ctypes.c_uint32), ("alto", ctypes.c_uint32)]

    def Valor(Self):
        return (Self.alto << 32) | Self.baixo


def Cronometrar(Comando, Pasta, Env=None):
    """Roda o comando em Pasta e devolve Resultado(codigo, stdout, stderr, ms por perf_counter)."""
    Inicio = time.perf_counter()
    Feito = subprocess.run([str(Parte) for Parte in Comando], cwd=Pasta, capture_output=True, timeout=TIMEOUT, env=Env)
    Ms = (time.perf_counter() - Inicio) * 1000.0
    return Resultado(Feito.returncode, Feito.stdout.decode("utf-8", "replace"), Feito.stderr.decode("utf-8", "replace"), Ms)


def ExigirOk(R, Rotulo):
    if R.codigo != 0:
        raise RuntimeError(f"{Rotulo} saiu com {R.codigo}: {(R.erro or R.saida)[:600]}")
    return R


def LerTemposDaCpu():
    """(ocioso, total) acumulados do sistema pelo GetSystemTimes (100 ns); a diferenca entre duas leituras da a ocupacao."""
    Ocioso, Kernel, Usuario = Filetime(), Filetime(), Filetime()
    ctypes.windll.kernel32.GetSystemTimes(ctypes.byref(Ocioso), ctypes.byref(Kernel), ctypes.byref(Usuario))
    return Ocioso.Valor(), Kernel.Valor() + Usuario.Valor()


def OcupacaoEntre(Antes, Depois):
    """% de CPU ocupada no sistema inteiro entre duas leituras de LerTemposDaCpu (0 se o intervalo nao teve tempo de CPU)."""
    OciosoA, TotalA = Antes
    OciosoB, TotalB = Depois
    Total = TotalB - TotalA
    return 100.0 * (1.0 - (OciosoB - OciosoA) / Total) if Total > 0 else 0.0


def OcupacaoDaCpu(Segundos):
    """% de CPU ocupada no sistema inteiro durante Segundos (GetSystemTimes; o contador do Get-Counter e localizado e falha no PT-BR)."""
    Antes = LerTemposDaCpu()
    time.sleep(Segundos)
    return OcupacaoEntre(Antes, LerTemposDaCpu())


def EsperarRepouso(Limite=REPOUSO_LIMITE_S):
    """Espera a CPU ficar <= REPOUSO_PCT por REPOUSO_JANELA_S seguidos (uma leitura a cada REPOUSO_PASSO_S); passados Limite segundos, levanta NaoConclusivo."""
    print(f"[repouso] esperando CPU <= {REPOUSO_PCT}% por {REPOUSO_JANELA_S} s seguidos (limite {Limite} s)", flush=True)
    Inicio = time.monotonic()
    Anterior, Calma = LerTemposDaCpu(), 0.0
    while Calma < REPOUSO_JANELA_S:
        if time.monotonic() - Inicio > Limite:
            raise NaoConclusivo(f"CPU nao ficou <= {REPOUSO_PCT}% por {REPOUSO_JANELA_S} s seguidos em {Limite} s (repouso antes de um grupo de amostras)")
        time.sleep(REPOUSO_PASSO_S)
        Atual = LerTemposDaCpu()
        # a janela so conta se a CPU ficou calma nela: uma leitura ocupada zera a contagem
        Calma = Calma + REPOUSO_PASSO_S if OcupacaoEntre(Anterior, Atual) <= REPOUSO_PCT else 0.0
        Anterior = Atual
    return time.monotonic() - Inicio


def AmostraComRepouso(Contexto, Executar):
    """Roda uma amostra; com --exigir-repouso mede o fundo numa janela curta ANTES (forja parado) e num 'sentinela' DEPOIS (forja ja terminou) e refaz a
    amostra se qualquer um passar de REPOUSO_PCT, ate REFACOES_MAXIMAS vezes. Nao se mede a CPU durante a amostra: la o proprio forja (j6/j12) satura os
    nucleos e todo paralelo sairia ruidoso; um terceiro processo ativo aparece nas duas janelas em que o forja nao roda."""
    if not Contexto.ExigirRepouso:
        return Executar()
    for Tentativa in range(REFACOES_MAXIMAS + 1):
        Antes = OcupacaoDaCpu(SENTINELA_S)
        Valor = Executar()
        Ruido = max(Antes, OcupacaoDaCpu(SENTINELA_S))
        if Ruido <= REPOUSO_PCT:
            return Valor
        Contexto.Refeitas.append(round(Ruido, 1))
    # ainda ruidosa depois das refacoes: segue com a ultima medida; o ruido de cada tentativa fica em Refeitas
    return Valor


def ApagarPasta(Caminho):
    """Apaga Caminho; se o antivirus segurar um arquivo, tenta de novo com espera crescente e, se ainda falhar, avisa em stderr com o caminho e o erro (a medicao segue)."""
    Erro = None
    for Tentativa in range(TENTATIVAS_LIMPEZA):
        if not Caminho.exists():
            return
        if Tentativa:
            time.sleep(ESPERA_LIMPEZA_S * 2 ** (Tentativa - 1))
        try:
            shutil.rmtree(Caminho)
        except OSError as Falha:
            Erro = Falha
    if Caminho.exists():
        print(f"[aviso] nao consegui apagar {Caminho} apos {TENTATIVAS_LIMPEZA} tentativas: {Erro}", file=sys.stderr, flush=True)


def Limpar(Pasta):
    """Apaga bin\\ e .forja_cache\\ (e o cache do script) do projeto; cada pasta passa por ApagarPasta, com nova tentativa se o antivirus segurar um arquivo."""
    for Nome in ("bin", ".forja_cache", ".forja_script"):
        ApagarPasta(Pasta / Nome)


def AmbienteOrquestrador(Contexto):
    """Ambiente das execucoes do orquestrador: com --tempos-orq aponta FORJA_ORQ_TEMPOS para o jsonl; sem a opcao devolve None (herda o ambiente)."""
    if Contexto.TemposOrq is None:
        return None
    return {**os.environ, "FORJA_ORQ_TEMPOS": str(Contexto.TemposOrq)}


def RodarOrquestrador(Contexto, Pasta, Jobs=None, Opcoes=()):
    Comando = [Contexto.Forja, "build", "--projeto", Pasta / "forja.proj"]
    Comando += ["--jobs", Jobs] if Jobs else []
    return Cronometrar(Comando + list(Opcoes), Pasta, AmbienteOrquestrador(Contexto))


def JobsEfetivos(Saida):
    Achado = re.search(r"jobs=(\d+)", Saida)
    return int(Achado.group(1)) if Achado else None


def LerManifesto(Pasta):
    """[(nome, tipo, entrada)] na ordem do forja.proj (que ja e topologica nos dois projetos gerados)."""
    Produtos = []
    for Linha in (Pasta / "forja.proj").read_text(encoding="utf-8").splitlines():
        Partes = Linha.split()
        if Partes and Partes[0] == "produto":
            Produtos.append((Partes[1], Partes[2], Partes[3]))
    return Produtos


def RodarScriptSequencial(Forja, Pasta):
    """Os mesmos 'forja build' filhos do orquestrador, um depois do outro, num script Python (sem hash, estado nem trava): a linha de base do A2."""
    Entradas = Pasta / ".forja_script"
    Entradas.mkdir(parents=True, exist_ok=True)
    (Pasta / "bin").mkdir(parents=True, exist_ok=True)
    Inicio = time.perf_counter()
    for Nome, Tipo, Entrada in LerManifesto(Pasta):
        Comando = [Forja, "build", "--backend", "x64"] + (["--modo", "dll"] if Tipo == "dll" else [])
        Comando += [Pasta / Entrada, "-o", Pasta / "bin" / f"{Nome}.{Tipo}", "--entradas", Entradas / f"{Nome}.entradas", "--lib-pasta", Pasta / "bin"]
        Feito = subprocess.run([str(Parte) for Parte in Comando], cwd=Pasta, capture_output=True, timeout=TIMEOUT)
        if Feito.returncode != 0:
            raise RuntimeError(f"script sequencial: {Nome} saiu com {Feito.returncode}")
    return (time.perf_counter() - Inicio) * 1000.0


def Sha256Arquivo(Caminho):
    return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()


def Fotografar(Pasta):
    """{caminho relativo em bin\\: sha256} de todo arquivo da saida."""
    Base = Pasta / "bin"
    return {Arquivo.relative_to(Base).as_posix(): Sha256Arquivo(Arquivo) for Arquivo in sorted(Base.rglob("*")) if Arquivo.is_file()}


def LerEstado(Pasta):
    """As colunas nome, chave e hash_interface do estado dev, ordenadas."""
    Linhas = (Pasta / ".forja_cache" / "estado" / "dev.tsv").read_text(encoding="utf-8").splitlines()
    return sorted(tuple(Linha.split("\t")[:3]) for Linha in Linhas if Linha and not Linha.startswith("#"))


def PrimeiroOffset(A, B):
    for Posicao, (X, Y) in enumerate(zip(A, B)):
        if X != Y:
            return Posicao
    return min(len(A), len(B))


def CompararRodada(Referencia, Foto, PastaRef, PastaAtual):
    """Lista das diferencas (arquivo, offset) da rodada contra a referencia; vazia = identica."""
    Diferencas = []
    for Arquivo in sorted(set(Referencia) | set(Foto)):
        if Referencia.get(Arquivo) == Foto.get(Arquivo):
            continue
        if Arquivo not in Referencia or Arquivo not in Foto:
            Diferencas.append({"arquivo": Arquivo, "offset": None, "motivo": "so existe em uma das saidas"})
            continue
        Offset = PrimeiroOffset((PastaRef / Arquivo).read_bytes(), (PastaAtual / "bin" / Arquivo).read_bytes())
        Diferencas.append({"arquivo": Arquivo, "offset": Offset, "motivo": "bytes diferentes"})
    return Diferencas


def ConferirPrograma(Pasta, Tipo):
    """Roda o exe gerado (bin\\ no PATH) e confere a saida com esperado.txt (CRLF do stdout do Windows -> LF); no largo tambem esperado.txt = corpus_200000.esperado.txt."""
    Feito = subprocess.run([str(Pasta / "bin" / EXECUTAVEL[Tipo])], cwd=Pasta / "bin", capture_output=True, timeout=TIMEOUT)
    Saida = Feito.stdout.replace(b"\r\n", b"\n")
    Esperado = (Pasta / "esperado.txt").read_bytes()
    Resposta = {"exit": Feito.returncode, "igual_ao_esperado": Saida == Esperado, "bytes": len(Saida)}
    if Tipo == "largo":
        Resposta["esperado_igual_ao_corpus_200000"] = Esperado == ESPERADO_200000.read_bytes()
    if Saida != Esperado:
        Resposta["primeiro_offset"] = PrimeiroOffset(Saida, Esperado)
    return Resposta


def MedirA1(Contexto, Pasta, Tipo):
    """A1: jobs=1 contra 3 rodadas de jobs=12 (saida limpa entre elas, mesmo caminho)."""
    Limpar(Pasta)
    ExigirOk(RodarOrquestrador(Contexto, Pasta, 1), "A1 jobs=1")
    Referencia, EstadoRef = Fotografar(Pasta), LerEstado(Pasta)
    PastaRef = Pasta.parent / f"ref_{Tipo}"
    shutil.rmtree(PastaRef, ignore_errors=True)
    shutil.copytree(Pasta / "bin", PastaRef)
    Programa = ConferirPrograma(Pasta, Tipo)
    Rodadas = []
    for Numero in range(1, 4):
        Limpar(Pasta)
        R = ExigirOk(RodarOrquestrador(Contexto, Pasta, 12), f"A1 jobs=12 #{Numero}")
        Diferencas = CompararRodada(Referencia, Fotografar(Pasta), PastaRef, Pasta)
        Rodadas.append({"rodada": Numero, "diferencas": Diferencas, "estado_igual": LerEstado(Pasta) == EstadoRef, "jobs": JobsEfetivos(R.saida)})
    Ok = all(not Rd["diferencas"] and Rd["estado_igual"] for Rd in Rodadas) and Programa["igual_ao_esperado"] and Programa.get("esperado_igual_ao_corpus_200000", True)
    return {"projeto": Tipo, "arquivos": len(Referencia), "rodadas": Rodadas, "programa": Programa, "ok": Ok}


def IqrRelativo(Valores):
    """IQR / mediana (quartis inclusivos); 0 com menos de 2 valores ou mediana 0."""
    Mediana = statistics.median(Valores)
    if len(Valores) < 2 or not Mediana:
        return 0.0
    Q = statistics.quantiles(Valores, n=4, method="inclusive")
    return (Q[2] - Q[0]) / Mediana


def Resumir(Amostras):
    Mediana = statistics.median(Amostras)
    return {"amostras_ms": [round(A, 1) for A in Amostras], "mediana_ms": round(Mediana, 1), "min_ms": round(min(Amostras), 1), "max_ms": round(max(Amostras), 1),
            "dispersao_pct": round(100.0 * (max(Amostras) - min(Amostras)) / Mediana, 1) if Mediana else 0.0, "iqr_rel": round(IqrRelativo(Amostras), 3)}


def Rodizio(Configs, Contexto):
    """Roda as configuracoes em rodizio (Aquecimentos rodadas descartadas + Amostras medidas); le o ruido da CPU antes de cada rodada medida."""
    Coletado, Ruidos = {Nome: [] for Nome in Configs}, []
    for Rodada in range(Contexto.Aquecimentos + Contexto.Amostras):
        if Rodada >= Contexto.Aquecimentos:
            Ruidos.append(OcupacaoDaCpu(1.0))
        for Nome, Executar in Configs.items():
            Ms = AmostraComRepouso(Contexto, Executar)
            if Rodada >= Contexto.Aquecimentos:
                Coletado[Nome].append(Ms)
    return ResumirRodizio(Coletado, Ruidos)


def ResumirRodizio(Coletado, Ruidos):
    """Resumo de cada config (Resumir) e o ruido medio/maximo lido antes das rodadas medidas."""
    Ruido = {"medio_pct": round(statistics.mean(Ruidos), 1), "max_pct": round(max(Ruidos), 1)}
    return {Nome: Resumir(Valores) for Nome, Valores in Coletado.items()}, Ruido


def ConfigOrquestrador(Contexto, Pasta, Jobs, Efetivos):
    """Config do rodizio: o orquestrador com --jobs Jobs (None = padrao); guarda em Efetivos os jobs efetivos que ele reportou."""
    Nome = "padrao" if Jobs is None else f"j{Jobs}"

    def Executar():
        Limpar(Pasta)
        R = ExigirOk(RodarOrquestrador(Contexto, Pasta, Jobs), Nome)
        Efetivos[Nome] = JobsEfetivos(R.saida)
        return R.ms
    return Executar


def ConfigScript(Forja, Pasta):
    def Executar():
        Limpar(Pasta)
        return RodarScriptSequencial(Forja, Pasta)
    return Executar


def ConfigMonolito(Forja, Saida):
    def Executar():
        Saida.mkdir(parents=True, exist_ok=True)
        return ExigirOk(Cronometrar([Forja, "build", "--backend", "x64", CORPUS_200000, "-o", Saida / "mono.exe"], Saida), "monolito").ms
    return Executar


def MedirJobs(Contexto, Pasta, Tipo):
    """Varre jobs 1/2/4/6/12 e o padrao (sem --jobs) no rodizio, mais o script sequencial e (no largo) o monolito x64."""
    Efetivos, Configs = {}, {}
    if Tipo == "largo":
        Configs["monolito"] = ConfigMonolito(Contexto.Forja, Contexto.Trabalho / "mono")
    for Jobs in JOBS_VARRIDOS:
        Configs[f"j{Jobs}"] = ConfigOrquestrador(Contexto, Pasta, Jobs, Efetivos)
    Configs["padrao"] = ConfigOrquestrador(Contexto, Pasta, None, Efetivos)
    # repouso antes do grupo (o rodizio inteiro e um cenario); dentro do grupo, cada amostra e conferida pelo AmostraComRepouso
    if Contexto.ExigirRepouso:
        EsperarRepouso()
    Resumos, Ruido = Rodizio(Configs, Contexto)
    for Nome, Efetivo in Efetivos.items():
        Resumos[Nome]["jobs_efetivos"] = Efetivo
    return {"projeto": Tipo, "configs": Resumos, "ruido": Ruido}


def OrdemDoPar(Numero):
    """ABBA: pares pares rodam (script, orquestrador), pares impares (orquestrador, script); o ruido que cresce ou cai durante o par nao favorece sempre o mesmo lado."""
    return ("script", "orq") if Numero % 2 == 0 else ("orq", "script")


def ResumirPares(Pares):
    """Pares [{script_ms, orq_ms, primeiro}] -> razao por par (orq/script), mediana das razoes (usada em A2), razao das medianas, min..max, IQR/mediana e conclusivo."""
    Razoes = [P["orq_ms"] / P["script_ms"] for P in Pares]
    Dispersao = IqrRelativo(Razoes)
    return {"pares": [{**P, "razao": round(R, 4)} for P, R in zip(Pares, Razoes)], "mediana_das_razoes": round(statistics.median(Razoes), 4),
            "razao_das_medianas": round(statistics.median(P["orq_ms"] for P in Pares) / statistics.median(P["script_ms"] for P in Pares), 4),
            "razao_min": round(min(Razoes), 4), "razao_max": round(max(Razoes), 4), "iqr_rel": round(Dispersao, 4), "conclusivo": Dispersao <= LIMITE_DISPERSAO}


def MedirA2(Contexto, Pasta):
    """A2 pareado: Aquecimentos pares descartados e Pares medidos; em cada par roda o script sequencial e o orquestrador --jobs 1 em ordem ABBA (OrdemDoPar)."""
    Executar = {"script": ConfigScript(Contexto.Forja, Pasta), "orq": ConfigOrquestrador(Contexto, Pasta, 1, {})}
    Pares = []
    for Numero in range(Contexto.Aquecimentos + Contexto.Pares):
        Ordem = OrdemDoPar(Numero)
        Tempos = {Quem: Executar[Quem]() for Quem in Ordem}
        if Numero >= Contexto.Aquecimentos:
            Pares.append({"script_ms": round(Tempos["script"], 1), "orq_ms": round(Tempos["orq"], 1), "primeiro": Ordem[0]})
    return ResumirPares(Pares)


def TempoDeProcessos(Script, Quantos):
    """ms de parede para Quantos processos 'python -I Script' (laco CPU-bound fixo) rodando juntos ate o ultimo terminar."""
    Inicio = time.perf_counter()
    Filhos = [subprocess.Popen([sys.executable, "-I", str(Script)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) for _ in range(Quantos)]
    for Filho in Filhos:
        Filho.wait()
    return (time.perf_counter() - Inicio) * 1000.0


def SpeedupsDe(Medianas):
    """{N: speedup(N) = N * t(1) / t(N)} das medianas {N: ms}."""
    return {N: N * Medianas[1] / Ms for N, Ms in Medianas.items()}


def NEfetivoDe(Speedups, N):
    """N efetivo = speedup(N) medido, limitado a [1, N] (ruido nao faz a maquina render mais que N nem menos que 1)."""
    return min(float(N), max(1.0, Speedups[N]))


def CalibrarParalelismo(Contexto):
    """Microteste CPU-bound: CALIB_RODADAS rodadas (mais 1 de aquecimento) de N processos para N em CALIB_NS, alternando a ordem dos N a cada rodada; mediana por N.
    Devolve {tabela [{n, mediana_ms, speedup, n_efetivo}]} (o N efetivo de 6 e de 12 alimenta o piso do A3N)."""
    Script = Contexto.Trabalho / "calib_cpu.py"
    Script.write_text(CODIGO_CALIB, encoding="utf-8")
    Coletado = {N: [] for N in CALIB_NS}
    for Rodada in range(CALIB_RODADAS + 1):
        for N in (CALIB_NS if Rodada % 2 == 0 else tuple(reversed(CALIB_NS))):
            Ms = TempoDeProcessos(Script, N)
            if Rodada:
                Coletado[N].append(Ms)
    Medianas = {N: statistics.median(Valores) for N, Valores in Coletado.items()}
    Speedups = SpeedupsDe(Medianas)
    return {"tabela": [{"n": N, "mediana_ms": round(Medianas[N], 1), "speedup": round(Speedups[N], 3), "n_efetivo": round(NEfetivoDe(Speedups, N), 3)} for N in CALIB_NS]}


def LerJson(Saida):
    """(produtos [{produto, acao, motivo, ms, exit}], resumo {projeto}) da saida --json do orquestrador."""
    Produtos, Resumo = [], {}
    for Linha in Saida.splitlines():
        if not Linha.startswith("{"):
            continue
        Item = json.loads(Linha)
        if "projeto" in Item:
            Resumo = Item["projeto"]
        else:
            Produtos.append(Item)
    return Produtos, Resumo


def CenarioIncremental(Tipo, Nome):
    """(mutar, desfazer) de um cenario incremental sobre a pasta do projeto."""
    Tabela = {
        "noop": (lambda P: None, lambda P: None),
        "corpo": (lambda P: gerar_projeto.MutarCorpo(P, Tipo), lambda P: gerar_projeto.DesfazerCorpo(P, Tipo)),
        "struct": (gerar_projeto.MutarStruct, gerar_projeto.DesfazerStruct),
        "assets": (gerar_projeto.CriarAssets, lambda P: None),
    }
    return Tabela[Nome]


def CicloIncremental(Contexto, Pasta, Nome, Cenario):
    """Uma amostra: desfaz a mutacao anterior, reconcilia (sem medir), muda e mede o build --json. Devolve o Resultado do build medido."""
    Mutar, Desfazer = Cenario
    Desfazer(Pasta)
    ExigirOk(RodarOrquestrador(Contexto, Pasta), f"{Nome}: reconciliar")
    Mutar(Pasta)
    return ExigirOk(RodarOrquestrador(Contexto, Pasta, None, ["--json"]), f"{Nome}: medido")


def ContarRecompilados(Produtos, Resumo):
    """Contagem de uma amostra: quantos produtos recompilaram (e o motivo de cada um) e os filhos do projeto."""
    Recompilados = sorted((P["produto"], P["motivo"]) for P in Produtos if P["acao"] == "recompilado")
    return {"recompilados": len(Recompilados), "filhos": Resumo.get("filhos"), "motivos": [f"{N}: {M}" for N, M in Recompilados]}


def MedirIncremental(Contexto, Pasta, Tipo, Nome):
    """Cada amostra e um CicloIncremental (refeito se ruidoso com --exigir-repouso); a contagem de recompilados tem de bater entre as amostras."""
    Mutar, Desfazer = CenarioIncremental(Tipo, Nome)
    Limpar(Pasta)
    if Contexto.ExigirRepouso:
        EsperarRepouso()
    Medidas, Contagens = [], []
    for Rodada in range(Contexto.Aquecimentos + Contexto.Amostras):
        R = AmostraComRepouso(Contexto, lambda: CicloIncremental(Contexto, Pasta, Nome, (Mutar, Desfazer)))
        Produtos, Resumo = LerJson(R.saida)
        if Rodada >= Contexto.Aquecimentos:
            Medidas.append(R.ms)
            Contagens.append(ContarRecompilados(Produtos, Resumo))
    Desfazer(Pasta)
    Iguais = all(C == Contagens[0] for C in Contagens)
    return {"projeto": Tipo, "cenario": Nome, **Resumir(Medidas), "contagem": Contagens[0], "contagem_estavel": Iguais}


def NucleosFisicos():
    """Nucleos fisicos (WMI via powershell). Sem leitura devolve None e avisa em stderr com o motivo: quem usa cai nos processadores logicos e o JSON grava null."""
    try:
        Feito = subprocess.run(["powershell", "-NoProfile", "-Command", "(Get-CimInstance Win32_Processor | Measure-Object NumberOfCores -Sum).Sum"], capture_output=True, timeout=60)
        return int(Feito.stdout.decode().strip())
    except (ValueError, OSError, subprocess.SubprocessError) as Falha:
        # nao chuta um valor: a meta do A3 e o padrao de jobs dependem dele, entao o motivo sai no aviso
        print(f"[aviso] nucleos fisicos indisponiveis ({type(Falha).__name__}: {Falha}); JSON grava null", file=sys.stderr, flush=True)
        return None


def Aceite(Nome, Medido, Meta, Ok, Conclusivo=True):
    return {"aceite": Nome, "medido": Medido, "meta": Meta, "ok": bool(Ok), "conclusivo": Conclusivo, "dispersao_alta": not Conclusivo}


def ConclusivoPorDispersao(Dados, Tipo, *Configs):
    """Conclusivo = IQR/mediana de cada config envolvida <= LIMITE_DISPERSAO (ruido de fundo alto aparece aqui, nao num limiar de CPU)."""
    return all(Dados["jobs"][Tipo]["configs"][Config]["iqr_rel"] <= LIMITE_DISPERSAO for Config in Configs)


def Mediana(Dados, Projeto, Config):
    return Dados["jobs"][Projeto]["configs"][Config]["mediana_ms"]


def AvaliarTempos(Dados):
    """A2..A7 a partir dos dados medidos; 'conclusivo' = False nas linhas medidas com a maquina ruidosa.
    A3N (decisao do usuario, 10/11/2026 + D-M4-5): piso fisico medido da rodada jN do largo (Dados['piso'], ver MedirPiso) contra t(jN); A8: mudanca abaixo de 1 s."""
    Lista = AvaliarA2(Dados) + AvaliarA3N(Dados)
    G4 = Mediana(Dados, "proj250", "j1") / Mediana(Dados, "proj250", "padrao")
    Lista.append(Aceite("A4 proj250: t(j1)/t(padrao)", round(G4, 2), f">= {META_J1_PADRAO}", G4 >= META_J1_PADRAO, ConclusivoPorDispersao(Dados, "proj250", "j1", "padrao")))
    Lista += AvaliarIncrementais(Dados) + AvaliarA8(Dados)
    Padrao, Monolito = Mediana(Dados, "largo", "padrao"), Mediana(Dados, "largo", "monolito")
    Lista.append(Aceite("A7 largo padrao x monolito x64 medido hoje (ms)", f"{Padrao} x {Monolito}", f"< {Monolito} (e < {MONOLITO_DE_HOJE_MS} do contrato)", Padrao < Monolito and Padrao < MONOLITO_DE_HOJE_MS,
                        ConclusivoPorDispersao(Dados, "largo", "padrao", "monolito")))
    return Lista


def AvaliarA2(Dados):
    """A2 por projeto: a MEDIANA DAS RAZOES POR PAR t_orq(j1)/t_script tem de ser <= META_SOBRECARGA; a razao das medianas e a dispersao entram no texto."""
    Lista = []
    for Tipo in ("largo", "proj250"):
        A2 = Dados["a2"][Tipo]
        Medido = f"{A2['mediana_das_razoes']} (razao das medianas {A2['razao_das_medianas']}; pares {A2['razao_min']}..{A2['razao_max']}; IQR/mediana {A2['iqr_rel']}; {len(A2['pares'])} pares)"
        Lista.append(Aceite(f"A2 sobrecarga {Tipo}: mediana das razoes t(j1)/t(script) por par", Medido, f"<= {META_SOBRECARGA}", A2["mediana_das_razoes"] <= META_SOBRECARGA, A2["conclusivo"]))
    return Lista


def ClassificarPiso(Razao):
    """piso_jN / t(jN) -> 'modelo' (> LIMITE_MODELO: o modelo conta menos do que a maquina fez, nunca passa), 'ok' (>= FRACAO_PISO) ou 'abaixo'."""
    if Razao > LIMITE_MODELO:
        return "modelo"
    return "ok" if Razao >= FRACAO_PISO else "abaixo"


def AvaliarA3N(Dados):
    """A3N (D-M4-5): por jN em (6, 12), piso_jN / t(jN) da rodada do largo (Dados['piso'], ver MedirPiso) tem de ficar em [FRACAO_PISO, LIMITE_MODELO]."""
    Lista = []
    for Jobs in (6, 12):
        P = Dados["piso"][f"j{Jobs}"]
        Veredito = ClassificarPiso(P["razao"])
        Medido = (f"{P['razao']} (piso {P['piso_ms']} ms / t {P['t_ms']} ms; serial {P['serial_ms']} + paralelo max(critico_j1 {P['critico_j1_ms']}, trabalho_j1 {P['trabalho_j1_ms']}/N efetivo {P['n_efetivo']}) + exe {P['exe_ms']}; "
                  f"contencao {P['contencao']}x; razoes {P['razao_min']}..{P['razao_max']}; IQR/mediana {P['iqr_rel']})" + (" FALHA modelo: piso > 1.05 x t" if Veredito == "modelo" else ""))
        Lista.append(Aceite(f"A3N largo: piso(j{Jobs}) / t(j{Jobs})", Medido, f">= {FRACAO_PISO} e <= {LIMITE_MODELO}", Veredito == "ok", P["conclusivo"]))
    return Lista


# cenarios do A8 que ja saem das medicoes incrementais do A6: (projeto, cenario); o monolito do corpus_200000 e o config "monolito" do largo (A7)
CENARIOS_A8 = (("largo", "corpo"), ("proj250", "corpo"), ("proj250", "struct"))


def MedidasA8(Dados):
    """[(rotulo, mediana_ms, conclusivo)] dos cenarios do A8: incrementais de CENARIOS_A8 e o build monolitico do corpus_200000."""
    Medidas = []
    for Tipo, Cenario in CENARIOS_A8:
        Item = next(I for I in Dados["incrementais"] if I["projeto"] == Tipo and I["cenario"] == Cenario)
        Medidas.append((f"{Tipo} {Cenario}", Item["mediana_ms"], Item["iqr_rel"] <= LIMITE_DISPERSAO))
    Mono = Dados["jobs"]["largo"]["configs"]["monolito"]
    return Medidas + [("monolito corpus_200000", Mono["mediana_ms"], Mono["iqr_rel"] <= LIMITE_DISPERSAO)]


def AvaliarA8(Dados):
    """A8 (ADR-37 / v2 6.2): mudanca abaixo de 1 s = parede (mediana) <= META_A8_MS em cada cenario de MedidasA8."""
    return [Aceite(f"A8 mudanca < 1 s: {Rotulo}", f"{Ms} ms", f"<= {META_A8_MS:.0f} ms", Ms <= META_A8_MS, Conclusivo) for Rotulo, Ms, Conclusivo in MedidasA8(Dados)]


# Criterios dos cenarios incrementais, uma linha por cenario: (cenario, criterio com {Tipo}, funcao que aceita o Item, meta exibida).
# A5 e so o no-op (nada recompila); o resto e A6. A contagem estavel entre amostras e exigida em todos (AceiteIncremental).
REGRAS_INCREMENTAIS = [
    ("noop", "A5 no-op {Tipo}: mediana ms e filhos", lambda I: I["mediana_ms"] <= META_NOOP_MS and I["contagem"]["filhos"] == 0, f"<= {META_NOOP_MS} ms e filhos 0"),
    ("corpo", "A6 corpo de 1 dll ({Tipo})", lambda I: I["contagem"]["recompilados"] == 1 and "fonte mudou" in I["contagem"]["motivos"][0], "1 recompilado (fonte mudou)"),
    ("struct", "A6 struct da base ({Tipo})", lambda I: I["contagem"]["recompilados"] == 10, "10 (base + 8 interface de base mudou + exe)"),
    ("assets", "A6 asset {Tipo}", lambda I: I["contagem"]["recompilados"] == 0 and I["contagem"]["filhos"] == 0, "0"),
]


def AceiteIncremental(Item, Regra, Ruidoso):
    """Aceite de um cenario incremental pela sua linha em REGRAS_INCREMENTAIS; contagem instavel entre as amostras reprova o criterio, com a mensagem."""
    Cenario, Criterio, Teste, Meta = Regra
    Contagem = Item["contagem"]
    Medido = f"{Item['mediana_ms']} ms, {Contagem['recompilados']} recompilados, filhos {Contagem['filhos']}"
    if not Item["contagem_estavel"]:
        Medido += " (contagem instavel entre amostras)"
    Ok = Teste(Item) and Item["contagem_estavel"]
    return Aceite(Criterio.format(Tipo=Item["projeto"]), Medido, Meta, Ok, not (Cenario == "noop" and Ruidoso))


def AvaliarIncrementais(Dados):
    """Um Aceite por medicao incremental, achando a linha de cada cenario em REGRAS_INCREMENTAIS."""
    Ruidoso = Dados["ruido_incremental"] > Dados["ruido_limite"]
    Lista = []
    for Item in Dados["incrementais"]:
        Regra = next(Linha for Linha in REGRAS_INCREMENTAIS if Linha[0] == Item["cenario"])
        Lista.append(AceiteIncremental(Item, Regra, Ruidoso))
    return Lista


def JobsPadraoRecomendado(Dados, Nucleos):
    """O contrato: j12 mais de 5% mais lento que j6 no largo => o padrao vira nucleos fisicos (e o relatorio grava o motivo)."""
    T6, T12 = Mediana(Dados, "largo", "j6"), Mediana(Dados, "largo", "j12")
    Pior = T12 > LIMITE_J12_CONTRA_J6 * T6
    Motivo = f"largo: j12 {T12} ms x j6 {T6} ms = {round(100.0 * (T12 / T6 - 1.0), 1)}% " + ("mais lento (> 5%): padrao = nucleos fisicos" if Pior else "(<= 5% mais lento): padrao = processadores logicos")
    return {"padrao": "nucleos fisicos" if Pior else "processadores logicos", "nucleos_fisicos": Nucleos, "motivo": Motivo}


def NEfetivoDaCalibracao(Calibracao, N):
    """N efetivo (coluna n_efetivo da tabela de CalibrarParalelismo) para N processos."""
    return next(Linha["n_efetivo"] for Linha in Calibracao["tabela"] if Linha["n"] == N)


def UniaoUs(Intervalos):
    """Comprimento (us) da uniao de intervalos (ini, fim): trechos sobrepostos contam uma vez so."""
    Total, Fim = 0, None
    for Ini, Final in sorted(Intervalos):
        if Fim is None or Ini > Fim:
            Total += Final - Ini
            Fim = Final
        elif Final > Fim:
            Total += Final - Fim
            Fim = Final
    return Total


def VidasDaRodada(Arquivo):
    """[(produto, ini_us, us)] das linhas vida_do_filho do jsonl do FORJA_ORQ_TEMPOS; sem a linha ou com ini_us ausente/negativo (forja sem o instante) levanta ValueError."""
    Vidas = []
    for Linha in Arquivo.read_text(encoding="utf-8").splitlines():
        Item = json.loads(Linha) if Linha.strip() else {}
        if Item.get("etapa") == "vida_do_filho":
            Vidas.append((Item["produto"], Item.get("ini_us", -1), Item["us"]))
    if not Vidas or any(Ini < 0 for _, Ini, _ in Vidas):
        raise ValueError(f"{Arquivo}: vida_do_filho sem ini_us valido ({len(Vidas)} linhas); o forja e anterior ao orquestrar_tempos 0.2.0?")
    return Vidas


def BaseDeJ1(RodadasJ1):
    """Base sem contencao (D-M4-5 revisado): mediana, por dll, das vidas das rodadas j1 [(t_us, vidas)]; {trabalho_us: soma das dlls, critico_us: a maior, t_us: mediana de t(j1)}."""
    PorDll = {}
    for _, Vidas in RodadasJ1:
        for Produto, _, Us in Vidas:
            if Produto != EXE_DO_LARGO:
                PorDll.setdefault(Produto, []).append(Us)
    Medianas = [statistics.median(Valores) for Valores in PorDll.values()]
    return {"trabalho_us": sum(Medianas), "critico_us": max(Medianas), "t_us": statistics.median(T for T, _ in RodadasJ1)}


def PisoDaRodada(Vidas, ParedeUs, NEfetivo, BaseJ1):
    """Piso fisico de UMA rodada jN (D-M4-5 revisado): serial_nao_sobreposto(jN) (parede menos a uniao das vidas de todos os filhos) + max(critico_j1, trabalho_j1 / N efetivo)
    + vida do exe final (jN); critico_j1/trabalho_j1 vem da BaseDeJ1 (sem contencao: a contencao ja esta no N efetivo). Devolve dict em us: serial_us, trabalho_us e critico_us
    (da propria rodada jN, informativos), exe_us, piso_us."""
    Dlls = [Us for Produto, _, Us in Vidas if Produto != EXE_DO_LARGO]
    ExeUs = sum(Us for Produto, _, Us in Vidas if Produto == EXE_DO_LARGO)
    if not Dlls:
        raise ValueError("rodada sem nenhuma vida de dll")
    Serial = ParedeUs - UniaoUs([(Ini, Ini + Us) for _, Ini, Us in Vidas])
    Paralelo = max(BaseJ1["critico_us"], BaseJ1["trabalho_us"] / NEfetivo)
    return {"serial_us": Serial, "trabalho_us": sum(Dlls), "critico_us": max(Dlls), "exe_us": ExeUs, "piso_us": Serial + Paralelo + ExeUs}


def RodadaDePiso(Contexto, Pasta, Jobs):
    """Uma rodada fria do largo em --jobs Jobs com FORJA_ORQ_TEMPOS: (t(jN) em us, vidas)."""
    Arquivo = Contexto.Trabalho / f"piso_j{Jobs}.jsonl"
    Limpar(Pasta)
    Arquivo.unlink(missing_ok=True)
    Env = {**os.environ, "FORJA_ORQ_TEMPOS": str(Arquivo)}
    Ms = ExigirOk(Cronometrar([Contexto.Forja, "build", "--projeto", Pasta / "forja.proj", "--jobs", Jobs], Pasta, Env), f"piso j{Jobs}").ms
    return Ms * 1000.0, VidasDaRodada(Arquivo)


def ResumirPiso(Rodadas, NEfetivo, BaseJ1):
    """Rodadas [(t_us, piso, vidas)] -> medianas em ms de t/piso/serial/trabalho/critico/exe (trabalho e critico da propria rodada jN), a base j1 (trabalho_j1_ms, critico_j1_ms, t_j1_ms),
    a contencao = trabalho_jN / trabalho_j1 (informativa), razao piso/t por rodada (mediana, min..max, IQR/mediana, conclusivo) e as vidas da ultima rodada."""
    Razoes = [P["piso_us"] / T for T, P, _ in Rodadas]

    def Ms(Valores):
        return round(statistics.median(Valores) / 1000.0, 1)
    Dispersao = IqrRelativo(Razoes)
    Resumo = {"n_efetivo": NEfetivo, "t_ms": Ms([T for T, _, _ in Rodadas]), "razao": round(statistics.median(Razoes), 4), "razao_min": round(min(Razoes), 4), "razao_max": round(max(Razoes), 4),
              "iqr_rel": round(Dispersao, 4), "conclusivo": Dispersao <= LIMITE_DISPERSAO,
              "vidas_ultima_rodada": [{"produto": Produto, "ini_us": Ini, "us": Us} for Produto, Ini, Us in Rodadas[-1][2]]}
    for Chave in ("piso", "serial", "trabalho", "critico", "exe"):
        Resumo[f"{Chave}_ms"] = Ms([P[f"{Chave}_us"] for _, P, _ in Rodadas])
    Resumo.update({"trabalho_j1_ms": Ms([BaseJ1["trabalho_us"]]), "critico_j1_ms": Ms([BaseJ1["critico_us"]]), "t_j1_ms": Ms([BaseJ1["t_us"]]),
                   "contencao": round(statistics.median(P["trabalho_us"] for _, P, _ in Rodadas) / BaseJ1["trabalho_us"], 3)})
    return Resumo


def MedirRodadas(Contexto, Pasta, Jobs):
    """Aquecimentos rodadas descartadas e Amostras medidas do largo em jN: [(t_us, vidas)]; cada amostra e uma RodadaDePiso (refeita se ruidosa com --exigir-repouso)."""
    Rodadas = []
    for Numero in range(Contexto.Aquecimentos + Contexto.Amostras):
        Rodada = AmostraComRepouso(Contexto, lambda: RodadaDePiso(Contexto, Pasta, Jobs))
        if Numero >= Contexto.Aquecimentos:
            Rodadas.append(Rodada)
    return Rodadas


def MedirPisoDeJobs(Contexto, Pasta, Jobs, BaseJ1):
    """Rodadas do largo em jN e seus pisos (cada rodada contra a mesma BaseJ1); o N efetivo vem da calibracao guardada em BaseJ1['n_efetivo'][Jobs]."""
    NEfetivo = BaseJ1["n_efetivo"][Jobs]
    Rodadas = [(T, PisoDaRodada(Vidas, T, NEfetivo, BaseJ1), Vidas) for T, Vidas in MedirRodadas(Contexto, Pasta, Jobs)]
    return ResumirPiso(Rodadas, NEfetivo, BaseJ1)


def MedirPiso(Contexto, Pasta, Calibracao):
    """Piso fisico MEDIDO do largo em j6 e j12 (A3N, D-M4-5 revisado): primeiro uma rodada j1 com FORJA_ORQ_TEMPOS (BaseDeJ1: trabalho e critico sem contencao), depois {jN: ResumirPiso}.
    O N efetivo de cada jN vem da Calibracao (nao do numero de logicos)."""
    BaseJ1 = BaseDeJ1(MedirRodadas(Contexto, Pasta, 1))
    BaseJ1["n_efetivo"] = {Jobs: NEfetivoDaCalibracao(Calibracao, Jobs) for Jobs in (6, 12)}
    return {f"j{Jobs}": MedirPisoDeJobs(Contexto, Pasta, Jobs, BaseJ1) for Jobs in (6, 12)}


def MedirTempos(Contexto, Pastas):
    Dados = {"jobs": {}, "incrementais": []}
    for Tipo in ("largo", "proj250"):
        print(f"[tempos] {Tipo}: varredura de jobs ...", flush=True)
        Dados["jobs"][Tipo] = MedirJobs(Contexto, Pastas[Tipo], Tipo)
    Dados["a2"] = {}
    for Tipo in ("largo", "proj250"):
        print(f"[tempos] {Tipo}: A2 pareado ({Contexto.Pares} pares ABBA) ...", flush=True)
        Dados["a2"][Tipo] = MedirA2(Contexto, Pastas[Tipo])
    print("[tempos] calibracao do paralelismo real (N efetivo) ...", flush=True)
    Dados["calibracao"] = CalibrarParalelismo(Contexto)
    print("[tempos] largo: piso fisico de j6 e j12 (FORJA_ORQ_TEMPOS com ini_us) ...", flush=True)
    Dados["piso"] = MedirPiso(Contexto, Pastas["largo"], Dados["calibracao"])
    Ruido = []
    Cenarios = [("largo", "noop"), ("largo", "corpo"), ("largo", "assets"), ("proj250", "noop"), ("proj250", "corpo"), ("proj250", "struct"), ("proj250", "assets")]
    for Tipo, Cenario in Cenarios:
        print(f"[tempos] {Tipo}: incremental {Cenario} ...", flush=True)
        Ruido.append(OcupacaoDaCpu(1.0))
        Dados["incrementais"].append(MedirIncremental(Contexto, Pastas[Tipo], Tipo, Cenario))
    Dados["ruido_incremental"] = round(statistics.mean(Ruido), 1)
    return Dados


def ImprimirA1(Itens):
    for Item in Itens:
        print(f"A1 {Item['projeto']}: {Item['arquivos']} arquivos, rodadas j12 identicas={all(not R['diferencas'] and R['estado_igual'] for R in Item['rodadas'])}, programa={Item['programa']} -> {'OK' if Item['ok'] else 'VERMELHO'}")


def ImprimirTempos(Tempos, Piso):
    """Tabelas de jobs, piso e incrementais; a linha de jobs sai RUIDOSO quando o ruido medio passa de ruido_limite."""
    for Tipo, Bloco in Tempos["jobs"].items():
        print(f"\n{Tipo}  (ruido medio {Bloco['ruido']['medio_pct']}% max {Bloco['ruido']['max_pct']}%{'  RUIDOSO' if Bloco['ruido']['medio_pct'] > Tempos['ruido_limite'] else ''})")
        for Nome, C in Bloco["configs"].items():
            print(f"  {Nome:9s} mediana {C['mediana_ms']:9.1f} ms  min {C['min_ms']:9.1f}  max {C['max_ms']:9.1f}  disp {C['dispersao_pct']:5.1f}%  jobs={C.get('jobs_efetivos', '-')}")
    for Nome, P in Piso.items():
        print(f"\nPiso largo {Nome}: t {P['t_ms']} ms, piso {P['piso_ms']} ms (serial {P['serial_ms']} + paralelo max(critico_j1 {P['critico_j1_ms']}, trabalho_j1 {P['trabalho_j1_ms']} / N efetivo {P['n_efetivo']}) "
              f"+ exe {P['exe_ms']}), t(j1) {P['t_j1_ms']} ms, contencao {P['contencao']}x, razao piso/t {P['razao']} ({P['razao_min']}..{P['razao_max']}, IQR/med {P['iqr_rel']})")
    print("Calibracao (processos CPU-bound simultaneos): " + "; ".join(f"N={L['n']} {L['mediana_ms']} ms speedup {L['speedup']}" for L in Tempos["calibracao"]["tabela"]))
    for Tipo, A2 in Tempos["a2"].items():
        print(f"A2 {Tipo}: mediana das razoes {A2['mediana_das_razoes']} razoes-das-medianas {A2['razao_das_medianas']} min..max {A2['razao_min']}..{A2['razao_max']} IQR/med {A2['iqr_rel']}"
              f"{'' if A2['conclusivo'] else '  DISPERSAO_ALTA'}")
    for Item in Tempos["incrementais"]:
        print(f"  incremental {Item['projeto']:7s} {Item['cenario']:7s} mediana {Item['mediana_ms']:8.1f} ms  recompilados {Item['contagem']['recompilados']} filhos {Item['contagem']['filhos']}")

def ImprimirTemposOrq(Tempos):
    for Etapa, Item in Tempos.items():
        print(f"  orquestrador {Etapa}: mediana {Item['mediana_us']} us (n={Item['n']})")


def ImprimirAceites(Lista):
    for Item in Lista:
        Marca = "OK " if Item["ok"] else "FALHA"
        print(f"{Marca} {'' if Item['conclusivo'] else '[dispersao_alta] '}{Item['aceite']}: {Item['medido']} (meta {Item['meta']})")


def ImprimirTabelas(Saida):
    if "a1" in Saida:
        ImprimirA1(Saida["a1"])
    if "tempos" in Saida:
        ImprimirTempos(Saida["tempos"], Saida["piso"])
    if "tempos_orq" in Saida:
        ImprimirTemposOrq(Saida["tempos_orq"])
    ImprimirAceites(Saida.get("aceites", []))


def ContextoDe(Args, Forja, Trabalho):
    """ContextoMedicao das opcoes da linha de comando; --tempos-orq aponta o jsonl do forja para a pasta temporaria do script."""
    TemposOrq = (Trabalho / "tempos_orq.jsonl") if Args.TemposOrq else None
    return ContextoMedicao(Forja, Trabalho, Args.amostras or AMOSTRAS, Args.aquecimentos, Args.ExigirRepouso, TemposOrq, Pares=Args.amostras or PARES_A2)


def MedirEtapas(Args, Contexto, Pastas):
    """As etapas pedidas (--tudo ou --so a1|tempos), no formato que vai para o JSON."""
    Saida = {}
    if Args.tudo or Args.so == "a1":
        Saida["a1"] = [MedirA1(Contexto, Pastas[Tipo], Tipo) for Tipo in ("largo", "proj250")]
    if Args.tudo or Args.so == "tempos":
        Saida["tempos"] = MedirTempos(Contexto, Pastas)
        Saida["tempos"].update({"ruido_limite": RUIDO_LIMITE})
        Saida["piso"] = Saida["tempos"].pop("piso")
        Saida["jobs"] = Saida["tempos"]["jobs"]
    return Saida


def AgregarTemposOrq(Arquivo):
    """{etapa: {mediana_us, n}} do jsonl do forja (uma linha JSON por etapa, com 'etapa' e 'us'); {} se o arquivo nao existe ou nao tem linhas."""
    if not Arquivo.is_file():
        return {}
    Por = {}
    for Linha in Arquivo.read_text(encoding="utf-8").splitlines():
        if Linha.strip():
            Item = json.loads(Linha)
            Por.setdefault(Item["etapa"], []).append(Item["us"])
    return {Etapa: {"mediana_us": statistics.median(Valores), "n": len(Valores)} for Etapa, Valores in Por.items()}


def RegistrarTemposOrq(Saida, Arquivo):
    """--tempos-orq: a mediana por etapa vai para Saida['tempos_orq']; sem linhas so avisa (o forja ainda nao grava FORJA_ORQ_TEMPOS)."""
    Tempos = AgregarTemposOrq(Arquivo)
    if Tempos:
        Saida["tempos_orq"] = Tempos
    else:
        print("sem tempos do orquestrador (forja sem FORJA_ORQ_TEMPOS)", flush=True)


def PlanoDeEnergia():
    """Saida de 'powercfg /getactivescheme'. O powercfg escreve na codepage OEM (cp850 no PT-BR); se falhar, o motivo vai no lugar do texto."""
    try:
        Feito = subprocess.run(["powercfg", "/getactivescheme"], capture_output=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as Falha:
        return f"indisponivel: {Falha}"
    if Feito.returncode != 0:
        return f"powercfg saiu com {Feito.returncode}: {Feito.stderr.decode('cp850', 'replace').strip()}"
    return Feito.stdout.decode("cp850", "replace").strip()


def Estatisticas(Amostras):
    """min, mediana, max e desvio relativo (desvio padrao populacional / media, em %) de uma lista de amostras em ms."""
    Media = statistics.mean(Amostras)
    return {"min_ms": round(min(Amostras), 1), "mediana_ms": round(statistics.median(Amostras), 1), "max_ms": round(max(Amostras), 1),
            "desvio_relativo_pct": round(100.0 * statistics.pstdev(Amostras) / Media, 1) if Media else 0.0}


def Dispersoes(Saida):
    """Dispersao de cada cenario medido: as configs do jobs (por projeto) e os incrementais."""
    if "tempos" not in Saida:
        return {}
    Achados = {}
    for Tipo, Bloco in Saida["tempos"]["jobs"].items():
        for Nome, C in Bloco["configs"].items():
            Achados[f"{Tipo} {Nome}"] = Estatisticas(C["amostras_ms"])
    for Item in Saida["tempos"]["incrementais"]:
        Achados[f"incremental {Item['projeto']} {Item['cenario']}"] = Estatisticas(Item["amostras_ms"])
    return Achados


def BlocoRepouso(Contexto, Saida):
    """Bloco 'repouso' do JSON: plano de energia ativo, ruido (%) de cada tentativa refeita e a dispersao de cada cenario."""
    return {"plano_energia": PlanoDeEnergia(), "refacoes": len(Contexto.Refeitas), "ruido_refeitas_pct": Contexto.Refeitas, "dispersao": Dispersoes(Saida)}


def ExecutarEtapas(Args, Forja, Trabalho):
    Contexto = ContextoDe(Args, Forja, Trabalho)
    Pastas = {}
    for Tipo in ("largo", "proj250"):
        Pastas[Tipo] = Trabalho / Tipo
        gerar_projeto.Gerar(Tipo, Pastas[Tipo])
    Saida = {"ruido_limite": RUIDO_LIMITE, "ruido_inicial_pct": round(OcupacaoDaCpu(10.0), 1)}
    print(f"[ruido] CPU ocupada por outros nos 10 s iniciais: {Saida['ruido_inicial_pct']}%", flush=True)
    Saida.update(MedirEtapas(Args, Contexto, Pastas))
    # so informativo: a medicao pareada nao refaz amostra por causa do fundo
    Saida["ruido_fundo_pct"] = {"antes": Saida["ruido_inicial_pct"], "depois": round(OcupacaoDaCpu(5.0), 1)}
    if Contexto.ExigirRepouso:
        Saida["repouso"] = BlocoRepouso(Contexto, Saida)
    if Contexto.TemposOrq is not None:
        RegistrarTemposOrq(Saida, Contexto.TemposOrq)
    return Saida


def FinalizarAceites(Saida, Nucleos):
    Aceites = [Aceite(f"A1 determinismo {Item['projeto']}", f"{len(Item['rodadas'])} rodadas j12 = j1, programa = esperado", "100%", Item["ok"], True) for Item in Saida.get("a1", [])]
    if "tempos" in Saida:
        Dados = {"jobs": Saida["tempos"]["jobs"], "a2": Saida["tempos"]["a2"], "piso": Saida["piso"], "incrementais": Saida["tempos"]["incrementais"], "ruido_incremental": Saida["tempos"]["ruido_incremental"], "ruido_limite": RUIDO_LIMITE}
        Aceites += AvaliarTempos(Dados)
        Saida["a8"] = {"meta_ms": META_A8_MS, "cenarios": [{"cenario": Rotulo, "mediana_ms": Ms, "ok": Ms <= META_A8_MS, "conclusivo": Conclusivo} for Rotulo, Ms, Conclusivo in MedidasA8(Dados)]}
        Saida["jobs_padrao_recomendado"] = JobsPadraoRecomendado(Dados, Nucleos)
    Saida["aceites"] = Aceites


def AmostraValida(Texto):
    """Tipo do --amostras: inteiro >= 1 (zero nao mede nada e a mediana de lista vazia quebra). O erro vira codigo 2 no argparse."""
    Valor = int(Texto)
    if Valor < 1:
        raise argparse.ArgumentTypeError(f"--amostras precisa ser >= 1 (recebi {Valor})")
    return Valor


def ValidarArgumentos(Parser, Args):
    """Erros de uso saem pelo parser (codigo 2): precisa de etapa, de --forja e de uma COPIA fora de language\\compiler. Devolve o Path do forja."""
    if not Args.tudo and not Args.so:
        Parser.error("informe --tudo ou --so a1|tempos")
    if not Args.forja:
        Parser.error("informe --forja <COPIA do forja.exe> (ou --selftest)")
    Forja = Path(Args.forja).resolve()
    if not Forja.is_file() or RAIZ_LINGUAGEM / "compiler" in Forja.parents:
        Parser.error("--forja precisa ser uma COPIA existente do forja.exe, fora de language\\compiler")
    return Forja


def GravarJson(Saida):
    """Grava o resultado em tests\\projeto\\resultados (nome pela hora) e devolve o caminho."""
    Pasta = PASTA_TESTES / "resultados"
    Pasta.mkdir(exist_ok=True)
    Arquivo = Pasta / (datetime.now().strftime("%Y-%m-%d_%H%M%S") + ".json")
    Arquivo.write_text(json.dumps(Saida, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return Arquivo


def GravarNaoConclusivo(Args, Motivo):
    """Sai com codigo 2: o JSON leva 'resultado': 'nao_conclusivo' e o motivo, para nao ser lido como medicao que passou ou falhou."""
    print(f"NAO CONCLUSIVO: {Motivo}", file=sys.stderr, flush=True)
    Saida = {"versao": VERSAO, "data": datetime.now().isoformat(timespec="seconds"), "resultado": "nao_conclusivo", "motivo": Motivo,
             "exigir_repouso": True, "amostras": Args.amostras, "aquecimentos": Args.aquecimentos}
    print(f"JSON: {GravarJson(Saida)}")
    return 2


def SelftestAmostras():
    """--amostras 0 sai com codigo 2 pelo argparse, antes de qualquer forja (o --forja aqui e so o proprio python, para passar do parse)."""
    try:
        Main(["--forja", sys.executable, "--tudo", "--amostras", "0"])
        Codigo = 0
    except SystemExit as Encerrou:
        Codigo = Encerrou.code
    Casos = [("--amostras 0 deveria sair com codigo 2", Codigo == 2)]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def SelftestNucleos():
    """NucleosFisicos sem powershell devolve None (o JSON grava null) e avisa em stderr; o subprocess.run original volta no fim."""
    Original = subprocess.run

    def Falhar(*Argumentos, **Opcoes):
        raise OSError("powershell indisponivel (selftest)")

    subprocess.run = Falhar
    try:
        Valor = NucleosFisicos()
    finally:
        subprocess.run = Original
    Casos = [("NucleosFisicos sem powershell deveria devolver None", Valor is None)]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def SelftestIncrementais():
    """Tabela de AvaliarIncrementais: no-op e corpo felizes passam; corpo com 2 recompilados e corpo com contagem instavel falham."""
    def Item(Cenario, Recompilados, Estavel=True):
        Motivos = ["parte_7: fonte mudou"] * Recompilados if Cenario == "corpo" else []
        return {"projeto": "largo", "cenario": Cenario, "mediana_ms": 10.0, "amostras_ms": [10.0], "contagem_estavel": Estavel,
                "contagem": {"recompilados": Recompilados, "filhos": 0, "motivos": Motivos}}
    Dados = {"incrementais": [Item("noop", 0), Item("corpo", 1), Item("corpo", 2), Item("corpo", 1, Estavel=False)], "ruido_incremental": 0.0, "ruido_limite": RUIDO_LIMITE}
    Lista = AvaliarIncrementais(Dados)
    Casos = [
        ("A5 no-op feliz deveria passar", Lista[0]["ok"] is True),
        ("A6 corpo feliz deveria passar", Lista[1]["ok"] is True),
        ("A6 corpo com 2 recompilados deveria falhar", Lista[2]["ok"] is False),
        ("A6 corpo com contagem instavel deveria falhar com a mensagem", Lista[3]["ok"] is False and "instavel" in Lista[3]["medido"]),
    ]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def SelftestTemposOrq():
    """AgregarTemposOrq: mediana e contagem por etapa de um jsonl sintetico; arquivo ausente ou so com linhas em branco devolve {}."""
    with tempfile.TemporaryDirectory(prefix="medir_projeto_") as Temp:
        Arquivo = Path(Temp) / "tempos.jsonl"
        Linhas = [{"etapa": "parse", "us": 100}, {"etapa": "parse", "us": 300, "produto": "a"}, {"etapa": "parse", "us": 200}, {"etapa": "gera", "us": 50}]
        Arquivo.write_text("\n".join(json.dumps(Linha) for Linha in Linhas) + "\n", encoding="utf-8")
        Agregado = AgregarTemposOrq(Arquivo)
        Vazio = Path(Temp) / "vazio.jsonl"
        Vazio.write_text("\n", encoding="utf-8")
        Casos = [
            ("parse deveria ter mediana 200 com n=3", Agregado.get("parse") == {"mediana_us": 200, "n": 3}),
            ("gera deveria ter mediana 50 com n=1", Agregado.get("gera") == {"mediana_us": 50, "n": 1}),
            ("arquivo so com linha em branco deveria devolver {}", AgregarTemposOrq(Vazio) == {}),
            ("arquivo ausente deveria devolver {}", AgregarTemposOrq(Path(Temp) / "nao_existe.jsonl") == {}),
        ]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def SelftestRepouso():
    """EsperarRepouso com limite curto injetado tem de levantar NaoConclusivo, sem esperar a janela de 3 s de calma."""
    Levantou = False
    try:
        EsperarRepouso(Limite=0.01)
    except NaoConclusivo:
        Levantou = True
    Casos = [("EsperarRepouso com limite de 0,01 s deveria levantar NaoConclusivo", Levantou)]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def SelftestSentinela():
    """AmostraComRepouso: fundo calmo nas duas janelas aceita de 1a; sentinela ocupada refaz e registra o ruido; sempre ocupada para apos REFACOES_MAXIMAS refacoes."""
    Original = globals()["OcupacaoDaCpu"]
    Chamadas, Leituras = [], []

    def Falsa(Segundos):
        return Leituras.pop(0)

    def Executar():
        Chamadas.append(1)
        return 42.0

    globals()["OcupacaoDaCpu"] = Falsa
    try:
        Contexto = ContextoMedicao(Path("."), Path("."), 1, 0, ExigirRepouso=True)
        Leituras[:] = [1.0, 2.0]
        Calmo = (AmostraComRepouso(Contexto, Executar), len(Chamadas), len(Contexto.Refeitas))
        Leituras[:] = [1.0, 30.0, 1.0, 2.0]
        Chamadas.clear()
        Refeito = (AmostraComRepouso(Contexto, Executar), len(Chamadas), Contexto.Refeitas[-1])
        Leituras[:] = [30.0, 1.0] * (REFACOES_MAXIMAS + 1)
        Chamadas.clear()
        AmostraComRepouso(Contexto, Executar)
        Sempre = len(Chamadas)
    finally:
        globals()["OcupacaoDaCpu"] = Original
    Casos = [("fundo calmo deveria aceitar de 1a sem refazer", Calmo == (42.0, 1, 0)),
             ("sentinela ocupada deveria refazer 1x e registrar 30.0", Refeito == (42.0, 2, 30.0)),
             ("ruido permanente deveria parar apos as refacoes maximas", Sempre == REFACOES_MAXIMAS + 1)]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def DadosDePisoSintetico(Vidas, ParedeUs, NEfetivo, BaseJ1):
    """Dados['piso'] (j6 e j12) de 3 rodadas identicas feitas dos intervalos sinteticos Vidas e da base j1 BaseJ1, para exercitar o AvaliarA3N de verdade."""
    Rodada = (ParedeUs, PisoDaRodada(Vidas, ParedeUs, NEfetivo, BaseJ1), Vidas)
    return {"piso": {f"j{Jobs}": ResumirPiso([Rodada] * 3, NEfetivo, BaseJ1) for Jobs in (6, 12)}}


def SelftestPiso():
    """PisoDaRodada com intervalos sinteticos (us) e base j1 sintetica: sobrepostos, disjuntos, filho unico, base j1 correta (razao em [0,8; 1,05] com contencao em jN) e um modelo errado
    (piso > 1,05 x t) que TEM de falhar; sem ini_us o forja velho e recusado."""
    Sobrepostos = [("a", 0, 100), ("b", 50, 100), (EXE_DO_LARGO, 150, 50)]
    Disjuntos = [("a", 0, 100), ("b", 120, 100), (EXE_DO_LARGO, 230, 50)]
    Unico = [("a", 10, 100), (EXE_DO_LARGO, 120, 50)]
    Base2, Base1, Base4 = BaseDeJ1([(200, [("a", 0, 100), ("b", 0, 100)])]), {"trabalho_us": 200, "critico_us": 100, "t_us": 300}, {"trabalho_us": 100, "critico_us": 100, "t_us": 180}
    PSobre, PDisj, PUnico = PisoDaRodada(Sobrepostos, 210, 2, Base2), PisoDaRodada(Disjuntos, 300, 1, Base1), PisoDaRodada(Unico, 180, 4, Base4)
    # base j1 correta: as 2 dlls levam 100 us cada sozinhas; em jN a contencao as estica para 120 us (trabalho_jN 240, contencao 1,2x) e o piso fica 10 + max(100, 200 / 2) + 50 = 160 contra t 180
    Contida = [("a", 0, 120), ("b", 0, 120), (EXE_DO_LARGO, 125, 50)]
    Certo = AvaliarA3N(DadosDePisoSintetico(Contida, 180, 2, Base2))
    # modelo errado: base j1 inflada (trabalho 500, critico 250: contencao ja contada) com as mesmas dlls sobrepostas -> piso 10 + 250 + 50 contra t 210
    Errado = AvaliarA3N(DadosDePisoSintetico(Sobrepostos, 210, 2, {"trabalho_us": 500, "critico_us": 250, "t_us": 600}))
    with tempfile.TemporaryDirectory(prefix="medir_projeto_") as Temp:
        Velho = Path(Temp) / "velho.jsonl"
        Velho.write_text(json.dumps({"etapa": "vida_do_filho", "us": 5, "produto": "a"}) + "\n", encoding="utf-8")
        try:
            VidasDaRodada(Velho)
            Recusou = False
        except ValueError:
            Recusou = True
    Casos = [("uniao de intervalos sobrepostos [0,100] [50,150] deveria ser 150 e a disjunta 200", UniaoUs([(0, 100), (50, 150)]) == 150 and UniaoUs([(0, 100), (120, 220)]) == 200),
             ("sobrepostos: serial 10, piso 160 (10 + max(100, 200/2) + 50)", PSobre["serial_us"] == 10 and PSobre["piso_us"] == 160),
             ("disjuntos com N efetivo 1: serial 50, piso 300 (50 + max(100, 200) + 50)", PDisj["serial_us"] == 50 and PDisj["piso_us"] == 300),
             ("filho unico: serial 30, trabalho = critico = 100, piso 180", PUnico["serial_us"] == 30 and PUnico["trabalho_us"] == 100 and PUnico["critico_us"] == 100 and PUnico["piso_us"] == 180),
             ("base j1 correta com contencao 1,2x em jN: piso 160 contra t 180 (razao 0.889, contencao 1.2) deveria passar",
              all(A["ok"] and "contencao 1.2x" in A["medido"] and "0.8889" in A["medido"] for A in Certo)),
             ("modelo errado: base j1 inflada (piso 310 contra t 210, razao 1.476 > 1.05) deveria FALHAR com 'modelo'", all(not A["ok"] and "modelo" in A["medido"] for A in Errado)),
             ("ClassificarPiso: 0.79 abaixo, 0.8 ok, 1.05 ok, 1.06 modelo", [ClassificarPiso(R) for R in (0.79, 0.8, 1.05, 1.06)] == ["abaixo", "ok", "ok", "modelo"]),
             ("jsonl sem ini_us deveria ser recusado", Recusou)]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def SelftestA8():
    """AvaliarA8: 1000 ms passa, 1000,1 ms falha, nos incrementais e no monolito do corpus_200000."""
    def Item(Tipo, Cenario, Ms):
        return {"projeto": Tipo, "cenario": Cenario, "mediana_ms": Ms, "iqr_rel": 0.01}

    def Dados(Corpo, Monolito):
        return {"incrementais": [Item("largo", "corpo", Corpo), Item("proj250", "corpo", 500.0), Item("proj250", "struct", 900.0)],
                "jobs": {"largo": {"configs": {"monolito": {"mediana_ms": Monolito, "iqr_rel": 0.01}}}}}
    Passa, Falha = AvaliarA8(Dados(1000.0, 780.0)), AvaliarA8(Dados(1000.1, 1000.1))
    Casos = [("A8 com 1000 ms em tudo ate o limite deveria passar os 4 cenarios", len(Passa) == 4 and all(A["ok"] for A in Passa)),
             ("A8 com largo corpo e monolito em 1000,1 ms deveria falhar so esses dois", [A["ok"] for A in Falha] == [False, True, True, False])]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def SelftestPareado():
    """ABBA alterna a ordem; ResumirPares (mediana das razoes x razao das medianas, conclusivo/dispersao_alta); NEfetivo e teto a partir de speedups sinteticos."""
    Ordens = [OrdemDoPar(K) for K in range(4)]
    Pares = [{"script_ms": 100.0, "orq_ms": R * 100.0, "primeiro": "script"} for R in (1.00, 1.02, 1.04, 1.50, 1.03)]
    Resumo = ResumirPares(Pares)
    Largos = ResumirPares([{"script_ms": 100.0, "orq_ms": R * 100.0, "primeiro": "orq"} for R in (0.5, 1.0, 1.5, 2.0, 2.5)])
    Speedups = SpeedupsDe({1: 500.0, 2: 510.0, 6: 520.0, 12: 1000.0})
    Casos = [("ordem deveria ser ABBA (script/orq, orq/script, ...)", Ordens[0] == ("script", "orq") and Ordens[1] == ("orq", "script") and Ordens[2] == Ordens[0] and Ordens[3] == Ordens[1]),
             ("mediana das razoes deveria ser 1.03", Resumo["mediana_das_razoes"] == 1.03 and Resumo["razao_min"] == 1.0 and Resumo["razao_max"] == 1.5),
             ("razao das medianas de pares com script fixo deveria ser 1.03", Resumo["razao_das_medianas"] == 1.03),
             ("par estreito deveria ser conclusivo", Resumo["conclusivo"] is True),
             ("par disperso deveria marcar dispersao alta", Largos["conclusivo"] is False),
             ("speedup(12) com t(12)=2 x t(1) deveria ser 6 e speedup(2)=1.961", abs(Speedups[12] - 6.0) < 1e-9 and abs(Speedups[2] - 1000.0 / 510.0) < 1e-9),
             ("N efetivo deveria ser limitado a [1, N]", NEfetivoDe({1: 1.0, 4: 9.0}, 4) == 4.0 and NEfetivoDe({1: 1.0, 4: 0.2}, 4) == 1.0)]
    return [Rotulo for Rotulo, Ok in Casos if not Ok]


def Selftest():
    """Confere o que nao precisa do forja: validacao do --amostras, NucleosFisicos sem WMI, tabela incremental, tempos_orq, o limite do repouso, a sentinela, o piso do A3N e o A8."""
    Falhas = SelftestAmostras() + SelftestNucleos() + SelftestIncrementais() + SelftestTemposOrq() + SelftestRepouso() + SelftestSentinela() + SelftestPiso() + SelftestA8() + SelftestPareado()
    for Falha in Falhas:
        print(f"SELFTEST FALHOU: {Falha}")
    if Falhas:
        return 1
    print("SELFTEST OK")
    return 0


def Main(Argumentos=None):
    Parser = argparse.ArgumentParser(description="Mede o orquestrador de projeto e prova A1..A7 (P22-O3).")
    Parser.add_argument("--forja", help="caminho absoluto de uma COPIA do forja.exe")
    Parser.add_argument("--tudo", action="store_true")
    Parser.add_argument("--so", choices=["a1", "tempos"])
    Parser.add_argument("--amostras", type=AmostraValida, default=None, help=f"inteiro >= 1 (amostras por config, padrao {AMOSTRAS}; pares do A2, padrao {PARES_A2})")
    Parser.add_argument("--aquecimentos", type=int, default=AQUECIMENTOS)
    Parser.add_argument("--manter", action="store_true", help="nao apaga a pasta de trabalho em %%TEMP%% no fim")
    Parser.add_argument("--exigir-repouso", dest="ExigirRepouso", action="store_true",
                        help="espera a CPU repousar antes de cada grupo de amostras e refaz amostra ruidosa (ate 3x); sem repouso sai com codigo 2")
    Parser.add_argument("--tempos-orq", dest="TemposOrq", action="store_true",
                        help="define FORJA_ORQ_TEMPOS nas execucoes do orquestrador e agrega a mediana por etapa em tempos_orq")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args(Argumentos)
    if Args.selftest:
        return Selftest()
    Forja = ValidarArgumentos(Parser, Args)
    Trabalho = Path(tempfile.mkdtemp(prefix="forja_medir_projeto_"))
    try:
        Saida = ExecutarEtapas(Args, Forja, Trabalho)
    except NaoConclusivo as Motivo:
        return GravarNaoConclusivo(Args, str(Motivo))
    finally:
        if not Args.manter:
            shutil.rmtree(Trabalho, ignore_errors=True)
    Nucleos = NucleosFisicos()
    Saida.update({"versao": VERSAO, "data": datetime.now().isoformat(timespec="seconds"), "forja": str(Forja), "forja_sha256": Sha256Arquivo(Forja), "nucleos_fisicos": Nucleos,
                  "processadores_logicos": os.cpu_count(), "amostras": Args.amostras or AMOSTRAS, "pares_a2": Args.amostras or PARES_A2, "aquecimentos": Args.aquecimentos})
    FinalizarAceites(Saida, Nucleos)
    ImprimirTabelas(Saida)
    print(f"\nJSON: {GravarJson(Saida)}")
    return 0 if all(Item["ok"] for Item in Saida["aceites"]) else 1


if __name__ == "__main__":
    sys.exit(Main())
