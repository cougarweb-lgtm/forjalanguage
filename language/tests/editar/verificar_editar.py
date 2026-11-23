# verificar_editar.py -- prova do P-IA1-I4: `forja editar` (renomear, trocar_no, trocar_corpo): M-E1 (a parte do I4: 20+ renomear, 5 trocar_no, 5 trocar_corpo) e M-E2
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  me1   M-E1 edicao estrutural: em copia da fixture tests\\editar\\proj (main + geo/vetor + geo/forma + util/conta), cada operacao e aplicada com --aplicar; depois `forja check` sai 0,
        `forja run` e `forja test` dao a MESMA saida da fixture original e, nos renomear, nenhum no da AST (`forja ast --projeto`) guarda o nome antigo (0 referencia esquecida:
        campo "nome" de declaracao, uso, rotulo de argumento nomeado e de literal; componente de "id"/"ref"). Pede >= 30 operacoes (24 renomear, 5 trocar_no, 5 trocar_corpo)
  me2   M-E2 editar nunca piora: colisao de nome (E0202), tipo trocado que quebra (E0301), corpo com nome inexistente, sintaxe quebrada e nome-palavra-chave sao DESFEITOS: exit 1, "desfeito": true,
        o pacote de diagnostico do check em "diagnosticos" e todos os arquivos intactos byte a byte (com e sem --aplicar); id inexistente e nome recusado (main, foreign) = exit 1 sem mexer em nada
  gab   gabarito de saida: o dry-run do renomear entre modulos devolve EXATAMENTE tests\\editar\\gabarito\\renomear_entre_modulos.json (e uma 2a execucao os mesmos bytes), sem gravar nada;
        duas operacoes na mesma lista valem juntas; a copia temporaria do check some
  uso   erros de uso: sem --ops, ops.json ausente, JSON invalido, operacao desconhecida ou incompleta, opcao desconhecida = exit 2; duas operacoes no mesmo trecho = exit 1 sem gravar
Uso:
    python verificar_editar.py --forja <forja.exe> [--caso me1,me2] [--jobs N]
    python verificar_editar.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python verificar_editar.py --selftest
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-I4: primeira versao.
Verificacao: python verificar_editar.py --selftest
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

AQUI = Path(__file__).resolve().parent
PROJ = AQUI / "proj"
GABARITO = AQUI / "gabarito" / "renomear_entre_modulos.json"
LIMITE_FALHAS = 8
MINIMO_OPERACOES = 30

# (alvo, novo, escopo): escopo = id da declaracao onde a busca do nome antigo vale (parametro e local repetem nome em outros procs); None = o projeto todo
RENOMEAR = [
    ("geo/vetor:somar_vetor", "juntar_vetores", None),
    ("comprimento_quadrado", "norma_quadrada", None),
    ("Vetor", "Vec2", None),
    ("Vetor.ex", "px", None),
    ("geo/vetor:Vetor/campos[1]", "py", None),
    ("ESCALA", "FATOR", None),
    ("Tipo", "Categoria", None),
    ("Tipo.quadrado", "retangulo", None),
    ("Forma", "Figura", None),
    ("Forma.peso", "massa", None),
    ("Forma.centro", "origem", None),
    ("area_aprox", "calcular_area", None),
    ("area_aprox.f", "figura", "geo/forma:area_aprox"),
    ("area_aprox.base", "area_base", "geo/forma:area_aprox"),
    ("dobro", "duplicar", None),
    ("acumular", "somar_passos", None),
    ("acumular.passo", "incremento", "util/conta:acumular"),
    ("acumular.vezes", "repeticoes", "util/conta:acumular"),
    ("Placar.rodadas", "partidas", None),
    ("atualizar.ganho", "pontos_ganhos", "main:atualizar"),
    ("main.soma", "resultante", "main:main"),
    ("Faixa", "Classe", None),
    ("util/conta:acumular/total@7:5", "acumulado", "util/conta:acumular"),
    ("main.i", "indice", "main:main"),
]

TROCAR_NO = [
    ("main:atualizar/corpo[0]/valor", "ganho + p.pontos"),
    ("geo/forma:area_aprox/corpo[4]/valor", "extra + f.peso * base"),
    ("util/conta:dobro/corpo[0]/valor", "n + n"),
    ("main:main/corpo[12]/valor", "vet.ESCALA * 2"),
    ("geo/forma:area_aprox/corpo[1]", "if f.tipo == Tipo.quadrado {\n        base = 2 + 2\n    }"),
]

TROCAR_CORPO = [
    ("util/conta:dobro", "    return n + n"),
    ("geo/vetor:comprimento_quadrado", "    let xx = v.ex * v.ex\n    let yy = v.ey * v.ey\n    return xx + yy"),
    ("main:nota_da_faixa", "    var nota: i32 = 1\n    if f == Faixa.media {\n        nota = 2\n    }\n    if f == Faixa.alta {\n        nota = 3\n    }\n    return nota"),
    ("main:test:acumular", "    expect(ct.acumular(1, passo = 1, vezes = 3) == 4)"),
    ("geo/forma:area_aprox", "    var base: i32 = 1\n    if f.tipo == Tipo.quadrado {\n        base = 4\n    }\n    if f.tipo == Tipo.circulo {\n        base = 3\n    }\n    return base * f.peso + f.centro.ex as i32"),
]

# M-E2: (rotulo, operacao, codigo do check esperado ou None quando a operacao e recusada antes do check)
RECUSADAS = [
    ("colisao de nome", {"op": "renomear", "id": "atualizar", "novo": "faixa_de"}, "E0202"),
    ("tipo trocado que quebra", {"op": "trocar_no", "id": "main:atualizar/corpo[0]/valor", "texto": "p.pontos + \"x\""}, "E0307"),
    ("corpo com nome inexistente", {"op": "trocar_corpo", "id": "util/conta:dobro", "texto": "    return nao_existe(n)"}, "E0201"),
    ("sintaxe quebrada no no", {"op": "trocar_no", "id": "util/conta:dobro/corpo[0]/valor", "texto": "n *"}, None),
    ("nome-palavra-chave", {"op": "renomear", "id": "dobro", "novo": "proc"}, None),
    ("renomear coloca um nome que ja existe no escopo", {"op": "renomear", "id": "acumular.passo", "novo": "vezes"}, None),
    ("id inexistente", {"op": "trocar_no", "id": "main:nada/corpo[0]", "texto": "1"}, "-"),
    ("nome inexistente", {"op": "renomear", "id": "nao_existe", "novo": "x"}, "-"),
    ("main nao se renomeia", {"op": "renomear", "id": "main", "novo": "principal"}, "-"),
    ("nome embutido", {"op": "renomear", "id": "print", "novo": "mostrar"}, "-"),
    ("biblioteca padrao embutida", {"op": "trocar_corpo", "id": "main:test:acumular", "texto": "}"}, None),
]


def Rodar(Forja, Argumentos, Cwd=None):
    return subprocess.run([str(Forja), *Argumentos], capture_output=True, timeout=300, stdin=subprocess.DEVNULL, cwd=Cwd)


def JsonDe(Bytes):
    try:
        return json.loads(Bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None


def Caso(Id, Valor, Meta, Falhas):
    return {"id": Id, "valor": Valor, "meta": Meta, "verde": not Falhas, "falhas": Falhas[:LIMITE_FALHAS]}


def HashPasta(Pasta):
    """{caminho relativo: sha256} de todo arquivo da pasta."""
    Saida = {}
    for Arquivo in sorted(Path(Pasta).rglob("*")):
        if Arquivo.is_file():
            Saida[Arquivo.relative_to(Pasta).as_posix()] = hashlib.sha256(Arquivo.read_bytes()).hexdigest()
    return Saida


class Copia:
    """Uma copia da fixture em pasta temporaria (com a pasta do ops.json ao lado, fora do projeto)."""

    def __init__(self):
        self.Raiz = Path(tempfile.mkdtemp(prefix="verificar_editar_"))
        self.Pasta = self.Raiz / "proj"
        shutil.copytree(PROJ, self.Pasta)

    def Ops(self, Lista):
        Arquivo = self.Raiz / "ops.json"
        Arquivo.write_text(json.dumps(Lista), encoding="utf-8")
        return Arquivo

    def __enter__(self):
        return self

    def __exit__(self, *_):
        shutil.rmtree(self.Raiz, ignore_errors=True)


def SaidaDoPrograma(Forja, Pasta):
    """(exit e stdout do `run` + exit e stdout do `test`) da pasta."""
    Run = Rodar(Forja, ["run", "main.fj"], Cwd=Pasta)
    Test = Rodar(Forja, ["test", "main.fj"], Cwd=Pasta)
    return (Run.returncode, Run.stdout, Test.returncode, Test.stdout)


# ---------- sobras do nome antigo na AST ----------

def Componentes(Id):
    return [P for P in re.split(r"[:/\[\]@]", Id) if P]


def Sobras(No, Antigo, Escopo=None, Dentro=None):
    """Quantos nos da AST ainda guardam `Antigo`: campo "nome" (menos o nome-texto de um `test`) ou componente de "ref" (id da declaracao a que o uso aponta).
    O "id" nao conta: as partes estruturais dele (base, a, b, campos) coincidem com nomes de variavel. Com Escopo so conta dentro da declaracao de id Escopo."""
    if Dentro is None:
        Dentro = Escopo is None
    Total = 0
    if isinstance(No, dict):
        if Escopo is not None and No.get("id") == Escopo:
            Dentro = True
        if Dentro:
            if No.get("nome") == Antigo and No.get("no") != "test":
                Total += 1
            if isinstance(No.get("ref"), str) and Antigo in Componentes(No["ref"]):
                Total += 1
        Total += sum(Sobras(V, Antigo, Escopo, Dentro) for V in No.values())
    elif isinstance(No, list):
        Total += sum(Sobras(V, Antigo, Escopo, Dentro) for V in No)
    return Total


def TemNome(No, Nome):
    if isinstance(No, dict):
        return No.get("nome") == Nome or any(TemNome(V, Nome) for V in No.values())
    if isinstance(No, list):
        return any(TemNome(V, Nome) for V in No)
    return False


def NomeAntigo(Alvo):
    """O nome simples que o alvo do renomear tem hoje (o que nao pode sobrar)."""
    if "@" in Alvo:
        return Alvo.split("@")[0].rsplit("/", 1)[1]
    if "/campos[" in Alvo:
        return {"geo/vetor:Vetor/campos[1]": "ey"}[Alvo]
    return Alvo.rsplit(".", 1)[-1].rsplit(":", 1)[-1]


# ---------- me1 ----------

def ConferirAplicada(Forja, Pasta, Base):
    """Falhas de uma copia onde a operacao foi aplicada: check, run e test iguais aos da fixture."""
    Falhas = []
    Check = Rodar(Forja, ["check", "main.fj"], Cwd=Pasta)
    if Check.returncode != 0:
        Falhas.append(f"check exit {Check.returncode}: {Check.stdout[:160]!r}")
        return Falhas
    if SaidaDoPrograma(Forja, Pasta) != Base:
        Falhas.append("run/test deram saida diferente da fixture original")
    return Falhas


def AplicarOperacao(Forja, Copia_, Operacao):
    Arquivo = Copia_.Ops([Operacao])
    Rodada = Rodar(Forja, ["editar", ".", "--ops", str(Arquivo), "--aplicar"], Cwd=Copia_.Pasta)
    Resposta = JsonDe(Rodada.stdout)
    Falhas = []
    if Rodada.returncode != 0 or Resposta is None:
        return [f"editar exit {Rodada.returncode}: {Rodada.stdout[:200]!r} {Rodada.stderr[:200]!r}"], Resposta
    if not (Resposta.get("aplicado") and Resposta.get("valido") and not Resposta.get("desfeito") and Resposta.get("erros_depois") == 0 and Resposta.get("edicoes")):
        Falhas.append(f"resposta sem aplicado/valido ou sem edicoes: {({K: Resposta.get(K) for K in ('aplicado', 'valido', 'desfeito', 'erros_depois', 'motivo')})}")
    return Falhas, Resposta


def CasoRenomear(Forja, Base, Alvo, Novo, Escopo):
    with Copia() as C:
        Falhas, _ = AplicarOperacao(Forja, C, {"op": "renomear", "id": Alvo, "novo": Novo})
        if Falhas:
            return [f"renomear {Alvo} -> {Novo}: {F}" for F in Falhas]
        Falhas = ConferirAplicada(Forja, C.Pasta, Base)
        Ast = JsonDe(Rodar(Forja, ["ast", "--projeto", "main.fj"], Cwd=C.Pasta).stdout)
        if Ast is None:
            return [f"renomear {Alvo}: forja ast nao devolveu JSON"]
        Antigo = NomeAntigo(Alvo)
        if Sobras(Ast, Antigo, Escopo):
            Falhas.append(f"{Sobras(Ast, Antigo, Escopo)} referencia(s) a '{Antigo}' esquecida(s)")
        if not TemNome(Ast, Novo):
            Falhas.append(f"o nome novo '{Novo}' nao aparece na AST")
        return [f"renomear {Alvo} -> {Novo}: {F}" for F in Falhas]


def CasoTroca(Forja, Base, Operacao):
    with Copia() as C:
        Original = HashPasta(C.Pasta)
        Falhas, _ = AplicarOperacao(Forja, C, Operacao)
        if not Falhas:
            Falhas = ConferirAplicada(Forja, C.Pasta, Base)
            if HashPasta(C.Pasta) == Original:
                Falhas.append("nenhum arquivo mudou")
        return [f"{Operacao['op']} {Operacao['id']}: {F}" for F in Falhas]


def CasoMe1(Forja, Jobs):
    Base = SaidaDoPrograma(Forja, PROJ)
    Falhas = []
    if Base[0] != 0 or Base[2] != 0 or not Base[1]:
        return Caso("me1", "fixture nao roda", "verde", [f"fixture original: run/test {Base[0]}/{Base[2]}"])
    Tarefas = [(CasoRenomear, (Forja, Base, A, N, E)) for A, N, E in RENOMEAR]
    Tarefas += [(CasoTroca, (Forja, Base, {"op": "trocar_no", "id": I, "texto": T})) for I, T in TROCAR_NO]
    Tarefas += [(CasoTroca, (Forja, Base, {"op": "trocar_corpo", "id": I, "texto": T})) for I, T in TROCAR_CORPO]
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Futuros = [Pool.submit(F, *A) for F, A in Tarefas]
        for Futuro in Futuros:
            Falhas += Futuro.result()
    if len(Tarefas) < MINIMO_OPERACOES:
        Falhas.append(f"so {len(Tarefas)} operacoes (pede {MINIMO_OPERACOES})")
    return Caso("me1", len(Tarefas), f">= {MINIMO_OPERACOES}, todas verdes", Falhas)


# ---------- me2 ----------

def CasoRecusada(Forja, Rotulo, Operacao, Codigo, Aplicar):
    with Copia() as C:
        Antes = HashPasta(C.Pasta)
        Argumentos = ["editar", ".", "--ops", str(C.Ops([Operacao]))] + (["--aplicar"] if Aplicar else [])
        Rodada = Rodar(Forja, Argumentos, Cwd=C.Pasta)
        Resposta = JsonDe(Rodada.stdout)
        Falhas = []
        Nome = f"{Rotulo}{' --aplicar' if Aplicar else ''}"
        if Rodada.returncode != 1:
            Falhas.append(f"exit {Rodada.returncode}, esperado 1")
        if HashPasta(C.Pasta) != Antes:
            Falhas.append("algum arquivo do projeto mudou")
        if Resposta is None:
            return [f"{Nome}: sem JSON no stdout"] + [f"{Nome}: {F}" for F in Falhas]
        if Resposta.get("aplicado") or Resposta.get("valido"):
            Falhas.append("a resposta diz aplicado/valido")
        if Codigo not in (None, "-"):
            Codigos = [D.get("codigo") for D in Resposta.get("diagnosticos", [])]
            if not Resposta.get("desfeito") or Codigo not in Codigos:
                Falhas.append(f"esperava desfeito com {Codigo} em diagnosticos, veio desfeito={Resposta.get('desfeito')} {Codigos}")
        if Codigo is None and not (Resposta.get("desfeito") or Resposta.get("motivo")):
            Falhas.append("sem desfeito e sem motivo")
        if Codigo == "-" and (Resposta.get("desfeito") or not Resposta.get("motivo")):
            Falhas.append("recusa antes do check deveria ter motivo e desfeito=false")
        if Resposta.get("desfeito") and "\\forja_editar_" in json.dumps(Resposta):
            Falhas.append("a resposta vaza o caminho da copia temporaria")
        return [f"{Nome}: {F}" for F in Falhas]


def CasoMe2(Forja, Jobs):
    Falhas = []
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Futuros = [Pool.submit(CasoRecusada, Forja, R, O, C, A) for R, O, C in RECUSADAS for A in (False, True)]
        for Futuro in Futuros:
            Falhas += Futuro.result()
    return Caso("me2", len(RECUSADAS) * 2, "todas recusadas, 0 arquivo alterado", Falhas)


# ---------- gab ----------

def PastasTemporarias():
    return {P.name for P in Path(tempfile.gettempdir()).glob("forja_editar_*")}


def CasoGab(Forja):
    Falhas = []
    Operacao = {"op": "renomear", "id": "geo/vetor:somar_vetor", "novo": "juntar_vetores"}
    with Copia() as C:
        Antes, Pastas = HashPasta(C.Pasta), PastasTemporarias()
        Arquivo = C.Ops([Operacao])
        A = Rodar(Forja, ["editar", ".", "--ops", str(Arquivo)], Cwd=C.Pasta)
        B = Rodar(Forja, ["editar", ".", "--ops", str(Arquivo)], Cwd=C.Pasta)
        if A.returncode != 0:
            Falhas.append(f"dry-run exit {A.returncode}")
        if A.stdout != B.stdout:
            Falhas.append("2 execucoes deram bytes diferentes")
        if GABARITO.exists():
            if A.stdout != GABARITO.read_bytes():
                Falhas.append("saida diferente de gabarito/renomear_entre_modulos.json")
        else:
            Falhas.append("falta o gabarito tests\\editar\\gabarito\\renomear_entre_modulos.json")
        if HashPasta(C.Pasta) != Antes:
            Falhas.append("o dry-run gravou arquivo")
        if PastasTemporarias() - Pastas:
            Falhas.append("a copia temporaria do check ficou no TEMP")
        Dupla = C.Ops([Operacao, {"op": "renomear", "id": "dobro", "novo": "duplicar"}, {"op": "trocar_no", "id": "util/conta:acumular/corpo[0]/valor", "texto": "inicio"}])
        Rodada = Rodar(Forja, ["editar", ".", "--ops", str(Dupla), "--aplicar"], Cwd=C.Pasta)
        Resposta = JsonDe(Rodada.stdout) or {}
        if Rodada.returncode != 0 or not Resposta.get("aplicado") or len(Resposta.get("operacoes", [])) != 3:
            Falhas.append(f"3 operacoes juntas: exit {Rodada.returncode} {Resposta.get('motivo')}")
        elif Rodar(Forja, ["check", "main.fj"], Cwd=C.Pasta).returncode != 0:
            Falhas.append("3 operacoes juntas: o check do resultado falhou")
    return Caso("gab", "gabarito", "bytes iguais", Falhas)


# ---------- uso ----------

def CasoUso(Forja):
    Falhas = []
    with Copia() as C:
        Antes = HashPasta(C.Pasta)
        Validas = C.Ops([{"op": "renomear", "id": "dobro", "novo": "duplicar"}])
        Ruins = C.Raiz / "ruim.json"
        Ruins.write_text("{nao e json", encoding="utf-8")
        Desconhecida = C.Raiz / "desconhecida.json"
        Desconhecida.write_text(json.dumps([{"op": "inventar", "id": "x"}]), encoding="utf-8")
        Incompleta = C.Raiz / "incompleta.json"
        Incompleta.write_text(json.dumps([{"op": "renomear", "id": "dobro"}]), encoding="utf-8")
        Conflito = C.Raiz / "conflito.json"
        Conflito.write_text(json.dumps([{"op": "renomear", "id": "dobro", "novo": "a1"}, {"op": "renomear", "id": "util/conta:dobro", "novo": "b2"}]), encoding="utf-8")
        Usos = [("sem --ops", ["editar", "."]), ("ops ausente", ["editar", ".", "--ops", str(C.Raiz / "nada.json")]), ("JSON invalido", ["editar", ".", "--ops", str(Ruins)]),
                ("operacao desconhecida", ["editar", ".", "--ops", str(Desconhecida)]), ("operacao incompleta", ["editar", ".", "--ops", str(Incompleta)]),
                ("opcao desconhecida", ["editar", ".", "--ops", str(Validas), "--zz"]), ("2 alvos", ["editar", ".", "..", "--ops", str(Validas)]), ("--ops sem valor", ["editar", ".", "--ops"])]
        for Rotulo, Argumentos in Usos:
            Rodada = Rodar(Forja, Argumentos, Cwd=C.Pasta)
            if Rodada.returncode != 2:
                Falhas.append(f"{Rotulo}: exit {Rodada.returncode}, esperado 2")
        Rodada = Rodar(Forja, ["editar", ".", "--ops", str(Conflito), "--aplicar"], Cwd=C.Pasta)
        if Rodada.returncode != 1 or "mesmo trecho" not in Rodada.stderr.decode("utf-8", "replace"):
            Falhas.append(f"duas operacoes no mesmo trecho: exit {Rodada.returncode}, esperado 1 com 'mesmo trecho' no stderr")
        Quebrado = C.Raiz / "quebrado"
        shutil.copytree(PROJ, Quebrado)
        (Quebrado / "main.fj").write_text("proc main() {\n    x\n}\n", encoding="utf-8")
        Rodada = Rodar(Forja, ["editar", str(Quebrado), "--ops", str(Validas)], Cwd=C.Raiz)
        if Rodada.returncode != 1:
            Falhas.append(f"projeto com erro: exit {Rodada.returncode}, esperado 1")
        Rodada = Rodar(Forja, ["editar", str(C.Raiz / "nao_existe"), "--ops", str(Validas)], Cwd=C.Raiz)
        if Rodada.returncode != 2:
            Falhas.append(f"alvo inexistente: exit {Rodada.returncode}, esperado 2")
        if HashPasta(C.Pasta) != Antes:
            Falhas.append("algum erro de uso alterou o projeto")
    return Caso("uso", len(Usos) + 3, "todos com o exit certo", Falhas)


def RodarCasos(Forja, Quais, Jobs):
    Saidas = []
    Tabela = {"me1": lambda: CasoMe1(Forja, Jobs), "me2": lambda: CasoMe2(Forja, Jobs), "gab": lambda: CasoGab(Forja), "uso": lambda: CasoUso(Forja)}
    for Id in Quais:
        try:
            Saidas.append(Tabela[Id]())
        except Exception as Erro:  # noqa: BLE001 -- um caso que quebra vira vermelho, nao derruba a rodada
            Saidas.append(Caso(Id, "quebrou", "verde", [f"{type(Erro).__name__}: {Erro}"]))
    return Saidas


# ---------- selftest ----------

def Selftest():
    Erros = []

    def Exigir(Nome, Condicao):
        if not Condicao:
            Erros.append(Nome)

    Ast = {"modulos": [{"declaracoes": [{"no": "proc", "id": "m:f", "nome": "f", "params": [{"id": "m:f/params[0]", "nome": "a"}],
                                         "corpo": [{"no": "let", "id": "m:f/corpo[0]", "nome": "x", "valor": {"no": "nome", "id": "m:f/corpo[0]/valor", "ref": "m:f/params[0]", "nome": "a"}}]},
                                        {"no": "proc", "id": "m:g", "nome": "g", "params": [{"id": "m:g/params[0]", "nome": "a"}]}]}]}
    Exigir("Sobras global", Sobras(Ast, "f") == 2)
    Exigir("Sobras de param no escopo", Sobras(Ast, "a", "m:f") == 2)
    Exigir("Sobras de param sem escopo", Sobras(Ast, "a") == 3)
    Exigir("Sobras de nome que nao existe", Sobras(Ast, "zz") == 0)
    Exigir("Sobras conta componente de ref", Sobras({"ref": "geo/m:velho/params[0]"}, "velho") == 1)
    Exigir("Sobras nao confunde prefixo", Sobras({"ref": "geo/m:velhote"}, "velho") == 0)
    Exigir("Sobras ignora id estrutural e nome de test", Sobras({"id": "m:f/x/base", "nome": "z"}, "base") == 0 and Sobras({"no": "test", "nome": "acumular"}, "acumular") == 0)
    Exigir("TemNome", TemNome(Ast, "x") and not TemNome(Ast, "zz"))
    Exigir("NomeAntigo global", NomeAntigo("geo/vetor:somar_vetor") == "somar_vetor" and NomeAntigo("Vetor") == "Vetor")
    Exigir("NomeAntigo membro", NomeAntigo("Tipo.quadrado") == "quadrado")
    Exigir("NomeAntigo local por id", NomeAntigo("util/conta:acumular/total@7:5") == "total")
    Exigir("Componentes", Componentes("a/b:c/params[0]") == ["a", "b", "c", "params", "0"])
    Exigir("Caso vermelho", not Caso("x", "", "", ["f"])["verde"] and Caso("x", "", "", [])["verde"])
    Exigir("fixture e gabarito existem", (PROJ / "main.fj").exists() and (PROJ / "geo" / "forma.fj").exists())
    Exigir("operacoes suficientes", len(RENOMEAR) + len(TROCAR_NO) + len(TROCAR_CORPO) >= MINIMO_OPERACOES and len(RENOMEAR) >= 20 and len(TROCAR_NO) >= 5 and len(TROCAR_CORPO) >= 5)
    with Copia() as C:
        Exigir("Copia tem a fixture", (C.Pasta / "util" / "conta.fj").exists() and HashPasta(C.Pasta) == HashPasta(PROJ))
    for E in Erros:
        print(json.dumps({"selftest": E, "verde": False}))
    print(json.dumps({"selftest": "verificar_editar", "verde": not Erros, "falhas": Erros}))
    return 0 if not Erros else 1


def Principal():
    Args = argparse.ArgumentParser(description="P-IA1-I4: forja editar")
    Args.add_argument("--forja")
    Args.add_argument("--caso", default="me1,me2,gab,uso")
    Args.add_argument("--jobs", type=int, default=6)
    Args.add_argument("--esperar-vermelho", action="store_true")
    Args.add_argument("--selftest", action="store_true")
    Dados = Args.parse_args()
    if Dados.selftest:
        return Selftest()
    if not Dados.forja:
        Args.error("--forja e obrigatorio (ou --selftest)")
    Saidas = RodarCasos(Path(Dados.forja), [C for C in Dados.caso.split(",") if C], Dados.jobs)
    for S in Saidas:
        print(json.dumps(S, ensure_ascii=False))
    AlgumVermelho = any(not S["verde"] for S in Saidas)
    if Dados.esperar_vermelho:
        return 0 if AlgumVermelho else 1
    return 1 if AlgumVermelho else 0


if __name__ == "__main__":
    sys.exit(Principal())
