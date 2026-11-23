# grafo_pastas.py - prova que o grafo de dependencia ENTRE PASTAS de compiler\ nao tem ciclo.
# Objetivo: o G3 reescreve o compilador em Forja (pacote = pasta) e a Forja proibe ciclo de import (E1002);
#   a ferramenta mede, nas duas visoes (#include e chamada de funcao), as pastas em ciclo e as arestas que fecham cada um.
# Uso: python -I tests\g3\grafo_pastas.py [--raiz <compiler>] | --selftest
# Saida: exit 0 = 0 ciclo entre pastas (include e chamada); exit 1 = ha ciclo (lista arquivo:linha); exit 2 = erro de uso.
# Depende de: so a biblioteca padrao. Pasta = 1o nivel de compiler\; para nativo\* usa-se nativo/<sub> (arquivos soltos = nativo).
# Versao: 1.0.0
# Changelog:
#   1.0.0 (19/11/2026) G3 G0b: criacao (logica de deps.py/scc3.py do Opus: include por basename, chamada por definicao externa unica).
# Verificacao: python -I tests\g3\grafo_pastas.py --selftest ; python -I tests\g3\grafo_pastas.py
# Reversao: apagar tests\g3\grafo_pastas.py (ferramenta so de leitura, nao altera nada).
import os
import re
import sys
import collections

RAIZ_PADRAO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'compiler'))
PALAVRAS_NAO_FUNCAO = ('if', 'while', 'for', 'switch', 'return', 'sizeof')
RX_STRING = re.compile(r'"(?:\\.|[^"\\\n])*"')
RX_CHAR = re.compile(r"'(?:\\.|[^'\\\n])*'")
RX_DEFINICAO = re.compile(r'^([A-Za-z_][\w \t\*]*?)\b(\w+)\s*\([^;{]*\)\s*\{', re.M)
RX_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
RX_CHAMADA = re.compile(r'\b([A-Za-z_]\w*)\s*\(')


def PastaDe(Caminho):
    Partes = Caminho.split('/')
    if Partes[0] == 'nativo' and len(Partes) >= 3:
        return 'nativo/' + Partes[1]
    return Partes[0]


def LimparTexto(Texto):
    # remove comentarios, strings e chars preservando as quebras de linha (numero de linha confiavel)
    Texto = re.sub(r'/\*.*?\*/', lambda M: '\n' * M.group(0).count('\n'), Texto, flags=re.S)
    Texto = re.sub(r'//[^\n]*', '', Texto)
    Texto = RX_STRING.sub('""', Texto)
    return RX_CHAR.sub("''", Texto)


def LerFontes(Raiz):
    Fontes = {}
    for Dir, Subs, Arqs in os.walk(Raiz):
        Subs[:] = [S for S in Subs if S not in ('tests', '__pycache__')]
        for Nome in Arqs:
            if Nome.endswith(('.c', '.h')):
                Rel = os.path.relpath(os.path.join(Dir, Nome), Raiz).replace(os.sep, '/')
                with open(os.path.join(Dir, Nome), encoding='utf-8', errors='replace') as Arq:
                    Fontes[Rel] = Arq.read()
    return Fontes


def LinhaDe(Texto, Pos):
    return Texto.count('\n', 0, Pos) + 1


def ArestasDeInclude(Fontes):
    # {(pastaA, pastaB): [(arquivo, linha, destino)]}
    Cabecalhos = collections.defaultdict(list)
    for Rel in Fontes:
        if Rel.endswith('.h'):
            Cabecalhos[Rel.rsplit('/', 1)[-1]].append(Rel)
    Arestas = collections.defaultdict(list)
    for Rel, Texto in Fontes.items():
        Origem = PastaDe(Rel)
        for Achado in RX_INCLUDE.finditer(Texto):
            for Alvo in ResolverInclude(Fontes, Cabecalhos, Rel, Achado.group(1)):
                if PastaDe(Alvo) != Origem:
                    Arestas[(Origem, PastaDe(Alvo))].append((Rel, LinhaDe(Texto, Achado.start()), Alvo))
    return Arestas


def ResolverInclude(Fontes, Cabecalhos, Rel, Incluido):
    # o gcc acha "x/y.h" por -I. (raiz de compiler\) e "y.h" na pasta do includente; so se nada casar cai no nome-base
    for Candidato in (os.path.normpath(Incluido).replace(os.sep, '/'),
                      os.path.normpath(os.path.join(os.path.dirname(Rel), Incluido)).replace(os.sep, '/')):
        if Candidato in Fontes:
            return [Candidato]
    return Cabecalhos.get(Incluido.rsplit('/', 1)[-1], [])


def DefinicoesExternas(Limpos):
    Defs = collections.defaultdict(list)
    for Rel, Texto in Limpos.items():
        for Achado in RX_DEFINICAO.finditer(Texto):
            if Achado.group(1).lstrip().startswith('static'):
                continue
            if Achado.group(2) not in PALAVRAS_NAO_FUNCAO:
                Defs[Achado.group(2)].append(Rel)
    return Defs


def ArestasDeChamada(Fontes):
    Limpos = {Rel: LimparTexto(Texto) for Rel, Texto in Fontes.items() if Rel.endswith('.c')}
    Defs = DefinicoesExternas(Limpos)
    Arestas = collections.defaultdict(list)
    for Rel, Texto in Limpos.items():
        Origem = PastaDe(Rel)
        Vistos = set()
        for Achado in RX_CHAMADA.finditer(Texto):
            Nome = Achado.group(1)
            Alvo = Defs.get(Nome)
            if not Alvo or len(Alvo) != 1 or PastaDe(Alvo[0]) == Origem or (Nome, Alvo[0]) in Vistos:
                continue
            Vistos.add((Nome, Alvo[0]))
            Arestas[(Origem, PastaDe(Alvo[0]))].append((Rel, LinhaDe(Texto, Achado.start()), Nome + '() em ' + Alvo[0]))
    return Arestas


def ComponentesFortes(Nos, Vizinhos):
    # Tarjan iterativo; devolve so os componentes com mais de 1 no
    Indice, Baixo, Pilha, NaPilha, Comps, Contador = {}, {}, [], set(), [], [0]
    for Raiz in Nos:
        if Raiz in Indice:
            continue
        Trabalho = [(Raiz, iter(sorted(Vizinhos.get(Raiz, ()))))]
        Indice[Raiz] = Baixo[Raiz] = Contador[0]
        Contador[0] += 1
        Pilha.append(Raiz)
        NaPilha.add(Raiz)
        while Trabalho:
            V, Itera = Trabalho[-1]
            Avancou = False
            for W in Itera:
                if W not in Indice:
                    Indice[W] = Baixo[W] = Contador[0]
                    Contador[0] += 1
                    Pilha.append(W)
                    NaPilha.add(W)
                    Trabalho.append((W, iter(sorted(Vizinhos.get(W, ())))))
                    Avancou = True
                    break
                if W in NaPilha:
                    Baixo[V] = min(Baixo[V], Indice[W])
            if Avancou:
                continue
            Trabalho.pop()
            if Trabalho:
                Pai = Trabalho[-1][0]
                Baixo[Pai] = min(Baixo[Pai], Baixo[V])
            if Baixo[V] == Indice[V]:
                Comp = []
                while True:
                    W = Pilha.pop()
                    NaPilha.discard(W)
                    Comp.append(W)
                    if W == V:
                        break
                if len(Comp) > 1:
                    Comps.append(sorted(Comp))
    return Comps


def CiclosDeArestas(Arestas):
    # Arestas: {(a, b): [locais]} -> lista de (pastas_do_ciclo, [(a, b, locais)]) das arestas internas a cada componente
    Viz = collections.defaultdict(set)
    Nos = set()
    for (A, B) in Arestas:
        Viz[A].add(B)
        Nos.update((A, B))
    Resultado = []
    for Comp in ComponentesFortes(sorted(Nos), Viz):
        Dentro = set(Comp)
        Internas = [(A, B, Arestas[(A, B)]) for (A, B) in sorted(Arestas) if A in Dentro and B in Dentro]
        Resultado.append((Comp, Internas))
    return Resultado


def Relatar(Titulo, Arestas):
    Ciclos = CiclosDeArestas(Arestas)
    print('== visao %s: %d pastas-aresta, %d ciclo(s)' % (Titulo, len(Arestas), len(Ciclos)))
    for Comp, Internas in Ciclos:
        print('  CICLO entre pastas: ' + ', '.join(Comp))
        for A, B, Locais in Internas:
            print('    %s -> %s (%d)' % (A, B, len(Locais)))
            for Arq, Linha, Alvo in Locais:
                print('      %s:%d  %s' % (Arq, Linha, Alvo))
    return len(Ciclos)


def Selftest():
    Sem = {('a', 'b'): [('a/x.c', 1, 'b/y.h')], ('b', 'c'): [('b/y.c', 2, 'c/z.h')], ('a', 'c'): [('a/x.c', 3, 'c/z.h')]}
    Com = dict(Sem)
    Com[('c', 'a')] = [('c/z.c', 9, 'f() em a/x.c')]
    Com[('d', 'e')] = [('d/q.c', 1, 'e/r.h')]
    Erros = []
    if CiclosDeArestas(Sem):
        Erros.append('grafo sem ciclo acusou ciclo')
    Achados = CiclosDeArestas(Com)
    if len(Achados) != 1 or Achados[0][0] != ['a', 'b', 'c']:
        Erros.append('grafo com ciclo a-b-c nao foi achado: %r' % (Achados,))
    elif sorted((A, B) for A, B, _ in Achados[0][1]) != [('a', 'b'), ('a', 'c'), ('b', 'c'), ('c', 'a')]:
        Erros.append('arestas internas do ciclo erradas')
    if PastaDe('nativo/x64/a.c') != 'nativo/x64' or PastaDe('nativo/alvo.c') != 'nativo' or PastaDe('common/diag.c') != 'common':
        Erros.append('PastaDe errada')
    if LimparTexto('int a; /* x\ny */ b("n\\"n");\n').count('\n') != 2:
        Erros.append('LimparTexto perdeu quebra de linha')
    for Erro in Erros:
        print('SELFTEST FALHOU: ' + Erro)
    print('selftest: ' + ('OK' if not Erros else 'FALHOU'))
    return 0 if not Erros else 1


def Principal(Argv):
    if '--selftest' in Argv:
        return Selftest()
    Raiz = RAIZ_PADRAO
    if '--raiz' in Argv:
        Pos = Argv.index('--raiz')
        if Pos + 1 >= len(Argv):
            print('uso: --raiz <pasta compiler>')
            return 2
        Raiz = Argv[Pos + 1]
    if not os.path.isdir(Raiz):
        print('raiz inexistente: ' + Raiz)
        return 2
    Fontes = LerFontes(Raiz)
    print('arquivos: %d (.c/.h, sem tests)' % len(Fontes))
    Total = Relatar('include', ArestasDeInclude(Fontes)) + Relatar('chamada', ArestasDeChamada(Fontes))
    print('RESULTADO: ' + ('0 ciclo entre pastas' if Total == 0 else '%d ciclo(s) entre pastas' % Total))
    return 0 if Total == 0 else 1


if __name__ == '__main__':
    sys.exit(Principal(sys.argv[1:]))
