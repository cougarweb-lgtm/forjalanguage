#!/usr/bin/env python3
"""verificar_linux.py - verifica o alvo x86_64-linux (ELF64) no WSL alpine-build (M5 P28).

Objetivo: cada caso (casos_linux.py) e compilado no Windows com `--target x86_64-linux` (dev e release), rodado no Linux dentro de UM lote (wsl_lote.py) e comparado
com o oraculo, em ordem: (1) o esperado do caso (.esperado/.codigo/.exit, ou a tabela portada ao Linux); (2) sem esperado, a saida do x64-windows da mesma rodada
(CRLF normalizado); (3) o C-musl (oraculo_c.py) SO para os casos da lista oraculo_c.txt, alem do esperado. divergencias_aceitas.json (no maximo 3 entradas) vale
para o que nao e diferenca de SO. Alem dos casos: dois builds com bytes iguais (fontes da prova) e readelf/file sem aviso (verificar_elf_wsl.ConferirElf).

Uso:   python -I verificar_linux.py --forja <copia do forja.exe> [--pasta <pasta de .fj>]  (sem --pasta: a suite inteira, conjunto_linux.py) [--modos dev,release] [--so <trecho do nome>] [--json] [--jobs N] [--relatorio <arq.json>]
       R8 (rodada enxuta): --tempos | --perfil fatia|completo (padrao completo) | --cache/--sem-cache/--manifesto <arq> | --congelar-oraculo <arq.json> | --oraculo-congelado <arq.json>
       (o perfil fatia NAO substitui a rodada completa; cache e congelado em cache_linux.py; o contrato esta na secao R8 de gaveta/contrato_m7a.txt).
       P3 linux_c: --congelar-c (grava o C-musl dos 13 casos de oraculo_c.txt, forja_g1 + gcc, so onde C == x64; so esses casos) | --contra-congelado-c (no-op desde 0.21.0 F3: o padrao ja compara esses 13 com o JSON selado, sem gcc).
Saida: uma linha JSON por caso com falha (todos com --json): {"caso","modo","ok","oraculo","motivo","divergencia":{"indice","esperado","obtido"}}; no fim
       "verificar_linux: <ok>/<total> casos de linux". Exit 0 so com tudo verde. Sem a distro: "linux: PULADA (WSL/distro ausente)" e exit 0.
Escrita: %TEMP%\\forja_linux\\linux (Windows) e /tmp/forja_lote_* (Linux, apagado pelo runner).  Reversao: apagar a pasta nativo\\linux.
Verificacao: python -I verificar_linux.py --selftest

Versao: 0.8.0
Changelog: 0.8.0 (18/11/2026) 0.21.0 F3: o padrao dos 13 casos de oraculo_c.txt e o C-musl do JSON selado (sem gcc, sem forja_g1); --contra-congelado-c vira no-op; so --congelar-c gera o C-musl; os demais casos seguem vivos.
           0.7.0 (18/11/2026) P3 linux_c - --congelar-c e --contra-congelado-c (so os 13 casos de oraculo_c.txt): o C-musl vem do JSON selado; sem ele, o gcc nao roda; RodarLote devolve o C-musl ao Coletor.
           0.6.2 (18/11/2026) P3 - o sha do ELF do manifesto e do alarme R2 sai sem os bytes de versao do forja (cache_linux.ShaElf).
           0.6.1 (17/11/2026) M7a F9a - o build release sai SEM --checagem (o padrao do release e sim: a rodada inteira prova o padrao; o ELF e o mesmo de --checagem sim).
           0.6.0 (17/11/2026) R8 passos 2-7 - --perfil fatia|completo (determinismo reduzido; cache por hash do ELF), lotes WSL em paralelo (o serial: teste, pasta, proc, arquivo), --congelar-oraculo/--oraculo-congelado,
                   --cache/--sem-cache/--manifesto (cache_linux.py), alarme R2, aviso de rodada completa; determinismo e estrutura contam a parte do total de casos (linha propria no resumo).
           0.5.0 (17/11/2026) R8 passo 1 - --tempos (tempo por fase: build, oraculo, musl/classificacao, lotes WSL, determinismo, estrutura).
           0.4.2 (16/11/2026) O0 P5 - o padrao do release passou a --checagem nao; o build release leva --checagem sim (os .codigo de indice e nulo valem nos dois modos).
           0.4.1 (15/11/2026) M6 P69 - AvaliarContraWindows normaliza a linha R do ELF (rcode_normalizar): o dev do Linux passou a ter pilha.
           0.4.0(14/11/2026) M5 P29b - Modo "teste" (default de --modos: dev,release,teste): o ELF e o exe de `forja test --target x86_64-linux -o`, rodado com FORJA_TESTE_JSON/FILTRO no ambiente e comparado (stdout byte a byte,
                   exit, stderr so nos campos da linha R) com o `forja test` do Windows da mesma entrada (PreencherOraculoTeste: variantes da mesma entrada em fila, o exe temporario do driver e por fonte);
                   esperado_so tambem para esses casos; determinismo so do ELF no Modo teste (--debug-code so vale em build); fora_queda: sinal 11 fora do executor.
           0.3.0 (14/11/2026) M5 P29a - sem --pasta roda a suite inteira (conjunto_linux.py): build X0003 = PULADO por desenho (contado e listado), oraculo x64-windows em paralelo e com stderr,
                   um lote do WSL por grupo, determinismo (ELF e .debug.jsonl) em todos os casos do conjunto; --jobs e --relatorio.
           0.2.0 (14/11/2026) M5 P28b-1 - o stdin do caso (Caso.Entrada) vai ao lote; determinismo tambem para as fixtures pasta/ (es/) e, em es/handle.fj, para o .debug.jsonl (sem o registro tempos).
           0.1.0 (14/11/2026) M5 P28 - primeira versao.
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
import zlib
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
sys.path.insert(0, str(AQUI.parents[1]))  # tests\: rcode_normalizar (P69)
from rcode_normalizar import NormalizarLinhasR  # noqa: E402
import cache_linux  # noqa: E402
import casos_linux  # noqa: E402
import conjunto_linux  # noqa: E402
import oraculo_c  # noqa: E402
import wsl_lote  # noqa: E402

LANGUAGE = AQUI.parents[2]
TEMPO_BUILD = 120
MODO_TESTE = "teste"  # P29b: o caso e o exe de `forja test` (nao existe --release no test)
TEMPO_EXE_WINDOWS = 60
MAX_ACEITAS = 3
NOME_PROGRAMA_MUSL = "#musl"
CODIGO_PULADO = "X0003"  # o alvo Linux recusa foreign com link/dll: pulado por desenho
RECUSAS_DO_ORACULO = ("X0001", "X0003")  # o x64-windows tambem recusa: sem oraculo, pulado
REGEX_CODIGO = re.compile(r'"codigo":\s*"([A-Z]\d{4})"')
CAMPOS_DO_RUNTIME = ("codigo", "tipo", "arquivo", "linha", "causa")  # a linha R do exe; o driver do Windows acrescenta explicacao, forma_certa e spec
PERFIL_FATIA = "fatia"
PERFIL_COMPLETO = "completo"
AMOSTRA_DEBUG_FATIA = 25  # perfil fatia: o .debug.jsonl de 1 em cada 25 chaves (crc32 do nome do caso: fixo e deterministico) alem do es/handle
GRUPOS_SERIAIS = ("teste", "pasta", "proc", "arquivo")  # portas do net, processos, arquivos e vazamento: um lote so, em serie
LOTES_PARALELOS = 4  # wsl.exe ao mesmo tempo (os grupos fora de GRUPOS_SERIAIS; o lote serial conta como um)
CASOS_POR_LOTE = 250  # grupo com mais casos que isso se parte em fatias (o lowering sozinho passava de 30 s e era o piso do paralelo)
SEGUNDOS_DOS_LOTES: dict = {}  # suite -> segundos do wsl.exe (so para o --tempos)
MODOS_PADRAO = "dev,release,teste"
AVISO_FATIA = ("AVISO perfil fatia: nao substitui a rodada COMPLETA, obrigatoria no F9/fecho de marco, ao mudar forja_rt.h/runtime embutido, conjunto_linux/casos_linux/esperado_so/distro "
               "ou quando o hash de um ELF dev/teste mudar (R2 violada).")


class Cronometro:
    """O tempo de cada fase da rodada (--tempos); so mede, nao muda comportamento."""

    def __init__(self) -> None:
        self.Fases: dict = {}
        self.Inicio = time.perf_counter()

    @contextmanager
    def Fase(self, Nome: str):
        Antes = time.perf_counter()
        try:
            yield
        finally:
            self.Fases[Nome] = self.Fases.get(Nome, 0.0) + time.perf_counter() - Antes

    def Resumo(self) -> str:
        Total = time.perf_counter() - self.Inicio
        Partes = ", ".join(f"{Nome} {Segundos:.1f}s" for Nome, Segundos in self.Fases.items())
        Lotes = ", ".join(f"{Suite} {Segundos:.1f}s" for Suite, Segundos in sorted(SEGUNDOS_DOS_LOTES.items(), key=lambda Item: -Item[1]))
        return f"tempos: {Partes}; soma das fases {sum(self.Fases.values()):.1f}s de {Total:.1f}s de relogio\ntempos dos lotes WSL: {Lotes}"


def ComandoDeConstruir(Forja, Entrada: str, Modo: str, Alvo: str, Saida: Path, Extra: tuple = ()) -> list:
    """build (dev/release) ou, no Modo "teste" (P29b), `forja test --target <alvo> -o <saida> <entrada>` (so constroi o exe de teste; a entrada e o caminho relativo a language\\ ou std:<m>)."""
    if Modo == MODO_TESTE:
        return [str(Forja), "test", "--target", Alvo, *Extra, Entrada, "-o", str(Saida)]
    return [str(Forja), "build", "--target", Alvo, *Extra] + (["--release"] if Modo == "release" else []) + [Entrada, "-o", str(Saida)]


def Construir(Forja, Entrada, Modo: str, Alvo: str, Saida: Path) -> str | None:
    Processo = subprocess.run(ComandoDeConstruir(Forja, str(Entrada), Modo, Alvo, Saida), capture_output=True, timeout=TEMPO_BUILD, stdin=subprocess.DEVNULL, cwd=LANGUAGE)
    if Processo.returncode != 0 or not Saida.is_file():
        return f"build {Alvo} exit {Processo.returncode}: " + (Processo.stdout + Processo.stderr).decode("utf-8", "replace")[:1500]
    return None


def PrimeiraDivergencia(Esperado: bytes, Obtido: bytes) -> dict | None:
    if Esperado == Obtido:
        return None
    Limite = min(len(Esperado), len(Obtido))
    Indice = next((I for I in range(Limite) if Esperado[I] != Obtido[I]), Limite)
    Byte = lambda Dado: f"0x{Dado[Indice]:02x}" if Indice < len(Dado) else "fim"
    return {"indice": Indice, "esperado": Byte(Esperado), "obtido": Byte(Obtido), "tamanhos": [len(Esperado), len(Obtido)]}


def CarregarAceitas() -> list:
    Aceitas = json.loads((AQUI / "divergencias_aceitas.json").read_text(encoding="utf-8"))["aceitas"]
    if len(Aceitas) > MAX_ACEITAS:
        raise SystemExit(f"divergencias_aceitas.json tem {len(Aceitas)} entradas (maximo {MAX_ACEITAS}): Regra #24, mude o angulo")
    return Aceitas


def EntradaDoCaso(Caso) -> str:
    """O texto que vai ao forja: caminho absoluto no build; no Modo teste o alvo std:<m> ou o caminho relativo a language\\ (o arquivo das mensagens `expect falhou` e o mesmo nos 2 alvos)."""
    if Caso.Teste is None:
        return str(Caso.Fonte)
    return Caso.Teste["alvo"] or casos_linux.EntradaRelativa(Caso.Fonte)


def ChaveDoElf(Caso) -> tuple:
    return (EntradaDoCaso(Caso), Caso.Modo)


def Paralelos() -> int:
    return min(8, os.cpu_count() or 4)


def ConstruirTodos(Forja, Casos: list, Pasta: Path, Falhas: dict, Jobs: int = 0) -> dict:
    """ELF por (fonte, modo), em paralelo (Jobs; 0 = padrao); erro de build vai para Falhas[chave]."""
    Chaves = sorted({ChaveDoElf(C) for C in Casos})
    Elfs = {}

    def Um(Indice_Chave):
        Indice, Chave = Indice_Chave
        Saida = Pasta / "elf" / f"{Indice}.elf"
        return Chave, Saida, Construir(Forja, Chave[0], Chave[1], "x86_64-linux", Saida)

    (Pasta / "elf").mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=Jobs or Paralelos()) as Piscina:
        for Chave, Saida, Erro in Piscina.map(Um, enumerate(Chaves)):
            if Erro:
                Falhas[Chave] = Erro
            else:
                Elfs[Chave] = Saida
    return Elfs


def DebugSemTempos(Arquivo: Path) -> list:
    """O .debug.jsonl sem o registro `tempos` (os ns de cada etapa mudam de uma rodada para outra)."""
    Linhas = Arquivo.read_text(encoding="utf-8").splitlines()
    return [L for L in Linhas if json.loads(L).get("tipo") != "tempos"]


def DeterminismoDaChave(Forja, Chave: tuple, Elf: Path, Pasta: Path, Nome: str, ComDebug: bool) -> list:
    Copia = Pasta / "elf" / ("repetido_" + Elf.name)
    Erro = Construir(Forja, Chave[0], Chave[1], "x86_64-linux", Copia)
    Motivo = Erro or (None if Copia.read_bytes() == Elf.read_bytes() else "dois builds com bytes diferentes")
    Resultados = [{"caso": f"determinismo/{Nome}", "modo": Chave[1], "ok": Motivo is None, "oraculo": "dois builds", "motivo": Motivo}]
    if Motivo is None and ComDebug:
        Resultados.append(ConferirDebugJsonl(Forja, Chave[0], Chave[1], Pasta, Elf.stem, Nome))
    return Resultados


def SelecionarDeterminismo(Casos: list, Elfs: dict, Todos: bool = False, Fatia: bool = False) -> list:
    """[(chave do ELF, nome do 1o caso, com .debug.jsonl)] das chaves cujo determinismo se confere: prova e fixtures es (pasta/); com Todos (P29a) o conjunto inteiro e o .debug.jsonl tambem
    (es\\handle desde o P28b-1; --debug-code so vale em build, o test nao o tem). Perfil fatia (R8): so 1 build repetido das chaves RELEASE (mais o es/handle) e o .debug.jsonl so no es/handle e
    em 1 de cada AMOSTRA_DEBUG_FATIA chaves (crc32 do nome do caso)."""
    Nomes = {}
    for C in Casos:
        if (Todos or C.Nome.startswith(("prova/", "pasta/"))) and ChaveDoElf(C) in Elfs:
            Nomes.setdefault(ChaveDoElf(C), C.Nome)
    Itens = []
    for Chave, Nome in sorted(Nomes.items()):
        EsHandle = Chave[0].replace("\\", "/").endswith("/es/handle.fj")
        if Fatia and Chave[1] != "release" and not EsHandle:
            continue
        Amostra = not Fatia or zlib.crc32(Nome.encode("utf-8")) % AMOSTRA_DEBUG_FATIA == 0
        Itens.append((Chave, Nome, EsHandle or (Todos and Chave[1] != MODO_TESTE and Amostra)))
    return Itens


def ConferirDeterminismo(Forja, Itens: list, Elfs: dict, Pasta: Path, Jobs: int = 0) -> list:
    """Dois builds da mesma fonte precisam dar bytes iguais (P27b); Itens vem de SelecionarDeterminismo. O .debug.jsonl e comparado sem o registro `tempos`."""
    with ThreadPoolExecutor(max_workers=Jobs or Paralelos()) as Piscina:
        Listas = Piscina.map(lambda Item: DeterminismoDaChave(Forja, Item[0], Elfs[Item[0]], Pasta, Item[1], Item[2]), Itens)
        return [R for Lista in Listas for R in Lista]


def ConferirDebugJsonl(Forja, Entrada: str, Modo: str, Pasta: Path, Marca: str, Nome: str) -> dict:
    """Dois builds com --debug-code json dao o mesmo .debug.jsonl (fora o registro `tempos`)."""
    Jsonls = []
    for N in (1, 2):
        Saida = Pasta / "elf" / f"debug_{Marca}_{N}.elf"
        Comando = ComandoDeConstruir(Forja, Entrada, Modo, "x86_64-linux", Saida, ("--debug-code", "json"))
        Processo = subprocess.run(Comando, capture_output=True, timeout=TEMPO_BUILD, stdin=subprocess.DEVNULL, cwd=LANGUAGE)
        Jsonl = Path(str(Saida) + ".debug.jsonl")
        if Processo.returncode != 0 or not Jsonl.is_file():
            return {"caso": f"determinismo-debug/{Nome}", "modo": Modo, "ok": False, "oraculo": "dois builds", "motivo": f"build {N} exit {Processo.returncode}"}
        Jsonls.append(DebugSemTempos(Jsonl))
    Motivo = None if Jsonls[0] == Jsonls[1] and Jsonls[0] else ".debug.jsonl diferente entre dois builds (ou vazio)"
    return {"caso": f"determinismo-debug/{Nome}", "modo": Modo, "ok": Motivo is None, "oraculo": "dois builds", "motivo": Motivo}


def ConferirEstrutura(Elfs: dict, Casos: list) -> list:
    """readelf sem aviso e file = ELF estatico x86-64 para o ola dev/release (ConferirElf do verificar_elf_wsl)."""
    Elf_Wsl = casos_linux.Carregar(casos_linux.TESTES / "verificar_elf_wsl.py", "verificar_elf_wsl_linux")
    PastaLinux = f"/tmp/forja_estrutura_{os.getpid()}"
    Elf_Wsl.Wsl(f"mkdir -p '{PastaLinux}'")
    Resultados = []
    try:
        for Caso in Casos:
            if Caso.Nome != "prova/ola" or ChaveDoElf(Caso) not in Elfs:
                continue
            Falhas = Elf_Wsl.ConferirElf(Elfs[ChaveDoElf(Caso)], PastaLinux, Caso.Modo)
            Resultados.append({"caso": "estrutura/ola", "modo": Caso.Modo, "ok": not Falhas, "oraculo": "readelf+file", "motivo": "; ".join(Falhas) or None})
    finally:
        Elf_Wsl.Wsl(f"rm -rf '{PastaLinux}'")
    return Resultados


def RodarNoWindows(Forja, Caso, Indice: int, Pasta: Path) -> tuple:
    """(erro de build ou execucao, Rodada) do x64-windows; cada execucao tem a propria pasta de trabalho."""
    Exe = Pasta / "win" / f"{Indice}.exe"
    Trabalho = Pasta / "win" / f"cwd{Indice}"
    Trabalho.mkdir(parents=True, exist_ok=True)
    Erro = Construir(Forja, Caso.Fonte, Caso.Modo, "x86_64-windows", Exe)
    if Erro:
        return Erro, None
    try:
        return None, subprocess.run([str(Exe)] + [A.replace("@D@", str(Pasta)) for A in Caso.Args], capture_output=True, timeout=TEMPO_EXE_WINDOWS,
                                    stdin=subprocess.DEVNULL, env={**os.environ, **Caso.Env}, cwd=Trabalho)
    except subprocess.TimeoutExpired:
        return f"x64-windows passou de {TEMPO_EXE_WINDOWS} s", None


def ReaproveitarCongelados(Primeiros: dict, Congelado: dict) -> dict:
    """{chave: (None, rodada)} das chaves com resultado no oraculo congelado (so os casos release dos grupos independentes, ver cache_linux.UsaCongelado)."""
    Usados = {}
    for Chave, Caso in Primeiros.items():
        Item = Congelado.get(cache_linux.ChaveCongelada(Caso)) if cache_linux.UsaCongelado(Caso) else None
        if Item:
            Usados[Chave] = (None, cache_linux.RodadaCongelada(Item))
    return Usados


def PreencherOraculoWindows(Forja, Casos: list, Pasta: Path, Jobs: int = 0, *, Congelado: dict | None = None, Coletor: dict | None = None) -> dict:
    """Casos sem esperado: roda o x64-windows (sem Dados nem Pre), em paralelo, e usa a saida dele (CRLF normalizado; o stderr tambem nos Pulaveis).
    Devolve {chave: erro} das chaves em que o proprio x64-windows nao deu oraculo (casos Pulaveis; nos demais o erro vira ErroTem, como no P28).
    Congelado (R8): {chave congelada: item}; as chaves nele nao rodam no Windows. Coletor: recebe {chave congelada: item} do que rodou (--congelar-oraculo)."""
    Primeiros = {}
    for Caso in (C for C in Casos if C.Esperado is None and C.Teste is None):
        Primeiros.setdefault(ChaveDoElf(Caso), Caso)
    Usados = ReaproveitarCongelados(Primeiros, Congelado) if Congelado is not None else {}
    ARodar = {Chave: Caso for Chave, Caso in Primeiros.items() if Chave not in Usados}
    (Pasta / "win").mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=Jobs or Paralelos()) as Piscina:
        Lista = list(Piscina.map(lambda Item: RodarNoWindows(Forja, Item[1], Item[0], Pasta), enumerate(ARodar.values())))
    Cache = {**Usados, **dict(zip(ARodar, Lista))}
    if Coletor is not None:
        Coletor.update({cache_linux.ChaveCongelada(ARodar[Chave]): cache_linux.ItemCongelado(Rodada) for Chave, (_, Rodada) in zip(ARodar, Lista) if Rodada is not None})
    SemOraculo = {}
    for Caso in (C for C in Casos if C.Esperado is None and C.Teste is None):
        Erro, Rodada = Cache[ChaveDoElf(Caso)]
        if Congelado is not None:
            Caso.OrigemOraculo = cache_linux.ORIGEM_CONGELADO if ChaveDoElf(Caso) in Usados else cache_linux.ORIGEM_MESMA_RODADA
        if Erro and Caso.Pulavel:
            SemOraculo[ChaveDoElf(Caso)] = Erro
        Caso.Esperado = b"" if Rodada is None else Rodada.stdout.replace(b"\r\n", b"\n")
        Caso.Exit = 0 if Rodada is None else Rodada.returncode
        Caso.ErroTem = None if Caso.Pulavel else Erro
        if Caso.Pulavel and Rodada is not None:
            Caso.ErroExato = Rodada.stderr.replace(b"\r\n", b"\n")
    return SemOraculo


def NormalizarStderrDoTeste(Bruto: bytes) -> bytes:
    """O stderr do `forja test` do Windows passa pelo driver, que enriquece a linha R (explicacao, forma_certa, spec); o exe Linux imprime so a linha do runtime. Fica a linha com os campos do runtime."""
    Linhas = []
    for Linha in Bruto.replace(b"\r\n", b"\n").split(b"\n"):
        try:
            Dado = json.loads(Linha) if Linha.startswith(b"{") else None
        except ValueError:
            Dado = None
        Linhas.append(json.dumps({Campo: Dado[Campo] for Campo in CAMPOS_DO_RUNTIME if Campo in Dado}, ensure_ascii=False).encode("utf-8") if isinstance(Dado, dict) else Linha)
    return b"\n".join(Linhas)


def RodarTesteNoWindows(Forja, Caso) -> tuple:
    """(erro, (stdout, stderr normalizado, exit)) do `forja test [--json] [--filtro T] <entrada>` no Windows, com a mesma entrada do build Linux (cwd = language\\)."""
    Opcoes = (["--json"] if Caso.Teste["json"] else []) + (["--filtro", Caso.Teste["filtro"]] if Caso.Teste["filtro"] else [])
    try:
        Rodada = subprocess.run([str(Forja), "test", *Opcoes, EntradaDoCaso(Caso)], capture_output=True, timeout=TEMPO_BUILD + TEMPO_EXE_WINDOWS, stdin=subprocess.DEVNULL, cwd=LANGUAGE)
    except subprocess.TimeoutExpired:
        return "forja test do Windows passou do tempo", None
    return None, (Rodada.stdout.replace(b"\r\n", b"\n"), NormalizarStderrDoTeste(Rodada.stderr), Rodada.returncode)


def PreencherOraculoTeste(Forja, Casos: list, Jobs: int = 0) -> None:
    """Casos do executor de testes com ComparaWin: roda o `forja test` do Windows (em paralelo) e guarda em OraculoWin; sem esperado do arquivo, o stdout e o exit dele valem."""
    Lista = [C for C in Casos if C.Teste is not None and C.ComparaWin]
    PorEntrada = {}
    for Caso in Lista:  # o exe temporario do forja test vem do hash do fonte (sem as opcoes): as variantes da mesma entrada rodam em fila, nunca juntas
        PorEntrada.setdefault(EntradaDoCaso(Caso), []).append(Caso)
    with ThreadPoolExecutor(max_workers=Jobs or Paralelos()) as Piscina:
        Grupos = list(Piscina.map(lambda Grupo: [RodarTesteNoWindows(Forja, Caso) for Caso in Grupo], PorEntrada.values()))
    Rodadas = {id(Caso): Rodada for Grupo, Resultados in zip(PorEntrada.values(), Grupos) for Caso, Rodada in zip(Grupo, Resultados)}
    for Caso in Lista:
        Erro, Dados = Rodadas[id(Caso)]
        Caso.OraculoWin = Dados if Erro is None else (b"", Erro.encode(), -1)
        if Caso.Esperado is None:
            Caso.Esperado, Caso.Exit = Caso.OraculoWin[0], Caso.OraculoWin[2]


def AvaliarContraWindows(Caso, R: wsl_lote.Resultado) -> list:
    """Motivos de divergencia entre o executor Linux e o `forja test` do Windows (stdout byte a byte, exit e stderr so nos campos do runtime)."""
    Saida, Erro, Codigo = Caso.OraculoWin
    Motivos = []
    if R.Stdout != Saida:
        Motivos.append(f"stdout difere do forja test do Windows: {PrimeiraDivergencia(Saida, R.Stdout)}")
    if R.Exit != Codigo:
        Motivos.append(f"exit {R.Exit} x {Codigo} no Windows")
    if NormalizarLinhasR(R.Stderr.replace(b"\r\n", b"\n")) != Erro:  # P69: o ELF dev traz coluna/valor/pilha; o Windows ja vem so com os campos do runtime
        Motivos.append(f"stderr difere do forja test do Windows: {PrimeiraDivergencia(Erro, R.Stderr)}")
    return Motivos


def ProgramaDoCaso(Caso, Elf: Path, Sufixo: str = "") -> wsl_lote.Programa:
    return wsl_lote.Programa(f"{Caso.Nome}/{Caso.Modo}{Sufixo}", Elf, Caso.Args, Caso.Pre, Caso.Env, Caso.Usuario, Dados=Caso.Dados, Pos=Caso.Pos, Entrada=Caso.Entrada)


def CompilarMusl(Forja, Casos: list, Pasta: Path) -> dict:
    """{(fonte, modo): ELF do C-musl ou texto de erro} para os casos da lista oraculo_c.txt."""
    Itens, Erros = [], {}
    for Indice, Chave in enumerate(sorted({ChaveDoElf(C) for C in Casos if C.Musl})):
        ArquivoC = Pasta / "musl" / f"{Indice}.c"
        ArquivoC.parent.mkdir(parents=True, exist_ok=True)
        Erro = oraculo_c.GerarC(Forja, Path(Chave[0]), Chave[1], ArquivoC)
        if Erro:
            Erros[Chave] = Erro
        else:
            Itens.append((Chave, ArquivoC, Pasta / "musl" / f"{Indice}.elf"))
    Resultado = oraculo_c.CompilarLote([(str(Chave), C, E, Chave[1] == "dev") for Chave, C, E in Itens])
    return {**Erros, **{Chave: (Elf if Resultado[str(Chave)] is None else Resultado[str(Chave)]) for Chave, _, Elf in Itens}}


def AvaliarCaso(Caso, R: wsl_lote.Resultado, Musl: wsl_lote.Resultado | None) -> dict:
    Registro = {"caso": Caso.Nome, "modo": Caso.Modo, "ok": True, "oraculo": Caso.OrigemOraculo or "esperado", "motivo": None}
    Motivos = []
    Divergencia = PrimeiraDivergencia(Caso.Esperado, R.Stdout)
    if R.Estourou or R.Exit != Caso.Exit:
        Motivos.append(f"exit={R.Exit} sinal={R.Sinal} estourou={R.Estourou} (esperado exit {Caso.Exit})")
    if Divergencia:
        Motivos.append("stdout difere do esperado")
        Registro["divergencia"] = Divergencia
    if Caso.ErroTem and Caso.ErroTem not in R.Stderr.decode("utf-8", "replace"):
        Motivos.append(f"stderr sem {Caso.ErroTem}: {R.Stderr[:120]!r}")
    if Caso.ErroExato is not None and R.Stderr != Caso.ErroExato:
        Motivos.append("stderr difere do x64-windows")
        Registro["divergencia_stderr"] = PrimeiraDivergencia(Caso.ErroExato, R.Stderr)
    if Caso.ErroVazio and R.Stderr:
        Motivos.append(f"stderr nao vazio: {R.Stderr[:120]!r}")
    if Caso.OraculoWin is not None:
        Motivos += AvaliarContraWindows(Caso, R)
    if Caso.PosEsperado is not None and R.Pos != Caso.PosEsperado:
        Motivos.append(f"estado do arquivo: {R.Pos[:80]!r} != {Caso.PosEsperado!r}")
    if Musl is not None:
        Registro["oraculo"] += "+musl"
        Dif = PrimeiraDivergencia(Musl.Stdout, R.Stdout)
        if Dif or Musl.Exit != R.Exit:
            Motivos.append(f"difere do C-musl (exit musl={Musl.Exit}, linux={R.Exit})")
            Registro["divergencia_musl"] = Dif
    Registro["ok"], Registro["motivo"] = not Motivos, "; ".join(Motivos) or None
    return Registro


def AplicarAceitas(Registros: list, Aceitas: list) -> None:
    for Registro in Registros:
        Casou = any(A["caso"] == Registro["caso"] and A.get("modo") in (None, Registro["modo"]) for A in Aceitas)
        if not Registro["ok"] and Casou:
            Registro["ok"], Registro["aceita"] = True, True


def RegistroPulado(Caso, Origem: str, Motivo: str) -> dict:
    return {"caso": Caso.Nome, "modo": Caso.Modo, "ok": True, "pulado": True, "oraculo": Origem, "motivo": Motivo[:160]}


def CodigoDoErro(Texto: str) -> str | None:
    Achado = REGEX_CODIGO.search(Texto or "")
    return Achado.group(1) if Achado else None


def RecusaDeFrontEnd(Forja, Caso, Erro: str, Pasta: Path) -> bool:
    """O diagnostico E0xxx (front-end, antes de qualquer backend) e o MESMO no x64-windows: a fixture e de erro e nao tem o que rodar."""
    Codigo = CodigoDoErro(Erro)
    if not Codigo or not Codigo.startswith("E"):
        return False
    Windows = Construir(Forja, Caso.Fonte, Caso.Modo, "x86_64-windows", Pasta / "win" / "frontend.exe")
    return CodigoDoErro(Windows) == Codigo


def ClassificarBuilds(Forja, Casos: list, Elfs: dict, Falhas: dict, Musl: dict, SemOraculo: dict, Pasta: Path) -> tuple:
    """(registros de pulado/falha de build, casos executaveis). Em caso Pulavel: recusa X0003 do alvo Linux, recusa do oraculo ou o mesmo E0xxx do front-end nos dois alvos = PULADO; o resto e falha."""
    Registros, Executaveis = [], []
    for Caso in Casos:
        Chave = ChaveDoElf(Caso)
        Recusa = Falhas.get(Chave) or SemOraculo.get(Chave)
        Codigos = (CODIGO_PULADO,) if Chave in Falhas else RECUSAS_DO_ORACULO
        if Caso.Pulavel and Recusa and Chave in Falhas and RecusaDeFrontEnd(Forja, Caso, Recusa, Pasta):
            Registros.append(RegistroPulado(Caso, "front-end " + CodigoDoErro(Recusa), Recusa))
        elif Caso.Pulavel and Recusa and any(Codigo in Recusa for Codigo in Codigos):
            Registros.append(RegistroPulado(Caso, "linux X0003" if Chave in Falhas else "oraculo windows", Recusa))
        elif Chave not in Elfs or (Caso.Musl and not isinstance(Musl.get(Chave), Path)) or Chave in SemOraculo:
            Registros.append({"caso": Caso.Nome, "modo": Caso.Modo, "ok": False, "oraculo": "build", "motivo": Recusa or f"oraculo C: {Musl.get(Chave)}"})
        else:
            Executaveis.append(Caso)
    return Registros, Executaveis


def NomeDoLote(Caso) -> str:
    """Os grupos de GRUPOS_SERIAIS (rede, processo, arquivo, vazamento) dividem um lote so; os demais, um lote por grupo (Caso.Grupo; "" = geral)."""
    return "seriais" if Caso.Grupo in GRUPOS_SERIAIS else (Caso.Grupo or "geral")


def RodarLote(Suite: str, Lista: list, Elfs: dict, Musl: dict, Coletor: dict | None = None) -> list:
    """Uma chamada do wsl.exe com os casos da Lista; os registros saem na ordem dos casos. Coletor (--congelar-c): {(caso, modo): (stdout, exit) do C-musl} dos casos que deram ok."""
    Programas = []
    for Caso in Lista:
        Programas.append(ProgramaDoCaso(Caso, Elfs[ChaveDoElf(Caso)]))
        if Caso.Musl:
            Programas.append(ProgramaDoCaso(Caso, Musl[ChaveDoElf(Caso)], NOME_PROGRAMA_MUSL))
    Antes = time.perf_counter()
    Resultados = iter(wsl_lote.ExecutarLote(Programas, "linux_" + Suite))
    SEGUNDOS_DOS_LOTES[Suite] = time.perf_counter() - Antes
    Registros = []
    for Caso in Lista:
        Linux = next(Resultados)
        MuslRes = next(Resultados) if Caso.Musl else None
        Registro = AvaliarCaso(Caso, Linux, MuslRes)
        if Coletor is not None and MuslRes is not None and Registro["ok"]:
            Coletor[(Caso.Nome, Caso.Modo)] = (MuslRes.Stdout, MuslRes.Exit)
        Registros.append(Registro)
    return Registros


def ExecutarPorGrupo(Executaveis: list, Elfs: dict, Musl: dict, Simultaneos: int = LOTES_PARALELOS, Coletor: dict | None = None) -> list:
    """Os lotes (NomeDoLote) rodam em paralelo (Simultaneos wsl.exe; cada um isolado em /tmp/forja_lote_$$); o serial comeca primeiro e depois os maiores. Devolve na ordem dos lotes."""
    Agrupados = {}
    for Caso in Executaveis:
        Agrupados.setdefault(NomeDoLote(Caso), []).append(Caso)
    Lotes = {}
    for Suite, Lista in Agrupados.items():  # o grupo grande (lowering) se parte em fatias para o paralelo equilibrar; o serial nunca
        Tamanho = len(Lista) if Suite == "seriais" else CASOS_POR_LOTE
        for Inicio in range(0, len(Lista), Tamanho):
            Lotes[Suite if Tamanho >= len(Lista) else f"{Suite}_{Inicio // Tamanho}"] = Lista[Inicio:Inicio + Tamanho]
    Ordem = sorted(Lotes, key=lambda Suite: (Suite != "seriais", -len(Lotes[Suite])))
    with ThreadPoolExecutor(max_workers=max(1, Simultaneos)) as Piscina:
        Feitos = dict(zip(Ordem, Piscina.map(lambda Suite: RodarLote(Suite, Lotes[Suite], Elfs, Musl, Coletor), Ordem)))
    return [Registro for Suite in Lotes for Registro in Feitos[Suite]]


def ImpressoesDaRodada(Casos: list, Elfs: dict, Musl: dict, Forja: Path) -> tuple:
    """(sha do ELF por chave, {(caso, modo): (sha do ELF, sha das entradas)}); ANTES de o oraculo preencher o esperado dos casos. O sha do ELF nao leva a versao do Forja (P3)."""
    ShaElfs = {Chave: cache_linux.ShaElf(Elf, Forja) for Chave, Elf in Elfs.items()}
    ShasMusl = {Chave: cache_linux.ShaArquivo(Item) for Chave, Item in Musl.items() if isinstance(Item, Path)}
    return ShaElfs, cache_linux.Impressoes(Casos, ShaElfs, ShasMusl, ChaveDoElf)


def SepararReaproveitados(Casos: list, Impressao: dict, Mani) -> tuple:
    """(registros verdes reaproveitados, casos pendentes, casos reaproveitados): ELF e entradas iguais ao manifesto, fora os que dependem do ambiente. Mani None = nada reaproveitado."""
    Registros, Pendentes, Reaproveitados = [], [], []
    for Caso in Casos:
        Verde = None
        if Mani is not None and not cache_linux.SempreRoda(Caso) and (Caso.Nome, Caso.Modo) in Impressao:
            Verde = Mani.Reaproveitar(Caso.Nome, Caso.Modo, Impressao[(Caso.Nome, Caso.Modo)])
        if Verde is None:
            Pendentes.append(Caso)
        else:
            Registros.append(Verde)
            Reaproveitados.append(Caso)
    return Registros, Pendentes, Reaproveitados


def DeterminismoComCache(Forja, Itens: list, Contexto: dict) -> list:
    """Registros de determinismo: reaproveita (ELF igual ao manifesto) e roda o resto. Contexto: elfs, pasta, jobs, fatia, mani (None = sem cache) e shas (sha do ELF por chave)."""
    Reusados, ParaRodar = [], []
    for Item in Itens:
        Verde = Contexto["mani"].DeterminismoVerde(Item[1], Item[0][1], Contexto["shas"][Item[0]], Contexto["fatia"]) if Contexto["mani"] is not None else None
        if Verde:
            Reusados += Verde
        else:
            ParaRodar.append(Item)
    return Reusados + ConferirDeterminismo(Forja, ParaRodar, Contexto["elfs"], Contexto["pasta"], Contexto["jobs"])


def AplicarOraculoC(Casos: list) -> tuple:
    """Padrao desde 0.21.0 F3: o esperado dos casos Musl (lista oraculo_c.txt) vira o C-musl congelado (sem gcc, sem forja_g1) e o caso deixa de rodar o C. Sem chave ou banco adulterado = FALHA do caso. Os demais casos seguem como estao.
    Devolve (casos a rodar, registros de falha)."""
    Banco = oraculo_c.BancoLinuxC()
    Executaveis, Ausentes = [], []
    for Caso_ in Casos:
        if not Caso_.Musl:
            Executaveis.append(Caso_)
            continue
        try:
            Caso_.Esperado, Caso_.Exit = oraculo_c.ReferenciaCongelada(Banco, Caso_.Fonte, Caso_.Modo, casos_linux.IdentidadeDoCaso(Caso_))
        except oraculo_c.OraculoCIndisponivel as Erro:
            Ausentes.append({"caso": Caso_.Nome, "modo": Caso_.Modo, "ok": False, "oraculo": oraculo_c.ORIGEM_CONGELADO, "motivo": str(Erro)[:200]})
            continue
        Caso_.Musl, Caso_.OrigemOraculo = False, oraculo_c.ORIGEM_CONGELADO
        Executaveis.append(Caso_)
    return Executaveis, Ausentes


def CongelarOraculoC(Casos: list, Coletor: dict, Registros: list) -> None:
    """--congelar-c: grava so os casos em que o C-musl (forja_g1 + gcc) deu a mesma saida do x64 e o esperado do caso bateu; o resto fica de fora e sai na lista."""
    Itens = {}
    for Caso_ in Casos:
        Item = Coletor.get((Caso_.Nome, Caso_.Modo))
        if Item is not None:
            Itens[oraculo_c.ChaveDoCaso(Caso_.Fonte, Caso_.Modo, casos_linux.IdentidadeDoCaso(Caso_))] = Item
    Motivos = {(R["caso"], R["modo"]): R["motivo"] for R in Registros if not R["ok"]}
    for Caso_ in Casos:
        if (Caso_.Nome, Caso_.Modo) not in Coletor:
            print(f"congelar-c fora: {Caso_.Nome} {Caso_.Modo}: {Motivos.get((Caso_.Nome, Caso_.Modo), 'sem C-musl no caso')}")
    Gravados = oraculo_c.GravarCongelado(Itens)
    print(f"congelar-c: {Gravados} itens gravados em {oraculo_c.CAMINHO_CONGELADO.name}; {len(Casos) - Gravados} de {len(Casos)} casos ficaram de fora")


def Rodar(Argumentos) -> list:
    Forja = Path(Argumentos.forja).resolve()
    Pasta = Path(tempfile.gettempdir()) / "forja_linux" / "verificar"
    shutil.rmtree(Pasta, ignore_errors=True)
    if Argumentos.oraculo_c_so:
        Casos = casos_linux.CasosDoOraculoC()
    else:
        Casos = conjunto_linux.TodoOConjunto() if Argumentos.conjunto else casos_linux.CasosPasta(Path(Argumentos.pasta)) if Argumentos.pasta else casos_linux.TodosOsCasos()
    Modos = Argumentos.modos.split(",")
    Casos = [C for C in Casos if C.Modo in Modos and (not Argumentos.so or Argumentos.so in C.Nome)]
    Ausentes = []
    if not Argumentos.congelar_c:  # 0.21.0 F3: o padrao; so --congelar-c gera o C-musl (forja_g1 + gcc)
        Casos, Ausentes = AplicarOraculoC(Casos)
    Aceitas, Falhas, Cron, Estado = CarregarAceitas(), {}, Argumentos.cron, Argumentos.estado
    Coletor = {} if Argumentos.congelar_c else None
    with Cron.Fase("build"):
        Elfs = ConstruirTodos(Forja, Casos, Pasta, Falhas, Argumentos.jobs)
    with Cron.Fase("musl/classificacao"):
        Musl = CompilarMusl(oraculo_c.FORJA_G1 if Argumentos.congelar_c else Forja, Casos, Pasta)  # sem casos Musl (contra) nao chama gcc nem forja_g1
    with Cron.Fase("cache"):
        ShaElfs, Impressao = ImpressoesDaRodada(Casos, Elfs, Musl, Forja)
        Mani = cache_linux.Manifesto(Argumentos.manifesto, Estado["ambiente"])
        Estado["impressao"], Estado["alarmes"] = Impressao, cache_linux.AlarmesR2(Mani, Impressao)
        Estado["manifesto"] = (Mani.Existe, Mani.AmbienteIgual)
        Mani = Mani if Argumentos.cache and not Argumentos.parcial else None
        Registros, Pendentes, Reaproveitados = SepararReaproveitados(Casos, Impressao, Mani)
    with Cron.Fase("oraculo"):
        Visiveis = [C for C in Pendentes if ChaveDoElf(C) in Elfs]
        SemOraculo = PreencherOraculoWindows(Forja, Visiveis, Pasta, Argumentos.jobs, Congelado=Argumentos.congelado, Coletor=Argumentos.coletor)
        PreencherOraculoTeste(Forja, Visiveis, Argumentos.jobs)
    with Cron.Fase("musl/classificacao"):
        Novos, Executaveis = ClassificarBuilds(Forja, Pendentes, Elfs, Falhas, Musl, SemOraculo, Pasta)
        Registros += Novos
    with Cron.Fase("lotes WSL"):
        Registros += ExecutarPorGrupo(Executaveis, Elfs, Musl, Coletor=Coletor) + Ausentes
    with Cron.Fase("determinismo"):
        Itens = SelecionarDeterminismo(Executaveis + Reaproveitados, Elfs, Argumentos.conjunto, Argumentos.perfil == PERFIL_FATIA)
        Estado["shas_det"] = {(Nome, Chave[1]): ShaElfs[Chave] for Chave, Nome, _ in Itens}
        Contexto = {"elfs": Elfs, "pasta": Pasta, "jobs": Argumentos.jobs, "fatia": Argumentos.perfil == PERFIL_FATIA, "mani": Mani, "shas": ShaElfs}
        Registros += DeterminismoComCache(Forja, Itens, Contexto)
    with Cron.Fase("estrutura"):
        Registros += ConferirEstrutura(Elfs, Executaveis + Reaproveitados)
    AplicarAceitas(Registros, Aceitas)
    if Argumentos.congelar_c:
        CongelarOraculoC(Casos, Coletor, Registros)
    return Registros


def SelftestDeterminismoELotes() -> list:
    """Perfil fatia (so release + es/handle; debug so no es/handle e na amostra), completo (tudo) e a separacao dos lotes seriais."""
    Falhas = []
    Fonte, Handle = Path("x/a.fj"), Path("x/es/handle.fj")
    Casos = [casos_linux.Caso(f"g/{Fonte.stem if Arquivo == Fonte else 'handle'}", Arquivo, Modo, Grupo="g") for Arquivo in (Fonte, Handle) for Modo in ("dev", "release")]
    Elfs = {ChaveDoElf(C): Path("e.elf") for C in Casos}
    Completo = SelecionarDeterminismo(Casos, Elfs, True, False)
    Fatia = SelecionarDeterminismo(Casos, Elfs, True, True)
    if len(Completo) != 4 or not all(Item[2] for Item in Completo):
        Falhas.append("determinismo completo: 4 chaves, todas com debug")
    if [(Item[0][0].endswith("handle.fj"), Item[0][1], Item[2]) for Item in Fatia if Item[0][1] == "dev"] != [(True, "dev", True)] or len(Fatia) != 3:
        Falhas.append(f"determinismo fatia: so release e es/handle (3 chaves): {Fatia}")
    Lotes = {NomeDoLote(casos_linux.Caso("a", Fonte, "dev", Grupo=G)) for G in GRUPOS_SERIAIS}
    if Lotes != {"seriais"} or NomeDoLote(casos_linux.Caso("a", Fonte, "dev", Grupo="dourado")) != "dourado" or NomeDoLote(casos_linux.Caso("a", Fonte, "dev")) != "geral":
        Falhas.append("lotes seriais")
    return Falhas


def Selftest() -> int:
    Falhas = []
    if PrimeiraDivergencia(b"abc", b"abc") is not None:
        Falhas.append("iguais")
    if PrimeiraDivergencia(b"abc", b"abd")["indice"] != 2 or PrimeiraDivergencia(b"ab", b"abc")["esperado"] != "fim":
        Falhas.append("indice da divergencia")
    if not isinstance(CarregarAceitas(), list) or not casos_linux.ListaMusl():
        Falhas.append("aceitas/oraculo_c.txt")
    if casos_linux.Selftest() != 0:
        Falhas.append("casos_linux")
    if cache_linux.Selftest() != 0:
        Falhas.append("cache_linux")
    Falhas += SelftestDeterminismoELotes()
    print("verificar_linux selftest:", "VERDE" if not Falhas else f"FALHOU {Falhas}")
    return 1 if Falhas else 0


def GravarRelatorio(Arquivo: str, Registros: list, Pulados: list) -> None:
    """Relatorio do P29a: o conjunto (casos e fontes por grupo), os pulados do build (lista) e os fora-por-desenho do conjunto_linux."""
    Fora = conjunto_linux.ForaPorDesenho()
    Texto = {"registros": len(Registros), "pulados_x0003": [{"caso": R["caso"], "modo": R["modo"], "origem": R["oraculo"]} for R in Pulados],
             "fora_por_desenho": [{"grupo": G, "fonte": str(F), "motivo": M} for G, F, M in Fora],
             "fontes_por_grupo": conjunto_linux.Contagens(conjunto_linux.TodoOConjunto())}
    Path(Arquivo).write_text(json.dumps(Texto, ensure_ascii=False, indent=1), encoding="utf-8")


def PrepararArgumentos(Argumentos, Analisador) -> None:
    """Deriva do que veio na linha: conjunto, cronometro, parcial (sem manifesto gravado), cache, manifesto, congelado/coletor e o estado da rodada."""
    if Argumentos.congelar_c and Argumentos.contra_congelado_c:
        Analisador.error("--congelar-c e --contra-congelado-c nao andam juntos")
    Argumentos.oraculo_c_so = Argumentos.congelar_c  # 0.21.0 F3: --contra-congelado-c e no-op (o padrao ja compara os 13 com o congelado)
    if Argumentos.oraculo_c_so and (Argumentos.pasta or Argumentos.so):
        Analisador.error("--congelar-c roda so os 13 casos de oraculo_c.txt: nao combina com --pasta nem --so")
    Argumentos.conjunto = not Argumentos.pasta and not Argumentos.oraculo_c_so  # sem --pasta: a suite inteira (P29a); P3: os 13 casos nao tem a suite
    Argumentos.cron = Cronometro()
    Argumentos.parcial = bool(Argumentos.so or Argumentos.pasta or Argumentos.modos != MODOS_PADRAO or Argumentos.oraculo_c_so)  # parcial nunca grava o manifesto
    Argumentos.cache = (Argumentos.cache or Argumentos.perfil == PERFIL_FATIA) and not Argumentos.sem_cache
    Argumentos.manifesto = Path(Argumentos.manifesto) if Argumentos.manifesto else cache_linux.MANIFESTO_PADRAO
    if Argumentos.oraculo_congelado and Argumentos.congelar_oraculo:
        Analisador.error("--oraculo-congelado e --congelar-oraculo nao andam juntos")
    Arquivo = Argumentos.oraculo_congelado or (str(cache_linux.CONGELADO_PADRAO) if Argumentos.perfil == PERFIL_FATIA and cache_linux.CONGELADO_PADRAO.is_file() else None)
    if Arquivo and not Path(Arquivo).is_file():
        Analisador.error(f"oraculo congelado inexistente: {Arquivo}")
    Argumentos.congelado = cache_linux.CarregarCongelado(Path(Arquivo)) if Arquivo and not Argumentos.congelar_oraculo else None
    Argumentos.coletor = {} if Argumentos.congelar_oraculo else None
    Argumentos.estado = {"ambiente": cache_linux.ShaDoAmbiente(LANGUAGE)}


def ResumirCacheEOraculo(Argumentos, Casos: list, Todos: list) -> None:
    """Linhas do resumo sobre o manifesto (reaproveitados, alarme R2) e o oraculo congelado (independentes x mesma rodada)."""
    Estado = Argumentos.estado
    if Argumentos.cache:
        Existe, Igual = Estado["manifesto"]
        Situacao = "valido" if Igual else ("ignorado: o ambiente mudou, rodada COMPLETA obrigatoria" if Existe else "inexistente (primeira rodada)")
        print(f"cache: {sum(1 for R in Casos if R.get('cache') is True)} casos e {sum(1 for R in Todos if isinstance(R.get('cache'), str))} registros de determinismo reaproveitados; manifesto {Situacao}")
    if Estado["alarmes"]:
        print(f"ALARME R2 violada: o ELF dev/teste mudou desde a rodada aprovada em {len(Estado['alarmes'])} casos (ex.: {Estado['alarmes'][:3]}); rodada COMPLETA obrigatoria")
    if Argumentos.congelado is not None:
        Sem = sum(1 for R in Casos if R["oraculo"].startswith(cache_linux.ORIGEM_MESMA_RODADA) and R["modo"] == "release" and R["caso"].split("/")[0] in cache_linux.GRUPOS_CONGELAVEIS)
        print(f"oraculo congelado: {sum(1 for R in Casos if R['oraculo'].startswith(cache_linux.ORIGEM_CONGELADO))} casos com oraculo independente; {Sem} casos release sem oraculo independente (windows-mesma-rodada)")


def GravarProdutos(Argumentos, Registros: list, Verde: bool) -> None:
    """--congelar-oraculo: grava o x64-windows desta rodada; o manifesto so quando a rodada inteira passa e nao e parcial (--so/--pasta/--modos)."""
    if Argumentos.coletor is not None:
        cache_linux.GravarCongelado(Path(Argumentos.congelar_oraculo), Argumentos.coletor, Path(Argumentos.forja).resolve())
        print(f"oraculo congelado gravado: {len(Argumentos.coletor)} itens em {Argumentos.congelar_oraculo}")
    if Verde and not Argumentos.parcial:
        Estado = Argumentos.estado
        Perfil = Argumentos.perfil
        cache_linux.GravarManifesto(Argumentos.manifesto, Estado["ambiente"], cache_linux.EntradasDoManifesto(Registros, Estado["impressao"]),
                                    cache_linux.DeterminismoDoManifesto(Registros, Estado["shas_det"], Perfil))
        print(f"manifesto gravado em {Argumentos.manifesto}")


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Verifica o alvo x86_64-linux no WSL (alpine-build).")
    Analisador.add_argument("--perfil", choices=(PERFIL_COMPLETO, PERFIL_FATIA), default=PERFIL_COMPLETO,
                            help="completo (padrao: determinismo inteiro, sem cache) ou fatia (determinismo reduzido + cache por hash do ELF + oraculo congelado se houver; NAO substitui a rodada completa)")
    Analisador.add_argument("--cache", action="store_true", help="reaproveita o manifesto da ultima rodada aprovada (a fatia ja o liga)")
    Analisador.add_argument("--sem-cache", action="store_true", help="ignora o manifesto (so o alarme R2 o le); a rodada aprovada ainda o regrava")
    Analisador.add_argument("--manifesto", help="arquivo do manifesto (padrao: %%TEMP%%\\forja_linux\\manifesto_verificar.json)")
    Analisador.add_argument("--congelar-oraculo", metavar="ARQ.json", help="grava o resultado do x64-windows desta rodada (por sha da fonte, modo, args/env)")
    Analisador.add_argument("--oraculo-congelado", metavar="ARQ.json", help="usa o x64-windows congelado nos casos release de dourado, exemplos, examples, semantica, m1, m2, m4, v111 e traco")
    Analisador.add_argument("--congelar-c", action="store_true", help="linux_c (P3): grava no JSON selado o C-musl (forja_g1 + gcc) dos 13 casos de oraculo_c.txt, so onde C == x64; so esses casos")
    Analisador.add_argument("--contra-congelado-c", action="store_true", help="no-op desde a 0.21.0 F3: o padrao ja compara os 13 casos de oraculo_c.txt com o JSON selado, sem gcc nem forja_g1")
    Analisador.add_argument("--jobs", type=int, default=0, help="builds em paralelo (0 = min(8, nucleos))")
    Analisador.add_argument("--relatorio", help="grava o relatorio do conjunto (pulados, fora por desenho, contagens) neste .json")
    Analisador.add_argument("--forja", help="forja.exe (de preferencia uma copia)")
    Analisador.add_argument("--pasta", help="pasta de .fj: sem .esperado, o oraculo e o x64-windows")
    Analisador.add_argument("--modos", default="dev,release,teste", help="dev, release e teste (o executor de testes, P29b)")
    Analisador.add_argument("--so", help="so os casos cujo nome contem este trecho")
    Analisador.add_argument("--json", action="store_true", help="imprime todos os casos, nao so as falhas")
    Analisador.add_argument("--tempos", action="store_true", help="imprime o tempo de cada fase no fim")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return Selftest()
    if not Argumentos.forja:
        Analisador.error("--forja e obrigatorio")
    if not wsl_lote.DistroDisponivel():
        print("linux: PULADA (WSL/distro ausente)")
        return 0
    PrepararArgumentos(Argumentos, Analisador)
    if Argumentos.perfil == PERFIL_FATIA:
        print(AVISO_FATIA)
    Registros = Rodar(Argumentos)
    for Registro in Registros:
        if Argumentos.json or not Registro["ok"]:
            print(json.dumps(Registro, ensure_ascii=False))
    Pulados = [R for R in Registros if R.get("pulado")]
    Rodados = [R for R in Registros if not R.get("pulado")]
    EhVerificacao = lambda R: R["caso"].startswith(("determinismo", "estrutura"))  # contam a parte: nao inflam o total de casos
    Verificacoes = [R for R in Rodados if EhVerificacao(R)]
    Casos = [R for R in Rodados if not EhVerificacao(R)]
    if Argumentos.relatorio:
        GravarRelatorio(Argumentos.relatorio, Registros, Pulados)
    BoasCasos, BoasVerificacoes = sum(1 for R in Casos if R["ok"]), sum(1 for R in Verificacoes if R["ok"])
    print(f"verificar_linux determinismo+estrutura: {BoasVerificacoes}/{len(Verificacoes)} registros (fora do total de casos)")
    print(f"verificar_linux: {BoasCasos}/{len(Casos)} casos de linux (pulados por desenho: {len(Pulados)})")
    Verde = BoasCasos == len(Casos) and BoasVerificacoes == len(Verificacoes)
    ResumirCacheEOraculo(Argumentos, Casos, Rodados)
    GravarProdutos(Argumentos, Registros, Verde)
    if Argumentos.tempos:
        print(Argumentos.cron.Resumo())
    return 0 if Verde else 1


if __name__ == "__main__":
    sys.exit(Principal())
