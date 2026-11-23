r"""Roda a bateria do compilador Forja em paralelo, uma suite por processo, em 3 modos (rapida, media, completa). Nao compara com build anterior.

Versao: 2.8.0 (M8 P3: suite "perfil" = tests\nativo\perfil\verificar_perfil.py, prova do --perfil; media das areas runtime e driver; M8 H3: suite "reload" = tests\reload\verificar_reload.py, prova do hot reload; media das areas runtime e std)
Changelog do 2.9.0 (0.38.0): suite "wasm" = tests\nativo\wasm\verificar_wasm_bateria.py (verificar_wasm, verificar_run_test e os 2 reactors em dev e release; PULADA sem Node); media das areas nativo_x64 e runtime.
Changelog do 2.8.0: suite "perfil" (9 casos: contagens dos .esperado, aninhamento, 2 fios, panic e os.exit com quadros fechados, release com e sem inline, teto de 1.048.576 eventos, formato JSON, ELF no WSL com PULADA sem a distro).
Changelog do 2.7.0: suite "reload" (cenarios 1 a 5 e 7 em dev e release, cenario 6 so em dev, exemplos_libs\hot_reload; entra na media de runtime e std).
Changelog do 2.6.0: M8 F0: suite "congelado" = fuzz_otim.py --fatia N, N=0..3, contra o congelado
Changelog do 2.6.0: suite "congelado" (cong_1..cong_4, grupo "congelado", area "otim" na media e na completa; 600 sementes por fatia, so x64 ligado; o congelado 2400 vira portao automatico).
Changelog do 2.5.1: suite "libs" (vendor:json x vendor:net no mesmo programa passa a ser pego pela bateria; antes a regressao do E0202 do IA1-L8 passou despercebida).
Changelog anterior: 2.4.0 (0.21.0 F4 passo 2a: a suite emit_K (rodar_teste_emit.bat, tests/backend) saiu com o emissor C; PARTES_EMIT e os grupos "emit" saem junto)
Uso: python -I rodar_bateria.py [--modo rapida|media|completa] [--mudou area,area] [--so suite,suite] [--forja <forja.exe>] [--jobs N] [--listar]
     python -I rodar_bateria.py --selftest
     --modo: rapida = laco a cada edicao (nucleo + suites A da area, alvo <= 60 s); media = portao de fatia (rapida + suites B da area, alvo <= 200 s);
             completa = as 41 suites logicas, rodadas em 49 processos (cli em 4 partes, nativo em 2 grupos, lowering em 2 backends); fechamento de marco; padrao sem --modo.
     Suite fatiada: AREAS, NUCLEO, --so e a deteccao por arquivo de teste falam o nome do grupo (cli, emit, nativo); --so cli_2 roda so a parte 2.
     --mudou: areas explicitas (semantic frontend ir backend nativo_x64 nativo_obj runtime std driver doc). Sem ela, a area sai do carimbo do ultimo verde
             em %TEMP%\forja_bateria_estado (mtime + tamanho de compiler, runtime, std, doc, tests; so detecta mudanca). Sem carimbo, so o nucleo.
             Arquivo fora da tabela (area desconhecida) sobe a rapida para media. Mudanca so em tests\ ou benchmark\ roda a suite do proprio script.
     --so: roda so as suites listadas, na ordem da lista (vence --modo).   --listar: mostra o plano e nao roda.
     --forja: copia o forja.exe para a pasta da rodada e passa a copia as suites (padrao: compiler/forja.exe).
     --jobs: suites em paralelo (padrao = processadores logicos). Suites exclusivas (desempenho) rodam sozinhas no fim.
     Pasta da rodada: TEMP\forja_bateria (completa sem --so) ou TEMP\forja_bateria_<modo|so>: uma rodada curta nao apaga a de uma completa em curso.
     O carimbo e o tempos.json so sao gravados com media ou completa verde, sem --mudou e sem --so. A rapida pula suite medida acima de 45 s.
Saida (stdout): uma linha "<suite> | <exit> | <contagem>" por suite, na ordem da lista, a linha "modo=... areas=..." e a linha
     "bateria: S suites, F falhas, parede X s, soma Y s, jobs=J" (a ultima). Exit 0 so com todas as suites em exit 0.
     Progresso e tempo de cada suite vao para stderr; o log completo de cada suite fica em <pasta da rodada>/<suite>.log.
Changelog: 2.5.0 - M7a F0: suite "otim" (nativo\otim\verificar_otim.py: passe ligado x --sem-otim tudo, 40 casos do lowering + 14 benches), na media do nativo_x64.
           2.4.0 - M5b P42: a copia do forja.exe vai em <rodada>\bin\ e a semente musl (language\third\musl\x86_64) em <rodada>\third\musl\x86_64, onde a Forja a procura (<exe>\..\third); sem isso os 6 casos pontes com foreign do verificar_linux davam X0003 so dentro da bateria.
           2.3.0 - M5 P28: suite "linux" (nativo\linux\verificar_linux.py: o alvo x86_64-linux no WSL alpine-build, dev e release, um lote por rodada) so em media (areas runtime, nativo_obj, nativo_x64) e completa; sem a distro imprime "linux: PULADA (WSL/distro ausente)" e sai 0; o selftest confere onde ela entra.
           2.2.0 - o grupo nativo "lowering" (C x x64, 779 execucoes, refazia o gcc dos 389 casos que a suite lowering ja roda) sai da bateria: entra a suite lowering_x64 (verificar_lowering.py --backend x64: o x64 contra os mesmos .esperado, ~24 s no lugar de ~255 s). Perde so a igualdade byte a byte do stderr C x x64 nessas fixtures; dourado, m1, m2 e o escopo seguem com o diferencial inteiro.
           2.1.0 - completa mais enxuta, mesma cobertura: cli em 4 partes (--parte K/N), emit em 2 (argumentos K N do .bat), nativo em 3 grupos (--grupo lowering|corpus|escopo);
             Suite.Grupo; as suites mais longas (tempos.json) largam primeiro; o cli passa a receber --forja (rodava com o compiler\forja.exe fixo); selftest confere a cobertura das fatias.
             Medido (12 logicos): parede 500 s -> 455 s, mesma cobertura; a CPU satura (soma 2714 -> 4589 s), entao o piso e a CPU. --jobs 4 nos grupos nativo piorou (540 s): fica o padrao.
           2.0.0 - bateria unica: entram os modos rapida/media/completa, --mudou, --listar, --selftest, o carimbo e as tabelas por area (vieram do
             verificacao_enxuta.py, removido); pasta da rodada por modo; sai a suite "c_igual" (comparava com base gravada = build antigo, D0591): 36 suites;
             a constante VERSAO estava em 1.7.0.
           1.9.1 - POS-Z: a suite "semantica" passa --forja (rodava com o compiler\forja.exe fixo e ignorava o forja da bateria).
           1.9.0 - POS-Z (colinha de bolso): suite "receitas" (tests\receitas\verificar_receitas.py: cada receita compila, ja e canonica, nao tem forma antiga e roda nos 2 backends com a saida do .esperado); 37 suites.
           1.8.0 - F1 P3: suite "fmt" (tests\fmt\verificar_fmt.py: fixtures, sinteticos, arquivo, corpus com AST e texto, execucao de 40 programas); 36 suites.
           1.7.0 - S4 P3: suite "std_testes" (tests\std\verificar_std_testes.py: cada std\<m>\testes.fj roda nos 2 backends, resumo sem falha e listas de testes iguais); 35 suites.
           1.6.1 - E4: a pasta ia_first foi movida para language\benchmark\ia_first; PASTA_IA_FIRST aponta para ela.
           1.6.0 - E3a: promovidas 8 provas que nenhuma suite rodava (b15, l3c2, l3c3, poda, plantado, estatica, editar, editar_i5); 34 suites. As de ia_first usam a constante PASTA_IA_FIRST (mudar so ela ao mover a pasta).
           1.5.0 - IA1 D0591: sai a suite "censo" (comparava com o forja.exe da copia language_v111_base, apagada: o antigo nao vale mais); 26 suites.
           1.4.0 - M5 P26c: suite "arquivo" (nativo\m5\arquivo\verificar_arquivo.py: std:arquivo nos dois backends, 0 B a 64 MiB + 1, caminho UTF-8, erros, posse); 27 suites.
           1.3.0 - M5 P26b: suite "proc" (nativo\m5\proc\verificar_proc.py: std:proc nos dois backends, parser contra o CommandLineToArgvW); 26 suites.
           1.2.1 - M3b P23y: docstring em string crua (o "\m" de nativo\m3\abi gerava SyntaxWarning).
           1.2.0 - M3 P22-O1b: suite "projeto" (projeto\verificar_projeto.py, 14 casos do --plano do orquestrador).
           1.1.1 - M3 P22b: a suite "abi" passa a rodar tambem o lado export do verificar_abi (--lado ambos e o padrao: mais 4 linhas de contagem, mesma regex).
           1.1.0 - M3 P20: suite "abi" (nativo\abi\verificar_abi.py, 3 sementes x dev/release; a pasta nativo\m3\abi entra na suite "nativo" sozinha).
           1.0.0 - P19-B: lista literal das suites da bateria M2 (com c_igual, censo, desempenho e nativo),
           copia unica do forja, TEMP e TMP por suite em pasta nova a cada rodada, contagem pela regex de cada suite.
Verificacao: python -I rodar_bateria.py --so teste_common
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

VERSAO = "2.0.0"
TESTES = Path(__file__).resolve().parent
LINGUAGEM = TESTES.parent
PASTA_IA_FIRST = TESTES.parent / "benchmark" / "ia_first"  # pasta-base das provas do benchmark IA1 (trocar so aqui ao mover)
COMPILADOR = TESTES.parent / "compiler"
PASTA_BATERIA = Path(tempfile.gettempdir()) / "forja_bateria"
PASTA_ESTADO = Path(tempfile.gettempdir()) / "forja_bateria_estado"  # carimbo do ultimo verde e tempos por suite
TENTATIVAS_COPIA = 3
ESPERA_COPIA = 20
TAMANHO_ULTIMA_LINHA = 120
LIMITE_RAPIDA = 45  # s: suite medida acima disto nao entra no modo rapida
PARTES_CLI = 4  # fatias do verificar_cli (--parte K/N)
GRUPOS_NATIVO = ("corpus", "escopo")  # os --grupo do verificar_nativo que a bateria roda (o grupo "lowering" fica so para uso manual: a suite lowering_x64 o substitui)
MODOS =("rapida", "media", "completa")
NUCLEO = ["teste_common", "teste_nativo", "regras", "sintaxe", "semantica"]

# area -> (suites que o modo rapida soma, suites que a media soma a mais)
AREAS = {
    "semantic": (["b15", "l3c2", "l3c3"], ["rodar_suite", "exemplos", "receitas", "fmt"]),
    "ir": (["rodar_suite", "bir", "poda"], ["lowering"]),
    "backend": (["rodar_suite", "exemplos", "rt"], ["std_testes", "receitas"]),
    "nativo_x64": (["bir", "enc", "unwind", "console", "rt_nativo", "abi"], ["poda", "plantado", "linux", "otim", "wasm"]),
    "nativo_obj": (["obj", "pe", "ligar", "estatica", "plantado"], ["abi", "console", "linux"]),
    "runtime": (["rt", "rt_nativo", "proc", "arquivo"], ["std_testes", "receitas", "linux", "reload", "perfil", "wasm"]),
    "std": (["std_testes", "receitas"], ["exemplos", "arquivo", "proc", "reload"]),
    "driver": (["projeto", "proc", "arquivo"], ["cli", "editar", "editar_i5", "exemplos", "perfil"]),
    "doc": (["receitas", "b15", "cobertura_doc"], ["editar", "cli"]),  # spec e cartao sao embutidos no forja.exe: o cli confere byte a byte
    "otim": ([], ["congelado"]),
}
AREAS["frontend"] = AREAS["semantic"]

# prefixo do caminho (relativo a language\) -> area; o mais especifico vem primeiro
PREFIXOS = [
    ("compiler/frontend/", "frontend"), ("compiler/semantic/", "semantic"), ("compiler/common/", "semantic"),
    ("compiler/ir/", "ir"), ("compiler/optimizer/", "ir"), ("compiler/layout/", "ir"), ("compiler/backend/", "backend"),
    ("compiler/nativo/obj/", "nativo_obj"), ("compiler/nativo/fmt/", "nativo_obj"), ("tools/forja_ligar/", "nativo_obj"),
    ("compiler/nativo/", "nativo_x64"), ("compiler/runtime/", "runtime"), ("runtime/", "runtime"),
    ("compiler/driver/", "driver"), ("compiler/cache/", "driver"), ("compiler/construir", "driver"),
    ("std/", "std"), ("doc/", "doc"),
]
DE_TESTES = ("tests/", "benchmark/")
# pasta (relativa a language\) -> extensoes que entram na varredura do carimbo
VARRIDAS = {"compiler": {".c", ".h", ".bat", ".py"}, "runtime": {".c", ".h"}, "std": {".fj"}, "doc": {".txt", ".tsv"},
            "tests": {".py", ".fj"}, "benchmark/ia_first": {".py"}}


@dataclass
class Suite:
    Nome: str
    Comando: list
    Regex: re.Pattern
    Exclusiva: bool = False
    Grupo: str = ""  # suites fatiadas (cli_1..cli_4) dividem o grupo "cli"; --so, AREAS e NUCLEO falam o nome do grupo

    def __post_init__(self):
        self.Grupo = self.Grupo or self.Nome


@dataclass
class Resultado:
    Nome: str
    Rc: int
    Contagem: str
    Duracao: float


def ArqTestes(*Partes):
    return str(TESTES.joinpath(*Partes))


def ArqCompilador(*Partes):
    return str(COMPILADOR.joinpath(*Partes))


def MontarSuites(Forja):
    # Comandos copiados da bateria M2 (bateria_par_p19a.ps1) e da enxuta (enxuta_fb.ps1); ordem da tabela = ordem desta lista.
    Py = sys.executable
    return [
        Suite("teste_common", [ArqCompilador("teste_common.exe")], re.compile(r"\d+ verificacoes")),
        Suite("teste_nativo", [ArqCompilador("teste_nativo.exe")], re.compile(r"\d+ verificacoes")),
        Suite("sintaxe", [Py, "-I", ArqTestes("sintaxe", "verificar_sintaxe.py")], re.compile(r"\d+/\d+ casos de sintaxe")),
        Suite("semantica", [Py, "-I", ArqTestes("semantica", "verificar_semantica.py"), "--forja", Forja], re.compile(r"\d+/\d+ casos de regressao")),
        Suite("rodar_suite", [Py, "-I", ArqTestes("rodar_suite.py"), "--compilador", Forja, "--corpus", ArqTestes("dourado")], re.compile(r"PASSOU \d+/\d+")),
        Suite("lowering", [Py, "-I", ArqTestes("lowering", "verificar_lowering.py"), "--forja", Forja], re.compile(r"\d+/\d+ casos de lowering")),
        Suite("lowering_x64", [Py, "-I", ArqTestes("lowering", "verificar_lowering.py"), "--forja", Forja, "--backend", "x64"],
              re.compile(r"\d+/\d+ casos de lowering"), Grupo="lowering"),
    ] + [
        Suite(f"cli_{K}", [Py, ArqTestes("cli", "verificar_cli.py"), "--forja", Forja, "--parte", f"{K}/{PARTES_CLI}"], re.compile(r"\d+/\d+ casos de cli"), Grupo="cli")
        for K in range(1, PARTES_CLI + 1)
    ] + [
        Suite("exemplos", [Py, "-I", ArqTestes("exemplos", "verificar_exemplos.py")], re.compile(r"\d+/\d+ exemplos")),
        Suite("ligar", [Py, "-I", ArqTestes("ligar", "verificar_ligar.py")], re.compile(r"\d+/\d+ casos de ligar")),
    ] + [
        Suite("rt", ["cmd", "/c", ArqTestes("runtime", "rodar_teste_rt.bat")], re.compile(r"\d+ verificacoes")),
        Suite("rt_nativo", [Py, "-I", ArqTestes("nativo", "rt", "verificar_rt_nativo.py")], re.compile(r"OK: \d+/\d+ iguais")),
        Suite("bir", [Py, "-I", ArqTestes("nativo", "bir", "verificar_bir.py")], re.compile(r"corpus \d+ arquivos[^;\r\n]*; falhas=\d+")),
        Suite("enc", [Py, "-I", ArqTestes("nativo", "enc", "verificar_enc.py")], re.compile(r"casos=\d+ iguais=\d+ equivalentes=\d+ falhas=\d+ recusados=\d+")),
        Suite("obj", [Py, "-I", ArqTestes("nativo", "obj", "verificar_obj.py")], re.compile(r"verificar_obj: \d+/\d+")),
        Suite("pe", [Py, "-I", ArqTestes("nativo", "pe", "verificar_pe.py")], re.compile(r"(?:OK|FALHOU): \d+/\d+ itens do PE")),
        Suite("unwind", [Py, "-I", ArqTestes("nativo", "unwind", "conferir_unwind.py")], re.compile(r"conferir_unwind: pontos=\d+ ok=\d+ falhas=\d+")),
        Suite("console", [Py, "-I", ArqTestes("nativo", "console", "conferir_console.py")], re.compile(r"checagens=\d+ ok=\d+ falhas=\d+")),
        Suite("regras", [Py, "-I", ArqTestes("nativo", "conferir_regras.py")], re.compile(r"arquivos=\d+ funcoes=\d+ violacoes=\d+")),
        Suite("otim", [Py, "-I", ArqTestes("nativo", "otim", "verificar_otim.py"), "--forja", Forja], re.compile(r"OK: \d+/\d+ alvos iguais")),
        Suite("desempenho", [Py, "-I", ArqTestes("semantica", "verificar_semantica.py"), "--desempenho"], re.compile(r"OK: portoes[^\r\n]*"), True),
    ] + [
        Suite(f"cong_{K}", [Py, "-I", ArqTestes("nativo", "otim", "fuzz_otim.py"), "--forja", Forja, "--fatia", str(K)],
              re.compile(r"fuzz_otim: \d+ programas, \d+ validos \([\d.]+%\), \d+ achados"), Grupo="congelado")
        for K in range(4)
    ] + [
        Suite(f"nativo_{Parte}", [Py, "-I", ArqTestes("nativo", "verificar_nativo.py"), "--forja", Forja, "--grupo", Parte],
              re.compile(r"[\w/*.]+: iguais=\d+ diferentes=\d+"), Grupo="nativo")
        for Parte in GRUPOS_NATIVO
    ] + [
        Suite("abi", [Py, "-I", ArqTestes("nativo", "abi", "verificar_abi.py"), "--forja", Forja, "--sementes", "1,2,3"],
              re.compile(r"verificar_abi: \d+ execucoes, \d+ iguais, \d+ diferentes")),
        Suite("projeto", [Py, "-I", ArqTestes("projeto", "verificar_projeto.py"), "--forja", Forja], re.compile(r"\d+/\d+ casos de projeto")),
        Suite("proc", [Py, "-I", ArqTestes("nativo", "m5", "proc", "verificar_proc.py"), "--forja", Forja], re.compile(r"\d+/\d+ casos de proc")),
        Suite("arquivo", [Py, "-I", ArqTestes("nativo", "m5", "arquivo", "verificar_arquivo.py"), "--forja", Forja], re.compile(r"\d+/\d+ casos de arquivo")),
        Suite("linux", [Py, "-I", ArqTestes("nativo", "linux", "verificar_linux.py"), "--forja", Forja],
              re.compile(r"\d+/\d+ casos de linux|linux: PULADA \(WSL/distro ausente\)")),
        Suite("b15", [Py, "-I", str(PASTA_IA_FIRST / "verificar_erros_b15.py"), "--forja", Forja, "--pasta", str(PASTA_IA_FIRST / "b15")],
              re.compile(r'"fixtures": \d+, "esperadas": \d+, "ok": \d+')),
        Suite("l3c2", [Py, "-I", str(PASTA_IA_FIRST / "verificar_l3c2.py"), "--forja", Forja], re.compile(r"casos=\d+ falhas=\d+")),
        Suite("l3c3", [Py, "-I", str(PASTA_IA_FIRST / "verificar_l3c3.py"), "--forja", Forja], re.compile(r"casos=\d+ falhas=\d+")),
        Suite("poda", [Py, "-I", ArqTestes("nativo", "m4", "poda", "verificar_poda.py"), "--forja", Forja], re.compile(r"poda: iguais=\d+ diferentes=\d+")),
        Suite("plantado", [Py, "-I", ArqTestes("nativo", "verificar_plantado.py"), "--forja", Forja], re.compile(r"plantadas=\d+ localizadas=\d+")),
        Suite("estatica", [Py, "-I", ArqTestes("nativo", "verificar_estatica.py")], re.compile(r"conjuntos iguais ao mapa do ld: \d+/\d+")),
        Suite("editar", [Py, "-I", ArqTestes("editar", "verificar_editar.py"), "--forja", Forja], re.compile(r'"id": "\w+", "valor": [^,]+, "meta": "[^"]*", "verde": \w+')),
        Suite("editar_i5", [Py, "-I", ArqTestes("editar", "verificar_editar_i5.py"), "--forja", Forja],
              re.compile(r'"id": "\w+", "valor": [^,]+, "meta": "[^"]*", "verde": \w+')),
        Suite("std_testes", [Py, "-I", ArqTestes("std", "verificar_std_testes.py"), "--forja", Forja], re.compile(r"OK: \d+ modulos, \d+ testes")),
        Suite("fmt", [Py, "-I", ArqTestes("fmt", "verificar_fmt.py"), "--forja", Forja],
              re.compile(r"OK: \d+ fixtures, \d+ sinteticos, \d+ arquivo, \d+ negativo, \d+ arquivos do corpus, \d+ programas de execucao")),
        Suite("cobertura_doc", [Py, "-I", ArqTestes("doc", "verificar_cobertura_doc.py"), "--forja", Forja], re.compile(r"verificar_cobertura_doc: \d+ falha\(s\)")),
        Suite("receitas", [Py, "-I", ArqTestes("receitas", "verificar_receitas.py"), "--forja", Forja, "--minimo", "40"], re.compile(r"OK: \d+ receitas, \d+ tags")),
        Suite("libs", [Py, str(TESTES.parent / "exemplos_libs" / "verificar_libs.py"), "--forja", Forja, "--backend", "x64"],
              re.compile(r"RESULTADO: tudo certo \(\d+/\d+ provas\)")),
        Suite("reload", [Py, "-I", ArqTestes("reload", "verificar_reload.py"), "--forja", Forja],
              re.compile(r"RESULTADO: tudo certo \(\d+/\d+ cenarios\)")),
        Suite("perfil", [Py, "-I", ArqTestes("nativo", "perfil", "verificar_perfil.py"), "--forja", Forja],
              re.compile(r"RESULTADO: tudo certo \(\d+/\d+ casos\)")),
        Suite("wasm", [Py, "-I", ArqTestes("nativo", "wasm", "verificar_wasm_bateria.py"), "--forja", Forja],
              re.compile(r"RESULTADO: tudo certo \(\d+/\d+ etapas\)|wasm: PULADA \(Node ausente\)")),
    ]


def FiltrarSuites(Suites, Pedido, Analisador):
    Nomes = [Nome.strip() for Nome in Pedido.split(",") if Nome.strip()]
    if not Nomes:
        return Suites
    Validos = {Alvo.Nome for Alvo in Suites} | {Alvo.Grupo for Alvo in Suites}
    Desconhecidos = [Nome for Nome in Nomes if Nome not in Validos]
    if Desconhecidos:
        Analisador.error("suite desconhecida: " + ", ".join(Desconhecidos))
    return [Alvo for Alvo in Suites if Alvo.Nome in Nomes or Alvo.Grupo in Nomes]


def PrepararCopia(Origem, Pasta):
    # Uma copia so para todas as suites; pasta anterior removida para que cada rodada comece limpa.
    if Pasta.exists():
        shutil.rmtree(Pasta, ignore_errors=True)
    Pasta.mkdir(parents=True)
    # A Forja acha a semente musl em <pasta do exe>\..\third\musl\x86_64 (nativo_biblioteca.c): a copia vai em bin\ e a semente ao lado, dentro da rodada.
    Semente = LINGUAGEM / "third" / "musl" / "x86_64"
    if Semente.is_dir():
        shutil.copytree(Semente, Pasta / "third" / "musl" / "x86_64")
    (Pasta / "bin").mkdir()
    Destino = Pasta / "bin" / "forja.exe"
    for Tentativa in range(1, TENTATIVAS_COPIA + 1):
        try:
            shutil.copy2(Origem, Destino)
            return Destino
        except PermissionError as Erro:
            # Arquivo em uso (outro agente reconstruindo o compilador): espera e tenta de novo.
            if Tentativa == TENTATIVAS_COPIA:
                raise
            print(f"copia em uso ({Erro}); nova tentativa em {ESPERA_COPIA} s", file=sys.stderr)
            time.sleep(ESPERA_COPIA)


def ExtrairContagem(Texto, Regex):
    # Todos os trechos que casam a regex, em ordem; sem casamento, mostra a ultima linha util para diagnostico.
    Achados = [Casa.group(0) for Casa in Regex.finditer(Texto)]
    if Achados:
        return " ; ".join(Achados)
    Linhas = [Linha.strip() for Linha in Texto.splitlines() if Linha.strip()]
    if not Linhas:
        return "(sem saida)"
    return "(sem casar a regex) " + Linhas[-1][:TAMANHO_ULTIMA_LINHA]


def ExecutarSuite(Alvo, PastaBase):
    # TEMP e TMP apontam para uma pasta nova da suite, para que suites nao disputem arquivos temporarios.
    PastaSuite = PastaBase / Alvo.Nome
    PastaSuite.mkdir(parents=True)
    AmbienteSuite = os.environ.copy()
    AmbienteSuite["TEMP"] = str(PastaSuite)
    AmbienteSuite["TMP"] = str(PastaSuite)
    print(f"inicio {Alvo.Nome}", file=sys.stderr)
    Inicio = time.monotonic()
    try:
        Proc = subprocess.run(Alvo.Comando, cwd=str(COMPILADOR), env=AmbienteSuite, stdin=subprocess.DEVNULL, capture_output=True)
        Rc = Proc.returncode
        Texto = (Proc.stdout + Proc.stderr).decode("utf-8", errors="replace")
    except OSError as Erro:
        Rc = -1
        Texto = f"nao iniciou: {Erro}"
    Duracao = time.monotonic() - Inicio
    (PastaBase / f"{Alvo.Nome}.log").write_text(Texto, encoding="utf-8")
    print(f"fim {Alvo.Nome} rc={Rc} {Duracao:.0f} s", file=sys.stderr)
    return Resultado(Alvo.Nome, Rc, ExtrairContagem(Texto, Alvo.Regex), Duracao)


def RodarBateria(Suites, Jobs, PastaBase, Tempos):
    """Roda as suites em paralelo; as mais demoradas (tempos.json) largam primeiro para a ultima nao sobrar sozinha."""
    Inicio = time.monotonic()
    Normais = sorted((Alvo for Alvo in Suites if not Alvo.Exclusiva), key=lambda Alvo: -Tempos.get(Alvo.Nome, 0))
    Exclusivas = [Alvo for Alvo in Suites if Alvo.Exclusiva]
    Feitos = {}
    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        Futuros = {Alvo.Nome: Piscina.submit(ExecutarSuite, Alvo, PastaBase) for Alvo in Normais}
        for Nome, Futuro in Futuros.items():
            Feitos[Nome] = Futuro.result()
    # Suites exclusivas rodam sozinhas: os portoes de tempo do desempenho nao valem com carga paralela.
    for Alvo in Exclusivas:
        Feitos[Alvo.Nome] = ExecutarSuite(Alvo, PastaBase)
    Parede = time.monotonic() - Inicio
    return [Feitos[Alvo.Nome] for Alvo in Suites], Parede


def LerJson(Arquivo):
    try:
        return json.loads(Arquivo.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def GravarJson(Arquivo, Dados):
    Arquivo.parent.mkdir(parents=True, exist_ok=True)
    Arquivo.write_text(json.dumps(Dados), encoding="utf-8")


def Varrer(Raiz):
    """Mapa caminho relativo (com /) -> [mtime_ns, tamanho] dos arquivos que o carimbo acompanha."""
    Achados = {}
    for Pasta, Extensoes in VARRIDAS.items():
        for Corrente, Subpastas, Arquivos in os.walk(Path(Raiz) / Pasta):
            Subpastas[:] = [Nome for Nome in Subpastas if Nome != "__pycache__"]
            for Nome in Arquivos:
                if os.path.splitext(Nome)[1].lower() not in Extensoes:
                    continue
                Caminho = Path(Corrente) / Nome
                Dados = Caminho.stat()
                Achados[Caminho.relative_to(Raiz).as_posix()] = [Dados.st_mtime_ns, Dados.st_size]
    return Achados


def Mudancas(Atual, Anterior):
    Tocados = [Caminho for Caminho, Dados in Atual.items() if Anterior.get(Caminho) != Dados]
    return sorted(Tocados + [Caminho for Caminho in Anterior if Caminho not in Atual])


def AreaDoCaminho(Caminho):
    """Area da tabela PREFIXOS; 'tests' para tests\\ e benchmark\\; 'desconhecida' para o resto."""
    if Caminho.startswith(DE_TESTES):
        return "tests"
    for Prefixo, Area in PREFIXOS:
        if Caminho.startswith(Prefixo):
            return Area
    return "desconhecida"


def SuitesDoArquivoDeTeste(Caminho, Suites):
    """Suites cujo script e o proprio arquivo; senao as do script na pasta mais funda que contem o arquivo."""
    Alvo = (LINGUAGEM / Caminho).resolve()
    Candidatas = []
    for Item in Suites:
        for Script in [Path(Arg).resolve() for Arg in Item.Comando if Arg.lower().endswith((".py", ".bat"))]:
            if Script == Alvo:
                return [Item.Grupo]
            if Alvo.is_relative_to(Script.parent):
                Candidatas.append((len(Script.parent.parts), Item.Grupo))
    if not Candidatas:
        return []
    Fundo = max(Profundidade for Profundidade, _ in Candidatas)
    return sorted({Nome for Profundidade, Nome in Candidatas if Profundidade == Fundo})


def ClassificarMudancas(Caminhos, Suites):
    """(areas conhecidas, suites vindas de arquivos de teste, houve area desconhecida)."""
    Areas, DeTeste, Desconhecida = set(), set(), False
    for Caminho in Caminhos:
        Area = AreaDoCaminho(Caminho)
        if Area == "tests":
            DeTeste.update(SuitesDoArquivoDeTeste(Caminho, Suites))
        elif Area == "desconhecida":
            Desconhecida = True
        else:
            Areas.add(Area)
    return Areas, DeTeste, Desconhecida


def Planejar(Modo, Areas, DeTeste, Suites, Tempos):
    """Nomes dos grupos de suites do modo (uma suite fatiada conta como um grupo), na ordem da tabela; completa = todos."""
    Grupos = list(dict.fromkeys(Item.Grupo for Item in Suites))
    if Modo == "completa":
        return Grupos
    Pedidas = set(NUCLEO) | set(DeTeste)
    for Area in Areas:
        Pedidas.update(AREAS[Area][0])
        if Modo == "media":
            Pedidas.update(AREAS[Area][1])
    if Modo == "rapida":
        Medido = {Grupo: max(Tempos.get(Item.Nome, 0) for Item in Suites if Item.Grupo == Grupo) for Grupo in Grupos}
        Pedidas = {Nome for Nome in Pedidas if Nome in NUCLEO or Nome in DeTeste or Medido.get(Nome, 0) <= LIMITE_RAPIDA}
    return [Grupo for Grupo in Grupos if Grupo in Pedidas]


def DescobrirAreas(Opcoes, Suites, Atual, Analisador):
    """(modo efetivo, areas, suites de teste, nota). --mudou explicito vence o carimbo; a completa ignora ambos."""
    Modo = Opcoes.modo
    if Modo == "completa":
        return Modo, set(), set(), "todas as suites"
    if Opcoes.mudou:
        Areas = {Nome.strip() for Nome in Opcoes.mudou.split(",") if Nome.strip()}
        Invalidas = sorted(Areas - set(AREAS))
        if Invalidas:
            Analisador.error("area desconhecida em --mudou: " + ", ".join(Invalidas) + " (validas: " + ", ".join(sorted(AREAS)) + ")")
        return Modo, Areas, set(), "--mudou"
    Carimbo = LerJson(PASTA_ESTADO / "ultimo_verde.json")
    if Carimbo is None:
        return ("media" if Modo == "rapida" else Modo), set(), set(), "sem carimbo: so o nucleo"
    Areas, DeTeste, Desconhecida = ClassificarMudancas(Mudancas(Atual, Carimbo), Suites)
    return ("media" if Desconhecida and Modo == "rapida" else Modo), Areas, DeTeste, "carimbo"


def PastaDaRodada(Opcoes):
    """completa sem --so usa a pasta de sempre; as rodadas curtas usam a delas (nao apagam a de uma completa em curso)."""
    if Opcoes.so:
        return PASTA_BATERIA.with_name("forja_bateria_so")
    return PASTA_BATERIA if Opcoes.modo == "completa" else PASTA_BATERIA.with_name("forja_bateria_" + Opcoes.modo)


def SuitesDaRodada(Opcoes, Suites, Atual, Analisador):
    """(modo, areas, suites escolhidas, nota); --so literal vence --modo."""
    if Opcoes.so:
        return "so", set(), FiltrarSuites(Suites, Opcoes.so, Analisador), "--so"
    Modo, Areas, DeTeste, Nota = DescobrirAreas(Opcoes, Suites, Atual, Analisador)
    Tempos = LerJson(PASTA_ESTADO / "tempos.json") or {}
    Nomes = set(Planejar(Modo, Areas, DeTeste, Suites, Tempos))
    return Modo, Areas, [Item for Item in Suites if Item.Grupo in Nomes], Nota


def GravarEstado(Opcoes, Modo, Atual, Resultados, Falhas):
    """Tempos de cada suite sempre; carimbo so com media ou completa verde, sem --mudou e sem --so."""
    Tempos = LerJson(PASTA_ESTADO / "tempos.json") or {}
    Tempos.update({Item.Nome: int(Item.Duracao) for Item in Resultados})
    GravarJson(PASTA_ESTADO / "tempos.json", Tempos)
    if Falhas == 0 and Modo in ("media", "completa") and not Opcoes.mudou and not Opcoes.so:
        GravarJson(PASTA_ESTADO / "ultimo_verde.json", Atual)


def LerArgumentos():
    Analisador = argparse.ArgumentParser(description="Bateria do compilador Forja em 3 modos (rapida, media, completa), uma suite por processo.")
    Analisador.add_argument("--modo", choices=MODOS, default="completa", help="rapida (laco de edicao), media (portao de fatia) ou completa (padrao)")
    Analisador.add_argument("--mudou", default="", help="areas separadas por virgula (substitui a deteccao pelo carimbo)")
    Analisador.add_argument("--forja", default=ArqCompilador("forja.exe"), help="forja.exe a testar (copiado uma vez para a pasta da rodada)")
    Analisador.add_argument("--jobs", type=int, default=os.cpu_count() or 1, help="suites em paralelo (padrao: processadores logicos)")
    Analisador.add_argument("--so", default="", help="suites separadas por virgula; vence --modo")
    Analisador.add_argument("--listar", action="store_true", help="mostra o plano e nao roda")
    Analisador.add_argument("--selftest", action="store_true")
    Opcoes = Analisador.parse_args()
    if Opcoes.jobs < 1:
        Analisador.error("--jobs precisa ser >= 1")
    return Analisador, Opcoes


def Verificar(Condicao, Mensagem, Falhas):
    print(("ok    " if Condicao else "FALHOU ") + Mensagem)
    if not Condicao:
        Falhas.append(Mensagem)


def ConferirCobertura(Falhas):
    """As fatias cobrem tudo sem repetir: cada pasta do verificar_nativo cai em um dos GRUPOS_NATIVO (e cada grupo tem pasta); --parte recusa K/N invalido."""
    Antes = list(sys.path)
    sys.path[:0] = [ArqTestes("nativo"), ArqTestes("cli")]
    try:
        import verificar_nativo
        import verificar_cli
        Grupos = [verificar_nativo.GrupoDaPasta(Pasta[0]) for Pasta in verificar_nativo.ListarPastas()]
        Verificar(set(Grupos) == set(GRUPOS_NATIVO) | {"lowering"}, "pastas do nativo cobertas pelos grupos (lowering e do lowering_x64), nenhum vazio", Falhas)
        Verificar(all(verificar_cli.LerParte(Texto) is None for Texto in ("0/4", "5/4", "x", "1", "1/0")) and verificar_cli.LerParte("2/4") == (2, 4),
                  "cli --parte K/N valida K/N", Falhas)
    finally:
        sys.path[:] = Antes


def Selftest():
    Falhas = []
    Suites = MontarSuites("forja.exe")
    Nomes = list(dict.fromkeys(Item.Grupo for Item in Suites))
    Citadas = set(NUCLEO) | {Nome for Par in AREAS.values() for Lista in Par for Nome in Lista}
    Verificar(Citadas <= set(Nomes), "toda suite das tabelas existe (faltam: " + ",".join(sorted(Citadas - set(Nomes))) + ")", Falhas)
    Verificar(len({Item.Nome for Item in Suites}) == len(Suites), "nomes de suite sem repeticao", Falhas)
    Verificar([Item.Nome for Item in Suites if Item.Grupo == "cli"] == [f"cli_{K}" for K in range(1, PARTES_CLI + 1)], "cli fatiado nas partes declaradas", Falhas)
    Esperado = [Nome for Nome in Nomes if Nome in set(NUCLEO) | {"rodar_suite", "bir", "poda", "lowering"}]
    Verificar(Planejar("media", {"ir"}, set(), Suites, {}) == Esperado, "media da area ir = nucleo + rapida + media na ordem da tabela", Falhas)
    Verificar(Planejar("rapida", {"ir"}, set(), Suites, {"bir": 99}) == [Nome for Nome in Esperado if Nome not in ("bir", "lowering")],
              "rapida pula suite medida acima de 45 s e nao soma a media", Falhas)
    Verificar(Planejar("completa", set(), set(), Suites, {}) == Nomes, "completa = todas as suites", Falhas)
    Verificar(ClassificarMudancas(["compiler/xyz/a.c"], Suites)[2], "arquivo fora da tabela = area desconhecida", Falhas)
    Verificar(SuitesDoArquivoDeTeste("tests/nativo/bir/verificar_bir.py", Suites) == ["bir"], "arquivo de teste -> a suite do proprio script", Falhas)
    Verificar(SuitesDoArquivoDeTeste("tests/cli/verificar_cli.py", Suites) == ["cli"], "arquivo do cli -> o grupo cli (todas as partes)", Falhas)
    Verificar([Item.Nome for Item in FiltrarSuites(Suites, "cli", None)] == [f"cli_{K}" for K in range(1, PARTES_CLI + 1)], "--so cli pega todas as partes", Falhas)
    Verificar("linux" in Nomes and all("linux" in Planejar("media", {Area}, set(), Suites, {}) and "linux" not in Planejar("rapida", {Area}, set(), Suites, {})
                                         for Area in ("runtime", "nativo_obj", "nativo_x64")), "suite linux: so na media (runtime, nativo_obj, nativo_x64) e na completa", Falhas)
    ConferirCobertura(Falhas)
    with tempfile.TemporaryDirectory() as Provisoria:
        Alvo = Path(Provisoria) / "compiler" / "semantic" / "a.c"
        Alvo.parent.mkdir(parents=True)
        Alvo.write_text("int a;\n")
        Antes = Varrer(Provisoria)
        Alvo.write_text("int a;\nint b;\n")
        Tocados = Mudancas(Varrer(Provisoria), Antes)
        Verificar(Tocados == ["compiler/semantic/a.c"] and ClassificarMudancas(Tocados, Suites)[0] == {"semantic"},
                  "carimbo detecta o .c tocado e a area semantic", Falhas)
        Real = ExecutarSuite(Suites[0], Path(Provisoria) / "rodada")
        Verificar(Real.Nome == "teste_common" and Real.Rc == 0 and Real.Duracao < 15, f"suite real: teste_common rc={Real.Rc} em {Real.Duracao:.1f} s", Falhas)
    print("selftest:", "OK" if not Falhas else f"{len(Falhas)} falhas")
    return 0 if not Falhas else 1


def Principal():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    Analisador, Opcoes = LerArgumentos()
    if Opcoes.selftest:
        return Selftest()
    Pasta = PastaDaRodada(Opcoes)
    Atual = {} if Opcoes.so else Varrer(LINGUAGEM)
    Modo, Areas, Escolhidas, Nota = SuitesDaRodada(Opcoes, MontarSuites(str(Pasta / "bin" / "forja.exe")), Atual, Analisador)
    Resumo = f"modo={Modo} areas={','.join(sorted(Areas)) or '-'} ({Nota})"
    if Opcoes.listar:
        print(Resumo, "suites=" + ",".join(Item.Nome for Item in Escolhidas))
        return 0
    Origem = Path(Opcoes.forja).resolve()
    if not Origem.is_file():
        Analisador.error(f"forja nao encontrado: {Origem}")
    PrepararCopia(Origem, Pasta)
    Resultados, Parede = RodarBateria(Escolhidas, Opcoes.jobs, Pasta, LerJson(PASTA_ESTADO / "tempos.json") or {})
    for Item in Resultados:
        print(f"{Item.Nome} | {Item.Rc} | {Item.Contagem}")
    Falhas = sum(1 for Item in Resultados if Item.Rc != 0)
    GravarEstado(Opcoes, Modo, Atual, Resultados, Falhas)
    print(Resumo)
    print(f"bateria: {len(Resultados)} suites, {Falhas} falhas, parede {Parede:.0f} s, soma {sum(Item.Duracao for Item in Resultados):.0f} s, jobs={Opcoes.jobs}")
    return 0 if Falhas == 0 else 1


if __name__ == "__main__":
    sys.exit(Principal())
