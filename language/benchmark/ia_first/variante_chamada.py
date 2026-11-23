# -*- coding: utf-8 -*-
"""variante_chamada.py - assinaturas e chamadas do preprocessador variante.py (P-IA1-a08): V-06 (posicional por assinatura) e V-07 (P2).

Versao 0.1.1
Changelog:
  0.1.1 (13/11/2026) IA1-S2b2a: assinaturas de std:<m> tambem dos pedacos std\<m>\*.fj (AssinaturasDoModulo).
  0.1.0 (11/11/2026) primeira versao: Contexto (assinaturas do proprio arquivo, dos outros .fj da pasta, da std e dos embutidos),
        chamada com argumentos posicionais -> o 1o fica posicional e os demais ganham o nome do parametro (v1.11); 1o argumento
        nomeado vai para a frente sem rotulo (posicional OU nomeado); P2: alloc/to/scope -> em/para/escopo e o rotulo antigo
        (em/para/escopo) vira erro E0110 "forma antiga" do proprio preprocessador. SEM analise semantica: so a assinatura escrita.

Uso interno: importado por variante.py. Reversao: apagar este arquivo. Depende de: variante_seg, variante_regras.
Verificacao: python variante.py --selftest
"""
from pathlib import Path

from variante_regras import RESERVADAS, Fecha, TrocarPalavraProc
from variante_seg import Espaco, Lexar, Novo, Sig

BUILTINS = {
    "min": ["a", "b"], "max": ["a", "b"], "clamp": ["x", "lo", "hi"], "append": ["l", "item"], "insert": ["m", "key", "value"],
    "get": ["m", "key"], "has": ["s", "key"], "get_ptr": ["m", "key"], "remove": ["m", "key"], "new": ["T", "em"],
    "make": ["T", "em", "cap"], "format": ["f", "em"], "format_c": ["f", "em"], "transmute": ["x", "para"],
    "assert": ["cond", "msg"], "static_assert": ["cond", "msg"], "slice_from": ["p", "len"],
}
ROTULOS_P2 = {"alloc": "em", "to": "para"}
ROTULOS_ANTIGOS = {"em": "alloc", "para": "to"}
MODULOS_STD = {"proc": "os"}


class Contexto:
    """Tudo que o preprocessador sabe do programa, lido do texto (nunca da semantica)."""

    def __init__(self, Pacote, Alvo, PastaStd, PastaOrigem):
        self.Pacote, self.Alvo = Pacote, Alvo
        self.PastaStd, self.PastaOrigem = Path(PastaStd) if PastaStd else None, Path(PastaOrigem) if PastaOrigem else None
        self.Locais, self.Modulos, self.Tipos = {}, {}, {}
        self.Erros, self.Pendencias = [], []


def PartirVirgulas(Segs, Ind, Abre, Fim):
    """Posicoes (em Ind) das virgulas de profundidade 0 entre Ind[Abre] e Ind[Fim]."""
    Prof, Virgulas = 0, []
    for K in range(Abre + 1, Fim):
        Txt = Segs[Ind[K]].Txt
        Prof += Txt in ("(", "[", "{")
        Prof -= Txt in (")", "]", "}")
        if Txt == "," and Prof == 0:
            Virgulas.append(K)
    return Virgulas


def AssinaturaDaLinha(Segs):
    """(nome, {params, ret, pub}) de uma linha 'proc nome(...) -> R' (com alias foreign 'proc nome = "x"(...)'); None se nao e."""
    Ind = Sig(Segs)
    K = 0
    while K < len(Ind) and Segs[Ind[K]].Txt in ("pub", "export"):
        K += 1
    if K + 2 >= len(Ind) or Segs[Ind[K]].Txt != "proc" or Segs[Ind[K + 1]].Tipo != "nome":
        return None
    Nome, P = Segs[Ind[K + 1]].Txt, K + 2
    if Segs[Ind[P]].Txt == "=" and P + 2 < len(Ind):
        P += 2
    if Segs[Ind[P]].Txt != "(":
        return None
    Fim = Fecha(Segs, Ind, P)
    if Fim is None:
        return None
    Cortes = [P] + PartirVirgulas(Segs, Ind, P, Fim) + [Fim]
    Params = [Segs[Ind[A + 1]].Txt for A, B in zip(Cortes, Cortes[1:]) if B > A + 1 and Segs[Ind[A + 1]].Tipo == "nome"]
    return Nome, {"params": Params, "ret": TipoDeRetorno(Segs, Ind, Fim), "pub": K > 0 and Segs[Ind[0]].Txt == "pub"}


def TipoDeRetorno(Segs, Ind, Fim):
    """Texto do tipo de valor depois de '->' (sem a parte '! E'); None se nao ha ou e tupla."""
    if Fim + 2 >= len(Ind) or Segs[Ind[Fim + 1]].Txt != "->":
        return None
    Txts = []
    for I in Ind[Fim + 2:]:
        if Segs[I].Txt in ("{", "!"):
            break
        Txts.append(Segs[I].Txt)
    return None if not Txts or Txts[0] == "(" else "".join(Txts)


def LerAssinaturas(Linhas, SoPublicas=False):
    Tabela = {}
    for Segs in Linhas:
        Par = AssinaturaDaLinha(Segs)
        if Par and (Par[1]["pub"] or not SoPublicas):
            Tabela[Par[0]] = Par[1]
    return Tabela


def AssinaturasDoArquivo(Caminho):
    try:
        Texto = Path(Caminho).read_bytes().decode("latin-1")
    except OSError:
        return {}
    return LerAssinaturas([TrocarPalavraProc(Lexar(L, N + 1)) for N, L in enumerate(Texto.split("\n"))], SoPublicas=True)


def AssinaturasDoModulo(Arq, EhStd):
    """Assinaturas do arquivo do modulo; em std, mais as dos pedacos std\\<m>\\*.fj (S2b2a: o modulo e a concatenacao deles)."""
    Tabela = AssinaturasDoArquivo(Arq)
    if EhStd:
        for Pedaco in sorted((Arq.parent / Arq.stem).glob("*.fj")):
            Tabela.update(AssinaturasDoArquivo(Pedaco))
    return Tabela


def ArquivoDoImport(Caminho, Ctx):
    """Arquivo .fj de um 'import "caminho"' (std, vendor ou relativo a pasta do programa); None se nao se acha."""
    Texto = Caminho.strip('"')
    Partes = Texto.split(":", 1)
    if len(Partes) == 2 and Ctx.PastaStd:
        Nome = MODULOS_STD.get(Partes[1], Partes[1])
        return Ctx.PastaStd / (Nome + ".fj") if Partes[0] == "std" else (Ctx.PastaStd.parent / "vendor" / (Partes[1] + ".fj") if Partes[0] == "vendor" else None)
    return Ctx.PastaOrigem / (Texto + ".fj") if Ctx.PastaOrigem and len(Partes) == 1 else None


def CarregarModulos(Linhas, Ctx):
    """Apelido de cada import -> assinaturas publicas do modulo."""
    for Segs in Linhas:
        Ind = Sig(Segs)
        if len(Ind) < 2 or Segs[Ind[0]].Txt != "import" or Segs[Ind[1]].Tipo != "texto":
            continue
        Caminho = Segs[Ind[1]].Txt
        Apelido = Segs[Ind[-1]].Txt if len(Ind) >= 4 and Segs[Ind[-2]].Txt == "as" else Caminho.strip('"').replace(":", "/").split("/")[-1]
        Arq = ArquivoDoImport(Caminho, Ctx)
        Ctx.Modulos[Apelido] = AssinaturasDoModulo(Arq, Caminho.startswith('"std:')) if Arq else {}


def AcharAssinatura(Nome, Qual, Ctx):
    """Parametros declarados do chamado (qualificado por modulo, do arquivo ou embutido); None se desconhecido."""
    if Qual is not None:
        Sig_ = Ctx.Modulos.get(Qual, {}).get(Nome)
        return Sig_["params"] if Sig_ else None
    if Nome in Ctx.Locais:
        return Ctx.Locais[Nome]["params"]
    return BUILTINS.get(Nome)


def Rotulo(Arg):
    """(posicao, nome) do rotulo 'nome =' no inicio do argumento; (None, None) se e posicional."""
    Ind = Sig(Arg)
    if len(Ind) >= 3 and Arg[Ind[0]].Tipo == "nome" and Arg[Ind[1]].Txt == "=":
        return Ind[0], Arg[Ind[0]].Txt
    return None, None


def Rotular(Arg, Nome):
    """Insere 'Nome = ' antes do primeiro token significativo do argumento."""
    Pos = Sig(Arg)[0]
    Ancora = Arg[Pos]
    Arg[Pos:Pos] = [Novo("nome", Nome, Ancora), Espaco(Ancora), Novo("simb", "=", Ancora), Espaco(Ancora)]


def TirarRotulo(Arg):
    Ind = Sig(Arg)
    Fim = Ind[1] + 1
    while Fim < len(Arg) and Arg[Fim].Tipo == "esp":
        Fim += 1
    Arg[Ind[0]:Fim] = []


def RotulosDoPacote(Args, Ctx):
    """P2: alloc/to -> em/para e 'scope' -> 'escopo'; o rotulo antigo (em/para) e 'escopo' viram erro E0110. P1: nada."""
    if Ctx.Pacote != "P2":
        return
    for Arg in Args:
        Pos, Nome = Rotulo(Arg)
        if Nome in ROTULOS_ANTIGOS:
            RegistrarAntigo(Ctx, Arg[Pos], Nome, ROTULOS_ANTIGOS[Nome])
        elif Nome in ROTULOS_P2:
            Arg[Pos] = Novo("nome", ROTULOS_P2[Nome], Arg[Pos])
        Ind = Sig(Arg)
        if len(Ind) >= 3 and Arg[Ind[-1]].Txt in ("scope", "escopo") and Arg[Ind[0]].Txt in ("em", "alloc"):
            TrocarEscopo(Arg, Ind, Ctx)


def TrocarEscopo(Arg, Ind, Ctx):
    Seg = Arg[Ind[-1]]
    if Seg.Txt == "escopo":
        RegistrarAntigo(Ctx, Seg, "escopo", "scope")
    else:
        Arg[Ind[-1]] = Novo("nome", "escopo", Seg)


def RegistrarAntigo(Ctx, Seg, Antigo, Novo_):
    Ctx.Erros.append({"codigo": "E0110", "tipo": "sintaxe", "linha": Seg.Lin, "coluna": Seg.Col + 1, "coluna_fim": Seg.Col + 1 + len(Seg.Txt),
                      "causa": "forma antiga '%s': neste pacote o rotulo e '%s'" % (Antigo, Novo_),
                      "explicacao": "A forma antiga mudou de grafia e nao e mais aceita.", "forma_certa": "troque '%s' por '%s'" % (Antigo, Novo_)})


def ReordenarPrimeiro(Args, Params):
    """Posicional OU nomeado: se o 1o argumento vem nomeado, o argumento do 1o parametro passa para a frente, sem rotulo."""
    if not Params or Rotulo(Args[0])[1] is None:
        return
    for I, Arg in enumerate(Args):
        if Rotulo(Arg)[1] == Params[0]:
            TirarRotulo(Arg)
            Args.insert(0, Args.pop(I))
            while Args[0] and Args[0][0].Tipo == "esp":
                Args[0].pop(0)
            for Resto in Args[1:]:
                if Resto and Resto[0].Tipo != "esp":
                    Resto.insert(0, Espaco(Resto[0]))
            return


def RotularRestantes(Args, Params):
    """Do 2o argumento em diante, o posicional ganha o nome do parametro da mesma posicao."""
    for I in range(1, min(len(Args), len(Params))):
        if Rotulo(Args[I])[1] is None and Sig(Args[I]):
            Rotular(Args[I], Params[I])


def Chamado(Segs, Ind, K):
    """(Nome, Qual) do chamado cujo '(' esta em Ind[K]; None se nao e uma chamada (palavra reservada, declaracao, cadeia longa)."""
    if K < 1 or Segs[Ind[K - 1]].Tipo != "nome" or Segs[Ind[K - 1]].Txt in RESERVADAS:
        return None
    Qual = None
    if K >= 2 and Segs[Ind[K - 2]].Txt == "proc":
        return None
    if K >= 3 and Segs[Ind[K - 2]].Txt == ".":
        Qual = Segs[Ind[K - 3]].Txt if Segs[Ind[K - 3]].Tipo == "nome" and (K < 4 or Segs[Ind[K - 4]].Txt != ".") else "?"
    return Segs[Ind[K - 1]].Txt, Qual


def AjustarChamada(Segs, K, Ctx):
    Ind = Sig(Segs)
    Par = Chamado(Segs, Ind, K)
    Fim = Fecha(Segs, Ind, K)
    if Par is None or Fim is None or Fim == K + 1:
        return Segs
    Params = AcharAssinatura(Par[0], Par[1], Ctx)
    Cortes = [K] + PartirVirgulas(Segs, Ind, K, Fim) + [Fim]
    Args = [Segs[Ind[A] + 1:Ind[B]] for A, B in zip(Cortes, Cortes[1:])]
    Virgulas = [Segs[Ind[C]] for C in Cortes[1:-1]]
    if Par[1] is None and Par[0] in BUILTINS and Par[0] not in Ctx.Locais:
        RotulosDoPacote(Args, Ctx)
    if Params:
        ReordenarPrimeiro(Args, Params)
        RotularRestantes(Args, Params)
    Novos = list(Args[0])
    for Virgula, Arg in zip(Virgulas, Args[1:]):
        Novos += [Virgula] + Arg
    Segs[Ind[K] + 1:Ind[Fim]] = Novos
    return Segs


def TraduzirChamadas(Segs, Ctx):
    """V-06 e V-07: processa os '(' de chamada da direita para a esquerda (a edicao de uma chamada nao desloca as anteriores)."""
    Ind = Sig(Segs)
    Abres = [K for K in range(len(Ind)) if Segs[Ind[K]].Txt == "(" and Chamado(Segs, Ind, K)]
    for K in reversed(Abres):
        Segs = AjustarChamada(Segs, K, Ctx)
    return Segs
