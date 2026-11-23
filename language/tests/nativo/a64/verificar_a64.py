#!/usr/bin/env python3
"""verificar_a64.py - verifica o alvo aarch64-linux da Forja (forja build --target aarch64-linux) contra o oraculo x86_64-linux.

Objetivo: cada caso do conjunto (conjunto_a64.py) e cada modo (dev e release) e compilado duas vezes:
(a) ORACULO: forja build --target x86_64-linux [--release] <fonte> -o x64.elf; roda no WSL (alpine-build) num lote (wsl_lote.py).
(b) CANDIDATO: forja build --target aarch64-linux [--release] <fonte> -o a64.elf; roda no qemu-user do WSL (qemu-aarch64) e,
    com --emulador, tambem no emulador Android (android_emulador.py executar). O harness NAO inicia o emulador.
Com --minimo nao ha oraculo: a referencia e o .esperado ao lado de cada fixture (JSON: {"exit": N, "stdout": "texto"}).
Compara: stdout byte a byte, exit e, contra o oraculo, stderr normalizado por NormalizarLinhasR (sem o campo pilha).
Recusa do alvo (X0001/X0003): no candidato = PULADO por desenho; no oraculo = PULADO (sem oraculo). Outra falha de build = falha de build.
Divergencia listada em divergencias_aceitas_a64.json ({caso, modo, motivo} ou {nome, motivo}; max 3 classes) vira `aceita`: relatada, nao falha.
Classe nan_bits (verificar_a64_nan.py): so o sinal do NaN diferiu (stdout igual depois da troca dos tokens; exit e stderr iguais) = diverge com classe nan_bits, aceita pela entrada {nome: nan_bits}.
Lote: os candidatos rodam num lote do WSL (wsl_lote.ExecutarLote, qemu-aarch64 por um sh de cada obra); o oraculo x86_64 e o
candidato tem cache por sha256 em %TEMP%\\.verificar_a64_cache (--sem-cache desliga).

Uso:   python -I verificar_a64.py [--forja <forja.exe>] [--candidato <forja.exe>] [--modos dev,release] [--filtro texto] [--lista]
                                  [--minimo] [--conjunto padrao|linux] [--sem-cache] [--emulador] [--jobs N] [--timeout-seg N] [--selftest]
                                  [--json <arq>] [--fechar]
Saida: uma linha por tarefa com o status (ok, aceita, diverge, falha de build, falha de run, PULADO) e o motivo; no fim
       `verificar_a64: ok/total` (PULADO fica fora do total) e, com PULADO, as 25 causas de maior contagem. Exit 0 so com tudo ok; 1 sem tarefas avaliadas; 2 se --emulador e o emulador estiver fora.
       --json <arq>: registros (caso, modo, status, causa, ms) e resumo por causa. --fechar: criterio de parada da A3 (exit 0 so com diverge, falha de build e falha de run em 0, aceitas nomeadas e PULADO so nas classes aceitas: front-end igual, X0003, X0001 listado em pulados_aceitos_a64.json).
Escrita: pasta temporaria (tempfile), apagada ao fim; a pasta do lote e de cada processo (%TEMP%\\forja_linux\\a64_<pid>, apagada ao fim); cache em %TEMP%\\.verificar_a64_cache.
Verificacao: python -I verificar_a64.py --selftest  (provas em verificar_a64_selftest.py: causa do diagnostico, recusa so no candidato, PosEsperado, classe nan_bits; e com WSL, 6 ELFs plantados no qemu-user em um lote: ok, saida errada, exit errado, timeout, PosEsperado ok e errado)
Versao: 1.3.3
Changelog: 1.3.3 (22/11/2026) A4.5 - classe aceita nan_bits (verificar_a64_nan.py; entrada {nome, motivo}, max 3 classes); PULADO aceito no resumo (`PULADO N (aceitos M)`, pulados_aceitos_a64.json); selftest em verificar_a64_selftest.py (teto de 800 linhas);
           1.3.2 (22/11/2026) A4.4 - divergencia aceita pode ter "env" (so vale com o mesmo --env); o --env entra na chave do cache;
           1.3.1 (22/11/2026) A4.4 - --env CHAVE=VALOR (repetivel): so no ambiente do candidato no qemu (forca FORJA_A64_JANELA_ALTA=1);
           1.3.0 (22/11/2026) A3.2a - pasta do lote por processo (duas instancias nao se apagam); --help corrigido (%%); causa do X-code (campo "causa" do diagnostico) agrupa o PULADO (top 25); E-code so no candidato = falha de build; PosEsperado conferido; --json; --fechar; selftest com provas puras.
           1.2.0 (22/11/2026) A3.1 - candidatos em lote (wsl_lote, sem um wsl.exe por caso); --conjunto linux (casos_linux via conjunto_linux.TodoOConjunto: args, env, entrada, pasta, usuario);
           PULADO agrupado por causa; cache por sha256 do oraculo x86_64 (chave: forja + fonte + modo + campos do caso) e do ELF a64; divergencias_aceitas_a64.json; selftest com timeout em lote.
           1.1.0 (22/11/2026) A1.3(iv) - --minimo aceita `<caso>_release.esperado` no modo release (o release faz wrap onde o dev confere estouro).
           1.0.0 (22/11/2026) A2.1 - primeira versao: oraculo x86_64-linux (WSL) x candidato aarch64-linux (qemu-user; --emulador),
           --minimo com .esperado, --lista, --selftest com ELF plantado. Fora desta versao: --congelar-oraculo, testes (A3), bytes_x64.
"""
import argparse
import base64
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
sys.path.insert(0, str(AQUI.parents[1]))  # tests\: rcode_normalizar
sys.path.insert(0, str(AQUI.parents[1] / "nativo" / "linux"))  # wsl_lote
sys.path.insert(0, str(AQUI))  # conjunto_a64
from rcode_normalizar import NormalizarLinhasR  # noqa: E402
import conjunto_a64  # noqa: E402
import wsl_lote  # noqa: E402
from verificar_a64_nan import SaidaSoNan  # noqa: E402

FORJA_PADRAO = LANGUAGE / "compiler" / "forja.exe"
EMULADOR = Path("C:/CLAUDE_AGI/tools/android_emulador/android_emulador.py")
ALVO_ORACULO = "x86_64-linux"
ALVO_CANDIDATO = "aarch64-linux"
MODOS_VALIDOS = ("dev", "release")
STATUS_VALIDOS = ("ok", "aceita", "diverge", "falha de build", "falha de run", "PULADO")
CODIGOS_PULADO = ("X0001", "X0003")  # recusa do alvo por desenho (contrato A0, secao 8)
REGEX_CODIGO = re.compile(r"\b(X\d{4})\b")
REGEX_FRENTE = re.compile(r"\b(E\d{4})\b")  # erro de front-end: so decide no conjunto linux (o candidato tem de recusar igual); X vem antes
TEMPO_PADRAO = 120
TEMPO_STATUS = 60  # status/adb do emulador: uma chamada curta
TEMPO_SELFTEST = 5
CODIGO_EMULADOR_FORA = 2
TRANCA_EMULADOR = threading.Lock()  # uma chamada do emulador de cada vez
CONJUNTO_LINUX = "linux"
ARQUIVO_ACEITAS = AQUI / "divergencias_aceitas_a64.json"
MAXIMO_ACEITAS = 3  # contrato A2.2(e): divergencias aceitas sao nomeadas e poucas
PASTA_CACHE = Path(tempfile.gettempdir()) / ".verificar_a64_cache"  # oraculo x86_64 e candidato por sha256; so o que terminou entra
REGEX_SIMBOLO = re.compile(r"forja_\w+")  # o nome do runtime muda entre casos: agrupa a causa sem ele
REGEX_CAUSA = re.compile(r'"causa": "((?:[^"\\]|\\.)*)"')  # campo causa do JSON de diagnostico do forja (1a ocorrencia; o texto da saida inteira, nao o resumo)
CAUSA_TAMANHO = 80  # causa sem runtime 'forja_X' agrupa pelos 80 primeiros caracteres
TOP_CAUSAS = 25  # o relatorio imprime so as causas de maior contagem
FRENTE_IGUAL = "front-end igual nos dois alvos"  # PULADO do E-code igual no oraculo e no candidato
ARQUIVO_PULADOS = AQUI / "pulados_aceitos_a64.json"  # causas X0001 aceitas pelo --fechar (lista de texto; vazia = nenhuma)
ARQUIVO_TEMPO = AQUI / "tempo_conhecido_a64.json"  # tempo_conhecido: limite de tempo maior por caso (desempenho conhecido do codigo gerado sem regalloc no qemu)
ARQUIVO_TRIAGEM = AQUI / "triagem_a3.json"  # ultima triagem gravada: o --fechar diz se o PULADO subiu em relacao a ela


@dataclass
class Tarefa:
    Caso: conjunto_a64.Caso
    Modo: str


@dataclass
class Saida:
    Stdout: bytes
    Stderr: bytes
    Exit: int | None
    Motivo: str = ""  # nao vazio = o programa nao chegou a terminar (timeout, infraestrutura)
    Pos: bytes = b""  # stdout do Pos do caso (estado do arquivo depois do programa); vazio sem Pos
    Ms: int = 0  # tempo do run no lote (0 vindo do cache ou sem run)


@dataclass
class Falha:
    """Build que nao gerou o arquivo: Erro = texto do forja (resumo); Codigo = X/E-code achado; Causa = chave que agrupa (ex.: 'X0001 forja_panic')."""
    Erro: str
    Codigo: str | None
    Causa: str = ""


@dataclass
class Esperado:
    Exit: int
    Stdout: bytes


@dataclass
class Opcoes:
    Forja: Path
    Candidato: Path
    Pasta: Path
    Tempo: int
    Minimo: bool
    Emulador: bool
    Linux: bool = False  # --conjunto linux: falha de build do candidato vira PULADO por causa (o a64 ainda nao suporta o caso)
    Cache: bool = True  # LerCache/GravarCache em PASTA_CACHE (--sem-cache e o selftest desligam)
    ShaForja: str = ""  # sha256 do forja do oraculo (calculado 1x por rodada; entra na chave do oraculo)


@dataclass
class Obra:
    Tarefa_: Tarefa
    Indice: int
    Oraculo: Path | None = None
    Elf: Path | None = None
    SaidaOraculo: Saida | None = None
    Registro: dict | None = None
    ChaveOraculo: str | None = None
    ChaveQemu: str | None = None
    SaidaQemu: Saida | None = None
    CodigoOraculo: str | None = None  # E-code de front-end que o oraculo recusou (conjunto linux): o candidato tem de recusar igual
    MsBuild: int = 0  # tempo dos builds (oraculo + candidato) desta obra, em ms: entra no ms do --json


def ListarTarefas(Casos: list, Modos: list) -> list:
    """Uma tarefa por (caso, modo), na ordem dos casos e dentro de cada um na ordem dos modos."""
    return [Tarefa(Caso_, Modo) for Caso_ in Casos for Modo in Modos]


def TarefasDoLinux(Casos: list, Modos: list) -> list:
    """O conjunto linux traz o modo em cada caso (casos_linux.Caso.Modo): uma tarefa por caso, no modo dele."""
    return [Tarefa(Caso_, Caso_.Modo) for Caso_ in Casos if Caso_.Modo in Modos]


def ShaDoArquivo(Arquivo: Path) -> str:
    return hashlib.sha256(Arquivo.read_bytes()).hexdigest() if Arquivo.is_file() else ""


def SerializarCampo(Valor):
    """default do json.dumps: bytes (dados, entrada) viram hex; o resto vira texto."""
    return Valor.hex() if isinstance(Valor, bytes) else str(Valor)


def ChaveDaEntrada(Tarefa_: Tarefa, Fixo: str) -> str:
    """sha256 do que muda o resultado do caso: fonte, modo, args, env, pre, pos, usuario, dados e entrada; Fixo = sha do forja (oraculo) ou do ELF (candidato)."""
    Itens = {"fonte": ShaDoArquivo(Tarefa_.Caso.Fonte), "modo": Tarefa_.Modo, "fixo": Fixo, "campos": CamposDoCaso(Tarefa_.Caso), "ambiente": ENV_EXTRA}
    return hashlib.sha256(json.dumps(Itens, sort_keys=True, default=SerializarCampo).encode("utf-8")).hexdigest()


def LerCache(Chave: str) -> Saida | None:
    Arquivo = PASTA_CACHE / f"{Chave}.json"
    if not Arquivo.is_file():
        return None
    Dados = json.loads(Arquivo.read_text(encoding="utf-8"))
    if "pos" not in Dados:  # entrada da 1.2.0 nao guardou o Pos: refaz o run em vez de conferir PosEsperado contra vazio
        return None
    return Saida(base64.b64decode(Dados["stdout"]), base64.b64decode(Dados["stderr"]), Dados["exit"], Pos=base64.b64decode(Dados["pos"]))


def GravarCache(Chave: str, Saida_: Saida) -> None:
    """So o que terminou entra (timeout e falha de infraestrutura nao): a proxima rodada tenta de novo."""
    PASTA_CACHE.mkdir(parents=True, exist_ok=True)
    Dados = {"stdout": base64.b64encode(Saida_.Stdout).decode("ascii"), "stderr": base64.b64encode(Saida_.Stderr).decode("ascii"), "exit": Saida_.Exit,
             "pos": base64.b64encode(Saida_.Pos).decode("ascii")}
    (PASTA_CACHE / f"{Chave}.json").write_text(json.dumps(Dados), encoding="utf-8")


def CamposDoCaso(Caso_) -> dict:
    """Campos do programa no lote (nomes = campos de wsl_lote.Programa); o conjunto a64 nao os tem e recebe o padrao do Programa."""
    return {
        "Args": getattr(Caso_, "Args", []),
        "Pre": getattr(Caso_, "Pre", ""),
        "Env": getattr(Caso_, "Env", {}),
        "Usuario": getattr(Caso_, "Usuario", None),
        "Dados": getattr(Caso_, "Dados", {}),
        "Pos": getattr(Caso_, "Pos", ""),
        "Entrada": getattr(Caso_, "Entrada", None),
    }


def ProgramaDoCaso(Caso_, Nome: str, Elf: Path, Limite: int) -> wsl_lote.Programa:
    """Programa do lote: o conjunto linux leva args, env, entrada, pasta e usuario do caso; o --minimo so o ELF."""
    return wsl_lote.Programa(Nome, Elf, Limite=Limite, **CamposDoCaso(Caso_))


def PrecisaOraculo(Tarefa_: Tarefa, Opc: Opcoes) -> bool:
    """O oraculo x86_64 roda fora do --minimo e quando o caso nao traz esperado proprio (esperado_so, lowering e receitas trazem)."""
    return not Opc.Minimo and getattr(Tarefa_.Caso, "Esperado", None) is None


def SaidaDoResultado(Resultado, Tempo: int) -> Saida:
    Motivo = f"passou de {Tempo} s" if Resultado.Estourou else ""
    return Saida(Resultado.Stdout, Resultado.Stderr, Resultado.Exit, Motivo, Resultado.Pos, Resultado.Ms)


def RodarLoteWsl(Programas: list) -> tuple:
    """(resultados, erro): uma chamada do wsl.exe para o lote; erro = motivo de o lote nao ter rodado (None = rodou).
    A pasta do lote e deste processo (wsl_lote.Preparar apaga a pasta do nome dado): duas instancias nao se apagam."""
    Suite = f"a64_{os.getpid()}"
    try:
        return wsl_lote.ExecutarLote(Programas, Suite), None
    except RuntimeError as Erro:
        return [], str(Erro)
    finally:
        shutil.rmtree(Path(tempfile.gettempdir()) / "forja_linux" / Suite, ignore_errors=True)


def MontarRegistro(Tarefa_: Tarefa, Status: str, Motivo: str, Causa: str = "") -> dict:
    """Causa = chave do agrupamento do PULADO e do --json (vazia = nao se aplica); classe = classe aceita do diverge (nan_bits; vazia = nenhuma); ms entra depois, em AvaliarObra."""
    return {"tarefa": Tarefa_.Caso.Nome, "modo": Tarefa_.Modo, "status": Status, "motivo": Motivo, "causa": Causa, "classe": "", "ms": 0}


def ExecutarProcesso(Comando: list, Pasta: Path, Tempo: int) -> Saida:
    """Roda um processo com limite de tempo; o erro (nao abriu, passou do limite) vai em Motivo e Exit fica None."""
    try:
        Processo = subprocess.run(Comando, capture_output=True, timeout=Tempo, stdin=subprocess.DEVNULL, cwd=Pasta)
    except subprocess.TimeoutExpired:
        return Saida(b"", b"", None, f"passou de {Tempo} s")
    except OSError as Erro:
        return Saida(b"", b"", None, f"nao abriu: {Erro}")
    return Saida(Processo.stdout, Processo.stderr, Processo.returncode)


def ComandoForja(Forja: Path, Alvo: str, Modo: str, Fonte: Path, Arquivo: Path) -> list:
    Release = ["--release"] if Modo == "release" else []
    return [str(Forja), "build", "--target", Alvo, *Release, str(Fonte), "-o", str(Arquivo)]


def CausaDoDiagnostico(Texto: str, Codigo: str | None) -> str:
    """Chave que agrupa a falha: X-code com runtime 'forja_X' na causa vira 'X0001 forja_X'; outra causa, 'X0001 <80 primeiros>'; sem causa, so o codigo."""
    Achado = REGEX_CAUSA.search(Texto)
    if not Codigo or not Achado:
        return Codigo or ""
    Simbolo = REGEX_SIMBOLO.search(Achado.group(1))
    return f"{Codigo} {Simbolo.group(0)}" if Simbolo else f"{Codigo} {Achado.group(1)[:CAUSA_TAMANHO]}"


def Compilar(Comando: list, Arquivo: Path, Tempo: int) -> Falha | None:
    """None = o build saiu 0 e gerou o arquivo; senao a Falha (X/E-code achado na saida do forja e a causa, lida do texto inteiro)."""
    Processo = ExecutarProcesso(Comando, LANGUAGE, Tempo)
    Texto = (Processo.Stdout + Processo.Stderr).decode("utf-8", "replace")
    Achado = REGEX_CODIGO.search(Texto) or REGEX_FRENTE.search(Texto)
    Codigo = Achado.group(1) if Achado else None
    Causa = CausaDoDiagnostico(Texto, Codigo)
    if Processo.Motivo:
        return Falha(f"build {Processo.Motivo}", Codigo, Causa)
    if Processo.Exit != 0 or not Arquivo.is_file():
        Resumo = Texto.replace("\n", " | ")[:400]
        return Falha(f"build exit {Processo.Exit}: {Resumo}", Codigo, Causa)
    return None


def CompilarMedido(Comando: list, Arquivo: Path, Tempo: int, Obra_: Obra) -> Falha | None:
    """Compila e soma o tempo do build na obra (o ms do --json)."""
    Inicio = time.monotonic()
    Falha_ = Compilar(Comando, Arquivo, Tempo)
    Obra_.MsBuild += round((time.monotonic() - Inicio) * 1000)
    return Falha_


def RegistroDeBuild(Tarefa_: Tarefa, Papel: str, Falha_: Falha) -> dict:
    """X0001/X0003 (recusa do alvo por desenho) vira PULADO com a causa; qualquer outro erro reprova como falha de build, E-code so do candidato inclusive."""
    if Falha_.Codigo not in CODIGOS_PULADO:
        return MontarRegistro(Tarefa_, "falha de build", f"{Papel}: {Falha_.Erro}", Falha_.Causa or "sem X/E-code")
    Motivo = "sem oraculo" if Papel == "oraculo" else "recusa do candidato por desenho"
    return MontarRegistro(Tarefa_, "PULADO", f"{Motivo}: {Falha_.Causa}", Falha_.Causa)


def ConstruirObra(Tarefa_: Tarefa, Indice: int, Opc: Opcoes) -> Obra:
    """Oraculo (quando PrecisaOraculo): do cache ou compilado; depois o candidato. Falha do oraculo para a obra antes do candidato."""
    Obra_ = Obra(Tarefa_, Indice)
    Pasta = Opc.Pasta / f"t{Indice:04d}"
    Pasta.mkdir(parents=True, exist_ok=True)
    if PrecisaOraculo(Tarefa_, Opc):
        Obra_.ChaveOraculo = ChaveDaEntrada(Tarefa_, Opc.ShaForja)
        Obra_.SaidaOraculo = LerCache(Obra_.ChaveOraculo) if Opc.Cache else None
        if Obra_.SaidaOraculo is None:
            Obra_.Oraculo = Pasta / "x64.elf"
            Comando = ComandoForja(Opc.Forja, ALVO_ORACULO, Tarefa_.Modo, Tarefa_.Caso.Fonte, Obra_.Oraculo)
            Falha_ = CompilarMedido(Comando, Obra_.Oraculo, Opc.Tempo, Obra_)
            if Falha_ and not ErroDeFrente(Falha_.Codigo, Opc):
                Obra_.Registro = RegistroDeBuild(Tarefa_, "oraculo", Falha_)
                return Obra_
            Obra_.CodigoOraculo = Falha_.Codigo if Falha_ else None
    Obra_.Elf = Pasta / "a64.elf"
    Comando = ComandoForja(Opc.Candidato, ALVO_CANDIDATO, Tarefa_.Modo, Tarefa_.Caso.Fonte, Obra_.Elf)
    Obra_.Registro = RegistroDoCandidato(Tarefa_, Obra_, CompilarMedido(Comando, Obra_.Elf, Opc.Tempo, Obra_))
    return Obra_


def ErroDeFrente(Codigo: str | None, Opc: Opcoes) -> bool:
    """Conjunto linux: E-code do front-end (lexico, sintaxe, semantica) do oraculo nao e falha do harness; o candidato tem de recusar igual."""
    return Opc.Linux and bool(Codigo) and Codigo.startswith("E")


def RegistroDoCandidato(Tarefa_: Tarefa, Obra_: Obra, Falha_: Falha | None) -> dict | None:
    """None = segue para rodar. Com o oraculo recusando no front-end: mesmo E-code do candidato = PULADO (igual nos dois alvos); o resto diverge."""
    if Obra_.CodigoOraculo is not None:
        if Falha_ and Falha_.Codigo == Obra_.CodigoOraculo:
            return MontarRegistro(Tarefa_, "PULADO", f"{FRENTE_IGUAL} ({Falha_.Codigo})", f"{FRENTE_IGUAL} ({Falha_.Codigo})")
        Situacao = f"candidato recusou com {Falha_.Codigo}" if Falha_ else "candidato compilou"
        return MontarRegistro(Tarefa_, "diverge", f"oraculo recusou no front-end ({Obra_.CodigoOraculo}) e {Situacao}", f"oraculo recusou no front-end ({Obra_.CodigoOraculo})")
    if Falha_:
        return RegistroDeBuild(Tarefa_, "candidato", Falha_)
    return None


def RodarOraculos(Obras: list, Opc: Opcoes) -> None:
    """Um lote no WSL para todos os oraculos compilados; falha do lote vira falha de run em cada obra."""
    Vivas = [Obra_ for Obra_ in Obras if Obra_.Registro is None and Obra_.Oraculo is not None]
    if not Vivas:
        return
    Programas = [ProgramaDoCaso(Obra_.Tarefa_.Caso, str(Obra_.Indice), Obra_.Oraculo, Opc.Tempo) for Obra_ in Vivas]
    Resultados, Erro = RodarLoteWsl(Programas)
    for Indice, Obra_ in enumerate(Vivas):
        Obra_.SaidaOraculo = Saida(b"", b"", None, f"lote: {Erro}") if Erro else SaidaDoResultado(Resultados[Indice], Opc.Tempo)
        if Opc.Cache and not Obra_.SaidaOraculo.Motivo:
            GravarCache(Obra_.ChaveOraculo, Obra_.SaidaOraculo)


ENV_EXTRA: dict = {}  # --env CHAVE=VALOR: so no ambiente do candidato no qemu (ex.: FORJA_A64_JANELA_ALTA=1 para forcar a janela alta)


def TempoConhecido() -> dict:
    """tempo_conhecido_a64.json: lista de {caso, segundos, motivo}; devolve {caso: segundos}. So o limite de TEMPO muda (a comparacao segue igual); arquivo ausente = nenhum."""
    if not ARQUIVO_TEMPO.is_file():
        return {}
    Dados = json.loads(ARQUIVO_TEMPO.read_text(encoding="utf-8"))
    if not (isinstance(Dados, list) and all(isinstance(I, dict) and {"caso", "segundos", "motivo"} <= I.keys() for I in Dados)):
        raise SystemExit(f"{ARQUIVO_TEMPO.name}: lista de {{caso, segundos, motivo}}")
    return {Item["caso"]: int(Item["segundos"]) for Item in Dados}


def TempoDoCaso(Caso: conjunto_a64.Caso, Padrao: int) -> int:
    """Limite do candidato no qemu: o do --timeout-seg, ou o da tabela tempo_conhecido quando for maior (caso fora da tabela fica no padrao)."""
    return max(Padrao, TempoConhecido().get(Caso.Nome, 0))


def ProgramaQemu(Obra_: Obra, Opc: Opcoes) -> wsl_lote.Programa:
    """O lote roda o Elf do programa direto: o Elf vira um sh na pasta da obra que faz exec do qemu-aarch64 no ELF (o exec deixa o pid do qemu; o timeout do lote o mata)."""
    Sh = Obra_.Elf.parent / f"qemu{Obra_.Indice}.sh"  # nome por obra: o lote copia o Elf no momento de rodar
    Sh.write_bytes(f"#!/bin/sh\nexec qemu-aarch64 {wsl_lote.Aspas(wsl_lote.CaminhoWsl(Obra_.Elf))} \"$@\"\n".encode("utf-8"))
    Programa_ = ProgramaDoCaso(Obra_.Tarefa_.Caso, str(Obra_.Indice), Sh, TempoDoCaso(Obra_.Tarefa_.Caso, Opc.Tempo))
    Programa_.Env = {**Programa_.Env, **ENV_EXTRA}
    return Programa_


def RodarCandidatos(Obras: list, Opc: Opcoes) -> None:
    """Um lote no WSL para os candidatos que nao estao no cache (chave = sha do ELF + entradas). Falha do lote vira falha de run em cada obra."""
    Vivas = [Obra_ for Obra_ in Obras if Obra_.Registro is None]
    Pendentes, Programas = [], []
    for Obra_ in Vivas:
        Obra_.ChaveQemu = ChaveDaEntrada(Obra_.Tarefa_, ShaDoArquivo(Obra_.Elf))
        Obra_.SaidaQemu = LerCache(Obra_.ChaveQemu) if Opc.Cache else None
        if Obra_.SaidaQemu is None:
            Pendentes.append(Obra_)
            Programas.append(ProgramaQemu(Obra_, Opc))
    if not Programas:
        return
    Resultados, Erro = RodarLoteWsl(Programas)
    for Indice, Obra_ in enumerate(Pendentes):
        Obra_.SaidaQemu = Saida(b"", b"", None, f"lote: {Erro}") if Erro else SaidaDoResultado(Resultados[Indice], TempoDoCaso(Obra_.Tarefa_.Caso, Opc.Tempo))
        if Opc.Cache and not Obra_.SaidaQemu.Motivo:
            GravarCache(Obra_.ChaveQemu, Obra_.SaidaQemu)


def RodarEmulador(Elf: Path, Tempo: int) -> Saida:
    """Roda o ELF no emulador Android (executar --json, uma chamada de cada vez). Codigo 2 da ferramenta = infraestrutura."""
    Comando = [sys.executable, "-I", str(EMULADOR), "executar", str(Elf), "--timeout", str(Tempo), "--json"]
    with TRANCA_EMULADOR:
        Processo = ExecutarProcesso(Comando, AQUI, Tempo + TEMPO_STATUS)
    if Processo.Motivo:
        return Saida(b"", b"", None, f"emulador {Processo.Motivo}")
    try:
        Dados = json.loads(Processo.Stdout.decode("utf-8", "replace"))
    except ValueError:
        return Saida(b"", Processo.Stderr, None, f"ferramenta sem JSON (codigo {Processo.Exit})")
    if Dados.get("timeout"):
        return Saida(b"", b"", None, f"passou de {Tempo} s")
    if Dados.get("codigo_saida") is None:
        return Saida(b"", b"", None, "falha de infraestrutura do emulador")
    return Saida(Dados["stdout"].encode("utf-8"), Dados["stderr"].encode("utf-8"), Dados["codigo_saida"])


def VerificarEmulador() -> str | None:
    """None se o emulador esta de pe; senao o motivo (o harness nao inicia o emulador: quem inicia e o usuario, com `iniciar`)."""
    if not EMULADOR.is_file():
        return f"emulador ausente: {EMULADOR}"
    Processo = ExecutarProcesso([sys.executable, "-I", str(EMULADOR), "status", "--json"], AQUI, TEMPO_STATUS)
    if Processo.Motivo:
        return f"emulador: status {Processo.Motivo}"
    try:
        Dados = json.loads(Processo.Stdout.decode("utf-8", "replace"))
    except ValueError:
        return "emulador: status sem JSON"
    if not Dados.get("de_pe"):
        return "emulador fora do ar: rode `android_emulador.py iniciar` antes (este harness nao sobe o emulador)"
    return None


def CompararComReferencia(Referencia, Saida_: Saida) -> list:
    """Motivos de divergencia (lista vazia = ok). Referencia Esperado: stdout e exit; Saida do oraculo: + stderr normalizado.
    Stdout que so difere no sinal de um NaN nao e motivo aqui: AvaliarElf o trata como classe nan_bits."""
    Motivos = []
    if Referencia.Stdout != Saida_.Stdout and not SaidaSoNan(Referencia.Stdout, Saida_.Stdout):
        Motivos.append(f"stdout difere: referencia {Referencia.Stdout[:80]!r} x candidato {Saida_.Stdout[:80]!r}")
    if isinstance(Referencia, Saida):
        Esquerda = NormalizarLinhasR(Referencia.Stderr)
        Direita = NormalizarLinhasR(Saida_.Stderr)
        if Esquerda != Direita:
            Motivos.append(f"stderr difere: referencia {Esquerda[:120]!r} x candidato {Direita[:120]!r}")
    if Referencia.Exit != Saida_.Exit:
        Motivos.append(f"exit {Referencia.Exit} (referencia) x {Saida_.Exit} (candidato)")
    return Motivos


def MotivosDoErro(Caso_, Saida_: Saida) -> list:
    """Checagens do stderr que o caso pede (ErroTem/ErroVazio do conjunto linux; o conjunto a64 nao os tem)."""
    Texto = Saida_.Stderr.decode("utf-8", "replace")
    Motivos = []
    if getattr(Caso_, "ErroTem", None) and Caso_.ErroTem not in Texto:
        Motivos.append(f"stderr sem {Caso_.ErroTem}: {Saida_.Stderr[:120]!r}")
    if getattr(Caso_, "ErroVazio", False) and Saida_.Stderr:
        Motivos.append(f"stderr nao vazio: {Saida_.Stderr[:120]!r}")
    return Motivos


def AvaliarElf(Tarefa_: Tarefa, Elf: Path, Referencia, SaidaQemu: Saida, Opc: Opcoes) -> dict:
    """Compara a saida do qemu (vinda do lote) e, com --emulador, a do emulador com a referencia; a 1a falha de run encerra.
    Stdout que so difere no sinal de um NaN (e exit e stderr iguais) vira diverge com classe nan_bits; a entrada da lista aceita decide."""
    Saidas = {"qemu": SaidaQemu}
    if Opc.Emulador:
        Saidas["emulador"] = RodarEmulador(Elf, Opc.Tempo)
    Motivos, Nans = [], []
    for Nome, Saida_ in Saidas.items():
        if Saida_.Motivo:
            return MontarRegistro(Tarefa_, "falha de run", f"{Nome}: {Saida_.Motivo}", Saida_.Motivo[:CAUSA_TAMANHO])
        Motivos += [f"{Nome}: {Motivo}" for Motivo in CompararComReferencia(Referencia, Saida_) + MotivosDoErro(Tarefa_.Caso, Saida_) + MotivosDoEstado(Tarefa_.Caso, Saida_)]
        if SaidaSoNan(Referencia.Stdout, Saida_.Stdout):
            Nans.append(f"{Nome}: stdout difere so no sinal do NaN")
    Todos = Motivos + Nans
    Registro = MontarRegistro(Tarefa_, "diverge" if Todos else "ok", "; ".join(Todos), CausaDaDivergencia(Todos))
    Registro["classe"] = "nan_bits" if Nans and not Motivos else ""
    return Registro


def MotivosDoEstado(Caso_, Saida_: Saida) -> list:
    """PosEsperado (conjunto linux): o stdout do Pos (sh como root, depois do programa, cwd = pasta do caso) tem de ser igual aos bytes esperados."""
    Esperado_ = getattr(Caso_, "PosEsperado", None)
    if Esperado_ is not None and Saida_.Pos != Esperado_:
        return [f"estado do arquivo: {Saida_.Pos[:80]!r} != {Esperado_[:80]!r}"]
    return []


def CausaDaDivergencia(Motivos: list) -> str:
    """Causa de um diverge = o tipo do 1o motivo ('stdout difere', 'exit ...'), sem o nome do lado: 'qemu: stdout difere: ...'."""
    return Motivos[0].split(": ")[1][:CAUSA_TAMANHO] if Motivos else ""


def EsperadoDoModo(Tarefa_: Tarefa) -> Path:
    """Em release usa `<caso>_release.esperado` quando existe (o release faz wrap onde o dev confere estouro); senao o .esperado comum."""
    Padrao = Tarefa_.Caso.Esperado
    if Tarefa_.Modo == "release":
        Especifico = Padrao.with_name(Padrao.stem + "_release.esperado")
        if Especifico.is_file():
            return Especifico
    return Padrao


def LerEsperado(Arquivo: Path) -> Esperado:
    """O .esperado e JSON {"exit": N, "stdout": "texto"} (stdout em UTF-8, LF). Levanta ValueError se faltar campo."""
    Dados = json.loads(Arquivo.read_text(encoding="utf-8"))
    if not isinstance(Dados, dict) or not isinstance(Dados.get("exit"), int) or not isinstance(Dados.get("stdout"), str):
        raise ValueError(f"{Arquivo.name}: falta 'exit' (inteiro) ou 'stdout' (texto)")
    return Esperado(Dados["exit"], Dados["stdout"].encode("utf-8"))


def ReferenciaDaObra(Obra_: Obra, Opc: Opcoes) -> tuple:
    """(referencia, erro): o .esperado no --minimo; o esperado do proprio caso (conjunto linux); senao a saida do oraculo no WSL."""
    if Opc.Minimo:
        try:
            return LerEsperado(EsperadoDoModo(Obra_.Tarefa_)), None
        except (OSError, ValueError) as Erro:
            return None, f"esperado: {Erro}"
    Caso_ = Obra_.Tarefa_.Caso
    if getattr(Caso_, "Esperado", None) is not None:
        return Esperado(Caso_.Exit, Caso_.Esperado), None
    if Obra_.SaidaOraculo.Motivo:
        return None, f"oraculo: {Obra_.SaidaOraculo.Motivo}"
    return Obra_.SaidaOraculo, None


def AvaliarObra(Obra_: Obra, Opc: Opcoes) -> dict:
    """O ms do registro = builds da obra + run no qemu (0 quando veio do cache ou nao rodou)."""
    if Obra_.Registro is None:
        Obra_.Registro = AvaliarRun(Obra_, Opc)
    Obra_.Registro["ms"] = Obra_.MsBuild + (Obra_.SaidaQemu.Ms if Obra_.SaidaQemu else 0)
    return Obra_.Registro


def AvaliarRun(Obra_: Obra, Opc: Opcoes) -> dict:
    Referencia, Erro = ReferenciaDaObra(Obra_, Opc)
    if Erro:
        return MontarRegistro(Obra_.Tarefa_, "falha de run", Erro, Erro[:CAUSA_TAMANHO])
    return AvaliarElf(Obra_.Tarefa_, Obra_.Elf, Referencia, Obra_.SaidaQemu, Opc)


def Paralelos() -> int:
    return min(8, os.cpu_count() or 4)


def RodarTarefas(Tarefas: list, Opc: Opcoes, Jobs: int) -> list:
    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        Obras = list(Piscina.map(lambda Item: ConstruirObra(Item[1], Item[0], Opc), enumerate(Tarefas)))
    if not Opc.Minimo:
        RodarOraculos(Obras, Opc)
    RodarCandidatos(Obras, Opc)
    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        return list(Piscina.map(lambda Obra_: AvaliarObra(Obra_, Opc), Obras))


def CarregarAceitas() -> list:
    """divergencias_aceitas_a64.json: cada entrada e uma classe, {caso, modo, motivo} ou {nome, motivo}; max 3 classes; arquivo ausente = nenhuma.
    Erro de formato para a rodada (configuracao)."""
    if not ARQUIVO_ACEITAS.is_file():
        return []
    Dados = json.loads(ARQUIVO_ACEITAS.read_text(encoding="utf-8"))
    Formato = isinstance(Dados, list) and all(EntradaDeAceita(Item) for Item in Dados)
    if not Formato or len(Dados) > MAXIMO_ACEITAS:
        raise SystemExit(f"{ARQUIVO_ACEITAS.name}: lista de {{caso, modo, motivo}} ou {{nome, motivo}} com no maximo {MAXIMO_ACEITAS} entradas")
    return Dados


def EntradaDeAceita(Item) -> bool:
    """Entrada valida: {caso, modo, motivo} (um caso num modo) ou {nome, motivo} (classe nomeada, ex.: nan_bits)."""
    return isinstance(Item, dict) and "motivo" in Item and ({"caso", "modo"} <= Item.keys() or "nome" in Item)


def AplicarAceitas(Registros: list, Aceitas: list) -> None:
    """Divergencia vira `aceita` pela classe nomeada (campo classe do registro = nome da entrada) ou pelo caso + modo (+ env) da entrada.
    Fica no relatorio com o motivo da lista, mas nao reprova; o campo classe passa a dizer qual entrada a aceitou."""
    Ambiente = ";".join(f"{Chave}={Valor}" for Chave, Valor in sorted(ENV_EXTRA.items()))  # aceita com "env" so vale com o mesmo --env
    Mapa = {(Item["caso"], Item["modo"], Item.get("env", "")): Item for Item in Aceitas if "caso" in Item}
    Classes = {Item["nome"]: Item for Item in Aceitas if "nome" in Item}
    for Registro in Registros:
        if Registro["status"] != "diverge":
            continue
        Item = Classes.get(Registro["classe"]) or Mapa.get((Registro["tarefa"], Registro["modo"], Ambiente))
        if Item:
            Registro["status"] = "aceita"
            Registro["motivo"] = f"ACEITA ({Item['motivo']}): {Registro['motivo']}"
            Registro["classe"] = Item.get("nome") or f"{Item['caso']} [{Item['modo']}]"


def ClassesAceitas(Registros: list) -> int:
    """Quantas classes aceitas (entradas da lista) pegaram algum registro; o limite MAXIMO_ACEITAS e de classes, nao de registros."""
    return len({R["classe"] for R in Registros if R["status"] == "aceita"})


def ImprimirLista(Tarefas: list) -> None:
    for Tarefa_ in Tarefas:
        print(f"{Tarefa_.Caso.Nome} [{Tarefa_.Modo}]")
    print(f"tarefas: {len(Tarefas)} ({len({T.Caso.Nome for T in Tarefas})} casos)")


def ImprimirRegistros(Registros: list) -> None:
    for Registro in Registros:
        Linha = f"{Registro['tarefa']} {Registro['modo']} {Registro['status']}"
        if Registro["motivo"]:
            Linha += f" {Registro['motivo'][:300]}"
        print(Linha)


def Resumir(Registros: list, Causas: list) -> int:
    Pulados = [R for R in Registros if R["status"] == "PULADO"]
    Avaliados = [R for R in Registros if R["status"] != "PULADO"]
    if not Avaliados:
        print(f"verificar_a64: nenhuma tarefa avaliada (PULADO {len(Pulados)}; confira --filtro, --modos e --minimo)")
        return 1
    Contagens = {Status: sum(1 for R in Avaliados if R["status"] == Status) for Status in STATUS_VALIDOS}
    Passam = Contagens["ok"] + Contagens["aceita"]
    Aceitos = sum(1 for R in Pulados if PuladoAceito(R, Causas))  # PULADO nas classes de pulados_aceitos_a64.json (ou front-end igual, X0003)
    Extra = f", aceitas {ClassesAceitas(Registros)} classes ({Contagens['aceita']} registros)" if Contagens["aceita"] else ""
    print(f"verificar_a64: {Passam}/{len(Avaliados)} (diverge {Contagens['diverge']}, falha de build {Contagens['falha de build']}, "
          f"falha de run {Contagens['falha de run']}, PULADO {len(Pulados)} (aceitos {Aceitos}){Extra})")
    return 0 if Passam == len(Avaliados) else 1


def ImprimirCausas(Registros: list) -> None:
    """PULADO agrupado pelo campo causa: contagem e ate 3 nomes de cada grupo; so as TOP_CAUSAS de maior contagem."""
    Grupos = {}
    for Registro in Registros:
        if Registro["status"] == "PULADO":
            Grupos.setdefault(Registro["causa"], []).append(f"{Registro['tarefa']} [{Registro['modo']}]")
    for Causa, Nomes in sorted(Grupos.items(), key=lambda Item: (-len(Item[1]), Item[0]))[:TOP_CAUSAS]:
        print(f"PULADO {len(Nomes)} x {Causa}: {'; '.join(Nomes[:3])}")


STATUS_JSON = {"falha de build": "falha", "falha de run": "falha", "PULADO": "pulado"}  # o --json usa os nomes da tabela do enunciado


def GravarTriagem(Registros: list, Caminho: Path) -> None:
    """--json: um registro por tarefa (caso, modo, status, causa, ms), contagem por status e resumo por (status, causa)."""
    Linhas = [{"caso": R["tarefa"], "modo": R["modo"], "status": STATUS_JSON.get(R["status"], R["status"]), "causa": R["causa"], "ms": R["ms"]} for R in Registros]
    Resumo = Counter((Linha["status"], Linha["causa"]) for Linha in Linhas).most_common()
    Dados = {"contagem": dict(Counter(Linha["status"] for Linha in Linhas)),
             "por_causa": [{"status": Status, "causa": Causa, "n": N} for (Status, Causa), N in Resumo], "registros": Linhas}
    Caminho.write_text(json.dumps(Dados, ensure_ascii=False, indent=1), encoding="utf-8")


def PuladoAnterior() -> int | None:
    """PULADO total da ultima triagem gravada (None = nao ha): o --fechar diz se o PULADO subiu em relacao a ela."""
    if not ARQUIVO_TRIAGEM.is_file():
        return None
    return json.loads(ARQUIVO_TRIAGEM.read_text(encoding="utf-8"))["contagem"].get("pulado", 0)


def CarregarCausasAceitas() -> list:
    """pulados_aceitos_a64.json: lista de causas (texto da coluna causa) cujo X0001 e aceito pelo --fechar; arquivo ausente = nenhuma."""
    if not ARQUIVO_PULADOS.is_file():
        return []
    Dados = json.loads(ARQUIVO_PULADOS.read_text(encoding="utf-8"))
    if not isinstance(Dados, list) or not all(isinstance(Item, str) for Item in Dados):
        raise SystemExit(f"{ARQUIVO_PULADOS.name}: lista de causas (textos)")
    return Dados


def PuladoAceito(Registro: dict, Causas: list) -> bool:
    """As 3 classes aceitas do PULADO no --fechar: front-end igual nos dois alvos, X0003, e X0001 com a causa listada."""
    Causa = Registro["causa"]
    return Causa.startswith(FRENTE_IGUAL) or Causa.startswith("X0003") or Causa in Causas


def TendenciaDoPulado(Atual: int, Anterior: int | None) -> str:
    if Anterior is None:
        return "sem triagem anterior"
    if Atual == Anterior:
        return f"igual a {Anterior}"
    return f"{'subiu' if Atual > Anterior else 'caiu'} de {Anterior}"


def Fechar(Registros: list, Causas: list, Anterior: int | None) -> int:
    """Criterio de parada da A3 (--fechar): 0 so com diverge, falha de build e falha de run em 0, aceitas nomeadas e PULADO so nas classes aceitas."""
    Contagem = Counter(R["status"] for R in Registros)
    Pulados = [R for R in Registros if R["status"] == "PULADO"]
    Fora = [R for R in Pulados if not PuladoAceito(R, Causas)]
    Abertos = Contagem["diverge"] + Contagem["falha de build"] + Contagem["falha de run"] + len(Fora)
    Classes = ClassesAceitas(Registros)
    print(f"fechar A3: PULADO {len(Pulados)} ({TendenciaDoPulado(len(Pulados), Anterior)}); fora das classes aceitas {len(Fora)}; aceitas {Classes} classes")
    if Abertos or Classes > MAXIMO_ACEITAS or len(Pulados) == len(Registros):
        print(f"fechar A3: ABERTO (diverge {Contagem['diverge']}, falha de build {Contagem['falha de build']}, falha de run {Contagem['falha de run']}, PULADO fora {len(Fora)})")
        return 1
    print("fechar A3: FECHADO")
    return 0


def ImprimirContagem(Tarefas: list) -> None:
    """--conjunto linux --lista: so a contagem (1732 casos x modos seriam linhas demais)."""
    Modos = Counter(Tarefa_.Modo for Tarefa_ in Tarefas)
    print(f"conjunto linux: {len({T.Caso.Nome for T in Tarefas})} casos, {len(Tarefas)} tarefas (dev {Modos['dev']}, release {Modos['release']})")


def CarregarConjunto(Linux: bool, Minimo: bool) -> list:
    """O conjunto linux vem de conjunto_linux (import tardio: a lista de 1732 casos so e montada quando pedida); senao o a64 (padrao ou --minimo)."""
    if Linux:
        import conjunto_linux  # noqa: E402 (tardio de proposito: TodoOConjunto monta a lista inteira)
        return conjunto_linux.TodoOConjunto()
    return conjunto_a64.ConjuntoDo(Minimo)


def MontarAnalisador() -> argparse.ArgumentParser:
    """Opcoes do harness. Texto de help com % precisa de %% (o argparse formata a string): sem isso --help quebra."""
    Analisador = argparse.ArgumentParser(description="Verifica o alvo aarch64-linux contra o oraculo x86_64-linux (WSL + qemu-user; --emulador).")
    Analisador.add_argument("--forja", default=str(FORJA_PADRAO), help="oraculo e padrao do candidato: forja.exe (padrao: language\\compiler\\forja.exe)")
    Analisador.add_argument("--candidato", default=None, help="forja com --target aarch64-linux (padrao: o mesmo de --forja)")
    Analisador.add_argument("--modos", default="dev,release", help="dev, release ou dev,release")
    Analisador.add_argument("--filtro", default="", help="so os casos cujo nome contem este texto")
    Analisador.add_argument("--lista", action="store_true", help="so lista as tarefas do conjunto e sai 0")
    Analisador.add_argument("--minimo", action="store_true", help="so as fixtures de tests\\nativo\\a64\\fixtures com .esperado (sem oraculo, sem runtime Forja)")
    Analisador.add_argument("--conjunto", default="padrao", choices=("padrao", CONJUNTO_LINUX), help="padrao = m1 + 01_ola; linux = casos_linux (1732 casos, conjunto_linux.TodoOConjunto)")
    Analisador.add_argument("--sem-cache", action="store_true", help="nao le nem grava o cache de oraculo e candidato (%%TEMP%%\\.verificar_a64_cache)")
    Analisador.add_argument("--emulador", action="store_true", help="roda tambem no emulador Android (exige o emulador de pe)")
    Analisador.add_argument("--jobs", type=int, default=0, help="tarefas em paralelo (0 = min(8, nucleos))")
    Analisador.add_argument("--timeout-seg", type=int, default=TEMPO_PADRAO, help="limite de cada build ou execucao, em segundos")
    Analisador.add_argument("--selftest", action="store_true", help="sem compilador: provas puras (causa, recusa so no candidato, PosEsperado) e ELFs plantados no qemu-user do WSL")
    Analisador.add_argument("--json", default="", help="grava os registros (caso, modo, status, causa, ms) e o resumo por causa neste arquivo")
    Analisador.add_argument("--env", action="append", default=[], metavar="CHAVE=VALOR", help="variavel a mais no ambiente do candidato (qemu); repetivel; ex.: FORJA_A64_JANELA_ALTA=1 forca a janela alta")
    Analisador.add_argument("--fechar", action="store_true",help="criterio de parada da A3: exit 0 so com diverge, falha de build e falha de run em 0 e PULADO so nas classes aceitas")
    return Analisador


def Principal() -> int:
    Analisador = MontarAnalisador()
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        import verificar_a64_selftest  # noqa: E402 (tardio de proposito: o selftest importa este modulo de volta)
        return verificar_a64_selftest.Selftest()
    Modos = Argumentos.modos.split(",")
    if not set(Modos) <= set(MODOS_VALIDOS):
        Analisador.error("--modos aceita dev e release")
    Linux = Argumentos.conjunto == CONJUNTO_LINUX
    if Linux and (Argumentos.minimo or Argumentos.emulador):
        Analisador.error("--conjunto linux nao anda com --minimo nem com --emulador (o emulador roda so o ELF, sem args)")
    Casos = CarregarConjunto(Linux, Argumentos.minimo)
    Tarefas = [T for T in (TarefasDoLinux(Casos, Modos) if Linux else ListarTarefas(Casos, Modos)) if Argumentos.filtro in T.Caso.Nome]
    if Argumentos.lista:
        ImprimirContagem(Tarefas) if Linux else ImprimirLista(Tarefas)
        return 0
    if Argumentos.emulador:
        Erro = VerificarEmulador()
        if Erro:
            print(f"verificar_a64: {Erro}", file=sys.stderr)
            return CODIGO_EMULADOR_FORA
    if not Tarefas:
        print("verificar_a64: nenhuma tarefa (confira --filtro, --modos e --minimo)")
        return 1
    ENV_EXTRA.update(dict(Par.split("=", 1) for Par in Argumentos.env))
    Anterior = PuladoAnterior() if Argumentos.fechar else None  # lido antes do --json, que pode gravar o mesmo arquivo
    Candidato = Argumentos.candidato or Argumentos.forja
    with tempfile.TemporaryDirectory(prefix="forja_a64_") as Pasta:
        Opc = Opcoes(Path(Argumentos.forja).resolve(), Path(Candidato).resolve(), Path(Pasta), Argumentos.timeout_seg, Argumentos.minimo, Argumentos.emulador,
                     Linux=Linux, Cache=not Argumentos.sem_cache, ShaForja=ShaDoArquivo(Path(Argumentos.forja).resolve()))
        Registros = RodarTarefas(Tarefas, Opc, Argumentos.jobs or Paralelos())
    AplicarAceitas(Registros, CarregarAceitas())
    ImprimirRegistros(Registros)
    ImprimirCausas(Registros)
    if Argumentos.json:
        GravarTriagem(Registros, Path(Argumentos.json).resolve())
    Causas = CarregarCausasAceitas()
    Codigo = Resumir(Registros, Causas)
    if Argumentos.fechar:
        return Fechar(Registros, Causas, Anterior)
    return Codigo


if __name__ == "__main__":
    sys.exit(Principal())
