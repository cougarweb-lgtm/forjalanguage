#!/usr/bin/env python3
r"""construir_vendor_linux.py - compila no alpine-build (WSL) as libs estaticas Linux (.a) que as pontes vendor pedem (link "z", "sqlite3", "cjson", "cgltf") e que tem fonte C portavel no repo.

Objetivo: P40 pista L (contrato_m5_resto). Saida: language\third\linux\x86_64\<pasta>\lib<link>.a + LEIAME.txt + SHA256SUMS.
Nao grava em vendor\, compiler\ nem runtime\. Fontes lidos so por leitura (sha256 conferido antes de compilar).
Escolha de flags: -fno-pie (ELF estatico, codigo nao-PIC; relocacoes ficam no censo ABS/PC32/PLT32/32S/GOTPCREL*);
-fno-asynchronous-unwind-tables e -fno-stack-protector (contrato). Sem -fPIC: o alvo linux e ELF estatico.
Uso: python -I construir_vendor_linux.py [--leitor <teste_elf_ler.exe>]
     python -I construir_vendor_linux.py --selftest
Versao: 0.1.2
Changelog:
  0.1.2 (20/11/2026) PREPARO F5: zlib entra com vendor\compression\zlib_ponte.c (cola de fluxo zlibf_*, -I do zlib-1.4 e do zlib.h); --so <link> reconstroi so uma lib.
  0.1.1 (14/11/2026) SHA256SUMS com LF puro (write_text no Windows gravava CRLF e o sha256sum -c do Linux recusava).
  0.1.0 (14/11/2026) P40 pista L: zlib 1.4, sqlite3 3.54.1, cJSON 1.7.19, cgltf 1.15; auditoria de relocacao (censo) e de indefinidos (libc/libm/libgcc).
Depende de: wsl.exe com a distro alpine-build (gcc, ar, readelf, nm, tar); comparar_readelf.py e teste_elf_ler.exe (censo pelo leitor; opcional).
"""
import hashlib
import re
import subprocess
import sys
from datetime import date
from pathlib import Path

DISTRO = "alpine-build"
PASTA_TESTE = Path(__file__).resolve().parent
RAIZ = Path(__file__).resolve().parents[4]
VENDOR = RAIZ / "vendor"
SAIDA_RAIZ = RAIZ / "third" / "linux" / "x86_64"
COMPARADOR = PASTA_TESTE.parent / "elf_ler" / "comparar_readelf.py"
FLAGS_BASE = ["-O2", "-fno-pie", "-fno-asynchronous-unwind-tables", "-fno-stack-protector"]
CENSO = {"R_X86_64_64", "R_X86_64_PC32", "R_X86_64_PLT32", "R_X86_64_32", "R_X86_64_32S",
         "R_X86_64_GOTPCREL", "R_X86_64_GOTPCRELX", "R_X86_64_REX_GOTPCRELX"}
PADRAO_RELOC = re.compile(r"R_X86_64_\w+")
ZLIB_ARQUIVOS = ["adler32", "compress", "crc32", "deflate", "gzclose", "gzlib", "gzread", "gzwrite",
                 "infback", "inffast", "inflate", "inftrees", "trees", "uncompr", "zutil"]

LIBS = [
    {"link": "z", "pasta": "zlib", "nome": "zlib", "versao": "1.4", "licenca": "zlib License",
     "atualizacao": "2026-11-05 (release v1.4)", "substituta": "N/A",
     "fonte": "https://github.com/madler/zlib (tarball v1.4)", "origem_repo": "vendor\\compression\\zlib\\zlib-1.4.tar.gz",
     "entrada": [("compression/zlib/zlib-1.4.tar.gz", "4331355048e286908260fdbeaa0b8cf75ceaf2907c75855a093a6487c7032aaa"),
                 ("compression/zlib_ponte.c", "dcab601bf02c68dd646db0c522541ddcb871ee837588a25139852468ab61045b")],
     "tar": "{vendor}/compression/zlib/zlib-1.4.tar.gz",
     "defines": ["-DHAVE_UNISTD_H"],  # o configure do zlib define isso no POSIX; sem ele gzlib.c usa lseek implicito
     "includes": ["{trab}/zlib-1.4"],  # zlib.h e zconf.h da propria versao do tarball (zlib_ponte.c inclui "zlib.h")
     "arquivos_c": ["{trab}/zlib-1.4/" + N + ".c" for N in ZLIB_ARQUIVOS] + ["{vendor}/compression/zlib_ponte.c"]},
    {"link": "sqlite3", "pasta": "sqlite3", "nome": "SQLite (amalgamation)", "versao": "3.54.1",
     "licenca": "dominio publico", "atualizacao": "2026 (3.54.1; dia exato nao registrado no dependencia.json)",
     "substituta": "N/A", "fonte": "https://www.sqlite.org/ (sqlite-amalgamation-3540100)",
     "origem_repo": "vendor\\db\\sqlite\\sqlite3.c",
     "entrada": [("db/sqlite/sqlite3.c", "0c657666d819f261cc4007526ad57e1669de9949df7a8ea847aa284a2bf0ee52"),
                 ("db/sqlite/sqlite3.h", "d6c84557261893439b341d2e6c35187ae3ea24fbfd843ad94616f72d89242b8c")],
     "tar": None, "defines": ["-DSQLITE_THREADSAFE=1", "-DSQLITE_OMIT_LOAD_EXTENSION", "-DSQLITE_ENABLE_FTS5"],
     "arquivos_c": ["{vendor}/db/sqlite/sqlite3.c"]},
    {"link": "cjson", "pasta": "cjson", "nome": "cJSON", "versao": "1.7.19", "licenca": "MIT",
     "atualizacao": "2025-09-09 (release v1.7.19); ultimo commit 2026-09-16", "substituta": "N/A",
     "fonte": "https://github.com/DaveGamble/cJSON (tarball v1.7.19)", "origem_repo": "vendor\\json\\cjson\\cJSON.c",
     "entrada": [("json/cjson/cJSON.c", "298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d"),
                 ("json/cjson/cJSON.h", "25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee")],
     "tar": None, "defines": [], "arquivos_c": ["{vendor}/json/cjson/cJSON.c"]},
    {"link": "cgltf", "pasta": "cgltf", "nome": "cgltf", "versao": "1.15", "licenca": "MIT",
     "atualizacao": "2026-02-02 (ultimo commit; v1.15 e a ultima release)", "substituta": "N/A",
     "fonte": "https://github.com/jkuhlmann/cgltf (v1.15)", "origem_repo": "vendor\\gltf\\cgltf_impl.c + cgltf\\cgltf.h",
     "entrada": [("gltf/cgltf/cgltf.h", "e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec"),
                 ("gltf/cgltf_impl.c", None)],
     "tar": None, "defines": ["-DNDEBUG"], "arquivos_c": ["{vendor}/gltf/cgltf_impl.c"]},
    {"link": "utf8proc", "pasta": "utf8proc", "nome": "utf8proc", "versao": "2.12.0", "licenca": "MIT",
     "atualizacao": "2026-09-25 (release v2.12.0; Unicode 18.0.0)", "substituta": "N/A",
     "fonte": "https://github.com/JuliaStrings/utf8proc (tarball v2.12.0)", "origem_repo": "vendor\\unicode\\utf8proc\\utf8proc.c + utf8proc_data.c + utf8proc.h",
     "entrada": [("unicode/utf8proc/utf8proc.c", "f38438eb986ba7eedad0f2e820a70fae1aaf05197364ba0381a5e84775099ef8"),
                 ("unicode/utf8proc/utf8proc_data.c", "b4911d4162d216afa612317a9b981851e15d672f4d93b25f3a1eb986f8c0aedb"),
                 ("unicode/utf8proc/utf8proc.h", "08da85e820efaa3613500946c26b2dc427a07f76001ed7c16bc90f298b814c35")],
     "tar": None, "defines": ["-DUTF8PROC_STATIC"], "arquivos_c": ["{vendor}/unicode/utf8proc/utf8proc.c"]},
    # MARCADOR-VENDOR-UTF8PROC (cada agente troca SO o proprio marcador pela sua entrada + o marcador de volta)
    {"link": "unibreak", "pasta": "libunibreak", "nome": "libunibreak", "versao": "8.0.1", "licenca": "zlib License",
     "atualizacao": "2026-11-16 (release libunibreak_8_0_1; Unicode 17.0.0)", "substituta": "N/A",
     "fonte": "https://github.com/adah1972/libunibreak (tarball libunibreak_8_0_1)", "origem_repo": "vendor\\linebreak\\libunibreak\\*.c + *.h",
     "entrada": [("linebreak/libunibreak/linebreak.c", "30c4cc858d9feb354f62de44b29ac89c397bdfc7dddb25bb46f0f558f286862a"),
                 ("linebreak/libunibreak/linebreakdata.c", "d2a1008ad93a15c15389e5cdd3c2d21b5e03820ed9daeee5a5a902ef56649719"),
                 ("linebreak/libunibreak/linebreakdef.c", "105b926c59ed3b1a87e2c7a19ba68adaec69032b501d0c61d9a7043f534bd62b"),
                 ("linebreak/libunibreak/wordbreak.c", "f6f89884aa0357ee230bbc429074ea09b3ea137783af12bce0f0a400dfd63a4d"),
                 ("linebreak/libunibreak/graphemebreak.c", "dc1101bb96af875571ab05f91e94d1772f741c65ac02df553f257fdfb0e74804"),
                 ("linebreak/libunibreak/eastasianwidthdef.c", "f609d5915e4cf7572765e66aaa8bda6d315cc8ec89e01085c92683a2b1edd42d"),
                 ("linebreak/libunibreak/emojidef.c", "4baf8f003201d49cbf78d58c0938eb28e19121095314724e8dedbb610bae327f"),
                 ("linebreak/libunibreak/unibreakbase.c", "aa44089bcde4bf6cedb387af7bed3ba58d02d3a2c1202b306812a0be74bc0834"),
                 ("linebreak/libunibreak/unibreakdef.c", "9d0f2a973ee07e748091ec18e09131ab7109899ae15b17fa769c93794a7fd58e"),
                 ("linebreak/libunibreak/linebreakauxdata.c", "09b9327653433d22cc1c3ff4cbdd176abe577286365275cc70617a79adea29db"),
                 ("linebreak/libunibreak/wordbreakdata.c", "8733e29abafb9208f0070a2a549b13533c97432b7809b51a121a4c46849e6950"),
                 ("linebreak/libunibreak/graphemebreakdata.c", "fbab281189b6a8a50d96c3361f2b9f22a37c92624ca6f4060edff7d169a20979"),
                 ("linebreak/libunibreak/indicconjunctbreakdata.c", "c69d1b5119fe53323e36913534378ea2ef91e0aa7839719cb5cf1c18e4c5250d"),
                 ("linebreak/libunibreak/eastasianwidthdata.c", "cfdc4c2abb651092cfa92fa8004a8dc7ae1e82841912720cb2b627b02534fab7"),
                 ("linebreak/libunibreak/emojidata.c", "b33ed203eb6bacfc7c9598a726a41c64ab67032b767bc676aecc430a207faeb6")],
     "tar": None, "defines": ["-DNDEBUG"],  # RELFLAGS do Makefile.gcc da release (sem assert no .a)
     "arquivos_c": ["{vendor}/linebreak/libunibreak/" + N + ".c" for N in ["linebreak", "linebreakdata", "linebreakdef", "wordbreak",
                                                                             "graphemebreak", "eastasianwidthdef", "emojidef", "unibreakbase", "unibreakdef"]]},
    # MARCADOR-VENDOR-UNIBREAK
    {"link": "stbi", "pasta": "stb_image", "nome": "stb_image", "versao": "2.34", "licenca": "MIT OU dominio publico (escolha do autor)",
     "atualizacao": "2026-11-19 (ultimo commit de stb_image.h no master, 1da9ade)", "substituta": "N/A",
     "fonte": "https://github.com/nothings/stb (stb_image.h, master)", "origem_repo": "vendor\\image\\stb_image\\stbi_impl.c + stb_image.h",
     "entrada": [("image/stb_image/stb_image.h", "dfc2aadda6295f67f677587773f25e5bf8c8ad427ed0db7c518525c91e72e020"),
                 ("image/stb_image/stbi_impl.c", "b7a7e0a77f6b6c4e47273c17d05257f60196e4e97f0ad2b3582b954372a4899c")],
     "tar": None, "defines": [], "arquivos_c": ["{vendor}/image/stb_image/stbi_impl.c"]},
    # MARCADOR-VENDOR-STBI
    {"link": "lexbor", "pasta": "lexbor", "nome": "lexbor", "versao": "3.0.0", "licenca": "Apache-2.0",
     "atualizacao": "v3.0.0 de 2026-03-31 (release; tarball conferido em 2026-11-20)", "substituta": "N/A",
     "fonte": "https://github.com/lexbor/lexbor/archive/refs/tags/v3.0.0.tar.gz", "origem_repo": "vendor\\html\\lexbor\\ + vendor\\html\\lexborf.c",
     "entrada": [("html/lexborf.c", "b09e8b7b734e1a838d3bd0f787177d47876045c41c214874cb0865df527208e2"),
                 ("html/lexbor/LICENSE", "7321caa1f366dfbebf799b6c6c2604772dbb12ef10ed6a6b7cbb384b3401c4dd")],
     "tar": None, "defines": ["-DLEXBOR_STATIC", "-D_DEFAULT_SOURCE", "-D_POSIX_C_SOURCE=199309L"],
     "includes": ["{vendor}/html/lexbor/source", "{vendor}/html"],
     # lista de fontes (201 .c: modulos do contrato + punycode + port posix + cola) no arquivo ao lado; sem o port windows_nt
     "arquivos_c": [Linha.strip() for Linha in open(Path(__file__).resolve().parents[4] / "vendor" / "html" / "lexbor" / "fontes_c.txt", encoding="utf-8") if Linha.strip()]},
    # MARCADOR-VENDOR-LEXBOR
    {"link": "harfbuzz", "pasta": "shaping", "nome": "HarfBuzz (subset amalgamado + ponte hbf)", "versao": "15.1.0", "licenca": "Old MIT (ver COPYING no tarball e em vendor\\shaping\\harfbuzz\\COPYING)",
     "atualizacao": "2026-11-20 (release 15.1.0; existe 15.2.0, nao usada)", "substituta": "N/A",
     "fonte": "https://github.com/harfbuzz/harfbuzz/releases (harfbuzz-15.1.0.tar.xz, sha256 0fc7c565443a5f4be7da65ac5ee5bd737d9b158dc908708b7689a0a5796b0c8)",
     "origem_repo": "vendor\\shaping\\harfbuzz\\harfbuzz-15.1.0-src.tar.xz (podado) + hbf.c + hbf.h",
     "entrada": [("shaping/harfbuzz/harfbuzz-15.1.0-src.tar.xz", "1546c91d3345cca65d25ff021450ea1514055fc790f16dbfbc40c3e3d14f5206"),
                 ("shaping/harfbuzz/hbf.c", "e209a16c4ed73b6c0fa535203f7b8a34f3d3a75b6ada25fc0bb5aef8e48e1ca0"),
                 ("shaping/harfbuzz/hbf.h", "6a6699d84241705bc9d7afa5d3eb2c2dcb3f615ff9355b86397b65c4d1b6673b")],
     "tar": "{vendor}/shaping/harfbuzz/harfbuzz-15.1.0-src.tar.xz",
     # R0009 (dev): o atexit do HB cai em hbf_atexit (hbf.c guarda; hbf_shutdown chama). -DHBF_AMALGAMA + -include hbf.h fazem
     # o #define atexit hbf_atexit depois de <cstdlib> (a libstdc++ desfaz um -Datexit). "-include" e "hbf.h" sao itens separados.
     "defines": ["-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics", "-DHB_NO_MT", "-DNDEBUG", "-DHB_NO_MMAP", "-DHB_NO_OPEN", "-DHB_NO_BUFFER_MESSAGE",
                 "-DHAVE_ATEXIT", "-DHB_USE_ATEXIT=1", "-DHBF_AMALGAMA", "-include", "hbf.h"],
     "includes": ["{trab}/harfbuzz-15.1.0/src", "{vendor}/shaping/harfbuzz"],
     # so o subset amalgamado: ele ja inclui o shaping (harfbuzz.cc e o mesmo codigo; juntos dao simbolos duplicados). .cc vai pelo gcc (C++).
     "arquivos_c": ["{trab}/harfbuzz-15.1.0/src/harfbuzz-subset.cc", "{vendor}/shaping/harfbuzz/hbf.c"]},
    # MARCADOR-VENDOR-HARFBUZZ
]

NAO_CONSTRUIDOS = [
    ("m", "libm do musl: libm.a do alpine tem 8 bytes (vazio); as funcoes estao na libc.a. Nada a construir."),
    ("box2d", "sem fonte no repo (vendor\\physics2d\\box2d so tem LICENSE e reduzir_cabecalho.py); so o .a Windows."),
    ("raylib", "sem fonte no repo (vendor\\raylib\\raylib so tem LICENSE); so o .a Windows."),
    ("kernel32, msvcrt, user32, ...", "DLL do Windows sem fonte: pulados por desenho."),
]


def Rodar(*Argumentos: str, Limite: int = 900) -> subprocess.CompletedProcess:
    """Comando na distro por LISTA (sem sh -c). Falha de codigo de saida vira RuntimeError com o trecho de erro."""
    Proc = subprocess.run(["wsl", "-d", DISTRO, "--", *Argumentos], capture_output=True,
                          timeout=Limite, stdin=subprocess.DEVNULL)
    if Proc.returncode != 0:
        Erro = Proc.stderr.decode("utf-8", errors="replace").strip()[-400:]
        raise RuntimeError(f"falhou ({Proc.returncode}): {' '.join(Argumentos)[:160]} :: {Erro}")
    return Proc


def Texto(Proc: subprocess.CompletedProcess) -> str:
    return Proc.stdout.decode("utf-8", errors="replace")


def CaminhoWsl(Caminho: Path) -> str:
    Absoluto = str(Caminho.resolve()).replace("\\", "/")
    return "/mnt/" + Absoluto[0].lower() + Absoluto[2:]


def Sha256(Caminho: Path) -> str:
    return hashlib.sha256(Caminho.read_bytes()).hexdigest()


def ConferirEntrada(Lib: dict) -> None:
    """Sha256 dos fontes do repo contra o registrado no dependencia.json; divergencia para o build."""
    for Rel, Esperado in Lib["entrada"]:
        Real = Sha256(VENDOR / Rel)
        if Esperado and Real != Esperado:
            raise RuntimeError(f"sha256 de {Rel} diverge: {Real} != {Esperado}")


def Substituir(Modelo: str, Trab: str) -> str:
    return Modelo.format(trab=Trab, vendor=CaminhoWsl(VENDOR))


def CompilarLib(Lib: dict, Trab: str) -> list:
    """Compila os fontes e empacota lib<link>.a em Trab. Devolve os comandos gcc como texto (para o LEIAME)."""
    Comandos = []
    if Lib["tar"]:
        Rodar("tar", "-xJf" if Lib["tar"].endswith(".tar.xz") else "-xzf", Substituir(Lib["tar"], Trab), "-C", Trab)
    Incs = [Flag for Pasta in Lib.get("includes", []) for Flag in ("-I", Substituir(Pasta, Trab))]
    Objetos = []
    for Indice, Modelo in enumerate(Lib["arquivos_c"], start=1):
        Fonte = Substituir(Modelo, Trab)
        Obj = f"{Trab}/obj{Indice:02d}.o"
        Argumentos = ["gcc", *FLAGS_BASE, *Lib["defines"], *Incs, "-c", Fonte, "-o", Obj]
        Rodar(*Argumentos)
        Comandos.append(" ".join(Argumentos).replace(Trab, "<trab>"))
        Objetos.append(Obj)
    Rodar("ar", "rcsD", f"{Trab}/lib{Lib['link']}.a", *Objetos)
    return Comandos


def ConstruirUma(Lib: dict) -> dict:
    """Confere, compila no alpine e copia o .a para language\\third\\linux\\x86_64\\<pasta>\\."""
    ConferirEntrada(Lib)
    Trab = Texto(Rodar("mktemp", "-d", "/tmp/vendor_a.XXXXXX")).strip()
    try:
        Comandos = CompilarLib(Lib, Trab)
        Destino = SAIDA_RAIZ / Lib["pasta"]
        Destino.mkdir(parents=True, exist_ok=True)
        Arquivo = Destino / f"lib{Lib['link']}.a"
        Rodar("cp", f"{Trab}/lib{Lib['link']}.a", CaminhoWsl(Arquivo))
    finally:
        Rodar("rm", "-rf", Trab)
    return {"lib": Lib, "arquivo": Arquivo, "comandos": Comandos, "tamanho": Arquivo.stat().st_size,
            "sha256": Sha256(Arquivo)}


def ParsearNm(Saida: str) -> tuple:
    """(indefinidos que precisam de simbolo, definidos). Weak undefined ('w') nao conta como exigencia."""
    Indefinidos, Definidos = set(), set()
    for Linha in Saida.splitlines():
        Partes = Linha.split()
        if len(Partes) == 2 and Partes[0] == "U":
            Indefinidos.add(Partes[1])
        elif len(Partes) == 3:
            if Partes[1] == "U":
                Indefinidos.add(Partes[2])
            elif Partes[1] != "w":
                Definidos.add(Partes[2])
    return Indefinidos, Definidos


def TiposDeReloc(Saida: str) -> set:
    return set(PADRAO_RELOC.findall(Saida))


def DefinidosDaLibc() -> set:
    """Simbolos definidos na libc.a, libm.a e libgcc.a do alpine (a base de 'libc/libm/libgcc')."""
    Gcc = Texto(Rodar("gcc", "-print-libgcc-file-name")).strip()
    Proc = Rodar("nm", "--defined-only", "/usr/lib/libc.a", "/usr/lib/libm.a", Gcc, Limite=300)
    return ParsearNm(Texto(Proc))[1]


def AuditarLib(Arquivo: Path, DaLibc: set) -> dict:
    """Censo de relocacao (readelf -rW) e indefinidos que nao caem em libc/libm/libgcc nem no proprio .a."""
    Caminho = CaminhoWsl(Arquivo)
    Tipos = TiposDeReloc(Texto(Rodar("readelf", "-rW", Caminho)))
    Indefinidos, Definidos = ParsearNm(Texto(Rodar("nm", Caminho)))
    Fora = sorted(Indefinidos - Definidos - DaLibc)
    return {"tipos": sorted(Tipos), "censo_fora": sorted(Tipos - CENSO), "indef": len(Indefinidos),
            "fora": Fora}


def LeitorContar(Leitor: Path, Arquivo: Path) -> dict:
    """teste_elf_ler.exe contar <a>: E (recusado pelo leitor, ex.: reloc fora do censo) e TOTAL."""
    Proc = subprocess.run([str(Leitor), "contar", str(Arquivo)], capture_output=True, timeout=300)
    Saida = Proc.stdout.decode("utf-8", errors="replace")
    Total = re.search(r"^TOTAL (\d+) (\d+)", Saida, re.M)
    return {"codigo": Proc.returncode, "erros": int(Total.group(2)) if Total else -1,
            "linhas_E": [L for L in Saida.splitlines() if L.startswith("E ")][:3]}


def TextoLeiame(Dado: dict, Auditoria: dict, Leitura: dict) -> str:
    Lib = Dado["lib"]
    Censo = "OK (so tipos do censo)" if not Auditoria["censo_fora"] else "FORA: " + ", ".join(Auditoria["censo_fora"])
    Indef = "nenhum fora de libc/libm/libgcc" if not Auditoria["fora"] else "FORA: " + ", ".join(Auditoria["fora"])
    Leitor = "nao rodado" if not Leitura else f"contar: {Leitura['erros']} erro(s) do leitor, codigo {Leitura['codigo']}"
    Linhas = [
        "VALIDACAO DE BIBLIOTECA (Regra #8)",
        f"Biblioteca: {Lib['nome']} | Versao: {Lib['versao']} | Status: Ativa",
        f"Ultima atualizacao: {Lib['atualizacao']} | Substituta: {Lib['substituta']}",
        f"Fonte: oficial - {Lib['fonte']}",
        f"Licenca: {Lib['licenca']} | Fonte no repo: {Lib['origem_repo']}",
        "",
        f"Lib para o link: \"{Lib['link']}\" -> arquivo libnome do link: lib{Lib['link']}.a",
        f"Destino proposto para a pista C: vendor\\linux\\lib{Lib['link']}.a (copia; nao criado aqui)",
        f"Gerado em {date.today().isoformat()} no alpine-build (gcc {'15.2.0'}, musl), estatico, -fno-pie.",
        "",
        "Comandos exatos (alvo <trab> = pasta temporaria do WSL, apagada ao fim):",
        *["  " + C for C in Dado["comandos"]],
        "",
        f"sha256 do .a: {Dado['sha256']}  ({Dado['tamanho']} bytes)",
        "sha256 das fontes do repo:",
        *([f"  {Rel}  {Sha}" for Rel, Sha in Lib["entrada"] if Sha] or ["  (ver entrada no dependencia.json da pasta)"]),
        "",
        f"Censo de relocacao (readelf -rW): {Censo}",
        f"Tipos vistos: {', '.join(Auditoria['tipos'])}",
        f"Indefinidos fora da libc: {Indef}",
        f"Leitor (teste_elf_ler): {Leitor}",
        "",
        "Escolhas: -fno-pie (ELF estatico, sem PIC); -O2; sem -fPIC. Build por ar rcsD (archive deterministico).",
    ]
    return "\n".join(Linhas) + "\n"


def GravarLeiame(Dado: dict, Auditoria: dict, Leitura: dict) -> None:
    Pasta = SAIDA_RAIZ / Dado["lib"]["pasta"]
    (Pasta / "LEIAME.txt").write_text(TextoLeiame(Dado, Auditoria, Leitura), encoding="utf-8")
    Arquivo = Dado["arquivo"]
    (Pasta / "SHA256SUMS").write_bytes(f"{Dado['sha256']}  {Arquivo.name}\n".encode("utf-8"))  # LF puro: sha256sum -c do Linux recusa CRLF


def ParaTabela(Dado: dict, Auditoria: dict, Leitura: dict) -> str:
    Censo = "OK" if not Auditoria["censo_fora"] else "FORA"
    Indef = "OK" if not Auditoria["fora"] else "FORA " + str(len(Auditoria["fora"]))
    Lei = "-" if not Leitura else str(Leitura["erros"])
    return (f"{Dado['lib']['link']:<8} sim  {Dado['tamanho']:>9}  {Dado['sha256'][:16]}..  "
            f"censo={Censo}  indef_fora_libc={Indef}  leitor_erros={Lei}")


def ProcessarLibs(Leitor: Path | None, Somente: str | None) -> int:
    DaLibc = DefinidosDaLibc()
    Problemas = 0
    for Lib in LIBS:
        if Somente and Lib["link"] != Somente:
            continue
        Dado = ConstruirUma(Lib)
        Auditoria = AuditarLib(Dado["arquivo"], DaLibc)
        Leitura = LeitorContar(Leitor, Dado["arquivo"]) if Leitor else None
        GravarLeiame(Dado, Auditoria, Leitura)
        print(ParaTabela(Dado, Auditoria, Leitura))
        if Auditoria["censo_fora"] or Auditoria["fora"]:
            Problemas += 1
            print(f"  ATENCAO {Lib['link']}: censo fora={Auditoria['censo_fora']} indef fora={Auditoria['fora'][:10]}")
    return Problemas


def SelfTest() -> int:
    Indef, Def = ParsearNm("\nadler.o:\n                 U memcpy\n0000000000000000 T adler32\n"
                           "                 w __gmon_start__\n0000000000000000 D tabela\n")
    if Indef != {"memcpy"} or Def != {"adler32", "tabela"}:
        print("FALHOU selftest: ParsearNm")
        return 1
    if TiposDeReloc("R_X86_64_PC32 x\nR_X86_64_PLT32 y\n") - CENSO:
        print("FALHOU selftest: censo")
        return 1
    print("PASSOU selftest construir_vendor_linux")
    return 0


def Principal(Argumentos: list) -> int:
    if Argumentos == ["--selftest"]:
        return SelfTest()
    Leitor = None
    Somente = None
    if len(Argumentos) == 2 and Argumentos[0] == "--leitor":
        Leitor = Path(Argumentos[1]).resolve()
    elif len(Argumentos) == 2 and Argumentos[0] == "--so":
        Somente = Argumentos[1]
    elif Argumentos:
        print("uso: python -I construir_vendor_linux.py [--leitor <teste_elf_ler.exe>] | [--so <link>] | --selftest")
        return 2
    Problemas = ProcessarLibs(Leitor, Somente)
    for Nome, Motivo in NAO_CONSTRUIDOS:
        print(f"nao construido: {Nome} - {Motivo}")
    return 1 if Problemas else 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
