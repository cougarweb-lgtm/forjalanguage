#!/usr/bin/env python3
"""verificar_gdb.py - gdb em lote contra os binarios da forja (M6 P64, D-M6-10).

Versao: 0.3.0
Changelog: 0.3.0 - P80b: fixtures print_array ([4]i32), print_enum (enum mostra o nome) e print_ptr (^Ponto); caso --pie automatico (linux, dev): ELF static-pie roda no alpine-build (saida 13, exit 0) e o gdb resolve break + print local (D-M6-4).
           0.2.0 - P64: nomes em CamelCase (PY-R01); controle positivo sem condicao redundante.
           0.1.0 - P64: controle positivo (gabarito.c com gcc -g -gdwarf-4, MinGW no Windows e alpine-build no Linux) + fixtures
                   gdb\\*.fj com oraculo gdb\\*.esperado (regex por linha, na ordem). Modo release espera "sem simbolos" (D-M6-5).
Uso: python verificar_gdb.py --forja <forja.exe> [--alvo windows|linux|ambos] [--modo dev|release] [--fixture NOME]
Depende de: gdb (WinLibs no PATH; alpine-build via wsl), gcc (MinGW no PATH; alpine-build via wsl).
Saida: 0 tudo PASSOU; 1 alguma fixture FALHOU; 2 ferramenta ausente; 3 controle positivo FALHOU (oraculo nao confiavel).
"""
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
GABARITO = RAIZ / "gabarito"
FIXTURES = RAIZ / "gdb"
TIMEOUT_S = 90
WSL_DISTRO = "alpine-build"
REGEX_SEM_DWARF = re.compile(r"No line \d+ in|No symbol table|Make breakpoint pending|No source file")
PIE_FIXTURE = "print_campo"
PIE_SAIDA_ESPERADA = "13"


class FerramentaAusente(Exception):
    """gdb, gcc ou wsl nao encontrado: o harness para com status 2."""


def ParaWsl(Caminho: Path) -> str:
    """C:/x/y -> /mnt/c/x/y (o alpine-build enxerga o disco do Windows por /mnt)."""
    Texto = Caminho.as_posix()
    if re.match(r"^[A-Za-z]:/", Texto):
        return "/mnt/" + Texto[0].lower() + Texto[2:]
    return Texto


def Executar(Comando: list) -> str:
    """Roda o comando e devolve stdout+stderr sem as linhas de aviso do wsl."""
    try:
        Saida = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TIMEOUT_S)
    except FileNotFoundError as Erro:
        raise FerramentaAusente(str(Erro)) from Erro
    except subprocess.TimeoutExpired:
        return "TIMEOUT"
    Linhas = (Saida.stdout + Saida.stderr).splitlines()
    return "\n".join(Linha for Linha in Linhas if not Linha.startswith("wsl:"))


def ComandoGdb(Alvo: str, Exe: Path, Comandos: Path) -> list:
    Base = ["-batch", "-nx", "-ex", "set pagination off", "-x"]
    if Alvo == "windows":
        return ["gdb", *Base, Comandos.as_posix(), Exe.as_posix()]
    return ["wsl", "-d", WSL_DISTRO, "--", "gdb", *Base, ParaWsl(Comandos), ParaWsl(Exe)]


def CasarEmOrdem(Saida: str, Esperado: Path) -> tuple:
    """Cada regex do .esperado tem de casar uma linha da saida, na ordem. Devolve (ok, detalhe)."""
    Linhas = Saida.splitlines()
    Inicio = 0
    for Regex in [Linha for Linha in Esperado.read_text(encoding="utf-8").splitlines() if Linha.strip()]:
        Achou = next((i for i in range(Inicio, len(Linhas)) if re.search(Regex, Linhas[i])), None)
        if Achou is None:
            return False, f"nao casou (a partir da linha {Inicio + 1}): {Regex}"
        Inicio = Achou + 1
    return True, ""


def CompilarGabarito(Alvo: str, Tmp: Path) -> Path:
    """gcc -g -gdwarf-4 do controle positivo; levanta RuntimeError se o exe nao sair."""
    Fonte = GABARITO / "gabarito.c"
    Flags = ["-g", "-gdwarf-4", "-O0", "-fno-omit-frame-pointer"]
    if Alvo == "windows":
        Exe = Tmp / "gabarito_windows.exe"
        Comando = ["gcc", *Flags, "-o", Exe.as_posix(), Fonte.as_posix()]
    else:
        Exe = Tmp / "gabarito_linux"
        Comando = ["wsl", "-d", WSL_DISTRO, "--", "gcc", *Flags, "-o", ParaWsl(Exe), ParaWsl(Fonte)]
    Saida = Executar(Comando)
    if not Exe.exists():
        raise RuntimeError(f"gcc nao gerou o controle: {Saida[-300:]}")
    return Exe


def CompilarFixture(Forja: Path, Fonte: Path, Alvo: str, Modo: str, Exe: Path) -> str:
    Comando = [Forja.as_posix(), "build"]
    if Alvo == "linux":
        Comando += ["--target", "x86_64-linux"]
    if Modo == "release":
        Comando.append("--release")
    Comando += [Fonte.as_posix(), "-o", Exe.as_posix()]
    return Executar(Comando)


def VerificarControle(Alvo: str, Tmp: Path) -> tuple:
    """O gabarito tem de passar no harness: prova que o oraculo e a ferramenta funcionam."""
    try:
        Exe = CompilarGabarito(Alvo, Tmp)
    except RuntimeError as Erro:
        return False, str(Erro)
    Saida = Executar(ComandoGdb(Alvo, Exe, GABARITO / "gabarito.gdb"))
    return CasarEmOrdem(Saida, GABARITO / "gabarito.esperado")


def VerificarFixture(Forja: Path, Nome: str, Alvo: str, Modo: str, Tmp: Path) -> tuple:
    Fonte = FIXTURES / f"{Nome}.fj"
    Exe = Tmp / f"{Nome}_{Alvo}_{Modo}.exe"
    ErroBuild = CompilarFixture(Forja, Fonte, Alvo, Modo, Exe)
    if not Exe.exists():
        return False, f"forja build nao gerou o exe: {ErroBuild[-200:]}"
    Saida = Executar(ComandoGdb(Alvo, Exe, FIXTURES / f"{Nome}.gdb"))
    if Modo == "release":
        if any(REGEX_SEM_DWARF.search(Linha) for Linha in Saida.splitlines()):
            return True, "release sem simbolos: gdb nao acha a linha (D-M6-5)"
        return False, "release: gdb achou linha sem --simbolos"
    return CasarEmOrdem(Saida, FIXTURES / f"{Nome}.esperado")


def RodarNoWsl(Exe: Path) -> tuple:
    """Roda o ELF no alpine-build e devolve (stdout, codigo de saida)."""
    Saida = subprocess.run(["wsl", "-d", WSL_DISTRO, "--", ParaWsl(Exe)], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TIMEOUT_S)
    return Saida.stdout.strip(), Saida.returncode


def VerificarPie(Forja: Path, Tmp: Path) -> tuple:
    """D-M6-4: ELF --pie (static-pie) roda no alpine-build com a saida esperada e o gdb resolve break + print local no DWARF."""
    Exe = Tmp / f"{PIE_FIXTURE}_pie"
    ErroBuild = Executar([Forja.as_posix(), "build", "--target", "x86_64-linux", "--pie", (FIXTURES / f"{PIE_FIXTURE}.fj").as_posix(), "-o", Exe.as_posix()])
    if not Exe.exists():
        return False, f"forja build --pie nao gerou o ELF: {ErroBuild[-200:]}"
    if "DYN (" not in Executar(["wsl", "-d", WSL_DISTRO, "--", "readelf", "-h", ParaWsl(Exe)]):
        return False, "ELF nao e ET_DYN (static-pie)"
    Saida, Codigo = RodarNoWsl(Exe)
    if Saida != PIE_SAIDA_ESPERADA or Codigo != 0:
        return False, f"saida={Saida!r} codigo={Codigo} (esperado {PIE_SAIDA_ESPERADA!r} e 0)"
    Saida = Executar(ComandoGdb("linux", Exe, FIXTURES / f"{PIE_FIXTURE}.gdb"))
    return CasarEmOrdem(Saida, FIXTURES / f"{PIE_FIXTURE}.esperado")


def ListarFixtures(Filtro: str | None) -> list:
    Nomes = sorted(Caminho.stem for Caminho in FIXTURES.glob("*.fj"))
    return [Nome for Nome in Nomes if Filtro is None or Nome == Filtro]


def ImprimirResultado(Estado: str, Alvo: str, Modo: str, Nome: str, Detalhe: str) -> None:
    print(f"{Estado} | {Alvo} | {Modo} | {Nome} | {Detalhe}")


def ChecarFerramentas(Alvos: list) -> None:
    if "windows" in Alvos and shutil.which("gdb") is None:
        raise FerramentaAusente("gdb (WinLibs) fora do PATH")
    if "windows" in Alvos and shutil.which("gcc") is None:
        raise FerramentaAusente("gcc (MinGW) fora do PATH")
    if "linux" in Alvos and shutil.which("wsl") is None:
        raise FerramentaAusente("wsl fora do PATH")


def Rodar(Argumentos: argparse.Namespace) -> int:
    Alvos = ["windows", "linux"] if Argumentos.alvo == "ambos" else [Argumentos.alvo]
    ChecarFerramentas(Alvos)
    DirTemp = Path(tempfile.mkdtemp(prefix="p64_gdb_"))
    Forja = DirTemp / "forja.exe"
    shutil.copy2(Argumentos.forja, Forja)
    for Alvo in Alvos:
        Passou, Detalhe = VerificarControle(Alvo, DirTemp)
        ImprimirResultado("PASSOU" if Passou else "FALHOU", Alvo, "controle", "gabarito", Detalhe or "gdb casou o .esperado")
        if not Passou:
            shutil.rmtree(DirTemp, ignore_errors=True)
            return 3
    Codigo = 0
    for Alvo in Alvos:
        for Nome in ListarFixtures(Argumentos.fixture):
            Passou, Detalhe = VerificarFixture(Forja, Nome, Alvo, Argumentos.modo, DirTemp)
            ImprimirResultado("PASSOU" if Passou else "FALHOU", Alvo, Argumentos.modo, Nome, Detalhe or "gdb casou o .esperado")
            Codigo = Codigo if Passou else 1
    if "linux" in Alvos and Argumentos.modo == "dev":
        Passou, Detalhe = VerificarPie(Forja, DirTemp)
        ImprimirResultado("PASSOU" if Passou else "FALHOU", "linux", "pie", PIE_FIXTURE, Detalhe or "static-pie roda e gdb casou o .esperado")
        Codigo = Codigo if Passou else 1
    shutil.rmtree(DirTemp, ignore_errors=True)
    return Codigo


def MontarArgumentos() -> argparse.Namespace:
    Parser = argparse.ArgumentParser(description="gdb em lote contra os binarios da forja (M6 P64)")
    Parser.add_argument("--forja", type=Path, required=True, help="forja.exe (copiado para o temp antes de usar)")
    Parser.add_argument("--alvo", choices=["windows", "linux", "ambos"], default="ambos")
    Parser.add_argument("--modo", choices=["dev", "release"], default="dev")
    Parser.add_argument("--fixture", default=None, help="so esta fixture (nome sem .fj)")
    return Parser.parse_args()


def Principal() -> int:
    Argumentos = MontarArgumentos()
    try:
        return Rodar(Argumentos)
    except FerramentaAusente as Erro:
        print(f"FERRAMENTA AUSENTE: {Erro}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(Principal())
