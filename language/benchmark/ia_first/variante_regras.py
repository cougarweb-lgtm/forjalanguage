# -*- coding: utf-8 -*-
"""variante_regras.py - regras de troca de tokens do preprocessador variante.py (P-IA1-a08): V-01, V-02, V-03, V-04, V-08, V-09.

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: palavra de proc (fn/func/def/function), ';' no fim, && || !, i++ / i--, corpo de 1 comando
        na mesma linha (divide a linha: as linhas novas apontam para a linha de origem), import std:os para o binario base e
        main -> int (return vira os.exit). Cada regra recebe e devolve a lista de segmentos de UMA linha.

Uso interno: importado por variante.py. Reversao: apagar este arquivo. Depende de: variante_seg.
Verificacao: python variante.py --selftest
"""
from variante_seg import Espaco, Novo, Sig

PALAVRAS_PROC = ("fn", "func", "def", "function")
CABECAS = ("if", "else", "for", "while", "proc", "test")
RESERVADAS = ("const", "let", "var", "proc", "struct", "enum", "union", "unit", "pub", "export", "import", "as", "foreign", "test",
              "if", "else", "for", "in", "while", "switch", "case", "default", "break", "continue", "return", "defer", "try",
              "or_else", "and", "or", "not", "true", "false", "none")
SEGUROS_APOS_NEGACAO = (None, "{", ")", "]", ",", "and", "or", ":", "}", "?")
TIPOS_INTEIROS = ("int", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "uint")


def Fecha(Segs, Ind, K):
    """Posicao (em Ind) do fecha-colchete que casa com o abre em Ind[K]; None se a linha acaba antes."""
    Prof = 0
    for J in range(K, len(Ind)):
        Txt = Segs[Ind[J]].Txt
        Prof += Txt in ("(", "[", "{")
        Prof -= Txt in (")", "]", "}")
        if Prof == 0:
            return J
    return None


def TrocarPalavraProc(Segs):
    """V-09: fn/func/def/function na posicao de 'proc' viram proc."""
    Ind = Sig(Segs)
    K = 0
    while K < len(Ind) and Segs[Ind[K]].Txt in ("pub", "export"):
        K += 1
    Cabeca = K + 2 < len(Ind) and Segs[Ind[K]].Txt in PALAVRAS_PROC and Segs[Ind[K + 1]].Tipo == "nome"
    if Cabeca and Segs[Ind[K + 2]].Txt == "(":
        Segs[Ind[K]] = Novo("nome", "proc", Segs[Ind[K]])
    return Segs


def TirarPontoVirgula(Segs):
    """V-02: ';' no fim do comando (antes do fim da linha ou de '}') e descartado."""
    Ind = Sig(Segs)
    Sair, Prof = [], 0
    for K, I in enumerate(Ind):
        Txt = Segs[I].Txt
        Prof += Txt in ("(", "[")
        Prof -= Txt in (")", "]")
        Depois = Segs[Ind[K + 1]].Txt if K + 1 < len(Ind) else None
        if Txt == ";" and Prof == 0 and Depois in (None, "}"):
            Sair.append(I)
    return [S for I, S in enumerate(Segs) if I not in Sair]


def AjustarImport(Segs, Alvo):
    """std:os -> std:proc as os quando o binario do par e o forja_ia1_base (nao conhece std:os)."""
    Ind = Sig(Segs)
    if Alvo != "base" or len(Ind) < 2 or Segs[Ind[0]].Txt != "import" or Segs[Ind[1]].Txt != '"std:os"':
        return Segs
    Segs[Ind[1]] = Novo("texto", '"std:proc"', Segs[Ind[1]])
    if len(Ind) == 2:
        Ancora = Segs[Ind[1]]
        Segs += [Espaco(Ancora), Novo("nome", "as", Ancora), Espaco(Ancora), Novo("nome", "os", Ancora)]
    return Segs


def FimOperando(Segs, Ind, K):
    """Posicao (em Ind) do ultimo token do operando unario que comeca em Ind[K]: prefixos, atomo ou (...), depois .x, [..], (..)."""
    while K < len(Ind) and Segs[Ind[K]].Txt in ("not", "-", "~"):
        K += 1
    if K >= len(Ind):
        return None
    if Segs[Ind[K]].Txt == "(":
        K = Fecha(Segs, Ind, K)
    while K is not None and K + 1 < len(Ind):
        Prox = Segs[Ind[K + 1]].Txt
        if Prox == "." and K + 2 < len(Ind):
            K += 2
        elif Prox in ("(", "["):
            K = Fecha(Segs, Ind, K + 1)
        else:
            break
    return K


def NegarUm(Segs, K):
    """Troca o '!' em Ind[K] por 'not' (com parenteses quando o proximo operador ligaria mais forte que a negacao)."""
    Ind = Sig(Segs)
    Bang = Segs[Ind[K]]
    Fim = FimOperando(Segs, Ind, K + 1)
    Novos = [Novo("nome", "not", Bang)] + ([] if Segs[Ind[K] + 1].Tipo == "esp" else [Espaco(Bang)])
    if Fim is None:
        Segs[Ind[K]:Ind[K] + 1] = Novos
        return Segs
    Prox = Segs[Ind[Fim + 1]].Txt if Fim + 1 < len(Ind) else None
    if Prox not in SEGUROS_APOS_NEGACAO:
        Segs.insert(Ind[Fim] + 1, Novo("simb", ")", Bang))
        Novos.insert(0, Novo("simb", "(", Bang))
    Segs[Ind[K]:Ind[K] + 1] = Novos
    return Segs


def TrocarLogicos(Segs):
    """V-03: && || ! viram and or not (a negacao ganha parenteses se o operador seguinte ligaria mais forte)."""
    for I, S in enumerate(Segs):
        if S.Txt in ("&&", "||"):
            Segs[I] = Novo("nome", "and" if S.Txt == "&&" else "or", S)
    while True:
        Ind = Sig(Segs)
        Bangs = [K for K, I in enumerate(Ind) if Segs[I].Txt == "!" and EhNegacao(Segs, Ind, K)]
        if not Bangs:
            return Segs
        Segs = NegarUm(Segs, Bangs[-1])


def EhNegacao(Segs, Ind, K):
    """'!' e negacao so em posicao de operando; 'T ! E' e '-> ! E' (tipo de erro do proc) nao sao."""
    if K == 0:
        return True
    Ant = Segs[Ind[K - 1]]
    Valor = Ant.Tipo in ("num", "texto", "ftexto") or (Ant.Tipo == "nome" and (Ant.Txt not in RESERVADAS or Ant.Txt in ("true", "false", "none")))
    return not (Valor or Ant.Txt in (")", "]", "->"))


def EhLvalue(Segs, Ind):
    """True se os tokens Ind formam NOME(.NOME | [..])* inteiros."""
    if not Ind or Segs[Ind[0]].Tipo != "nome" or Segs[Ind[0]].Txt in RESERVADAS:
        return False
    K = 1
    while K < len(Ind):
        Txt = Segs[Ind[K]].Txt
        if Txt == "." and K + 1 < len(Ind) and Segs[Ind[K + 1]].Tipo == "nome":
            K += 2
        elif Txt == "[":
            Fim = Fecha(Segs, Ind, K)
            if Fim is None:
                return False
            K = Fim + 1
        else:
            return False
    return True


def TrocarIncDec(Segs):
    """V-04: 'x++' / 'x--' como comando viram 'x += 1' / 'x -= 1' (nunca em expressao)."""
    Ind = Sig(Segs)
    if len(Ind) < 2 or Segs[Ind[-1]].Txt not in ("++", "--") or not EhLvalue(Segs, Ind[:-1]):
        return Segs
    Op = Segs[Ind[-1]]
    Novos = [Novo("simb", "+=" if Op.Txt == "++" else "-=", Op), Espaco(Op), Novo("num", "1", Op)]
    Segs[Ind[-1]:Ind[-1] + 1] = ([] if Segs[Ind[-1] - 1].Tipo == "esp" else [Espaco(Op)]) + Novos
    return Segs


def Recuo(Segs):
    return Segs[0].Txt if Segs and Segs[0].Tipo == "esp" else ""


def AbreBloco(Segs, Ind, K):
    """True se o '{' em Ind[K] abre o corpo da cabeca da linha (e nao e literal de struct, array ou enum)."""
    if K == 0:
        return False
    Ant = Segs[Ind[K - 1]]
    if Ant.Tipo == "nome" and Ant.Txt not in RESERVADAS and Ind[K] == Ind[K - 1] + 1:
        return False
    return Ant.Txt in (")", "]", "else") or (Ant.Tipo in ("nome", "num", "texto") and Ant.Txt != "in")


def AcharAbertura(Segs, Ind):
    """Posicao (em Ind) do '{' que abre o corpo da cabeca (if/else/for/while/proc/test); None se a linha nao e cabeca."""
    K = 1 if Segs[Ind[0]].Txt == "}" else 0
    while K < len(Ind) and Segs[Ind[K]].Txt in ("pub", "export"):
        K += 1
    if K >= len(Ind) or Segs[Ind[K]].Txt not in CABECAS:
        return None
    Prof = 0
    for J in range(K, len(Ind)):
        Txt = Segs[Ind[J]].Txt
        Prof += Txt in ("(", "[")
        Prof -= Txt in (")", "]")
        if Txt == "{" and Prof == 0 and AbreBloco(Segs, Ind, J):
            return J
    return None


def LinhaNova(Nivel, Segs, Ancora):
    """Linha de saida: o recuo (texto novo) mais os segmentos, sem espacos nas pontas."""
    while Segs and Segs[0].Tipo == "esp":
        Segs = Segs[1:]
    while Segs and Segs[-1].Tipo == "esp":
        Segs = Segs[:-1]
    return ([Espaco(Ancora, Nivel)] if Nivel else []) + Segs


def AcharFechaChave(Resto):
    """Indice (em Resto) do '}' que fecha o bloco aberto antes de Resto; None se o bloco continua nas linhas seguintes."""
    Prof = 1
    for I, S in enumerate(Resto):
        if S.Tipo != "simb":
            continue
        Prof += S.Txt == "{"
        Prof -= S.Txt == "}"
        if Prof == 0:
            return I
    return None


def DividirBlocos(Segs):
    """V-01: 'if c { x }' em uma linha vira 3 linhas (cabeca, corpo, '}'); as linhas novas apontam para a de origem."""
    Ind = Sig(Segs)
    Ab = AcharAbertura(Segs, Ind) if Ind else None
    if Ab is None or Ab + 1 >= len(Ind):
        return [Segs]
    Ancora, Nivel = Segs[Ind[Ab]], Recuo(Segs)
    Resto = Segs[Ind[Ab] + 1:]
    Fe = AcharFechaChave(Resto)
    Corpo, Cauda = (Resto, []) if Fe is None else (Resto[:Fe], Resto[Fe:])
    Linhas = [Segs[:Ind[Ab] + 1]]
    if Sig(Corpo):
        Linhas += DividirBlocos(LinhaNova(Nivel + "    ", Corpo, Ancora))
    if Cauda:
        Linhas += DividirBlocos(LinhaNova(Nivel, Cauda, Ancora))
    return Linhas


def AcharAlias(Linhas):
    """Apelido de std:os / std:proc nos imports (None se o arquivo nao importa)."""
    for Segs in Linhas:
        Ind = Sig(Segs)
        if len(Ind) >= 2 and Segs[Ind[0]].Txt == "import" and Segs[Ind[1]].Txt in ('"std:os"', '"std:proc"'):
            return Segs[Ind[-1]].Txt if len(Ind) >= 4 and Segs[Ind[-2]].Txt == "as" else "os"
    return None


def CabecaMain(Segs):
    """Posicoes (em Segs) de '->' e do tipo quando a linha e 'proc main() -> int-like {'; None caso contrario."""
    Ind = Sig(Segs)
    Txts = [Segs[I].Txt for I in Ind]
    if Txts[:4] != ["proc", "main", "(", ")"] or len(Txts) < 6 or Txts[4] != "->" or Txts[5] not in TIPOS_INTEIROS:
        return None
    return Ind[4], Ind[5]


def ChaveLiquida(Segs):
    """Quantas chaves '{' a mais que '}' a linha abre (f"..." e texto nao contam: sao segmentos proprios)."""
    return sum(S.Txt == "{" for S in Segs if S.Tipo == "simb") - sum(S.Txt == "}" for S in Segs if S.Tipo == "simb")


def TraduzirMainRetorno(Linhas, Alias):
    """V-08: 'proc main() -> int' perde o tipo e cada 'return X' dentro dele vira 'Alias.exit(X)'. Devolve (linhas, usou_exit)."""
    Prof, Dentro, Usou = 0, False, False
    for Pos, Segs in enumerate(Linhas):
        Par = None if Dentro else CabecaMain(Segs)
        if Par:
            Dentro = True
            Segs[Par[0]:Par[1] + 1] = []
        if not Dentro:
            continue
        Linhas[Pos], Achou = TrocarReturn(Segs, Alias)
        Usou = Usou or Achou
        Prof += ChaveLiquida(Segs)
        Dentro = Prof > 0
    return Linhas, Usou


def TrocarReturn(Segs, Alias):
    """'return X' -> 'Alias.exit(X)' (so o 1o return da linha; 'return' sozinho fica)."""
    Ind = Sig(Segs)
    Pos = [K for K, I in enumerate(Ind) if Segs[I].Txt == "return"]
    if not Pos or Pos[0] + 1 >= len(Ind):
        return Segs, False
    IR = Ind[Pos[0]]
    Ret = Segs[IR]
    Segs.insert(Ind[-1] + 1, Novo("simb", ")", Ret))
    Fim = IR + 2 if Segs[IR + 1].Tipo == "esp" else IR + 1
    Segs[IR:Fim] = [Novo("nome", Alias, Ret), Novo("simb", ".", Ret), Novo("nome", "exit", Ret), Novo("simb", "(", Ret)]
    return Segs, True
