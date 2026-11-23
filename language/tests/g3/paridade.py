# paridade.py - harness do G3 (O1): compara o forja de referencia (E0, em C) com o forja candidato (o futuro forja em Forja), insumo por insumo e modo por modo.
# Objetivo: provar, fase a fase, que o candidato devolve a mesma saida que o oraculo (stdout, stderr, codigo de saida e arquivos gerados por sha256); classe por (insumo, modo): igual, diverge (1o ponto) ou stub (ainda nao portado).
# Uso: python -I tests\g3\paridade.py [--forja <exe>] [--oraculo <exe>] [--fase ast|tipos|check|ir|bir|bir0|otim0|asm|build|cli|tudo] [--so <trecho>] [--lista <arquivo>] [--amostra N] [--timeout S] [--jobs N] [--cache <pasta>] [--min-nao-vazio N] [--sem-otim LISTA] [--sem-stub] [--stubs] [--json] | --selftest | --versao
# Saida: uma linha por divergencia, tabela por fase e RESULTADO; exit 0 = 0 diverge (e 0 stub com --sem-stub); exit 1 = ha diverge; exit 2 = erro de uso.
#   --stubs: ranking dos "G3 stub: <pacote>.<proc>" (bloqueios por nome, fase mais frequente); top 40 na tela, completo em %TEMP%\forja_paridade\stubs.tsv e no JSON (ranking_stubs).
#   classe `cai` (1.2.0): os dois lados com saida anormal (C: codigo >= 0xC0000000 ou 101; Forja: 101 ou R-code de runtime) = fora da conta, contada a parte (coluna cai, "N cai" no RESULTADO, `cai` no JSON). Serve ao espelho neutro (tests\g3\espelho.py): os dois lados caem no mesmo stub.
# Depende de: so a biblioteca padrao e tests\nativo\otim\mascara_versao.py (sha256 de exe/ELF/PDB sem os bytes de versao do forja).
# Versao: 1.7.1
# Changelog:
#   1.7.1 (19/11/2026) dbgcode: o registro {"tipo": "tempos"...} do .debug.jsonl (relogio) e mascarado nos arquivos de texto; NAO-VAZIO da fase dbgcode conta arquivo de texto com conteudo (o dump vai para x.exe.debug.*, o stdout fica vazio). VERSAO sobe (cache do oraculo recalculado). selftest 9/9.
#   1.7.0 (19/11/2026) G3 V4 (D0788): fase `dbgcode` com 8 modos `build --debug-code <item> {f} -o {s}` (formato, reloc, quadros, linhas, locais, interface, perfil, json; sem tempos, que e relogio); NAO-VAZIO e --min-nao-vazio valem tambem nela. A fase `build` (sha256 de exe/dll/ELF/PDB com mascara de versao+GUID) ja existia e e a prova final do obj/fmt/dbg contra o E0 real. Reversao: tirar FASE_DBGCODE, 'dbgcode' de FASES e MODOS e voltar VERSAO a 1.6.1 (sem .bak: so 4 trechos). VERSAO sobe: o cache do oraculo e recalculado.
#   1.6.1 (19/11/2026) ArgsComSemOtim: --sem-otim so entra em comando com --release (o dev recusa a flag: rc 2 e saida vazia nos 2 lados). VERSAO sobe: o cache do oraculo e recalculado.
#   1.6.0 (19/11/2026) G3 X64_0 (V3): fase `x64_0` com 5 modos (x64-release, x64-perfil, x64-dev, x64-linux, x64-dll); NAO-VAZIO e --sem-otim valem tambem nela. Reversao: copiar paridade.py.bak-1.5.0. VERSAO sobe: o cache do oraculo e recalculado.
#   1.5.0 (19/11/2026) G3 OTIM0 (V2): fase `otim0` com modos otim0-release (`build --release {f} -o {s}`) e otim0-perfil (`build --release --perfil {f} -o {s}`, so em amostra); sem modo dev (o otimizador nao roda passe em dev); NAO-VAZIO passa a valer para bir0 e otim0 (FASES_COM_NAO_VAZIO), e --min-nao-vazio vale para as duas; flag --sem-otim LISTA entra nos ARGS do comando (antes do insumo), entao entra na chave do cache do oraculo; selftest (i): otim0 com oraculo falso de saida fixa (o forja real nao imprime nada em build --release, entao NAO-VAZIO dele seria 0): --sem-otim no argv, NAO-VAZIO>0 e determinismo, forja que descarta a saida diverge. VERSAO sobe: o cache do oraculo e recalculado na 1a rodada.
#   1.0.0 (19/11/2026) G3 O1: criacao. Fases ast, tipos, check, bir, asm, build (dev, release, dll, linux dev/release) e cli; ir = modo nao suportado pelo oraculo (X0003); cache do oraculo por hash; selftest com E0 x E0 e sensibilidade com forja falso.
#   1.1.0 (19/11/2026) --stubs: ranking dos G3 stub por nome, com bloqueios e fase mais frequente (tela 40, stubs.tsv e JSON); sem --stubs a saida fica igual; selftest (e) do parse e do ranking. VERSAO sobe: o cache do oraculo e recalculado na 1a rodada.
# Verificacao: python -I tests\g3\paridade.py --selftest ; python -I tests\g3\paridade.py --amostra 200 --fase ast,tipos,check ; ... --amostra 20 --fase ast --stubs
#   1.2.0 (19/11/2026) G3 espelho neutro: classe `cai` (os 2 lados com saida anormal; fora da conta, contada a parte); "G3 stub:" no candidato com o oraculo tambem caido = cai (com o oraculo normal segue stub); timeout dos dois lados = igual e de 1 lado = diverge (como antes, e agora na frente de cai); selftest (f) cai. VERSAO sobe: o cache do oraculo e recalculado na 1a rodada.
#   1.3.0 (19/11/2026) --timeout S (inteiro, padrao 120 = TIMEOUT_S; vale para o oraculo e a forja; entra na chave do cache do oraculo, entao timeout diferente nao reaproveita resultado) e --lista <arquivo> (um caminho .fj por linha, UTF-8; linhas vazias e `#` ignoradas; roda so esses insumos em vez da varredura do corpus; inexistente = aviso em stderr e conta no RESULTADO/JSON; arquivo da lista inexistente = exit 2). Por que: rodar um subconjunto de insumos e testar timeout sem varrer o corpus inteiro. selftest (f) timeout na chave do cache e (g) --lista restringe (7/7). VERSAO sobe: o cache do oraculo e recalculado na 1a rodada.
#   1.4.0 (19/11/2026) G3 BIR0 (decisao D0778): fase `bir0` com 3 modos (bir0-dev `build {f} -o {s}`, bir0-release `build --release {f} -o {s}`, bir0-test `test {f}`), comparando stdout, stderr e rc; saida `x.exe` nos dois build (como exe-dev), para o -o ter caminho; NAO-VAZIO por modo (classe igual, rc 0 e stdout com texto nos 2 lados) na tabela e no JSON, so na fase bir0; flag --min-nao-vazio N (modo bir0 abaixo de N = exit 1; sem bir0 em --fase = exit 2); selftest (h): E0 x E0 com NAO-VAZIO > 0 e forja falso que descarta a saida da diverge. Outras fases sem mudanca. VERSAO sobe: o cache do oraculo e recalculado na 1a rodada.
# Reversao: apagar tests\g3\paridade.py (ferramenta de leitura: executa os dois forjas; grava so em %TEMP%, nunca no insumo nem em compiler\). Voltar a 1.3.0 (inclui reverter 1.4.0): copiar tests\g3\paridade.py.bak-1.3.0 por cima de tests\g3\paridade.py.
import argparse
import collections
import functools
import hashlib
import itertools
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

RAIZ_LANGUAGE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(RAIZ_LANGUAGE, 'tests', 'nativo', 'otim'))
import mascara_versao  # precisa do sys.path acima (python -I nao poe a pasta do script no caminho)

VERSAO = '1.7.1'
FORJA_PADRAO = os.path.join(RAIZ_LANGUAGE, 'compiler', 'forja.exe')
RAIZ_ENGINE = os.path.normpath(os.path.join(RAIZ_LANGUAGE, '..'))
PASTAS_INSUMOS = (os.path.join(RAIZ_LANGUAGE, 'tests'), os.path.join(RAIZ_LANGUAGE, 'std'),
                  os.path.join(RAIZ_LANGUAGE, 'exemplos_libs'), os.path.join(RAIZ_ENGINE, 'examples'))
PASTAS_PULADAS = ('__pycache__', 'saida', 'saidas')
CACHE_PADRAO = os.path.join(tempfile.gettempdir(), 'forja_paridade_cache')
TIMEOUT_S = 120
TAMANHO_TRECHO = 160
STUB_TEXTO = 'G3 stub:'
REGEX_STUB = re.compile(r'G3 stub: *([^\s",\\]+)')
REGEX_RCODE = re.compile(r'"codigo": *"R\d+"')
REGEX_TEMPOS_JSON = re.compile(r'^\{"tipo": "tempos".*$', re.M)  # registro de relogio do --debug-code json: os numeros mudam a cada corrida
SAIDA_PANIC = 101
LIMITE_CODIGO_NT = 0xC0000000  # excecao do Windows (0xC0000005 etc.): o processo caiu
LIMITE_RANKING_TELA = 40
PASTA_SAIDA = os.path.join(tempfile.gettempdir(), 'forja_paridade')
EXTENSOES_TEXTO = ('.txt', '.jsonl', '.json')
FASES = ('ast', 'tipos', 'check', 'ir', 'bir', 'bir0', 'otim0', 'x64_0', 'asm', 'dbgcode', 'build', 'cli')
FASE_BIR0 = 'bir0'  # fase do corte (D0778): NAO-VAZIO e --min-nao-vazio
FASE_OTIM0 = 'otim0'  # fase do corte OTIM0 (V2): release com passes; sem dev (o dev nao roda passe)
FASE_X640 = 'x64_0'  # fase do corte X64 (V3): dump do Objeto apos o X64Gerar (Windows/linux, release/perfil/dev/dll)
FASE_DBGCODE = 'dbgcode'  # fase da V4 (D0788): --debug-code por camada (formato = secoes/imports/exports; separa defeito do obj/ligacao do fmt/bytes)
FASES_COM_NAO_VAZIO = (FASE_BIR0, FASE_OTIM0, FASE_X640, FASE_DBGCODE)
FASES_COM_SEM_OTIM = (FASE_OTIM0, FASE_X640)
FASES_NAO_SUPORTADAS = {
    'ir': 'build --debug-code ir = X0003 (itens validos: bir, asm, quadros, reloc, formato, tempos, json, locais, linhas, otim, interface, perfil, passes)',
}
# Modo de build/dump: (nome, argv com {f} = insumo e {s} = arquivo de saida, nome do arquivo de saida dentro da pasta do modo).
MODOS = {
    'ast': [('ast', ['ast', '{f}'], None)],
    'tipos': [('tipos', ['ast', '--tipos', '{f}'], None)],
    'check': [('check-canonico', ['check', '--canonico', '{f}'], None),
              ('check-resumo', ['check', '--resumo', '{f}'], None)],
    'bir': [('bir', ['build', '--debug-code', 'bir', '{f}', '-o', '{s}'], 'x.exe')],
    # saida 'x.exe' nos build (como exe-dev): {s} precisa de caminho; o corte nao grava exe, so o stdout conta
    'bir0': [('bir0-dev', ['build', '{f}', '-o', '{s}'], 'x.exe'),
             ('bir0-release', ['build', '--release', '{f}', '-o', '{s}'], 'x.exe'),
             ('bir0-test', ['test', '{f}'], None)],
    # otim0: --sem-otim (se houver) entra antes do {f}, pela MontarTarefas; o perfil so em amostra (a rodada e que escolhe)
    'otim0': [('otim0-release', ['build', '--release', '{f}', '-o', '{s}'], 'x.exe'),
              ('otim0-perfil', ['build', '--release', '--perfil', '{f}', '-o', '{s}'], 'x.exe')],
    # x64_0 (V3): o corte imprime o dump do Objeto apos o X64Gerar; --sem-otim sobe a escada (alocador,isel,divconst)
    'x64_0': [('x64-release', ['build', '--release', '{f}', '-o', '{s}'], 'x.exe'),
              ('x64-perfil', ['build', '--release', '--perfil', '{f}', '-o', '{s}'], 'x.exe'),
              ('x64-dev', ['build', '{f}', '-o', '{s}'], 'x.exe'),
              ('x64-linux', ['build', '--release', '--target', 'x86_64-linux', '{f}', '-o', '{s}'], 'x'),
              ('x64-dll', ['build', '--release', '--modo', 'dll', '{f}', '-o', '{s}'], 'x.dll')],
    'asm': [('asm', ['build', '--debug-code', 'asm', '{f}', '-o', '{s}'], 'x.exe')],
    # dbgcode (V4): um modo por item do --debug-code que nao depende de relogio (tempos fica de fora); stdout e rc comparados
    'dbgcode': [('dbg-' + Item, ['build', '--debug-code', Item, '{f}', '-o', '{s}'], 'x.exe')
                for Item in ('formato', 'reloc', 'quadros', 'linhas', 'locais', 'interface', 'perfil', 'json')],
    'build': [('exe-dev', ['build', '{f}', '-o', '{s}'], 'x.exe'),
              ('exe-release', ['build', '--release', '{f}', '-o', '{s}'], 'x.exe'),
              ('dll-dev', ['build', '--modo', 'dll', '{f}', '-o', '{s}'], 'x.dll'),
              ('linux-dev', ['build', '--target', 'x86_64-linux', '{f}', '-o', '{s}'], 'x'),
              ('linux-release', ['build', '--target', 'x86_64-linux', '--release', '{f}', '-o', '{s}'], 'x')],
}
CLI_POR_INSUMO = [('fmt-json', ['fmt', '{f}', '--json']),
                  ('mapa-json', ['mapa', '{f}', '--json']),
                  ('simbolos-json', ['consultar', 'simbolos', '{f}', '--json']),
                  ('imports-json', ['consultar', 'imports', '{f}', '--json']),
                  ('corrigir', ['corrigir', '{f}']),
                  ('migrar-dry-json', ['migrar', '{f}', '--dry-run', '--json'])]
CLI_GLOBAIS = [('versao', ['--versao']),
               ('explicar-lista-json', ['explicar', '--lista', '--json']),
               ('spec-json', ['spec', '--json']),
               ('spec-cartao', ['spec', '--cartao'])]


def ListarInsumos(Trecho):
    Achados = set()
    for Raiz in PASTAS_INSUMOS:
        for Dir, Subs, Nomes in os.walk(Raiz):
            Subs[:] = [Sub for Sub in Subs if Sub not in PASTAS_PULADAS]
            Achados.update(os.path.join(Dir, Nome).replace('\\', '/') for Nome in Nomes if Nome.endswith('.fj'))
    Lista = sorted(Achados)
    if Trecho:
        Lista = [Caminho for Caminho in Lista if Trecho.replace('\\', '/') in Caminho]
    return Lista


def AmostrarInsumos(Lista, Amostra):
    # passo fixo sobre a lista ordenada: N insumos espalhados e sempre os mesmos
    if Amostra is None or Amostra >= len(Lista):
        return Lista
    Passo = max(1, len(Lista) // Amostra)
    return Lista[::Passo][:Amostra]


def ShaDeArquivo(Caminho):
    with open(Caminho, 'rb') as Arq:
        return hashlib.sha256(Arq.read()).hexdigest()


def MapaDePastas(Insumos):
    # sha da pasta = nomes e conteudos dos .fj dela: um import de pacote (pasta inteira) muda o resultado do insumo
    Mapa = {}
    for Pasta in {os.path.dirname(Insumo) for Insumo in Insumos}:
        Partes = [Nome + ':' + ShaDeArquivo(os.path.join(Pasta, Nome)) for Nome in sorted(os.listdir(Pasta))
                  if Nome.endswith('.fj') and os.path.isfile(os.path.join(Pasta, Nome))]
        Mapa[Pasta] = hashlib.sha256('\n'.join(Partes).encode('utf-8')).hexdigest()
    return Mapa


def ArgsComSemOtim(Args, SemOtim):
    # --sem-otim entra nos ARGS, antes do insumo: assim vai para a chave do cache do oraculo (variavel de ambiente deixaria cache velho valendo)
    if SemOtim is None or '--release' not in Args:
        return Args  # o forja recusa --sem-otim sem --release (uso, rc 2 nos 2 lados): o modo dev nao roda passe, entao nao leva a flag
    Posicao = Args.index('{f}')
    return Args[:Posicao] + ['--sem-otim', SemOtim] + Args[Posicao:]


def MontarTarefas(Fase, Insumos, SemOtim=None):
    if Fase == 'cli':
        Globais = [{'fase': Fase, 'insumo': '<global>', 'modo': Nome, 'args': Args, 'saida': None} for Nome, Args in CLI_GLOBAIS]
        Por = [{'fase': Fase, 'insumo': Insumo, 'modo': Nome, 'args': Args, 'saida': None}
               for Insumo in Insumos for Nome, Args in CLI_POR_INSUMO]
        return Globais + Por
    Sem = SemOtim if Fase in FASES_COM_SEM_OTIM else None
    return [{'fase': Fase, 'insumo': Insumo, 'modo': Nome, 'args': ArgsComSemOtim(Args, Sem), 'saida': Saida}
            for Insumo in Insumos for Nome, Args, Saida in MODOS.get(Fase, [])]


def PastaDaTarefa(Tarefa):
    # caminho deterministico pelo hash do insumo: E0 e candidato gravam no MESMO caminho (o exe dev leva o caminho de saida)
    if Tarefa['saida'] is None:
        return None
    Sha8 = hashlib.sha256(Tarefa['insumo'].encode('utf-8')).hexdigest()[:8]
    return os.path.join(tempfile.gettempdir(), 'forja_paridade', Sha8, Tarefa['modo'])


def ArgvDaTarefa(Tarefa, Pasta):
    Saida = os.path.join(Pasta, Tarefa['saida']) if Pasta else None
    Trocas = {'{f}': Tarefa['insumo'], '{s}': Saida}
    return [Trocas.get(Arg, Arg) for Arg in Tarefa['args']]


def ChaveDoCache(Tarefa, Ctx):
    # o timeout entra na chave: um resultado do oraculo com outro limite de tempo nao vale para este
    Dados = {'versao_harness': VERSAO, 'oraculo': Ctx['oraculo_sha'], 'insumo': Tarefa['insumo'],
             'pasta': Ctx['pastas'].get(os.path.dirname(Tarefa['insumo'])), 'modo': Tarefa['modo'],
             'argv': ArgvDaTarefa(Tarefa, PastaDaTarefa(Tarefa)), 'versoes': list(Ctx['versoes']),
             'temp': tempfile.gettempdir(), 'timeout': Ctx['timeout']}
    return hashlib.sha256(json.dumps(Dados, sort_keys=True).encode('utf-8')).hexdigest()


def MontarTrocas(Pasta, Versoes):
    # troca caminho temporario por <SAIDA>/<TMP> e a versao por <VERSAO>, nas duas barras
    Trocas = []
    if Pasta:
        Trocas += [(Pasta, '<SAIDA>'), (Pasta.replace('\\', '/'), '<SAIDA>')]
    TempSistema = tempfile.gettempdir()
    Trocas += [(TempSistema, '<TMP>'), (TempSistema.replace('\\', '/'), '<TMP>')]
    Trocas += [(Versao, '<VERSAO>') for Versao in Versoes]
    return Trocas


def NormalizarTexto(Texto, Trocas):
    Texto = Texto.replace('\r\n', '\n')
    for De, Para in Trocas:
        Texto = Texto.replace(De, Para)
    return Texto


def Decodificar(Dados):
    return Dados.decode('utf-8', errors='replace')


def ListarArquivos(Pasta):
    Achados = {}
    if not Pasta:
        return Achados
    for Dir, _, Nomes in os.walk(Pasta):
        for Nome in Nomes:
            Caminho = os.path.join(Dir, Nome)
            Achados[os.path.relpath(Caminho, Pasta).replace(os.sep, '/')] = Caminho
    return Achados


def ExecutarComando(Exe, Argv, Pasta, Timeout):
    # a pasta de saida e limpa e recriada antes de cada corrida (E0 e candidato reusam o mesmo caminho)
    if Pasta:
        shutil.rmtree(Pasta, ignore_errors=True)
        os.makedirs(Pasta)
    Cwd = Pasta or tempfile.gettempdir()
    try:
        Proc = subprocess.run([Exe] + Argv, capture_output=True, timeout=Timeout, stdin=subprocess.DEVNULL, cwd=Cwd)
    except subprocess.TimeoutExpired:
        Proc = None
    return {'stdout': Proc.stdout if Proc else b'', 'stderr': Proc.stderr if Proc else b'',
            'exit': Proc.returncode if Proc else None, 'timeout': Proc is None,
            'arquivos': ListarArquivos(Pasta)}


def LerBytes(Caminho):
    with open(Caminho, 'rb') as Arq:
        return Arq.read()


def HashDeArquivo(Caminho, Norm):
    # dump de texto: normaliza caminhos e versao e faz sha do texto; exe/dll/ELF/PDB: mascara de versao (P3)
    if Caminho.endswith(EXTENSOES_TEXTO):
        Texto = REGEX_TEMPOS_JSON.sub('{"tipo": "tempos", "mascarado": true}', NormalizarTexto(Decodificar(LerBytes(Caminho)), Norm['trocas']))
        return hashlib.sha256(Texto.encode('utf-8')).hexdigest(), Texto
    try:
        return mascara_versao.HashSemVersao(Caminho, Norm['versoes']), None
    except mascara_versao.ErroMascaraVersao:
        # PE/ELF com versao fora das ancoras: troca so o texto da versao (ASCII) e segue
        Dados = LerBytes(Caminho)
        for Versao in Norm['versoes']:
            Dados = Dados.replace(Versao.encode('ascii'), b'<VERSAO>')
        return hashlib.sha256(Dados).hexdigest(), None


def NormalizarResultado(Bruto, Norm):
    Arquivos = {}
    for Rel, Caminho in sorted(Bruto['arquivos'].items()):
        Sha, Texto = HashDeArquivo(Caminho, Norm)
        Arquivos[Rel] = {'sha': Sha, 'texto': Texto}
    return {'stdout': NormalizarTexto(Decodificar(Bruto['stdout']), Norm['trocas']),
            'stderr': NormalizarTexto(Decodificar(Bruto['stderr']), Norm['trocas']),
            'exit': Bruto['exit'], 'timeout': Bruto['timeout'], 'arquivos': Arquivos}


def RodarNormalizado(Exe, Tarefa, Versoes, Timeout):
    Pasta = PastaDaTarefa(Tarefa)
    Norm = {'trocas': MontarTrocas(Pasta, Versoes), 'versoes': Versoes}
    return NormalizarResultado(ExecutarComando(Exe, ArgvDaTarefa(Tarefa, Pasta), Pasta, Timeout), Norm)


def OraculoComCache(Tarefa, Ctx):
    if not Ctx['cache']:
        return RodarNormalizado(Ctx['oraculo'], Tarefa, Ctx['versoes'], Ctx['timeout'])
    Caminho = os.path.join(Ctx['cache'], ChaveDoCache(Tarefa, Ctx) + '.json')
    if os.path.isfile(Caminho):
        with open(Caminho, encoding='utf-8') as Arq:
            return json.load(Arq)
    Orac = RodarNormalizado(Ctx['oraculo'], Tarefa, Ctx['versoes'], Ctx['timeout'])
    Temporario = Caminho + '.tmp'
    with open(Temporario, 'w', encoding='utf-8') as Arq:
        json.dump(Orac, Arq)
    os.replace(Temporario, Caminho)
    return Orac


def Ponto(Canal, Linha, Oraculo, Candidato):
    return {'canal': Canal, 'linha': Linha, 'oraculo': str(Oraculo)[:TAMANHO_TRECHO],
            'candidato': str(Candidato)[:TAMANHO_TRECHO]}


def PrimeiraDiferencaDeTexto(TextoA, TextoB):
    for Indice, (LinhaA, LinhaB) in enumerate(itertools.zip_longest(TextoA.split('\n'), TextoB.split('\n')), start=1):
        if LinhaA != LinhaB:
            return (Indice, '<fim>' if LinhaA is None else LinhaA, '<fim>' if LinhaB is None else LinhaB)
    return None


def PontoDoArquivo(Nome, ArqO, ArqC):
    Dif = None
    if ArqO['texto'] is not None and ArqC['texto'] is not None:
        Dif = PrimeiraDiferencaDeTexto(ArqO['texto'], ArqC['texto'])
    if Dif:
        return Ponto('arquivo ' + Nome, *Dif)
    return Ponto('arquivo ' + Nome, None, 'sha256 ' + ArqO['sha'][:16], 'sha256 ' + ArqC['sha'][:16])


def CompararArquivos(ArquivosO, ArquivosC):
    if sorted(ArquivosO) != sorted(ArquivosC):
        return 'diverge', Ponto('arquivos', None, ', '.join(sorted(ArquivosO)), ', '.join(sorted(ArquivosC)))
    for Nome in sorted(ArquivosO):
        if ArquivosO[Nome]['sha'] != ArquivosC[Nome]['sha']:
            return 'diverge', PontoDoArquivo(Nome, ArquivosO[Nome], ArquivosC[Nome])
    return 'igual', None


def CompararTempo(Orac, Cand):
    # estouro so de um lado e divergencia; estouro dos dois lados e igual
    if not (Orac['timeout'] or Cand['timeout']):
        return None
    if Orac['timeout'] and Cand['timeout']:
        return 'igual', None
    return 'diverge', Ponto('timeout', None, 'estourou' if Orac['timeout'] else 'terminou',
                            'estourou' if Cand['timeout'] else 'terminou')


def SaiuAnormal(Resultado):
    # saida anormal = o processo caiu: excecao do Windows (>= 0xC0000000), panic (101) ou R-code de runtime da Forja; timeout nao conta (CompararTempo)
    Codigo = Resultado['exit']
    if Codigo is None:
        return False
    if Codigo == SAIDA_PANIC or (Codigo & 0xFFFFFFFF) >= LIMITE_CODIGO_NT:
        return True
    return bool(REGEX_RCODE.search(Resultado['stdout'] + Resultado['stderr']))


def CompararResultados(Orac, Cand):
    # ordem: stub (candidato ainda nao portado; com o oraculo tambem caido = cai) > tempo > cai (os 2 caidos) > stdout > stderr > exit > arquivos
    Cairam = SaiuAnormal(Orac) and SaiuAnormal(Cand)
    if STUB_TEXTO in Cand['stdout'] or STUB_TEXTO in Cand['stderr']:
        return ('cai', None) if Cairam else ('stub', None)
    Tempo = CompararTempo(Orac, Cand)
    if Tempo:
        return Tempo
    if Cairam:
        return 'cai', None
    for Canal in ('stdout', 'stderr'):
        Dif = PrimeiraDiferencaDeTexto(Orac[Canal], Cand[Canal])
        if Dif:
            return 'diverge', Ponto(Canal, *Dif)
    if Orac['exit'] != Cand['exit']:
        return 'diverge', Ponto('exit', None, Orac['exit'], Cand['exit'])
    return CompararArquivos(Orac['arquivos'], Cand['arquivos'])


def TemArquivoDeTexto(Resultado):
    return any(Item['texto'] for Item in Resultado['arquivos'].values())


def EhNaoVazio(Orac, Cand, Classe, Fase=None):
    # NAO-VAZIO (fase bir0): igual com rc 0 e stdout com texto nos 2 lados; saida vazia dos dois lados nao prova o corte
    # dbgcode grava o dump em arquivo (x.exe.debug.*), entao la conta o arquivo de texto com conteudo
    if Classe != 'igual' or Orac['exit'] != 0 or Cand['exit'] != 0:
        return False
    if Fase == FASE_DBGCODE:
        return TemArquivoDeTexto(Orac) and TemArquivoDeTexto(Cand)
    return bool(Orac['stdout']) and bool(Cand['stdout'])


def PrimeiroStub(Cand):
    # primeiro "G3 stub: <nome>" do stdout e, sem ele, do stderr do candidato; sem nome = '<sem nome>'
    Achado = REGEX_STUB.search(Cand['stdout'] + '\n' + Cand['stderr'])
    return Achado.group(1) if Achado else '<sem nome>'


def ExecutarTarefa(Tarefa, Ctx):
    Stub = None
    NaoVazio = False
    try:
        Orac = OraculoComCache(Tarefa, Ctx)
        Cand = RodarNormalizado(Ctx['forja'], Tarefa, Ctx['versoes'], Ctx['timeout'])
        Classe, PontoDiverge = CompararResultados(Orac, Cand)
        if Classe == 'stub':
            Stub = PrimeiroStub(Cand)
        NaoVazio = EhNaoVazio(Orac, Cand, Classe, Tarefa['fase'])
    except (OSError, ValueError) as Erro:
        Classe, PontoDiverge = 'diverge', Ponto('erro do harness', None, '', str(Erro))
    Resultado = {'fase': Tarefa['fase'], 'insumo': Tarefa['insumo'], 'modo': Tarefa['modo'],
                 'classe': Classe, 'ponto': PontoDiverge}
    if Stub is not None:
        # so o stub leva o nome: divergencias e JSON sem --stubs ficam como antes
        Resultado['stub'] = Stub
    if Tarefa['fase'] in FASES_COM_NAO_VAZIO:
        # so as fases do corte (bir0, otim0) levam NAO-VAZIO: as outras seguem com o mesmo resultado de antes
        Resultado['nao_vazio'] = NaoVazio
    return Resultado


def RodarFase(Fase, Insumos, Ctx, Jobs):
    Tarefas = MontarTarefas(Fase, Insumos, Ctx.get('sem_otim'))
    Inicio = time.perf_counter()
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Resultados = list(Pool.map(functools.partial(ExecutarTarefa, Ctx=Ctx), Tarefas))
    return Resultados, time.perf_counter() - Inicio


def ContarNaoVazio(Resultados):
    # por modo: quantos NAO-VAZIO de quantas comparacoes o modo teve
    Total = collections.Counter(Resultado['modo'] for Resultado in Resultados)
    Achados = collections.Counter(Resultado['modo'] for Resultado in Resultados if Resultado.get('nao_vazio'))
    return {Modo: {'nao_vazio': Achados[Modo], 'total': Total[Modo]} for Modo in sorted(Total)}


def ResumirFase(Fase, Resultados, Tempo):
    Contagem = {'igual': 0, 'diverge': 0, 'stub': 0, 'cai': 0}
    for Resultado in Resultados:
        Contagem[Resultado['classe']] += 1
    Resumo = {'fase': Fase, 'total': len(Resultados), **Contagem, 'tempo_s': round(Tempo, 2),
              'nao_suportado': FASES_NAO_SUPORTADAS.get(Fase)}
    if Fase in FASES_COM_NAO_VAZIO:
        Resumo['nao_vazio'] = ContarNaoVazio(Resultados)
    return Resumo


def LerLista(Caminho):
    # um caminho .fj por linha (UTF-8, com ou sem BOM); vazia e '#' ficam de fora; relativo vale a partir do diretorio atual
    with open(Caminho, encoding='utf-8-sig') as Arq:
        Linhas = [Linha.strip() for Linha in Arq]
    return [os.path.abspath(Linha).replace('\\', '/') for Linha in Linhas if Linha and not Linha.startswith('#')]


def ResolverInsumos(Opcoes):
    # sem --lista: varredura do corpus (com --so); com --lista: so os caminhos da lista (--so filtra tambem)
    if Opcoes['lista'] is None:
        return ListarInsumos(Opcoes['so']), []
    Caminhos = sorted(set(LerLista(Opcoes['lista'])))
    if Opcoes['so']:
        Caminhos = [Caminho for Caminho in Caminhos if Opcoes['so'].replace('\\', '/') in Caminho]
    Existentes = [Caminho for Caminho in Caminhos if os.path.isfile(Caminho)]
    Ausentes = [Caminho for Caminho in Caminhos if not os.path.isfile(Caminho)]
    return Existentes, Ausentes


def MontarContexto(Oraculo, Candidato, Cache, Insumos, Timeout, SemOtim=None):
    if Cache:
        os.makedirs(Cache, exist_ok=True)
    return {'oraculo': Oraculo, 'forja': Candidato,
            'versoes': mascara_versao.VersoesDoForja(Oraculo, Candidato),
            'oraculo_sha': ShaDeArquivo(Oraculo), 'pastas': MapaDePastas(Insumos), 'cache': Cache, 'timeout': Timeout,
            'sem_otim': SemOtim}


def RodarTudo(Opcoes):
    Insumos, Ausentes = ResolverInsumos(Opcoes)
    Insumos = AmostrarInsumos(Insumos, Opcoes['amostra'])
    Ctx = MontarContexto(Opcoes['oraculo'], Opcoes['forja'], Opcoes['cache'], Insumos, Opcoes['timeout'], Opcoes.get('sem_otim'))
    Resumos, Todos = [], []
    for Fase in Opcoes['fases']:
        Resultados, Tempo = RodarFase(Fase, Insumos, Ctx, Opcoes['jobs'])
        Resumos.append(ResumirFase(Fase, Resultados, Tempo))
        Todos.extend(Resultados)
    return {'versoes': list(Ctx['versoes']), 'insumos': len(Insumos), 'fases': Resumos, 'resultados': Todos,
            'ausentes': Ausentes}


def ResolverFases(Texto):
    if Texto == 'tudo':
        return list(FASES)
    Fases = [Parte.strip() for Parte in Texto.split(',') if Parte.strip()]
    if not Fases or any(Fase not in FASES for Fase in Fases):
        return None
    return Fases


def LinhaDeDivergencia(Resultado):
    Ponto_ = Resultado['ponto']
    return (f"DIVERGE {Resultado['insumo']} | {Resultado['modo']} | {Ponto_['canal']} linha {Ponto_['linha'] or '-'}"
            f" | oraculo={Ponto_['oraculo']!r} | candidato={Ponto_['candidato']!r}")


def TabelaDeFases(Resumos):
    Linhas = [f"{'fase':<8}{'total':>7}{'igual':>8}{'diverge':>9}{'stub':>6}{'cai':>6}{'tempo s':>10}"]
    for Resumo in Resumos:
        Linhas.append(f"{Resumo['fase']:<8}{Resumo['total']:>7}{Resumo['igual']:>8}{Resumo['diverge']:>9}"
                      f"{Resumo['stub']:>6}{Resumo['cai']:>6}{Resumo['tempo_s']:>10.2f}")
        if 'nao_vazio' in Resumo:
            Linhas.append('  NAO-VAZIO: ' + ', '.join(f"{Modo} {Contagem['nao_vazio']}/{Contagem['total']}"
                                                      for Modo, Contagem in Resumo['nao_vazio'].items()))
        if Resumo['nao_suportado']:
            Linhas.append(f"  modo nao suportado pelo oraculo: {Resumo['fase']} ({Resumo['nao_suportado']})")
    return '\n'.join(Linhas)


def NaoVazioAbaixo(Resumos, Minimo):
    # --min-nao-vazio N: cada modo da fase bir0 tem de chegar a N NAO-VAZIO; sem a flag nao ha linha
    if Minimo is None:
        return []
    Linhas = []
    for Resumo in Resumos:
        for Modo, Contagem in Resumo.get('nao_vazio', {}).items():
            if Contagem['nao_vazio'] < Minimo:
                Linhas.append(f"NAO-VAZIO abaixo de {Minimo}: {Resumo['fase']} {Modo} {Contagem['nao_vazio']}/{Contagem['total']}")
    return Linhas


def FaseMaisFrequente(Fases):
    # empate de contagem vai para a fase em ordem alfabetica: o ranking sai igual em toda rodada
    return min(Fases, key=lambda Fase: (-Fases[Fase], Fase))


def RankingDeStubs(Resultados):
    # cada (insumo, modo) bloqueado conta 1 para o nome do seu primeiro G3 stub; ordem: mais bloqueios, depois nome
    Fases = {}
    for Resultado in Resultados:
        if Resultado['classe'] == 'stub':
            Contagem = Fases.setdefault(Resultado['stub'], collections.Counter())
            Contagem[Resultado['fase']] += 1
    Ranking = [{'nome': Nome, 'bloqueia': sum(Contagem.values()), 'fase_mais_frequente': FaseMaisFrequente(Contagem)}
               for Nome, Contagem in Fases.items()]
    return sorted(Ranking, key=lambda Linha: (-Linha['bloqueia'], Linha['nome']))


def GravarRankingStubs(Ranking):
    # pasta de saida do paridade = %TEMP%\forja_paridade (o cache fica em outra pasta)
    os.makedirs(PASTA_SAIDA, exist_ok=True)
    Caminho = os.path.join(PASTA_SAIDA, 'stubs.tsv')
    Linhas = ['nome\tbloqueia\tfase_mais_frequente']
    Linhas += [f"{Linha['nome']}\t{Linha['bloqueia']}\t{Linha['fase_mais_frequente']}" for Linha in Ranking]
    with open(Caminho, 'w', encoding='utf-8', newline='\n') as Arq:
        Arq.write('\n'.join(Linhas) + '\n')
    return Caminho


def ImprimirRanking(Ranking, Caminho):
    if not Ranking:
        print('nenhum G3 stub visto')
        return
    print(f"STUBS: {len(Ranking)} nomes bloqueiam (insumo, modo), do que mais bloqueia para o que menos")
    print(f"{'bloqueia':>9}  {'fase':<8}nome")
    for Linha in Ranking[:LIMITE_RANKING_TELA]:
        print(f"{Linha['bloqueia']:>9}  {Linha['fase_mais_frequente']:<8}{Linha['nome']}")
    if len(Ranking) > LIMITE_RANKING_TELA:
        print(f"... mais {len(Ranking) - LIMITE_RANKING_TELA} nomes")
    print(f"ranking completo: {Caminho}")


def ImprimirRelatorio(Rodada, Args):
    Divergencias = [R for R in Rodada['resultados'] if R['classe'] == 'diverge']
    Stubs = [R for R in Rodada['resultados'] if R['classe'] == 'stub']
    Cais = [R for R in Rodada['resultados'] if R['classe'] == 'cai']
    for Caminho in Rodada['ausentes']:
        print('AVISO: insumo da lista inexistente, pulado: ' + Caminho, file=sys.stderr)
    Abaixo = NaoVazioAbaixo(Rodada['fases'], Args.min_nao_vazio)
    for Linha in Abaixo:
        print(Linha, file=sys.stderr)
    Codigo = 0 if not Divergencias and not (Args.sem_stub and Stubs) and not Abaixo else 1
    Ranking = RankingDeStubs(Rodada['resultados']) if Args.stubs else None
    Caminho = GravarRankingStubs(Ranking) if Args.stubs else None
    if Args.json:
        Saida = {'versoes': Rodada['versoes'], 'insumos': Rodada['insumos'], 'fases': Rodada['fases'],
                 'divergencias': Divergencias, 'stubs': len(Stubs), 'cai': len(Cais), 'ausentes': Rodada['ausentes']}
        if Ranking is not None:
            Saida['ranking_stubs'] = Ranking
        print(json.dumps(Saida, indent=2, ensure_ascii=True))
        return Codigo
    for Resultado in Divergencias:
        print(LinhaDeDivergencia(Resultado))
    if Args.sem_stub:
        for Resultado in Stubs:
            print(f"STUB {Resultado['insumo']} | {Resultado['modo']}")
    print(TabelaDeFases(Rodada['fases']))
    Versoes = ', '.join(Rodada['versoes'])
    Ausentes = f", ausentes {len(Rodada['ausentes'])}" if Rodada['ausentes'] else ''
    print(f"RESULTADO: {len(Divergencias)} diverge, {len(Stubs)} stub, {len(Cais)} cai (insumos {Rodada['insumos']}{Ausentes}, versoes {Versoes})")
    if Ranking is not None:
        ImprimirRanking(Ranking, Caminho)
    return Codigo


def ValidarArgumentos(Args):
    if not os.path.isfile(Args.forja):
        return 'forja candidato inexistente: ' + Args.forja
    if not os.path.isfile(Args.oraculo):
        return 'oraculo inexistente: ' + Args.oraculo
    if Args.amostra is not None and Args.amostra < 1:
        return '--amostra precisa ser >= 1'
    if Args.jobs < 1:
        return '--jobs precisa ser >= 1'
    if Args.timeout < 1:
        return '--timeout precisa ser >= 1'
    if Args.lista is not None and not os.path.isfile(Args.lista):
        return 'arquivo da lista inexistente: ' + Args.lista
    if ResolverFases(Args.fase) is None:
        return '--fase invalida: ' + Args.fase + ' (validas: ' + ','.join(FASES) + ', tudo)'
    if Args.min_nao_vazio is not None and Args.min_nao_vazio < 0:
        return '--min-nao-vazio precisa ser >= 0'
    if Args.min_nao_vazio is not None and not set(FASES_COM_NAO_VAZIO) & set(ResolverFases(Args.fase)):
        return '--min-nao-vazio so vale com a fase bir0 ou otim0 em --fase'
    if Args.sem_otim is not None and not set(FASES_COM_SEM_OTIM) & set(ResolverFases(Args.fase)):
        return '--sem-otim so vale com a fase otim0 ou x64_0 em --fase'
    return None


def ConstruirParser():
    Analisador = argparse.ArgumentParser(description='Paridade do forja candidato contra o oraculo E0 (G3 O1).')
    Analisador.add_argument('--forja', default=FORJA_PADRAO)
    Analisador.add_argument('--oraculo', default=FORJA_PADRAO)
    Analisador.add_argument('--fase', default='tudo')
    Analisador.add_argument('--so', default=None)
    Analisador.add_argument('--lista', default=None)
    Analisador.add_argument('--amostra', type=int, default=None)
    Analisador.add_argument('--timeout', type=int, default=TIMEOUT_S)
    Analisador.add_argument('--jobs', type=int, default=os.cpu_count() or 1)
    Analisador.add_argument('--cache', default=CACHE_PADRAO)
    Analisador.add_argument('--min-nao-vazio', type=int, default=None)
    Analisador.add_argument('--sem-otim', dest='sem_otim', default=None, help='lista de passes do --sem-otim do forja (so fase otim0; entra nos ARGS e na chave do cache)')
    Analisador.add_argument('--sem-stub', action='store_true')
    Analisador.add_argument('--stubs', action='store_true')
    Analisador.add_argument('--json', action='store_true')
    Analisador.add_argument('--selftest', action='store_true')
    Analisador.add_argument('--versao', action='store_true')
    return Analisador


def ResultadoSintetico(*, Stdout='a\nb\nc\n', Stderr='', Exit=0, Arquivos=None, Timeout=False):
    return {'stdout': Stdout, 'stderr': Stderr, 'exit': Exit, 'timeout': Timeout, 'arquivos': dict(Arquivos or {})}


def TestarComparador():
    Base = ResultadoSintetico()
    Casos = [
        ('igual', Base, ResultadoSintetico(), ('igual', None, None)),
        ('stdout linha 3', Base, ResultadoSintetico(Stdout='a\nb\nX\n'), ('diverge', 'stdout', 3)),
        ('exit', Base, ResultadoSintetico(Exit=1), ('diverge', 'exit', None)),
        ('hash de arquivo', ResultadoSintetico(Arquivos={'x.exe': {'sha': '1', 'texto': None}}),
         ResultadoSintetico(Arquivos={'x.exe': {'sha': '2', 'texto': None}}), ('diverge', 'arquivo x.exe', None)),
        ('dump de texto linha 2', ResultadoSintetico(Arquivos={'x.debug.txt': {'sha': 'a', 'texto': 'p\nq\n'}}),
         ResultadoSintetico(Arquivos={'x.debug.txt': {'sha': 'b', 'texto': 'p\nr\n'}}), ('diverge', 'arquivo x.debug.txt', 2)),
        ('stub', Base, ResultadoSintetico(Stdout='G3 stub: parse\n'), ('stub', None, None)),
        ('timeout nos dois lados', ResultadoSintetico(Timeout=True), ResultadoSintetico(Timeout=True), ('igual', None, None)),
        ('timeout de 1 lado', ResultadoSintetico(Timeout=True, Exit=None), ResultadoSintetico(Exit=101), ('diverge', 'timeout', None)),
        ('cai: 101 nos 2 lados', ResultadoSintetico(Exit=101), ResultadoSintetico(Exit=101, Stdout='x\n'), ('cai', None, None)),
        ('cai: stub nos 2 lados', ResultadoSintetico(Exit=101, Stderr='G3 stub: a.b\n'),
         ResultadoSintetico(Exit=101, Stdout='{"codigo": "R0003", "causa": "G3 stub: a.b"}\n'), ('cai', None, None)),
        ('cai: 0xC0000005 x R-code', ResultadoSintetico(Exit=0xC0000005), ResultadoSintetico(Exit=1, Stdout='{"codigo": "R0001"}\n'), ('cai', None, None)),
        ('so o candidato cai', Base, ResultadoSintetico(Exit=101), ('diverge', 'exit', None)),
        ('so o oraculo cai', ResultadoSintetico(Exit=101), Base, ('diverge', 'exit', None)),
    ]
    Erros = []
    for Nome, Orac, Cand, Esperado in Casos:
        Classe, Ponto_ = CompararResultados(Orac, Cand)
        Obtido = (Classe, Ponto_['canal'] if Ponto_ else None, Ponto_['linha'] if Ponto_ else None)
        if Obtido != Esperado:
            Erros.append(f'{Nome}: esperado {Esperado}, obtido {Obtido}')
    return Erros, f'{len(Casos)} casos sinteticos'


def TestarNormalizacao():
    Pasta = os.path.join(tempfile.gettempdir(), 'forja_paridade', 'abc12345', 'bir')
    TempSistema = tempfile.gettempdir()
    Bruto = Pasta + '\\x.exe.debug.txt forja 0.23.0 ' + Pasta.replace('\\', '/') + '/x\r\n' + TempSistema + '\\outro\n'
    Esperado = '<SAIDA>\\x.exe.debug.txt forja <VERSAO> <SAIDA>/x\n<TMP>\\outro\n'
    Obtido = NormalizarTexto(Bruto, MontarTrocas(Pasta, ('0.23.0',)))
    Erros = [] if Obtido == Esperado else [f'esperado {Esperado!r}, obtido {Obtido!r}']
    return Erros, 'caminho de saida, temp e versao viram <SAIDA>/<TMP>/<VERSAO>'


def ClassesDe(Rodada):
    return [(R['insumo'], R['modo'], R['classe']) for R in Rodada['resultados']]


def ContarClasse(Rodada, Classe):
    return sum(1 for R in Rodada['resultados'] if R['classe'] == Classe)


def TestarCacheEIgualdade(PastaSelftest):
    Opcoes = {'oraculo': FORJA_PADRAO, 'forja': FORJA_PADRAO, 'fases': list(FASES), 'amostra': 60,
              'jobs': os.cpu_count() or 1, 'cache': os.path.join(PastaSelftest, 'cache'), 'so': None,
              'timeout': TIMEOUT_S, 'lista': None}
    Primeira = RodarTudo(Opcoes)
    Segunda = RodarTudo(Opcoes)
    Tempo1 = sum(F['tempo_s'] for F in Primeira['fases'])
    Tempo2 = sum(F['tempo_s'] for F in Segunda['fases'])
    Erros = []
    if ContarClasse(Primeira, 'diverge') or ContarClasse(Primeira, 'stub'):
        Erros.append(f"E0 x E0 deu {ContarClasse(Primeira, 'diverge')} diverge e {ContarClasse(Primeira, 'stub')} stub")
    if ClassesDe(Primeira) != ClassesDe(Segunda):
        Erros.append('segunda rodada com cache deu classes diferentes da primeira')
    if Tempo2 >= Tempo1:
        Erros.append(f'segunda rodada nao foi mais rapida ({Tempo2:.2f} s >= {Tempo1:.2f} s)')
    Oraculos = len(os.listdir(Opcoes['cache']))
    Msg = (f"{len(Primeira['resultados'])} comparacoes, rodada 1 {Tempo1:.2f} s, rodada 2 {Tempo2:.2f} s "
           f"com cache ({Oraculos} oraculos gravados)")
    return Erros, Msg


def TestarSensibilidade(PastaSelftest):
    Bat = os.path.join(PastaSelftest, 'forja_falso.bat')
    with open(Bat, 'w', encoding='ascii', newline='\r\n') as Arq:
        Arq.write('@echo off\n"%s" %%*\necho x\n' % FORJA_PADRAO)
    Opcoes = {'oraculo': FORJA_PADRAO, 'forja': Bat, 'fases': ['ast', 'tipos'], 'amostra': 5,
              'jobs': os.cpu_count() or 1, 'cache': None, 'so': None, 'timeout': TIMEOUT_S, 'lista': None}
    Rodada = RodarTudo(Opcoes)
    Divergem = ContarClasse(Rodada, 'diverge')
    Erros = [] if Divergem else ['o forja falso (acrescenta linha x) passou sem nenhum diverge']
    return Erros, f"{Divergem} diverge em {len(Rodada['resultados'])} comparacoes com o forja falso"


def TestarRankingDeStubs():
    Casos = [('stdout antes do stderr', ResultadoSintetico(Stdout='x\nG3 stub: parse.Expr\nG3 stub: y.z\n',
                                                          Stderr='G3 stub: tipos.Infer\n'), 'parse.Expr'),
             ('so no stderr', ResultadoSintetico(Stdout='a\n', Stderr='G3 stub: tipos.Infer\n'), 'tipos.Infer'),
             ('sem nome', ResultadoSintetico(Stdout='G3 stub:\n'), '<sem nome>')]
    Erros = [f'{Nome}: esperado {Esperado!r}, obtido {PrimeiroStub(Cand)!r}'
             for Nome, Cand, Esperado in Casos if PrimeiroStub(Cand) != Esperado]
    Resultados = [{'fase': 'ast', 'classe': 'stub', 'stub': 'b.p', 'insumo': 'i1', 'modo': 'ast'},
                  {'fase': 'tipos', 'classe': 'stub', 'stub': 'a.q', 'insumo': 'i2', 'modo': 'tipos'},
                  {'fase': 'tipos', 'classe': 'stub', 'stub': 'a.q', 'insumo': 'i3', 'modo': 'tipos'},
                  {'fase': 'ast', 'classe': 'stub', 'stub': 'c.r', 'insumo': 'i4', 'modo': 'ast'},
                  {'fase': 'check', 'classe': 'stub', 'stub': 'c.r', 'insumo': 'i5', 'modo': 'check-resumo'},
                  {'fase': 'ast', 'classe': 'stub', 'stub': 'd.s', 'insumo': 'i6', 'modo': 'ast'},
                  {'fase': 'ast', 'classe': 'igual', 'insumo': 'i7', 'modo': 'ast'}]
    Obtido = [(Linha['nome'], Linha['bloqueia'], Linha['fase_mais_frequente']) for Linha in RankingDeStubs(Resultados)]
    Esperado = [('a.q', 2, 'tipos'), ('c.r', 2, 'ast'), ('b.p', 1, 'ast'), ('d.s', 1, 'ast')]
    if Obtido != Esperado:
        Erros.append(f'ranking: esperado {Esperado}, obtido {Obtido}')
    return Erros, f'{len(Casos)} linhas G3 stub e ranking com desempate (bloqueios, depois nome; fase alfabetica)'


def TestarTimeoutNaChave():
    Tarefa = {'fase': 'ast', 'insumo': 'C:/exemplo/a.fj', 'modo': 'ast', 'args': ['ast', '{f}'], 'saida': None}
    Base = {'oraculo_sha': 'sha-falso', 'pastas': {}, 'versoes': ['0.23.0']}
    Chave30 = ChaveDoCache(Tarefa, {**Base, 'timeout': 30})
    Chave120 = ChaveDoCache(Tarefa, {**Base, 'timeout': 120})
    Chave30Outra = ChaveDoCache(Tarefa, {**Base, 'timeout': 30})
    Erros = []
    if Chave30 == Chave120:
        Erros.append('timeouts 30 e 120 deram a mesma chave do cache')
    if Chave30 != Chave30Outra:
        Erros.append('mesmo timeout (30) deu chaves diferentes')
    return Erros, 'timeouts 30 e 120 geram chaves de cache diferentes; mesmo timeout, mesma chave'


def TestarListaRestringe(PastaSelftest):
    Insumos = ListarInsumos(None)[:3]
    Lista = os.path.join(PastaSelftest, 'lista.txt')
    with open(Lista, 'w', encoding='utf-8', newline='\n') as Arq:
        Arq.write('# insumos do selftest\n\n' + '\n'.join(Insumos) + '\n')
    Opcoes = {'oraculo': FORJA_PADRAO, 'forja': FORJA_PADRAO, 'fases': ['ast', 'tipos'], 'amostra': None,
              'jobs': os.cpu_count() or 1, 'cache': None, 'so': None, 'timeout': TIMEOUT_S, 'lista': Lista}
    Rodada = RodarTudo(Opcoes)
    Erros = []
    for Fase in Rodada['fases']:
        if Fase['total'] != len(Insumos):
            Erros.append(f"fase {Fase['fase']} deu {Fase['total']} comparacoes, esperado {len(Insumos)}")
    Visto = {R['insumo'] for R in Rodada['resultados']}
    if Visto != set(LerLista(Lista)):
        Erros.append('os insumos comparados nao sao os da lista')
    if Rodada['ausentes']:
        Erros.append(f"{len(Rodada['ausentes'])} insumos da lista marcados como ausentes")
    return Erros, f"{len(Insumos)} insumos da lista, {len(Rodada['resultados'])} comparacoes (2 fases)"


def TestarNaoVazio(PastaSelftest):
    # (h): o E0 x E0 da bir0 tem NAO-VAZIO; um forja que descarta a saida (>nul) diverge do E0 nos modos que imprimem
    Base = {'oraculo': FORJA_PADRAO, 'forja': FORJA_PADRAO, 'fases': [FASE_BIR0], 'amostra': 5,
            'jobs': os.cpu_count() or 1, 'cache': None, 'so': None, 'timeout': TIMEOUT_S, 'lista': None}
    Iguais = RodarTudo(Base)
    Erros = []
    if ContarClasse(Iguais, 'diverge') or ContarClasse(Iguais, 'stub'):
        Erros.append(f"E0 x E0 da bir0 deu {ContarClasse(Iguais, 'diverge')} diverge e {ContarClasse(Iguais, 'stub')} stub")
    NaoVazios = {Modo: C['nao_vazio'] for Modo, C in Iguais['fases'][0]['nao_vazio'].items()}
    if sum(NaoVazios.values()) == 0:
        Erros.append('E0 x E0 da bir0 deu NAO-VAZIO 0 em todos os modos')
    Bat = os.path.join(PastaSelftest, 'forja_vazio.bat')
    with open(Bat, 'w', encoding='ascii', newline='\r\n') as Arq:
        Arq.write('@echo off\n"%s" %%* >nul\n' % FORJA_PADRAO)
    Vazio = dict(Base, forja=Bat)
    Divergem = ContarClasse(RodarTudo(Vazio), 'diverge')
    if not Divergem:
        Erros.append('o forja que descarta a saida passou sem nenhum diverge na bir0')
    Msg = f"NAO-VAZIO por modo {NaoVazios}; forja que descarta a saida: {Divergem} diverge"
    return Erros, Msg


def TestarOtim0(PastaSelftest):
    # (i): o forja real nao imprime nada em build --release (NAO-VAZIO dele seria 0), entao o oraculo aqui e um .bat de saida fixa: prova a mecanica da fase otim0
    # (--sem-otim no argv, NAO-VAZIO por modo, E0xE0 determinista e forja que descarta a saida = diverge). O NAO-VAZIO real do corte e o autoteste (v) do espelho.
    Erros = []
    Argv = ArgsComSemOtim(['build', '--release', '{f}', '-o', '{s}'], 'numeracao,cfg')
    if Argv != ['build', '--release', '--sem-otim', 'numeracao,cfg', '{f}', '-o', '{s}']:
        Erros.append(f'--sem-otim fora dos ARGS na posicao certa: {Argv}')
    Bat = os.path.join(PastaSelftest, 'otim_falso_saida.bat')
    with open(Bat, 'w', encoding='ascii', newline='\r\n') as Arq:
        Arq.write('@echo off\necho otim: 0 passes\n')
    Vazio = os.path.join(PastaSelftest, 'otim_falso_vazio.bat')
    with open(Vazio, 'w', encoding='ascii', newline='\r\n') as Arq:
        Arq.write('@echo off\nexit /b 0\n')
    Base = {'oraculo': Bat, 'forja': Bat, 'fases': [FASE_OTIM0], 'amostra': 3, 'jobs': os.cpu_count() or 1, 'cache': None, 'so': None,
            'timeout': TIMEOUT_S, 'lista': None, 'sem_otim': 'numeracao,cfg'}
    Primeira = RodarTudo(Base)
    Segunda = RodarTudo(Base)
    if ContarClasse(Primeira, 'diverge') or ContarClasse(Primeira, 'stub'):
        Erros.append(f"E0xE0 otim0 deu {ContarClasse(Primeira, 'diverge')} diverge e {ContarClasse(Primeira, 'stub')} stub")
    if ClassesDe(Primeira) != ClassesDe(Segunda):
        Erros.append('otim0 nao deterministico: segunda rodada deu classes diferentes')
    NaoVazios = {Modo: C['nao_vazio'] for Modo, C in Primeira['fases'][0]['nao_vazio'].items()}
    if sum(NaoVazios.values()) == 0:
        Erros.append('E0xE0 otim0 deu NAO-VAZIO 0 em todos os modos')
    Divergem = ContarClasse(RodarTudo(dict(Base, forja=Vazio)), 'diverge')
    if not Divergem:
        Erros.append('o forja que descarta a saida passou sem nenhum diverge na otim0')
    Msg = f"--sem-otim no argv; NAO-VAZIO por modo {NaoVazios}; forja que descarta a saida: {Divergem} diverge"
    return Erros, Msg


def RodarSelftest():
    PastaSelftest = tempfile.mkdtemp(prefix='paridade_selftest_')
    Testes = [('(a) comparador', TestarComparador),
              ('(b) normalizacao', TestarNormalizacao),
              ('(c) E0 x E0 amostra 60 com cache', functools.partial(TestarCacheEIgualdade, PastaSelftest)),
              ('(d) sensibilidade (forja falso)', functools.partial(TestarSensibilidade, PastaSelftest)),
              ('(e) ranking de stubs', TestarRankingDeStubs),
              ('(f) timeout entra na chave do cache', TestarTimeoutNaChave),
              ('(g) --lista restringe a execucao', functools.partial(TestarListaRestringe, PastaSelftest)),
              ('(h) bir0: NAO-VAZIO e forja que descarta a saida', functools.partial(TestarNaoVazio, PastaSelftest)),
              ('(i) otim0: --sem-otim no argv, NAO-VAZIO e forja que descarta a saida', functools.partial(TestarOtim0, PastaSelftest))]
    Falhas = 0
    try:
        for Nome, Teste in Testes:
            Erros, Msg = Teste()
            if Erros:
                Falhas += 1
                print(f"FALHOU {Nome}: " + '; '.join(Erros))
            else:
                print(f"OK {Nome}: {Msg}")
    finally:
        shutil.rmtree(PastaSelftest, ignore_errors=True)
    Total = len(Testes)
    print(f"selftest: {'OK' if not Falhas else 'FALHOU'} ({Total - Falhas}/{Total})")
    return 0 if not Falhas else 1


def Principal(Argv):
    Args = ConstruirParser().parse_args(Argv)
    if Args.versao:
        print('paridade.py ' + VERSAO)
        return 0
    if Args.selftest:
        return RodarSelftest()
    Erro = ValidarArgumentos(Args)
    if Erro:
        print('uso: ' + Erro)
        return 2
    Opcoes = {'oraculo': os.path.abspath(Args.oraculo), 'forja': os.path.abspath(Args.forja),
              'fases': ResolverFases(Args.fase), 'amostra': Args.amostra, 'jobs': Args.jobs,
              'cache': os.path.abspath(Args.cache), 'so': Args.so, 'timeout': Args.timeout,
              'lista': os.path.abspath(Args.lista) if Args.lista else None, 'sem_otim': Args.sem_otim}
    return ImprimirRelatorio(RodarTudo(Opcoes), Args)


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    sys.exit(Principal(sys.argv[1:]))
