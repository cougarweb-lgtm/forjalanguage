#!/usr/bin/env python3
"""wsl_lote.py - roda um LOTE de executaveis Linux no WSL (distro alpine-build) com UMA chamada de wsl.exe (M5 P28).

Objetivo: o custo de uma chamada do wsl.exe (~150 ms quente) domina qualquer teste; o lote monta tudo em %TEMP%\\forja_linux\\<suite>, gera um runner.sh (LF, sem BOM)
e roda todos os programas nele. Cada programa recebe sua pasta de trabalho ($D, em /tmp, gravavel por todos), um prefixo opcional de preparo (sh), variaveis de ambiente,
timeout (padrao 10 s) e, opcionalmente, o usuario nobody (EACCES real; o WSL roda como root).

API:  DistroDisponivel() -> bool | ExecutarLote(Programas, Suite) -> list[Resultado]
      Programa(Nome, Elf, Args=[], Pre="", Env={}, Usuario=None, Limite=10); "@D@" dentro de um argumento vira a pasta de trabalho no Linux.
      Resultado(Nome, Stdout, Stderr, Exit, Sinal, Ms, Estourou, Pos): Exit = status cru do sh; Sinal = Exit - 128 quando Exit > 128 (so uma PISTA: o sh nao separa
      exit(139) de SIGSEGV; confira o Exit esperado); 124 do timeout = Estourou (Exit e Sinal None).
Saida: resultado.jsonl na pasta do lote (stdout/stderr em base64, exit, sinal, ms).
Regras: wsl.exe sempre por lista de argumentos (o Git Bash converte caminhos); o aviso UTF-16 "Falha ao detectar suporte a virtualizacao aninhada" e filtrado;
        o runner apaga o /tmp/forja_lote_* que criou (trap); nada e instalado na distro; nunca wsl --shutdown.
Verificacao: python -I wsl_lote.py --selftest   (exit 0 = verde ou PULADO sem a distro)
Reversao: apagar este arquivo.

Versao: 0.3.0
Changelog: 0.3.0 (22/11/2026) A2.2(c) - estouro detectado tambem pelo relogio (busybox do alpine-build devolve 0 no timeout, nao 124): EstourouPeloTempo.
           0.2.0 (14/11/2026) M5 P28b-1 - Programa.Entrada (bytes ligados ao stdin do programa; None = /dev/null); o selftest confere o stdin.
           0.1.0 (14/11/2026) M5 P28 - primeira versao.
"""
import argparse
import base64
import json
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

DISTRO = "alpine-build"
LIMITE_PADRAO = 10
LIMITE_WSL_SEMDISTRO = 60
AVISO_VIRTUALIZACAO = "Falha ao detectar suporte a virtualizacao aninhada"
STATUS_TIMEOUT = 124
TOLERANCIA_TEMPO_MS = 20
BASE_SINAL = 128
NOBODY = "nobody"


@dataclass
class Programa:
    Nome: str
    Elf: Path
    Args: list = field(default_factory=list)
    Pre: str = ""
    Env: dict = field(default_factory=dict)
    Usuario: str | None = None
    Limite: int = LIMITE_PADRAO
    Dados: dict = field(default_factory=dict)  # nome -> bytes: arquivos copiados para $D antes do Pre
    Pos: str = ""  # sh rodado como root depois do programa (cwd = $D); o stdout dele vira Resultado.Pos
    Entrada: bytes | None = None  # stdin do programa (P28b-1); None = /dev/null


@dataclass
class Resultado:
    Nome: str
    Stdout: bytes
    Stderr: bytes
    Exit: int | None
    Sinal: int | None
    Ms: int
    Estourou: bool
    Pos: bytes = b""


def CaminhoWsl(Caminho) -> str:
    """C:\\a\\b -> /mnt/c/a/b (a distro monta os discos do Windows)."""
    Texto = str(Caminho).replace("\\", "/")
    return "/mnt/" + Texto[0].lower() + Texto[2:]


def Wsl(Argumentos: list, Limite: int) -> subprocess.CompletedProcess:
    return subprocess.run(["wsl", "-d", DISTRO, "--"] + Argumentos, capture_output=True, timeout=Limite, stdin=subprocess.DEVNULL)


def TextoDoWsl(Bruto: bytes) -> str:
    """O wsl.exe escreve os proprios avisos em UTF-16LE; o resto (saida da distro) e UTF-8. Remove o aviso conhecido."""
    Texto = Bruto.decode("utf-16-le", "replace") if b"\x00" in Bruto[:8] else Bruto.decode("utf-8", "replace")
    return "\n".join(L for L in Texto.splitlines() if AVISO_VIRTUALIZACAO not in L).strip()


def DistroDisponivel() -> bool:
    """True se a distro responde em ate 60 s; False sem wsl.exe, sem a distro ou sem resposta."""
    try:
        return Wsl(["true"], LIMITE_WSL_SEMDISTRO).returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def Aspas(Texto: str) -> str:
    return "'" + Texto.replace("'", "'\\''") + "'"


def AspasComPasta(Texto: str) -> str:
    """Aspas simples, com cada @D@ virando "$D" (a pasta de trabalho do programa no Linux)."""
    return '"$D"'.join(Aspas(Parte) for Parte in Texto.split("@D@"))


def LinhaDeExecucao(Indice: int, Programa_: Programa) -> str:
    """O comando do programa: timeout, stdin vazio, saida em $T/r; com Usuario roda por su -s /bin/sh nobody."""
    Binario = f'"$T/p{Indice}"'
    Argumentos = " ".join(AspasComPasta(A) for A in Programa_.Args)
    Ambiente = "env " + " ".join(Aspas(f"{Chave}={Valor}") for Chave, Valor in Programa_.Env.items()) + " " if Programa_.Env else ""
    Comando = f"{Binario} {Argumentos}"
    if Programa_.Usuario:
        Comando = f"su -s /bin/sh {Programa_.Usuario} -c 'exec \"$0\" \"$@\"' {Comando}"
    Entrada = f'"$L/entrada/{Indice}"' if Programa_.Entrada is not None else "/dev/null"
    return f'{Ambiente}timeout {Programa_.Limite} {Comando} > "$T/r/{Indice}.out" 2> "$T/r/{Indice}.err" < {Entrada}'


def BlocoDoPrograma(Indice: int, Programa_: Programa) -> str:
    return "\n".join([
        f'D="$T/d{Indice}"; mkdir -p "$D"; chmod 777 "$D"; cd "$D"',
        f'[ -d "$L/dados/{Indice}" ] && cp -r "$L/dados/{Indice}/." "$D"/',
        Programa_.Pre,
        'A=$(cut -d" " -f1 /proc/uptime)',
        LinhaDeExecucao(Indice, Programa_),
        'RC=$?',
        'B=$(cut -d" " -f1 /proc/uptime)',
        f'echo "$RC $A $B" > "$T/r/{Indice}.meta"',
        f'{{ {Programa_.Pos or ":"} ; }} > "$T/r/{Indice}.pos" 2>&1',
        'cd "$T"',
    ])


def GerarRunner(Programas: list, PastaWsl: str) -> str:
    Cabeca = [
        "#!/bin/sh",
        "# gerado por wsl_lote.py",
        f"L={Aspas(PastaWsl)}",
        "T=/tmp/forja_lote_$$",
        'trap \'rm -rf "$T"\' EXIT',
        'mkdir -p "$T/r" && chmod 755 "$T" && cp "$L"/bin/* "$T"/ && chmod 755 "$T"/p*',
        "umask 022",
    ]
    Corpo = [BlocoDoPrograma(I, P) for I, P in enumerate(Programas)]
    Fecho = ['mkdir -p "$L/res" && cp "$T"/r/* "$L/res"/']
    return "\n".join(Cabeca + Corpo + Fecho) + "\n"


def Preparar(Programas: list, Pasta: Path) -> None:
    """Pasta limpa com bin/p<N> (copias dos executaveis) e runner.sh em LF puro."""
    shutil.rmtree(Pasta, ignore_errors=True)
    (Pasta / "bin").mkdir(parents=True)
    for Indice, Item in enumerate(Programas):
        shutil.copyfile(Item.Elf, Pasta / "bin" / f"p{Indice}")
        if Item.Entrada is not None:
            (Pasta / "entrada").mkdir(exist_ok=True)
            (Pasta / "entrada" / str(Indice)).write_bytes(Item.Entrada)
        for NomeDado, Conteudo in Item.Dados.items():
            Destino = Pasta / "dados" / str(Indice) / NomeDado
            Destino.parent.mkdir(parents=True, exist_ok=True)
            Destino.write_bytes(Conteudo)
    Texto = GerarRunner(Programas, CaminhoWsl(Pasta))
    (Pasta / "runner.sh").write_bytes(Texto.encode("utf-8"))


def LerMeta(Texto: str) -> tuple:
    """'rc inicio fim' (uptime em segundos) -> (exit, sinal, ms, estourou)."""
    Rc, Inicio, Fim = Texto.split()
    Codigo = int(Rc)
    Ms = max(0, round((float(Fim) - float(Inicio)) * 1000))
    if Codigo == STATUS_TIMEOUT:
        return None, None, Ms, True
    return Codigo, (Codigo - BASE_SINAL if Codigo > BASE_SINAL else None), Ms, False


def EstourouPeloTempo(Codigo: int, Ms: int, Limite: int) -> bool:
    """O busybox do alpine-build devolve 0 (nao 124) quando o timeout mata o programa: so o relogio prova o estouro.
    Sai 0 ou 143 e gastou o limite inteiro (tolerancia de 20 ms da resolucao do uptime)."""
    return Codigo in (0, BASE_SINAL + 15) and Ms >= Limite * 1000 - TOLERANCIA_TEMPO_MS


def LerResultados(Programas: list, Pasta: Path) -> list:
    Resultados = []
    for Indice, Item in enumerate(Programas):
        Base = Pasta / "res" / str(Indice)
        Meta = Base.with_suffix(".meta")
        if not Meta.is_file():
            raise RuntimeError(f"lote: sem resultado de {Item.Nome} (o runner parou antes?)")
        Codigo, Sinal, Ms, Estourou = LerMeta(Meta.read_text())
        if not Estourou and EstourouPeloTempo(Codigo, Ms, Item.Limite):
            Codigo, Sinal, Estourou = None, None, True
        Resultados.append(Resultado(Item.Nome, Base.with_suffix(".out").read_bytes(), Base.with_suffix(".err").read_bytes(), Codigo, Sinal, Ms, Estourou,
                                    Base.with_suffix(".pos").read_bytes()))
    return Resultados


def GravarJsonl(Resultados: list, Pasta: Path) -> None:
    Linhas = []
    for R in Resultados:
        Linhas.append(json.dumps({"nome": R.Nome, "stdout": base64.b64encode(R.Stdout).decode(), "stderr": base64.b64encode(R.Stderr).decode(),
                                  "exit": R.Exit, "sinal": R.Sinal, "ms": R.Ms}))
    (Pasta / "resultado.jsonl").write_text("\n".join(Linhas) + "\n", encoding="utf-8")


def ExecutarLote(Programas: list, Suite: str) -> list:
    """Uma chamada do wsl.exe para o lote inteiro. Levanta RuntimeError se o wsl falhar ou o runner nao terminar."""
    Pasta = Path(tempfile.gettempdir()) / "forja_linux" / Suite
    Preparar(Programas, Pasta)
    Limite = LIMITE_WSL_SEMDISTRO + sum(P.Limite for P in Programas)
    try:
        Processo = Wsl(["sh", CaminhoWsl(Pasta / "runner.sh")], Limite)
    except subprocess.TimeoutExpired as Erro:
        raise RuntimeError(f"lote {Suite}: wsl.exe passou de {Limite} s") from Erro
    if Processo.returncode != 0:
        raise RuntimeError(f"lote {Suite}: runner saiu {Processo.returncode}: {TextoDoWsl(Processo.stderr)[:300]}")
    Resultados = LerResultados(Programas, Pasta)
    GravarJsonl(Resultados, Pasta)
    return Resultados


def Selftest() -> int:
    if not DistroDisponivel():
        print("wsl_lote selftest: PULADO (WSL/distro ausente)")
        return 0
    Pasta = Path(tempfile.mkdtemp(prefix="forja_selftest_"))
    try:
        Roteiros = {
            "ola": "#!/bin/sh\necho ola\necho \"$1\" >&2\nexit 0\n",
            "sete": "#!/bin/sh\nexit 7\n",
            "queda": "#!/bin/sh\nkill -SEGV $$\n",
            "eco": "#!/bin/sh\ncat\n",
        }
        Programas = []
        for Nome, Texto in Roteiros.items():
            Caminho = Pasta / Nome
            Caminho.write_bytes(Texto.encode("utf-8"))
            Programas.append(Programa(Nome, Caminho, ["a b", "@D@/x"] if Nome == "ola" else [], Entrada=b"entrada\r\n\x00x" if Nome == "eco" else None))
        R = {X.Nome: X for X in ExecutarLote(Programas, "selftest")}
        Falhas = []
        if R["ola"].Stdout != b"ola\n" or R["ola"].Exit != 0 or not R["ola"].Stderr.startswith(b"a b"):
            Falhas.append(f"ola: {R['ola']}")
        if R["sete"].Exit != 7 or R["sete"].Sinal is not None or R["sete"].Pos != b"":
            Falhas.append(f"sete: {R['sete']}")
        if R["queda"].Sinal != 11 or R["queda"].Exit != 139:
            Falhas.append(f"queda: {R['queda']}")
        if R["eco"].Stdout != b"entrada\r\n\x00x" or R["ola"].Exit != 0:
            Falhas.append(f"eco (stdin): {R['eco']}")
        for Linha in Falhas:
            print("FALHA", Linha)
        print("wsl_lote selftest:", "VERDE" if not Falhas else f"{len(Falhas)} falha(s)")
        return 1 if Falhas else 0
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Lote de executaveis Linux no WSL (uma chamada do wsl.exe).")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return Selftest()
    Analisador.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(Principal())
