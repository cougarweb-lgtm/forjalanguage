#!/usr/bin/env python3
"""casos_linux.py - as tabelas de casos do alvo Linux (M5 P28), montadas SEM copiar os arquivos que ja existem.

Cada Caso e um programa .fj + argumentos, ambiente, arquivos de dados, preparo (sh), usuario e o que se espera dele no Linux (stdout, exit, trecho de stderr, estado
do arquivo depois). Fontes das tabelas:
  prova\\        6 programas novos, esperado em <nome>.esperado (+ .exit para exit(N), + .codigo para erro de execucao = exit 101 e o codigo no stderr);
  lowering\\     205/206/207/211/212/215 pelos .esperado/.codigo/.dev.* do verificar_lowering.py (importado, nao copiado);
  m5\\proc       args, env e exit com o esperado POR SO (sem o CommandLineToArgvW: no Linux o argv chega como veio);
  m5\\arquivo    copiar, erros, vazamento e linhas, com o errno do Linux (a<b.bin e not_found; EACCES real roda como nobody);
  --pasta      qualquer pasta de .fj: sem .esperado, o oraculo e a saida do x64-windows da mesma rodada.
Verificacao: python -I casos_linux.py --selftest   (monta as tabelas e confere contagens e que toda fonte existe)
Reversao: apagar este arquivo.

Versao: 0.6.1
Changelog: 0.6.1 (19/11/2026) D-G3-L2a - oraculo_c.txt passa de 13 para 9 nomes (os 4 casos de arquivo/erros saem: a fixture ganhou o case FileError.locked e a chave do C-musl selado sumiu); so o texto do docstring muda aqui.
           0.6.0 (18/11/2026) P3 linux_c - CasosDoOraculoC (os 13 nomes de oraculo_c.txt, dev e release) e IdentidadeDoCaso (chave do oraculo C congelado: args, env, arquivos por sha, pre, usuario, stdin).
           0.5.1 (17/11/2026) R8 passo 5 - Caso.OrigemOraculo (congelado | windows-mesma-rodada).
           0.5.0 (14/11/2026) M5 P29b - Caso ganha Teste, ComparaWin e OraculoWin (executor de testes: forja test -o, texto/--json/--filtro por ambiente); CasosTeste (tests\\nativo\\linux\\teste\\) e CasosFora (queda fora do executor = sinal 11).
           0.4.0 (14/11/2026) M5 P29a - Caso ganha Grupo (lote do WSL), Pulavel (X0003 = pulado) e ErroExato (stderr do x64-windows); o conjunto inteiro vive em conjunto_linux.py.
           0.3.0 (14/11/2026) M5 P28b-2b - CasosPasta le <caso>.stderr (trecho obrigatorio do stderr, em vez de exigir stderr vazio); o selftest confere as 10 fixtures de es (20 casos).
           0.2.0 (14/11/2026) M5 P28b-1 - Caso.Entrada; CasosPasta passa a ler <caso>.args, .env e .entrada, pular .fj sem `proc main` (modulos de apoio) e exigir stderr vazio quando ha .esperado;
           o selftest confere as 8 fixtures de tests\\nativo\\linux\\es (16 casos).
           0.1.0 (14/11/2026) M5 P28 - primeira versao.
"""
import hashlib
import importlib.util
import os
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
import wsl_lote  # noqa: E402

LANGUAGE = AQUI.parents[2]
TESTES = AQUI.parents[1]
MODOS = ("dev", "release")
CODIGO_FALHA_RUNTIME = 101
SAIDA_SINAL_11 = 128 + 11  # o shell devolve 128 + sinal
LOWERING_ALVO = ("205_", "206_", "207_", "211_", "212_", "215_")
MIB = 1024 * 1024


@dataclass
class Caso:
    Nome: str
    Fonte: Path
    Modo: str
    Args: list = field(default_factory=list)  # "@D@" = pasta de trabalho do programa no Linux
    Env: dict = field(default_factory=dict)
    Dados: dict = field(default_factory=dict)
    Pre: str = ""
    Pos: str = ""
    Usuario: str | None = None
    Esperado: bytes | None = None  # stdout; None = usar a saida do x64-windows (oraculo 2)
    Exit: int = 0
    ErroTem: str | None = None  # trecho obrigatorio do stderr
    ErroVazio: bool = False
    PosEsperado: bytes | None = None
    Musl: bool = False  # tambem comparar com o C-musl (lista oraculo_c.txt)
    Entrada: bytes | None = None  # stdin do programa (<caso>.entrada)
    Grupo: str = ""  # lote do WSL (uma chamada do wsl.exe por grupo; "" = o lote geral)
    Pulavel: bool = False  # P29a: recusa X0003 do alvo Linux (foreign com link/dll) = PULADO por desenho, nao falha
    ErroExato: bytes | None = None  # P29a: stderr do x64-windows da mesma rodada (CRLF normalizado), comparado byte a byte
    Teste: dict | None = None  # P29b: caso do `forja test` (Modo "teste"): {"json": bool, "filtro": str|None, "alvo": "std:<m>"|None}; o ELF e o exe de teste (forja test -o)
    ComparaWin: bool = False  # P29b: o `forja test` do Windows (mesmo texto de entrada) e o oraculo: stdout, exit e stderr (so os campos do runtime) iguais
    OraculoWin: tuple | None = None  # P29b: (stdout, stderr normalizado, exit) do `forja test` do Windows, preenchido pelo verificar_linux
    OrigemOraculo: str = ""  # R8: "congelado" ou "windows-mesma-rodada" (so com --oraculo-congelado); "" = o esperado do proprio caso ou sem congelado


def Carregar(Caminho: Path, Apelido: str):
    Especificacao = importlib.util.spec_from_file_location(Apelido, Caminho)
    Modulo = importlib.util.module_from_spec(Especificacao)
    sys.modules[Apelido] = Modulo
    Especificacao.loader.exec_module(Modulo)
    return Modulo


def ListaMusl() -> set:
    Linhas = (AQUI / "oraculo_c.txt").read_text(encoding="utf-8").splitlines()
    return {L.strip() for L in Linhas if L.strip() and not L.startswith("#")}


def CasosProva(Musl: set) -> list:
    Argumentos = {"arquivo": ["@D@"], "saida": ["um", "dois tres", "ação"]}
    Ambiente = {"saida": {"FORJA_PROVA": "vv"}}
    Casos = []
    for Fonte in sorted((AQUI / "prova").glob("*.fj")):
        Base = Fonte.with_suffix("")
        Exit = int(Base.with_suffix(".exit").read_text()) if Base.with_suffix(".exit").is_file() else 0
        for Modo in MODOS:
            Raiz = str(Base) + (".dev" if Modo == "dev" and Path(str(Base) + ".dev.esperado").is_file() else "")  # como o verificar_lowering
            Codigo = Path(Raiz + ".codigo").read_text().strip() if Path(Raiz + ".codigo").is_file() else None
            Casos.append(Caso(f"prova/{Fonte.stem}", Fonte, Modo, Argumentos.get(Fonte.stem, []), Ambiente.get(Fonte.stem, {}),
                              Esperado=Path(Raiz + ".esperado").read_bytes(), Exit=CODIGO_FALHA_RUNTIME if Codigo else Exit,
                              ErroTem=f'"{Codigo}"' if Codigo else None, Musl=f"prova/{Fonte.stem}" in Musl))
    return Casos


def CasosLowering(Musl: set) -> list:
    Lowering = Carregar(TESTES / "lowering" / "verificar_lowering.py", "verificar_lowering_linux")
    Casos = []
    for Fonte in sorted((TESTES / "lowering").glob("[0-9][0-9]*_*.fj")):
        if not Fonte.name.startswith(LOWERING_ALVO):
            continue
        Base = Lowering.MontarCaso(Fonte)
        for Modo in MODOS:
            Codigo = Lowering.CodigoEsperadoNoModo(Base, Modo)
            Casos.append(Caso(f"lowering/{Fonte.stem}", Fonte, Modo, Esperado=Lowering.StdoutEsperadoNoModo(Base, Modo).encode("utf-8"),
                              Exit=CODIGO_FALHA_RUNTIME if Codigo else 0, ErroTem=f'"{Codigo}"' if Codigo else None, Musl=f"lowering/{Fonte.stem}" in Musl))
    return Casos


def CasosProc() -> list:
    Pasta = TESTES / "nativo" / "m5" / "proc"
    Args = ["", "a b c", 'a"b', "ação coração", "😀 x", "a\\b", "a\tb", "x" * 5000, "'q'"]
    Esperado = f"n={len(Args) + 1}\n" + "".join(f"[{I}] ({len(A.encode())}) {A}\n" for I, A in enumerate(Args, 1))
    Env = {"FJ_CHEIA": "héllo", "FJ_VAZIA": "", "FJ_ÇÃO": "ação ✓", "FJ_LONGA": "x" * 5000}
    EsperadoEnv = ["FJ_CHEIA=<héllo> (6)", "FJ_VAZIA=<> (0)", "FJ_AUSENTE=none", "fj_cheia=none",
                   "FJ_ÇÃO=<ação ✓> (" + str(len("ação ✓".encode())) + ")", "FJ_LONGA=<" + "x" * 5000 + "> (5000)"]
    Saidas = [("sair_0", 0, b"antes\n", None), ("sair_7", 7, b"antes\n", None), ("sair_255", 255, b"antes\n", None),
              ("sair_256", 101, b"antes\n", "R0012"), ("sair_menos1", 101, b"antes\n", "R0012"),
              ("sair_pendente", 2, "".join(f"{K}\n" for K in range(300)).encode(), None), ("sair_heap", 4, b"5\n", None)]
    Casos = []
    for Modo in MODOS:
        Casos.append(Caso("proc/args_eco", Pasta / "args_eco.fj", Modo, Args, Esperado=Esperado.encode(), ErroVazio=True))
        Casos.append(Caso("proc/env_eco", Pasta / "env_eco.fj", Modo, ["FJ_CHEIA", "FJ_VAZIA", "FJ_AUSENTE", "fj_cheia", "FJ_ÇÃO", "FJ_LONGA"], Env,
                          Esperado=("\n".join(EsperadoEnv) + "\n").encode(), ErroVazio=True))
        for Nome, Codigo, Saida, Trecho in Saidas:
            Casos.append(Caso(f"proc/{Nome}", Pasta / f"{Nome}.fj", Modo, Esperado=Saida, Exit=Codigo, ErroTem=Trecho, ErroVazio=Trecho is None))
    return Casos


def Padrao(Tamanho: int) -> bytes:
    """Os 256 valores de byte repetidos, com as sequencias que um modo texto traduziria (a mesma do verificar_arquivo)."""
    Base = bytes(range(256)) + b"\r\n\n\r\x1a\x00\r\r\n"
    return (Base * (Tamanho // len(Base) + 1))[:Tamanho]


def CasosCopiar(Pasta: Path) -> list:
    Tamanhos = [("0B", b"", "entrada.bin"), ("1B", b"\x00", "entrada.bin"), ("1MiB", Padrao(MIB), "entrada.bin"), ("256+CRLF", Padrao(4096), "entrada.bin"),
                ("acento", Padrao(1000), "entrada ç ã.bin")]
    Casos = []
    for Rotulo, Conteudo, Nome in Tamanhos:
        for Modo in MODOS:
            Casos.append(Caso(f"arquivo/copiar {Rotulo}", Pasta / "copiar.fj", Modo, [f"@D@/{Nome}", "@D@/saida.bin"], Dados={Nome: Conteudo},
                              Pos=f'cmp "$D/{Nome}" "$D/saida.bin" && echo igual', PosEsperado=b"igual\n",
                              Esperado=f"n={len(Conteudo)} iguais=true\n".encode(), ErroVazio=True))
    return Casos


def CasosErros(Pasta: Path) -> list:
    Dados = {"existe.bin": b"12345", "maior.bin": b"x" * 100, "so leitura.bin": b"intacto", "ilegivel.bin": b"segredo"}
    Pre = 'mkdir -p "$D/pasta"; chmod 444 "$D/so leitura.bin"; chmod 000 "$D/ilegivel.bin"'
    Tabela = [  # rotulo, modo, caminho, esperado, usuario, Pos, PosEsperado
        ("r inexistente", "r", "@D@/nao existe.bin", "not_found 0", None, "", None),
        ("r pasta inexistente", "r", "@D@/nao_existe_pasta/x.bin", "not_found 0", None, "", None),
        ("r caminho vazio", "r", "", "not_found 0", None, "", None),
        ("r nome a<b (diferenca de SO: no Linux e so um nome)", "r", "@D@/a<b.bin", "not_found 0", None, "", None),
        ("r arquivo usado como pasta (ENOTDIR)", "r", "@D@/existe.bin/x.bin", "not_found 0", None, "", None),
        ("r componente longo (ENAMETOOLONG)", "r", "@D@/" + "a" * 300, "not_found 0", None, "", None),
        ("r pasta no lugar do arquivo (EISDIR)", "r", "@D@/pasta", "no_permission 0", None, "", None),
        ("r arquivo existente", "r", "@D@/existe.bin", "ok 5", None, "", None),
        ("r sem permissao de leitura (EACCES, nobody)", "r", "@D@/ilegivel.bin", "no_permission 0", "nobody", "", None),
        ("w pasta no lugar do arquivo (EISDIR)", "w", "@D@/pasta", "no_permission 0", None, "", None),
        ("w destino somente leitura (EACCES, nobody)", "w", "@D@/so leitura.bin", "no_permission 0", "nobody", 'cat "$D/so leitura.bin"', b"intacto"),
        ("w pasta inexistente", "w", "@D@/nao_existe_pasta/x.bin", "not_found 0", None, "", None),
        ("w arquivo novo", "w", "@D@/novo.bin", "ok 0", None, 'od -An -tx1 -v "$D/novo.bin"', b" 01 02 03\n"),
        ("w sobre arquivo maior (trunca)", "w", "@D@/maior.bin", "ok 0", None, 'od -An -tx1 -v "$D/maior.bin"', b" 01 02 03\n"),
    ]
    return [Caso(f"arquivo/erros {Rotulo}", Pasta / "erros.fj", Modo, [Operacao, Caminho], Dados=Dados, Pre=Pre, Usuario=Usuario, Pos=Pos, PosEsperado=PosEsperado,
                 Esperado=(Texto + "\n").encode(), ErroVazio=True)
            for Rotulo, Operacao, Caminho, Texto, Usuario, Pos, PosEsperado in Tabela for Modo in MODOS]


def CasosVazamentoELinhas(Pasta: Path, Arquivo) -> list:
    Casos = []
    for Modo in MODOS:
        Dev = Modo == "dev"
        Casos.append(Caso("arquivo/vazamento", Pasta / "vazamento.fj", Modo, ["@D@/v.bin"], Dados={"v.bin": Padrao(5000)}, Esperado=b"n=5000\n",
                          Exit=CODIGO_FALHA_RUNTIME if Dev else 0, ErroTem="R0009" if Dev else None, ErroVazio=not Dev))
        for Rotulo, Conteudo, Saida in Arquivo.CasosLinhas():
            Casos.append(Caso(f"arquivo/linhas {Rotulo}", Pasta / "linhas.fj", Modo, ["@D@/linhas.txt"], Dados={"linhas.txt": Conteudo}, Esperado=Saida.encode(), ErroVazio=True))
        Casos.append(Caso("arquivo/linhas inexistente", Pasta / "linhas.fj", Modo, ["@D@/nao existe.txt"], Esperado=b"erro\n", ErroVazio=True))
    return Casos


def CasosArquivo() -> list:
    Pasta = TESTES / "nativo" / "m5" / "arquivo"
    Arquivo = Carregar(Pasta / "verificar_arquivo.py", "verificar_arquivo_linux")
    return CasosCopiar(Pasta) + CasosErros(Pasta) + CasosVazamentoELinhas(Pasta, Arquivo)


def LerLinhas(Arquivo: Path) -> list:
    return Arquivo.read_text(encoding="utf-8").splitlines() if Arquivo.is_file() else []


def ArgumentosDoCaso(Base: Path) -> list:
    """<caso>.args: um argumento por linha; @D@ = pasta de trabalho do programa no Linux, @T@ = a pasta temporaria do Windows vista pelo WSL (/mnt/c/...)."""
    Temporaria = wsl_lote.CaminhoWsl(tempfile.gettempdir())
    return [Linha.replace("@T@", Temporaria) for Linha in LerLinhas(Base.with_suffix(".args"))]


def CasosPasta(Pasta: Path) -> list:
    """Cada .fj da pasta que tem `proc main` (modulos de apoio ficam de fora); com .esperado/.codigo/.exit vale o do arquivo, senao o oraculo e o x64-windows.
    Arquivos ao lado do caso: .args (argumentos), .env (NOME=valor por linha) e .entrada (bytes do stdin). Com .esperado o stderr tem de ficar vazio (exceto com .codigo)."""
    Casos = []
    for Fonte in sorted(Pasta.glob("*.fj")):
        if "proc main(" not in Fonte.read_text(encoding="utf-8"):
            continue
        Base = Fonte.with_suffix("")
        Codigo = Base.with_suffix(".codigo").read_text().strip() if Base.with_suffix(".codigo").is_file() else None
        Esperado = Base.with_suffix(".esperado").read_bytes() if Base.with_suffix(".esperado").is_file() else None
        Entrada = Base.with_suffix(".entrada").read_bytes() if Base.with_suffix(".entrada").is_file() else None
        ErroDoCaso = Base.with_suffix(".stderr").read_text(encoding="utf-8") if Base.with_suffix(".stderr").is_file() else None
        Ambiente = dict(Linha.split("=", 1) for Linha in LerLinhas(Base.with_suffix(".env")))
        for Modo in MODOS:
            Casos.append(Caso(f"pasta/{Fonte.stem}", Fonte, Modo, ArgumentosDoCaso(Base), Ambiente, Esperado=Esperado, Exit=CODIGO_FALHA_RUNTIME if Codigo else 0,
                              ErroTem=f'"{Codigo}"' if Codigo else ErroDoCaso, ErroVazio=Esperado is not None and Codigo is None and ErroDoCaso is None, Entrada=Entrada))
    return Casos


def AmbienteDoTeste(Json: bool, Filtro: str | None) -> dict:
    """O `forja test --json --filtro X` do Windows passa FORJA_TESTE_JSON e FORJA_TESTE_FILTRO ao exe (OpcoesAmbienteDoTeste); o executor Linux so le o ambiente."""
    Ambiente = {"FORJA_TESTE_JSON": "1"} if Json else {}
    return {**Ambiente, "FORJA_TESTE_FILTRO": Filtro} if Filtro else Ambiente


def EntradaRelativa(Fonte: Path) -> str:
    """O caminho do .fj como o `forja test` o recebe (relativo a language\\, barras normais): o arquivo das linhas `expect falhou` e do --json e esse texto, igual nos dois alvos."""
    return os.path.relpath(Fonte, LANGUAGE).replace("\\", "/")  # examples\ fica fora de language\: "../examples/x.fj"


def VariantesDoTeste(Base: Path) -> list:
    """[(rotulo do nome, json, filtro, arquivo do .esperado)]: texto, --json e, com <caso>.filtro, --filtro."""
    Variantes = [("", False, None, Base.with_suffix(".esperado")), (" json", True, None, Base.with_suffix(".json.esperado"))]
    if Base.with_suffix(".filtro").is_file():
        Trecho = Base.with_suffix(".filtro").read_text(encoding="utf-8").strip()
        Variantes.append((" filtro", False, Trecho, Base.with_suffix(".filtro.esperado")))
    return Variantes


def CasosTeste(Pasta: Path) -> list:
    """tests\\nativo\\linux\\teste\\: cada .fj (menos os fora_*) vira os casos do executor de testes: texto, --json e --filtro (se ha <caso>.filtro). Arquivos ao lado:
    .esperado/.json.esperado/.filtro.esperado (stdout), .exit (padrao 1: ha teste que falha), .stderr (trecho obrigatorio), .so_linux (diferenca de SO: sem comparar com o Windows)."""
    Casos = []
    for Fonte in sorted(Pasta.glob("*.fj")):
        if Fonte.name.startswith("fora_"):
            continue
        Base = Fonte.with_suffix("")
        Saida = int(Base.with_suffix(".exit").read_text()) if Base.with_suffix(".exit").is_file() else 1
        Erro = Base.with_suffix(".stderr").read_text(encoding="utf-8") if Base.with_suffix(".stderr").is_file() else None
        for Rotulo, Json, Filtro, Esperado in VariantesDoTeste(Base):
            Casos.append(Caso(f"teste/{Fonte.stem}{Rotulo}", Fonte, "teste", Env=AmbienteDoTeste(Json, Filtro), Esperado=Esperado.read_bytes(), Exit=Saida, ErroTem=Erro,
                              Grupo="teste", Teste={"json": Json, "filtro": Filtro, "alvo": None}, ComparaWin=not Base.with_suffix(".so_linux").is_file()))
    return Casos


def CasosFora(Pasta: Path, Musl: set) -> list:
    """fora_*.fj: programa NORMAL (forja build, dev e release) que cai por acesso invalido: sem tratador fora do executor de testes, o kernel mata com o sinal 11 (exit 139 no shell), como o C do oraculo."""
    return [Caso(f"teste/{Fonte.stem}", Fonte, Modo, Esperado=b"", Exit=SAIDA_SINAL_11, Musl=f"teste/{Fonte.stem}" in Musl)
            for Fonte in sorted(Pasta.glob("fora_*.fj")) for Modo in MODOS]


def TodosOsCasos() -> list:
    Musl = ListaMusl()
    Casos = CasosProva(Musl) + CasosLowering(Musl) + CasosProc() + CasosArquivo()
    for Caso_ in Casos:
        Caso_.Musl = Caso_.Nome in Musl
    return Casos


def CasosDoOraculoC() -> list:
    """Os casos da lista oraculo_c.txt (9 nomes desde o D-G3-L2a, cada um em dev e release): prova, lowering e teste/fora_queda (o fora fica em AQUI\\teste); os 4 erros de arquivo sairam da lista."""
    Musl = ListaMusl()
    Casos = [C for C in CasosProva(Musl) + CasosLowering(Musl) + CasosArquivo() + CasosFora(AQUI / "teste", Musl) if C.Nome in Musl]
    for Caso_ in Casos:
        Caso_.Musl = True  # CasosArquivo nao marca Musl (so TodosOsCasos marca): todo caso desta lista e comparado com o C-musl
    return Casos


def ShaDe(Dado: bytes) -> str:
    return hashlib.sha256(Dado).hexdigest()


def IdentidadeDoCaso(Caso_: Caso) -> list:
    """O que o programa recebe, como lista JSON para a chave do oraculo C congelado: args, env, preparo, usuario, stdin e os arquivos de dados (so o sha)."""
    Entrada = None if Caso_.Entrada is None else ShaDe(Caso_.Entrada)
    return [*Caso_.Args, {"env": Caso_.Env, "dados": {Nome: ShaDe(Conteudo) for Nome, Conteudo in Caso_.Dados.items()},
                          "pre": Caso_.Pre, "usuario": Caso_.Usuario, "entrada": Entrada}]


def Selftest() -> int:
    Casos = TodosOsCasos()
    Faltam = [C.Nome for C in Casos if not C.Fonte.is_file()]
    Duplicados = len(Casos) - len({(C.Nome, C.Modo) for C in Casos})
    Esperadas = {"prova": 12, "lowering": 12, "proc": 18}
    Contagens = {Chave: sum(1 for C in Casos if C.Nome.startswith(Chave + "/")) for Chave in Esperadas}
    Ruins = {Chave: Valor for Chave, Valor in Contagens.items() if Valor != Esperadas[Chave]}
    Es = CasosPasta(AQUI / "es")
    EsRuins = [C.Nome for C in Es if C.Esperado is None]
    Oraculo = CasosDoOraculoC()
    OraculoRuim = {C.Nome for C in Oraculo} != ListaMusl() or len(Oraculo) != 2 * len(ListaMusl())  # 13 nomes x (dev, release)
    print(f"casos_linux selftest: {len(Casos)} casos; contagens {Contagens}; sem fonte {Faltam}; duplicados {Duplicados}; es: {len(Es)} casos, sem .esperado {EsRuins}; oraculo_c: {len(Oraculo)} casos")
    return 1 if Faltam or Duplicados or Ruins or len(Es) != 20 or EsRuins or OraculoRuim else 0


if __name__ == "__main__":
    sys.exit(Selftest() if "--selftest" in sys.argv else 2)
