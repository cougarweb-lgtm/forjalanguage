#!/usr/bin/env python3
"""auditar_runtime.py - audita o runtime Linux x86-64 da Forja (M5 P28b-1).

Objetivo: tres garantias que o harness de execucao (verificar_linux.py) nao enxerga.
  1. SYSL_* de rt_syscall_linux.h = a LISTA FECHADA de syscalls (D-M5-3 com as emendas de 14/11/2026): nenhum numero a mais, a menos ou trocado; e todo SYSL_* usado nos .c do runtime
     esta definido (header ou o proprio .c).
  2. Nenhum "/proc" nos ELF gerados (o runtime nao le /proc; o servidor de producao pode nao te-lo): compila uma amostra (ola + as fixtures es\\) em dev e release e procura os bytes.
  3. nm -u dos objetos do runtime: rt_linux_es.o so com sys_heap_* e rt_saida_*; rt_linux_dev.o e rt_linux_rel.o so com forja_programa, sys_heap_*, rt_fmt_fixo e forja_curto_texto_f64 (o texto do R0002, do curto);
     nenhum simbolo __imp_ (nenhum import do Windows).

Uso:   python -I auditar_runtime.py [--forja <forja.exe>] [--objetos <pasta dos .o>]     (sem --forja a etapa 2 e PULADA e dita)
       python -I auditar_runtime.py --selftest
Saida: uma linha por verificacao ("ok"/"FALHA") e "auditar_runtime: <n> falha(s)"; exit 0 so sem falhas. Escrita: so %TEMP% (os ELF da amostra). Reversao: apagar este arquivo.

Versao: 0.7.6
Changelog: 0.7.6 (19/11/2026) D-G3-L2b - lista fechada ganha sysinfo 99 (std:os.mem_available) e flock 73 (std:file.lock); rt_linux_es.o pode deixar rt_os_ultimo_erro indefinido (o errno do std:os.last_error, definido em rt_os_linux.c); kill 62, wait4 61, nanosleep 35, dup2 33, unlink 87 e clock_gettime 228 ja estavam.
           0.7.5 (19/11/2026) M8 P1 - lista fechada ganha gettid 186 (id da thread do perfil, rt_perfil_linux.c); rt_linux_perfil.o entra em OBJETOS_LINUX.
           0.7.4 (19/11/2026) M8 K1 (D-M8-8) - lista fechada ganha getpid 39 (std:os.pid) e sched_getaffinity 204 (std:os.cpu_count), emenda do D-M5-3 como getrusage/setrlimit.
           0.7.3 (18/11/2026) R9 - lista fechada ganha as syscalls de TCP do std:net (poll 7, connect 42, listen 50, getsockopt 55, fcntl 72, accept4 288).
           0.7.2 (16/11/2026) Ryu P1 - o auditor estava velho desde o M6b/M6d: forja_rastro_tabela/forja_rastro_texto/forja_libmem_tabela e malloc/calloc/realloc/free/strdup entram em SIMBOLOS_VARIANTE; o teste do /proc ignora "/processo/" (caminho gravado no DWARF do ELF dev).
           0.7.1 (15/11/2026) M5c-b P63 - rt_linux_es.o pode deixar sys_ambiente_travar e sys_ambiente_destravar indefinidos (a trava do ambiente, D-R7, mora em rt_proc_linux.c, da variante).
           0.7.0 (15/11/2026) M5c-b P59/P60 - lista fechada ganha mprotect 10 (std:mem VirtualArena) e as syscalls de UDP do std:net (socket 41, sendto 44, recvfrom 45, bind 49, getsockname 51, setsockopt 54); rt_linux_varena.o e rt_linux_net.o entram em OBJETOS_LINUX (D0654: sem memcpy sem prefixo).
           0.6.0 (15/11/2026) M5c-b P57 - lista fechada ganha futex 202 (a trava entre threads); rt_linux_es.o e rt_linux_comum.o podem referenciar sys_trava_* e rt_multithread; fontes do runtime sem clone, arch_prctl nem TLS por fs (rt_fio_linux.c e rt_sys_linux.c entram em FontesDoLinux por glob).
           0.5.0 (15/11/2026) M5b P39-B2 (D0654) - rt_linux_entrada_libc.o entra na lista; sys_sair (indefinido em rt_linux_dev/rel, definido pelo objeto de entrada) entra em SIMBOLOS_VARIANTE.
           0.4.0 (15/11/2026) M5b P39-B1 (D0654) - nenhum .o linux do runtime tem memcpy, memset, memmove, memcmp ou strlen (definido ou indefinido); rt_linux_dev.o e rt_linux_rel.o definem os 5 forja_*.
           0.3.0 (14/11/2026) M5 P28b-2b - lista fechada ganha getrusage 98 (std:os peak_memory) e setrlimit 160 (Command.mem_limit_bytes).
           0.2.0 (14/11/2026) M5 P28b-2 - rt_linux_es.o pode deixar sys_ambiente_vetor indefinido (a variante o define); vazamento.fj fica fora da amostra do /proc (o script do filho o cita de proposito); lista fechada ganha dup2 33, fork 57, execve 59, wait4 61, kill 62, chdir 80, setpgid 109 e pipe2 293; a amostra do /proc inclui as fixtures de processo\\.
           0.1.0 (14/11/2026) M5 P28b-1 - primeira versao.
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
RUNTIME = LANGUAGE / "runtime" / "nativo"
OBJETOS_PADRAO = LANGUAGE / "compiler"
TEMPO_BUILD = 120

# A lista fechada: nome SYSL_ -> numero da syscall (x86-64). D-M5-3 + emendas (nanosleep; P28b-1: lseek, fsync, getcwd, rename, mkdir, rmdir, unlink, getdents64, newfstatat, getrandom).
LISTA_FECHADA = {
    "READ": 0, "WRITE": 1, "CLOSE": 3, "FSTAT": 5, "LSEEK": 8, "MMAP": 9, "MUNMAP": 11, "RT_SIGACTION": 13, "RT_SIGPROCMASK": 14, "IOCTL": 16, "MREMAP": 25, "DUP2": 33, "NANOSLEEP": 35,
    "FORK": 57, "EXECVE": 59, "WAIT4": 61, "KILL": 62, "FSYNC": 74, "GETCWD": 79, "CHDIR": 80, "RENAME": 82, "MKDIR": 83, "RMDIR": 84, "UNLINK": 87, "GETRUSAGE": 98, "SYSINFO": 99, "FLOCK": 73, "GETPID": 39, "SCHED_GETAFFINITY": 204, "GETTID": 186, "SETPGID": 109, "SIGALTSTACK": 131, "SETRLIMIT": 160,
    "FUTEX": 202, "CLOCK_GETTIME": 228,
    "MPROTECT": 10, "SOCKET": 41, "SENDTO": 44, "RECVFROM": 45, "BIND": 49, "GETSOCKNAME": 51, "SETSOCKOPT": 54, "GETDENTS64": 217,
    "POLL": 7, "CONNECT": 42, "LISTEN": 50, "GETSOCKOPT": 55, "FCNTL": 72, "ACCEPT4": 288, "EXIT_GROUP": 231, "OPENAT": 257, "NEWFSTATAT": 262, "PIPE2": 293, "GETRANDOM": 318,
}
# SYSL_* do header que NAO sao numero de syscall.
CONSTANTES_DO_HEADER = {"EINTR", "ERRO_MAXIMO", "PROT_LER_ESCREVER", "MAP_ANONIMO_PRIVADO"}
PREFIXOS_ES = ("sys_heap_", "rt_saida_", "sys_trava_", "rt_multithread")  # P57: as travas de forja_rt_es.h/stdin/processo vem de rt_sys_linux.c (variante)
SIMBOLOS_ES = {"sys_ambiente_vetor", "sys_ambiente_travar", "sys_ambiente_destravar", "rt_os_ultimo_erro"}  # L2b: o errno de std:os.last_error mora em rt_os_linux.c (variante) e o es o grava;  # P28b-2: o vetor vivo do ambiente mora em rt_proc_linux.c (variante); o execve do filho o usa. P63: a trava do ambiente (D-R7) tambem
PREFIXOS_VARIANTE = ("sys_heap_",)
# P57: o NOSSO runtime nao cria thread nem toca o TLS: clone, arch_prctl e fs: ficam proibidos nos fontes do runtime Linux (o musl faz o dele). /proc e conferido nos ELF.
PROIBIDOS_NO_FONTE = ("SYSL_CLONE", "arch_prctl", "%fs:", "%%fs:", "SYSL_ARCH_PRCTL")
EXCECOES_PROC = {"vazamento.fj"}  # o proprio teste nomeia /proc no script que o FILHO roda (mede fds e zumbis do pai); o runtime nao o le
SIMBOLOS_VARIANTE = {"forja_programa", "rt_fmt_fixo", "forja_curto_texto_f64", "sys_sair",  # P4 d2fixed: rt_fmt_fixo (rt_linux_comum.o) e o Display curto (rt_linux_curto.o) no lugar do Ryu d2fixed/d2exp
                     "forja_rastro_tabela", "forja_rastro_texto", "forja_libmem_tabela",  # 0.7.2: tabelas que o compilador emite junto do programa (M6b rastreio, M6d libmem), como forja_programa
                     "malloc", "calloc", "realloc", "free", "strdup"}  # 0.7.2: invólucros do libmem do M6d; so a variante libc os resolve (o ELF cru nao os alcanca)  # sys_sair (P39-B2): o objeto de entrada (cru ou libc) o define
NOMES_MEM = ("memcpy", "memset", "memmove", "memcmp", "strlen")  # D0654: no alvo linux o runtime so exporta a versao forja_*
NOMES_MEM_FORJA = tuple("forja_" + N for N in NOMES_MEM)
OBJETOS_LINUX = ("rt_linux_dev.o", "rt_linux_rel.o", "rt_linux_comum.o", "rt_linux_es.o", "rt_linux_entrada_cru.o", "rt_linux_entrada_libc.o", "rt_linux_curto.o", "rt_linux_varena.o", "rt_linux_net.o", "rt_linux_perfil.o")
AMOSTRA = ("examples/01_ola.fj",)  # relativo a pasta-mae de language\; as fixtures es\ entram por glob

# 0.7.2: "/proc" so conta quando nao e prefixo de outro nome (o DWARF do ELF dev grava o caminho da fixture, ex.: ".../linux/processo/env_cwd.fj")
PADRAO_PROC = re.compile(rb"/proc(?![A-Za-z0-9_])")
PADRAO_DEFINE = re.compile(r"^#define\s+SYSL_([A-Z0-9_]+)\s+\(?(-?\d+)LL\)?", re.MULTILINE)
PADRAO_USO = re.compile(r"\bSYSL_([A-Z0-9_]+)\b")
PADRAO_DEFINE_QUALQUER = re.compile(r"^#define\s+SYSL_([A-Z0-9_]+)\b", re.MULTILINE)


def LerSyscallsDoHeader(Texto: str) -> dict:
    """{nome: numero} dos #define SYSL_<NOME> <n>LL que sao syscalls (fora as constantes do header)."""
    return {Nome: int(Valor) for Nome, Valor in PADRAO_DEFINE.findall(Texto) if Nome not in CONSTANTES_DO_HEADER and not Nome.startswith(("EINTR", "ERRO"))}


def ConferirListaFechada(Texto: str) -> list:
    Lido = LerSyscallsDoHeader(Texto)
    Problemas = [f"syscall fora da lista fechada: SYSL_{N} = {V}" for N, V in sorted(Lido.items()) if N not in LISTA_FECHADA]
    Problemas += [f"syscall da lista fechada ausente: SYSL_{N}" for N in sorted(LISTA_FECHADA) if N not in Lido]
    Problemas += [f"numero trocado: SYSL_{N} = {Lido[N]} (esperado {V})" for N, V in sorted(LISTA_FECHADA.items()) if N in Lido and Lido[N] != V]
    return Problemas


def ConferirUsosDefinidos(Header: str, Fontes: dict) -> list:
    """Todo SYSL_<X> usado num .c do runtime Linux esta definido no header ou em algum .c do runtime Linux (rt_sigreturn, 15, e asm e nao entra)."""
    Definidos = set(PADRAO_DEFINE_QUALQUER.findall(Header))
    for Texto in Fontes.values():
        Definidos |= set(PADRAO_DEFINE_QUALQUER.findall(Texto))
    Problemas = []
    for Nome, Texto in Fontes.items():
        Usados = set(PADRAO_USO.findall(Texto))
        Problemas += [f"{Nome}: SYSL_{U} usado e nao definido" for U in sorted(Usados - Definidos)]
    return Problemas


def ConferirSemClone(Fontes: dict) -> list:
    """P57: nenhum fonte do runtime Linux usa clone, arch_prctl nem TLS por fs (os comentarios que os citam como proibidos nao contam: so linhas de codigo)."""
    Problemas = []
    for Nome, Texto in Fontes.items():
        for Numero, Linha in enumerate(Texto.splitlines(), 1):
            Codigo = Linha.split("//")[0]
            if Linha.lstrip().startswith(("*", "/*")):
                continue
            Problemas += [f"{Nome}:{Numero}: {P} no codigo" for P in PROIBIDOS_NO_FONTE if P in Codigo]
    return Problemas


def SimbolosIndefinidos(Objeto: Path) -> list:
    Saida = subprocess.run(["nm", "-u", str(Objeto)], capture_output=True, text=True, timeout=60).stdout
    return [Linha.split()[-1] for Linha in Saida.splitlines() if Linha.strip()]


def ConferirObjeto(Objeto: Path, Prefixos: tuple, Exatos: set) -> list:
    if not Objeto.is_file():
        return [f"{Objeto.name}: nao existe (rode construir.bat)"]
    Indefinidos = SimbolosIndefinidos(Objeto)
    Ruins = [S for S in Indefinidos if S.startswith("__imp_") or not (S.startswith(Prefixos) or S in Exatos)]
    return [f"{Objeto.name}: simbolo indefinido nao permitido: {S}" for S in Ruins]


def SimbolosDoObjeto(Objeto: Path) -> set:
    """Nomes de todos os simbolos (definidos e indefinidos) de um .o; o ultimo campo de cada linha do nm."""
    Saida = subprocess.run(["nm", str(Objeto)], capture_output=True, text=True, timeout=60).stdout
    return {Campos[-1] for Campos in (Linha.split() for Linha in Saida.splitlines()) if Campos}


def ConferirNomesMem(Objeto: Path, Definir: bool) -> list:
    """D0654: nenhum dos 5 nomes sem prefixo no .o linux; com Definir, os 5 forja_* tem de estar la (dev e rel)."""
    if not Objeto.is_file():
        return [f"{Objeto.name}: nao existe (rode construir.bat)"]
    Simbolos = SimbolosDoObjeto(Objeto)
    Problemas = [f"{Objeto.name}: nome sem prefixo presente: {N}" for N in NOMES_MEM if N in Simbolos]
    if Definir:
        Problemas += [f"{Objeto.name}: {F} ausente" for F in NOMES_MEM_FORJA if F not in Simbolos]
    return Problemas


def FontesDoLinux() -> dict:
    Nomes = [P for P in sorted(RUNTIME.glob("*.c")) if P.name.endswith("_linux.c") or P.name in ("rt_linux.c", "rt_linux_es.c", "rt_comum_linux.c")]
    return {P.name: P.read_text(encoding="utf-8") for P in Nomes}


def AmostraDeFontes() -> list:
    Fontes = [LANGUAGE.parent / Parte for Parte in AMOSTRA]
    for Pasta in ("es", "processo"):
        Fontes += [P for P in sorted((AQUI / Pasta).glob("*.fj")) if "proc main(" in P.read_text(encoding="utf-8") and P.name not in EXCECOES_PROC]
    return [P for P in Fontes if P.is_file()]


def ConferirSemProc(Forja: Path) -> list:
    """Compila a amostra em dev e release para x86_64-linux e procura b"/proc" nos bytes de cada ELF."""
    Problemas = []
    with tempfile.TemporaryDirectory(prefix="forja_auditar_") as Temporaria:
        for Fonte in AmostraDeFontes():
            for Modo in ("dev", "release"):
                Saida = Path(Temporaria) / f"{Fonte.stem}_{Modo}.elf"
                Comando = [str(Forja), "build", "--target", "x86_64-linux"] + (["--release"] if Modo == "release" else []) + [str(Fonte), "-o", str(Saida)]
                Processo = subprocess.run(Comando, capture_output=True, timeout=TEMPO_BUILD, stdin=subprocess.DEVNULL)
                if Processo.returncode != 0 or not Saida.is_file():
                    Problemas.append(f"{Fonte.name} {Modo}: build falhou (exit {Processo.returncode}): {(Processo.stdout + Processo.stderr)[:200]!r}")
                elif PADRAO_PROC.search(Saida.read_bytes()):
                    Problemas.append(f"{Fonte.name} {Modo}: o ELF contem \"/proc\"")
    return Problemas


def Relatar(Rotulo: str, Problemas: list) -> int:
    print(f"{Rotulo}: {'ok' if not Problemas else 'FALHA'}")
    for Problema in Problemas:
        print(f"  - {Problema}")
    return len(Problemas)


def Auditar(Argumentos) -> int:
    Header = (RUNTIME / "rt_syscall_linux.h").read_text(encoding="utf-8")
    Pasta = Path(Argumentos.objetos)
    Falhas = Relatar("lista fechada de syscalls (rt_syscall_linux.h)", ConferirListaFechada(Header))
    Falhas += Relatar("todo SYSL_* usado esta definido", ConferirUsosDefinidos(Header, FontesDoLinux()))
    Falhas += Relatar("sem clone, arch_prctl nem TLS por fs nos fontes do runtime (P57)", ConferirSemClone(FontesDoLinux()))
    Falhas += Relatar("nm -u rt_linux_es.o (so sys_heap_*, rt_saida_*, sys_trava_*, rt_multithread e sys_ambiente_*)",ConferirObjeto(Pasta / "rt_linux_es.o", PREFIXOS_ES, SIMBOLOS_ES))
    for Nome in ("rt_linux_dev.o", "rt_linux_rel.o"):
        Falhas += Relatar(f"nm -u {Nome} (forja_programa, sys_heap_*, d2*)", ConferirObjeto(Pasta / Nome, PREFIXOS_VARIANTE, SIMBOLOS_VARIANTE))
    for Nome in OBJETOS_LINUX:
        Falhas += Relatar(f"memcpy/memset/memmove/memcmp/strlen sem prefixo fora de {Nome} (D0654)", ConferirNomesMem(Pasta / Nome, Nome in ("rt_linux_dev.o", "rt_linux_rel.o")))
    if Argumentos.forja:
        Falhas += Relatar("nenhum /proc nos ELF da amostra (ola + es\\, dev e release)", ConferirSemProc(Path(Argumentos.forja).resolve()))
    else:
        print("nenhum /proc nos ELF: PULADA (sem --forja)")
    print(f"auditar_runtime: {Falhas} falha(s)")
    return 0 if Falhas == 0 else 1


def Selftest() -> int:
    Falhas = []
    Bom = "\n".join(f"#define SYSL_{N} {V}LL" for N, V in LISTA_FECHADA.items()) + "\n#define SYSL_EINTR (-4LL)\n#define SYSL_ERRO_MAXIMO 4095ULL\n"
    if ConferirListaFechada(Bom):
        Falhas.append("lista boa reprovada")
    if not ConferirListaFechada(Bom + "#define SYSL_EXTRA 999LL\n"):
        Falhas.append("syscall extra nao detectada")
    if not ConferirListaFechada(Bom.replace("SYSL_MKDIR 83LL", "SYSL_MKDIR 84LL")):
        Falhas.append("numero trocado nao detectado")
    if not ConferirListaFechada(Bom.replace("#define SYSL_UNLINK 87LL\n", "")):
        Falhas.append("syscall ausente nao detectada")
    if not ConferirUsosDefinidos(Bom, {"x.c": "sysl_chamar1(SYSL_NAO_EXISTE, 1)"}) or ConferirUsosDefinidos(Bom, {"x.c": "sysl_chamar1(SYSL_READ, 1)"}):
        Falhas.append("uso de SYSL_ nao definido")
    Real = ConferirListaFechada((RUNTIME / "rt_syscall_linux.h").read_text(encoding="utf-8"))
    if Real:
        Falhas.append(f"header real fora da lista: {Real}")
    print("auditar_runtime selftest:", "VERDE" if not Falhas else f"FALHOU {Falhas}")
    return 1 if Falhas else 0


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Audita o runtime Linux da Forja (syscalls, /proc, simbolos indefinidos).")
    Analisador.add_argument("--forja", help="forja.exe (de preferencia uma copia) para a etapa do /proc")
    Analisador.add_argument("--objetos", default=str(OBJETOS_PADRAO), help="pasta dos rt_linux_*.o (padrao: compiler\\)")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    return Selftest() if Argumentos.selftest else Auditar(Argumentos)


if __name__ == "__main__":
    sys.exit(Principal())
