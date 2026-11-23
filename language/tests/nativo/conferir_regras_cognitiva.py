# -*- coding: utf-8 -*-
"""Complexidade cognitiva (modelo SonarSource) de uma funcao C, medida por texto (tokens), para o conferir_regras.py.

Regras (CROSS-R05, Emenda 5 do contrato_ia1): +1 por if / else if / else / for / while / do-while / ternario / catch;
cada sequencia de '&&' ou '||' do mesmo operador conta 1 e cada troca de operador conta +1; soma-se o nivel de
aninhamento de cada construcao (if, for, while, do, switch, ternario); 'switch' inteiro conta 1 (+aninhamento) e
case/default contam 0; 'else if' soma 1 sem aninhamento; chamada recursiva soma 1.
Fora da conta (como no complexidade.py): switch com 'default' em que cada ramo so chama funcao e nao ha if/for/while/do/'?'.

Limitacoes conhecidas da medida por texto:
  - 'a || b && c || d' conta 3 aqui (3 sequencias, Sonar); o complexidade.py (arvore por precedencia) conta 2.
  - macro de laco sem ponto e virgula ('LISTA_PARA(x) { ... }') vira uma instrucao de expressao ate o proximo ';'.
  - o ':' do ternario e o do rotulo 'case' nao se distinguem: so o '?' conta.

Mede tambem os niveis (aninhamento de construcoes de controle, igual ao 'indentacao' do complexidade.py: if/for sem chaves contam).

Uso: importado por conferir_regras.py (MedirCorpo(Corpo, Nome) -> (cognitiva, niveis)).
Versao: 1.0.0
Changelog: 1.0.0 - P-IA1-reg1c: primeira versao (parser de instrucoes por tokens; selftest no conferir_regras.py).
Verificacao: python -I conferir_regras.py --selftest
"""

import re

ReToken = re.compile(r"\w+|&&|\|\||[{}();,?:]")
Abrem = ("(", "{")
Fecham = (")", "}")
Operadores = ("&&", "||")
Estruturas = ("if", "else", "for", "while", "do", "?")
NaoChamadas = ("if", "for", "while", "switch", "return", "sizeof")


class Quadro:
    """Estado de uma expressao dentro de um par de parenteses/chaves: ultimo operador booleano e fundo de ternarios."""
    __slots__ = ("Ultimo", "Ramos", "Base")

    def __init__(self, Base):
        self.Ultimo = ""
        self.Ramos = Base
        self.Base = Base


class ContadorCognitivo:
    """Percorre os tokens do corpo de uma funcao e soma a complexidade cognitiva em Custo."""

    def __init__(self, Tokens, Nome):
        self.Tokens = Tokens
        self.Nome = Nome
        self.Pos = 0
        self.Custo = 0
        self.Maximo = 0

    def Atual(self):
        return self.Tokens[self.Pos] if self.Pos < len(self.Tokens) else ""

    def Medir(self):
        if self.Atual() == "{":
            self.Bloco(0)
        return self.Custo

    def Bloco(self, Nivel):
        self.Pos += 1
        while self.Atual() not in ("}", ""):
            self.Instrucao(Nivel)
        self.Pos += 1

    def Instrucao(self, Nivel):
        self.Maximo = max(self.Maximo, Nivel)
        Tratadores = {"{": self.Bloco, "if": self.If, "for": self.Laco, "while": self.Laco, "do": self.Faca,
                      "switch": self.Chave, "case": self.Rotulo, "default": self.Rotulo}
        Tratador = Tratadores.get(self.Atual())
        if Tratador:
            Tratador(Nivel)
        else:
            self.Varrer(Nivel, ";")

    def Condicao(self, Nivel):
        """Consome '(' ... ')' contando o que ha dentro; o nivel da condicao e o da construcao."""
        self.Pos += 1
        if self.Atual() == "(":
            self.Pos += 1
            self.Varrer(Nivel, ")")

    def If(self, Nivel, ElseIf=False):
        self.Custo += 1 if ElseIf else 1 + Nivel
        self.Condicao(Nivel)
        self.Instrucao(Nivel + 1)
        if self.Atual() != "else":
            return
        self.Pos += 1
        if self.Atual() == "if":
            self.If(Nivel, True)
            return
        self.Custo += 1
        self.Instrucao(Nivel + 1)

    def Laco(self, Nivel):
        self.Custo += 1 + Nivel
        self.Condicao(Nivel)
        self.Instrucao(Nivel + 1)

    def Faca(self, Nivel):
        self.Custo += 1 + Nivel
        self.Pos += 1
        self.Instrucao(Nivel + 1)
        if self.Atual() == "while":
            self.Condicao(Nivel)
            self.Pos += 1 if self.Atual() == ";" else 0

    def Rotulo(self, Nivel):
        """case X: / default: contam 0; consome o rotulo ate o ':'."""
        while self.Atual() not in (":", ""):
            self.Pos += 1
        self.Pos += 1

    def Chave(self, Nivel):
        Fim = self.FimDoCorpoDoSwitch()
        if Fim is not None and EhDespacho(self.Tokens[self.Pos:Fim]):
            self.Pos = Fim
            self.Maximo = max(self.Maximo, Nivel + 1)
            return
        self.Custo += 1 + Nivel
        self.Condicao(Nivel)
        if self.Atual() == "{":
            self.Bloco(Nivel + 1)

    def FimDoCorpoDoSwitch(self):
        """Posicao logo apos a '}' que fecha o corpo do switch atual (ou None se nao achar)."""
        Profundidade = 0
        Visto = False
        for Idx in range(self.Pos, len(self.Tokens)):
            Tok = self.Tokens[Idx]
            Profundidade += (Tok == "{") - (Tok == "}")
            Visto = Visto or Tok == "{"
            if Visto and Profundidade == 0:
                return Idx + 1
        return None

    def Varrer(self, Nivel, Ate):
        """Consome tokens ate o 'Ate' (';' ou ')') do nivel de fora, contando booleanos, ternarios e recursao."""
        Pilha = [Quadro(0)]
        while self.Pos < len(self.Tokens):
            Tok = self.Tokens[self.Pos]
            self.Pos += 1
            if self.Token(Tok, Nivel, Ate, Pilha):
                return

    def Token(self, Tok, Nivel, Ate, Pilha):
        """Trata 1 token de expressao; True quando a expressao terminou."""
        Quadro_ = Pilha[-1]
        if Tok in Abrem:
            self.Custo += self.RecursaoAoAbrir(Tok)
            Pilha.append(Quadro(Quadro_.Ramos))
            return False
        if Tok in Fecham:
            return self.Fechar(Tok, Ate, Pilha)
        if Tok == ";" and len(Pilha) == 1 and Ate == ";":
            return True
        self.Operador(Tok, Nivel, Quadro_)
        return False

    def Operador(self, Tok, Nivel, Quadro_):
        """Ternario, && / || e os separadores que reiniciam a sequencia."""
        if Tok == "?":
            self.Custo += 1 + Nivel + Quadro_.Ramos
            Quadro_.Ramos += 1
            Quadro_.Ultimo = ""
            return
        if Tok in Operadores:
            self.Custo += Tok != Quadro_.Ultimo
            Quadro_.Ultimo = Tok
            return
        if Tok == ":":
            Quadro_.Ultimo = ""
        elif Tok in (";", ","):
            Quadro_.Ultimo, Quadro_.Ramos = "", Quadro_.Base

    def RecursaoAoAbrir(self, Tok):
        """1 se o '(' recem consumido abre a chamada da propria funcao."""
        Anterior = self.Tokens[self.Pos - 2] if self.Pos >= 2 else ""
        return 1 if Tok == "(" and Anterior == self.Nome else 0

    def Fechar(self, Tok, Ate, Pilha):
        if len(Pilha) > 1:
            Pilha.pop()
            return False
        if Tok == Ate:
            return True
        if Tok == "}":
            self.Pos -= 1
            return True
        return False


def EhDespacho(Tokens):
    """True se o switch (tokens de 'switch' ate a '}') tem default, nenhuma estrutura de decisao e uma chamada em cada ramo."""
    if "default" not in Tokens or any(Tok in Estruturas for Tok in Tokens):
        return False
    Secoes = SecoesDoSwitch(Tokens)
    return bool(Secoes) and bool(Secoes[-1]) and all(TemChamada(Secao) for Secao in Secoes if Secao)


def SecoesDoSwitch(Tokens):
    """Lista, por rotulo case/default, os pares (token, proximo) do que vem depois do rotulo ate o proximo."""
    Secoes = []
    EmRotulo = False
    for Idx, Tok in enumerate(Tokens):
        if Tok in ("case", "default"):
            Secoes.append([])
            EmRotulo = True
            continue
        if EmRotulo:
            EmRotulo = Tok != ":"
            continue
        if Secoes:
            Secoes[-1].append((Tok, Tokens[Idx + 1] if Idx + 1 < len(Tokens) else ""))
    return Secoes


def TemChamada(Secao):
    """Algum identificador (fora de palavras de controle) seguido de '('."""
    return any(Seguinte == "(" and re.fullmatch(r"[A-Za-z_]\w*", Tok) and Tok not in NaoChamadas for Tok, Seguinte in Secao)


def MedirCorpo(Corpo, Nome):
    """(cognitiva, niveis) do corpo de funcao C (texto ja sem comentarios/literais e sem linhas '#').
    niveis = maior numero de construcoes de controle (if/for/while/do/switch) aninhadas ao redor de uma instrucao, com ou sem chaves."""
    Contador = ContadorCognitivo(ReToken.findall(Corpo), Nome)
    return Contador.Medir(), Contador.Maximo
