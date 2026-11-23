# -*- coding: utf-8 -*-
"""verificar_m6c.py - M6c P81: harness do c_long por alvo (grupo clong) e da posse de handle no foreign (grupo handle).

Versao: 0.5.1
Changelog: 0.5.1 - 0.21.0 F5-a: release-igual compila COM e SEM do mesmo caminho (o caminho absoluto do fonte entra no .rdata com --checagem sim) e confere todos os alvos.
           0.5.0 - 0.21.0 F4 passo 3: sai o backend C do --release-igual; as 2 variantes x64 (windows e linux) comparam COM e SEM a clausula, byte a byte; sai o ruido do gcc/carimbo do PE (era so do backend c).
           0.4.0 - P90: PrepararVendorZlib copia tambem vendor\\db\\sqlite3.fj, vendor\\json\\cjson.fj e libsqlite3.a/libcjson.a (windows e linux) para as fixtures sqlite_*/cjson_takes.
           0.3.0 - P88: os ok_handle_* da semantica conferem por `forja check` (declaram foreign de simbolos que nao existem; o build falharia no ligador). --release-igual (0.1.0) agora forca --backend x64 nas 2 variantes x64
           (o --release padrao e backend c, nao reprodutivel) e, no backend c, descarta como ruido os bytes que mudam entre 3 builds do mesmo fonte (carimbo do PE, nome do .c temporario do gcc).
           0.2.0 - P84: fixture com import vendor: (gzheader_layout, zlib_crc) roda numa pasta de trabalho em %TEMP% com o .fj, vendor\\compression\\{compression,zlib}.fj,
           vendor\\libz.a e vendor\\linux\\libz.a (copia com shutil.copy2, sem juncao); a pasta some com o TemporaryDirectory do Principal.
           0.1.0 - P81: primeira versao. Runtime em dev nos 2 alvos (windows e linux por wsl alpine-build) contra <nome>.esperado (JSON por alvo: exit, stdout, r = R-codes);
           a chave "release" (so windows) confere exit/stdout/r e o simbolo proibido no exe. --release-igual compila cada fixture de handle COM e SEM as clausulas
           (regex propria, TirarClausulas) e compara o exe --release byte a byte em x64 windows, x64 linux e backend c. Semantica: build das err_/ok_ de
           tests\\semantica (clong e handle) nos 2 alvos; o 1o diagnostico JSON bate com <nome>.esperado.jsonl e e igual nos 2 alvos. --selftest so confere o harness.
           (check nao aceita --target: a semantica do alvo linux sai pelo build.)
Uso: python tests\\nativo\\m6c\\verificar_m6c.py --forja <forja.exe> [--alvo windows|linux|ambos] [--grupo clong|handle|tudo] [--repetir N] [--release-igual] [--fixture nome]
     python tests\\nativo\\m6c\\verificar_m6c.py --selftest
Depende de: wsl com a distro alpine-build (so no alvo linux). Saida 0 = tudo PASSOU; 1 = alguma FALHOU; 2 = ferramenta ausente.
Verificacao: python tests\\nativo\\m6c\\verificar_m6c.py --selftest
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PASTA = Path(__file__).resolve().parent
LANGUAGE = PASTA.parent.parent.parent
VENDOR = LANGUAGE / "vendor"
PASTA_CLONG = PASTA / "clong"
PASTA_HANDLE = PASTA / "handle"
PASTA_SEMANTICA = PASTA.parent.parent / "semantica"
WSL_DISTRO = "alpine-build"
TIMEOUT_S = 120
ALVOS = ("windows", "linux")
REPETIR_FIXO = {"handle_threads": 20}

FIXTURES_CLONG = ["labs_alvo", "tamanho_alvo", "strtoul_grande", "gzheader_layout", "zlib_crc"]
FIXTURES_HANDLE = ["handle_esquecido", "handle_ok", "handle_fopen_none", "handle_desconhecido", "handle_threads",
                   "handle_release", "sqlite_esquecido", "sqlite_ok", "cjson_takes"]
# Fixtures de handle com a clausula no proprio .fj (as de sqlite/cjson usam o vendor: fora do --release-igual).
RELEASE_IGUAL_HANDLE = ["handle_esquecido", "handle_ok", "handle_fopen_none", "handle_desconhecido", "handle_threads", "handle_release"]
# 0.4.0 (P90): gemeo com o vendor sqlite3.fj/cjson.fj com e sem clausulas (tiradas por regex na copia).
RELEASE_IGUAL_VENDOR = ["sqlite_ok", "cjson_takes"]
SEMANTICA_CLONG = ["ok_clong_fronteira", "err_clong_mistura", "err_clong_conta", "err_clong_proc_forja",
                   "err_clong_export", "err_clong_array"]
SEMANTICA_HANDLE = ["ok_handle_defer", "ok_handle_saida", "ok_handle_none", "ok_handle_emprestimo",
                    "ok_handle_entrega_campo", "ok_handle_takes", "ok_handle_struct_valor", "err_handle_esquecido",
                    "err_handle_copia", "err_handle_return_vivo", "err_handle_saida_sem_local",
                    "err_handle_proc_forja_fecha", "err_E0912_param_inexistente", "err_E0912_tipo_nao_handle",
                    "err_E0912_creates_p_sem_ptr", "err_E0912_destroys_sem_nome", "err_E0912_creates_sem_retorno",
                    "err_E0912_clausula_cstr"]

# Linha de proc foreign com clausula no fim da assinatura (creates [p] | destroys p | takes p); linha com '{' nao casa.
CLAUSULA = re.compile(r"^(\s*(?:pub\s+)?proc\s+[^{]*\))(\s*->\s*[^{]*?)?\s+(?:creates(?:\s+\w+)?|destroys\s+\w+|takes\s+\w+)\s*$")


def TirarClausulas(Texto: str) -> str:
    """Fonte sem as clausulas de posse (o 'release-igual' compara este com o original)."""
    Saida = []
    for Linha in Texto.splitlines(keepends=True):
        Corpo = Linha.rstrip("\r\n")
        Casou = CLAUSULA.match(Corpo)
        Saida.append((Casou.group(1) + (Casou.group(2) or "") if Casou else Corpo) + Linha[len(Corpo):])
    return "".join(Saida)


def ParaWsl(Caminho: Path) -> str:
    """C:/x/y -> /mnt/c/x/y."""
    Texto = Caminho.as_posix()
    return "/mnt/" + Texto[0].lower() + Texto[2:] if len(Texto) > 2 and Texto[1] == ":" else Texto


def ExtrairJson(Linha: str):
    """O JSON do R-code e achado dentro da linha (o aviso do wsl.exe pode vir colado antes)."""
    Inicio = Linha.find('{"codigo"')
    if Inicio < 0:
        return None
    return json.loads(Linha[Inicio:])


def Rodar(Alvo: str, Exe: Path) -> tuple:
    """Executa o programa e devolve (exit, stdout, linhas JSON do stderr)."""
    Comando = [str(Exe)] if Alvo == "windows" else ["wsl.exe", "-d", WSL_DISTRO, "--", ParaWsl(Exe)]
    Saida = subprocess.run(Comando, capture_output=True, timeout=TIMEOUT_S)
    Texto = Saida.stderr.decode("utf-8", errors="replace").replace("\x00", "")
    Linhas = [Achado for Achado in (ExtrairJson(Linha) for Linha in Texto.splitlines()) if Achado is not None]
    return Saida.returncode, Saida.stdout.decode("utf-8", errors="replace").replace("\r", "").strip(), Linhas


def Procs(Linha: dict) -> list:
    return [Quadro["proc"] for Quadro in Linha.get("pilha", [])]


def CompararLinhaR(Obtido: dict, Pedido: dict) -> str:
    for Chave, Valor in Pedido.items():
        if Chave == "pilha":
            if Procs(Obtido) != Valor:
                return f"pilha={Procs(Obtido)} (esperado {Valor})"
        elif Obtido.get(Chave) != Valor:
            return f"{Chave}={Obtido.get(Chave)!r} (esperado {Valor!r})"
    return ""


def CompararRodada(Esperado: dict, Codigo: int, Saida: str, Linhas: list) -> str:
    if Codigo != Esperado["exit"] or Saida != Esperado["stdout"] or len(Linhas) != len(Esperado["r"]):
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    for Obtido, Pedido in zip(Linhas, Esperado["r"]):
        Erro = CompararLinhaR(Obtido, Pedido)
        if Erro:
            return Erro
    return ""


def CarregarEsperado(Caminho: Path) -> dict:
    return json.loads(Caminho.read_text(encoding="utf-8"))


def Construir(Forja: Path, Fonte: Path, Alvo: str, Extras: list, Saida: Path) -> tuple:
    Comando = [str(Forja), "build"] + (["--target", "x86_64-linux"] if Alvo == "linux" else []) + Extras + [str(Fonte), "-o", str(Saida)]
    Resultado = subprocess.run(Comando, capture_output=True, timeout=TIMEOUT_S)
    return Resultado.returncode == 0, Resultado.stdout.decode("utf-8", errors="replace")[:300] + Resultado.stderr.decode("utf-8", errors="replace")[:300]


def ExtensaoDoExe(Alvo: str) -> str:
    return ".exe" if Alvo == "windows" else ".elf"


def PrepararVendorZlib(Pasta: Path, Nome: str, Tmp: Path, SemClausulas: bool = False) -> Path:
    """0.2.0 (P84): fixture com 'vendor:' e buildada numa pasta de trabalho: o .fj, vendor\\compression\\{compression,zlib}.fj, vendor\\libz.a e vendor\\linux\\libz.a (copia)."""
    Raiz = Tmp / "vendor_zlib" / Nome
    Vendor_ = Raiz / "vendor"
    (Vendor_ / "compression").mkdir(parents=True, exist_ok=True)
    (Vendor_ / "linux").mkdir(parents=True, exist_ok=True)
    shutil.copy2(Pasta / (Nome + ".fj"), Raiz / (Nome + ".fj"))
    for Arquivo in ("compression.fj", "zlib.fj"):
        shutil.copy2(VENDOR / "compression" / Arquivo, Vendor_ / "compression" / Arquivo)
    shutil.copy2(VENDOR / "libz.a", Vendor_ / "libz.a")
    shutil.copy2(VENDOR / "linux" / "libz.a", Vendor_ / "linux" / "libz.a")
    # 0.4.0 (P90): sqlite3.fj/cjson.fj (import interno vendor:db/sqlite3, vendor:json/cjson) + libs .a por alvo
    for Pacote, Arquivo, Lib in (("db", "sqlite3.fj", "libsqlite3.a"), ("json", "cjson.fj", "libcjson.a")):
        (Vendor_ / Pacote).mkdir(parents=True, exist_ok=True)
        Destino = Vendor_ / Pacote / Arquivo
        shutil.copy2(VENDOR / Pacote / Arquivo, Destino)
        if SemClausulas:  # gemeo do --release-igual: o vendor sem creates/destroys/takes (regex TirarClausulas)
            Destino.write_text(TirarClausulas(Destino.read_text(encoding="utf-8")), encoding="utf-8", newline="")
        shutil.copy2(VENDOR / Lib, Vendor_ / Lib)
        shutil.copy2(VENDOR / "linux" / Lib, Vendor_ / "linux" / Lib)
    return Raiz / (Nome + ".fj")


def VerificarRuntime(Forja: Path, Pasta: Path, Nome: str, Alvo: str, Repetir, Tmp: Path) -> tuple:
    Esperado = CarregarEsperado(Pasta / (Nome + ".esperado"))
    if Alvo not in Esperado:
        return True, "alvo nao previsto no .esperado"
    Exe = Tmp / f"{Nome}_{Alvo}{ExtensaoDoExe(Alvo)}"
    Fonte = Pasta / (Nome + ".fj")
    if "vendor:" in Fonte.read_text(encoding="utf-8"):
        Fonte = PrepararVendorZlib(Pasta, Nome, Tmp)
    Ok, Detalhe = Construir(Forja, Fonte, Alvo, [], Exe)
    if not Ok:
        return False, "build falhou: " + Detalhe
    Vezes = Repetir or REPETIR_FIXO.get(Nome, 1)
    for Volta in range(Vezes):
        Erro = CompararRodada(Esperado[Alvo], *Rodar(Alvo, Exe))
        if Erro:
            return False, f"rodada {Volta + 1}/{Vezes}: {Erro}"
    return True, f"{Vezes} de {Vezes}"


def VerificarRelease(Forja: Path, Pasta: Path, Nome: str, Tmp: Path) -> tuple:
    Esperado = CarregarEsperado(Pasta / (Nome + ".esperado"))
    if "release" not in Esperado:
        return True, "sem bloco release"
    Pedido = Esperado["release"]
    Exe = Tmp / f"{Nome}_release.exe"
    Ok, Detalhe = Construir(Forja, Pasta / (Nome + ".fj"), "windows", ["--release"], Exe)
    if not Ok:
        return False, "build --release falhou: " + Detalhe
    Erro = CompararRodada(Pedido, *Rodar("windows", Exe))
    if Erro:
        return False, "release: " + Erro
    Conteudo = Exe.read_bytes()
    for Simbolo in Pedido.get("simbolos_proibidos", []):
        if Simbolo.encode("ascii") in Conteudo:
            return False, f"release contem o simbolo {Simbolo!r}"
    return True, "release ok"


def PrimeiroDiagnostico(Texto: str):
    for Linha in Texto.splitlines():
        Achado = ExtrairJson(Linha)
        if Achado is not None:
            return Achado
    return None


def DiagnosticoDoBuild(Forja: Path, Fonte: Path, Alvo: str, Tmp: Path) -> tuple:
    Comando = [str(Forja), "build"] + (["--target", "x86_64-linux"] if Alvo == "linux" else []) + [str(Fonte), "-o", str(Tmp / (Fonte.stem + "_" + Alvo + ".bin"))]
    Resultado = subprocess.run(Comando, capture_output=True, timeout=TIMEOUT_S)
    return Resultado.returncode, PrimeiroDiagnostico(Resultado.stdout.decode("utf-8", errors="replace"))


def PosicaoDoDiagnostico(Diag: dict) -> tuple:
    return (Diag.get("codigo"), Diag.get("linha"), Diag.get("coluna"), Path(str(Diag.get("arquivo", ""))).name)


def VerificarSemantica(Forja: Path, Caminho: Path, Alvos: list, Tmp: Path) -> tuple:
    Arquivo = Caminho.with_suffix(".esperado.jsonl")
    Posicoes = {}
    if not Arquivo.exists() and Caminho.stem.startswith("ok_handle"):
        # P88: os ok_handle_* declaram foreign de nomes que nao existem em lib nenhuma; so a semantica vale (check), nao o ligador.
        Resultado = subprocess.run([str(Forja), "check", str(Caminho)], capture_output=True, timeout=TIMEOUT_S)
        return Resultado.returncode == 0, f"check exit {Resultado.returncode}"
    for Alvo in Alvos:
        Codigo, Diag = DiagnosticoDoBuild(Forja, Caminho, Alvo, Tmp)
        if not Arquivo.exists():
            if Codigo != 0:
                return False, f"{Alvo}: build exit {Codigo}"
            continue
        if Codigo == 0 or Diag is None:
            return False, f"{Alvo}: sem diagnostico (exit {Codigo})"
        Posicoes[Alvo] = PosicaoDoDiagnostico(Diag)
    if not Arquivo.exists():
        return True, f"ok nos alvos {', '.join(Alvos)}"
    Pedido = json.loads(Arquivo.read_text(encoding="utf-8").splitlines()[0])
    Esperado = (Pedido["codigo"], Pedido["linha"], Pedido["coluna"], Pedido["arquivo"])
    Divergentes = {Alvo: Pos for Alvo, Pos in Posicoes.items() if Pos != Esperado}
    return (not Divergentes), (f"ok nos alvos {', '.join(Alvos)}" if not Divergentes else f"obtido {Divergentes} esperado {Esperado}")


def EscreverFonteRelease(Nome: str, Texto: str, Base: Path, SemClausulas: bool) -> Path:
    """Grava a variante na pasta de fonte do alvo e devolve o .fj. Com vendor, a clausula esta no sqlite3.fj/cjson.fj: PrepararVendorZlib refaz a mesma pasta."""
    if Nome in RELEASE_IGUAL_VENDOR:
        return PrepararVendorZlib(PASTA_HANDLE, Nome, Base, SemClausulas)
    Base.mkdir(parents=True, exist_ok=True)
    Fonte = Base / (Nome + ".fj")
    Fonte.write_text(Texto, encoding="utf-8")
    return Fonte


def CompararReleaseAlvo(Forja: Path, Nome: str, Rotulo: str, Alvo: str, Extras: list, Textos: dict, Tmp: Path) -> tuple:
    """Um alvo: a pasta de fonte e o nome do exe sao os mesmos nas 2 variantes (COM e SEM); so muda a pasta de saida (A e B)."""
    Sufixo = Rotulo.replace(" ", "_")
    Base = Tmp / f"fonte_{Sufixo}"
    Exes = {}
    for Tag, Saida in (("com", "saida_A"), ("sem", "saida_B")):
        Fonte = EscreverFonteRelease(Nome, Textos[Tag], Base, Tag == "sem")
        PastaSaida = Tmp / f"{Saida}_{Sufixo}"
        PastaSaida.mkdir(parents=True, exist_ok=True)
        Exe = PastaSaida / f"bin{ExtensaoDoExe(Alvo)}"
        Ok, Detalhe = Construir(Forja, Fonte, Alvo, ["--release"] + Extras, Exe)
        if not Ok:
            return False, f"{Tag}: build falhou: {Detalhe}"
        Exes[Tag] = Exe.read_bytes()
    Com, Sem = Exes["com"], Exes["sem"]
    if Com != Sem:
        Pos = next((K for K, (A, B) in enumerate(zip(Com, Sem)) if A != B), min(len(Com), len(Sem)))
        return False, f"exe release diferente no byte {Pos} (tamanhos {len(Com)} x {len(Sem)})"
    return True, "release com = sem clausula"


def VerificarReleaseIgual(Forja: Path, Nome: str, Tmp: Path) -> tuple:
    Original = (PASTA_HANDLE / (Nome + ".fj")).read_text(encoding="utf-8")
    ComVendor = Nome in RELEASE_IGUAL_VENDOR
    Sem = Original if ComVendor else TirarClausulas(Original)  # com vendor a clausula esta no sqlite3.fj/cjson.fj: o gemeo tira dele
    if Sem == Original and not ComVendor:
        return False, "nenhuma clausula encontrada para tirar"
    Textos = {"com": Original, "sem": Sem}
    Variantes = [("x64 windows", "windows", ["--backend", "x64"]), ("x64 linux", "linux", ["--backend", "x64"])]
    Passaram = 0
    for Rotulo, Alvo, Extras in Variantes:  # todos os alvos rodam; a falha de um nao esconde o outro
        Ok, Detalhe = CompararReleaseAlvo(Forja, Nome, Rotulo, Alvo, Extras, Textos, Tmp)
        Passaram += 1 if Ok else 0
        print(f"{'PASSOU' if Ok else 'FALHOU'} release-igual {Nome} {Rotulo}: {Detalhe}")
    return Passaram == len(Variantes), f"{Passaram} de {len(Variantes)} alvos com release igual"


def ParaExecutar(Nomes: list, Filtro: str) -> list:
    return [Nome for Nome in Nomes if Filtro is None or Filtro == Nome]


def ExecutarRuntime(Forja: Path, Alvos: list, Nomes: list, Pasta: Path, Repetir, Tmp: Path) -> int:
    Falhas = 0
    for Nome in Nomes:
        for Alvo in Alvos:
            Ok, Detalhe = VerificarRuntime(Forja, Pasta, Nome, Alvo, Repetir, Tmp)
            Falhas += 0 if Ok else 1
            print(f"{'PASSOU' if Ok else 'FALHOU'} runtime {Alvo} {Nome}: {Detalhe}")
        if (Pasta / (Nome + ".esperado")).exists() and "release" in CarregarEsperado(Pasta / (Nome + ".esperado")):
            Ok, Detalhe = VerificarRelease(Forja, Pasta, Nome, Tmp)
            Falhas += 0 if Ok else 1
            print(f"{'PASSOU' if Ok else 'FALHOU'} runtime release {Nome}: {Detalhe}")
    return Falhas


def ExecutarSemantica(Forja: Path, Alvos: list, Nomes: list, Tmp: Path) -> int:
    Falhas = 0
    for Nome in Nomes:
        Ok, Detalhe = VerificarSemantica(Forja, PASTA_SEMANTICA / (Nome + ".fj"), Alvos, Tmp)
        Falhas += 0 if Ok else 1
        print(f"{'PASSOU' if Ok else 'FALHOU'} semantica {Nome}: {Detalhe}")
    return Falhas


def Autoteste() -> int:
    """Confere so o harness (sem forja): regex de clausula, extracao do JSON, comparacao, arquivos das listas."""
    Falhas = []

    def Checar(Rotulo: str, Condicao: bool):
        print(f"{'PASSOU' if Condicao else 'FALHOU'} selftest {Rotulo}")
        if not Condicao:
            Falhas.append(Rotulo)

    Checar("tirar clausula de destroys", TirarClausulas("    proc fclose_c(f: ?^Arq) -> i32 destroys f\n") == "    proc fclose_c(f: ?^Arq) -> i32\n")
    Checar("tirar clausula de creates sem retorno", TirarClausulas("    proc abre(x: i32) creates\n") == "    proc abre(x: i32)\n")
    Checar("tirar creates com saida", TirarClausulas("  proc s(n: cstr, banco: ^?^Db) -> i32 creates banco\n") == "  proc s(n: cstr, banco: ^?^Db) -> i32\n")
    Checar("tirar takes com alias", TirarClausulas('proc c_free = "free"(p: rawptr) destroys p\n') == 'proc c_free = "free"(p: rawptr)\n')
    Checar("linha com corpo fica", TirarClausulas("proc main() {\n") == "proc main() {\n")
    Checar("chamada com creates no texto fica", TirarClausulas("    let p = c_malloc(16)\n") == "    let p = c_malloc(16)\n")
    Checar("json com lixo antes", ExtrairJson('\x00x\x00 {"codigo": "R0009", "tipo": "handle"}') == {"codigo": "R0009", "tipo": "handle"})
    Checar("json ausente", ExtrairJson("linha sem json") is None)
    Checar("comparacao bate", CompararRodada({"exit": 0, "stdout": "5", "r": []}, 0, "5", []) == "")
    Checar("comparacao pega stdout", CompararRodada({"exit": 0, "stdout": "5", "r": []}, 0, "6", []) != "")
    Checar("comparacao pega pilha", CompararLinhaR({"pilha": [{"proc": "main"}]}, {"pilha": ["abrir", "main"]}) != "")
    Checar("release-igual: fixture real tem clausula", TirarClausulas((PASTA_HANDLE / "handle_ok.fj").read_text(encoding="utf-8")) != (PASTA_HANDLE / "handle_ok.fj").read_text(encoding="utf-8"))
    Fonte = (PASTA_HANDLE / "handle_ok.fj").read_text(encoding="utf-8")
    Checar("fonte real perde as clausulas", " creates" not in TirarClausulas(Fonte) and " destroys " not in TirarClausulas(Fonte))
    for Nome in FIXTURES_CLONG:
        Checar(f"arquivos clong {Nome}", (PASTA_CLONG / (Nome + ".fj")).exists() and CarregarEsperado(PASTA_CLONG / (Nome + ".esperado"))["windows"] is not None)
    for Nome in FIXTURES_HANDLE:
        Checar(f"arquivos handle {Nome}", (PASTA_HANDLE / (Nome + ".fj")).exists() and (PASTA_HANDLE / (Nome + ".esperado")).exists())
    for Nome in SEMANTICA_CLONG + SEMANTICA_HANDLE:
        Checar(f"semantica {Nome}", (PASTA_SEMANTICA / (Nome + ".fj")).exists())
    print(f"selftest: {'PASSOU' if not Falhas else 'FALHOU ' + str(len(Falhas))}")
    return 1 if Falhas else 0


def Principal() -> int:
    Parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Parser.add_argument("--forja")
    Parser.add_argument("--alvo", choices=["windows", "linux", "ambos"], default="ambos")
    Parser.add_argument("--grupo", choices=["clong", "handle", "tudo"], default="tudo")
    Parser.add_argument("--repetir", type=int, default=None)
    Parser.add_argument("--release-igual", action="store_true")
    Parser.add_argument("--fixture")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return Autoteste()
    if not Args.forja:
        print("falta --forja <forja.exe>")
        return 2
    Forja = Path(Args.forja).resolve()
    Alvos = list(ALVOS) if Args.alvo == "ambos" else [Args.alvo]
    if "linux" in Alvos and shutil.which("wsl.exe") is None:
        print("wsl.exe fora do PATH")
        return 2
    Falhas = 0
    with tempfile.TemporaryDirectory(prefix="verificar_m6c_") as Diretorio:
        PastaTemporaria = Path(Diretorio)
        if Args.grupo in ("clong", "tudo"):
            Falhas += ExecutarRuntime(Forja, Alvos, ParaExecutar(FIXTURES_CLONG, Args.fixture), PASTA_CLONG, Args.repetir, PastaTemporaria)
            Falhas += ExecutarSemantica(Forja, Alvos, ParaExecutar(SEMANTICA_CLONG, Args.fixture), PastaTemporaria)
        if Args.grupo in ("handle", "tudo"):
            Falhas += ExecutarRuntime(Forja, Alvos, ParaExecutar(FIXTURES_HANDLE, Args.fixture), PASTA_HANDLE, Args.repetir, PastaTemporaria)
            Falhas += ExecutarSemantica(Forja, Alvos, ParaExecutar(SEMANTICA_HANDLE, Args.fixture), PastaTemporaria)
        if Args.release_igual:
            for Nome in ParaExecutar(RELEASE_IGUAL_HANDLE + RELEASE_IGUAL_VENDOR, Args.fixture):
                Ok, Detalhe = VerificarReleaseIgual(Forja, Nome, PastaTemporaria)
                Falhas += 0 if Ok else 1
                print(f"{'PASSOU' if Ok else 'FALHOU'} release-igual {Nome}: {Detalhe}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
