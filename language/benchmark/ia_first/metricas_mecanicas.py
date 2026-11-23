# -*- coding: utf-8 -*-
"""metricas_mecanicas.py - as metricas M do IA1 que JA dao para medir hoje, em JSON (P-IA1-a07, contrato secao 1).

Versao 0.2.0
Changelog:
  0.2.0 (14/11/2026) M-U4, M-D2, M-D13, M-D4, M-D10, M-D7, M-D8, M-D12, M-A1..M-A4, M-E1, M-E2 e M-I1..M-I3 passam a sair dos verificadores existentes (MedirVerificadores: -I e --forja, 8 em paralelo; cada script roda uma vez). M-D4, M-D7 e M-D10 vem da linha de TEXTO do verificador (LinhasDeTexto: limiar do proprio verificador). M-U5, M-W6, M-V1 e M-V2 seguem pendentes.
  0.1.0 (11/11/2026) primeira versao. Mede de verdade: M-U1, M-U2, M-U3 (verificar_spec.py contra o binario), M-D1, M-D3 (parte 1),
        M-D5, M-D6 e M-D11 (sondas_compilador.py). Todas as demais saem verde=null com valor "pendente" e o que falta
        ("depende"): nunca verdes de mentira. A meta nunca muda aqui; so o numero.

Uso:
  python metricas_mecanicas.py --forja <forja.exe> [--json] [--sem-fixtures]
Saida (--json): UMA linha JSON por metrica {id, valor, meta, verde} (verde true | false | null = pendente), em ordem de contrato;
  nada mais no stdout. Sem --json: tabela de texto. --sem-fixtures pula as sondas lentas (M-D1/D3/D5/D6/D11 saem pendentes).
Exit: 0 nenhuma metrica vermelha; 1 alguma vermelha; 2 uso. Verificacao: python selftest_ia_first.py
Reversao: apagar este arquivo. Depende de: Python 3.9+ e dos scripts irmaos (verificar_spec, gerar_doc_mem, sondas_compilador).
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import sondas_compilador as SC
from ia_comum import FORJA_CONGELADA, RAIZ, Metrica

SUBMETRICAS_SPEC = ("M-U1", "M-U2", "M-U3")
PENDENTES = [
    ("M-U5", "100%", "nomes em ingles em std e vendor (L-07, L-09) e regra de sinonimos"),
    ("M-W6", "100%", "forja fmt e F0001 (b-trilha)"),
    ("M-V1", "geomedia x64-dev / gcc -O0 <= 1,30, sem regressao > 3%", "bench_nativo em maquina quieta (ABBA) contra a base congelada"),
    ("M-V2", "sem regressao > 5% e 01_ola <= 25 ms", "medida ABBA em maquina quieta contra a base congelada"),
]
# Ids com verificador existente: saem de PENDENTES e guardam aqui a meta do contrato (usada tambem por --sem-fixtures).
METAS_VERIFICADOS = {
    "M-U4": "100%",
    "M-D2": ">= 85% vao a exit 0, 100% nao pioram",
    "M-D4": "100% (>= 24 entradas)",
    "M-D7": "linha media <= 250 chars",
    "M-D8": "100% dos R-codes com forma_certa e spec",
    "M-D10": ">= 70% (piso) / 85% dos err_ verdes so com correcoes certa",
    "M-D12": "100% de 30 fixtures de 3 erros e 20 de cascata",
    "M-D13": "0 casos de piora",
    "M-A1": "100% no esquema tests\\ast\\esquema_ast.json",
    "M-A2": "100% de 40 casos de id estavel",
    "M-A3": "100% dos nomes e chamadas com ref",
    "M-A4": "100% de recorte fiel",
    "M-E1": "100% de 60 operacoes",
    "M-E2": "100% dos casos que pioram sao desfeitos",
    "M-I1": "100% de 60 pontos de prova",
    "M-I2": "mapa <= 4.000 tokens, engine\\forja em <= 1 s",
    "M-I3": "consulta <= 300 ms",
}
LINGUAGEM = RAIZ.parents[1]
TEMPO_VERIFICADOR = 900
# (M-id, script relativo a language, argumentos, id da linha JSON que o script emite). M-D8 e M-D12 somam 2 linhas.
VERIFICADORES = [
    ("M-U4", "tests/consultar/verificar_consultar.py", ["--caso", "mu4"], "mu4"),
    ("M-D2", "benchmark/ia_first/verificar_corrigir.py", [], "M-D2"),
    ("M-D13", "benchmark/ia_first/verificar_corrigir.py", [], "M-D13"),
    ("M-D4", "benchmark/ia_first/verificar_m_d4.py", [], "M-D4"),
    ("M-D10", "benchmark/ia_first/verificar_m_d4.py", [], "M-D10"),
    ("M-D7", "tests/cli/b02/md7_resumo.py", [], "M-D7"),
    ("M-D8", "benchmark/ia_first/verificar_rastreio_b13.py", [], "m_d8"),
    ("M-D8", "benchmark/ia_first/verificar_limites_b10.py", [], "catalogo"),
    ("M-D12", "benchmark/ia_first/verificar_erros_b15.py", [], "M-D12 independentes"),
    ("M-D12", "benchmark/ia_first/verificar_erros_b15.py", [], "M-D12 cascata"),
    ("M-A1", "tests/ast/verificar_ast.py", ["--caso", "ma1,ma2,ma3,ma4"], "ma1"),
    ("M-A2", "tests/ast/verificar_ast.py", ["--caso", "ma1,ma2,ma3,ma4"], "ma2"),
    ("M-A3", "tests/ast/verificar_ast.py", ["--caso", "ma1,ma2,ma3,ma4"], "ma3"),
    ("M-A4", "tests/ast/verificar_ast.py", ["--caso", "ma1,ma2,ma3,ma4"], "ma4"),
    ("M-E1", "tests/editar/verificar_editar_i5.py", ["--caso", "me1,me2"], "me1"),
    ("M-E2", "tests/editar/verificar_editar_i5.py", ["--caso", "me1,me2"], "me2"),
    ("M-I1", "tests/consultar/verificar_mapa.py", ["--caso", "mi1,mi2,mi3"], "mi1"),
    ("M-I2", "tests/consultar/verificar_mapa.py", ["--caso", "mi1,mi2,mi3"], "mi2"),
    ("M-I3", "tests/consultar/verificar_mapa.py", ["--caso", "mi1,mi2,mi3"], "mi3"),
]
ORDEM = ["M-U1", "M-U2", "M-U3", "M-U4", "M-U5", "M-W6", "M-D1", "M-D2", "M-D3", "M-D4", "M-D5", "M-D6", "M-D7", "M-D8", "M-D10", "M-D11",
         "M-D12", "M-D13", "M-A1", "M-A2", "M-A3", "M-A4", "M-E1", "M-E2", "M-I1", "M-I2", "M-I3", "M-V1", "M-V2"]


def Agrupar(LinhasSpec):
    """M-U1a..d viram M-U1: verde so se todas as partes forem verdes; valor = {parte: valor}."""
    Saida = []
    for Id in SUBMETRICAS_SPEC:
        Partes = [L for L in LinhasSpec if L["id"].startswith(Id)]
        Metas = {L["id"]: L["meta"] for L in Partes}
        Saida.append(Metrica(Id, {L["id"]: L["valor"] for L in Partes}, Metas, all(L["verde"] for L in Partes) if Partes else False))
    return Saida


def LinhasDoSpec(Texto):
    Linhas = []
    for Linha in Texto.splitlines():
        try:
            Objeto = json.loads(Linha)
        except ValueError:
            continue
        if isinstance(Objeto, dict) and "id" in Objeto and "verde" in Objeto:
            Linhas.append(Objeto)
    return Linhas


def MedirSpec(Forja):
    """M-U1..M-U3: gera os textos de doc num diretorio temporario e roda verificar_spec contra o binario."""
    with tempfile.TemporaryDirectory(prefix="a07_doc_") as Saida:
        Gerar = subprocess.run([sys.executable, "-I", str(RAIZ / "gerar_doc_mem.py"), "--forja", str(Forja), "--saida", Saida],
                               capture_output=True, text=True, timeout=300)
        if Gerar.returncode != 0:
            return FalhaDoSpec("gerar_doc_mem falhou: " + (Gerar.stderr or Gerar.stdout).strip()[:200])
        Ver = subprocess.run([sys.executable, "-I", str(RAIZ / "verificar_spec.py"), "--forja", str(Forja), "--saida-doc", Saida],
                             capture_output=True, text=True, timeout=300)
    Linhas = LinhasDoSpec(Ver.stdout)
    return Agrupar(Linhas) if Linhas else FalhaDoSpec("verificar_spec sem saida: " + (Ver.stderr or "").strip()[:200])


def FalhaDoSpec(Motivo):
    return [Metrica(Id, "nao mediu: " + Motivo, "ver verificar_spec.py", False) for Id in SUBMETRICAS_SPEC]


def MedirSondas(Forja):
    Sondas = SC.SondarTodas(Forja)
    return [SC.MetricaD1(Sondas), SC.MetricaD3(Forja, Sondas), SC.MetricaD5(Sondas), SC.MetricaD6(Forja), SC.MetricaD11(Sondas)]


PADRAO_D4 = re.compile(r"^M-D4: entradas=(\d+) sem_caso=(\d+)\s+casos=(\d+) falhas=(\d+)", re.M)
PADRAO_D10 = re.compile(r"^M-D10 \(idiomas\): ([\d.]+)%", re.M)
PADRAO_D7 = re.compile(r"media=([\d.]+).*\n?M-D7: (verde|VERMELHO)", re.M)


def LinhasDeTexto(Texto):
    """M-D4, M-D10 e M-D7: os 3 verificadores imprimem texto; vira {id, valor, verde} com o limiar do proprio verificador (M-D4 100% e >= 24 entradas, M-D10 >= 95%)."""
    Linhas = []
    Achado = PADRAO_D4.search(Texto)
    if Achado:
        Entradas, SemCaso, Casos, Falhas = (int(G) for G in Achado.groups())
        Linhas.append({"id": "M-D4", "valor": {"entradas": Entradas, "sem_caso": SemCaso, "casos": Casos, "falhas": Falhas}, "verde": Entradas >= 24 and SemCaso == 0 and Falhas == 0})
    Achado = PADRAO_D10.search(Texto)
    if Achado:
        Linhas.append({"id": "M-D10", "valor": {"pct_certa_no_exit_0": float(Achado.group(1))}, "verde": float(Achado.group(1)) >= 95.0})
    Achado = PADRAO_D7.search(Texto)
    if Achado:
        Linhas.append({"id": "M-D7", "valor": {"linha_media": float(Achado.group(1))}, "verde": Achado.group(2) == "verde"})
    return Linhas


def RodarVerificador(Forja, Script, Argumentos):
    """Roda um verificador existente com -I e --forja; devolve ({id da linha: linha}, motivo da falha ou None)."""
    Caminho = LINGUAGEM / Script
    if not Caminho.is_file():
        return {}, "verificador nao existe: " + Script
    Comando = [sys.executable, "-I", str(Caminho), "--forja", str(Forja), *Argumentos]
    try:
        Rodada = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TEMPO_VERIFICADOR)
    except (OSError, subprocess.TimeoutExpired) as Erro:
        return {}, "nao rodou: %s" % Erro
    Linhas = LinhasDoSpec(Rodada.stdout) + LinhasDeTexto(Rodada.stdout)
    if not Linhas:
        return {}, "saida sem linha JSON nem linha de texto conhecida (exit %d)" % Rodada.returncode
    return {L["id"]: L for L in Linhas}, None


def MetricaVerificador(Id, Rodadas):
    """Um M-id com verificador: verde so se todas as suas linhas existem e sao verdes; linha faltando vira 'nao mediu'."""
    Valores, Verdes, Motivos = {}, [], []
    for _, Script, Argumentos, LinhaId in [E for E in VERIFICADORES if E[0] == Id]:
        Linhas, Motivo = Rodadas[(Script, tuple(Argumentos))]
        if LinhaId not in Linhas:
            Motivos.append(Motivo or "nao emitiu " + LinhaId)
            continue
        Valores[LinhaId] = Linhas[LinhaId]["valor"]
        Verdes.append(bool(Linhas[LinhaId]["verde"]))
    if Motivos:
        return Metrica(Id, "nao mediu: " + "; ".join(Motivos), METAS_VERIFICADOS[Id], False)
    return Metrica(Id, Valores, METAS_VERIFICADOS[Id], all(Verdes))


def MedirVerificadores(Forja):
    """Os M-ids com verificador: cada (script, argumentos) roda uma vez, em paralelo (8 trabalhadores)."""
    Chaves = list(dict.fromkeys((Script, tuple(Argumentos)) for _, Script, Argumentos, _ in VERIFICADORES))
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Rodadas = dict(zip(Chaves, Pool.map(lambda Chave: RodarVerificador(Forja, *Chave), Chaves)))
    return [MetricaVerificador(Id, Rodadas) for Id in dict.fromkeys(E[0] for E in VERIFICADORES)]


def Pendente(Id, Meta, Depende):
    return Metrica(Id, "pendente", Meta, None, depende=Depende)


def Medir(Forja, ComFixtures=True):
    """Todas as metricas M na ordem do contrato."""
    Medidas = {L["id"]: L for L in MedirSpec(Forja)}
    if ComFixtures:
        Medidas.update({L["id"]: L for L in MedirSondas(Forja)})
        Medidas.update({L["id"]: L for L in MedirVerificadores(Forja)})
    else:
        Medidas.update({Id: Pendente(Id, Meta, "verificador desligado por --sem-fixtures") for Id, Meta in METAS_VERIFICADOS.items()})
    for Id, Meta, Depende in PENDENTES:
        Medidas[Id] = Pendente(Id, Meta, Depende)
    for Id in ("M-D1", "M-D3", "M-D5", "M-D6", "M-D11"):
        Medidas.setdefault(Id, Pendente(Id, "ver contrato", "sondas desligadas por --sem-fixtures"))
    return [Medidas[Id] for Id in ORDEM]


def Texto(Linha):
    Estado = {True: "VERDE", False: "VERMELHO", None: "pendente"}[Linha["verde"]]
    return "%-6s %-9s %s" % (Linha["id"], Estado, json.dumps(Linha["valor"], ensure_ascii=False)[:150])


def Principal():
    Analisador = argparse.ArgumentParser(description="Metricas mecanicas (M) do IA1")
    Analisador.add_argument("--forja", default=str(FORJA_CONGELADA))
    Analisador.add_argument("--json", action="store_true")
    Analisador.add_argument("--sem-fixtures", action="store_true")
    Args = Analisador.parse_args()
    if not Path(Args.forja).is_file():
        print("uso: --forja <forja.exe> (nao achei %s)" % Args.forja, file=sys.stderr)
        return 2
    Linhas = Medir(Path(Args.forja), not Args.sem_fixtures)
    for Linha in Linhas:
        print(json.dumps(Linha, ensure_ascii=False) if Args.json else Texto(Linha))
    return 1 if any(L["verde"] is False for L in Linhas) else 0


if __name__ == "__main__":
    sys.exit(Principal())
