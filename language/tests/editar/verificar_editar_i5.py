# verificar_editar_i5.py -- prova do P-IA1-I5: `forja editar` parte 2 (inserir_decl, remover_decl, mover_decl): M-E1 completo (60 operacoes) e M-E2 das 3 operacoes novas
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  me1   M-E1 edicao estrutural, 60 operacoes aplicadas com --aplicar, cada uma em copia de fixture (tests\\editar\\proj ou proj2): 20 renomear, 10 trocar_no/trocar_corpo, 10 inserir_decl,
        10 remover_decl, 10 mover_decl. Em cada uma `forja check` sai 0 e `forja run` e `forja test` dao a MESMA saida da fixture original; nos renomear, nos remover e nos mover nenhum
        no da AST (`forja ast --projeto`) guarda o nome/id antigo (0 referencia esquecida). ORACULO INDEPENDENTE: de 50 das 60 (todas as 30 novas, 10 renomear e os 10 trocar) o texto
        esperado dos arquivos foi escrito a mao em gabaritos_i5.py, lendo o fonte (nao e saida do forja), e o resultado e comparado byte a byte
  me2   M-E2 editar nunca piora, para as 3 operacoes novas: nome repetido, texto sem sintaxe, ancora/id/modulo inexistente, duas posicoes, remover com referencia viva, ciclo de import,
        mover para o mesmo modulo, main nao se move nem se remove: exit 1, nada gravado (arquivos intactos byte a byte, com e sem --aplicar), desfeito com o pacote do check quando a recusa vem do check
  uso   erros de uso das 3 operacoes novas (falta texto, falta para, falta id): exit 2
  crlf  fixture em CRLF: inserir, remover e mover mantem CRLF em todo arquivo (nenhum LF solto) e o programa continua igual
Uso:
    python verificar_editar_i5.py --forja <forja.exe> [--caso me1,me2,uso,crlf] [--jobs N]
    python verificar_editar_i5.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python verificar_editar_i5.py --selftest
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-I5: primeira versao.
Verificacao: python verificar_editar_i5.py --selftest
"""

import argparse
import difflib
import json
import re
import shutil
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import NamedTuple, Optional

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))

import gabaritos_i5 as G  # noqa: E402
from verificar_editar import (RENOMEAR, Caso, CasoRenomear, Copia, HashPasta, JsonDe, NomeAntigo, Rodar, SaidaDoPrograma, Sobras, TemNome,  # noqa: E402
                              TROCAR_CORPO, TROCAR_NO)

PROJ = AQUI / "proj"
PROJ2 = AQUI / "proj2"
TOTAL_ESPERADO = 60
HASH = __import__("hashlib").sha256

# ---------- as 60 operacoes ----------

# renomear com oracle a mao: o rotulo ("alvo -> novo") e a chave de gabaritos_i5.PROJ_RENOMEAR; escopo do nome antigo (parametro/local repetem nome em outros procs) ou None
ESCOPO_RENOMEAR = {"main.soma": "main:main"}
# renomear so com a varredura da AST (os 10 do I4 que o oracle a mao nao cobre)
RENOMEAR_SO_AST = ["geo/vetor:Vetor/campos[1]", "Tipo", "Forma", "Forma.peso", "Forma.centro", "area_aprox.f", "dobro", "acumular.passo", "atualizar.ganho", "Faixa"]

OPS_TROCAR = {
    "trocar_no main:atualizar/corpo[0]/valor": {"op": "trocar_no", "id": "main:atualizar/corpo[0]/valor", "texto": "ganho + p.pontos"},
    "trocar_no geo/forma:area_aprox/corpo[4]/valor": {"op": "trocar_no", "id": "geo/forma:area_aprox/corpo[4]/valor", "texto": "extra + f.peso * base"},
    "trocar_no util/conta:dobro/corpo[0]/valor": {"op": "trocar_no", "id": "util/conta:dobro/corpo[0]/valor", "texto": "n + n"},
    "trocar_no main:main/corpo[12]/valor": {"op": "trocar_no", "id": "main:main/corpo[12]/valor", "texto": "vet.ESCALA * 2"},
    "trocar_no geo/forma:area_aprox/corpo[1]": {"op": "trocar_no", "id": "geo/forma:area_aprox/corpo[1]", "texto": "if f.tipo == Tipo.quadrado {\n        base = 2 + 2\n    }"},
    "trocar_corpo util/conta:dobro": {"op": "trocar_corpo", "id": "util/conta:dobro", "texto": "    return n + n"},
    "trocar_corpo geo/vetor:comprimento_quadrado": {"op": "trocar_corpo", "id": "geo/vetor:comprimento_quadrado", "texto": "    let xx = v.ex * v.ex\n    let yy = v.ey * v.ey\n    return xx + yy"},
    "trocar_corpo main:nota_da_faixa": {"op": "trocar_corpo", "id": "main:nota_da_faixa",
                                        "texto": "    var nota: i32 = 1\n    if f == Faixa.media {\n        nota = 2\n    }\n    if f == Faixa.alta {\n        nota = 3\n    }\n    return nota"},
    "trocar_corpo main:test:acumular": {"op": "trocar_corpo", "id": "main:test:acumular", "texto": "    expect(ct.acumular(1, passo = 1, vezes = 3) == 4)"},
    "trocar_corpo geo/forma:area_aprox": {"op": "trocar_corpo", "id": "geo/forma:area_aprox", "texto": "    var base: i32 = 1\n    if f.tipo == Tipo.quadrado {\n        base = 4\n    }\n    if f.tipo == Tipo.circulo {\n        base = 3\n    }\n    return base * f.peso + f.centro.ex as i32"},
}

# inserir: rotulo (chave de G.INSERIR) -> lista de operacoes (da mesma lista), e os nomes que passam a existir
INSERIR_OPS = {
    "depois_de somar (aritmetica)": ([{"op": "inserir_decl", "depois_de": "somar", "texto": "// Subtrai dois inteiros.\npub proc subtrair(a: i32, b: i32) -> i32 {\n    return a - b\n}"}], ["subtrair"]),
    "antes_de dobrar, com comentario colado (aritmetica)": ([{"op": "inserir_decl", "antes_de": "dobrar", "texto": "pub proc metade(n: i32) -> i32 {\n    return n / 2\n}"}], ["metade"]),
    "no_fim (aritmetica)": ([{"op": "inserir_decl", "modulo": "calc/aritmetica", "no_fim": True, "texto": "pub const TETO: i32 = 1000"}], ["TETO"]),
    "no_inicio_depois_dos_imports (formas)": ([{"op": "inserir_decl", "modulo": "calc/formas", "no_inicio_depois_dos_imports": True, "texto": "pub const LADOS: i32 = 4"}], ["LADOS"]),
    "no_inicio_depois_dos_imports sem import (texto)": ([{"op": "inserir_decl", "modulo": "util/texto", "no_inicio_depois_dos_imports": True, "texto": "pub const SEPARADOR: i32 = 45"}], ["SEPARADOR"]),
    "no_fim no arquivo raiz (main)": ([{"op": "inserir_decl", "modulo": "main", "no_fim": True, "texto": "proc extra() -> i32 {\n    return 7\n}"}], ["extra"]),
    "depois_de Retangulo (struct, formas)": ([{"op": "inserir_decl", "depois_de": "Retangulo", "texto": "pub struct Ponto3 {\n    x: i32 = 0\n    y: i32 = 0\n    z: i32 = 0\n}"}], ["Ponto3"]),
    "antes_de Par, sem comentario (aritmetica)": ([{"op": "inserir_decl", "antes_de": "Par", "texto": "pub enum Sinal { menos, mais }"}], ["Sinal"]),
    "no_fim pelo arquivo util/texto.fj": ([{"op": "inserir_decl", "arquivo": "util/texto.fj", "no_fim": True, "texto": "pub proc eco(n: i32) -> i32 {\n    return n\n}"}], ["eco"]),
    "duas insercoes em pontos diferentes (texto)": ([{"op": "inserir_decl", "depois_de": "moldura", "texto": "pub const A1: i32 = 1"},
                                                     {"op": "inserir_decl", "depois_de": "casas", "texto": "pub const A2: i32 = 2"}], ["A1", "A2"]),
}

# remover: rotulo (chave de G.REMOVER) -> id (e o nome que nao pode sobrar na AST)
REMOVER_OPS = {
    "obsoleta (pub, comentario colado, meio do arquivo)": "obsoleta",
    "moldura (primeira declaracao depois do cabecalho)": "moldura",
    "sobra (ultima declaracao do arquivo)": "sobra",
    "MINIMO (const)": "MINIMO",
    "Modo (enum)": "Modo",
    "Ponto (struct)": "Ponto",
    "dobro_area (o import ar continua em uso)": "dobro_area",
    "legenda (o import tx fica sem uso e sai)": "legenda",
    "Marca (ultima, sem comentario)": "Marca",
    "PASSOS (const logo depois dos imports)": "PASSOS",
}

# mover: rotulo (chave de G.MOVER) -> (id, modulo de destino, id antigo e id novo na AST)
MOVER_OPS = {
    "dobrar: calc/aritmetica -> util/texto": ("dobrar", "util/texto", "calc/aritmetica:dobrar", "util/texto:dobrar"),
    "triplicar: calc/aritmetica -> main (raiz)": ("triplicar", "main", "calc/aritmetica:triplicar", "main:triplicar"),
    "somar_par: calc/aritmetica -> util/texto": ("somar_par", "util/texto", "calc/aritmetica:somar_par", "util/texto:somar_par"),
    "Par: calc/aritmetica -> util/texto": ("Par", "util/texto", "calc/aritmetica:Par", "util/texto:Par"),
    "Cor: calc/formas -> calc/aritmetica": ("Cor", "calc/aritmetica", "calc/formas:Cor", "calc/aritmetica:Cor"),
    "LIMITE: calc/aritmetica -> util/texto": ("LIMITE", "util/texto", "calc/aritmetica:LIMITE", "util/texto:LIMITE"),
    "legenda: main -> util/texto (o import tx sai do main)": ("legenda", "util/texto", "main:legenda", "util/texto:legenda"),
    "resumo: main -> calc/formas": ("resumo", "calc/formas", "main:resumo", "calc/formas:resumo"),
    "rotulo: main -> calc/aritmetica": ("rotulo", "calc/aritmetica", "main:rotulo", "calc/aritmetica:rotulo"),
    "codigo_da_cor: calc/formas -> util/texto (texto passa a importar formas)": ("codigo_da_cor", "util/texto", "calc/formas:codigo_da_cor", "util/texto:codigo_da_cor"),
}
# depois deste mover ninguem importa util/texto, entao `forja ast --projeto` nem lista o modulo: o nome novo nao tem como aparecer la (o gabarito a mao confere o texto)
MOVER_SEM_AST_NOVA = {"legenda: main -> util/texto (o import tx sai do main)"}


# ---------- infraestrutura ----------

class CopiaDe(Copia):
    """Copia de uma fixture qualquer (a Copia do I4 so copia a proj)."""

    def __init__(self, Fixture, Crlf=False):
        self.Raiz = Path(tempfile.mkdtemp(prefix="verificar_editar_i5_"))
        self.Pasta = self.Raiz / "proj"
        shutil.copytree(Fixture, self.Pasta)
        if Crlf:
            for Arquivo in self.Pasta.rglob("*.fj"):
                Arquivo.write_bytes(Arquivo.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))


def TextoEsperado(Fixture, Entrada):
    """{arquivo: texto} depois de aplicar, em ordem, os pares (antigo, novo) do gabarito a mao; cada 'antigo' tem de existir UMA vez (senao o gabarito e que esta errado)."""
    Saida = {}
    for Arquivo, Pares in Entrada.items():
        Texto = (Fixture / Arquivo).read_bytes().decode("utf-8")
        for Antigo, Novo in Pares:
            if Texto.count(Antigo) != 1:
                raise ValueError(f"gabarito errado em {Arquivo}: {Antigo[:50]!r} aparece {Texto.count(Antigo)} vez(es)")
            Texto = Texto.replace(Antigo, Novo)
        Saida[Arquivo] = Texto
    return Saida


def CompararComGabarito(Fixture, Pasta, Entrada):
    """Falhas de uma pasta editada contra o gabarito a mao (arquivo fora da entrada tem de estar intacto)."""
    Esperado = TextoEsperado(Fixture, Entrada)
    Falhas = []
    for Arquivo in sorted(Path(Fixture).rglob("*.fj")):
        Rel = Arquivo.relative_to(Fixture).as_posix()
        Certo = Esperado[Rel] if Rel in Esperado else Arquivo.read_bytes().decode("utf-8")
        Real = (Pasta / Rel).read_bytes().decode("utf-8")
        if Real != Certo:
            Dif = [L.rstrip("\n") for L in difflib.unified_diff(Certo.splitlines(True), Real.splitlines(True), "esperado", "forja", n=0)][:8]
            Falhas.append(f"{Rel} difere do gabarito a mao: {Dif}")
    return Falhas


def AplicarLista(Forja, Copia_, Operacoes):
    """Aplica a lista de operacoes com --aplicar; devolve as falhas da resposta."""
    Rodada = Rodar(Forja, ["editar", ".", "--ops", str(Copia_.Ops(Operacoes)), "--aplicar"], Cwd=Copia_.Pasta)
    Resposta = JsonDe(Rodada.stdout)
    if Rodada.returncode != 0 or Resposta is None:
        return [f"editar exit {Rodada.returncode}: {Rodada.stdout[:200]!r} {Rodada.stderr[:200]!r}"]
    if not (Resposta.get("aplicado") and Resposta.get("valido") and not Resposta.get("desfeito") and Resposta.get("erros_depois") == 0 and Resposta.get("edicoes")):
        return [f"resposta sem aplicado/valido ou sem edicoes: {({K: Resposta.get(K) for K in ('aplicado', 'valido', 'desfeito', 'erros_depois', 'motivo')})}"]
    return []


def ConferirPrograma(Forja, Pasta, Base):
    Check = Rodar(Forja, ["check", "main.fj"], Cwd=Pasta)
    if Check.returncode != 0:
        return [f"check exit {Check.returncode}: {Check.stdout[:200]!r}"]
    return [] if SaidaDoPrograma(Forja, Pasta) == Base else ["run/test deram saida diferente da fixture original"]


class Operacao(NamedTuple):
    """Uma operacao do M-E1: a fixture, a lista de ops, o gabarito a mao (None = so a varredura da AST) e o que a AST tem de provar."""

    Rotulo: str
    Fixture: Path
    Ops: list
    Gabarito: Optional[dict] = None
    Sumiram: tuple = ()     # (nome, escopo): o nome nao pode sobrar na AST
    Surgiram: tuple = ()    # nomes que passam a existir
    IdsAntigos: tuple = ()  # "modulo:nome" que nao pode aparecer em lugar nenhum da AST


def ConferirAst(Forja, Pasta, Op):
    """0 referencia esquecida: nome que sumiu nao aparece na AST; nome novo aparece; id antigo (modulo:nome) nao aparece em lugar nenhum."""
    Ast = JsonDe(Rodar(Forja, ["ast", "--projeto", "main.fj"], Cwd=Pasta).stdout)
    if Ast is None:
        return ["forja ast nao devolveu JSON"]
    Falhas = [f"{Sobras(Ast, N, E)} referencia(s) a '{N}' esquecida(s)" for N, E in Op.Sumiram if Sobras(Ast, N, E)]
    Falhas += [f"o nome novo '{N}' nao aparece na AST" for N in Op.Surgiram if not TemNome(Ast, N)]
    Texto = json.dumps(Ast)
    Falhas += [f"o id antigo '{I}' ainda aparece na AST" for I in Op.IdsAntigos if re.search(re.escape(I) + r"(?![A-Za-z0-9_])", Texto)]
    return Falhas


def CasoOperacao(Forja, Base, Op):
    """Uma operacao do M-E1: aplica, compara com o gabarito a mao (quando ha), check/run/test e AST."""
    with CopiaDe(Op.Fixture) as C:
        Original = HashPasta(C.Pasta)
        Falhas = AplicarLista(Forja, C, Op.Ops)
        if not Falhas:
            Falhas = ConferirPrograma(Forja, C.Pasta, Base)
            if HashPasta(C.Pasta) == Original:
                Falhas.append("nenhum arquivo mudou")
        if not Falhas and Op.Gabarito is not None:
            Falhas = CompararComGabarito(Op.Fixture, C.Pasta, Op.Gabarito)
        if not Falhas and (Op.Sumiram or Op.Surgiram or Op.IdsAntigos):
            Falhas = ConferirAst(Forja, C.Pasta, Op)
        return [f"{Op.Rotulo}: {F}" for F in Falhas]


# ---------- me1 ----------

def TarefasProj(Forja, Base):
    Tarefas = []
    for Rotulo, Gabarito in G.PROJ_RENOMEAR.items():
        Alvo, Novo = Rotulo.split(" -> ")
        Escopo = ESCOPO_RENOMEAR.get(Alvo)
        Ops = [{"op": "renomear", "id": Alvo, "novo": Novo}]
        Tarefas.append((CasoOperacao, (Forja, Base, Operacao("renomear " + Rotulo, PROJ, Ops, Gabarito, ((NomeAntigo(Alvo), Escopo),), (Novo,)))))
    for Alvo in RENOMEAR_SO_AST:
        Dado = next(R for R in RENOMEAR if R[0] == Alvo)
        Tarefas.append((CasoRenomear, (Forja, Base, Dado[0], Dado[1], Dado[2])))
    for Rotulo, Op in OPS_TROCAR.items():
        Tarefas.append((CasoOperacao, (Forja, Base, Operacao(Rotulo, PROJ, [Op], G.PROJ_TROCAR[Rotulo]))))
    return Tarefas


def TarefasProj2(Forja, Base2):
    Tarefas = []
    for Rotulo, (Ops, Novos) in INSERIR_OPS.items():
        Tarefas.append((CasoOperacao, (Forja, Base2, Operacao("inserir " + Rotulo, PROJ2, Ops, G.INSERIR[Rotulo], (), tuple(Novos)))))
    for Rotulo, Id in REMOVER_OPS.items():
        Tarefas.append((CasoOperacao, (Forja, Base2, Operacao("remover " + Rotulo, PROJ2, [{"op": "remover_decl", "id": Id}], G.REMOVER[Rotulo], ((Id, None),)))))
    for Rotulo, (Id, Para, Velho, _Novo) in MOVER_OPS.items():
        Surgiu = () if Rotulo in MOVER_SEM_AST_NOVA else (Id,)
        Tarefas.append((CasoOperacao, (Forja, Base2, Operacao("mover " + Rotulo, PROJ2, [{"op": "mover_decl", "id": Id, "para": Para}], G.MOVER[Rotulo], (), Surgiu, (Velho,)))))
    return Tarefas


def CasoMe1(Forja, Jobs):
    Base, Base2 = SaidaDoPrograma(Forja, PROJ), SaidaDoPrograma(Forja, PROJ2)
    for Nome, B in (("proj", Base), ("proj2", Base2)):
        if B[0] != 0 or B[2] != 0 or not B[1]:
            return Caso("me1", "fixture nao roda", "verde", [f"fixture {Nome}: run/test {B[0]}/{B[2]}"])
    Tarefas = TarefasProj(Forja, Base) + TarefasProj2(Forja, Base2)
    Falhas = []
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        for Futuro in [Pool.submit(F, *A) for F, A in Tarefas]:
            Falhas += Futuro.result()
    if len(Tarefas) != TOTAL_ESPERADO:
        Falhas.append(f"{len(Tarefas)} operacoes (pede {TOTAL_ESPERADO})")
    return Caso("me1", len(Tarefas), f"{TOTAL_ESPERADO}, todas verdes", Falhas)


# ---------- me2 ----------

INSERIR_BASE = {"op": "inserir_decl", "modulo": "calc/aritmetica", "no_fim": True}

# (rotulo, operacoes, codigo do check esperado em "diagnosticos" | None = desfeito ou motivo | "-" = recusa antes do check, com motivo)
RECUSADAS = [
    ("inserir: nome que ja existe", [{**INSERIR_BASE, "texto": "pub proc somar(a: i32) -> i32 {\n    return a\n}"}], "E0202"),
    ("inserir: texto sem sintaxe", [{**INSERIR_BASE, "texto": "pub proc quebrado( {"}], None),
    ("inserir: corpo que nao compila", [{**INSERIR_BASE, "texto": "pub proc ruim() -> i32 {\n    return nao_existe(1)\n}"}], "E0201"),
    ("inserir: ancora inexistente", [{"op": "inserir_decl", "depois_de": "nao_existe", "texto": "pub const X: i32 = 1"}], "-"),
    ("inserir: modulo inexistente", [{"op": "inserir_decl", "modulo": "calc/nada", "no_fim": True, "texto": "pub const X: i32 = 1"}], "-"),
    ("inserir: duas posicoes", [{"op": "inserir_decl", "depois_de": "somar", "no_fim": True, "modulo": "calc/aritmetica", "texto": "pub const X: i32 = 1"}], "-"),
    ("inserir: nenhuma posicao", [{"op": "inserir_decl", "modulo": "calc/aritmetica", "texto": "pub const X: i32 = 1"}], "-"),
    ("inserir: modulo e ancora de modulos diferentes", [{"op": "inserir_decl", "modulo": "util/texto", "depois_de": "somar", "texto": "pub const X: i32 = 1"}], "-"),
    ("remover: ainda tem referencias", [{"op": "remover_decl", "id": "somar"}], "-"),
    ("remover: struct em uso", [{"op": "remover_decl", "id": "Retangulo"}], "-"),
    ("remover: id inexistente", [{"op": "remover_decl", "id": "nao_existe"}], "-"),
    ("remover: main", [{"op": "remover_decl", "id": "main"}], "-"),
    ("remover: campo nao e declaracao", [{"op": "remover_decl", "id": "Par.esq"}], "-"),
    ("remover: forcar com referencia viva", [{"op": "remover_decl", "id": "somar", "forcar": True}], "E0201"),
    ("mover: ciclo de import", [{"op": "mover_decl", "id": "Par", "para": "calc/formas"}], "E1002"),
    ("mover: para o mesmo modulo", [{"op": "mover_decl", "id": "dobrar", "para": "calc/aritmetica"}], "-"),
    ("mover: destino inexistente", [{"op": "mover_decl", "id": "dobrar", "para": "calc/nada"}], "-"),
    ("mover: main", [{"op": "mover_decl", "id": "main", "para": "util/texto"}], "-"),
    ("mover: id inexistente", [{"op": "mover_decl", "id": "nao_existe", "para": "util/texto"}], "-"),
    ("mover: usada por outro modulo para a raiz", [{"op": "mover_decl", "id": "somar", "para": "main"}], None),
    ("inserir + mover da mesma declaracao", [{"op": "inserir_decl", "depois_de": "moldura", "texto": "pub proc dobrar(n: i32) -> i32 {\n    return n\n}"},
                                             {"op": "mover_decl", "id": "dobrar", "para": "util/texto"}], "E0202"),
]


def FalhasDaResposta(Resposta, Codigo):
    Falhas = []
    if Resposta.get("aplicado") or Resposta.get("valido"):
        Falhas.append("a resposta diz aplicado/valido")
    if Codigo not in (None, "-"):
        Codigos = [D.get("codigo") for D in Resposta.get("diagnosticos", [])]
        if not Resposta.get("desfeito") or Codigo not in Codigos:
            Falhas.append(f"esperava desfeito com {Codigo} em diagnosticos, veio desfeito={Resposta.get('desfeito')} {Codigos} {Resposta.get('motivo')}")
    if Codigo is None and not (Resposta.get("desfeito") or Resposta.get("motivo")):
        Falhas.append("sem desfeito e sem motivo")
    if Codigo == "-" and (Resposta.get("desfeito") or not Resposta.get("motivo")):
        Falhas.append(f"recusa antes do check deveria ter motivo e desfeito=false: desfeito={Resposta.get('desfeito')} motivo={Resposta.get('motivo')!r}")
    if Resposta.get("desfeito") and "\\forja_editar_" in json.dumps(Resposta):
        Falhas.append("a resposta vaza o caminho da copia temporaria")
    return Falhas


def CasoRecusada(Forja, Recusada, Aplicar):
    Rotulo, Operacoes, Codigo = Recusada
    with CopiaDe(PROJ2) as C:
        Antes = HashPasta(C.Pasta)
        Argumentos = ["editar", ".", "--ops", str(C.Ops(Operacoes))] + (["--aplicar"] if Aplicar else [])
        Rodada = Rodar(Forja, Argumentos, Cwd=C.Pasta)
        Resposta = JsonDe(Rodada.stdout)
        Nome = f"{Rotulo}{' --aplicar' if Aplicar else ''}"
        Falhas = [] if Rodada.returncode == 1 else [f"exit {Rodada.returncode}, esperado 1"]
        if HashPasta(C.Pasta) != Antes:
            Falhas.append("algum arquivo do projeto mudou")
        Falhas += [f"sem JSON no stdout: {Rodada.stdout[:120]!r}"] if Resposta is None else FalhasDaResposta(Resposta, Codigo)
        return [f"{Nome}: {F}" for F in Falhas]


def CasoMe2(Forja, Jobs):
    Falhas = []
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Futuros = [Pool.submit(CasoRecusada, Forja, Recusada, A) for Recusada in RECUSADAS for A in (False, True)]
        for Futuro in Futuros:
            Falhas += Futuro.result()
    return Caso("me2", len(RECUSADAS) * 2, "todas recusadas, 0 arquivo alterado", Falhas)


# ---------- uso ----------

USOS = [
    ("inserir sem texto", {"op": "inserir_decl", "modulo": "calc/aritmetica", "no_fim": True}),
    ("mover sem para", {"op": "mover_decl", "id": "dobrar"}),
    ("remover sem id", {"op": "remover_decl"}),
    ("mover sem id", {"op": "mover_decl", "para": "util/texto"}),
]


def CasoUso(Forja):
    Falhas = []
    with CopiaDe(PROJ2) as C:
        Antes = HashPasta(C.Pasta)
        for Rotulo, Operacao in USOS:
            Rodada = Rodar(Forja, ["editar", ".", "--ops", str(C.Ops([Operacao]))], Cwd=C.Pasta)
            if Rodada.returncode != 2:
                Falhas.append(f"{Rotulo}: exit {Rodada.returncode}, esperado 2")
        if HashPasta(C.Pasta) != Antes:
            Falhas.append("algum erro de uso alterou o projeto")
    return Caso("uso", len(USOS), "todos com exit 2", Falhas)


# ---------- crlf ----------

OPS_CRLF = [
    ("inserir", [{"op": "inserir_decl", "depois_de": "somar", "texto": "pub proc subtrair(a: i32, b: i32) -> i32 {\n    return a - b\n}"}]),
    ("remover", [{"op": "remover_decl", "id": "obsoleta"}]),
    ("mover", [{"op": "mover_decl", "id": "dobrar", "para": "util/texto"}]),
]


def LfSolto(Pasta):
    return [A.relative_to(Pasta).as_posix() for A in Path(Pasta).rglob("*.fj") if re.search(rb"(?<!\r)\n", A.read_bytes())]


def CasoCrlfUma(Forja, Base, Rotulo, Operacoes):
    with CopiaDe(PROJ2, Crlf=True) as C:
        Falhas = AplicarLista(Forja, C, Operacoes)
        if not Falhas:
            Falhas = ConferirPrograma(Forja, C.Pasta, Base)
        Soltos = LfSolto(C.Pasta)
        if Soltos:
            Falhas.append(f"LF solto em {Soltos}")
        return [f"crlf {Rotulo}: {F}" for F in Falhas]


def CasoCrlf(Forja):
    Base = SaidaDoPrograma(Forja, PROJ2)
    Falhas = []
    for Rotulo, Operacoes in OPS_CRLF:
        Falhas += CasoCrlfUma(Forja, Base, Rotulo, Operacoes)
    return Caso("crlf", len(OPS_CRLF), "CRLF preservado", Falhas)


def RodarCasos(Forja, Quais, Jobs):
    Saidas = []
    Tabela = {"me1": lambda: CasoMe1(Forja, Jobs), "me2": lambda: CasoMe2(Forja, Jobs), "uso": lambda: CasoUso(Forja), "crlf": lambda: CasoCrlf(Forja)}
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

    Exigir("rotulos de inserir batem", set(INSERIR_OPS) == set(G.INSERIR))
    Exigir("rotulos de remover batem", set(REMOVER_OPS) == set(G.REMOVER))
    Exigir("rotulos de mover batem", set(MOVER_OPS) == set(G.MOVER))
    Exigir("rotulos de renomear e trocar batem", set(OPS_TROCAR) == set(G.PROJ_TROCAR) and len(G.PROJ_RENOMEAR) == 10)
    Exigir("renomear so com a AST existem no I4", all(any(R[0] == A for R in RENOMEAR) for A in RENOMEAR_SO_AST) and len(RENOMEAR_SO_AST) == 10)
    Exigir("trocar existem no I4", len(TROCAR_NO) >= 5 and len(TROCAR_CORPO) >= 5)
    Exigir("60 operacoes", len(G.PROJ_RENOMEAR) + len(RENOMEAR_SO_AST) + len(OPS_TROCAR) + len(INSERIR_OPS) + len(REMOVER_OPS) + len(MOVER_OPS) == TOTAL_ESPERADO)
    Exigir("50 com gabarito a mao", len(G.PROJ_RENOMEAR) + len(G.PROJ_TROCAR) + len(G.INSERIR) + len(G.REMOVER) + len(G.MOVER) == 50)
    for Fixture, Entradas in ((PROJ, list(G.PROJ_RENOMEAR.items()) + list(G.PROJ_TROCAR.items())),
                              (PROJ2, list(G.INSERIR.items()) + list(G.REMOVER.items()) + list(G.MOVER.items()))):
        for Rotulo, Entrada in Entradas:
            try:
                Esperado = TextoEsperado(Fixture, Entrada)
                Exigir(f"gabarito muda algo: {Rotulo}", any(Esperado[A] != (Fixture / A).read_bytes().decode("utf-8") for A in Esperado))
            except (ValueError, OSError) as Erro:
                Erros.append(f"{Rotulo}: {Erro}")
    Exigir("recusadas das 3 operacoes", len(RECUSADAS) >= 15 and {Op["op"] for _, Lista, _ in RECUSADAS for Op in Lista} == {"inserir_decl", "remover_decl", "mover_decl"})
    Exigir("Caso vermelho", not Caso("x", "", "", ["f"])["verde"])
    with CopiaDe(PROJ2, Crlf=True) as C:
        Exigir("CopiaDe CRLF", not LfSolto(C.Pasta) and (C.Pasta / "main.fj").read_bytes().count(b"\r\n") > 10)
    for E in Erros:
        print(json.dumps({"selftest": E, "verde": False}, ensure_ascii=False))
    print(json.dumps({"selftest": "verificar_editar_i5", "verde": not Erros, "falhas": len(Erros)}))
    return 0 if not Erros else 1


def Principal():
    Args = argparse.ArgumentParser(description="P-IA1-I5: forja editar parte 2")
    Args.add_argument("--forja")
    Args.add_argument("--caso", default="me1,me2,uso,crlf")
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
