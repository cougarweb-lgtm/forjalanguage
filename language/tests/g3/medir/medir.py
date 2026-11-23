# medir.py - mede o tempo de cada verificador com o forja dado (TEMP privado por verificador); so leitura do repositorio
import os, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor
Forja, Base, Rotulo = sys.argv[1], sys.argv[2], sys.argv[3]
T = 'C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/tests'
Lista = [('consultar', 'consultar/verificar_consultar.py'), ('mapa', 'consultar/verificar_mapa.py'), ('editar', 'editar/verificar_editar.py'),
         ('editar_i5', 'editar/verificar_editar_i5.py'), ('fmt', 'fmt/verificar_fmt.py'), ('cli', 'cli/verificar_cli.py'),
         ('ligar', 'ligar/verificar_ligar.py'), ('interop', 'interop/verificar_interop.py'), ('projeto', 'projeto/verificar_projeto.py')]
So = sys.argv[4].split(',') if len(sys.argv) > 4 else None
def Rodar(Item):
    Nome, Script = Item
    Tmp = os.path.join(Base, 'tmp_' + Rotulo + '_' + Nome)
    os.makedirs(Tmp, exist_ok=True)
    Amb = dict(os.environ, TEMP=Tmp, TMP=Tmp)
    Ini = time.perf_counter()
    try:
        P = subprocess.run([sys.executable, os.path.join(T, Script), '--forja', Forja], capture_output=True, stdin=subprocess.DEVNULL, timeout=400, env=Amb, cwd=Tmp)
        Rc, Saida = P.returncode, (P.stdout + P.stderr).decode('utf-8', 'replace')
    except subprocess.TimeoutExpired as E:
        Rc, Saida = 'TIMEOUT', ((E.stdout or b'') + (E.stderr or b'')).decode('utf-8', 'replace')
    Dur = time.perf_counter() - Ini
    with open(os.path.join(Base, Rotulo + '_' + Nome + '.out'), 'w', encoding='utf-8') as Arq:
        Arq.write(Saida)
    Linhas = [L for L in Saida.splitlines() if L.strip()]
    Fim = ' || '.join(L[:200] for L in Linhas[-2:])
    with open(os.path.join(Base, Rotulo + '_tempos.log'), 'a', encoding='utf-8') as Arq:
        Arq.write(f"{Nome}\trc={Rc}\t{Dur:.1f}s\t{Fim}\n")
    return Nome
Itens = [I for I in Lista if So is None or I[0] in So]
with ThreadPoolExecutor(max_workers=3) as Pool:
    list(Pool.map(Rodar, Itens))
print(open(os.path.join(Base, Rotulo + '_tempos.log'), encoding='utf-8').read())
