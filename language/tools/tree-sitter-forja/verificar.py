# verificar.py - gera o parser tree-sitter da Forja, compila a DLL e confere os .fj validos do projeto.
# Objetivo: provar que a gramatica (grammar.js + src/scanner.c) aceita TODO .fj que o compilador aceita (ZERO nos ERROR/MISSING).
# Versao: 0.8.0
# Changelog: 0.8.0 - so a versao (acompanha grammar.js 0.9.0: clausula de posse 'ends'; sem mudanca de logica).
#            0.7.0 - so a versao (acompanha grammar.js 0.8.0: clausula de posse creates/destroys/takes; sem mudanca de logica).
#            0.6.0 - opcao --forja <exe> (compilador usado como criterio de validade; default continua o caminho antigo).
#            0.5.0 - so a versao (acompanha grammar.js 0.5.0: static_assert de topo, [dynamic, N]T; sem mudanca de logica).
#            0.4.0 - so a versao (acompanha grammar.js 0.4.0: map, bit_set, soa, matrix, indice duplo; sem mudanca de logica).
#            0.3.0 - so a versao (acompanha grammar.js 0.3.0: opaque, valor de enum; sem mudanca de logica).
#            0.2.0 - inclui language/std; arquivo em pasta 'mods' (ou em std) vale por 'forja ast' (modulo sozinho nao e programa).
#            0.1.0 - primeira versao.
# Uso: python verificar.py [--sem-gerar] [--detalhe] [--forja <exe>]
#   --sem-gerar  nao roda 'tree-sitter generate' (usa src/parser.c ja gerado)
#   --detalhe    imprime a linha com erro de cada arquivo que falhar
# Depende de: tree_sitter (pip), gcc (MinGW), node + tree-sitter-cli local (npm install, na propria pasta).
# Criterio de validade: C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language\compiler\forja.exe check <arq> com saida 0 (err_* ficam de fora sozinhos);
#   arquivo dentro de pasta 'mods' ou de language\std: forja ast <arq> com saida 0.
# Reversao: apagar a pasta tree-sitter-forja e as linhas "forja" do indexador.py do CodeGraph.

import argparse
import ctypes
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PASTA = Path(__file__).resolve().parent
DLL = PASTA / "build" / "forja.dll"
FORJA_EXE = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language\compiler\forja.exe")
RAIZ_PROJETO = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA")
PASTA_STD = RAIZ_PROJETO / "language" / "std"
PASTAS_FJ = [RAIZ_PROJETO / "language" / "tests", PASTA_STD, RAIZ_PROJETO / "examples", RAIZ_PROJETO / "benchmark"]


def GerarParser() -> None:
    Binario = PASTA / "node_modules" / ".bin" / "tree-sitter.cmd"
    Resultado = subprocess.run([str(Binario), "generate"], cwd=PASTA, capture_output=True, text=True)
    if Resultado.returncode != 0:
        raise RuntimeError(f"tree-sitter generate falhou:\n{Resultado.stdout}\n{Resultado.stderr}")


def CompilarDll() -> None:
    DLL.parent.mkdir(exist_ok=True)
    Comando = ["gcc", "-shared", "-O2", "-o", str(DLL), "src/parser.c", "src/scanner.c", "-I", "src"]
    Resultado = subprocess.run(Comando, cwd=PASTA, capture_output=True, text=True)
    if Resultado.returncode != 0:
        raise RuntimeError(f"gcc falhou:\n{Resultado.stderr}")


def CarregarLinguagem():
    from tree_sitter import Language
    Biblioteca = ctypes.cdll.LoadLibrary(str(DLL))
    Biblioteca.tree_sitter_forja.restype = ctypes.c_void_p
    Ponteiro = Biblioteca.tree_sitter_forja()
    # PyCapsule "tree_sitter.Language" e a forma sem aviso de depreciacao no py-tree-sitter 0.25
    ctypes.pythonapi.PyCapsule_New.restype = ctypes.py_object
    ctypes.pythonapi.PyCapsule_New.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]
    return Language(ctypes.pythonapi.PyCapsule_New(Ponteiro, b"tree_sitter.Language", None))


def ListarFj() -> list[Path]:
    Arquivos: list[Path] = []
    for Pasta in PASTAS_FJ:
        if Pasta.exists():
            Arquivos.extend(sorted(Pasta.rglob("*.fj")))
    return Arquivos


def CompiladorAceita(Arquivo: Path) -> bool:
    EhModulo = "mods" in Arquivo.parent.parts or PASTA_STD in Arquivo.parents
    Comando = "ast" if EhModulo else "check"
    Resultado = subprocess.run([str(FORJA_EXE), Comando, str(Arquivo)], capture_output=True, cwd=Arquivo.parent)
    return Resultado.returncode == 0


def PrimeiroErro(No):
    """Primeiro no ERROR/MISSING em ordem de documento (None se nao houver)."""
    if No.type == "ERROR" or No.is_missing:
        return No
    for Filho in No.children:
        if Filho.has_error or Filho.is_missing:
            Achado = PrimeiroErro(Filho)
            if Achado is not None:
                return Achado
    return None


def ConferirConsultas(Linguagem, Analisador) -> int:
    """Compila queries/*.scm e roda em examples/01_ola.fj; devolve 1 se alguma nao compilar."""
    from tree_sitter import Query, QueryCursor
    Fonte = (RAIZ_PROJETO / "examples" / "04_defer_e_testes.fj").read_bytes()
    Raiz = Analisador.parse(Fonte).root_node
    Status = 0
    for Nome in ("highlights", "tags"):
        try:
            Consulta = Query(Linguagem, (PASTA / "queries" / f"{Nome}.scm").read_text(encoding="utf-8"))
        except Exception as Erro:
            print(f"consulta {Nome}.scm NAO compila: {Erro}")
            Status = 1
            continue
        Capturas = QueryCursor(Consulta).captures(Raiz)
        print(f"consulta {Nome}.scm compila; capturas em 04_defer_e_testes.fj: " +
              ", ".join(f"{K}={len(V)}" for K, V in sorted(Capturas.items())))
    return Status


def Main() -> int:
    global FORJA_EXE
    Argumentos = argparse.ArgumentParser()
    Argumentos.add_argument("--consultas", action="store_true", help="tambem compila e roda highlights.scm e tags.scm")
    Argumentos.add_argument("--sem-gerar", action="store_true")
    Argumentos.add_argument("--detalhe", action="store_true")
    Argumentos.add_argument("--forja", default=str(FORJA_EXE), help="forja.exe usado como criterio de validade")
    Opcoes = Argumentos.parse_args()
    FORJA_EXE = Path(Opcoes.forja)
    if not Opcoes.sem_gerar:
        GerarParser()
    CompilarDll()
    from tree_sitter import Parser
    Linguagem = CarregarLinguagem()
    Analisador = Parser(Linguagem)
    Status = ConferirConsultas(Linguagem, Analisador) if Opcoes.consultas else 0

    Todos = ListarFj()
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Validos = [A for A, Ok in zip(Todos, Pool.map(CompiladorAceita, Todos)) if Ok]
    Falhas = []
    for Arquivo in Validos:
        Dados = Arquivo.read_bytes()
        Arvore = Analisador.parse(Dados)
        if Arvore.root_node.has_error:
            No = PrimeiroErro(Arvore.root_node)
            Linha = No.start_point[0] if No is not None else 0
            Texto = Dados.decode("utf-8", "replace").splitlines()[Linha] if Dados else ""
            Falhas.append((Arquivo, Linha + 1, Texto))
    print(f"arquivos .fj encontrados: {len(Todos)} | validos pelo compilador: {len(Validos)} | "
          f"rejeitados de proposito (ignorados): {len(Todos) - len(Validos)}")
    print(f"falhas no tree-sitter (ERROR/MISSING): {len(Falhas)}")
    for Arquivo, Linha, Texto in Falhas:
        print(f"  FALHA {Arquivo} linha {Linha}" + (f": {Texto.strip()[:100]}" if Opcoes.detalhe else ""))
    return 1 if (Falhas or Status) else 0


if __name__ == "__main__":
    sys.exit(Main())
