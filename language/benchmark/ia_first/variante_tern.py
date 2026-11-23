# -*- coding: utf-8 -*-
"""variante_tern.py - V-05 do preprocessador variante.py (P-IA1-a08): 'c ? a : b' vira if/else de comando (a v1.11 nao tem expressao condicional).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao. Formas traduzidas: atribuicao 'x = c ? a : b', 'return c ? a : b', comando/chamada com o
        ternario dentro ('print(c ? a : b)': a linha e repetida nos dois ramos), 'let|var x: T = ...c ? a : b...' e 'let|var x =
        c ? a : b' (tipo T lido da anotacao ou deduzido DO TEXTO: literal, nome com tipo declarado, retorno de proc da assinatura,
        'as T', comparacao = bool). Ternario em cabeca de if/while/for/switch/case, dentro de condicao ou sem tipo deduzivel NAO e
        traduzido: a linha fica como veio e o ponto vai para Ctx.Pendencias (precisaria de analise semantica).

Uso interno: importado por variante.py. Reversao: apagar este arquivo. Depende de: variante_seg, variante_regras.
Verificacao: python variante.py --selftest
"""
from variante_regras import RESERVADAS, Recuo
from variante_seg import Espaco, Novo, Sig

ATRIBUICOES = ("=", "+=", "-=", "*=", "/=")
CABECAS_SEM_TERNARIO = ("if", "else", "while", "for", "switch", "case", "default", "defer")
CABECAS_DE_DECLARACAO = ("proc", "pub", "export", "foreign", "struct", "enum", "union", "unit", "import", "test", "const")
COMPARACOES = ("==", "!=", "<", ">", "<=", ">=", "and", "or", "not")
ZEROS = {"bool": "false", "str": '""', "f32": "0.0", "f64": "0.0"}
INTEIROS = ("int", "uint", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64")
RETORNO_EMBUTIDO = {"len": "int"}


def Zero(Tipo):
    """Valor inicial neutro do tipo (a variavel e sobrescrita nos dois ramos); None se o tipo nao e simples."""
    if Tipo in INTEIROS:
        return "0"
    if Tipo and Tipo.startswith("?"):
        return "none"
    return ZEROS.get(Tipo)


def FimDoTipo(Segs, Ind, Ini):
    """Posicao (em Ind), exclusiva, onde termina o tipo que comeca em Ind[Ini]."""
    Prof = 0
    for K in range(Ini, len(Ind)):
        Txt = Segs[Ind[K]].Txt
        if Prof == 0 and Txt in (",", ")", "=", "{", "in"):
            return K
        Prof += Txt in ("(", "[")
        Prof -= Txt in (")", "]")
    return len(Ind)


def RegistrarTipos(Linhas, Ctx):
    """Tabela lexical nome -> tipo escrito (parametros, campos, let/var anotados, literais, retorno de proc, laco sobre faixa)."""
    for Segs in Linhas:
        Ind = Sig(Segs)
        if Ind and Segs[Ind[0]].Txt not in ("case", "default"):
            RegistrarAnotados(Segs, Ind, Ctx)
        RegistrarInferidos(Segs, Ind, Ctx)


def RegistrarAnotados(Segs, Ind, Ctx):
    for K in range(len(Ind) - 2):
        Seg = Segs[Ind[K]]
        Antes = Segs[Ind[K - 1]].Txt if K > 0 else None
        if Seg.Tipo == "nome" and Seg.Txt not in RESERVADAS and Segs[Ind[K + 1]].Txt == ":" and Antes in (None, "(", ",", "let", "var", "const", "unit"):
            Fim = FimDoTipo(Segs, Ind, K + 2)
            if Fim > K + 2:
                Ctx.Tipos[Seg.Txt] = "".join(Segs[I].Txt for I in Ind[K + 2:Fim])


def RegistrarInferidos(Segs, Ind, Ctx):
    Txts = [Segs[I].Txt for I in Ind]
    if len(Txts) >= 4 and Txts[0] in ("let", "var") and Txts[2] == "=":
        Tipo = TipoDoAtomo(Segs, Ind, 3, Ctx) if len(Txts) == 4 or Txts[4] == "(" else None
        if Tipo:
            Ctx.Tipos.setdefault(Txts[1], Tipo)
    if len(Txts) >= 4 and Txts[0] == "for" and Txts[2] == "in" and ".." in "".join(Txts[3:]).replace("<", "").replace("=", ""):
        Ctx.Tipos[Txts[1]] = "int"


def RetornoDe(Segs, Ind, K, Ctx):
    """Tipo de retorno declarado do chamado cujo nome esta em Ind[K] (ate 'mod.nome'); None se desconhecido."""
    Nome = Segs[Ind[K]].Txt
    if K >= 2 and Segs[Ind[K - 1]].Txt == ".":
        Reg = Ctx.Modulos.get(Segs[Ind[K - 2]].Txt, {}).get(Nome)
    else:
        Reg = Ctx.Locais.get(Nome)
    return Reg["ret"] if Reg else RETORNO_EMBUTIDO.get(Nome)


def TipoDoAtomo(Segs, Ind, K, Ctx):
    """Tipo do token em Ind[K] lido do proprio texto: literal, nome com tipo declarado ou chamada com retorno declarado."""
    Seg = Segs[Ind[K]]
    if Seg.Tipo == "num":
        return "f32" if "." in Seg.Txt else "int"
    if Seg.Tipo == "texto":
        return "str"
    if Seg.Txt in ("true", "false"):
        return "bool"
    if Seg.Tipo != "nome" or Seg.Txt in RESERVADAS:
        return None
    Prox = Segs[Ind[K + 1]].Txt if K + 1 < len(Ind) else None
    if Prox == "(":
        return RetornoDe(Segs, Ind, K, Ctx)
    if Prox in (".", "[") or (K > 0 and Segs[Ind[K - 1]].Txt == "."):
        return None
    return Ctx.Tipos.get(Seg.Txt)


def InferirTipo(Segs, Ctx):
    """Tipo de uma expressao pelo texto: 'as T' final, comparacao = bool, senao o 1o nome/chamada tipado e por fim o 1o literal."""
    Ind = Sig(Segs)
    Txts = [Segs[I].Txt for I in Ind]
    if "as" in Txts:
        return "".join(Txts[len(Txts) - Txts[::-1].index("as"):])
    if any(T in COMPARACOES for T in Txts):
        return "bool"
    for Literais in (False, True):
        for K, I in enumerate(Ind):
            Literal = Segs[I].Tipo in ("num", "texto")
            Tipo = TipoDoAtomo(Segs, Ind, K, Ctx) if Literal == Literais else None
            if Tipo:
                return Tipo
    return None


def AcharDoisPontos(Segs, Ind, Q):
    """Posicao (em Ind) do ':' do ternario cujo '?' esta em Ind[Q]; None se nao fecha (era '?' de tipo)."""
    Prof, Pendentes = 0, 0
    for J in range(Q + 1, len(Ind)):
        Txt = Segs[Ind[J]].Txt
        if Txt in ("(", "[", "{"):
            Prof += 1
        elif Txt in (")", "]", "}"):
            Prof -= 1
            if Prof < 0:
                return None
        elif Prof == 0 and Txt == "?":
            Pendentes += 1
        elif Prof == 0 and Txt == ":":
            if Pendentes == 0:
                return J
            Pendentes -= 1
    return None


def InicioDaCondicao(Segs, Ind, Q):
    """Posicao (em Ind) do 1o token da condicao: depois de '(' / '[' aberto, ',', atribuicao ou 'return'; ou o inicio da linha."""
    Prof = 0
    for J in range(Q - 1, -1, -1):
        Txt = Segs[Ind[J]].Txt
        if Txt in (")", "]", "}"):
            Prof += 1
        elif Txt in ("(", "[", "{"):
            if Prof == 0:
                return J + 1
            Prof -= 1
        elif Prof == 0 and (Txt in ATRIBUICOES or Txt in (",", "return")):
            return J + 1
    return 0


def FimDoRamo(Segs, Ind, Dp):
    """Posicao (em Ind), exclusiva, do fim do ramo 'senao' que comeca depois de Ind[Dp]: ')' ou ',' de fora, ou o fim da linha."""
    Prof = 0
    for J in range(Dp + 1, len(Ind)):
        Txt = Segs[Ind[J]].Txt
        if Txt in ("(", "[", "{"):
            Prof += 1
        elif Txt in (")", "]", "}"):
            if Prof == 0:
                return J
            Prof -= 1
        elif Prof == 0 and Txt == ",":
            return J
    return len(Ind)


def AcharTernario(Segs, Ind):
    """(Ini, Q, Dp, Fim) em posicoes de Ind do primeiro ternario da linha; None se nao ha."""
    for Q in range(1, len(Ind) - 1):
        Ant = Segs[Ind[Q - 1]]
        if Segs[Ind[Q]].Txt != "?" or not (Ant.Tipo in ("nome", "num", "texto") or Ant.Txt in (")", "]")):
            continue
        Dp = AcharDoisPontos(Segs, Ind, Q)
        if Dp is not None:
            return InicioDaCondicao(Segs, Ind, Q), Q, Dp, FimDoRamo(Segs, Ind, Dp)
    return None


def Aparar(Segs):
    while Segs and Segs[0].Tipo == "esp":
        Segs = Segs[1:]
    while Segs and Segs[-1].Tipo == "esp":
        Segs = Segs[:-1]
    return Segs


def Linha(Nivel, Segs, Ancora):
    return ([Espaco(Ancora, Nivel)] if Nivel else []) + Aparar(Segs)


def Declaracao(Segs, Ind, Par, Ctx):
    """Para 'let|var x[: T] = ...': ((linha 'var x: T = zero', prefixo dos ramos 'x = ...'), None) ou (None, motivo)."""
    Txts = [Segs[I].Txt for I in Ind]
    if len(Txts) < 4 or Segs[Ind[1]].Tipo != "nome" or "=" not in Txts:
        return None, "declaracao que nao da para ler"
    Eq, Ancora = Txts.index("="), Segs[Ind[0]]
    Ini, Q, Dp, Fim = Par
    Tipo = "".join(Txts[3:Eq]) if Txts[2] == ":" else None
    if Tipo is None and (Ini != Eq + 1 or Fim != len(Ind)):
        return None, "let sem tipo com ternario dentro de expressao maior"
    if Tipo is None:
        Tipo = InferirTipo(Segs[Ind[Q] + 1:Ind[Dp]], Ctx) or InferirTipo(Segs[Ind[Dp] + 1:], Ctx)
    if Zero(Tipo) is None:
        return None, "tipo %s nao deduzivel do texto" % (Tipo or "?")
    Nome = Segs[Ind[1]]
    Decl = [Novo("nome", "var", Ancora), Espaco(Ancora), Nome, Novo("simb", ":", Ancora), Espaco(Ancora), Novo("nome", Tipo, Ancora),
            Espaco(Ancora), Novo("simb", "=", Ancora), Espaco(Ancora), Novo("nome", Zero(Tipo), Ancora)]
    Prefixo = [Nome, Espaco(Ancora), Novo("simb", "=", Ancora), Espaco(Ancora)] + Segs[Ind[Eq] + 1:Ind[Ini]]
    return (Decl, Prefixo), None


def MontarRamos(Segs, Ind, Par, Ctx):
    """((declaracao ou None, condicao, ramo A, ramo B), None) ou (None, motivo)."""
    Ini, Q, Dp, Fim = Par
    FimB = Ind[Fim] if Fim < len(Ind) else Ind[-1] + 1
    Sufixo = [S for S in Segs[FimB:] if S.Tipo != "coment"]
    Prefixo, Decl = Segs[:Ind[Ini]], None
    if Segs[Ind[0]].Txt in ("let", "var"):
        Resultado, Motivo = Declaracao(Segs, Ind, Par, Ctx)
        if Resultado is None:
            return None, Motivo
        Decl, Prefixo = Resultado
    A, B = Aparar(Segs[Ind[Q] + 1:Ind[Dp]]), Aparar(Segs[Ind[Dp] + 1:FimB])
    return (Decl, Segs[Ind[Ini]:Ind[Q]], Prefixo + A + Sufixo, Prefixo + B + Sufixo), None


def TraduzirTernario(Segs, Ctx):
    """V-05. Devolve a lista de linhas; a linha fica como veio (e o motivo vai para Ctx.Pendencias) quando nao da para traduzir."""
    Ind = Sig(Segs)
    Par = AcharTernario(Segs, Ind) if Ind and Segs[Ind[0]].Txt not in CABECAS_DE_DECLARACAO else None
    if Par is None:
        return [Segs]
    if Segs[Ind[0]].Txt in CABECAS_SEM_TERNARIO:
        Ctx.Pendencias.append((Segs[Ind[0]].Lin, "ternario em cabeca de %s (exige analise semantica)" % Segs[Ind[0]].Txt))
        return [Segs]
    Ramos, Motivo = MontarRamos(Segs, Ind, Par, Ctx)
    if Ramos is None:
        Ctx.Pendencias.append((Segs[Ind[0]].Lin, "ternario: " + Motivo))
        return [Segs]
    Decl, Cond, RamoA, RamoB = Ramos
    Ancora, Nivel = Segs[Ind[Par[1]]], Recuo(Segs)
    Saida = [Linha(Nivel, Decl, Ancora)] if Decl else []
    Se = [Novo("nome", "if", Ancora), Espaco(Ancora)] + Aparar(Cond) + [Espaco(Ancora), Novo("simb", "{", Ancora)]
    Saida.append(Linha(Nivel, Se, Ancora))
    Senao = [Novo("simb", "}", Ancora), Espaco(Ancora), Novo("nome", "else", Ancora), Espaco(Ancora), Novo("simb", "{", Ancora)]
    for Ramo, Fecho in ((RamoA, Senao), (RamoB, [Novo("simb", "}", Ancora)])):
        Saida += TraduzirTernario(Linha(Nivel + "    ", Ramo, Ancora), Ctx)
        Saida.append(Linha(Nivel, Fecho, Ancora))
    return Saida
