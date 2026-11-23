# -*- coding: utf-8 -*-
"""montar_conjuntos.py - monta os conjuntos L, Cx, Dx, C_forja e D_forja a partir de montagem\\dados (P-IA1-a06).

Versao 0.2.1
Changelog:
  0.2.1 (14/11/2026) Portao Z: SEMANTICA aponta para language\tests\semantica (antes benchmark\semantica, que nao existe).
  0.2.0 (11/11/2026) P-IA1-a17: `--conjunto P,R` delega para montar_pr.py (so quando pedido; o padrao continua L,Cx,Dx,C_forja,D_forja).
  0.1.0 (11/11/2026) primeira versao: le os blocos de dados, escreve tarefas\\ e referencias\\, roda a
        referencia e o programa com defeito na copia congelada da Forja e no gcc, e grava
        esperado.json, gabarito.json, diagnostico_inicial.txt, pedido.txt e catalogo.json.

Formato dos dados (um arquivo por conjunto, blocos '### ID | texto'):
  L: L_forja.txt, L_c.txt (programas) e L_gabarito.txt ('### L01 | exit=0' + stdout previsto).
  Cx, Dx, C_forja, D_forja: secoes '@chave [valor na linha]' + corpo. Chaves: titulo, classe, pedido,
  <lingua>.ref (fonte certa), <lingua>.troca / <lingua>.por (trecho da fonte certa -> trecho com defeito),
  <lingua>.fixture / <lingua>.de / <lingua>.para (defeito vindo de tests\\semantica; de -> para conserta),
  <lingua>.decl (JSON: o que o defeito tem de fazer: fase, codigo, padrao, exit, exit_nao_zero, saida_difere, modo).

Uso:
  python -I montar_conjuntos.py --forja <forja.exe> [--conjunto L,Cx,Dx,C_forja,D_forja]
A fonte da verdade do que o defeito faz e o verificar_conjunto.py (rodar depois). Este script SOBRESCREVE tarefas\\<conj>
e referencias\\*\\<conj> dos conjuntos pedidos; nao toca em tarefas\\E nem em verificar_tarefa.py.
Reversao: apagar as pastas geradas e rodar de novo.
Depende de: Python 3.9+, gcc no PATH, verificar_conjunto.py (mesma arvore).
"""
import argparse
import json
import random
import re
import shutil
import sys
from pathlib import Path

MONTAGEM = Path(__file__).resolve().parent
RAIZ = MONTAGEM.parent
SEMANTICA = RAIZ.parent.parent / "tests" / "semantica"
sys.path.insert(0, str(RAIZ))
import verificar_conjunto as VC  # noqa: E402  (precisa do sys.path acima)

CONJUNTOS = ["L", "Cx", "Dx", "C_forja", "D_forja"]
EXTENSAO = {"forja": ".fj", "c": ".c"}
NOME_DA_LINGUA = {"forja": "Forja", "c": "C"}
PEDIDO_L = """TAREFA {id} - Prever a saida

Leia o programa prog{ext} deste diretorio SEM compilar nem executar nada e escreva em resposta.txt exatamente o que ele imprime na saida padrao (uma linha de resposta para cada linha impressa, na mesma ordem) e, como ULTIMA linha, `exit=N` com o codigo de saida do processo.
"""
PEDIDO_BUG = """TAREFA {id} - {titulo}

{descricao}

O arquivo prog{ext} deste diretorio {quando}. Corrija-o alterando o minimo necessario; o arquivo diagnostico_inicial.txt mostra o que aconteceu na primeira execucao. Nao mude o que o programa imprime quando ele esta certo.

Depois de corrigido {como} deve terminar com o codigo de saida {saida} e imprimir exatamente:
{stdout}"""
SEMENTE_AMOSTRA = 20261111
CODIGOS_DA_AMOSTRA = 25


def Escrever(Caminho, Texto):
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Caminho.write_text(Texto, encoding="utf-8", newline="\n")


def LerBlocos(Arquivo):
    """Blocos '### ID | cabecalho': lista de (id, cabecalho, corpo)."""
    Texto = (MONTAGEM / "dados" / Arquivo).read_text(encoding="utf-8")
    Saida = []
    for Parte in Texto.split("### ")[1:]:
        Linha, _, Corpo = Parte.partition("\n")
        Id, _, Cab = Linha.partition(" | ")
        Saida.append((Id.strip(), Cab.strip(), Corpo))
    return Saida


def LerSecoes(Corpo):
    """'@chave valor' abre uma secao; o texto ate a proxima '@' e o corpo. Devolve {chave: (valor, corpo)}."""
    Secoes = {}
    Atual = None
    for Linha in Corpo.splitlines(keepends=True):
        if Linha.startswith("@"):
            Chave, _, Valor = Linha[1:].rstrip("\n").partition(" ")
            Atual = Secoes.setdefault(Chave, [Valor.strip(), ""])
        elif Atual is not None:
            Atual[1] += Linha
    return {Chave: (Par[0], Par[1]) for Chave, Par in Secoes.items()}


def Trocar(Texto, De, Para, Onde):
    if Texto.count(De) != 1:
        raise ValueError("%s: o trecho aparece %d vezes (esperado 1): %r" % (Onde, Texto.count(De), De[:60]))
    return Texto.replace(De, Para)


def FontesDoItem(Secoes, Lingua, Id):
    """(fonte com defeito, fonte certa) de um item, conforme as chaves presentes."""
    Pre = Lingua + "."
    if Pre + "ref" in Secoes:
        Ref = Secoes[Pre + "ref"][1]
        Bug = Trocar(Ref, Secoes[Pre + "troca"][1], Secoes[Pre + "por"][1], Id + " " + Lingua)
        return Bug, Ref
    Bug = (SEMANTICA / Secoes[Pre + "fixture"][0]).read_text(encoding="utf-8").replace("\r\n", "\n")
    return Bug, Trocar(Bug, Secoes[Pre + "de"][1], Secoes[Pre + "para"][1], Id + " " + Lingua)


def MontarCtx(Forja):
    Cfg = VC.LerJson(RAIZ / "linguagens.json")["linguagens"]
    return {"raiz": RAIZ, "forja": Forja, "cfg": Cfg}


def LimparConjunto(Conj):
    shutil.rmtree(RAIZ / "tarefas" / Conj, ignore_errors=True)
    for Lingua in EXTENSAO:
        shutil.rmtree(RAIZ / "referencias" / Lingua / Conj, ignore_errors=True)


def GravarCatalogo(Conj, Linguas, Itens, Extra):
    Cat = {"conjunto": Conj, "versao_dados": "0.1.0", "linguas": Linguas, "tarefas": Itens}
    Cat.update(Extra)
    Escrever(RAIZ / "tarefas" / Conj / "catalogo.json", json.dumps(Cat, ensure_ascii=False, indent=2) + "\n")


def MontarL(Ctx):
    """20 programas por linguagem + gabarito (stdout/exit previsto, escrito a mao em L_gabarito.txt)."""
    Gab = {Id: Cab for Id, Cab, _ in LerBlocos("L_gabarito.txt")}
    Saida = {Id: Corpo for Id, _, Corpo in LerBlocos("L_gabarito.txt")}
    Itens = []
    for Lingua in ("forja", "c"):
        for Id, Titulo, Fonte in LerBlocos("L_%s.txt" % Lingua):
            Ext = EXTENSAO[Lingua]
            Escrever(RAIZ / "tarefas" / "L" / Id / Lingua / ("prog" + Ext), Fonte)
            Escrever(RAIZ / "tarefas" / "L" / Id / Lingua / "pedido.txt", PEDIDO_L.format(id=Id, ext=Ext))
            Ref = RAIZ / "referencias" / Lingua / "L" / Id
            Escrever(Ref / ("prog" + Ext), Fonte)
            Codigo = int(Gab[Id].split("=")[1])
            Escrever(Ref / "gabarito.json", json.dumps({"stdout": Saida[Id], "exit": Codigo}, ensure_ascii=False) + "\n")
            if Lingua == "forja":
                Itens.append({"id": Id, "titulo": Titulo})
    GravarCatalogo("L", ["forja", "c"], Itens, {"resposta": "resposta.txt: a saida exata e, na ultima linha, exit=N"})


def DeclDoFixture(Arquivo):
    """Codigo, linha e coluna do diagnostico esperado de uma fixture de tests\\semantica."""
    Linha = (SEMANTICA / Arquivo).with_suffix(".esperado.jsonl").read_text(encoding="utf-8").splitlines()[0]
    Esp = json.loads(Linha)
    return {"fase": "check", "codigo": Esp["codigo"],
            "padrao": '"linha": %d, "coluna": %d' % (Esp["linha"], Esp["coluna"])}


def DeclDe(Secoes, Lingua):
    Valor = Secoes.get(Lingua + ".decl")
    if Valor:
        return json.loads(Valor[0])
    Fixture = Secoes.get(Lingua + ".fixture")
    return DeclDoFixture(Fixture[0]) if Fixture else {}


def Casos(Stdout, Saida):
    return {"casos": [{"args": [], "env": {}, "stdin": "", "stdout": Stdout, "exit": Saida}]}


def QuandoFalha(Decl):
    if Decl["fase"] in ("check", "build"):
        return "nao passa pela verificacao do compilador"
    if Decl.get("saida_difere") and Decl.get("exit") == 0:
        return "compila e roda, mas imprime algo diferente do esperado"
    return "compila, mas falha ao executar"


def PedidoDoItem(Id, Secoes, Lingua, Esp):
    Decl = DeclDe(Secoes, Lingua)
    Caso = Esp["casos"][0]
    Como = "o comando `forja test prog.fj`" if Decl.get("modo") == "test" and Lingua == "forja" else "o programa"
    return PEDIDO_BUG.format(
        id=Id, titulo=Secoes["titulo"][0], descricao=Secoes["pedido"][1].strip(), ext=EXTENSAO[Lingua],
        quando=QuandoFalha(Decl), saida=Caso["exit"], como=Como,
        stdout=Caso["stdout"] or "(nada: a saida fica vazia)\n")


def PastasDoItem(Conj, Id, Lingua):
    """(pasta da referencia, pasta da tarefa); Cx e Dx tem uma subpasta por linguagem."""
    Ref = RAIZ / "referencias" / Lingua / Conj / Id
    Tarefa = RAIZ / "tarefas" / Conj / Id
    return Ref, (Tarefa / Lingua if Conj in VC.CONJUNTOS_DUAS_LINGUAS else Tarefa)


def MontarLingua(Ctx, Item, Secoes, Lingua):
    """Escreve bug e referencia de um item (Conj, Id) numa linguagem; devolve o esperado (da referencia)."""
    Conj, Id = Item
    Nome = "prog" + EXTENSAO[Lingua]
    Bug, Ref = FontesDoItem(Secoes, Lingua, Id)
    Modo = DeclDe(Secoes, Lingua).get("modo", "run")
    PastaRef, PastaTar = PastasDoItem(Conj, Id, Lingua)
    Escrever(PastaRef / Nome, Ref)
    Escrever(PastaTar / Nome, Bug)
    RRef = VC.RodarFases(Ctx, Lingua, PastaRef, Modo)
    Esp = Casos(RRef["stdout"], RRef["rc"])
    Escrever(PastaTar / "esperado.json", json.dumps(Esp, ensure_ascii=False, indent=2) + "\n")
    Escrever(PastaTar / "diagnostico_inicial.txt", VC.TextoDiagnostico(VC.RodarFases(Ctx, Lingua, PastaTar, Modo)))
    Escrever(PastaTar / "pedido.txt", PedidoDoItem(Id, Secoes, Lingua, Esp))
    return Esp


def MontarItem(Ctx, Item, Secoes, Linguas):
    Id = Item[1]
    Esperados = {Lingua: MontarLingua(Ctx, Item, Secoes, Lingua) for Lingua in Linguas}
    Saidas = {Esp["casos"][0]["stdout"] for Esp in Esperados.values()}
    if len(Saidas) > 1 and "stdout_proprio" not in Secoes:
        raise ValueError("%s: a saida certa difere entre Forja e C" % Id)
    return EntradaDoCatalogo(Id, Secoes, Linguas)


def EntradaDoCatalogo(Id, Secoes, Linguas):
    Entrada = {"id": Id, "classe": Secoes.get("classe", [""])[0], "titulo": Secoes["titulo"][0]}
    for Lingua in Linguas:
        Entrada[Lingua] = DeclDe(Secoes, Lingua)
    Entrada.update({Chave: Secoes["forja." + Chave][0] for Chave in ("fixture", "origem") if "forja." + Chave in Secoes})
    return Entrada


def MontarBugs(Ctx, Conj):
    Linguas = ["forja", "c"] if Conj in VC.CONJUNTOS_DUAS_LINGUAS else ["forja"]
    Itens = []
    for Id, _, Corpo in LerBlocos(Conj + ".txt"):
        Itens.append(MontarItem(Ctx, (Conj, Id), LerSecoes(Corpo), Linguas))
    GravarCatalogo(Conj, Linguas, Itens, ExtraDoCatalogo(Conj))


def ExtraDoCatalogo(Conj):
    if Conj != "C_forja":
        return {}
    Regra = ("os %d codigos mais frequentes dos err_* de tests\\semantica que tem fixture simples (proc main, sem import nem "
             "export, 1 diagnostico); 1 fixture por codigo sorteada com random.Random(semente).choice, na ordem dos itens "
             "do catalogo; E0901 e E1001 (so DLL/vendor) ficam de fora; + 15 idiomas de outras linguagens. "
             "Conferir: python montar_conjuntos.py --conferir-amostra" % CODIGOS_DA_AMOSTRA)
    return {"amostra": {"semente": SEMENTE_AMOSTRA, "regra": Regra}}


def FixturesSimples(Codigo):
    """Fixtures err_<codigo>_*.fj de tests\\semantica com proc main, sem import/export e 1 diagnostico esperado."""
    Achadas = []
    for Arq in sorted(SEMANTICA.glob("err_%s_*.fj" % Codigo)):
        Texto = Arq.read_text(encoding="utf-8")
        Linhas = Arq.with_suffix(".esperado.jsonl").read_text(encoding="utf-8").splitlines()
        if "proc main" in Texto and "import" not in Texto and "export" not in Texto and len(Linhas) == 1:
            Achadas.append(Arq.name)
    return Achadas


def ConferirAmostra():
    """Sorteia de novo a fixture de cada um dos 25 codigos e compara com o que esta em C_forja.txt."""
    Sorteio = random.Random(SEMENTE_AMOSTRA)
    Divergencias = 0
    for Id, _, Corpo in LerBlocos("C_forja.txt")[:CODIGOS_DA_AMOSTRA]:
        Fixture = LerSecoes(Corpo)["forja.fixture"][0]
        Esperada = Sorteio.choice(FixturesSimples(Fixture.split("_")[1]))
        if Esperada != Fixture:
            Divergencias += 1
            print("diverge %s: catalogo %s, sorteio %s" % (Id, Fixture, Esperada))
    print("amostra: %d divergencias" % Divergencias)
    return 1 if Divergencias else 0


def Montar(Forja, Conjuntos):
    Ctx = MontarCtx(Forja)
    for Conj in Conjuntos:
        if Conj in ("P", "R"):  # conjuntos P e R (a17): montar_pr.py cuida da limpeza e da montagem
            import montar_pr
            montar_pr.Montar(Forja, [Conj])
            continue
        LimparConjunto(Conj)
        MontarL(Ctx) if Conj == "L" else MontarBugs(Ctx, Conj)
        print("montado", Conj)


def Principal():
    Parser = argparse.ArgumentParser(description="Monta os conjuntos L, Cx, Dx, C_forja e D_forja")
    Parser.add_argument("--forja", help="forja.exe da copia congelada")
    Parser.add_argument("--conjunto", default=",".join(CONJUNTOS))
    Parser.add_argument("--conferir-amostra", action="store_true", help="refaz o sorteio das fixtures de C_forja e compara")
    Args = Parser.parse_args()
    if Args.conferir_amostra:
        return ConferirAmostra()
    if not Args.forja:
        Parser.error("informe --forja <forja.exe>")
    Montar(Path(Args.forja).resolve(), [C for C in Args.conjunto.split(",") if re.fullmatch(r"\w+", C)])
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
