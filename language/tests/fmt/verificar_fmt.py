"""Verifica o 'forja fmt' (F1 P1a nucleo, P1b arquivo, P3 provas completas). Tudo roda em copias em %TEMP%: o repositorio so e lido.

Provas:
  - fixtures: tests\\fmt\\casos\\<nome>.antes.fj -> 'forja fmt --stdin' tem de dar byte a byte <nome>.depois.fj; e fmt(depois) == depois.
  - sinteticos: BOM + CRLF, arquivo vazio, so comentario sem LF, so espacos, erro de sintaxe (exit 1, stdout vazio, JSON no stderr), token de 65535 bytes, uso errado (exit 2).
  - arquivo (dry-run): 'forja fmt <arq>' sobre as fixtures (antes = exit 1 e intacto; depois = exit 0 sem diff).
  - negativo em arquivo: token de 65535 bytes = nao_analisado no --json, exit 1, arquivo intacto.
  - corpus (std, vendor, exemplos_libs, examples, engine\\forja, tests\\lowering, tests\\semantica ok_*, benchmark\\corpus), para cada .fj que o fmt aceita:
    (a) idempotencia: fmt(fmt(x)) == fmt(x), 0 trocas;
    (b) equivalencia do AST: 'forja ast --sem-pos' antes e depois, saida textual inteira igual;
    (c) texto: igual antes e depois sem espacos e quebras, fora de string e comentario. Codigo sai sem espaco nenhum; string sai byte a byte; comentario sai byte a byte sem o espaco do fim (R4). Escolha: o AST nao ve comentario nem o texto de string, e o lexer da Forja so tem comentario de linha (// ; /* da E0110) e string de uma linha; a comparacao por segmento cobre o que o AST deixa passar.
  - execucao (d): amostra fixa de 40 programas (35 de tests\\lowering com .esperado e sem .codigo, espacados pelo nome; mais os .fj de examples, sem .esperado: vale o original). A copia formatada roda com o mesmo exit e stdout do original e do .esperado.
Pulados (fmt recusa, ast nao le, original nao roda igual) sao contados em separado e NAO sao falha.

Uso: python tests\\fmt\\verificar_fmt.py --forja compiler\\forja.exe [--listar] | --selftest
Versao: 0.4.2
Changelog: 0.4.2 - 0.21.0 F4 passo 3: sai o c_igual (build --modo c da copia formatada contra o do original): o backend C foi aposentado. A prova (d) segue com exit e stdout iguais, no x64.
           0.4.1 - F1 U1: examples entra no portao canonico (os 2 arquivos migrados com autorizacao do usuario).
           0.4.0 - F1 U1: +portao canonico (VerificarPortaoCanonico): 'forja fmt <arq>' em dry-run nas pastas migradas (std, engine\\forja, tests\\lowering, tests\\semantica ok_*) tem de sair 0; nao_analisado (std\\fmt\\date.fj) nao e falha.
           0.3.0 - F1 P3: +(c) texto sem espaco fora de string e comentario; +(d) execucao de 40 programas (run e build --modo c); +negativo de token grande em arquivo; corpus com vendor, engine\\forja e benchmark\\corpus; resumo por prova (a), (b), (c).
           0.2.0 - F1 P1b: +1 conferencia (VerificarArquivoFmt: 'forja fmt <arq>' em dry-run sobre as fixtures, antes = exit 1 e arquivo intacto, depois = exit 0 sem diff).
           0.1.0 - F1 P1a: primeira versao (fixtures, sinteticos, corpus em paralelo, medicao por pasta e por regra).
Verificacao: python tests\\fmt\\verificar_fmt.py --selftest && python tests\\fmt\\verificar_fmt.py --forja compiler\\forja.exe
"""
import argparse
import collections
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading

RAIZ_LINGUAGEM = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAIZ_PROJETO = os.path.dirname(RAIZ_LINGUAGEM)
PASTA_CASOS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "casos")
PASTA_LOWERING = os.path.join(RAIZ_LINGUAGEM, "tests", "lowering")
PASTA_EXEMPLOS = os.path.join(RAIZ_PROJETO, "examples")
TEMPO_MAX = 60
TEMPO_EXECUCAO = 120
TAMANHO_AMOSTRA_LOWERING = 35
ESPACOS = " \t\r\n﻿"  # ﻿ = BOM, que o fmt tira (R1)
FONTE_TOKEN_GRANDE = b'const A: str = "' + b"x" * 70000 + b'"\n'


def RodarForja(Forja, Argumentos, Entrada=None):
    """Roda o forja e devolve (codigo, stdout em bytes, stderr em bytes)."""
    Proc = subprocess.run([Forja] + Argumentos, input=Entrada, capture_output=True, timeout=TEMPO_MAX)
    return Proc.returncode, Proc.stdout, Proc.stderr


def LerRegras(Stderr):
    """'desvios=N primeiro=L:C:regra regras=a:1,b:2' (stderr do --regras) -> (desvios, {regra: n}); sem a linha: (None, {})."""
    for Linha in Stderr.decode("utf-8", "replace").splitlines():
        if not Linha.startswith("desvios="):
            continue
        Campos = dict(Campo.split("=", 1) for Campo in Linha.split(" ") if "=" in Campo)
        Regras = {}
        for Par in Campos.get("regras", "").split(","):
            if ":" in Par:
                Nome, Qtd = Par.split(":")
                Regras[Nome] = int(Qtd)
        return int(Campos["desvios"]), Regras
    return None, {}


def VerificarFixtures(Forja):
    """Cada <nome>.antes.fj vira <nome>.depois.fj, e o depois e canonico. Devolve (total, falhas)."""
    Falhas = []
    Nomes = sorted(Arq[: -len(".antes.fj")] for Arq in os.listdir(PASTA_CASOS) if Arq.endswith(".antes.fj"))
    for Nome in Nomes:
        Antes = open(os.path.join(PASTA_CASOS, Nome + ".antes.fj"), "rb").read()
        Depois = open(os.path.join(PASTA_CASOS, Nome + ".depois.fj"), "rb").read()
        Codigo, Saida, Erro = RodarForja(Forja, ["fmt", "--stdin"], Antes)
        if Codigo != 0 or Saida != Depois:
            Falhas.append(f"{Nome}: fmt(antes) difere do depois (exit {Codigo}) {Erro[:200]!r}")
            continue
        Codigo, DeNovo, _ = RodarForja(Forja, ["fmt", "--stdin"], Depois)
        if Codigo != 0 or DeNovo != Depois:
            Falhas.append(f"{Nome}: fmt(depois) nao e idempotente")
    return len(Nomes), Falhas


def CasosSinteticos():
    """(nome, entrada, exit esperado, saida esperada, desvios esperados ou None, trecho do stderr)."""
    return [
        ("bom_crlf", b"\xef\xbb\xbfproc main() {\r\n    print(1)\r\n}\r\n", 0, b"proc main() {\n    print(1)\n}\n", 2, b""),
        ("canonico", b"proc main() {\n    print(1)\n}\n", 0, b"proc main() {\n    print(1)\n}\n", 0, b""),
        ("vazio", b"", 0, b"", 0, b""),
        ("so_comentario_sem_lf", b"// x  ", 0, b"// x\n", 2, b""),
        ("so_espacos", b"   \n\n", 0, b"", 1, b""),
        ("erro_sintaxe", b"proc main( {\n", 1, b"", None, b'"estado": "nao_analisado"'),
        ("token_grande", FONTE_TOKEN_GRANDE, 1, b"", None, b"65535"),
    ]


def VerificarSinteticos(Forja):
    Falhas = []
    Casos = CasosSinteticos()
    for Nome, Entrada, ExitEsperado, SaidaEsperada, DesviosEsperados, Trecho in Casos:
        Codigo, Saida, Erro = RodarForja(Forja, ["fmt", "--stdin", "--regras"], Entrada)
        Desvios, _ = LerRegras(Erro)
        Errado = Codigo != ExitEsperado or Saida != SaidaEsperada or Trecho not in Erro
        if DesviosEsperados is not None and Desvios != DesviosEsperados:
            Errado = True
        if Errado:
            Falhas.append(f"{Nome}: exit {Codigo} (esperava {ExitEsperado}), desvios {Desvios} (esperava {DesviosEsperados}), saida {Saida[:80]!r}, stderr {Erro[:160]!r}")
    for Nome, Args in (("sem_args", ["fmt"]), ("aplicar", ["fmt", "--stdin", "--aplicar"]), ("caminho", ["fmt", "x.fj"])):
        Codigo, Saida, _ = RodarForja(Forja, Args, b"")
        if Codigo != 2 or Saida != b"":
            Falhas.append(f"uso_{Nome}: exit {Codigo} (esperava 2)")
    return len(Casos) + 3, Falhas


def ListarCorpus():
    """[(pasta do relatorio, caminho)] do corpus pedido (so leitura)."""
    Itens = []

    def Varrer(Rotulo, Base, Recursivo, Filtro=lambda Nome: True):
        if not os.path.isdir(Base):
            return
        for Atual, Pastas, Arquivos in os.walk(Base):
            Pastas.sort()
            for Arq in sorted(Arquivos):
                if Arq.endswith(".fj") and Filtro(Arq):
                    Itens.append((Rotulo, os.path.join(Atual, Arq)))
            if not Recursivo:
                break

    Varrer("std", os.path.join(RAIZ_LINGUAGEM, "std"), True)
    Varrer("vendor", os.path.join(RAIZ_LINGUAGEM, "vendor"), True)
    Varrer("exemplos_libs", os.path.join(RAIZ_LINGUAGEM, "exemplos_libs"), True)
    Varrer("examples", PASTA_EXEMPLOS, False)
    Varrer("engine\\forja", os.path.join(RAIZ_PROJETO, "engine", "forja"), True)
    Varrer("tests\\lowering", PASTA_LOWERING, False)
    Varrer("tests\\semantica", os.path.join(RAIZ_LINGUAGEM, "tests", "semantica"), False, lambda Nome: Nome.startswith("ok_"))
    Varrer("benchmark\\corpus", os.path.join(RAIZ_LINGUAGEM, "benchmark", "corpus"), True)
    return Itens


def AstSemPos(Forja, Caminho):
    """Saida do 'forja ast --sem-pos' (so sintaxe) ou None se nao le."""
    Codigo, Saida, _ = RodarForja(Forja, ["ast", "--sem-pos", Caminho])
    return Saida if Codigo == 0 else None


def GravarAtomico(Destino, Dados):
    """Grava em arquivo vizinho e troca: outro 'ast' que leia este arquivo como import nunca ve metade."""
    with open(Destino + ".tmp", "wb") as Arq:
        Arq.write(Dados)
    os.replace(Destino + ".tmp", Destino)


def FimDoLiteral(Texto, Inicio):
    """Posicao logo depois do literal ('"' fecha; '\\' escapa o proximo caractere) ou do comentario ('//' vai ate o fim da linha) que comeca em Inicio."""
    if Texto[Inicio] == "/":
        Fim = Texto.find("\n", Inicio)
        return len(Texto) if Fim < 0 else Fim
    Pos = Inicio + 1
    while Pos < len(Texto) and Texto[Pos] not in '"\n':
        Pula = Texto[Pos] == "\\" and Texto[Pos + 1: Pos + 2] != "\n"
        Pos += 2 if Pula else 1
    return min(Pos + 1, len(Texto))


def SemEspacos(Bytes):
    """Texto do fonte sem espaco nem quebra fora de string e de comentario, como [(tipo, texto)]: c = codigo, s = string, k = comentario.
    Codigo sai sem nenhum espaco; string sai byte a byte; comentario sai sem o espaco do fim (R4). Prova (c): o fmt so mexe em espacos."""
    Texto = Bytes.decode("utf-8", "surrogateescape")
    Partes = []
    Codigo = []
    Pos = 0
    while Pos < len(Texto):
        if Texto[Pos] == '"' or Texto.startswith("//", Pos):
            Fim = FimDoLiteral(Texto, Pos)
            if Codigo:
                Partes.append(("c", "".join(Codigo)))
                Codigo = []
            Trecho = Texto[Pos:Fim]
            Partes.append(("k", Trecho.rstrip(ESPACOS)) if Texto[Pos] == "/" else ("s", Trecho))
            Pos = Fim
        else:
            if Texto[Pos] not in ESPACOS:
                Codigo.append(Texto[Pos])
            Pos += 1
    if Codigo:
        Partes.append(("c", "".join(Codigo)))
    return Partes


def VerificarArquivo(Forja, Rotulo, Caminho, Copia):
    """Mede um arquivo do corpus; Copia = onde o formatado vai (no espelho da pasta, para os imports relativos acharem os vizinhos).
    estado: pulado (fmt recusa ou ast nao le) | erro (motivo: (a), (b), (c) ou outro) | ok."""
    Original = open(Caminho, "rb").read()
    Codigo, Formatado, Erro = RodarForja(Forja, ["fmt", "--stdin", "--regras"], Original)
    Resultado = {"rotulo": Rotulo, "caminho": Caminho, "estado": "ok", "mudou": False, "regras": {}, "motivo": ""}
    if Codigo != 0:
        return {**Resultado, "estado": "pulado", "motivo": "fmt recusou"}
    Desvios, Regras = LerRegras(Erro)
    Resultado["mudou"] = Formatado != Original
    Resultado["regras"] = Regras
    if (Desvios or 0) > 0 and not Resultado["mudou"]:
        return {**Resultado, "estado": "erro", "motivo": "desvios sem mudanca de bytes"}
    Antes = AstSemPos(Forja, Caminho)
    if Antes is None:
        return {**Resultado, "estado": "pulado", "motivo": "ast nao le o original"}
    GravarAtomico(Copia, Formatado)
    if AstSemPos(Forja, Copia) != Antes:
        return {**Resultado, "estado": "erro", "motivo": "(b) o ast mudou depois do fmt"}
    if SemEspacos(Original) != SemEspacos(Formatado):
        return {**Resultado, "estado": "erro", "motivo": "(c) texto mudou fora do espaco (codigo, string ou comentario)"}
    Codigo2, DeNovo, _ = RodarForja(Forja, ["fmt", "--stdin"], Formatado)
    if Codigo2 != 0 or DeNovo != Formatado:
        return {**Resultado, "estado": "erro", "motivo": "(a) fmt(fmt(x)) != fmt(x)"}
    return Resultado


def MedirCorpus(Forja):
    Itens = ListarCorpus()
    Espelhos = {}
    Trava = threading.Lock()
    with tempfile.TemporaryDirectory(prefix="verificar_fmt_") as Raiz:
        def Espelho(Pasta):
            """Copia (uma vez) a pasta do original para %TEMP%; so le o repositorio."""
            with Trava:
                if Pasta not in Espelhos:
                    Destino = os.path.join(Raiz, str(len(Espelhos)))
                    shutil.copytree(Pasta, Destino, ignore=shutil.ignore_patterns("*.exe", "*.o", "*.dll"))
                    Espelhos[Pasta] = Destino
                return Espelhos[Pasta]

        def Trabalho(Item):
            Rotulo, Caminho = Item
            Copia = os.path.join(Espelho(os.path.dirname(Caminho)), os.path.basename(Caminho))
            return VerificarArquivo(Forja, Rotulo, Caminho, Copia)

        with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as Pool:
            return list(Pool.map(Trabalho, Itens))


def Relatar(Resultados):
    """Imprime os numeros do corpus; devolve a quantidade de erros."""
    PorPasta = collections.OrderedDict()
    for Res in Resultados:
        Cont = PorPasta.setdefault(Res["rotulo"], collections.Counter())
        Cont["total"] += 1
        Cont[Res["estado"]] += 1
        Cont["mudariam"] += 1 if Res["estado"] == "ok" and Res["mudou"] else 0
    for Rotulo, Cont in PorPasta.items():
        print(f"  {Rotulo}: {Cont['total']} arquivos, {Cont['mudariam']} mudariam, {Cont['pulado']} pulados, {Cont['erro']} erros")
    PorRegra = collections.Counter()
    ArquivosPorRegra = collections.Counter()
    for Res in Resultados:
        for Regra, Qtd in Res["regras"].items():
            PorRegra[Regra] += Qtd
            ArquivosPorRegra[Regra] += 1
    print("  por regra (ocorrencias / arquivos): " + ", ".join(f"{Chave} {PorRegra[Chave]}/{ArquivosPorRegra[Chave]}" for Chave in sorted(PorRegra)))
    for Res in Resultados:
        if Res["estado"] == "ok" and Res["mudou"]:
            print("  MUDARIA " + os.path.relpath(Res["caminho"], RAIZ_PROJETO) + " " + ",".join(f"{Chave}:{Val}" for Chave, Val in sorted(Res["regras"].items())))
    Erros = [Res for Res in Resultados if Res["estado"] == "erro"]
    for Res in Erros:
        print(f"  ERRO {Res['caminho']}: {Res['motivo']}")
    return len(Erros)


def ImprimirCorpus(Resultados):
    """Linha de resumo do corpus: analisados (o fmt aceitou e o ast leu), nao_analisados e as falhas por prova."""
    Analisados = [Res for Res in Resultados if Res["estado"] in ("ok", "erro")]
    Falhas = collections.Counter(Res["motivo"][:3] if Res["motivo"].startswith("(") else "outro" for Res in Analisados if Res["estado"] == "erro")
    NaoAnalisados = sum(1 for Res in Resultados if Res["estado"] == "pulado")
    print(f"corpus: {len(Resultados)} arquivos, {len(Analisados)} analisados, {NaoAnalisados} nao_analisados (fmt recusa ou ast nao le; esperado, nao e falha)")
    print(f"  (a) idempotencia {len(Analisados) - Falhas['(a)']}/{len(Analisados)}, (b) AST {len(Analisados) - Falhas['(b)']}/{len(Analisados)}, (c) texto {len(Analisados) - Falhas['(c)']}/{len(Analisados)}, outro {Falhas['outro']}")
    return [f"corpus: {Res['caminho']}: {Res['motivo']}" for Res in Analisados if Res["estado"] == "erro"]


def ProgramaSemCodigo(Base):
    """Programa do lowering que sai 0: sem .codigo nem .dev.codigo (esses esperam exit 101)."""
    return not os.path.isfile(Base + ".codigo") and not os.path.isfile(Base + ".dev.codigo")


def ListarAmostraExecucao():
    """[(rotulo, caminho do .fj, caminho do .esperado ou None)]: 35 programas de tests\\lowering com .esperado (espacados pelo nome) + os .fj de examples."""
    Candidatos = []
    for Arq in sorted(os.listdir(PASTA_LOWERING)):
        Base = os.path.join(PASTA_LOWERING, Arq[: -len(".fj")])
        if Arq.endswith(".fj") and Arq[:2].isdigit() and os.path.isfile(Base + ".esperado") and ProgramaSemCodigo(Base):
            Candidatos.append(("tests\\lowering", Base + ".fj", Base + ".esperado"))
    Passo = max(1, len(Candidatos) // TAMANHO_AMOSTRA_LOWERING)
    Amostra = Candidatos[::Passo][:TAMANHO_AMOSTRA_LOWERING]
    for Arq in sorted(os.listdir(PASTA_EXEMPLOS)):
        if Arq.endswith(".fj"):
            Amostra.append(("examples", os.path.join(PASTA_EXEMPLOS, Arq), None))
    return Amostra


def RodarNaPasta(Forja, Pasta, Argumentos):
    """Roda o forja com a copia como pasta de trabalho e stdin vazio (como o runner do lowering). Devolve (exit, stdout com LF); exit None = passou do tempo."""
    try:
        Proc = subprocess.run([Forja] + Argumentos, cwd=Pasta, stdin=subprocess.DEVNULL, capture_output=True, timeout=TEMPO_EXECUCAO)
    except subprocess.TimeoutExpired:
        return None, b""
    return Proc.returncode, Proc.stdout.replace(b"\r\n", b"\n")


def PrepararCopia(Pasta, Destino, Nome, Dados):
    """Escreve o programa em Destino, com a pasta mods/ ao lado quando o original tem: o original nunca e tocado."""
    os.makedirs(Destino)
    Mods = os.path.join(Pasta, "mods")
    if os.path.isdir(Mods):
        shutil.copytree(Mods, os.path.join(Destino, "mods"))
    with open(os.path.join(Destino, Nome), "wb") as Arq:
        Arq.write(Dados)


def PrevistoDoPrograma(Esperado, Obtido):
    """Saida que a copia tem de dar: o .esperado (exit 0) ou, sem .esperado, a do original se ele sai 0. None = o original nao serve de referencia."""
    if Esperado is not None:
        Texto = open(Esperado, "rb").read().replace(b"\r\n", b"\n")
        return (0, Texto) if Obtido == (0, Texto) else None
    return Obtido if Obtido[0] == 0 else None


def VerificarProgramaCaso(Forja, Caso, Raiz, Indice):
    """Prova (d) num programa, numa so pasta de copia: o original roda igual ao .esperado (ou sai 0, sem .esperado); depois a copia com o fmt
    roda igual a isso. Devolve {estado, motivo}."""
    _, Fj, Esperado = Caso
    Pasta, Nome = os.path.dirname(Fj), os.path.basename(Fj)
    Original = open(Fj, "rb").read()
    Codigo, Formatado, _ = RodarForja(Forja, ["fmt", "--stdin"], Original)
    if Codigo != 0:
        return {"estado": "pulado", "motivo": "fmt recusou"}
    Dir = os.path.join(Raiz, str(Indice))
    PrepararCopia(Pasta, Dir, Nome, Original)
    Obtido = RodarNaPasta(Forja, Dir, ["run", Nome])
    Previsto = PrevistoDoPrograma(Esperado, Obtido)
    if Previsto is None:
        return {"estado": "pulado", "motivo": f"original nao bate com o esperado (exit {Obtido[0]})"}
    with open(os.path.join(Dir, Nome), "wb") as Arq:
        Arq.write(Formatado)
    if RodarNaPasta(Forja, Dir, ["run", Nome]) != Previsto:
        return {"estado": "erro", "motivo": "copia formatada: exit ou stdout diferente do esperado"}
    return {"estado": "ok", "motivo": ""}


def VerificarExecucao(Forja):
    """Prova (d): roda a amostra no original e na copia formatada. Devolve {total, ok, pulados (Counter por motivo), falhas}."""
    Amostra = ListarAmostraExecucao()
    with tempfile.TemporaryDirectory(prefix="verificar_fmt_exec_") as Raiz:
        with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as Pool:
            Resultados = list(Pool.map(lambda Par: VerificarProgramaCaso(Forja, Par[1], Raiz, Par[0]), enumerate(Amostra)))
    Resumo = {"total": len(Amostra), "ok": 0, "pulados": collections.Counter(), "falhas": []}
    for Caso, Res in zip(Amostra, Resultados):
        if Res["estado"] == "pulado":
            Resumo["pulados"][Res["motivo"]] += 1
        elif Res["estado"] == "ok":
            Resumo["ok"] += 1
        else:
            Resumo["falhas"].append(f"execucao {Caso[1]}: {Res['motivo']}")
    return Resumo


def ImprimirExecucao(Resumo):
    Pulados = sum(Resumo["pulados"].values())
    Checados = Resumo["ok"] + len(Resumo["falhas"])
    print(f"execucao: {Resumo['ok']}/{Checados} programas iguais ao esperado (amostra {Resumo['total']}, pulados {Pulados})")
    for Motivo, Qtd in Resumo["pulados"].most_common():
        print(f"  pulado {Qtd}: {Motivo}")


def VerificarTokenGrandeArquivo(Forja):
    """Negativo em arquivo: token de 65535 bytes = nao_analisado no 'fmt --json', exit 1 e arquivo intacto. Devolve (total, falhas)."""
    Pasta = tempfile.mkdtemp(prefix="fmt_token_")
    try:
        Caminho = os.path.join(Pasta, "grande.fj")
        with open(Caminho, "wb") as Arq:
            Arq.write(FONTE_TOKEN_GRANDE)
        Codigo, Saida, _ = RodarForja(Forja, ["fmt", "--json", Caminho])
        try:
            Estado = json.loads(Saida)["arquivos"][0]["estado"]
        except (ValueError, KeyError, IndexError):
            Estado = None
        Intacto = open(Caminho, "rb").read() == FONTE_TOKEN_GRANDE
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    if Codigo != 1 or Estado != "nao_analisado" or not Intacto:
        return 1, [f"token_grande em arquivo: exit {Codigo}, estado {Estado}, intacto {Intacto}"]
    return 1, []


def Selftest():
    """Confere o leitor do --regras, o segmentador de texto (prova c) e a lista de sinteticos (sem rodar o forja)."""
    assert LerRegras(b"desvios=3 primeiro=1:1:bom regras=bom:1,espaco:2\n") == (3, {"bom": 1, "espaco": 2})
    assert LerRegras(b"") == (None, {})
    assert LerRegras(b"desvios=0 primeiro=0:0:- regras=\n") == (0, {})
    assert SemEspacos(b'\xef\xbb\xbfproc  main() { // a b  \r\n  "x  y"  }\r\n') == [("c", "procmain(){"), ("k", "// a b"), ("s", '"x  y"'), ("c", "}")]
    assert SemEspacos(b'"a\\"  b"') == [("s", '"a\\"  b"')]
    assert SemEspacos(b"a  +  b") == SemEspacos(b"a+b")
    assert SemEspacos(b"x // a  b") != SemEspacos(b"x // a b"), "comentario mudou e a prova (c) tem de ver"
    assert SemEspacos(b'"a  b"') != SemEspacos(b'"a b"'), "string mudou e a prova (c) tem de ver"
    assert len(CasosSinteticos()) == 7
    assert os.path.isdir(PASTA_CASOS) and any(Arq.endswith(".antes.fj") for Arq in os.listdir(PASTA_CASOS))
    for Arq in os.listdir(PASTA_CASOS):
        if Arq.endswith(".antes.fj"):
            assert os.path.exists(os.path.join(PASTA_CASOS, Arq[: -len(".antes.fj")] + ".depois.fj")), Arq
    print("selftest ok")
    return 0


def VerificarArquivoFmt(Forja):
    """'forja fmt <arq>' (sem --stdin) sobre as fixtures, numa copia em %TEMP%: antes -> rodape de mudariam, exit 1 e arquivo intacto; depois -> exit 0 e sem diff. Devolve (total, falhas)."""
    Falhas = []
    Nomes = sorted(Arq[: -len(".antes.fj")] for Arq in os.listdir(PASTA_CASOS) if Arq.endswith(".antes.fj"))
    Pasta = tempfile.mkdtemp(prefix="fmt_arquivo_")
    try:
        for Nome in Nomes:
            Caminho = os.path.join(Pasta, Nome + ".fj")
            Antes = open(os.path.join(PASTA_CASOS, Nome + ".antes.fj"), "rb").read()
            Depois = open(os.path.join(PASTA_CASOS, Nome + ".depois.fj"), "rb").read()
            with open(Caminho, "wb") as Arquivo:
                Arquivo.write(Antes)
            Codigo, Saida, Erro = RodarForja(Forja, ["fmt", Caminho])
            if Codigo != 1 or b"mudariam" not in Saida or open(Caminho, "rb").read() != Antes:
                Falhas.append(f"{Nome}: fmt <arq> (antes) fora do esperado: exit {Codigo}, arquivo alterado ou sem rodape {Erro[:160]!r}")
            with open(Caminho, "wb") as Arquivo:
                Arquivo.write(Depois)
            Codigo, Saida, Erro = RodarForja(Forja, ["fmt", Caminho])
            if Codigo != 0 or b"--- a/" in Saida:
                Falhas.append(f"{Nome}: fmt <arq> (depois) deveria sair 0 sem diff: exit {Codigo}")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return len(Nomes), Falhas


def ConferirPastaCanonica(Forja, Rotulo, Caminho):
    """'forja fmt <arq>' em dry-run: (Rotulo, Caminho, estado) com estado 'canonico', 'nao_analisado' ou 'mudaria'."""
    Codigo, _, Erro = RodarForja(Forja, ["fmt", Caminho])
    if Codigo == 0:
        return Rotulo, Caminho, "canonico"
    if Codigo == 1 and b"nao analisado" in Erro:
        return Rotulo, Caminho, "nao_analisado"
    return Rotulo, Caminho, "mudaria"


def VerificarPortaoCanonico(Forja):
    """Portao da migracao U1: nas pastas migradas (std, engine\\forja, tests\\lowering, tests\\semantica ok_*) o fmt nao muda nada. Devolve (total, falhas)."""
    Migradas = ("std", "engine\\forja", "tests\\lowering", "tests\\semantica", "examples")
    Itens = [(Rotulo, Caminho) for Rotulo, Caminho in ListarCorpus() if Rotulo in Migradas]
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as Pool:
        Resultados = list(Pool.map(lambda Item: ConferirPastaCanonica(Forja, *Item), Itens))
    Falhas = [f"portao canonico: {Caminho} mudaria com 'forja fmt --aplicar'" for _, Caminho, Estado in Resultados if Estado == "mudaria"]
    return len(Resultados), Falhas


def Principal():
    Analisador = argparse.ArgumentParser(description="verifica o forja fmt")
    Analisador.add_argument("--forja", help="caminho do forja.exe")
    Analisador.add_argument("--selftest", action="store_true")
    Analisador.add_argument("--listar", action="store_true", help="so lista o corpus")
    Args = Analisador.parse_args()
    if Args.selftest:
        return Selftest()
    if Args.listar:
        for Rotulo, Caminho in ListarCorpus():
            print(Rotulo, Caminho)
        return 0
    if not Args.forja:
        Analisador.error("falta --forja")
    Forja = os.path.abspath(Args.forja)
    TotalF, Falhas = VerificarFixtures(Forja)
    print(f"fixtures: {TotalF - len(Falhas)} de {TotalF} ok")
    TotalS, FalhasS = VerificarSinteticos(Forja)
    print(f"sinteticos: {TotalS - len(FalhasS)} de {TotalS} ok")
    TotalA, FalhasA = VerificarArquivoFmt(Forja)
    print(f"arquivo (dry-run): {TotalA - len(FalhasA)} de {TotalA} ok")
    TotalN, FalhasN = VerificarTokenGrandeArquivo(Forja)
    print(f"negativo em arquivo (token de 65535 B): {TotalN - len(FalhasN)} de {TotalN} ok")
    TotalP, FalhasP = VerificarPortaoCanonico(Forja)
    print(f"portao canonico (pastas migradas, dry-run): {TotalP - len(FalhasP)} de {TotalP} ok")
    Resultados = MedirCorpus(Forja)
    FalhasC = ImprimirCorpus(Resultados)
    Erros = Relatar(Resultados)
    Resumo = VerificarExecucao(Forja)
    ImprimirExecucao(Resumo)
    Todas = Falhas + FalhasS + FalhasA + FalhasN + FalhasP + FalhasC + Resumo["falhas"]
    for Falha in Todas:
        print("  FALHOU " + Falha)
    if Todas or Erros:
        return 1
    Analisados = sum(1 for Res in Resultados if Res["estado"] in ("ok", "erro"))
    print(f"OK: {TotalF} fixtures, {TotalS} sinteticos, {TotalA} arquivo, {TotalN} negativo, {Analisados} arquivos do corpus, {Resumo['ok']} programas de execucao")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
