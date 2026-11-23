# espelho.py - espelho neutro do G3 (onda 0): monta, em pasta scratch, o par E1-neutro (forja_fj com os stubs neutros) x E0-espelho (compiler\ com as MESMAS funcoes neutras)
# Objetivo: provar um lote de porte sem esperar os outros lotes. O stub de outro lote devolve o neutro (0, false, 0.0, "", none, T{}, {}) nos dois lados, no lugar do panic; assim o
#   forja candidato (E1) e o oraculo (E0-espelho) seguem a execucao inteira e paridade.py compara de verdade. Stub com retorno `^T` (neutro = `nada as ^T`) mantem o panic no
#   Forja e vira fputs("G3 stub: <pac>.<nome>") + exit(101) no C: os dois caem (classe `cai` do paridade 1.2.0).
# Uso:
#   python -I tests\g3\espelho.py --lote <arquivos .fj do lote | -> --saida <pasta scratch> [--real-de <forja_fj>] [--cobertura] [--sem-dev] [--jobs N]
#   python -I tests\g3\espelho.py --lote - --corte tests\g3\corte_bir0 --saida <pasta>   (BIR0: corte de nativo_construir_exe nos 2 lados; saidas e0_corte.exe, e1_corte_release/dev.exe)
#   python -I tests\g3\espelho.py --lote - --corte tests\g3\corte_otim0 --saida <pasta>  (OTIM0: mesmo nome de saida; use pasta --saida DIFERENTE do bir0, senao o e0_corte.exe do bir0 e sobrescrito)
#   python -I tests\g3\espelho.py --autoteste        (5 provas: conjunto vazio, mesmas funcoes nos 2 lados, so 1 lado, corte BIR0 e corte OTIM0 em 01_ola; ~1,5 min)
#   Cobertura (A4): --cobertura-zerar; paridade.py --oraculo <saida>\e0_espelho.exe --forja <saida>\e0_cobertura\language\compiler\forja.exe --jobs 1 ... (o exe de cobertura roda como candidato, 1 por vez); --saida <pasta> --cobertura-relatorio (grava cobertura_funcoes.tsv)
#   Depois: python -I tests\g3\paridade.py --oraculo <saida>\e0_espelho.exe --forja <saida>\e1_neutro_release.exe --fase ast,tipos,check
# Conjunto neutro = TODO `panic("G3 stub: ...")` que sobra na arvore = (forja_fj real, ou --real-de) + os .fj do --lote por cima (cada um no caminho relativo a forja_fj\ em que mora).
#   Cada lote tira os seus stubs do conjunto (porque ja nao sao stub): o espelho recebe so o que continua stub. Stub sem funcao C (nomes_t1.tsv), sem corpo achado ou de tipo
#   incompativel com o C fica panic no Forja e SEM troca no C: o candidato cai e o oraculo segue = classe `stub` do paridade (nunca falso diverge).
# Saida (<saida>): e1\forja_fj + doc + std, e0\language\compiler (+ runtime/std/doc/third/tools por juncao), e0_espelho.exe, e1_neutro_release.exe, e1_neutro_dev.exe, plano.tsv (1 linha por stub),
#   lote_funcoes.tsv (funcoes C dos .fj do lote, para a cobertura A4), relatorio.txt; --cobertura: e0_cobertura\...\forja.exe (gcc --coverage; o .gcda sai na pasta do cache de objetos).
# Depende de: libclang (pip --user) via C:\CLAUDE_AGI\tools\c2forja\c2forja_clang.py; compiler\construir_rapido.py (cache de objetos); gcc no PATH; compiler\forja.exe (E0 real, para compilar o E1).
# Versao: 1.2.0
# Changelog: 1.3.0 (19/11/2026) V3 X64: corte tests\g3\corte_x64 (fronteira Alvo.gerar); PACOTES_C_REAL_COM_CORTE ganha nativo/x64 (o C do x64 fica real nos 2 lados; backup espelho.py.bak-1.2.0). 1.2.0 (19/11/2026) V2 OTIM0: corte tests\g3\corte_otim0 (release: BIR -> OtimExecutar -> relato otim + passes) pelo mesmo --corte; com --corte o C de PACOTES_C_REAL_COM_CORTE (nativo.optimizer) fica real nos 2 lados (antes o espelho neutralizava o passes.c inteiro e o E0 nao rodava passe nenhum); autoteste (v): corte OTIM0 em 01_ola, build --release, conjunto neutro normal, stdout nao vazio nos 2 e relato 'otim: ' no C; (iv) e (v) viram TesteCorteDe (generico). 1.1.0 (19/11/2026) BIR0 (D0778): opcao --corte <pasta> troca o corpo de nativo.nativo_construir_exe (Forja: corte_e1.fj, so linhas do corpo + import) e de
#   NativoConstruirExe (C: corte_e0.c, bloco { } inteiro) nos 2 lados, sem neutro nessa rotina; saidas e0_corte.exe e e1_corte_<dev|release>.exe; relatorio com sha256 dos cortes;
#   autoteste (iv): 01_ola.fj com corte nos 2 lados (rc 0, stdout igual e nao vazio); (i) passa `lista` e `timeout` ao RodarTudo do paridade (1.3.0+ exige; antes dava KeyError). 1.0.0 (19/11/2026) G3 onda 0: criacao. Neutro lido do proprio retorno do stub Forja e conferido
#   contra o tipo C (libclang); build do E0-espelho = construir_rapido com -w -ftrivial-auto-var-init=zero (e --coverage na variante); E1 = forja.exe real (check, build --release e dev); autoteste i-iii.
# Verificacao: python -I tests\g3\espelho.py --autoteste ; python -I tests\g3\espelho.py --lote - --corte tests\g3\corte_bir0 --saida <pasta>
# Reversao: apagar tests\g3\espelho.py e tests\g3\corte_bir0 (ferramenta de leitura: grava so na pasta --saida e em %TEMP%\forja_espelho; nunca em compiler\, forja_fj\ ou forja.exe reais);
#   a versao 1.0.0 esta em tests\g3\espelho.py.bak-1.0.0 (copiar de volta sobre espelho.py).
import argparse
import concurrent.futures
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import _winapi
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
COMPILADOR = RAIZ / 'compiler'
FORJA_REAL = COMPILADOR / 'forja.exe'
NOMES_TSV = RAIZ / 'gaveta' / 'g3_t1' / 'nomes_t1.tsv'
C2FORJA = Path(r'C:\CLAUDE_AGI\tools\c2forja')
CACHE_RAIZ = Path(tempfile.gettempdir()) / 'forja_espelho'
VERSAO = '1.3.0'
ID_CORTE = 'nativo.nativo_construir_exe'  # corte BIR0: o neutro desta rotina sai do conjunto; o corpo vem de --corte
PACOTES_C_REAL_COM_CORTE = ('nativo/optimizer', 'nativo/x64')  # id do stub usa '/' no pacote (ex.: nativo/optimizer.absorver)  # com --corte, o C destes pacotes fica REAL nos 2 lados (o corte chama o otimizador de verdade); o Forja segue neutro
ARQ_C_CORTE = Path('nativo') / 'nativo.c'  # NativoConstruirExe mora aqui no compiler\
NOME_C_CORTE = 'NativoConstruirExe'
CODIGO_PANIC = 101
FLAGS_ESPELHO = '-w -ftrivial-auto-var-init=zero'  # -w: o corpo neutro deixa parametro e static sem uso (o -Werror do .bat so vale no E0 real)
JUNCOES = ('runtime', 'std', 'doc', 'third', 'tools')
IGNORAR_COMPILADOR = shutil.ignore_patterns('*.exe', '*.o', '*.pdb', '*.rsp', '__pycache__', '*.gcno', '*.gcda')
REGEX_PANIC = re.compile(r'^\s*panic\("G3 stub: ([^\s"]+)[^"]*"\)\s*$')
REGEX_CABECALHO = re.compile(r'^(?:\w+\s+)*proc\s+(\w+)\s*(?:\[[^\]]*\])?\((.*)\)\s*(?:->\s*(.+?))?\s*\{\s*$')
REGEX_STRUCT_VAZIA = re.compile(r'^return [\w.]+\{\}$')
RETORNOS_ESCALAR = ('return false', 'return 0', 'return 0.0')
TIPOS_ESCALAR_FORJA = {'bool': 'return false', 'f32': 'return 0.0', 'f64': 'return 0.0', 'int': 'return 0', 'uint': 'return 0', 'usize': 'return 0', 'isize': 'return 0',
                       **{f'{S}{N}': 'return 0' for S in 'iu' for N in (8, 16, 32, 64)}}
# neutro Forja -> categorias de retorno C compativeis (outra combinacao = panic nos dois lados do que for possivel, nunca neutro so de um lado)
COMPATIVEL = {'escalar': {'escalar'}, 'none': {'ponteiro'}, 'texto': {'ponteiro'}, 'fatia': {'ponteiro'}, 'struct': {'struct'}, 'void': {'void'}}
KIND_ESCALAR = ('BOOL', 'CHAR_U', 'UCHAR', 'CHAR16', 'CHAR32', 'USHORT', 'UINT', 'ULONG', 'ULONGLONG', 'UINT128', 'CHAR_S', 'SCHAR', 'WCHAR', 'SHORT', 'INT', 'LONG',
                'LONGLONG', 'INT128', 'FLOAT', 'DOUBLE', 'LONGDOUBLE', 'ENUM')


# ---------------------------------------------------------------- pastas e juncoes
def ApagarPasta(Pasta):
    # juncao sai por rmdir (nunca rmtree: apagaria o alvo real)
    Pasta = Path(Pasta)
    if not Pasta.exists():
        return
    for Raiz, Subs, _ in os.walk(Pasta):
        for Sub in list(Subs):
            if os.path.isjunction(os.path.join(Raiz, Sub)):
                os.rmdir(os.path.join(Raiz, Sub))
                Subs.remove(Sub)
    shutil.rmtree(Pasta, ignore_errors=True)


def FazerJuncoes(Language):
    for Nome in JUNCOES:
        _winapi.CreateJunction(str(RAIZ / Nome), str(Language / Nome))


def CopiarCompilador(Language):
    Language.mkdir(parents=True, exist_ok=True)
    shutil.copytree(COMPILADOR, Language / 'compiler', ignore=IGNORAR_COMPILADOR)
    FazerJuncoes(Language)


def CopiarForjaFj(Base, Destino):
    shutil.copytree(Base, Destino / 'forja_fj', ignore=shutil.ignore_patterns('__pycache__'))
    for Nome in ('doc', 'std'):
        shutil.copytree(RAIZ / Nome, Destino / Nome)


def LerListaLote(Texto):
    # uma linha por arquivo .fj; linha vazia e '#' ignorados
    return [L.strip().strip('"') for L in Texto.splitlines() if L.strip() and not L.strip().startswith('#')]


def RelativoAForjaFj(Caminho):
    Partes = Path(Caminho).resolve().parts
    if 'forja_fj' not in Partes:
        raise ValueError(f'o arquivo do lote precisa morar dentro de uma pasta forja_fj\\: {Caminho}')
    Indice = len(Partes) - 1 - Partes[::-1].index('forja_fj')
    return Path(*Partes[Indice + 1:])


def SobreporLote(ArvoreE1, Arquivos):
    Copiados = []
    for Arq in Arquivos:
        if not Path(Arq).is_file() or not str(Arq).endswith('.fj'):
            raise ValueError(f'arquivo do lote inexistente ou sem .fj: {Arq}')
        Rel = RelativoAForjaFj(Arq)
        Destino = ArvoreE1 / 'forja_fj' / Rel
        Destino.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(Arq, Destino)
        Copiados.append(Rel.as_posix())
    return Copiados


# ---------------------------------------------------------------- lado Forja: stubs e neutros
def LerLinhas(Caminho):
    return Path(Caminho).read_bytes().decode('utf-8').split('\n')


def GravarLinhas(Caminho, Linhas):
    Path(Caminho).write_bytes('\n'.join(Linhas).encode('utf-8'))


def ClassificarRetornoStub(Linhas, I):
    # Entrada: linhas do .fj e o indice do panic. Saida: (tipo de neutro, retorno em texto). 'nada'/'desconhecido' = o panic fica.
    Prox = Linhas[I + 1].strip() if I + 1 < len(Linhas) else ''
    if Prox == '}':
        return 'void'
    if Prox in RETORNOS_ESCALAR:
        return 'escalar'
    if Prox == 'return none':
        return 'none'
    if Prox == 'return ""':
        return 'texto'
    if Prox == 'return {}':
        return 'fatia'
    if REGEX_STRUCT_VAZIA.match(Prox):
        return 'struct'
    if Prox.startswith('let nada:') and 'return nada as' in Linhas[I + 2]:
        return 'nada'
    return 'desconhecido'


def ColherStubsForja(ArvoreE1):
    # Entrada: pasta que tem forja_fj\. Saida: lista de dicts (id, pacote, nome, arquivo .fj, linha do panic, tipo de neutro).
    Stubs = []
    Base = ArvoreE1 / 'forja_fj'
    for Caminho in sorted(Base.rglob('*.fj')):
        Linhas = LerLinhas(Caminho)
        for I, Linha in enumerate(Linhas):
            Achado = REGEX_PANIC.match(Linha.rstrip('\r'))
            if not Achado:
                continue
            Id = Achado.group(1)
            Pacote, Nome = Id.rsplit('.', 1)
            Cab = REGEX_CABECALHO.match(Linhas[I - 1].rstrip('\r')) if I > 0 else None
            Tipo = ClassificarRetornoStub(Linhas, I) if Cab and Cab.group(1) == Nome else 'desconhecido'
            Stubs.append({'id': Id, 'pacote': Pacote, 'nome': Nome, 'arquivo': Caminho, 'linha': I, 'tipo': Tipo, 'real': False})
    return Stubs


def NeutroDoTipoForja(Retorno):
    # Entrada: texto do `-> T` do proc (None = sem retorno). Saida: (tipo de neutro, linha de retorno ou None); ValueError se o tipo nao tem neutro simples.
    if Retorno is None:
        return 'void', None
    Tipo = Retorno.strip()
    if Tipo in TIPOS_ESCALAR_FORJA:
        return 'escalar', TIPOS_ESCALAR_FORJA[Tipo]
    if Tipo == 'str':
        return 'texto', 'return ""'
    if Tipo.startswith('?'):
        return 'none', 'return none'
    raise ValueError(f'tipo de retorno Forja sem neutro simples: {Tipo}')


def LocalizarProcForja(ArvoreE1, Pacote, Nome):
    # proc real (nao stub) pelo pacote (pasta de forja_fj) e nome: (arquivo, indice do cabecalho, indice do `}` de fechamento, retorno)
    Pasta = ArvoreE1 / 'forja_fj' / Pacote
    Achados = []
    for Caminho in sorted(Pasta.glob('*.fj')):
        Linhas = LerLinhas(Caminho)
        for I, Linha in enumerate(Linhas):
            Cab = REGEX_CABECALHO.match(Linha.rstrip('\r'))
            if Cab and Cab.group(1) == Nome:
                Fim = next(J for J in range(I + 1, len(Linhas)) if Linhas[J].rstrip('\r') == '}')
                Achados.append((Caminho, I, Fim, Cab.group(3)))
    if len(Achados) != 1:
        raise ValueError(f'proc {Pacote}.{Nome}: {len(Achados)} definicoes em forja_fj\\{Pacote}')
    return Achados[0]


def ItemDeFuncaoReal(ArvoreE1, Id):
    # funcao real escolhida por --funcoes (autoteste): corpo trocado pelo neutro do tipo de retorno
    Pacote, Nome = Id.rsplit('.', 1)
    Arq, Cab, Fim, Retorno = LocalizarProcForja(ArvoreE1, Pacote, Nome)
    Tipo, Linha = NeutroDoTipoForja(Retorno)
    return {'id': Id, 'pacote': Pacote, 'nome': Nome, 'arquivo': Arq, 'linha': Cab, 'fim': Fim, 'tipo': Tipo, 'retorno': Linha, 'real': True}


def LerCorteFj(Caminho):
    # corte_e1.fj: linhas `import` vao para o topo do nativo.fj; o resto e o corpo da proc (sem cabecalho nem chaves). Devolve {'imports', 'corpo'} com CRLF (o .fj da casa e CRLF).
    Imports, Corpo = [], []
    for Linha in LerLinhas(Caminho):
        Limpa = Linha.rstrip('\r')
        if Limpa.startswith('import '):
            Imports.append(Limpa)
        else:
            Corpo.append(Limpa)
    while Corpo and Corpo[-1].strip() == '':
        Corpo.pop()
    if not Imports:
        raise ValueError(f'corte_e1.fj sem a linha import (ex.: import "std:os"): {Caminho}')
    return {'imports': Imports, 'corpo': [L + '\r' for L in Corpo]}


def LerCorteC(Caminho):
    # corte_e0.c: o bloco { } inteiro de NativoConstruirExe; LF no arquivo, CRLF no nativo.c da copia (mesmo EOL do resto do arquivo)
    Texto = Path(Caminho).read_bytes().decode('utf-8')
    if not Texto.lstrip().startswith('{') or not Texto.rstrip().endswith('}'):
        raise ValueError(f'corte_e0.c precisa ser um bloco {{ }} inteiro: {Caminho}')
    return Texto.replace('\r\n', '\n').replace('\n', '\r\n')


def InserirImports(Linhas, Novos):
    # acrescenta no topo (logo depois do ultimo import) cada linha que ainda nao existe no arquivo
    for Nova in Novos:
        if any(L.rstrip('\r') == Nova for L in Linhas):
            continue
        Posicoes = [I for I, L in enumerate(Linhas) if L.startswith('import ')]
        Linhas.insert(Posicoes[-1] + 1 if Posicoes else 0, Nova + '\r')
    return Linhas


def AplicarNeutrosForja(Itens):
    # Troca nos .fj da copia: stub neutro perde a linha do panic; funcao real escolhida ganha corpo neutro; corte BIR0 troca o corpo inteiro e acrescenta o import. Itens em ordem; edita de baixo para cima por arquivo.
    PorArquivo = {}
    for Item in Itens:
        if Item['modo'] in ('neutro', 'corte'):
            PorArquivo.setdefault(Item['arquivo'], []).append(Item)
    for Arquivo, Lista in PorArquivo.items():
        Linhas = LerLinhas(Arquivo)
        Imports = []
        for Item in sorted(Lista, key=lambda X: -X['linha']):
            if Item['modo'] == 'corte':
                Linhas[Item['linha'] + 1:Item['fim']] = Item['linhas_fj']
                Imports += Item['imports_fj']
            elif Item['real']:
                Linhas[Item['linha'] + 1:Item['fim']] = [] if Item['retorno'] is None else ['    ' + Item['retorno']]
            else:
                del Linhas[Item['linha']]
        GravarLinhas(Arquivo, InserirImports(Linhas, Imports))
    return sum(len(L) for L in PorArquivo.values())


# ---------------------------------------------------------------- lado C: corpo por libclang
def ParsearFuncoesC(Caminho):
    # Processo filho: abre o .c/.h com as flags do E0 e devolve {nome: {linha, ini, fim, categoria, retorno}} das definicoes do proprio arquivo.
    sys.path.insert(0, str(C2FORJA))
    import c2forja_clang as Cc
    Cx = Cc.Cindex
    Unidade, _ = Cc.AbrirUnidade(Caminho)
    Dados = Path(Caminho).read_bytes()
    Achados = {}
    for Cur in Unidade.cursor.get_children():
        if Cur.kind != Cx.CursorKind.FUNCTION_DECL or not Cur.is_definition() or not Cc.NoDoArquivo(Cur, Caminho):
            continue
        Corpo = next((F for F in Cur.get_children() if F.kind == Cx.CursorKind.COMPOUND_STMT), None)
        if Corpo is None:
            continue
        Ini, Fim = Corpo.extent.start.offset, Corpo.extent.end.offset
        if Dados[Ini:Ini + 1] != b'{' or Dados[Fim - 1:Fim] != b'}':
            continue  # definicao montada por macro: sem corpo editavel
        Tipo = Cur.result_type.get_canonical()
        Nome = Tipo.kind.name
        Categoria = 'void' if Nome == 'VOID' else 'ponteiro' if Nome == 'POINTER' else 'struct' if Nome == 'RECORD' else 'escalar' if Nome in KIND_ESCALAR else 'outro'
        Achados[Cur.spelling] = {'linha': Cur.location.line, 'ini': Ini, 'fim': Fim, 'categoria': Categoria, 'retorno': Cur.result_type.spelling}
    return Achados


def HashDosHeaders():
    Sha = hashlib.sha256()
    for Cab in sorted(COMPILADOR.rglob('*.h')):
        Sha.update(Cab.read_bytes())
    return Sha.hexdigest()[:16]


def ParseComCache(Caminho, HashHeaders):
    # cache em %TEMP%\forja_espelho\parse por (versao, sha do arquivo, hash dos headers): o parse nao depende do lote
    Chave = hashlib.sha256((VERSAO + HashHeaders + hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()).encode()).hexdigest()[:24]
    Arq = CACHE_RAIZ / 'parse' / f'{Chave}.json'
    if Arq.is_file():
        return json.loads(Arq.read_text(encoding='utf-8'))
    return None


def ParsearTodos(Arquivos, Jobs):
    # Entrada: caminhos de .c/.h. Saida: {caminho: {nome: info}}; arquivos ja no cache nao reabrem o libclang.
    (CACHE_RAIZ / 'parse').mkdir(parents=True, exist_ok=True)
    HashHeaders = HashDosHeaders()
    Resultado, Faltam = {}, []
    for Caminho in Arquivos:
        Dado = ParseComCache(Caminho, HashHeaders)
        if Dado is None:
            Faltam.append(Caminho)
        else:
            Resultado[Caminho] = Dado
    if Faltam:
        with concurrent.futures.ProcessPoolExecutor(max_workers=Jobs) as Pool:
            for Caminho, Dado in zip(Faltam, Pool.map(ParsearFuncoesC, Faltam)):
                Resultado[Caminho] = Dado
                Chave = hashlib.sha256((VERSAO + HashHeaders + hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()).encode()).hexdigest()[:24]
                (CACHE_RAIZ / 'parse' / f'{Chave}.json').write_text(json.dumps(Dado), encoding='utf-8')
    return Resultado


def LerNomes():
    # (pacote, nome Forja) -> (arquivo C relativo a compiler\, nome C, linha) so das linhas `proc`
    Mapa = {}
    with open(NOMES_TSV, encoding='utf-8', newline='') as Arq:
        for Linha in csv.DictReader(Arq, delimiter='\t'):
            if Linha['categoria'] == 'proc':
                Mapa[(Linha['pacote'], Linha['nome_forja'])] = (Linha['arquivo'], Linha['nome_c'], int(Linha['linha'] or 0))
    return Mapa


def CorpoNeutroC(Item, InfoC):
    # texto do corpo `{ ... }` que substitui o da funcao C; panic = fputs + exit(101), neutro por categoria do retorno; corte = o texto do corte BIR0
    if Item['modo'] == 'corte':
        return Item['texto_c']
    if Item['modo'] == 'panic':
        return '{ fputs("G3 stub: ' + Item['id'] + '\\n", stderr); exit(' + str(CODIGO_PANIC) + '); }'
    Categoria, Retorno = InfoC['categoria'], InfoC['retorno']
    if Categoria == 'void':
        return '{ }'
    if Categoria == 'struct':
        return '{ ' + Retorno + ' g3_r; __builtin_memset(&g3_r, 0, sizeof g3_r); return g3_r; }'
    if Item['tipo'] == 'texto':
        return '{ return (' + Retorno + ')""; }'
    return '{ return (' + Retorno + ')0; }'


def DecidirItem(Item, Nomes, InfoPorArquivo):
    # Preenche Item['modo'] (neutro | panic | sem_c | sem_corpo), Item['c'] (arquivo, nome, info) e Item['motivo']. Regra: nunca neutro so de um lado.
    Chave = (Item['pacote'], Item['nome'])
    if Chave not in Nomes:
        return {**Item, 'modo': 'sem_c', 'c': None, 'motivo': 'sem funcao C em nomes_t1.tsv (ex.: chamar_* por ponteiro de funcao)'}
    Arq, NomeC, _ = Nomes[Chave]
    Info = InfoPorArquivo.get(str(COMPILADOR / Arq), {}).get(NomeC)
    if Info is None:
        return {**Item, 'modo': 'sem_corpo', 'c': None, 'motivo': f'{Arq}:{NomeC} sem corpo editavel (macro ou fora do arquivo)'}
    Pronto = {**Item, 'c': {'arquivo': Arq, 'nome': NomeC, **Info}}
    if Item['tipo'] in ('nada', 'desconhecido'):
        return {**Pronto, 'modo': 'panic', 'motivo': 'retorno ^T ou forma nao reconhecida: panic nos dois lados'}
    if Info['categoria'] not in COMPATIVEL[Item['tipo']]:
        return {**Pronto, 'modo': 'panic', 'motivo': f"neutro Forja '{Item['tipo']}' x retorno C {Info['categoria']} ({Info['retorno']})"}
    return {**Pronto, 'modo': 'neutro', 'motivo': ''}


def PrefixoDeIncludes(Corpos):
    return '#include <stdio.h>\n#include <stdlib.h>\n' if any('fputs' in C for C in Corpos) else ''


def AplicarNeutrosC(Itens, CompiladorCopia):
    # Edita os .c/.h da copia do compiler: corpo de cada item pelo neutro C; offsets de baixo para cima por arquivo.
    PorArquivo = {}
    for Item in Itens:
        if Item['c'] and Item['modo'] in ('neutro', 'panic', 'corte'):
            PorArquivo.setdefault(Item['c']['arquivo'], []).append(Item)
    for Arq, Lista in PorArquivo.items():
        Caminho = CompiladorCopia / Arq
        Dados = bytearray(Caminho.read_bytes())
        Corpos = []
        for Item in sorted(Lista, key=lambda X: -X['c']['ini']):
            Corpo = CorpoNeutroC(Item, Item['c'])
            Corpos.append(Corpo)
            Dados[Item['c']['ini']:Item['c']['fim']] = Corpo.encode('utf-8')
        Caminho.write_bytes(PrefixoDeIncludes(Corpos).encode('ascii') + bytes(Dados))
    return sum(len(L) for L in PorArquivo.values())


def ItemDoCorte(ArvoreE1, Pasta, Info):
    # item do corte BIR0: corpo de nativo.nativo_construir_exe na copia da Forja e bloco de NativoConstruirExe na copia do C (offsets da Info do libclang)
    Arq, Cab, Fim, _ = LocalizarProcForja(ArvoreE1, 'nativo', 'nativo_construir_exe')
    InfoC = Info[str(COMPILADOR / ARQ_C_CORTE)][NOME_C_CORTE]
    Fj = LerCorteFj(Pasta / 'corte_e1.fj')
    return {'id': ID_CORTE, 'pacote': 'nativo', 'nome': 'nativo_construir_exe', 'modo': 'corte', 'tipo': 'corte', 'arquivo': Arq, 'linha': Cab, 'fim': Fim,
            'real': True, 'retorno': None, 'c': {'arquivo': ARQ_C_CORTE.as_posix(), 'nome': NOME_C_CORTE, **InfoC}, 'motivo': 'corte BIR0 (--corte)',
            'linhas_fj': Fj['corpo'], 'imports_fj': Fj['imports'], 'texto_c': LerCorteC(Pasta / 'corte_e0.c')}


def ShaDoCorte(Pasta):
    return {'c': hashlib.sha256((Pasta / 'corte_e0.c').read_bytes()).hexdigest(), 'fj': hashlib.sha256((Pasta / 'corte_e1.fj').read_bytes()).hexdigest()}


# ---------------------------------------------------------------- build
def AjustarBat(Compilador, Extras):
    # copia do construir.bat com flags a mais SO na linha `set FLAGS=` (o runtime, FLAGS_RT, nao muda)
    Bat = Compilador / 'construir.bat'
    Linhas = Bat.read_text(encoding='utf-8').split('\n')
    for I, Linha in enumerate(Linhas):
        if Linha.startswith('set FLAGS='):
            Linhas[I] = Linha.replace('-Wextra -Werror', '-Wextra -Werror ' + Extras, 1)
            Bat.write_text('\n'.join(Linhas), encoding='utf-8', newline='')
            return
    raise RuntimeError('construir.bat sem a linha set FLAGS=')


def LigarHarness(Cr, Cache, Plano, Objetos, Compilador):
    # autoteste: o mesmo COMUM + fadec do forja.exe, sem o driver\, com o main de harness_g3.c (copiado para a pasta da copia)
    Lig = Plano.Ligacoes[0]
    Flags = tuple(Cr.FlagsDeCompilacao(Lig.Flags))
    Objetos[('harness_g3.c', Flags)] = Cr.CompilarPasso(Cache, Cr.Passo([*Flags, '-c', 'harness_g3.c'], 'harness_g3.c'))
    Fontes = [F for F in Lig.Fontes if not F.startswith('driver\\')] + ['harness_g3.c']
    Pasta = Path(tempfile.mkdtemp(prefix='espelho_rsp_'))
    try:
        Cr.Ligar(Cr.Ligacao('harness_g3.exe', Lig.Flags, Fontes), Objetos, Compilador, Pasta / 'h.rsp')
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Compilador / 'harness_g3.exe'


def ConstruirE0(Compilador, Jobs, CacheObj, Harness=False):
    # forja.exe da copia (so ele, sem teste_nativo/teste_common) pelo construir_rapido; devolve (exe, (compilados, reaproveitados), exe do harness ou None)
    sys.path.insert(0, str(COMPILADOR))
    import construir_rapido as Cr
    Cr.COMPILADOR, Cr.BAT, Cr.RSP_BAT = Compilador, Compilador / 'construir.bat', Compilador / 'comum_fontes.rsp'
    Plano = Cr.LerPlano(Compilador)
    Plano.Ligacoes = [L for L in Plano.Ligacoes if L.Exe == 'forja.exe']
    Cache = Cr.CacheObjetos(CacheObj, True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Cr.EntregarRuntime(Cache, Pool, Plano, Compilador)
        Passos = Cr.PassosDasLigacoes(Plano)
        Objetos = dict(zip(Passos, Pool.map(lambda P: Cr.CompilarPasso(Cache, P), Passos.values())))
    Pasta = Path(tempfile.mkdtemp(prefix='espelho_rsp_'))
    try:
        Cr.Ligar(Plano.Ligacoes[0], Objetos, Compilador, Pasta / 'forja.rsp')
        ExeHarness = LigarHarness(Cr, Cache, Plano, Objetos, Compilador) if Harness else None
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
        for Provisorio in Cache.Provisorios:
            Provisorio.unlink(missing_ok=True)
    return Compilador / 'forja.exe', (Cache.Compilados, Cache.Reaproveitados), ExeHarness


def RodarForja(Argv, Cwd):
    Cp = subprocess.run([str(FORJA_REAL), *Argv], cwd=Cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return Cp.returncode, (Cp.stdout + Cp.stderr).strip()


def ConstruirE1(ArvoreE1, Saida, Dev, NomeRaiz='forja.fj', Prefixo='neutro'):
    # check da raiz (A1) e build --release (+ dev) do forja_fj da copia, com o forja.exe REAL (so le e compila; nada dele muda)
    Raiz = str(ArvoreE1 / 'forja_fj' / NomeRaiz)
    Codigo, Texto = RodarForja(['check', Raiz], ArvoreE1)
    if Codigo != 0:
        raise RuntimeError('check do forja_fj neutro falhou (o neutro quebrou a arvore):\n' + Texto[:1500])
    Exes = {}
    for Nome, Extra in [('release', ['--release'])] + ([('dev', [])] if Dev else []):
        Exe = Saida / f'e1_{Prefixo}_{Nome}.exe'
        Codigo, Texto = RodarForja(['build', *Extra, Raiz, '-o', str(Exe)], ArvoreE1)
        if Codigo != 0:
            raise RuntimeError(f'build {Nome} do forja_fj neutro falhou:\n' + Texto[:1500])
        Exes[Nome] = Exe
    return Exes


# ---------------------------------------------------------------- orquestracao
def FuncoesDoLote(ArquivosLote, Nomes):
    # funcoes C dos .fj do lote (mesmo caminho relativo, .fj -> .c): base da cobertura A4
    Cs = {Rel[:-3] + '.c' for Rel in ArquivosLote}
    return sorted((C, NomeC, Pacote, NomeF) for (Pacote, NomeF), (C, NomeC, _) in Nomes.items() if C in Cs)


def GravarPlano(Saida, Itens, Lote, Nomes):
    with open(Saida / 'plano.tsv', 'w', encoding='utf-8', newline='\n') as Arq:
        Arq.write('id\tmodo\ttipo_neutro\tarquivo_c\tnome_c\tmotivo\n')
        for Item in sorted(Itens, key=lambda X: X['id']):
            C = Item['c'] or {}
            Arq.write(f"{Item['id']}\t{Item['modo']}\t{Item['tipo']}\t{C.get('arquivo', '')}\t{C.get('nome', '')}\t{Item['motivo']}\n")
    with open(Saida / 'lote_funcoes.tsv', 'w', encoding='utf-8', newline='\n') as Arq:
        Arq.write('arquivo_c\tnome_c\tpacote\tnome_forja\n')
        for Linha in FuncoesDoLote(Lote, Nomes):
            Arq.write('\t'.join(Linha) + '\n')


def MontarArvores(Saida, Base, Lote):
    ApagarPasta(Saida / 'e1')
    ApagarPasta(Saida / 'e0')
    CopiarForjaFj(Base, Saida / 'e1')
    Sobrepostos = SobreporLote(Saida / 'e1', Lote)
    CopiarCompilador(Saida / 'e0' / 'language')
    return Sobrepostos


def Montar(Saida, Base, Lote, Opcoes):
    """Monta o par. Opcoes: jobs, dev, cobertura, funcoes (ids de funcoes reais a neutralizar), lado ('ambos'|'c'|'forja'), vazio (conjunto vazio). Devolve dict com caminhos, contagens e tempos."""
    Tempos, Inicio = {}, time.monotonic()
    Saida = Path(Saida).resolve()
    Saida.mkdir(parents=True, exist_ok=True)
    Sobrepostos = MontarArvores(Saida, Base, Lote)
    ArvoreE1, CompiladorCopia = Saida / 'e1', Saida / 'e0' / 'language' / 'compiler'
    if Opcoes.get('harness'):  # autoteste: {'c': texto do main C, 'fj': texto da raiz Forja}
        (CompiladorCopia / 'harness_g3.c').write_text(Opcoes['harness']['c'], encoding='ascii', newline='\n')
        (ArvoreE1 / 'forja_fj' / 'harness_g3.fj').write_text(Opcoes['harness']['fj'], encoding='ascii', newline='\n')
    Tempos['copias'] = time.monotonic() - Inicio
    Nomes = LerNomes()
    Corte = Path(Opcoes['corte']).resolve() if Opcoes.get('corte') else None
    Stubs = [] if Opcoes.get('vazio') else ColherStubsForja(ArvoreE1)
    if Corte:
        Stubs = [S for S in Stubs if S['id'] != ID_CORTE]  # a rotina do corte sai do conjunto neutro: o corpo vem do --corte nos 2 lados
    Stubs += [ItemDeFuncaoReal(ArvoreE1, Id) for Id in Opcoes.get('funcoes', [])]
    ArquivosC = {str(COMPILADOR / Nomes[(S['pacote'], S['nome'])][0]) for S in Stubs if (S['pacote'], S['nome']) in Nomes}
    if Corte:
        ArquivosC.add(str(COMPILADOR / ARQ_C_CORTE))
    Info = ParsearTodos(sorted(ArquivosC), Opcoes['jobs'])
    Itens = [DecidirItem(S, Nomes, Info) for S in Stubs]
    if Corte:
        for Item in Itens:
            if Item['pacote'] in PACOTES_C_REAL_COM_CORTE:
                Item['c'] = None  # sem edicao no C: o OtimExecutar e os passes do E0 rodam de verdade (neutro so no Forja = o E1 ainda nao portou)
    if Corte:
        Itens.append(ItemDoCorte(ArvoreE1, Corte, Info))
    Tempos['plano'] = time.monotonic() - Inicio - Tempos['copias']
    Lado = Opcoes.get('lado', 'ambos')
    NForja = AplicarNeutrosForja(Itens) if Lado in ('ambos', 'forja') else 0
    NC = AplicarNeutrosC([I for I in Itens if I['modo'] in ('neutro', 'panic', 'corte')], CompiladorCopia) if Lado in ('ambos', 'c') else 0
    GravarPlano(Saida, Itens, Sobrepostos, Nomes)
    Tempos['edicoes'] = time.monotonic() - Inicio - Tempos['copias'] - Tempos['plano']
    Resultado = {'saida': Saida, 'itens': Itens, 'edicoes_forja': NForja, 'edicoes_c': NC, 'lote': Sobrepostos, 'corte': ShaDoCorte(Corte) if Corte else None}
    return ConstruirPar(Resultado, Opcoes, Tempos, Inicio)


def ConstruirPar(Resultado, Opcoes, Tempos, Inicio):
    Saida, Jobs = Resultado['saida'], Opcoes['jobs']
    CompiladorCopia = Saida / 'e0' / 'language' / 'compiler'
    Marca = time.monotonic()
    AjustarBat(CompiladorCopia, FLAGS_ESPELHO)
    Harness = Opcoes.get('harness')
    Prefixo = 'corte' if Resultado['corte'] else 'neutro'
    NomeE0 = 'e0_corte.exe' if Resultado['corte'] else 'e0_espelho.exe'
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as Pool:
        FuturoE0 = Pool.submit(ConstruirE0, CompiladorCopia, Jobs, CACHE_RAIZ / 'obj', bool(Harness))
        FuturoE1 = None if Opcoes.get('sem_e1') else Pool.submit(ConstruirE1, Saida / 'e1', Saida, Opcoes.get('dev', True), 'harness_g3.fj' if Harness else 'forja.fj', Prefixo)
        Exe0, Contagem, ExeHarness = FuturoE0.result()
        Resultado['e1'] = FuturoE1.result() if FuturoE1 else {}
    shutil.copy2(Exe0, Saida / NomeE0)
    Resultado.update({'e0': Saida / NomeE0, 'objetos_e0': Contagem, 'e0_harness': ExeHarness})
    Tempos['build'] = time.monotonic() - Marca
    if Opcoes.get('cobertura'):
        Resultado['cobertura'] = ConstruirCobertura(Saida, Jobs)
    Tempos['total'] = time.monotonic() - Inicio
    Resultado['tempos'] = Tempos
    return Resultado


def ConstruirCobertura(Saida, Jobs):
    # variante --coverage: copia das fontes ja editadas (nada de exe/.o) com o .bat acrescido de --coverage; objetos em cache proprio
    Language = Saida / 'e0_cobertura' / 'language'
    ApagarPasta(Saida / 'e0_cobertura')
    Language.mkdir(parents=True)
    shutil.copytree(Saida / 'e0' / 'language' / 'compiler', Language / 'compiler', ignore=IGNORAR_COMPILADOR)
    FazerJuncoes(Language)
    AjustarBat(Language / 'compiler', '--coverage')
    Exe, _, _ = ConstruirE0(Language / 'compiler', Jobs, CACHE_RAIZ / 'obj_cobertura')
    return Exe


def ZerarCobertura():
    # apaga os .gcda do cache da variante --coverage (as contagens se somam entre execucoes do exe)
    Pasta = CACHE_RAIZ / 'obj_cobertura'
    Gcdas = list(Pasta.glob('*.gcda'))
    for Gcda in Gcdas:
        Gcda.unlink()
    return len(Gcdas)


def FuncoesDoGcda(Gcda):
    Cp = subprocess.run(['gcov', '--json-format', '--stdout', Gcda.name], cwd=Gcda.parent, capture_output=True, text=True, encoding='utf-8', errors='replace')
    Achados = []
    for Linha in Cp.stdout.splitlines():
        for Arq in json.loads(Linha)['files']:
            Achados += [(Arq['file'], F['name'], F['execution_count']) for F in Arq['functions']]
    return Achados


def RelatorioCobertura(Saida):
    # Entrada: pasta --saida. Saida: cobertura_funcoes.tsv (arquivo_c, funcao C, execucoes) somando todos os .gcda; rodar o exe de cobertura com --jobs 1 no paridade (execucoes simultaneas corrompem o .gcda)
    Total = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as Pool:
        for Lista in Pool.map(FuncoesDoGcda, sorted((CACHE_RAIZ / 'obj_cobertura').glob('*.gcda'))):
            for Arq, Nome, Vezes in Lista:
                Total[(Arq, Nome)] = Total.get((Arq, Nome), 0) + Vezes
    Caminho = Path(Saida) / 'cobertura_funcoes.tsv'
    Caminho.write_text('arquivo_c\tfuncao_c\texecucoes\n' + ''.join(f'{A}\t{N}\t{V}\n' for (A, N), V in sorted(Total.items())), encoding='utf-8', newline='\n')
    return Caminho, len(Total), sum(1 for V in Total.values() if V)


def Relatorio(Resultado):
    Itens = Resultado['itens']
    Contagem = {}
    for Item in Itens:
        Contagem[Item['modo']] = Contagem.get(Item['modo'], 0) + 1
    T = Resultado['tempos']
    Linhas = [f"espelho {VERSAO}: {len(Itens)} stubs no conjunto: " + ', '.join(f'{K}={V}' for K, V in sorted(Contagem.items())),
              f"edicoes: forja {Resultado['edicoes_forja']} (panic tirado), C {Resultado['edicoes_c']} (corpo trocado); lote: {len(Resultado['lote'])} arquivos .fj",
              f"E0-espelho: {Resultado['e0']}  (objetos compilados {Resultado['objetos_e0'][0]}, reaproveitados {Resultado['objetos_e0'][1]})"]
    if Resultado.get('corte'):
        Linhas.append(f"corte BIR0 sha256: corte_e0.c {Resultado['corte']['c']} | corte_e1.fj {Resultado['corte']['fj']}")
    Linhas += [f"E1-neutro {Nome}: {Exe}" for Nome, Exe in Resultado['e1'].items()]
    if Resultado.get('cobertura'):
        Linhas.append(f"E0-cobertura: {Resultado['cobertura']}")
    Linhas.append('tempos s: ' + ', '.join(f'{K} {V:.1f}' for K, V in T.items()))
    Linhas.append(f"plano: {Resultado['saida'] / 'plano.tsv'}  (modo panic/sem_c/sem_corpo = o candidato cai e vira `stub` no paridade, nunca diverge)")
    Texto = '\n'.join(Linhas)
    (Resultado['saida'] / 'relatorio.txt').write_text(Texto + '\n', encoding='utf-8')
    return Texto


# ---------------------------------------------------------------- autoteste (i: conjunto vazio; ii: mesmas funcoes nos 2 lados; iii: so 1 lado)
# O driver .fj ainda e stub no G3, entao (ii) e (iii) usam um harness: o MESMO main nas duas linguagens chama lexer_ler_inteiro_u64 (frontend, ja provado) com 4 literais;
# as 3 funcoes neutralizadas sao os helpers do lexer que ele usa (digito, digito hex, prefixo 0x/0b): neutras, o resultado muda e tem de mudar igual nos 2 lados.
HARNESS_C = '''#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "frontend/lexer.h"
static void Tenta(const char *t) { uint64_t v = 0; bool ok = LexerLerInteiroU64(t, &v); printf("%s\\n%llu\\n", ok ? "true" : "false", (unsigned long long)v); }
int main(void) { Tenta("123"); Tenta("0x1F"); Tenta("0b101"); Tenta("99999999999999999999"); return 0; }
'''
HARNESS_FJ = '''import "frontend"

proc tenta(t: str) {
    var v: u64 = 0
    let ok = frontend.lexer_ler_inteiro_u64(as_bytes(t), &v)
    print(ok)
    print(v)
}

proc main() {
    tenta("123")
    tenta("0x1F")
    tenta("0b101")
    tenta("99999999999999999999")
}
'''
FUNCOES_AUTOTESTE = ['frontend.eh_digito', 'frontend.eh_digito_hex', 'frontend.eh_prefixo_de_numero']
SAIDA_REAL_HARNESS = b'true\n123\ntrue\n31\ntrue\n5\nfalse\n0\n'


def SaidaDoHarness(Exe):
    Cp = subprocess.run([str(Exe)], capture_output=True, timeout=60, stdin=subprocess.DEVNULL)
    return Cp.returncode, Cp.stdout.replace(b'\r\n', b'\n')


def MontarAutoteste(Pasta, **Opcoes):
    return Montar(Pasta, RAIZ / 'forja_fj', [], {'jobs': os.cpu_count() or 4, 'dev': False, 'vazio': True, 'harness': {'c': HARNESS_C, 'fj': HARNESS_FJ}, **Opcoes})


def ParesDoHarness(R):
    return SaidaDoHarness(R['e0_harness']), SaidaDoHarness(R['e1']['release'])


def TesteConjuntoVazio(Pasta):
    # E0-espelho sem nenhuma troca = E0 real no corpus ast (amostra 200); controle: o harness C e o Forja dao a saida esperada
    Resultado = MontarAutoteste(Pasta / 'i')
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import paridade
    Rodada = paridade.RodarTudo({'oraculo': str(FORJA_REAL), 'forja': str(Resultado['e0']), 'fases': ['ast'], 'amostra': 200, 'jobs': os.cpu_count() or 4,
                                 'cache': str(CACHE_RAIZ / 'oraculo'), 'so': None, 'lista': None, 'timeout': paridade.TIMEOUT_S})
    Classes = [R['classe'] for R in Rodada['resultados']]
    (C, SC), (F, SF) = ParesDoHarness(Resultado)
    Erros = [f'{Classes.count(K)} {K}' for K in ('diverge', 'stub', 'cai') if Classes.count(K)]
    Erros += [] if (C, SC) == (0, SAIDA_REAL_HARNESS) and (F, SF) == (0, SAIDA_REAL_HARNESS) else [f'harness sem neutro: C {C} {SC!r}, Forja {F} {SF!r}']
    return Erros, f"{len(Classes)} comparacoes ast, {Classes.count('igual')} igual; harness C e Forja = saida esperada"


def TesteMesmasFuncoes(Pasta):
    # 3 funcoes proprias do lexer neutras nos 2 lados: a saida muda (difere da real) e muda igual (0 diverge)
    (C, SC), (F, SF) = ParesDoHarness(MontarAutoteste(Pasta / 'ii', funcoes=FUNCOES_AUTOTESTE))
    Erros = [] if (C, SC) == (F, SF) else [f'diverge: C {C} {SC!r} x Forja {F} {SF!r}']
    Erros += ['o neutro nao teve efeito (saida igual a real)'] if SC == SAIDA_REAL_HARNESS else []
    return Erros, f'saida neutra igual nos 2 lados ({SC.decode().split(chr(10))[:4]}...), diferente da real'


def TesteUmLado(Pasta):
    # so 1 lado neutro: tem de divergir, nos dois sentidos (sensibilidade)
    Erros, Mensagens = [], []
    for Lado in ('c', 'forja'):
        (C, SC), (F, SF) = ParesDoHarness(MontarAutoteste(Pasta / f'iii_{Lado}', funcoes=FUNCOES_AUTOTESTE, lado=Lado))
        Mensagens.append(f'{Lado}: ' + ('diverge' if SC != SF else 'IGUAL'))
        Erros += [] if SC != SF else [f'neutro so no lado {Lado} e a saida ficou igual']
    return Erros, ', '.join(Mensagens)


def RodarCorteNosDoisLados(Pasta, Rotulo, NomeCorte, Flags, Vazio):
    # monta o corte NomeCorte (pasta ao lado deste arquivo) nos 2 lados e roda 01_ola.fj com `build <Flags> fonte -o <exe>`: devolve {'C': (rc, stdout), 'Forja': (rc, stdout)}
    # Vazio=True: conjunto neutro vazio (so o corte tem corpo); Vazio=False: stubs neutros normais (o E1 do otimizador cai em panic se vazio)
    Corte = str(Path(__file__).resolve().parent / NomeCorte)
    Resultado = Montar(Pasta / Rotulo, RAIZ / 'forja_fj', [], {'jobs': os.cpu_count() or 4, 'dev': False, 'vazio': Vazio, 'corte': Corte})
    Fonte = RAIZ.parent / 'examples' / '01_ola.fj'
    Lados = {}
    for Nome, Exe in (('C', Resultado['e0']), ('Forja', Resultado['e1']['release'])):
        Cp = subprocess.run([str(Exe), 'build', *Flags, str(Fonte), '-o', str(Pasta / f'{Rotulo}_{Nome}.exe')], capture_output=True, timeout=120, stdin=subprocess.DEVNULL)
        Lados[Nome] = (Cp.returncode, Cp.stdout)
    return Lados


def TesteCorteDe(Pasta, Rotulo, NomeCorte, Flags, Igual, Vazio=True, Marca=None):
    # corte nos 2 lados. Igual=True: rc 0 e stdout igual nos 2 lados (iv). Igual=False: so stdout nao vazio nos 2 (v: o E1 ainda tem o otim_executar neutro).
    # Marca: texto que o stdout do C TEM de trazer (prova que o corte rodou o codigo real, nao o neutro).
    try:
        Lados = RodarCorteNosDoisLados(Pasta, Rotulo, NomeCorte, Flags, Vazio)
    except (ValueError, RuntimeError) as Erro:
        return [f'montagem do corte {NomeCorte} falhou: {Erro}'], ''
    (RcC, SaidaC), (RcF, SaidaF) = Lados['C'], Lados['Forja']
    Erros = []
    if Igual:
        Erros += [] if RcC == 0 and RcF == 0 else [f'rc C {RcC} x Forja {RcF}']
        Erros += [] if SaidaC == SaidaF else [f'stdout diverge: C {len(SaidaC)} bytes x Forja {len(SaidaF)} bytes']
    Erros += [] if SaidaC and SaidaF else ['stdout vazio em algum lado']
    if Marca is not None and Marca.encode('ascii') not in SaidaC:
        Erros.append(f'C sem o relato {Marca!r}: o corte nao rodou o otimizador de verdade')
    return Erros, f'01_ola ({NomeCorte}): rc {RcC}/{RcF}, stdout {len(SaidaC)}/{len(SaidaF)} bytes'


def TesteCorte(Pasta):
    # (iv) corte BIR0 nos 2 lados, sem nenhum neutro (forja_fj real inteiro): 01_ola.fj tem rc 0 e o mesmo stdout nos 2 lados, nao vazio
    return TesteCorteDe(Pasta, 'iv', 'corte_bir0', [], True)


def TesteCorteOtim0(Pasta):
    # (v) corte OTIM0 em release: E0 com OtimExecutar e passes reais (relato 'otim: ' no stdout); E1 com o otim_executar neutro. Nos 2 lados stdout nao vazio.
    return TesteCorteDe(Pasta, 'v', 'corte_otim0', ['--release'], False, Vazio=False, Marca='otim: ')


def RodarAutoteste():
    Pasta = Path(tempfile.mkdtemp(prefix='espelho_autoteste_'))
    Testes = [('(i) conjunto vazio', TesteConjuntoVazio), ('(ii) mesmas funcoes nos 2 lados', TesteMesmasFuncoes), ('(iii) so 1 lado (sensibilidade)', TesteUmLado),
              ('(iv) corte BIR0 nos 2 lados (01_ola)', TesteCorte),
              ('(v) corte OTIM0 nos 2 lados, release (01_ola)', TesteCorteOtim0)]
    Falhas = 0
    try:
        for Nome, Teste in Testes:
            Marca = time.monotonic()
            Erros, Msg = Teste(Pasta)
            Falhas += bool(Erros)
            print(f"{'FALHOU' if Erros else 'OK'} {Nome}: " + ('; '.join(Erros) if Erros else Msg) + f' [{time.monotonic() - Marca:.1f} s]')
    finally:
        ApagarPasta(Pasta)
    print(f"autoteste: {'OK' if not Falhas else 'FALHOU'} ({len(Testes) - Falhas}/{len(Testes)})")
    return 1 if Falhas else 0


def ConstruirParser():
    P = argparse.ArgumentParser(description='Espelho neutro do G3: E1-neutro (forja_fj) x E0-espelho (compiler) com as mesmas funcoes neutras.', add_help=False)
    P.add_argument('-h', '--ajuda', '--help', action='help', help='esta ajuda')
    P.add_argument('--lote', help='arquivo com a lista de .fj do lote (um por linha) ou - para stdin')
    P.add_argument('--saida', help='pasta scratch (apagada e refeita em e0\\ e e1\\)')
    P.add_argument('--real-de', dest='real_de', default=str(RAIZ / 'forja_fj'), help='forja_fj base cujos stubs valem (padrao: o real)')
    P.add_argument('--cobertura', action='store_true', help='tambem constroi a variante --coverage do E0-espelho')
    P.add_argument('--sem-dev', dest='sem_dev', action='store_true', help='nao compila o E1 dev')
    P.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    P.add_argument('--cobertura-zerar', dest='cobertura_zerar', action='store_true', help='apaga os .gcda antes de medir')
    P.add_argument('--cobertura-relatorio', dest='cobertura_relatorio', action='store_true', help='le os .gcda e grava <saida>\\cobertura_funcoes.tsv (sem montar)')
    P.add_argument('--corte', help='pasta com corte_e0.c (bloco de NativoConstruirExe) e corte_e1.fj (corpo de nativo_construir_exe): corte BIR0 nos 2 lados')
    P.add_argument('--autoteste', action='store_true')
    return P


def Principal(Argv):
    Args = ConstruirParser().parse_args(Argv)
    if Args.autoteste:
        return RodarAutoteste()
    if Args.cobertura_zerar:
        print(f'cobertura: {ZerarCobertura()} .gcda apagados')
        return 0
    if not Args.saida:
        print('uso: --saida <pasta scratch> obrigatorio (veja --ajuda)')
        return 2
    if Args.cobertura_relatorio:
        Caminho, Funcoes, Executadas = RelatorioCobertura(Args.saida)
        print(f'cobertura: {Executadas} de {Funcoes} funcoes executadas -> {Caminho}')
        return 0
    Texto = sys.stdin.read() if Args.lote == '-' else Path(Args.lote).read_text(encoding='utf-8') if Args.lote else ''
    try:
        Resultado = Montar(Args.saida, Path(Args.real_de), LerListaLote(Texto),
                           {'jobs': Args.jobs, 'dev': not Args.sem_dev, 'cobertura': Args.cobertura, 'corte': Args.corte})
    except (ValueError, RuntimeError) as Erro:
        print('espelho: FALHOU\n' + str(Erro))
        return 1
    print(Relatorio(Resultado))
    return 0


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    sys.exit(Principal(sys.argv[1:]))
