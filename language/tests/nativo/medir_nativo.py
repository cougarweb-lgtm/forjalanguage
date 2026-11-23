# -*- coding: utf-8 -*-
"""Mede o backend nativo x64 e o backend C da Forja (P8 do M1; fechamento do M2 no P18), com time.perf_counter.

Itens (as tres metas do plano 10.6, medidas separadamente):
  (2a) build --modo exe do 01_ola no x64 (--backend x64; mediana de 15, 2 aquecimentos; meta <= 25 ms);
       build FRIO do corpus_200000 no x64 (mediana de 15, sem aquecimento, -o em %TEMP%; meta <= 500 ms): tempo de
       parede de builds SEM --debug-code e, em builds alternados com eles (ordem ABBA), o registro "tempos" do
       <saida>.debug.jsonl (--debug-code tempos,json): tabela das 10 etapas + "outros" (mediana e minimo), "outros" em %
       do total e vazao do backend (linhas/s das 6 etapas bir..escrever); o exe gerado roda e o stdout e comparado
       com benchmark\\corpus\\gerado\\corpus_200000.esperado.txt (IGUAL/DIFERENTE).
  (2b) tests\\nativo\\bench\\*.fj (medir_bench.py): release x64 contra os binarios gcc -O0 e -O2 congelados (D0730; sem
       backend C nem gcc na medicao), mediana de 5 execucoes, razao por bench e media geometrica; linha 'G6 serial' com o
       tempo de particulas_soa e nbody_f64.
  (3)  tamanho do compiler\\forja.exe (so leitura; o arquivo nao e tocado).
  --contra <exe>: ABBA pareado (15 pares alternados) do forja medido contra outro forja.exe, no corpus_200000 e no 01_ola:
       mediana das razoes (medido / contra), min..max e dispersao IQR/mediana (conclusivo se <= 0,15).
Fecha com a linha PORTAO: 2a_ola=<ms> (meta <=25) 2a=<ms> (meta <=500) backend=<ms> vazao=<linhas/s> (informado)
  2b_O0=<razao> (meta <=1.30) 2b_O2=<razao> 3=<MB> (meta <=10.00).
Exit 1 se um bench falhar/divergir ou o corpus sair DIFERENTE; as metas sao lidas pelo main (nao fazem exit 1).

Enquanto o x64 nao existir, o build x64 sai com X0003 e os itens dele viram "n/d".
O forja executado e uma copia em TEMP (o compiler\\forja.exe pode estar em uso). Maquina quieta: sem outros processos pesados.

Uso:
    python -I medir_nativo.py [--forja <exe>] [--contra <exe>]

Versao: 1.4.2
Changelog: 1.4.2 - 0.21.0 F6: a meta do tamanho do forja.exe impressa (2 MB) estava defasada; o criterio C7 e 10 MB (o exe mede 3,91 MB).
           1.4.1 - 0.21.0 F6 (decisao D0733, Fable): meta do corpus_200000 sobe de 400 para 500 ms. A de 400 foi calibrada no M4 (0.19.0, ~395 ms) antes de o dev gravar DWARF e .pdb; hoje o dev mede ~470 ms
                    (simbolos ~45 ms) e a 0.21.0 nao piorou (razao 0,9925 contra o forja_g1). Divida: otimizar o escritor DWARF/PDB, o check e o bir/isel do dev se o dev passar de 500 ms em maquina quieta.
           1.4.0 - 0.21.0 F3: o 01_ola sai do backend C (--backend c, aposentado na 0.21.0; X0003): so o x64 e medido, com a mesma meta;
                    a 2b passa a usar as referencias gcc congeladas do medir_bench 1.4.0 (sem forja --modo c nem gcc na medicao).
           1.3.1 - M4 P25b: o build do lado C do 01_ola leva --backend c explicito (o padrao do dev passou a ser x64).
           1.3.0 - M4 P24a: metas novas (01_ola <= 25 ms, corpus <= 400 ms, 15 repeticoes); tabela de etapas pelo registro "tempos" do .debug.jsonl (nao mais pelo --tempos);
                    --contra <exe> (ABBA pareado); linha PORTAO com 2a_ola, backend e vazao e a meta do tamanho.
           1.2.0 - P18 do M2: corpus_200000 (build frio, mediana de 5, vazao do backend, saida contra o esperado), bench x64 contra gcc -O0/-O2 (medir_bench.py), G6 serial, linha PORTAO; saem o corpus de 20k e o fib(30).
           1.1.0 - P8 do M1: BIR% do 01_ola por uma rodada extra com --debug-code tempos; 1a/2a execucao tambem do exe do x64 (regravado antes de cada par).
           1.0.0 - primeira versao (P8-T do M1).
Verificacao: python -I medir_nativo.py
"""

import argparse
import json
import shutil
import statistics
import sys
import tempfile
import time
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui))
import medir_bench as Bench  # noqa: E402

PastaLanguage = PastaAqui.parents[1]
PastaExemplos = PastaLanguage.parent / "examples"
PastaCorpus = PastaLanguage / "benchmark" / "corpus" / "gerado"
ArquivoForjaOrigem = PastaLanguage / "compiler" / "forja.exe"
ArquivoOla = PastaExemplos / "01_ola.fj"
ArquivoCorpus = PastaCorpus / "corpus_200000.fj"
ArquivoCorpusEsperado = PastaCorpus / "corpus_200000.esperado.txt"
Aquecimentos = 2
Repeticoes = 15
RepeticoesCorpus = 15
ParesContra = 15
MaximaDispersaoConclusiva = 0.15  # IQR / mediana das razoes
TentativasCopia = 5
EsperaCopiaSegundos = 30
LimiteForjaBytes = 10 * 1024 * 1024
MetaOlaMs = 25
MetaCorpusMs = 500
MetaRazaoO0 = 1.30
MetaForjaMb = 10.00
# As 10 etapas do registro "tempos" (P24a), na ordem do pipeline; "outros" fecha o total. O backend e o que vai de bir a escrever.
EtapasDoBuild = ("lex", "parse", "check", "ir", "bir", "isel", "regs", "codificar", "ligar", "escrever")
EtapasBir = ("bir", "isel", "regs", "codificar", "ligar", "escrever")
ItensDebugTempos = "tempos,json"
NaoDisponivel = "n/d"


def CopiarForja(PastaTrabalho):
    Destino = Path(PastaTrabalho) / "forja.exe"
    for Tentativa in range(1, TentativasCopia + 1):
        try:
            shutil.copy2(ArquivoForjaOrigem, Destino)
            return str(Destino)
        except OSError as Erro:
            if Tentativa == TentativasCopia:
                raise SystemExit(f"X0001 nao copiei o forja.exe apos {TentativasCopia} tentativas: {Erro}")
            time.sleep(EsperaCopiaSegundos)
    return str(Destino)


def RodarForja(Forja, Argumentos, Cwd):
    return Bench.Executar([Forja] + Argumentos, Cwd)


def ContarLinhas(Arquivo):
    return len(Path(Arquivo).read_bytes().splitlines())


def LerRegistroTempos(Jsonl):
    """O registro "tempos" do <saida>.debug.jsonl (json.loads por linha, tipo == "tempos"); None se o arquivo ou o registro nao existe."""
    if not Jsonl.is_file():
        return None
    for Linha in Jsonl.read_text(encoding="utf-8").splitlines():
        Registro = json.loads(Linha)
        if Registro.get("tipo") == "tempos":
            return Registro
    return None


def SomarBackendNs(Registro):
    # As 6 etapas do backend (BIR ate escrever), sem lex/parse/check/ir.
    return sum(Registro[Etapa + "_ns"] for Etapa in EtapasBir)


def BuildCorpus(Forja, Pasta, ComRegistro):
    """Um build frio do corpus_200000 no x64: (ms de parede, registro tempos ou None sem --debug-code) ou None se o forja recusar."""
    Saida = Pasta / "corpus_200000_x64.exe"
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    Saida.unlink(missing_ok=True)
    Jsonl.unlink(missing_ok=True)
    Argumentos = ["build", "--modo", "exe", str(ArquivoCorpus), "-o", str(Saida), "--backend", "x64"]
    if ComRegistro:
        Argumentos += ["--debug-code", ItensDebugTempos]
    Inicio = time.perf_counter()
    Resultado = RodarForja(Forja, Argumentos, Pasta)
    Ms = (time.perf_counter() - Inicio) * 1000.0
    if Resultado.returncode != 0:
        return None
    return Ms, LerRegistroTempos(Jsonl) if ComRegistro else None


def RodadaCorpus(Forja, Pasta, Indice):
    """Um build sem --debug-code (a parede) e um com ele (o registro), em ordem alternada: par ABBA, o ruido que muda no meio nao favorece sempre o mesmo lado."""
    Ordem = (False, True) if Indice % 2 == 0 else (True, False)
    Resultados = {Com: BuildCorpus(Forja, Pasta, Com) for Com in Ordem}
    return Resultados[False], Resultados[True]


def CompararCorpus(Pasta):
    Resultado = Bench.Executar([Pasta / "corpus_200000_x64.exe"], Pasta)
    if Resultado.returncode != 0:
        return "DIFERENTE"
    Esperado = ArquivoCorpusEsperado.read_bytes()
    Igual = Resultado.stdout.replace(b"\r\n", b"\n") == Esperado.replace(b"\r\n", b"\n")
    return "IGUAL" if Igual else "DIFERENTE"


def ImprimirTabelaEtapas(Registros):
    """Tabela das 10 etapas e do "outros" (ms; mediana e minimo das amostras) e o "outros" em % do total (mediana das razoes por amostra)."""
    print(f"(2a) etapas do build do corpus_200000 (registro tempos de {len(Registros)} builds)  {'mediana ms':>11} {'minimo ms':>10}")
    for Etapa in EtapasDoBuild + ("outros",):
        Valores = [R[Etapa + "_ns"] / 1e6 for R in Registros]
        print(f"(2a)   {Etapa:<10} {statistics.median(Valores):>41.2f} {min(Valores):>10.2f}")
    Totais = [R["total_ns"] / 1e6 for R in Registros]
    PorCento = statistics.median(Bench.DividirValor(R["outros_ns"] * 100.0, R["total_ns"]) for R in Registros)
    print(f"(2a)   {'total':<10} {statistics.median(Totais):>41.2f} {min(Totais):>10.2f}   outros = {PorCento:.1f}% do total (mediana das razoes)")


def MedirCorpus(Forja, Pasta):
    """(2a) Mediana de 15 builds frios sem --debug-code (a parede), tabela de etapas e vazao pelo registro tempos de builds alternados com eles e comparacao do exe
    com o esperado; devolve (mediana em ms, backend em ms, vazao em linhas/s, situacao), ou Nones e n/d se o x64 recusou o build."""
    Rodadas = [RodadaCorpus(Forja, Pasta, Indice) for Indice in range(RepeticoesCorpus)]
    if any(Sem is None or Com is None or Com[1] is None for Sem, Com in Rodadas):
        print("(2a) corpus_200000 x64: n/d (o build x64 foi recusado ou nao gravou o registro tempos)")
        return None, None, None, NaoDisponivel
    Mediana = statistics.median(Sem[0] for Sem, _ in Rodadas)
    MedianaCom = statistics.median(Com[0] for _, Com in Rodadas)
    Registros = [Com[1] for _, Com in Rodadas]
    BackendNs = statistics.median(SomarBackendNs(R) for R in Registros)
    Linhas = Registros[-1]["linhas"]
    Vazao = Bench.DividirValor(Linhas * 1e9, BackendNs)
    print(f"(2a) corpus_200000 x64 build frio: {Mediana:.1f} ms (mediana de {RepeticoesCorpus} sem --debug-code; meta <= {MetaCorpusMs} ms), {Linhas} linhas")
    print(f"(2a) com --debug-code {ItensDebugTempos}: {MedianaCom:.1f} ms (custo {Bench.DividirValor(MedianaCom, Mediana) - 1.0:+.1%} sobre a parede)")
    ImprimirTabelaEtapas(Registros)
    print(f"(2a) vazao do backend x64: {Vazao:.0f} linhas/s (backend {BackendNs / 1e6:.1f} ms = bir..escrever; informado)")
    Situacao = CompararCorpus(Pasta)
    print(f"(2a) exe do corpus_200000 x64 contra corpus_200000.esperado.txt: {Situacao}")
    return Mediana, BackendNs / 1e6, Vazao, Situacao


def ParContra(Forja, Contra, Fonte, Pasta, Indice):
    """Um par ABBA: (ms do forja medido, ms do forja de comparacao); pares pares rodam (medido, contra), impares (contra, medido). Build frio, sem flags."""
    Ordem = (("medido", Forja), ("contra", Contra)) if Indice % 2 == 0 else (("contra", Contra), ("medido", Forja))
    Tempos = {}
    for Rotulo, Exe in Ordem:
        Saida = Pasta / f"contra_{Rotulo}.exe"
        Saida.unlink(missing_ok=True)
        Inicio = time.perf_counter()
        Resultado = RodarForja(Exe, ["build", "--modo", "exe", str(Fonte), "-o", str(Saida), "--backend", "x64"], Pasta)
        Tempos[Rotulo] = (time.perf_counter() - Inicio) * 1000.0
        if Resultado.returncode != 0:
            raise SystemExit(f"X0001 {Rotulo} recusou o build de {Fonte.name} (exit {Resultado.returncode})")
    return Tempos["medido"], Tempos["contra"]


def ResumirRazoes(Razoes):
    """Mediana das razoes, minimo, maximo e dispersao IQR/mediana; conclusivo se a dispersao for <= MaximaDispersaoConclusiva."""
    Mediana = statistics.median(Razoes)
    Quartis = statistics.quantiles(Razoes, n=4)
    Dispersao = Bench.DividirValor(Quartis[2] - Quartis[0], Mediana)
    return Mediana, min(Razoes), max(Razoes), Dispersao, Dispersao <= MaximaDispersaoConclusiva


def MedirContra(Forja, Contra, Pasta):
    """--contra: ABBA pareado do forja medido contra o outro no 01_ola e no corpus_200000; razao > 1 = o medido ficou mais lento."""
    for Rotulo, Fonte in (("01_ola", ArquivoOla), ("corpus_200000", ArquivoCorpus)):
        Pares = [ParContra(Forja, Contra, Fonte, Pasta, Indice) for Indice in range(ParesContra)]
        Mediana, Minimo, Maximo, Dispersao, Conclusivo = ResumirRazoes([Medido / Antes for Medido, Antes in Pares])
        print(f"(2a) --contra {Rotulo}: razao medido/contra {Mediana:.4f} (mediana de {ParesContra} pares ABBA; min {Minimo:.4f} max {Maximo:.4f}), "
              f"dispersao IQR/mediana {Dispersao:.3f} ({'conclusivo' if Conclusivo else 'NAO conclusivo'})")


def MedirBuildOla(Forja, Pasta):
    Saida = Pasta / "ola_x64.exe"
    Argumentos = ["build", "--modo", "exe", str(ArquivoOla), "-o", str(Saida), "--backend", "x64"]
    if RodarForja(Forja, Argumentos, Pasta).returncode != 0:
        return None
    return Bench.MedirMediana(lambda: RodarForja(Forja, Argumentos, Pasta), Aquecimentos, Repeticoes)


def ImprimirOla(Forja, Pasta):
    """Mede o build do 01_ola no backend x64; devolve os ms (None = n/d)."""
    Linhas = ContarLinhas(ArquivoOla)
    Tempo = MedirBuildOla(Forja, Pasta)
    if Tempo is None:
        print("(2a) build 01_ola backend x64: n/d")
        return None
    Vazao = Bench.DividirValor(Linhas * 1000.0, Tempo)
    print(f"(2a) build 01_ola backend x64: {Tempo:.1f} ms (mediana de {Repeticoes}; meta <= {MetaOlaMs} ms), {Vazao:.0f} linhas/s ({Linhas} linhas)")
    return Tempo


def ImprimirForja():
    Tamanho = ArquivoForjaOrigem.stat().st_size
    Situacao = "dentro do limite" if Tamanho <= LimiteForjaBytes else "ACIMA do limite de 10 MB"
    Mb = Bench.DividirValor(Tamanho, 1024 * 1024)
    print(f"(3) forja.exe: {Tamanho} bytes ({Mb:.2f} MB), {Situacao}")
    return Mb


def MedirDesempenho(Forja, Pasta):
    """(2b) Tabela por bench, media geometrica contra -O0 e -O2 e a linha G6; devolve (razao O0, razao O2, houve falha)."""
    Resultados = Bench.MedirTodos(Forja, Pasta)
    Bench.ImprimirTabela(Resultados)
    RazaoO0 = Bench.MediaGeometrica(Bench.RazoesContra(Resultados, "O0"))
    RazaoO2 = Bench.MediaGeometrica(Bench.RazoesContra(Resultados, "O2"))
    print(f"(2b) media geometrica x64/gcc-O0: {RazaoO0:.3f} (meta <= {MetaRazaoO0:.2f}); x64/C -O2: {RazaoO2:.3f} (so informado)")
    print(Bench.LinhaSerial(Resultados))
    return RazaoO0, RazaoO2, any("erro" in R for R in Resultados)


def Formatar(Valor, Formato):
    return NaoDisponivel if Valor is None else format(Valor, Formato)


def Principal():
    Parser = argparse.ArgumentParser(description="Mede o x64 da Forja (01_ola, corpus_200000 e bench contra gcc -O0/-O2 congelados).")
    Parser.add_argument("--forja", default=None, help="exe da Forja (padrao: copia em TEMP)")
    Parser.add_argument("--contra", default=None, help="outro forja.exe: ABBA pareado (15 pares) no corpus_200000 e no 01_ola")
    Args = Parser.parse_args()

    PastaTrabalho = Path(tempfile.mkdtemp(prefix="medir_nativo_"))
    try:
        Forja = Args.forja or CopiarForja(PastaTrabalho)
        Mb = ImprimirForja()
        MsOla = ImprimirOla(Forja, PastaTrabalho)
        MsCorpus, MsBackend, Vazao, SituacaoCorpus = MedirCorpus(Forja, PastaTrabalho)
        if Args.contra:
            MedirContra(Forja, Args.contra, PastaTrabalho)
        RazaoO0, RazaoO2, Falhou = MedirDesempenho(Forja, PastaTrabalho)
        print(f"PORTAO: 2a_ola={Formatar(MsOla, '.1f')} (meta <={MetaOlaMs}) 2a={Formatar(MsCorpus, '.0f')} (meta <={MetaCorpusMs}) backend={Formatar(MsBackend, '.0f')} "
              f"vazao={Formatar(Vazao, '.0f')} (informado) 2b_O0={RazaoO0:.2f} (meta <={MetaRazaoO0:.2f}) 2b_O2={RazaoO2:.2f} 3={Mb:.2f} (meta <={MetaForjaMb:.2f})")
        return 1 if (Falhou or SituacaoCorpus == "DIFERENTE") else 0
    finally:
        shutil.rmtree(PastaTrabalho, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Principal())
