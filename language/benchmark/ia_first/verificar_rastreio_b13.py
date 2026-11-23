# verificar_rastreio_b13.py -- prova do P-IA1-b13: forja run|test --rastreio no backend C (pilha sombra) com a MESMA linha JSON do x64
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  paridade   o mesmo programa dá a MESMA linha R (texto igual, campo a campo: coluna, valor, pilha com proc/arquivo/linha/coluna, "quadros") e o mesmo exit/stdout em
             --backend x64 e --backend c com --rastreio (R0006 de 2 quadros, recursao, recursao funda com "quadros", fatia, R0003 assert/panic, R0002, R0004, modulo,
             lista, forja test com 1 e 2 falhas, vazamento R0009, programa sem erro)
  m_d8       100% das linhas R com explicacao, forma_certa e spec nos 2 backends; com --rastreio o R0006 leva coluna, valor e pilha >= 2 quadros nos 2 backends
  c_sem_flag backend c SEM --rastreio: a linha R nao tem coluna, valor nem pilha; com a flag, tirados esses campos, stdout, stderr e exit sao os mesmos
  c_texto    `build --modo c` sem a flag: deterministico (2 builds iguais) e sem a pilha sombra (corpus: tests\\cli\\*.fj, examples\\*.fj, tests\\cli\\b12 e b13); com a flag o C ganha a pilha
  release    --release ignora a flag (saida igual com e sem), nos 2 backends
  uso        --rastreio fora de build/run/test e com --suporte/--modo dll = exit 2
Uso:
    python -I verificar_rastreio_b13.py --forja <forja.exe> [--referencia <forja_b12.exe>]
    python -I verificar_rastreio_b13.py --forja <forja_b12.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python -I verificar_rastreio_b13.py --selftest
Versao: 0.2.1
Changelog: 0.2.1 - M6 P70: a paridade ignora "pilha" e "forma_certa" so no R0009 (o backend C nao tem pilha de vazamento, D-M6-7).
           0.2.0 - M6 P69: c_sem_flag e c_texto com oraculo proprio (sem forja de referencia; --referencia ignorado); a normalizacao de linha R vem de tests\\rcode_normalizar.py.
           0.1.0- P-IA1-b13: primeira versao.
Verificacao: python -I verificar_rastreio_b13.py --selftest
"""

import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LANG = Path(r"C:\Users\USUARIO\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
sys.path.insert(0, str(LANG / "tests"))  # rcode_normalizar (P69)
from rcode_normalizar import NormalizarLinhasR  # noqa: E402
CLI = LANG / "tests" / "cli"
REFERENCIA_PADRAO = Path(os.environ.get("TEMP", ".")) / "forja_b12_final" / "forja.exe"
CAMPOS_NOVOS = ("coluna", "valor", "pilha")
CAMPOS_DO_DRIVER = ("explicacao", "forma_certa", "spec")
# (comando, fixture, codigo R esperado ou None): o programa roda nos 2 backends com --rastreio e a saida tem de ser igual
PROGRAMAS = [
    ("run", "b12/indice_2_quadros.fj", "R0006"), ("run", "b12/recursao_indice.fj", "R0006"), ("run", "b13/recursao_funda.fj", "R0006"),
    ("run", "b13/fatia_aninhada.fj", "R0006"), ("run", "b12/assert_aninhado.fj", "R0003"), ("run", "panico.fj", "R0003"),
    ("run", "b13/estouro_aninhado.fj", "R0002"), ("run", "main_erro.fj", "R0004"), ("run", "modulos_panico.fj", "R0003"),
    ("run", "lista_indice_fora.fj", "R0006"), ("test", "b12/teste_indice.fj", "R0006"), ("test", "b13/testes_dois_falham.fj", "R0006"),
    ("test", "teste_falha.fj", None), ("test", "json_estouro.fj", "R0002"), ("test", "modulos_teste.fj", None), ("run", "vazamento.fj", "R0009"),
    ("run", "b12/sem_erro.fj", None), ("test", "tres_testes.fj", None),
]
# quem exige coluna, valor e pilha >= 2 quadros (M-D8 com --rastreio)
PROGRAMAS_M_D8 = [("run", "b12/indice_2_quadros.fj", "R0006", True), ("run", "b13/fatia_aninhada.fj", "R0006", True), ("run", "b12/assert_aninhado.fj", "R0003", False)]


def Rodar(Forja, Argumentos):
    """CompletedProcess com bytes; sempre em tests\\cli (o caminho do fixture sai relativo e igual para os dois forja)."""
    return subprocess.run([str(Forja), *Argumentos], capture_output=True, timeout=300, cwd=CLI, stdin=subprocess.DEVNULL)


def LinhasR(Bytes):
    """As linhas de R-code do stderr, como texto, na ordem."""
    Texto = Bytes.decode("utf-8", errors="replace")
    return [L for L in Texto.splitlines() if L.startswith('{"codigo": "R')]


def Campos(Linha):
    try:
        Dado = json.loads(Linha)
    except json.JSONDecodeError:
        return None
    return Dado if isinstance(Dado, dict) else None


# ---------- julgamentos puros (o selftest planta falhas neles) ----------

def SemPilhaDoR0009(Linha):
    """M6 P70 (D-M6-7): o R0009 do backend C fica com arquivo:linha, sem pilha; a forma_certa do x64 cita o proc da pilha. So neste codigo os 2 campos saem da comparacao."""
    Dado = Campos(Linha)
    if not Dado or Dado.get("codigo") != "R0009":
        return Linha
    Dado.pop("pilha", None)
    Dado.pop("forma_certa", None)
    return json.dumps(Dado, ensure_ascii=False)


def JulgarParidade(LinhasX64, LinhasC, Nome):
    """As linhas R dos 2 backends: mesma quantidade, mesmo texto; se diferem, diz o campo."""
    if len(LinhasX64) != len(LinhasC):
        return [f"{Nome}: {len(LinhasX64)} linhas R no x64 e {len(LinhasC)} no c"]
    Falhas = []
    for X64, C in zip(map(SemPilhaDoR0009, LinhasX64), map(SemPilhaDoR0009, LinhasC)):
        if X64 == C:
            continue
        A, B = Campos(X64) or {}, Campos(C) or {}
        Diferentes = sorted(Chave for Chave in set(A) | set(B) if A.get(Chave) != B.get(Chave))
        Ordem = list(A) != list(B) and not Diferentes
        Falhas.append(f"{Nome}: linha R difere em {Diferentes or ('ordem dos campos' if Ordem else 'texto')}: x64={X64[:240]!r} c={C[:240]!r}")
    return Falhas


def JulgarDriver(Linha):
    """O enriquecimento do driver (b10): explicacao, forma_certa e spec em toda linha R."""
    Dado = Campos(Linha) or {}
    return [f"falta {Campo}" for Campo in CAMPOS_DO_DRIVER if not isinstance(Dado.get(Campo), str) or not Dado[Campo].strip()]


def JulgarRastreio(Linha, ComValor):
    """Com --rastreio: coluna > 0, pilha >= 2 quadros completos (proc, arquivo, linha, coluna) e, no R0006, valor."""
    Dado = Campos(Linha) or {}
    Falhas = []
    if not isinstance(Dado.get("coluna"), int) or Dado["coluna"] < 1:
        Falhas.append(f"coluna {Dado.get('coluna')!r}")
    if ComValor and not isinstance(Dado.get("valor"), dict):
        Falhas.append(f"valor {Dado.get('valor')!r}")
    Pilha = Dado.get("pilha")
    if not isinstance(Pilha, list) or len(Pilha) < 2:
        return Falhas + [f"pilha com menos de 2 quadros: {Pilha!r:.160}"]
    for K, Quadro in enumerate(Pilha):
        if not isinstance(Quadro, dict) or set(Quadro) != {"proc", "arquivo", "linha", "coluna"}:
            Falhas.append(f"quadro {K} fora do formato: {Quadro!r:.120}")
    return Falhas


def JulgarSemCamposNovos(Linha):
    Dado = Campos(Linha) or {}
    return [f"campo {Campo} sem --rastreio" for Campo in CAMPOS_NOVOS if Campo in Dado]


def JulgarIguais(Obtido, Esperado, Nome):
    return [] if Obtido == Esperado else [f"{Nome}: difere ({Obtido!r:.200} x {Esperado!r:.200})"]


# ---------- casos ----------

def Resultado(Rodada):
    return (Rodada.returncode, Rodada.stdout, LinhasR(Rodada.stderr))


def CasoParidade(Forja):
    def Um(Programa):
        Comando, Fixture, Codigo = Programa
        X64 = Rodar(Forja, [Comando, "--backend", "x64", Fixture, "--rastreio"])
        C = Rodar(Forja, [Comando, "--backend", "c", Fixture, "--rastreio"])
        Falhas = JulgarIguais(C.returncode, X64.returncode, f"{Comando} {Fixture}: exit") + JulgarIguais(C.stdout, X64.stdout, f"{Comando} {Fixture}: stdout")
        Falhas += JulgarParidade(LinhasR(X64.stderr), LinhasR(C.stderr), f"{Comando} {Fixture}")
        if Codigo is not None and not any(f'"codigo": "{Codigo}"' in L for L in LinhasR(C.stderr)):
            Falhas.append(f"{Comando} {Fixture}: o backend c nao deu {Codigo}")
        return Falhas
    with ThreadPoolExecutor(max_workers=6) as Pool:
        Todas = list(Pool.map(Um, PROGRAMAS))
    return "paridade", [F for Falhas in Todas for F in Falhas]


def CasoMD8(Forja):
    Falhas = []
    for Backend in ("x64", "c"):
        for Comando, Fixture, _, _ in PROGRAMAS_M_D8:
            for L in LinhasR(Rodar(Forja, [Comando, "--backend", Backend, Fixture]).stderr):
                Falhas += [f"{Backend} {Fixture}: {F}" for F in JulgarDriver(L)]
        for Comando, Fixture, Codigo, ComValor in PROGRAMAS_M_D8:
            Rodada = Rodar(Forja, [Comando, "--backend", Backend, Fixture, "--rastreio"])
            Achadas = [L for L in LinhasR(Rodada.stderr) if f'"codigo": "{Codigo}"' in L]
            if len(Achadas) != 1:
                Falhas.append(f"{Backend} {Fixture}: {len(Achadas)} linhas {Codigo}")
                continue
            Falhas += [f"{Backend} {Fixture}: {F}" for F in JulgarRastreio(Achadas[0], ComValor) + JulgarDriver(Achadas[0])]
    return "m_d8", Falhas


def CasoCSemFlag(Forja):
    """Oraculo proprio (sem forja de referencia: politica de nao comparar com versao anterior): sem a flag a linha R do backend c nao tem coluna, valor nem pilha, e com a
    flag, tirados esses campos (rcode_normalizar, o ponto unico), stdout, exit e stderr sao os mesmos."""
    Falhas = []
    for Comando, Fixture, _ in PROGRAMAS:
        Sem = Rodar(Forja, [Comando, "--backend", "c", Fixture])
        Com = Rodar(Forja, [Comando, "--backend", "c", Fixture, "--rastreio"])
        Falhas += JulgarIguais((Com.returncode, Com.stdout, NormalizarLinhasR(Com.stderr)), (Sem.returncode, Sem.stdout, NormalizarLinhasR(Sem.stderr)), f"{Comando} {Fixture} com e sem a flag")
        for L in LinhasR(Sem.stderr):
            Falhas += [f"{Fixture}: {F}" for F in JulgarSemCamposNovos(L)]
    return "c_sem_flag", Falhas


def CorpusDoC():
    Raizes = [CLI, CLI / "b12", CLI / "b13", LANG / "examples"]
    return [str(P) for Raiz in Raizes if Raiz.is_dir() for P in sorted(Raiz.glob("*.fj"))]


def ProduzirC(Forja, Fonte, Extra=()):
    """(exit, stdout do forja, bytes do .c ou None) de `build --modo c`."""
    Saida = Path(os.environ.get("TEMP", ".")) / f"b13_c_{os.getpid()}_{abs(hash((str(Forja), Fonte, tuple(Extra)))) % 10**9}.c"
    Rodada = Rodar(Forja, ["build", "--modo", "c", Fonte, "-o", str(Saida), *Extra])
    Dados = Saida.read_bytes() if Saida.exists() else None
    Saida.unlink(missing_ok=True)
    return Rodada.returncode, Rodada.stdout, Dados


def CasoCTexto(Forja):
    """Oraculo proprio: `build --modo c` sem a flag e deterministico (2 builds do MESMO forja iguais) e nao traz a pilha sombra (FORJA_QUADRO, FORJA_RASTREIO)."""
    Corpus = CorpusDoC()

    def Um(Fonte):
        Novo, Outro = ProduzirC(Forja, Fonte), ProduzirC(Forja, Fonte)
        Falhas = [] if Novo == Outro else [f"{Path(Fonte).name}: dois builds do mesmo forja deram C (ou diagnostico) diferente"]
        if Novo[2] is not None and (b"FORJA_QUADRO" in Novo[2] or b"FORJA_RASTREIO" in Novo[2]):
            Falhas.append(f"{Path(Fonte).name}: o C sem a flag trouxe a pilha sombra")
        return Falhas
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Todas = list(Pool.map(Um, Corpus))
    Falhas = [F for Lista in Todas for F in Lista]
    Com = ProduzirC(Forja, str(CLI / "b12" / "indice_2_quadros.fj"), ["--rastreio"])
    Sem = ProduzirC(Forja, str(CLI / "b12" / "indice_2_quadros.fj"))
    if Com[2] is None or b"FORJA_QUADRO" not in Com[2] or b"FORJA_PILHA_PARTE" not in Com[2]:
        Falhas.append("build --modo c --rastreio nao trouxe a pilha sombra")
    if Sem[2] is not None and (b"FORJA_QUADRO" in Sem[2] or b"FORJA_RASTREIO" in Sem[2]):
        Falhas.append("build --modo c sem a flag trouxe a pilha sombra")
    return f"c_texto", Falhas + ([] if Corpus else ["corpus vazio"])


def CasoRelease(Forja):
    Falhas = []
    for Backend in ("c", "x64"):
        for Fixture in ("panico.fj", "b12/sem_erro.fj"):
            Sem = Rodar(Forja, ["run", "--release", "--backend", Backend, Fixture])
            Com = Rodar(Forja, ["run", "--release", "--backend", Backend, Fixture, "--rastreio"])
            Falhas += JulgarIguais((Com.returncode, Com.stdout, Com.stderr), (Sem.returncode, Sem.stdout, Sem.stderr), f"release {Backend} {Fixture}")
    return "release", Falhas


def CasoUso(Forja):
    Falhas = []
    for Pedido in (["check", "b12/sem_erro.fj", "--rastreio"], ["ast", "b12/sem_erro.fj", "--rastreio"], ["build", "b12/sem_erro.fj", "--rastreio", "--modo", "dll", "-o", "x.dll"]):
        Rodada = Rodar(Forja, Pedido)
        if Rodada.returncode != 2:
            Falhas.append(f"{' '.join(Pedido)}: exit {Rodada.returncode}, esperado 2")
    return "uso", Falhas


def RodarCasos(Forja, Referencia):
    Casos = [CasoParidade, CasoMD8, CasoCSemFlag, CasoCTexto, CasoRelease, CasoUso]  # Referencia: sem uso desde o P69 (oraculo proprio)
    Vermelhos = 0
    for Caso in Casos:
        try:
            Id, Falhas = Caso(Forja)
        except Exception as Erro:  # um caso que quebra e vermelho, nao derruba os outros
            Id, Falhas = getattr(Caso, "__name__", "caso"), [f"excecao: {type(Erro).__name__}: {Erro}"]
        Vermelhos += bool(Falhas)
        print(json.dumps({"id": Id, "valor": "ok" if not Falhas else f"{len(Falhas)} falhas", "meta": "ok", "verde": not Falhas, "falhas": Falhas[:12]}, ensure_ascii=False), flush=True)
    return Vermelhos


def Selftest():
    """Cada julgamento aceita o certo e recusa o errado plantado."""
    Quadro = {"proc": "pegar", "arquivo": "a.fj", "linha": 2, "coluna": 13}
    Main = {"proc": "main", "arquivo": "a.fj", "linha": 7, "coluna": 11}
    Bom = {"codigo": "R0006", "tipo": "runtime", "arquivo": "a.fj", "linha": 2, "coluna": 13, "causa": "c", "valor": {"indice": 7, "tamanho": 3}, "pilha": [Quadro, Main],
           "explicacao": "e", "forma_certa": "f", "spec": "s"}
    Texto = json.dumps(Bom)
    SemPilha = json.dumps({Chave: Valor for Chave, Valor in Bom.items() if Chave != "pilha"})
    Reordenado = json.dumps({Chave: Bom[Chave] for Chave in reversed(list(Bom))})
    Casos = {
        "paridade igual": not JulgarParidade([Texto], [Texto], "x"),
        "paridade sem pilha no c": bool(JulgarParidade([Texto], [SemPilha], "x")),
        "paridade conta de linhas": bool(JulgarParidade([Texto], [], "x")),
        "paridade campo diferente": "valor" in JulgarParidade([Texto], [json.dumps(dict(Bom, valor={"indice": 7, "tamanho": 4}))], "x")[0],
        "paridade ordem dos campos": "ordem" in JulgarParidade([Texto], [Reordenado], "x")[0],
        "driver ok": not JulgarDriver(Texto),
        "driver sem spec": bool(JulgarDriver(json.dumps(dict(Bom, spec="")))),
        "rastreio ok": not JulgarRastreio(Texto, True),
        "rastreio sem coluna": bool(JulgarRastreio(json.dumps({Chave: Valor for Chave, Valor in Bom.items() if Chave != "coluna"}), True)),
        "rastreio 1 quadro": bool(JulgarRastreio(json.dumps(dict(Bom, pilha=[Quadro])), True)),
        "rastreio sem valor": bool(JulgarRastreio(json.dumps({Chave: Valor for Chave, Valor in Bom.items() if Chave != "valor"}), True)),
        "rastreio R0003 sem valor ok": not JulgarRastreio(json.dumps({Chave: Valor for Chave, Valor in Bom.items() if Chave != "valor"}), False),
        "rastreio quadro sem coluna": bool(JulgarRastreio(json.dumps(dict(Bom, pilha=[Quadro, {"proc": "m", "arquivo": "a.fj", "linha": 1}])), True)),
        "sem campos novos ok": not JulgarSemCamposNovos(SemPilha.replace('"coluna": 13, ', "").replace(', "valor": {"indice": 7, "tamanho": 3}', "")),
        "sem campos novos com pilha": bool(JulgarSemCamposNovos(Texto)),
        "iguais ok": not JulgarIguais(1, 1, "x"),
        "iguais difere": bool(JulgarIguais(b"a", b"b", "x")),
        "linhas R so as de R-code": LinhasR(b'x\n{"codigo": "R0003", "a": 1}\n{"codigo": "E0001"}\n') == ['{"codigo": "R0003", "a": 1}'],
    }
    Falhas = [Nome for Nome, Ok in Casos.items() if not Ok]
    print(json.dumps({"id": "selftest", "valor": f"{len(Casos) - len(Falhas)}/{len(Casos)}", "meta": f"{len(Casos)}/{len(Casos)}", "verde": not Falhas, "falhas": Falhas}))
    return 1 if Falhas else 0


def Principal():
    Parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Parser.add_argument("--forja", help="forja.exe a verificar")
    Parser.add_argument("--referencia", default=str(REFERENCIA_PADRAO), help="forja.exe do b12, para o byte a byte sem a flag")
    Parser.add_argument("--esperar-vermelho", action="store_true", help="exit 0 so se algum caso ficar vermelho (prova de que a regua discrimina)")
    Parser.add_argument("--selftest", action="store_true")
    Argumentos = Parser.parse_args()
    if Argumentos.selftest:
        return Selftest()
    if not Argumentos.forja:
        Parser.error("--forja e obrigatorio")
    Vermelhos = RodarCasos(Path(Argumentos.forja), Path(Argumentos.referencia))
    if Argumentos.esperar_vermelho:
        return 0 if Vermelhos else 1
    return 1 if Vermelhos else 0


if __name__ == "__main__":
    sys.exit(Principal())
