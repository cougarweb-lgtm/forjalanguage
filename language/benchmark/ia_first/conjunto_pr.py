# -*- coding: utf-8 -*-
"""conjunto_pr.py - nucleo compartilhado dos conjuntos P (projeto + 20 perguntas) e R (10 refatoracoes) do benchmark IA FIRST (P-IA1-a17).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: texto sem comentarios, chamada do forja (rodar/consultar), normalizacao e correcao das respostas do P,
        operacoes de edicao das tarefas R (sub, mover, cortar), conferencia de pasta do R (check, run, checks estruturais) e as Medidas
        que o coleta_ia_first grava no resultado.json das celulas P e R.

Conjunto P: tarefas\\P\\P01 = o projeto + pedido.txt + perguntas.txt; gabarito em referencias\\forja\\P\\P01\\gabarito.json (NUNCA copiado para a celula).
Conjunto R: tarefas\\R\\Rnn = pasta inicial + pedido.txt; gabarito em referencias\\forja\\R\\Rnn\\{esperado.json, checks.json, solucao\\}.
Quem usa: montagem\\montar_pr.py (monta), verificar_conjunto_pr.py (prova), coleta_ia_first.py (nota as celulas).
Verificacao: python -I verificar_conjunto_pr.py --forja <forja.exe> --selftest
Reversao: apagar este arquivo e os que o importam. Depende de: Python 3.9+ (biblioteca padrao) e do forja.exe congelado.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
LIMITE_FORJA_S = 120
LIMITE_PROGRAMA_S = 20
SEPARADORES = re.compile(r"[,;\s]+")
LINHA_RESPOSTA = re.compile(r"^\s*(Q\d\d)\s*[:=]\s*(.*?)\s*$")
EXIT_FORJA_AUSENTE = 127


def LerJson(Caminho):
    with open(Caminho, encoding="utf-8") as Arq:
        return json.load(Arq)


def TextoSemComentarios(Texto):
    """Tira o '//' ate o fim de cada linha, sem apagar linhas (os numeros de linha continuam valendo)."""
    return "\n".join(Linha.split("//", 1)[0] for Linha in Texto.split("\n"))


def ArquivosFj(Pasta):
    """Caminhos relativos (com /) de todos os .fj da pasta, em ordem alfabetica."""
    Pasta = Path(Pasta)
    return sorted(P.relative_to(Pasta).as_posix() for P in Pasta.rglob("*.fj"))


def LerFontes(Pasta, SemComentario=True):
    """{caminho relativo: texto} dos .fj da pasta (LF; sem comentarios por padrao)."""
    Fontes = {}
    for Rel in ArquivosFj(Pasta):
        Texto = (Path(Pasta) / Rel).read_text(encoding="utf-8").replace("\r\n", "\n")
        Fontes[Rel] = TextoSemComentarios(Texto) if SemComentario else Texto
    return Fontes


def RodarForja(Forja, Argumentos, Pasta, Limite=LIMITE_FORJA_S):
    """Roda o forja.exe; devolve {rc, stdout, stderr}. Erro de execucao vira rc 127 com o motivo no stderr."""
    try:
        P = subprocess.run([str(Forja)] + list(Argumentos), cwd=str(Pasta), capture_output=True, timeout=Limite)
    except subprocess.TimeoutExpired:
        return {"rc": 124, "stdout": "", "stderr": "tempo excedido"}
    except OSError as Erro:
        return {"rc": EXIT_FORJA_AUSENTE, "stdout": "", "stderr": "nao executou: %s" % Erro}
    Texto = lambda Dados: Dados.decode("utf-8", errors="replace").replace("\r\n", "\n")
    return {"rc": P.returncode, "stdout": Texto(P.stdout), "stderr": Texto(P.stderr)}


def Consultar(Forja, Pergunta, Argumentos, Pasta):
    """`forja consultar <pergunta> ... --json` -> dict; levanta ValueError se a consulta falhar."""
    R = RodarForja(Forja, ["consultar", Pergunta] + list(Argumentos) + ["--json"], Pasta)
    if R["rc"] != 0 or not R["stdout"].strip():
        raise ValueError("consultar %s %s falhou (rc=%s): %s" % (Pergunta, Argumentos, R["rc"], (R["stderr"] or R["stdout"])[:200]))
    return json.loads(R["stdout"])


def RelativoA(Pasta, Caminho):
    """Caminho do forja (relativo ou absoluto, / ou \\) -> relativo a Pasta com /."""
    Candidato = Path(str(Caminho).replace("\\", "/"))
    if not Candidato.is_absolute():
        return Candidato.as_posix()
    return Candidato.resolve().relative_to(Path(Pasta).resolve()).as_posix()


# ---------------------------------------------------------------- respostas do P

def LimparToken(Token):
    return Token.strip().strip("`'\"").replace("\\", "/").removeprefix("./")


def NormalizarResposta(Formato, Texto):
    """Forma canonica de uma resposta para comparar com o gabarito. Formatos: linha, conjunto, lista, texto, inteiro."""
    Texto = (Texto or "").strip()
    Tokens = [LimparToken(T) for T in SEPARADORES.split(Texto) if T.strip("`'\"")]
    if Formato == "linha":
        return Tokens[0] if Tokens else ""
    if Formato == "inteiro":
        Achado = re.search(r"-?\d+", Texto)
        return Achado.group(0) if Achado else ""
    if Formato == "conjunto":
        return ",".join(sorted(set(Tokens)))
    if Formato == "lista":
        return ",".join(Tokens)
    return " ".join(Texto.replace("`", "").strip("\"' ").split())


def LerRespostas(Texto):
    """resposta.txt -> {Qnn: texto}. A ultima linha de cada pergunta vale."""
    Respostas = {}
    for Linha in Texto.replace("\r\n", "\n").split("\n"):
        Achado = LINHA_RESPOSTA.match(Linha)
        if Achado:
            Respostas[Achado.group(1)] = Achado.group(2)
    return Respostas


def CorrigirP(Gabarito, TextoResposta):
    """Compara resposta.txt com o gabarito: {acertos, total, acuracia, erradas: [ids]}."""
    Respostas = LerRespostas(TextoResposta)
    Erradas = []
    for Item in Gabarito["perguntas"]:
        Obtido = NormalizarResposta(Item["formato"], Respostas.get(Item["id"], ""))
        if Obtido != NormalizarResposta(Item["formato"], Item["resposta"]):
            Erradas.append(Item["id"])
    Total = len(Gabarito["perguntas"])
    return {"acertos": Total - len(Erradas), "total": Total, "acuracia": (Total - len(Erradas)) / Total, "erradas": Erradas}


# ---------------------------------------------------------------- edicao das tarefas R

def LerLf(Caminho):
    return Path(Caminho).read_text(encoding="utf-8").replace("\r\n", "\n")


def EscreverLf(Caminho, Texto):
    Caminho = Path(Caminho)
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Caminho.write_text(Texto, encoding="utf-8", newline="\n")


def OpSub(Pasta, Op):
    """Troca por regex (multilinha); 'n' e a contagem exata de trocas quando informado (senao, pelo menos 1)."""
    Novo, Vezes = re.subn(Op["de"], Op["para"], LerLf(Path(Pasta) / Op["arquivo"]), flags=re.M)
    if Vezes != Op.get("n", Vezes) or Vezes == 0:
        raise ValueError("%s: sub %r aplicou %d vezes, esperado %s" % (Op["arquivo"], Op["de"], Vezes, Op.get("n", ">= 1")))
    EscreverLf(Path(Pasta) / Op["arquivo"], Novo)


def OpMover(Pasta, Op):
    """Recorta blocos (regex com re.S: comentario + proc ate o '}' da coluna 0) de um arquivo e os poe, na ordem, em 'destino'."""
    Texto, Movidos = LerLf(Path(Pasta) / Op["arquivo"]), []
    for Regex in Op["blocos"]:
        Achado = re.search(Regex, Texto, flags=re.M | re.S)
        if not Achado:
            raise ValueError("%s: bloco %r nao existe" % (Op["arquivo"], Regex))
        Movidos.append(Achado.group(0).rstrip("\n") + "\n")
        Texto = Texto[:Achado.start()] + Texto[Achado.end():]
    EscreverLf(Path(Pasta) / Op["arquivo"], Texto)
    EscreverLf(Path(Pasta) / Op["destino"], Op["cabecalho"] + "\n".join(Movidos))


def OpCortar(Pasta, Op):
    Novo, Vezes = re.subn(Op["de"], "", LerLf(Path(Pasta) / Op["arquivo"]), flags=re.M | re.S)
    if Vezes != 1:
        raise ValueError("%s: cortar %r aplicou %d vezes, esperado 1" % (Op["arquivo"], Op["de"], Vezes))
    EscreverLf(Path(Pasta) / Op["arquivo"], Novo)


OPERACOES = {"sub": OpSub, "mover": OpMover, "cortar": OpCortar}


def AplicarOps(Pasta, Ops):
    """Aplica em ordem as operacoes de edicao (sub, mover, cortar) sobre a pasta."""
    for Op in Ops:
        OPERACOES[Op["op"]](Pasta, Op)


def ChecagemEstrutural(Fontes, Check):
    """Uma checagem de checks.json contra os fontes sem comentarios; devolve o motivo da falha ou ''."""
    Tipo, Regex = Check["tipo"], re.compile(Check["regex"], re.M)
    Alvos = [Fontes[Check["arquivo"]]] if "arquivo" in Check and Check["arquivo"] in Fontes else (
        [] if "arquivo" in Check else list(Fontes.values()))
    Vezes = sum(len(Regex.findall(T)) for T in Alvos)
    if Tipo == "ausente" and Vezes:
        return "%r ainda aparece %d vez(es) %s" % (Check["regex"], Vezes, Check.get("arquivo", "no projeto"))
    if Tipo == "existe" and Vezes < Check.get("minimo", 1):
        return "%r aparece %d vez(es), esperado >= %d %s" % (Check["regex"], Vezes, Check.get("minimo", 1), Check.get("arquivo", "no projeto"))
    if Tipo == "exato" and Vezes != Check["vezes"]:
        return "%r aparece %d vez(es), esperado %d %s" % (Check["regex"], Vezes, Check["vezes"], Check.get("arquivo", "no projeto"))
    return ""


def CompilarERodar(Forja, Pasta):
    """check + build + run de prog.fj da pasta. Devolve (resultado do run ou None, motivo da falha ou '')."""
    Pasta = Path(Pasta)
    if not (Pasta / "prog.fj").is_file():
        return None, "prog.fj nao existe"
    Check = RodarForja(Forja, ["check", "prog.fj"], Pasta)
    if Check["rc"] != 0:
        return None, "check falhou (rc=%s): %s" % (Check["rc"], (Check["stdout"] + Check["stderr"])[:300])
    with tempfile.TemporaryDirectory(prefix="pr_run_") as Tmp:
        Exe = Path(Tmp) / "prog.exe"
        Build = RodarForja(Forja, ["build", "prog.fj", "-o", str(Exe)], Pasta)
        if Build["rc"] != 0:
            return None, "build falhou (rc=%s): %s" % (Build["rc"], (Build["stdout"] + Build["stderr"])[:300])
        return RodarForja(Exe, [], Tmp, LIMITE_PROGRAMA_S), ""


def MotivosR(Forja, Pasta, Esperado, Checks):
    """R: a pasta compila (check rc 0), roda e imprime o esperado (rc 0) e passa em todas as checagens estruturais."""
    Run, Falha = CompilarERodar(Forja, Pasta)
    if Run is None:
        return [Falha]
    Motivos = []
    if Run["rc"] != Esperado["exit"] or Run["stdout"] != Esperado["stdout"]:
        Motivos.append("saida difere: rc=%s stdout=%r, esperado rc=%s stdout=%r" % (Run["rc"], Run["stdout"][:160], Esperado["exit"], Esperado["stdout"][:160]))
    Fontes = LerFontes(Pasta)
    return Motivos + [M for M in (ChecagemEstrutural(Fontes, C) for C in Checks) if M]


# ---------------------------------------------------------------- medidas da celula (coleta_ia_first)

def MedidasDeP(Cel, Pasta, Linhas):
    """P: nota o resposta.txt da celula contra o gabarito. Verde = acuracia >= 95% (B-I4); pass1 = as 20 certas."""
    Gab = LerJson(RAIZ / "referencias" / Cel["lingua"] / "P" / Cel["tarefa"] / "gabarito.json")
    Arq = Path(Pasta) / "resposta.txt"
    Nota = CorrigirP(Gab, Arq.read_text(encoding="utf-8", errors="replace")) if Arq.is_file() else {
        "acertos": 0, "total": len(Gab["perguntas"]), "acuracia": 0.0, "erradas": [P["id"] for P in Gab["perguntas"]]}
    Verde = Nota["acuracia"] >= 0.95
    return {"idas": 0, "idas_ate_verde": 0 if Verde else None, "pass1": Nota["acertos"] == Nota["total"], "pass5": Verde,
            "pass1_mais_maquina": Verde, "idas_maquina": 0, "consultas": 0, "chars_diagnostico": 0, "chars_consulta": 0, "codigos": {},
            "violou_l": False, "verde_final": Verde, "motivo_final": "" if Verde else "erradas: " + ",".join(Nota["erradas"]),
            "acertos": Nota["acertos"], "perguntas": Nota["total"], "acuracia": Nota["acuracia"]}


def MedidasDeR(Cel, Forja, Pasta, Linhas):
    """R: confere a PASTA final da celula (varios arquivos). idas_ate_verde = idas feitas, se a pasta final esta verde (ia.py so guarda
    prog.fj por ida, entao a pasta de cada ida nao e reconstruivel: pass1 = verde com no maximo 1 ida)."""
    Ref = RAIZ / "referencias" / Cel["lingua"] / "R" / Cel["tarefa"]
    Motivos = MotivosR(Forja, Pasta, LerJson(Ref / "esperado.json"), LerJson(Ref / "checks.json")["checks"])
    Idas = [L for L in Linhas if L.get("tipo") == "ida"]
    Verde = not Motivos
    return {"idas": len(Idas), "idas_ate_verde": len(Idas) if Verde else None, "pass1": Verde and len(Idas) <= 1, "pass5": Verde,
            "pass1_mais_maquina": Verde and len(Idas) <= 1, "idas_maquina": sum(1 for L in Linhas if L.get("tipo") == "maquina"),
            "consultas": sum(1 for L in Linhas if L.get("tipo") == "consulta"), "chars_diagnostico": sum(L.get("chars_stdout", 0) + L.get("chars_stderr", 0) for L in Idas),
            "chars_consulta": sum(L.get("chars_stdout", 0) for L in Linhas if L.get("tipo") == "consulta"), "codigos": {},
            "violou_l": any(L.get("tipo") == "proibido" for L in Linhas), "verde_final": Verde, "motivo_final": "; ".join(Motivos)[:300]}
