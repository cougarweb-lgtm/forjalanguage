"""Bateria de headers do 'forja ligar': mede, por header C real, o quanto da lib vira ponte Forja.

Objetivo : acompanhar o plano de interop C (gaveta\\plano_interop_c.txt, F0): por header, procs gerados x simbolos exportados da lib
           (nm sobre o .a/.dll.a de language\\vendor) x declaracoes de funcao do header (libclang), recusas por codigo Gxxxx
           (do log JSON e dos comentarios '// recusado' da ponte) e erros do 'forja check' sobre a ponte gerada.
Uso      : python -I bateria_headers.py [--forja forja.exe] [--ligar forja_ligar.py] [--so sqlite3,zlib] [--json]
                                        [--gravar arq.json] [--comparar arq.json]
           --ligar poe o script em FORJA_LIGAR_SCRIPT (testa um ligar novo sem tocar o compilador).
           Header que nao existe no disco entra como PULADO com o motivo (nunca inventado).
Versao   : 1.2.2
Changelog: 1.2.2 - 0.21.0 F3: --layout mede so o x64 (backend C aposentado na 0.21.0); sem forja-c no dicionario.
           1.2.1 - IA1-L3c2: os caminhos de vendor/ renomeados (db, compression, net, db_server, physics2d).
           1.2.0 - Interop F1e: modo --cola (ligar 0.6.0: roda o ligar com --cola --compilar-cola; mede G0005 e G0004 que sobram, wrappers da cola, se a
                   lib<x>_cola.a saiu e o 'forja check'); colunas G0004 e cola na tabela; baseline_f1e.json; item 'macros' do catalogo (--macros).
           1.1.0 - box2d.h completo (v3.2.0, MIT) lido de %TEMP%\box2d_inc_320; modo --layout (+ --plantar): libclang x gcc x Forja (c e x64).
           1.0.0 - F0 do plano de interop C: 10 headers, tabela, --json, linha de base (--gravar) e antes/depois (--comparar).
Depende de: forja.exe, gcc/nm no PATH, libclang (so a coluna 'decl'; sem ela vira n/d).
Verificar: python -I bateria_headers.py --selftest
Reversao : arquivo novo; apagar.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

VERSAO = "1.2.2"
PastaLanguage = Path(__file__).resolve().parent.parent.parent
PastaProjeto = PastaLanguage.parent
ForjaPadrao = PastaLanguage / "compiler" / "forja.exe"
LigarPadrao = PastaLanguage / "tools" / "forja_ligar" / "forja_ligar.py"
TimeoutSegundos = 600
Vendor = "vendor"
Raylib = "interop_lab/raylib/vendor/include"

# nome, header (relativo a language\; "../" sai para a raiz do projeto), -I, .a/.dll.a para o nm, link, motivo do pulo
CATALOGO = [
    {"nome": "sqlite3", "header": "vendor/db/sqlite/sqlite3.h", "incs": [], "lib": "vendor/libsqlite3.a", "link": "sqlite3"},
    {"nome": "zlib", "header": "vendor/compression/zlib/zlib.h", "incs": [], "lib": "vendor/libz.a", "link": "z"},
    {"nome": "box2d", "header": "%TEMP%/box2d_inc_320/box2d/box2d.h", "incs": ["%TEMP%/box2d_inc_320"],
     "lib": "vendor/libbox2d.a", "link": "box2d",
     "motivo": "headers do Box2D v3.2.0 (MIT) nao estao em %TEMP%\\box2d_inc_320\\box2d (baixar de github.com/erincatto/box2d tag v3.2.0, include/box2d/*.h; vendor\\physics2d guarda so box2d.fj)"},
    {"nome": "raylib", "header": Raylib + "/raylib.h", "incs": [Raylib], "lib": "vendor/libraylib.a", "link": "raylib"},
    {"nome": "raymath", "header": Raylib + "/raymath.h", "incs": [Raylib], "lib": None, "link": "raylib"},
    {"nome": "curl", "header": "vendor/net/curl/include/curl/curl.h", "incs": ["vendor/net/curl/include"],
     "lib": "vendor/libcurl.a", "link": "curl"},
    {"nome": "mysql", "header": "vendor/db_server/mariadb/include/mysql.h", "incs": ["vendor/db_server/mariadb/include"],
     "lib": "vendor/libmariadb.dll.a", "link": "mariadb"},
    {"nome": "cgltf", "header": "vendor/gltf/cgltf/cgltf.h", "incs": [], "lib": "vendor/libcgltf.a", "link": "cgltf"},
    {"nome": "miniaudio", "header": "vendor/audio/miniaudio/miniaudio.h", "incs": [], "lib": "vendor/libminiaudio.a",
     "link": "miniaudio"},
    {"nome": "stb_image", "header": "../engine/vendor/stb/stb_image.h", "incs": [], "lib": None, "link": "stb_image"},
    # 1.2.0: mecanica de --macros no raylib.h real: 3 macro-funcoes (RL_MALLOC, RL_CALLOC, RL_FREE) ganham wrapper pelo .sig; as outras seguem G0004
    {"nome": "raylib_sig", "header": Raylib + "/raylib.h", "incs": [Raylib], "lib": "vendor/libraylib.a", "link": "raylib", "so_cola": True,
     "macros": "tests/ligar/mods/raylib_macros.sig"},
    # 1.2.0: raymath como 'static inline' de verdade (a RMAPI padrao e 'inline' C99 com definicao externa na libraylib): so a cola resolve os 146
    {"nome": "raymath_si", "header": Raylib + "/raymath.h", "incs": [Raylib], "lib": None, "link": "raylib", "so_cola": True,
     "defs": ["RAYMATH_STATIC_INLINE"]},
]

# roda sem -I e com o forja_ligar como modulo: conta as funcoes (unicas) declaradas na pasta do header
CODIGO_DECLARACOES = """
import importlib.util, os, sys
Spec = importlib.util.spec_from_file_location("fl", sys.argv[1]); L = importlib.util.module_from_spec(Spec); Spec.loader.exec_module(L)
L.CarregarLibclang(); Header = sys.argv[2]; Incs = sys.argv[3:]
Pastas = Incs + L.IncludesDoCompilador("gcc"); U = L.AbrirUnidade(Header, Pastas, [])
Pasta = os.path.dirname(os.path.normcase(os.path.abspath(Header))); Nomes = set()
for C in U.cursor.get_children():
    if C.kind == L.Ci.CursorKind.FUNCTION_DECL and C.location.file and os.path.normcase(os.path.abspath(C.location.file.name)).startswith(Pasta):
        Nomes.add(C.spelling)
print(len(Nomes))
"""


def Rodar(Comando: list, Ambiente: dict = None) -> subprocess.CompletedProcess:
    return subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace",
                          timeout=TimeoutSegundos, env=Ambiente)


def LinhasJson(Texto: str) -> list:
    """Linhas do stderr/stdout que sao objetos JSON de diagnostico (uma por evento); o resto e ignorado."""
    Itens = []
    for Linha in Texto.splitlines():
        if Linha.startswith("{"):
            try:
                Itens.append(json.loads(Linha))
            except ValueError:  # lint-ok: iaf-erro-engolido linha que parece JSON mas nao e: ignorada de proposito
                pass
    return Itens


def ContarCodigos(Eventos: list, Inicial: str) -> dict:
    """Contagem por codigo (G... ou E...) entre os eventos JSON."""
    Contagem: dict = {}
    for E in Eventos:
        Codigo = str(E.get("codigo", ""))
        if Codigo.startswith(Inicial):
            Contagem[Codigo] = Contagem.get(Codigo, 0) + 1
    return dict(sorted(Contagem.items()))


def ContarProcsDoModulo(Texto: str) -> int:
    """Procs de foreign ('    proc nome(...)') da ponte."""
    return len(re.findall(r"^    proc ", Texto, re.M))


def ContarComentariosRecusa(Texto: str) -> dict:
    """Comentarios '// recusado NOME Gxxxx causa' (ligar >= 0.4.0), por codigo."""
    Contagem: dict = {}
    for Codigo in re.findall(r"^// recusado \S+ (G\d{4})", Texto, re.M):
        Contagem[Codigo] = Contagem.get(Codigo, 0) + 1
    return dict(sorted(Contagem.items()))


def PrimeiroUso(Texto: str, Modulo: str) -> str:
    """Linha de uso para o main do stub: a 1a const publica; vazio se a ponte nao tem const."""
    Achado = re.search(r"^pub const (\w+):", Texto, re.M)
    return f"    let _v = {Modulo}.{Achado.group(1)}\n" if Achado else ""  # print recusa f32; '_v' evita o descarte '_'


def MontarStub(Texto: str, Modulo: str) -> str:
    return f'import "{Modulo}"\n\nproc main() {{\n{PrimeiroUso(Texto, Modulo)}}}\n'


def ContarSimbolosNm(Lib: Path) -> int:
    """Simbolos de codigo definidos e globais da lib (T/t de objeto, I de import lib); -1 se nm falhar."""
    try:
        Saida = Rodar(["nm", "-g", "--defined-only", str(Lib)]).stdout
    except (OSError, subprocess.SubprocessError):
        return -1
    Nomes = set()
    for Linha in Saida.splitlines():
        Partes = Linha.split()
        if len(Partes) == 3 and Partes[1] in ("T", "I"):
            Nomes.add(Partes[2])
    return len(Nomes)


def ContarDeclaracoes(Ligar: Path, Header: Path, Incs: list) -> int:
    """Funcoes unicas declaradas na pasta do header (libclang via o proprio forja_ligar); -1 sem libclang."""
    try:
        Saida = Rodar([sys.executable, "-c", CODIGO_DECLARACOES, str(Ligar), str(Header)] + Incs)
        return int(Saida.stdout.strip().splitlines()[-1]) if Saida.returncode == 0 else -1
    except (OSError, subprocess.SubprocessError, ValueError, IndexError):
        return -1


def Caminho(Relativo: str) -> Path:
    """Relativo a language\\; '%TEMP%' no inicio vira a pasta temporaria do usuario (headers baixados fora do projeto)."""
    return (PastaLanguage / Relativo.replace("%TEMP%", tempfile.gettempdir())).resolve()


def ChecarPonte(Forja: Path, Pasta: Path, Modulo: str) -> dict:
    """forja check de um stub que importa a ponte: erros E por codigo (E1005 de import sem uso nao conta)."""
    Texto = (Pasta / f"{Modulo}.fj").read_text(encoding="utf-8")
    Stub = Pasta / "stub_bateria.fj"
    Stub.write_text(MontarStub(Texto, Modulo), encoding="utf-8", newline="\n")
    Rodada = Rodar([str(Forja), "check", str(Stub)])
    Erros = ContarCodigos(LinhasJson(Rodada.stdout + "\n" + Rodada.stderr), "E")
    Erros.pop("E1005", None)
    return {"erros": sum(Erros.values()), "codigos": Erros}


def ComandoDoLigar(Item: dict, Forja: Path, Header: Path, Saida: Path) -> list:
    """forja ligar do item; com cola (1.2.0) leva --cola --compilar-cola e, se o item tem 'macros', --macros <sig>."""
    Comando = [str(Forja), "ligar", str(Header), "--link", Item["link"], "-o", str(Saida)] + [f"-I{Caminho(P)}" for P in Item["incs"]]
    Comando += [f"-D{D}" for D in Item.get("defs", [])]
    if Item.get("cola"):
        Comando += ["--cola", "--compilar-cola"]
        if Item.get("macros"):
            Comando += ["--macros", str(Caminho(Item["macros"]))]
    return Comando


def ContarWrappersDaCola(Texto: str) -> int:
    """'// cola C: x_cola.c (N wrappers; ...)' da ponte (ligar >= 0.6.0); 0 sem cola."""
    Achado = re.search(r"^// cola C: \S+ \((\d+) wrappers", Texto, re.M)
    return int(Achado.group(1)) if Achado else 0


def MedirHeader(Item: dict, Forja: Path, Ligar: Path, Cola: bool = False) -> dict:
    """Roda ligar + check num header do catalogo; PULADO se o header nao existe. Cola=True mede com --cola --compilar-cola."""
    Header = Caminho(Item["header"])
    if not Header.is_file():
        return {"nome": Item["nome"], "pulado": Item.get("motivo", f"header nao encontrado em {Item['header']}")}
    Incs = [str(Caminho(P)) for P in Item["incs"]]
    Pasta = Path(tempfile.mkdtemp(prefix="bateria_ligar_"))
    Saida = Pasta / f"{Item['nome']}.fj"
    Ambiente = dict(os.environ, FORJA_LIGAR_SCRIPT=str(Ligar))
    Rodada = Rodar(ComandoDoLigar(dict(Item, cola=Cola), Forja, Header, Saida), Ambiente)
    if not Saida.is_file():
        return {"nome": Item["nome"], "pulado": f"ligar nao gerou a ponte (exit {Rodada.returncode}): {Rodada.stderr[:200]}"}
    Texto = Saida.read_text(encoding="utf-8")
    Recusas = ContarCodigos(LinhasJson(Rodada.stderr), "G")
    Resultado = {"nome": Item["nome"], "procs": ContarProcsDoModulo(Texto), "recusas": Recusas,
                 "total_recusas": sum(Recusas.values()), "comentarios": ContarComentariosRecusa(Texto),
                 "decl": ContarDeclaracoes(Ligar, Header, Incs), "exp": -1, "md5_ponte": hashlib.md5(Texto.encode()).hexdigest()[:8]}
    if Cola:
        Resultado["cola"] = {"wrappers": ContarWrappersDaCola(Texto), "a_gerado": any(Pasta.glob("lib*_cola.a"))}
    if Item["lib"] and Caminho(Item["lib"]).is_file():
        Resultado["exp"] = ContarSimbolosNm(Caminho(Item["lib"]))
    Resultado["check"] = ChecarPonte(Forja, Pasta, Item["nome"])
    LimparPasta(Pasta)
    return Resultado


def LimparPasta(Pasta: Path) -> None:
    for Arquivo in Pasta.glob("*"):
        try:
            Arquivo.unlink()
        except OSError:  # lint-ok: iaf-erro-engolido limpeza de temporario e melhor esforco
            pass
    try:
        Pasta.rmdir()
    except OSError:  # lint-ok: iaf-erro-engolido limpeza de temporario e melhor esforco
        pass


def MedirTudo(Forja: Path, Ligar: Path, Nomes: list, Cola: bool = False) -> dict:
    """Mede os headers do catalogo em paralelo; cada um numa pasta temporaria propria. Itens 'so_cola' so entram com Cola=True."""
    Itens = [I for I in CATALOGO if (not Nomes or I["nome"] in Nomes) and (Cola or not I.get("so_cola"))]
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as Pool:
        Resultados = list(Pool.map(lambda I: MedirHeader(I, Forja, Ligar, Cola), Itens))
    return {R["nome"]: R for R in Resultados}


def VersaoDoLigar(Forja: Path, Ligar: Path) -> str:
    Rodada = Rodar([str(Forja), "ligar", "--versao"], dict(os.environ, FORJA_LIGAR_SCRIPT=str(Ligar)))
    return (Rodada.stdout or Rodada.stderr).strip()


def Md5Arquivo(Arquivo: Path) -> str:
    return hashlib.md5(Arquivo.read_bytes()).hexdigest()


def Celula(Valor) -> str:
    return "n/d" if Valor in (None, -1) else str(Valor)


def LinhaDaTabela(R: dict) -> str:
    if "pulado" in R:
        return f"{R['nome']:<10} PULADO: {R['pulado']}"
    G = R["recusas"]
    Cola = R.get("cola", {})
    return (f"{R['nome']:<10} {R['procs']:>5} {Celula(R['exp']):>5} {Celula(R['decl']):>5} {R['total_recusas']:>5} "
            f"{G.get('G0003', 0):>5} {G.get('G0019', 0):>5} {G.get('G0004', 0):>5} {G.get('G0005', 0):>5} {sum(R['comentarios'].values()):>5} "
            f"{R['check']['erros']:>5} {Cola.get('wrappers', '-'):>5} {('sim' if Cola.get('a_gerado') else 'nao') if Cola else '-':>4}")


def TextoTabela(Dados: dict) -> str:
    Cabeca = (f"{'header':<10} {'procs':>5} {'exp':>5} {'decl':>5} {'recus':>5} {'G0003':>5} {'G0019':>5} {'G0004':>5} {'G0005':>5} "
              f"{'cmt':>5} {'chk':>5} {'cola':>5} {'.a':>4}")
    return "\n".join([Cabeca] + [LinhaDaTabela(R) for R in Dados["resultados"].values()])


def PiorouContra(Antes: dict, Depois: dict) -> list:
    """Motivos pelos quais Depois piorou em relacao a Antes (menos procs ou mais erros no check)."""
    Motivos = []
    if Depois["procs"] < Antes["procs"]:
        Motivos.append(f"procs {Antes['procs']} -> {Depois['procs']}")
    if Depois["check"]["erros"] > Antes["check"]["erros"]:
        Motivos.append(f"erros do check {Antes['check']['erros']} -> {Depois['check']['erros']}")
    return Motivos


def LinhaAntesDepois(Nome: str, Antes: dict, Depois: dict) -> tuple:
    if "pulado" in Antes or "pulado" in Depois:
        return f"{Nome:<10} (pulado)", False
    GA, GD = Antes["recusas"], Depois["recusas"]
    Motivos = PiorouContra(Antes, Depois)
    Texto = (f"{Nome:<10} procs {Antes['procs']:>4}->{Depois['procs']:<4} recusas {Antes['total_recusas']:>3}->{Depois['total_recusas']:<3} "
             f"G0003 {GA.get('G0003', 0):>3}->{GD.get('G0003', 0):<3} G0019 {GA.get('G0019', 0):>3}->{GD.get('G0019', 0):<3} "
             f"G0004 {GA.get('G0004', 0):>3}->{GD.get('G0004', 0):<3} G0005 {GA.get('G0005', 0):>3}->{GD.get('G0005', 0):<3} "
             f"chk {Antes['check']['erros']:>3}->{Depois['check']['erros']:<3}")
    return Texto + (f"  PIOROU: {'; '.join(Motivos)}" if Motivos else ""), bool(Motivos)


def TextoAntesDepois(Base: dict, Dados: dict) -> tuple:
    Linhas, Piorou = [], False
    for Nome, Depois in Dados["resultados"].items():
        if Nome in Base["resultados"]:
            Linha, Pior = LinhaAntesDepois(Nome, Base["resultados"][Nome], Depois)
            Linhas.append(Linha)
            Piorou = Piorou or Pior
    return "\n".join(Linhas), Piorou


REGEX_LAYOUT = re.compile(r"^// layout (\S+) (\S+) tamanho=(\d+) alinhamento=(\d+)(.*)$", re.M)
BackendsDaProva = ("x64",)


def LerLayouts(Texto: str) -> list:
    """Comentarios '// layout NOME tipoC tamanho=N alinhamento=A campoForja:campoC=OFFSET ...' da ponte (ligar >= 0.5.0)."""
    Itens = []
    for Nome, TipoC, Tamanho, Alinhamento, Resto in REGEX_LAYOUT.findall(Texto):
        Campos = []
        for Par in Resto.split():
            Nomes, Offset = Par.rsplit("=", 1)
            Forja, C = Nomes.split(":", 1)
            Campos.append((Forja, C, int(Offset)))
        Itens.append({"nome": Nome, "tipo_c": TipoC.replace("~", " "), "tamanho": int(Tamanho), "alinhamento": int(Alinhamento), "campos": Campos})
    return Itens


def LayoutsMedidos(Layouts: list) -> list:
    """So o que o C nomeia: tira struct/union anonima (tipo '-') e membro gerado pelo ligar (nome C '-': armazenamento de bitfield, _anonN);
    esses ficam cobertos pelo static_assert da propria ponte."""
    return [dict(L, campos=[C for C in L["campos"] if C[1] != "-"]) for L in Layouts if L["tipo_c"] != "-"]


def NumerosEsperados(Layouts: list) -> list:
    """Sequencia (tamanho, alinhamento, offsets...) por struct, na ordem da ponte: a mesma que os 2 programas de prova imprimem."""
    Numeros = []
    for L in Layouts:
        Numeros += [L["tamanho"], L["alinhamento"]] + [O for _, _, O in L["campos"]]
    return Numeros


def ProvaForja(Layouts: list, Modulo: str) -> str:
    """Programa .fj que imprime size_of, align_of e offset_of (um por linha) de cada struct da ponte."""
    Linhas = [f'import "{Modulo}"', "", "proc main() {"]
    for L in Layouts:
        Tipo = f"{Modulo}.{L['nome']}"
        Linhas += [f"    print(size_of({Tipo}))", f"    print(align_of({Tipo}))"]
        Linhas += [f"    print(offset_of({Tipo}.{F}))" for F, _, _ in L["campos"]]
    return "\n".join(Linhas + ["}", ""])


def ProvaC(Layouts: list, Header: Path) -> str:
    """Programa C equivalente: sizeof, _Alignof e offsetof dos mesmos tipos, pelo gcc."""
    Linhas = ["#include <stddef.h>", "#include <stdio.h>", f'#include "{Header.as_posix()}"', "", "int main(void) {"]
    for L in Layouts:
        T = L["tipo_c"]
        Linhas.append(f'    printf("%zu\\n%zu\\n", sizeof({T}), (size_t)_Alignof({T}));')
        Linhas += [f'    printf("%zu\\n", offsetof({T}, {C}));' for _, C, _ in L["campos"]]
    return "\n".join(Linhas + ["    return 0;", "}", ""])


def NumerosDaSaida(Rodada: subprocess.CompletedProcess) -> tuple:
    """(numeros, erro): inteiros do stdout; erro textual se o programa falhou ou imprimiu algo que nao e inteiro."""
    if Rodada.returncode != 0:
        return [], f"exit {Rodada.returncode}: {(Rodada.stderr or Rodada.stdout)[:240].strip()}"
    try:
        return [int(L) for L in Rodada.stdout.split()], ""
    except ValueError:
        return [], f"saida nao numerica: {Rodada.stdout[:120]!r}"


def PrepararLibs(Pasta: Path, Link: str) -> None:
    """O 'link' da ponte precisa existir ao lado do .fj: copia lib<link>.a/.dll.a (e as .dll do vendor, p/ import lib); sem lib, cria um .a vazio
    (a prova so mede layout, nenhuma funcao da lib e chamada)."""
    Vendor_ = PastaLanguage / Vendor
    Achadas = [Vendor_ / f"lib{Link}.a", Vendor_ / f"lib{Link}.dll.a"]
    Achadas = [A for A in Achadas if A.is_file()]
    for Arquivo in Achadas:
        shutil.copy2(Arquivo, Pasta / Arquivo.name)
    if any(A.name.endswith(".dll.a") for A in Achadas):
        for Dll in Vendor_.glob("*.dll"):
            shutil.copy2(Dll, Pasta / Dll.name)
    if not Achadas:
        (Pasta / "vazio.c").write_text("int fj_vazio_layout;\n", encoding="utf-8")
        Rodar(["gcc", "-c", str(Pasta / "vazio.c"), "-o", str(Pasta / "vazio.o")])
        Rodar(["ar", "rcs", str(Pasta / f"lib{Link}.a"), str(Pasta / "vazio.o")])


def MedirNoForja(Forja: Path, Pasta: Path, Layouts: list, Modulo: str, Backend: str) -> tuple:
    Prova = Pasta / f"prova_{Backend}.fj"
    Prova.write_text(ProvaForja(Layouts, Modulo), encoding="utf-8", newline="\n")
    return NumerosDaSaida(Rodar([str(Forja), "run", "--backend", Backend, str(Prova)]))


def MedirNoGcc(Pasta: Path, Layouts: list, Header: Path, Incs: list) -> tuple:
    Fonte = Pasta / "prova_gcc.c"
    Fonte.write_text(ProvaC(Layouts, Header), encoding="utf-8", newline="\n")
    Exe = Pasta / "prova_gcc.exe"
    Compilacao = Rodar(["gcc", "-std=gnu11", "-w"] + [f"-I{P}" for P in Incs] + ["-o", str(Exe), str(Fonte)])
    if Compilacao.returncode != 0:
        return [], f"gcc: {Compilacao.stderr[:240].strip()}"
    return NumerosDaSaida(Rodar([str(Exe)]))


def PlantarOffset(Texto: str) -> str:
    """Plantio: soma 4 ao offset do ultimo campo da 1a struct com 2+ campos, SO no comentario (o static_assert fica certo)."""
    for Casamento in REGEX_LAYOUT.finditer(Texto):
        Pares = Casamento.group(5).split()
        if len(Pares) >= 2:
            Nomes, Offset = Pares[-1].rsplit("=", 1)
            Novo = Casamento.group(0).rsplit(Pares[-1], 1)[0] + f"{Nomes}={int(Offset) + 4}"
            return Texto.replace(Casamento.group(0), Novo, 1)
    return Texto


def Divergencias(Esperado: list, Medidos: dict) -> list:
    """Frases 'quem: primeiro indice que diverge' para cada medicao (gcc, forja x64) que difere da libclang."""
    Problemas = []
    for Quem, (Numeros, Erro) in Medidos.items():
        if Erro:
            Problemas.append(f"{Quem}: {Erro}")
            continue
        Indice = next((I for I, (A, B) in enumerate(zip(Esperado, Numeros)) if A != B), None)
        if Indice is not None or len(Numeros) != len(Esperado):
            Problemas.append(f"{Quem}: libclang {Esperado[Indice] if Indice is not None else '-'} x {Numeros[Indice] if Indice is not None else '-'} "
                             f"na posicao {Indice} (total {len(Esperado)} x {len(Numeros)})")
    return Problemas


def ConferirLayoutDoHeader(Item: dict, Forja: Path, Ligar: Path, Plantar: bool) -> dict:
    """Gera a ponte, le os '// layout' e compara libclang x gcc x Forja (backend x64); PULADO se nao deu para medir."""
    Header = Caminho(Item["header"])
    if not Header.is_file():
        return {"nome": Item["nome"], "pulado": Item.get("motivo", f"header nao encontrado em {Item['header']}")}
    Incs = [str(Caminho(P)) for P in Item["incs"]]
    Pasta = Path(tempfile.mkdtemp(prefix="bateria_layout_"))
    Ponte = Pasta / f"{Item['nome']}.fj"
    Comando = [str(Forja), "ligar", str(Header), "--link", Item["link"], "-o", str(Ponte)] + [f"-I{P}" for P in Incs]
    Rodada = Rodar(Comando, dict(os.environ, FORJA_LIGAR_SCRIPT=str(Ligar)))
    if not Ponte.is_file():
        return {"nome": Item["nome"], "pulado": f"ligar nao gerou a ponte (exit {Rodada.returncode})"}
    Texto = Ponte.read_text(encoding="utf-8")
    Layouts = LayoutsMedidos(LerLayouts(PlantarOffset(Texto) if Plantar else Texto))
    Esperado = NumerosEsperados(Layouts)
    PrepararLibs(Pasta, Item["link"])
    Medidos = {"gcc": MedirNoGcc(Pasta, Layouts, Header, Incs)}
    Medidos.update({f"forja-{B}": MedirNoForja(Forja, Pasta, Layouts, Item["nome"], B) for B in BackendsDaProva})
    LimparPasta(Pasta)
    return {"nome": Item["nome"], "estruturas": len(Layouts), "numeros": len(Esperado), "divergencias": Divergencias(Esperado, Medidos)}


def ExecutarLayout(A) -> int:
    Forja, Ligar = Path(A.forja), Path(A.ligar)
    Nomes = [N for N in A.so.split(",") if N] or [I["nome"] for I in CATALOGO if not I.get("so_cola")]
    Itens = [I for I in CATALOGO if I["nome"] in Nomes]
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as Pool:
        Resultados = list(Pool.map(lambda I: ConferirLayoutDoHeader(I, Forja, Ligar, A.plantar), Itens))
    Falhou = False
    for R in Resultados:
        if "pulado" in R:
            print(f"{R['nome']:<10} PULADO: {R['pulado']}")
            continue
        Situacao = "OK" if not R["divergencias"] else "DIVERGE: " + " | ".join(R["divergencias"])
        Falhou = Falhou or bool(R["divergencias"])
        print(f"{R['nome']:<10} estruturas {R['estruturas']:>4} numeros {R['numeros']:>5}  {Situacao}")
    return 1 if Falhou else 0


def CriarAnalisador() -> argparse.ArgumentParser:
    A = argparse.ArgumentParser(description="bateria de headers do forja ligar")
    A.add_argument("--forja", default=str(ForjaPadrao))
    A.add_argument("--ligar", default=str(LigarPadrao), help="forja_ligar.py a testar (FORJA_LIGAR_SCRIPT)")
    A.add_argument("--so", default="", help="nomes separados por virgula")
    A.add_argument("--json", action="store_true")
    A.add_argument("--gravar", help="grava o resultado neste JSON (linha de base)")
    A.add_argument("--comparar", help="JSON da linha de base: imprime antes/depois e sai 1 se algum header piorou")
    A.add_argument("--layout", action="store_true", help="confere size_of/align_of/offset_of: libclang x gcc x Forja (backend x64)")
    A.add_argument("--plantar", action="store_true", help="com --layout: adultera um offset de proposito; tem que sair 1 (prova do detector)")
    A.add_argument("--cola", action="store_true", help="roda o ligar com --cola --compilar-cola (0.6.0) e mede o que sobra de G0005/G0004")
    A.add_argument("--selftest", action="store_true")
    A.add_argument("--versao", action="store_true")
    return A


def Executar(A) -> int:
    Forja, Ligar = Path(A.forja), Path(A.ligar)
    Dados = {"bateria": VERSAO, "ligar": VersaoDoLigar(Forja, Ligar), "forja_md5": Md5Arquivo(Forja),
             "resultados": MedirTudo(Forja, Ligar, [N for N in A.so.split(",") if N], A.cola)}
    if A.gravar:
        Path(A.gravar).write_text(json.dumps(Dados, indent=1, ensure_ascii=False), encoding="utf-8", newline="\n")
    print(json.dumps(Dados, indent=1, ensure_ascii=False) if A.json else f"{Dados['ligar']} | forja md5 {Dados['forja_md5']}\n{TextoTabela(Dados)}")
    if not A.comparar:
        return 0
    Texto, Piorou = TextoAntesDepois(json.loads(Path(A.comparar).read_text(encoding="utf-8")), Dados)
    print("\nantes -> depois\n" + Texto)
    return 1 if Piorou else 0


def Selftest() -> int:
    Log = ('{"codigo": "G0019", "x": 1}\nlixo\n{"codigo": "G0003"}\n{"codigo": "G0019"}\n{"codigo": "E0101"}\n')
    Eventos = LinhasJson(Log)
    assert len(Eventos) == 4
    assert ContarCodigos(Eventos, "G") == {"G0003": 1, "G0019": 2}
    assert ContarCodigos(Eventos, "E") == {"E0101": 1}
    Ponte = "pub foreign {\n    link \"z\"\n    proc a(x: i32)\n    proc b() -> i32\n}\npub const K: i32 = 1\n"
    assert ContarProcsDoModulo(Ponte) == 2
    assert ContarComentariosRecusa("// recusado Foo G0006 bitfield\n// recusado Bar G0019 x\n// recusado Baz G0006 y\n") == {"G0006": 2, "G0019": 1}
    assert "let _v = m.K" in MontarStub(Ponte, "m") and "let _v" not in MontarStub("pub foreign {\n}\n", "m")
    Antes = {"procs": 10, "recusas": {}, "total_recusas": 0, "check": {"erros": 0}}
    assert PiorouContra(Antes, dict(Antes, procs=9)) and not PiorouContra(Antes, dict(Antes, procs=11))
    assert "PULADO" in LinhaDaTabela({"nome": "x", "pulado": "falta"})
    Ponte2 = "// layout P struct~p tamanho=16 alinhamento=8 a:a=0 b:bb=8\n// layout Q Q tamanho=4 alinhamento=4 x:x=0\n"
    Layouts = LerLayouts(Ponte2)
    assert Layouts[0]["tipo_c"] == "struct p" and Layouts[0]["campos"] == [("a", "a", 0), ("b", "bb", 8)]
    assert NumerosEsperados(Layouts) == [16, 8, 0, 8, 4, 4, 0]
    assert "offsetof(struct p, bb)" in ProvaC(Layouts, Path("h.h")) and "offset_of(m.P.b)" in ProvaForja(Layouts, "m")
    assert LerLayouts(PlantarOffset(Ponte2))[0]["campos"][1][2] == 12 and LerLayouts(PlantarOffset(Ponte2))[1]["campos"][0][2] == 0
    Gerado = LayoutsMedidos(LerLayouts("// layout A A tamanho=8 alinhamento=4 bits_0:-=0 x:x=4\n// layout B - tamanho=4 alinhamento=4\n"))
    assert len(Gerado) == 1 and Gerado[0]["campos"] == [("x", "x", 4)]
    assert Divergencias([1, 2], {"g": ([1, 2], ""), "f": ([1, 3], ""), "e": ([], "boom")}) == [
        "f: libclang 2 x 3 na posicao 1 (total 2 x 2)", "e: boom"]
    assert ContarWrappersDaCola("// cola C: x_cola.c (12 wrappers; --compilar-cola gera libx_cola.a)\n") == 12 and ContarWrappersDaCola("// x\n") == 0
    Item = {"nome": "n", "link": "l", "cola": True, "macros": "mods/m.sig", "incs": []}
    Comando = ComandoDoLigar(dict(Item, defs=["D1"]), Path("f.exe"), Path("h.h"), Path("o.fj"))
    assert "--cola" in Comando and "--compilar-cola" in Comando and "-DD1" in Comando and "--macros" in Comando
    assert "--cola" not in ComandoDoLigar(dict(Item, cola=False), Path("f.exe"), Path("h.h"), Path("o.fj"))
    Com = {"nome": "x", "procs": 1, "exp": 1, "decl": 1, "total_recusas": 0, "recusas": {}, "comentarios": {}, "check": {"erros": 0},
           "cola": {"wrappers": 3, "a_gerado": True}}
    assert LinhaDaTabela(Com).split()[-2:] == ["3", "sim"] and LinhaDaTabela(dict(Com, cola={})).split()[-2:] == ["-", "-"]
    print("selftest OK")
    return 0


def Principal() -> int:
    A = CriarAnalisador().parse_args()
    if A.versao:
        print(f"bateria_headers {VERSAO}")
        return 0
    if A.selftest:
        return Selftest()
    return ExecutarLayout(A) if A.layout else Executar(A)


if __name__ == "__main__":
    sys.exit(Principal())
