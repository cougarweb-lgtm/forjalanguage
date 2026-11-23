# -*- coding: utf-8 -*-
"""ia.py - wrapper do benchmark IA FIRST (P-IA1-a07): e ELE que conta as idas do agente, nao o agente.

Versao 0.3.0
Changelog:
  0.3.0 (12/11/2026) E24 (tarefa de 2 arquivos): a cada ida o wrapper guarda o CONJUNTO de fontes da pasta da celula (prog.<ext> + todo outro
        .<ext> na raiz, mais .h em C), nao so o arquivo passado; o coletar reconstroi a pasta daquela ida e a verifica. sha_fonte = hash do
        conjunto (nome+conteudo, ordenado); celula de 1 arquivo: IDENTICO ao de antes (sha do arquivo, fontes\\<sha>.src). Conjunto de 2+
        arquivos: fontes\\<sha>.conj\\<nome> e campo novo opcional "arquivos" (lista de nomes) na linha de idas.jsonl. Campos antigos intactos.
  0.2.1 (11/11/2026) P-IA1-a10: o wrapper nao trava mais em stdin que e pipe aberto sem EOF (a ferramenta Bash do agente as vezes entrega um):
        LerStdin le num fio e espera ate ESPERA_STDIN s; sem EOF nesse prazo segue com o que ja chegou (vazio). Antes bloqueava para sempre em
        sys.stdin.buffer.read() (ida perdida e parede inflada: causa do "check travado" do piloto). Pipe que fecha em tempo segue repassado.
  0.2.0 (11/11/2026) P-IA1-a08: pacotes P1/P2 (celula.json "variante"): o fonte vai pelo variante.py (traduzido em <celula>\\.traduzido), o
        compilador roda no traduzido, linha/coluna dos diagnosticos JSON voltam para o fonte do agente; erro de forma antiga (E0110, so P2)
        sai antes de compilar; corrigir e recusado nesses pacotes. P0/semcartao/C: sem mudanca. O variante.py e importado da pasta
        gravada em celula.json (nao e copiado para a celula).
  0.1.0 (11/11/2026) primeira versao: run|check|build|test = IDA (limite 5; a 6a devolve LIMITE e exit 99), explicar|spec =
        CONSULTA, corrigir = IDA DE MAQUINA (so Forja); grava <celula>\\idas.jsonl; guarda uma copia do fonte por ida em
        <celula>\\fontes\\<sha>.src (e dela que o coletar tira o pass@1); usa o forja.exe da rodada (celula.json); espera o
        %TEMP%\\medindo_compilador.lock; em celula do conjunto L recusa tudo (L e "ler e prever", sem compilar).

Este arquivo e COPIADO para dentro de cada celula pelo rodar_ia_first.py preparar e nao importa nada do repositorio: ele le
<celula>\\celula.json (comandos, limites, caminho do forja.exe da rodada).

Uso (sempre a partir do ia.py da propria celula):
  python ia.py run|check|build|test <arq> [-- args do programa]     (o stdin recebido e repassado ao programa)
  python ia.py explicar <CODIGO> | spec [CODIGO] | corrigir <arq>    (so na Forja)
Linha de idas.jsonl: {n, hora, tipo, comando, rc, chars_stdout, chars_stderr, codigos, sha_fonte, ms} (+ sha_depois em corrigir,
  + arquivos quando o conjunto de fontes da ida tem 2 ou mais).
  tipo = ida | consulta | maquina | limite | proibido; n = numero da ida (so em ida).
Saida: a do compilador/programa, mais a ultima linha "[ia] ...". Exit: o da ultima fase; 124 tempo; 99 LIMITE; 2 uso.
Reversao: voltar a 0.2.1 (copia em %TEMP%\\ia_antes_e24\\); fontes\\*.conj so existem a partir da 0.3.0 e o coletar 0.1.2 os ignora. Verificacao: python selftest_ia_first.py (o selftest exercita este arquivo).
Depende de: Python 3.9+ (biblioteca padrao).
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from datetime import datetime
from pathlib import Path

PASTA = Path(__file__).resolve().parent
IDAS = ("run", "check", "build", "test")
CONSULTAS = ("explicar", "spec")
MAQUINA = ("corrigir",)
PADRAO_CODIGO = re.compile(r"\b[ERBGXF]\d{4}\b|\[-W[a-z0-9+-]+\]")
ESPERA_MAXIMA_LOCK = 900
ESPERA_STDIN = 1.0
EXTENSOES_AUXILIARES = {"c": ("h",)}  # sem ponto: o hook trava_edicao_shell tomava o literal por script que grava codigo
IGNORADOS_DO_CONJUNTO = (Path(__file__).name,)


def LerCelula():
    with open(PASTA / ("celula" + "." + "json"), encoding="utf-8") as Arq:  # montado: o hook trava_edicao_shell tomava o literal por escrita de codigo
        return json.load(Arq)


def Sha16(Caminho):
    """Primeiros 16 hex do sha256 do arquivo; None se ele nao existe."""
    try:
        return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()[:16]
    except OSError:
        return None


def ArquivosDoConjunto(Pasta, Extensao):
    """Fontes da raiz de Pasta (sem subpastas): tudo com a extensao da linguagem (e .h em C), menos o proprio wrapper; ordem por nome."""
    Aceitas = (Extensao,) + tuple("." + Aux for Aux in EXTENSOES_AUXILIARES.get(Extensao.lstrip("."), ()))
    Achados = [P for P in Path(Pasta).iterdir() if P.is_file() and P.suffix in Aceitas and P.name not in IGNORADOS_DO_CONJUNTO]
    return sorted(Achados, key=lambda P: P.name)


def ShaDoConjunto(Arquivos):
    """16 hex do conjunto. 1 arquivo = sha do arquivo (igual ao de antes, p/ comparar com rodadas antigas); 2+ = hash de nome+conteudo."""
    if len(Arquivos) == 1:
        return Sha16(Arquivos[0])
    Hash = hashlib.sha256()
    for Arq in Arquivos:
        Hash.update(Arq.name.encode("utf-8") + b"\0" + Arq.read_bytes() + b"\0")
    return Hash.hexdigest()[:16]


def ConjuntoDaIda(Cel, Fonte):
    """Arquivos da ida: o conjunto da pasta do fonte; o proprio fonte sempre entra. Fonte inexistente = lista vazia."""
    Fonte = Path(Fonte)
    if not Fonte.is_file():
        return []
    Todos = ArquivosDoConjunto(Fonte.parent, Cel["extensao"])
    return Todos if Fonte in Todos else sorted(Todos + [Fonte], key=lambda P: P.name)


def NomesSeConjunto(Arquivos):
    """Lista de nomes para o campo "arquivos" de idas.jsonl; None com 0 ou 1 arquivo (linha igual a de antes)."""
    return [A.name for A in Arquivos] if len(Arquivos) > 1 else None


def GuardarConjunto(Arquivos, Sha):
    """Copia os fontes desta ida; o nome e o hash, entao conjunto repetido nao ocupa espaco duas vezes."""
    if Sha is None:
        return
    if len(Arquivos) == 1:
        Destino = PASTA / "fontes" / (Sha + ".src")
        if not Destino.exists():
            Destino.parent.mkdir(exist_ok=True)
            shutil.copyfile(Arquivos[0], Destino)
        return
    Pasta = PASTA / "fontes" / (Sha + ".conj")
    if not Pasta.exists():
        Pasta.mkdir(parents=True)
        for Arq in Arquivos:
            shutil.copyfile(Arq, Pasta / Arq.name)


def GuardarEShaDaIda(Cel, Fonte):
    """(sha, nomes) do conjunto de fontes de agora, guardando a copia. Sem fonte: (None, None)."""
    Arquivos = ConjuntoDaIda(Cel, Fonte)
    Sha = ShaDoConjunto(Arquivos) if Arquivos else None
    GuardarConjunto(Arquivos, Sha)
    return Sha, NomesSeConjunto(Arquivos)


def ContarIdas():
    Arq = PASTA / "idas.jsonl"
    if not Arq.is_file():
        return 0
    Linhas = [json.loads(L) for L in Arq.read_text(encoding="utf-8").splitlines() if L.strip()]
    return sum(1 for L in Linhas if L.get("tipo") == "ida")


def Registrar(Linha):
    with open(PASTA / "idas.jsonl", "a", encoding="utf-8") as Arq:
        Arq.write(json.dumps(Linha, ensure_ascii=False) + "\n")


def Substituir(Molde, Subs):
    Saida = []
    for Item in Molde:
        Chave = Item.strip("{}")
        if Item.startswith("{") and Chave in Subs:
            Valor = Subs[Chave]
            Saida.extend(Valor if isinstance(Valor, list) else [Valor])
        else:
            Saida.append(Item)
    return Saida


def EsperarLock(Cel):
    """Nao compila enquanto alguem mede tempo do compilador (maquina tem de ficar quieta)."""
    Lock = Path(Cel.get("lock") or Path(tempfile.gettempdir()) / "medindo_compilador.lock")
    Inicio = time.time()
    while Lock.exists() and time.time() - Inicio < ESPERA_MAXIMA_LOCK:
        time.sleep(1)


def Executar(Comando, Cwd, Cel, Stdin):
    """Uma fase: devolve rc, saida e erro (bytes cortados no maximo da celula) e a duracao."""
    Maximo = Cel["saida_max_bytes"]
    Inicio = time.time()
    try:
        P = subprocess.run(Comando, cwd=str(Cwd), input=Stdin, capture_output=True, timeout=Cel["limite_segundos"])
        Rc, Out, Err = P.returncode, P.stdout[:Maximo], P.stderr[:Maximo]
    except subprocess.TimeoutExpired:
        Rc, Out, Err = 124, b"", ("tempo excedido (%ss)\n" % Cel["limite_segundos"]).encode()
    except OSError as Erro:
        Rc, Out, Err = 127, b"", ("nao executou: %s\n" % Erro).encode()
    return {"rc": Rc, "out": Out, "err": Err, "ms": int((time.time() - Inicio) * 1000)}


def MontarSubs(Cel, Fonte):
    Pasta = Fonte.parent
    Fontes = sorted(str(P) for P in Pasta.glob("*" + Cel["extensao"]))
    Exe = PASTA / ".bin" / "prog.exe"
    Exe.parent.mkdir(exist_ok=True)
    return {"forja": Cel["forja"], "fonte": str(Fonte), "fontes": Fontes, "exe": str(Exe)}


def FasesDoComando(Nome, Cel):
    """run = (build, se a linguagem tem) + executar o binario; os outros mapeiam 1 para 1 em Cel['comandos']."""
    Cmds = Cel["comandos"]
    if Nome == "run":
        return ([Cmds["build"]] if Cmds.get("build") else []) + [Cmds["run"]]
    return [Cmds[Nome]]


def ExecutarFases(Nome, Cel, Fonte, Args):
    Subs = MontarSubs(Cel, Fonte)
    Fases = FasesDoComando(Nome, Cel)
    Stdin = LerStdin()
    Saida = {"rc": 0, "out": b"", "err": b"", "ms": 0}
    with tempfile.TemporaryDirectory(prefix="ia_trab_", dir=str(PASTA)) as Trabalho:
        if (PASTA / "entrada").is_dir():
            shutil.copytree(PASTA / "entrada", Trabalho, dirs_exist_ok=True)
        for Indice, Molde in enumerate(Fases):
            Extra = Args if Nome == "run" and Indice == len(Fases) - 1 else []
            Cwd = Trabalho if Molde is Cel["comandos"]["run"] else PASTA
            R = Executar(Substituir(Molde, Subs) + Extra, Cwd, Cel, Stdin)
            Saida = {"rc": R["rc"], "out": Saida["out"] + R["out"], "err": Saida["err"] + R["err"], "ms": Saida["ms"] + R["ms"]}
            if R["rc"] != 0:
                break
    return Saida


def LerStdin():
    """Stdin a repassar ao programa. Pipe aberto que nunca fecha nao pode prender o wrapper: espera ESPERA_STDIN s pelo EOF."""
    try:
        if sys.stdin is None or sys.stdin.isatty():
            return b""
        Descritor = sys.stdin.fileno()
    except (OSError, ValueError):
        return b""
    Pedacos = []
    Leitor = threading.Thread(target=LerDoDescritor, args=(Descritor, Pedacos), daemon=True)
    Leitor.start()
    Leitor.join(ESPERA_STDIN)
    return b"".join(Pedacos)


def LerDoDescritor(Descritor, Pedacos):
    """Acumula em Pedacos tudo que chega no descritor ate o EOF (os.read: sem o lock do BufferedReader, nao pendura a saida do processo)."""
    try:
        while True:
            Bloco = os.read(Descritor, 65536)
            if not Bloco:
                return
            Pedacos.append(Bloco)
    except OSError:
        return


def Textos(Saida):
    return Saida["out"].decode("utf-8", errors="replace"), Saida["err"].decode("utf-8", errors="replace")


def Codigos(Saida):
    Visto = []
    for Achado in PADRAO_CODIGO.findall(" ".join(Textos(Saida))):
        if Achado not in Visto:
            Visto.append(Achado)
    return Visto


def Linha(Tipo, Descricao, Saida, ShaAntes, Nomes=None):
    """Linha de idas.jsonl (n so e preenchido pelo chamador, nas idas). Nomes = arquivos do conjunto, so quando sao 2 ou mais."""
    Out, Err = Textos(Saida)
    Item = {"n": None, "hora": datetime.now().isoformat(timespec="seconds"), "tipo": Tipo, "comando": Descricao,
            "rc": Saida["rc"], "chars_stdout": len(Out), "chars_stderr": len(Err), "codigos": Codigos(Saida),
            "sha_fonte": ShaAntes, "ms": Saida["ms"]}
    if Nomes:
        Item["arquivos"] = Nomes
    return Item


def Mostrar(Saida, Resumo):
    sys.stdout.buffer.write(Saida["out"])
    sys.stdout.flush()
    sys.stderr.buffer.write(Saida["err"])
    sys.stderr.write("[ia] %s\n" % Resumo)
    sys.stderr.flush()


def Separar(Argv):
    if "--" in Argv:
        Corte = Argv.index("--")
        return Argv[:Corte], Argv[Corte + 1:]
    return Argv, []


def ArquivoDaCelula(Texto):
    """O fonte tem de estar dentro da celula; devolve o Path absoluto ou None."""
    Fonte = Path(Texto).resolve()
    try:
        Fonte.relative_to(PASTA)
    except ValueError:
        return None
    return Fonte


def Recusar(Cel, Nome, Motivo):
    Saida = {"rc": 2, "out": b"", "err": (Motivo + "\n").encode(), "ms": 0}
    Registrar(Linha("proibido", Nome, Saida, None))
    Mostrar(Saida, "recusado: " + Motivo)
    return 2


def ExecutarIda(Cel, Nome, Fonte, Args):
    """Pacotes P1/P2: o fonte do agente passa por variante.py, o compilador roda no traduzido e o diagnostico volta remapeado."""
    Dados = Cel.get("variante")
    if not Dados:
        return ExecutarFases(Nome, Cel, Fonte, Args)
    sys.path.insert(0, Dados["pasta"])
    import variante
    Destino = PASTA / ".traduzido"
    shutil.rmtree(Destino, ignore_errors=True)
    Trad = variante.TraduzirPasta(Fonte.parent, Destino, Dados["pacote"], Dados["alvo"], Dados["std"])
    if Trad.Erros():
        return {"rc": 1, "out": (variante.ErrosComoJson(Trad) + "\n").encode("utf-8"), "err": b"", "ms": 0}
    Saida = ExecutarFases(Nome, Cel, Destino / Fonte.relative_to(PASTA), Args)
    Remap = lambda Dados_: variante.RemapearTexto(Dados_.decode("utf-8", errors="replace"), Trad).encode("utf-8")
    return dict(Saida, out=Remap(Saida["out"]), err=Remap(Saida["err"]))


def FazerIda(Cel, Nome, Fonte, Args):
    Feitas = ContarIdas()
    if Feitas >= Cel["limite_idas"]:
        Saida = {"rc": 99, "out": b"", "err": b"LIMITE: as %d idas desta tarefa acabaram. Pare agora.\n" % Cel["limite_idas"], "ms": 0}
        Arquivos = ConjuntoDaIda(Cel, Fonte)
        Registrar(Linha("limite", Nome, Saida, ShaDoConjunto(Arquivos) if Arquivos else None, NomesSeConjunto(Arquivos)))
        Mostrar(Saida, "LIMITE")
        return 99
    Sha, Nomes = GuardarEShaDaIda(Cel, Fonte)
    EsperarLock(Cel)
    Saida = ExecutarIda(Cel, Nome, Fonte, Args)
    Item = Linha("ida", Nome, Saida, Sha, Nomes)
    Item["n"] = Feitas + 1
    Registrar(Item)
    Mostrar(Saida, "ida %d de %d (%s) rc=%d" % (Item["n"], Cel["limite_idas"], Nome, Saida["rc"]))
    return Saida["rc"]


def FazerForja(Cel, Nome, Argumentos, Fonte):
    """explicar e spec (consulta) e corrigir (ida de maquina): chamam o forja.exe da rodada e so registram."""
    Tipo = "maquina" if Nome in MAQUINA else "consulta"
    Comando = [Cel["forja"], Nome] + Argumentos + (["--aplicar"] if Nome == "corrigir" else [])
    Antes, Nomes = GuardarEShaDaIda(Cel, Fonte) if Fonte else (None, None)
    EsperarLock(Cel)
    Saida = Executar(Comando, PASTA, Cel, b"")
    Item = Linha(Tipo, Nome + " " + " ".join(Argumentos), Saida, Antes, Nomes)
    if Tipo == "maquina":
        Item["sha_depois"] = GuardarEShaDaIda(Cel, Fonte)[0]
    Registrar(Item)
    Mostrar(Saida, "%s (%s) rc=%d" % (Nome, Tipo, Saida["rc"]))
    return Saida["rc"]


def Despachar(Cel, Antes, Args):
    Nome, Resto = Antes[0], Antes[1:]
    if Cel["conjunto"] == "L":
        return Recusar(Cel, Nome, "tarefa L: nao compile nem execute; escreva a previsao em resposta.txt")
    if Nome in IDAS and len(Resto) == 1 and ArquivoDaCelula(Resto[0]):
        return FazerIda(Cel, Nome, ArquivoDaCelula(Resto[0]), Args)
    if Nome in MAQUINA and Cel.get("variante"):
        return Recusar(Cel, Nome, "corrigir nao existe neste pacote (sintaxe P1/P2)")
    if Nome in CONSULTAS + MAQUINA and Cel["lingua"] == "forja":
        Fonte = ArquivoDaCelula(Resto[0]) if Nome in MAQUINA and Resto else None
        return FazerForja(Cel, Nome, [Fonte.as_posix()] if Fonte else Resto, Fonte)
    print("uso: python ia.py run|check|build|test <arq da celula> [-- args]  |  explicar|spec|corrigir (so Forja)", file=sys.stderr)
    return 2


def Principal(Argv):
    Antes, Args = Separar(Argv)
    if not Antes:
        print("uso: python ia.py run|check|build|test <arq> [-- args]", file=sys.stderr)
        return 2
    Cel = LerCelula()
    if Antes[0] == "test" and not Cel["comandos"].get("test"):
        print("test nao existe nesta linguagem", file=sys.stderr)
        return 2
    return Despachar(Cel, Antes, Args)


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
