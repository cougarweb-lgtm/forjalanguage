#!/usr/bin/env python3
"""conjunto_linux.py - o CONJUNTO do P29a: a suite inteira do compilador, montada por script para o alvo x86_64-linux (M5 P29a).

Objetivo: recontar (nunca a mao) o que roda no Linux e o que e PULADO por desenho. Cada .fj com `proc main(` dos grupos abaixo vira um Caso por modo (dev e release); o oraculo e,
em ordem: o esperado fixo (lowering e receitas: .esperado/.codigo), senao o x64-windows da mesma rodada (verificar_linux.PreencherOraculoWindows). Casos do conjunto sao Pulaveis:
a recusa X0003 do alvo Linux (foreign com link/dll) os tira do lote e eles entram na lista de pulados, contados e listados.
Grupos: dourado, lowering, exemplos (tests\\exemplos e examples\\), semantica (so ok_*), m1, m2, m4 (poda), v111, traco, receitas, mais os existentes de casos_linux (prova, m5 proc/arquivo)
e os de pasta (es, processo). Grupo "teste" (P29b, Modo "teste", exe de `forja test --target x86_64-linux -o`): as fixtures de nativo\\linux\\teste, as "// modos: teste" dos grupos e o std\\<m>\\testes.fj
de cada modulo, em texto e --json, contra o `forja test` do Windows (esperado_so\\ onde a saida depende do SO). Fora por desenho, sem build: nativo\\m3 inteiro (X0003).
Uso:   python -I conjunto_linux.py            (imprime as contagens por grupo, os fora-por-desenho e confere que nenhuma fonte some)
Verificacao: python -I conjunto_linux.py --selftest
Reversao: apagar este arquivo.

Versao: 0.5.5
           0.5.5 (20/11/2026) PREPARO 0.24.1 - pontes compressao_basico passa a levar compression/zlib_ponte.fj (cola zlib_ponte do libz.a; compression.fj importa vendor:compression/zlib_ponte).
           0.5.4 (19/11/2026) M8 - receitas/reload_erro (std:reload so Windows; no Linux devolve unsupported_target) entra em PULADOS_POR_DESENHO.
           0.5.3 (16/11/2026) M6d P92c - semantica/ok_handle_int_zero (fixture do P92b, so `forja check`, link "teste_a" sem a lib) entra em PULADOS_POR_DESENHO.
           0.5.2 (16/11/2026) M6c P91 - 7 fixtures semantica/ok_handle_* (so de `forja check`, simbolos foreign ficticios) entram em PULADOS_POR_DESENHO: 14 falsos X0101 no linux desde o P87.
           0.5.1 (16/11/2026) M6c P84 - motivo das 3 congeladas lowering/128, 164 e 176 trocado para "coberta por tests\\nativo\\m6c\\clong\\*"; MAX_ACEITAS nao sobe.
Changelog: 0.5.0 (15/11/2026) M5 P40 (D0655 b) - grupo "pontes": compressao_basico, json_basico e banco_basico (tests\\interop\\estatica, so Windows antes) rodam no alvo linux; cada uma numa pasta de trabalho com vendor\\ (modulos .fj, libz/libcjson/libsqlite3 do Windows e vendor\\linux do Linux) e o esperado.txt do interop (sem CRLF: o stdout do alvo e LF).
           0.4.0 (15/11/2026) M5 P39 (D0655) - FORA_POR_ABI_LP64: lowering/128, 164 e 176 (fixtures congeladas que declaram labs(long) como i32, LLP64) saem do conjunto e aparecem em ForaPorDesenho; voltam com a fixture c_long do M6c.
           0.3.0 (15/11/2026) M5 P39-B4 - PULADOS_POR_DESENHO: casos por NOME que o alvo linux nao roda (build X0101/X0102): DLL do Windows sem equivalente e lib propria sem fonte; saem do conjunto e aparecem em ForaPorDesenho.
           0.2.0 (14/11/2026) M5 P29b - grupo "teste" (CasosDoExecutorDeTestes) sai do "fora por desenho"; esperado_so tambem vale para o executor de testes (AplicarEsperadoDoTeste).
           0.1.0 (14/11/2026) M5 P29a - primeira versao (inclui esperado_so\\: saida que depende do alvo, escrita a mao).
"""
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
import casos_linux  # noqa: E402
from casos_linux import Caso  # noqa: E402

TESTES = casos_linux.TESTES
LANGUAGE = casos_linux.LANGUAGE
REGEX_MAIN = re.compile(rb"^\s*proc\s+main\s*\(", re.MULTILINE)
REGEX_BLOCO_TESTE = re.compile(rb'^\s*test\s+"', re.MULTILINE)
CABECALHO_DEV = b"// modos: dev"
CABECALHO_TESTE = b"// modos: teste"
# (grupo, pasta, padrao do nome); a pasta e varrida inteira, sem as ocultas
GRUPOS = (
    ("dourado", TESTES / "dourado", "*.fj"),
    ("lowering", TESTES / "lowering", "[0-9][0-9]*_*.fj"),
    ("exemplos", TESTES / "exemplos", "*.fj"),
    ("examples", LANGUAGE.parent / "examples", "*.fj"),
    ("semantica", TESTES / "semantica", "ok_*.fj"),
    ("m1", TESTES / "nativo" / "m1", "*.fj"),
    ("m2", TESTES / "nativo" / "m2", "*.fj"),
    ("m4", TESTES / "nativo" / "m4", "*.fj"),
    ("v111", TESTES / "nativo" / "v111", "*.fj"),
    ("traco", TESTES / "nativo" / "traco", "*.fj"),
    ("receitas", TESTES / "receitas", "*.fj"),
)
PASTAS_PROPRIAS = (("pasta", AQUI / "es"), ("pasta", AQUI / "processo"))  # casos_linux.CasosPasta (com .args/.env/.entrada)
PASTA_ESPERADO_SO = AQUI / "esperado_so"
# Os testes do std:process rodam cmd.exe e powershell.exe; o WSL poe o PATH do Windows no do Linux (interop) e os acharia. Num Linux de verdade eles nao existem: o PATH do caso nao os tem.
PATH_SEM_WINDOWS = {"PATH": "/usr/local/bin:/usr/bin:/bin"}
# P39-B4: casos (pelo nome, os dois modos) que o alvo linux nao roda por desenho: build X0101/X0102 por DLL do Windows sem equivalente no Linux
# (kernel32, user32, raylib sem fonte) ou simbolo de lib propria que nao existe em nenhum alvo (o x64-windows recusa igual). Nao inclui X0003 (esses ja sao pulados).
PULADOS_POR_DESENHO = (
    ("lowering/165_foreign_opaco", "link kernel32 (GetProcessHeap, HeapAlloc): DLL do Windows sem equivalente no Linux (X0102)"),
    ("lowering/199_proc_valor_as_proc_rawptr", "GetModuleHandleA e do kernel32 (DLL do Windows), sem link: o alvo linux nao o acha (X0101)"),
    ("semantica/ok_f4_simbolo_c_igual", "link kernel32 (GetCurrentProcessId): DLL do Windows sem equivalente no Linux (X0102)"),
    ("semantica/ok_proc_as_rawptr", "GetModuleHandleA do kernel32, sem link: DLL do Windows sem equivalente no Linux (X0101)"),
    ("semantica/ok_proc_foreign_wndclass", "link user32 (RegisterClassA): DLL do Windows sem equivalente no Linux (X0102)"),
    ("semantica/ok_v111_quat_basico", "link raylib: lib sem fonte para Linux (sem libraylib em vendor/linux, X0102)"),
    ("semantica/ok_09_foreign", "simbolos preencher, estado, dividir_por_valor, InitWindow de lib propria sem .lib/.dll na pasta: X0101 tambem no x64-windows"),
    ("semantica/ok_v18b_opaco", "criar_janela e fechar_janela de lib propria sem .lib/.dll na pasta: X0101 tambem no x64-windows"),
    ("teste/semantica/ok_09_foreign", "mesma lib propria de semantica/ok_09_foreign (executor de testes): X0101 tambem no x64-windows"),
    ("teste/semantica/ok_09_foreign json", "mesma lib propria de semantica/ok_09_foreign (executor de testes, --json): X0101 tambem no x64-windows"),
    ("semantica/ok_handle_defer", "so para `forja check` (M6c P87): fopen_c fictício, X0101 tambem no x64-windows"),
    ("semantica/ok_handle_emprestimo", "so para `forja check` (M6c P87): fopen_c fictício, X0101 tambem no x64-windows"),
    ("semantica/ok_handle_entrega_campo", "so para `forja check` (M6c P87): fopen_c fictício, X0101 tambem no x64-windows"),
    ("semantica/ok_handle_none", "so para `forja check` (M6c P87): fopen_c fictício, X0101 tambem no x64-windows"),
    ("semantica/ok_handle_saida", "so para `forja check` (M6c P87): sqlite3_open sem a lib no link, X0101 tambem no x64-windows"),
    ("semantica/ok_handle_struct_valor", "so para `forja check` (M6c P87): abrir_id fictício, X0101 tambem no x64-windows"),
    ("semantica/ok_handle_takes", "so para `forja check` (M6c P87): cJSON_CreateObject sem a lib no link, X0101 tambem no x64-windows"),
    ("semantica/ok_handle_int_zero", "so para `forja check` (M6d P92b): link \"teste_a\" sem a lib na pasta, X0102 tambem no x64-windows"),
    ("receitas/reload_erro", "std:reload e so Windows (M8): no Linux todo proc devolve unsupported_target, entao o not_found da receita nao vale; coberto por tests\\reload\\verificar_reload.py"),
)
# D0655: fixtures congeladas que declaram labs(long) como i32 (long = 32 bits so no Windows/LLP64). No Linux LP64 o argumento sai com os 32 bits altos zerados e o resultado fica errado;
# o compilador faz o que a declaracao manda. Nao entram em divergencias_aceitas.json (resultado errado) nem em esperado_so. Voltam com a fixture c_long do M6c.
FORA_POR_ABI_LP64 = (
    ("lowering/128_foreign_libc_sem_link", "declara labs(long) como i32 (LLP64); congelada; coberta por tests\\nativo\\m6c\\clong\\*"),
    ("lowering/164_enum_com_valor", "declara labs(long) como i32 (LLP64); congelada; coberta por tests\\nativo\\m6c\\clong\\*"),
    ("lowering/176_foreign_alias", "declara labs(long) como i32 (LLP64); congelada; coberta por tests\\nativo\\m6c\\clong\\*"),
)

# P40 (D0655 b): pontes vendor que o alvo linux liga pelas libs de vendor\ (Windows) e vendor\linux (Linux). Ficavam so em m3/interop (Windows).
# Cada uma vai para uma pasta de trabalho com vendor\ (modulos .fj da fachada e as .a); a saida esperada e a do interop (igual nos dois SOs).
PONTES = (
    ("compressao_basico", ("compression/compression.fj", "compression/zlib.fj", "compression/zlib_ponte.fj"), "z"),
    ("json_basico", ("json/json.fj", "json/cjson.fj"), "cjson"),
    ("banco_basico", ("db/db.fj", "db/sqlite3.fj"), "sqlite3"),
)
PASTA_INTEROP = LANGUAGE / "tests" / "interop" / "estatica"
PASTA_PONTES = Path(tempfile.gettempdir()) / "forja_linux" / "pontes"


def PrepararPonte(Nome: str, Modulos: tuple, Lib: str) -> Path:
    """Pasta de trabalho da ponte: caso.fj, vendor\\<pacote>\\<modulo>.fj, vendor\\lib<lib>.a (Windows) e vendor\\linux\\lib<lib>.a (Linux)."""
    Raiz = PASTA_PONTES / Nome
    Vendor = Raiz / "vendor"
    (Vendor / "linux").mkdir(parents=True, exist_ok=True)
    shutil.copy2(PASTA_INTEROP / Nome / "caso.fj", Raiz / "caso.fj")
    for Modulo in Modulos:
        (Vendor / Modulo).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(LANGUAGE / "vendor" / Modulo, Vendor / Modulo)
    shutil.copy2(LANGUAGE / "vendor" / f"lib{Lib}.a", Vendor / f"lib{Lib}.a")
    shutil.copy2(LANGUAGE / "vendor" / "linux" / f"lib{Lib}.a", Vendor / "linux" / f"lib{Lib}.a")
    return Raiz / "caso.fj"


def CasosDasPontes() -> list:
    """Uma ponte = um Caso por modo (dev, release); sem Pulavel: X0003 aqui e falha, nao pulo."""
    Casos = []
    for Nome, Modulos, Lib in PONTES:
        Fonte = PrepararPonte(Nome, Modulos, Lib)
        Esperado = (PASTA_INTEROP / Nome / "esperado.txt").read_bytes().replace(b"\r\n", b"\n")
        Casos.extend(Caso(f"pontes/{Nome}", Fonte, Modo, Esperado=Esperado, Grupo="pontes") for Modo in casos_linux.MODOS)
    return Casos


def ListarFontes(Pasta: Path, Padrao: str) -> list:
    """Os .fj da pasta (recursivo, sem pastas ocultas) que tem `proc main(`; os sem main sao modulos de apoio."""
    Achados = []
    for Raiz, Subpastas, Nomes in os.walk(Pasta):
        Subpastas[:] = sorted(S for S in Subpastas if not S.startswith("."))
        Achados += [Path(Raiz) / N for N in sorted(Nomes) if Path(N).match(Padrao) and REGEX_MAIN.search((Path(Raiz) / N).read_bytes())]
    return Achados


def PrimeiraLinha(Fonte: Path) -> bytes:
    with open(Fonte, "rb") as Arquivo:
        return Arquivo.readline().strip()


def ModosDaFonte(Fonte: Path) -> tuple:
    """dev e release; so dev com '// modos: dev'; nenhum com '// modos: teste' (e do forja test, P29b)."""
    Primeira = PrimeiraLinha(Fonte)
    if Primeira == CABECALHO_TESTE:
        return ()
    return ("dev",) if Primeira == CABECALHO_DEV else casos_linux.MODOS


def NomeDoCaso(Grupo: str, Pasta: Path, Fonte: Path) -> str:
    return f"{Grupo}/" + Fonte.relative_to(Pasta).with_suffix("").as_posix()


def CasosFixosDoLowering(Fonte: Path, Nome: str, Modos: tuple) -> list:
    Lowering = casos_linux.Carregar(TESTES / "lowering" / "verificar_lowering.py", "verificar_lowering_linux")
    Base = Lowering.MontarCaso(Fonte)
    Casos = []
    for Modo in Modos:
        Codigo = Lowering.CodigoEsperadoNoModo(Base, Modo)
        Casos.append(Caso(Nome, Fonte, Modo, Esperado=Lowering.StdoutEsperadoNoModo(Base, Modo).encode("utf-8"), Exit=casos_linux.CODIGO_FALHA_RUNTIME if Codigo else 0,
                          ErroTem=f'"{Codigo}"' if Codigo else None, Grupo="lowering", Pulavel=True))
    return Casos


def CasosFixosDaReceita(Fonte: Path, Nome: str, Modos: tuple) -> list:
    Esperado = Fonte.with_suffix(".esperado").read_bytes().replace(b"\r\n", b"\n")
    return [Caso(Nome, Fonte, Modo, Esperado=Esperado, ErroVazio=True, Grupo="receitas", Pulavel=True) for Modo in Modos]


def CasosDaFonte(Grupo: str, Pasta: Path, Fonte: Path) -> list:
    Nome, Modos = NomeDoCaso(Grupo, Pasta, Fonte), ModosDaFonte(Fonte)
    if Grupo == "lowering":
        return CasosFixosDoLowering(Fonte, Nome, Modos)
    if Grupo == "receitas":
        return CasosFixosDaReceita(Fonte, Nome, Modos)
    return [Caso(Nome, Fonte, Modo, Grupo=Grupo, Pulavel=True) for Modo in Modos]


def ForaPorDesenho() -> list:
    """[(grupo, fonte, motivo)] das fontes que o conjunto NAO roda: m3 inteiro (foreign com link/dll) e os PULADOS_POR_DESENHO (fonte = nome do caso). As '// modos: teste' e os std\\<m>\\testes.fj rodam no executor de testes (P29b)."""
    Fora = [("m3", F, "m3 inteiro: foreign com link/dll (X0003 por desenho)") for F in ListarFontes(TESTES / "nativo" / "m3", "*.fj")]
    return Fora + [(Nome.split("/")[0], Nome, Motivo) for Nome, Motivo in PULADOS_POR_DESENHO + FORA_POR_ABI_LP64]


def CasosDeUmTeste(Nome: str, Fonte: Path, Alvo: str | None, Extra: dict | None = None) -> list:
    """Texto e --json do executor de testes para uma fonte (forja test <fonte|std:m>); o oraculo e o `forja test` do Windows da mesma rodada. Extra = ambiente a mais do exe."""
    return [Caso(Nome + Rotulo, Fonte, "teste", Env={**casos_linux.AmbienteDoTeste(Json, None), **(Extra or {})}, Grupo="teste", Pulavel=True, ComparaWin=True,
                 Teste={"json": Json, "filtro": None, "alvo": Alvo}) for Rotulo, Json in (("", False), (" json", True))]


def CasosDoExecutorDeTestes() -> list:
    """P29b: as fixtures proprias (teste\\), as '// modos: teste' dos grupos e o testes.fj de cada modulo std, mais o programa que cai fora do executor (sinal 11)."""
    Casos = casos_linux.CasosTeste(AQUI / "teste") + casos_linux.CasosFora(AQUI / "teste", casos_linux.ListaMusl())
    for Grupo, Pasta, Padrao in GRUPOS:  # as "// modos: teste" e, como o verificar_nativo, todo programa dos grupos que tem bloco test "..."
        for Fonte in (F for F in ListarFontes(Pasta, Padrao) if not ModosDaFonte(F) or REGEX_BLOCO_TESTE.search(F.read_bytes())):
            Casos += CasosDeUmTeste("teste/" + NomeDoCaso(Grupo, Pasta, Fonte), Fonte, None)
    for Fonte in sorted((LANGUAGE / "std").glob("*/testes.fj")):
        Modulo = Fonte.parent.name
        Casos += CasosDeUmTeste(f"teste/std/{Modulo}", Fonte, f"std:{Modulo}", PATH_SEM_WINDOWS if Modulo == "process" else None)
    return Casos


def CasosDeGrupos() -> list:
    Casos = []
    for Grupo, Pasta, Padrao in GRUPOS:
        for Fonte in ListarFontes(Pasta, Padrao):
            Casos += CasosDaFonte(Grupo, Pasta, Fonte)
    return Casos


def CasosDeCasosLinux() -> list:
    """prova, proc e arquivo (com esperado proprio) e as pastas es e processo (P28), todos agora Pulaveis nao: sao os 114 do P28."""
    Casos = casos_linux.TodosOsCasos()
    for _, Pasta in PASTAS_PROPRIAS:
        Casos += casos_linux.CasosPasta(Pasta)
    return Casos


def AplicarEsperadoDoTeste(Caso_: Caso, Prefixo: str) -> None:
    """Caso do executor de testes cuja saida depende do SO (os testes do std\\path esperam a barra do Windows): <Prefixo>.esperado (stdout), .exit (padrao 1) e .stderr (exato, ou ausente = sem checar)."""
    Esperado, Saida, Erro = Path(Prefixo + ".esperado"), Path(Prefixo + ".exit"), Path(Prefixo + ".stderr")
    Caso_.Esperado, Caso_.ComparaWin, Caso_.ErroTem, Caso_.ErroVazio = Esperado.read_bytes(), False, None, False
    Caso_.Exit = int(Saida.read_text()) if Saida.is_file() else 1
    Caso_.ErroExato = Erro.read_bytes() if Erro.is_file() else None


def AplicarEsperadoPorSO(Casos: list) -> None:
    """esperado_so\\<grupo>__<caso>.esperado (escrito e revisto a mao) vale no lugar do esperado/oraculo para o programa cuja saida depende do alvo (os.TARGET, std:path).
    Nos casos do executor de testes o nome tem o espaco trocado por ponto (teste__std__path.json.esperado)."""
    for Caso_ in Casos:
        Nome = Caso_.Nome.replace("/", "__").replace(" ", ".")
        Arquivo = PASTA_ESPERADO_SO / (Nome + ".esperado")
        if Arquivo.is_file() and Caso_.Teste is not None:
            AplicarEsperadoDoTeste(Caso_, str(Arquivo)[: -len(".esperado")])
        elif Arquivo.is_file():
            Caso_.Esperado, Caso_.Exit, Caso_.ErroTem, Caso_.ErroVazio, Caso_.ErroExato = Arquivo.read_bytes(), 0, None, True, None


def TodoOConjunto() -> list:
    """Os casos do P28 (sem os 6 lowering, que o gerador de lowering ja inclui com o mesmo esperado) mais o conjunto novo."""
    Antigos = [C for C in CasosDeCasosLinux() if not C.Nome.startswith("lowering/")]
    for C in Antigos:
        C.Grupo = C.Nome.split("/")[0]
    Casos = Antigos + CasosDeGrupos() + CasosDoExecutorDeTestes() + CasosDasPontes()
    Pulados = {Nome for Nome, _ in PULADOS_POR_DESENHO + FORA_POR_ABI_LP64}
    Casos = [C for C in Casos if C.Nome not in Pulados]
    AplicarEsperadoPorSO(Casos)
    return Casos


def Contagens(Casos: list) -> dict:
    Fontes = {}
    for C in Casos:
        Fontes.setdefault(C.Grupo or C.Nome.split("/")[0], set()).add(str(C.Fonte))
    return {Chave: len(Valor) for Chave, Valor in sorted(Fontes.items())}


def Selftest() -> int:
    Casos = TodoOConjunto()
    Chaves = [(C.Nome, C.Modo) for C in Casos]
    Duplicados = len(Chaves) - len(set(Chaves))
    Faltam = [C.Nome for C in Casos if not C.Fonte.is_file()]
    Fora = ForaPorDesenho()
    print(f"conjunto_linux selftest: {len(Casos)} casos; fontes por grupo {Contagens(Casos)}; fora por desenho {len(Fora)}; duplicados {Duplicados}; sem fonte {Faltam}")
    return 1 if Duplicados or Faltam or not Casos else 0


if __name__ == "__main__":
    sys.exit(Selftest())
