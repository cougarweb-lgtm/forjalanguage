# sonda_bytes.py - compara BYTES CRUS (stdout, stderr, exit) do E0 e do candidato; o paridade.py troca CRLF por LF antes de comparar e nao ve diferenca so de fim de linha.
# Uso: python -I sonda_bytes.py <E0> <candidato> [lista.txt de .fj] ; exit 1 se algo diverge. Nao grava nada (cwd = %TEMP%).
import collections, glob, os, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
E0, E1 = sys.argv[1], sys.argv[2]
RAIZ = 'C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA'
CR, LF = bytes([13]), bytes([10])
GLOBAIS = [['--versao'], ['--ajuda'], [], ['explicar'], ['explicar', 'E0101'], ['explicar', 'E0101', '--json'], ['explicar', 'R0006'], ['explicar', 'ZZZ'],
           ['explicar', '--lista'], ['explicar', '--lista', '--json'], ['explicar', '--exemplos'], ['explicar', 'E0101', '--exemplos'],
           ['spec'], ['spec', '--json'], ['spec', '--cartao'], ['spec', '--receitas'], ['spec', 'E0101'], ['spec', '--exemplo', 'E0101'], ['spec', '--secao', 'tipos'], ['spec', '--zzz'],
           ['consultar'], ['consultar', 'foo'], ['mapa'], ['editar'], ['corrigir'], ['migrar'], ['fmt'], ['ligar'], ['ligar', '--codigos'], ['build', '--projeto']]
POR_INSUMO = [('fmt-json', ['fmt', '{f}', '--json']), ('fmt-stdin', ['fmt', '--stdin']), ('mapa-json', ['mapa', '{f}', '--json']), ('mapa-resumo', ['mapa', '{f}', '--resumo']),
              ('simbolos', ['consultar', 'simbolos', '{f}']), ('simbolos-json', ['consultar', 'simbolos', '{f}', '--json']), ('imports-json', ['consultar', 'imports', '{f}', '--json']),
              ('api-json', ['consultar', 'api', '{f}', '--json']), ('testes-json', ['consultar', 'testes', '{f}', '--json']), ('entrada-json', ['consultar', 'entrada', '{f}', '--json']),
              ('corrigir', ['corrigir', '{f}']), ('corrigir-provavel', ['corrigir', '{f}', '--tambem-provavel']), ('migrar-dry', ['migrar', '{f}', '--dry-run']),
              ('migrar-dry-json', ['migrar', '{f}', '--dry-run', '--json'])]


def InsumosPadrao():
    Padroes = [('/examples/*.fj', 4), ('/language/tests/consultar/proj*/main.fj', 4), ('/language/tests/editar/proj*/main.fj', 4), ('/language/tests/semantica/err_*.fj', 40),
               ('/language/tests/semantica/ok_g3_pacote*.fj', 6), ('/language/tests/sintaxe/*.fj', 15), ('/language/tests/lowering/*.fj', 15), ('/language/std/*.fj', 12)]
    Achados = []
    for Padrao, Quantos in Padroes:
        Lista = sorted(glob.glob(RAIZ + Padrao))
        Passo = max(1, len(Lista) // Quantos)
        Achados += [Caminho.replace(os.sep, '/') for Caminho in Lista[::Passo][:Quantos]]
    return Achados


def Rodar(Exe, Argv, Entrada):
    try:
        Proc = subprocess.run([Exe] + Argv, capture_output=True, input=Entrada, timeout=60, cwd=tempfile.gettempdir())
        return Proc.returncode, Proc.stdout, Proc.stderr
    except subprocess.TimeoutExpired:
        return 'TIMEOUT', b'', b''


def Comparar(Tarefa):
    Nome, Argv, Insumo = Tarefa
    Entrada = open(Insumo, 'rb').read() if Nome == 'fmt-stdin' else b''
    Argv = [Insumo if Arg == '{f}' else Arg for Arg in Argv]
    Orac, Cand = Rodar(E0, Argv, Entrada), Rodar(E1, Argv, Entrada)
    if Orac == Cand:
        return Nome, 'igual', None
    Sem = lambda Dados: Dados.replace(CR + LF, LF)
    SoFimDeLinha = Orac[0] == Cand[0] and Sem(Orac[1]) == Sem(Cand[1]) and Sem(Orac[2]) == Sem(Cand[2])
    Canal, DadosO, DadosC = ('stdout', Orac[1], Cand[1]) if Orac[1] != Cand[1] else ('stderr', Orac[2], Cand[2])
    Onde = next((K for K in range(min(len(DadosO), len(DadosC))) if DadosO[K] != DadosC[K]), min(len(DadosO), len(DadosC)))
    Detalhe = f"{' '.join(Argv)} | exit {Orac[0]} x {Cand[0]} | {Canal} byte {Onde}: {DadosO[max(0, Onde - 30):Onde + 60]!r} x {DadosC[max(0, Onde - 30):Onde + 60]!r}"
    return Nome, 'so-fim-de-linha' if SoFimDeLinha else 'diverge', Detalhe


Insumos = [Linha.strip() for Linha in open(sys.argv[3], encoding='utf-8-sig') if Linha.strip()] if len(sys.argv) > 3 else InsumosPadrao()
Tarefas = [('global', Argv, None) for Argv in GLOBAIS] + [(Nome, Argv, Insumo) for Insumo in Insumos for Nome, Argv in POR_INSUMO]
with ThreadPoolExecutor(max_workers=8) as Pool:
    Resultados = list(Pool.map(Comparar, Tarefas))
Placar, Mostrados = collections.Counter(), collections.Counter()
for Nome, Classe, Detalhe in Resultados:
    Placar[Nome, Classe] += 1
    if Detalhe and Mostrados[Nome, Classe] < 2:
        Mostrados[Nome, Classe] += 1
        print(Classe.upper(), Nome, Detalhe[:900])
for Nome in ['global'] + [Nome for Nome, _ in POR_INSUMO]:
    print(f"{Nome:18} igual={Placar[Nome, 'igual']:4} so-fim-de-linha={Placar[Nome, 'so-fim-de-linha']:3} diverge={Placar[Nome, 'diverge']:3}")
print(f"{len(Insumos)} insumos, {len(Tarefas)} comparacoes")
sys.exit(1 if any(Classe != 'igual' for _, Classe, _ in Resultados) else 0)
