# -*- coding: utf-8 -*-
"""sondas_compilador.py - as metricas M que o forja.exe JA permite medir hoje, sem modelo (P-IA1-a07).

Versao 0.2.2
Changelog:
  0.2.2 (16/11/2026) M6e P93g: E0913 (ponteiro para dentro de lista/map apos a mutacao) entra na LISTA_CORRIGIVEL (dica em todo caso, correcao provavel quando o indice e literal/let/parametro).
  0.2.1 (14/11/2026) Portao Z z5: M-D3 conta como "existente" os nomes dos modulos std importados (TextoComIrmaos); M-D1 so conta correcao com edicoes e
        confianca certa/provavel (TemCorrecaoUtil); M-D11 passa a PacoteCompleto (mesmos campos de antes; o compilador do z5 emite o pacote tambem sem sugestao).
  0.2.0 (11/11/2026) P-IA1-b08: M-D3 completa (nome fora do arquivo + por codigo): TextoComIrmaos, NomesForaDoArquivo, ResumirForma; a sonda guarda o "texto" do fixture.
  0.1.0 (11/11/2026) primeira versao. Sonda os err_*.fj do repositorio (check + build nos 2 backends) e calcula M-D1 (sugestao
        nas ocorrencias da LISTA CORRIGIVEL), M-D3 (parte 1: forma_certa == texto do catalogo), M-D5 (paridade dos 2 backends e
        stderr de build so com linhas JSON) e M-D11 (campos 1 a 5 do pacote pronto); M-D6 (determinismo: ~60 casos de check,
        build, run e test rodados 2 vezes). Hoje o compilador nao tem esses campos: o numero sai vermelho de verdade.

Usado por metricas_mecanicas.py. Escrita: so diretorios temporarios proprios. Reversao: apagar este arquivo.
Depende de: Python 3.9+, gcc no PATH (backend c), forja.exe (copia congelada).
"""
import hashlib
import json
import re
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path

from ia_comum import Metrica

LANG = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
EXEMPLOS = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\examples")
PASTAS_DE_FIXTURES = ("sintaxe", "semantica", "dourado", "regressao")
LISTA_CORRIGIVEL = {"E0001", "E0101", "E0102", "E0201", "E0202", "E0310", "E0311", "E0502", "E0707", "E0708", "E0709", "E1003",
                    "E1001", "E1006", "E0110", "E0307", "F0001", "E0913"}
CASOS_D6 = 60
TRABALHADORES = 8


def Executar(Comando, Cwd, Limite=60):
    """(rc, stdout, stderr) em texto; tempo estourado vira rc=None."""
    try:
        P = subprocess.run(Comando, cwd=str(Cwd), capture_output=True, timeout=Limite, stdin=subprocess.DEVNULL)
    except (subprocess.TimeoutExpired, OSError) as Erro:
        return None, "", "falhou: %s" % type(Erro).__name__
    return P.returncode, P.stdout.decode("utf-8", errors="replace"), P.stderr.decode("utf-8", errors="replace")


def LinhasJson(Texto):
    """Objetos JSON das linhas que comecam com '{'; (objetos, quantas linhas '{' nao eram JSON valido)."""
    Objetos, Ruins = [], 0
    for Linha in Texto.splitlines():
        if Linha.lstrip().startswith("{"):
            try:
                Objetos.append(json.loads(Linha))
            except ValueError:
                Ruins += 1
    return Objetos, Ruins


def Fixtures(Prefixo):
    Achados = []
    for Pasta in PASTAS_DE_FIXTURES:
        Achados += sorted((LANG / "tests" / Pasta).glob(Prefixo + "*.fj"))
    return Achados


def CodigoDoNome(Arquivo):
    Achado = re.match(r"err_([A-Z]\d{4})_", Arquivo.name)
    return Achado.group(1) if Achado else None


def Triplas(Objetos):
    return [(O.get("codigo"), O.get("linha"), O.get("coluna")) for O in Objetos if "codigo" in O]


def SondarBackend(Forja, Arquivo, Backend):
    with tempfile.TemporaryDirectory(prefix="a07_sonda_") as Tmp:
        Rc, Out, Err = Executar([Forja, "build", "--backend", Backend, Arquivo.name, "-o", str(Path(Tmp) / "x.exe")], Arquivo.parent)
    Objetos, _ = LinhasJson(Out + "\n" + Err)
    return {"triplas": Triplas(Objetos), "stderr_ruim": LinhasJson(Err)[1] + sum(1 for L in Err.splitlines() if L.strip() and not L.lstrip().startswith("{"))}


def SondarFixture(Forja, Arquivo):
    Rc, Out, Err = Executar([Forja, "check", Arquivo.name], Arquivo.parent)
    Objetos, _ = LinhasJson(Out + "\n" + Err)
    return {"arquivo": Arquivo.name, "codigo": CodigoDoNome(Arquivo), "diags": [O for O in Objetos if "codigo" in O], "texto": TextoComIrmaos(Arquivo),
            "c": SondarBackend(Forja, Arquivo, "c"), "x64": SondarBackend(Forja, Arquivo, "x64")}


def SondarTodas(Forja):
    with ThreadPoolExecutor(max_workers=TRABALHADORES) as Pool:
        return list(Pool.map(partial(SondarFixture, str(Forja)), Fixtures("err_")))


def Percentual(Parte, Total):
    return round(100.0 * Parte / Total, 1) if Total else 0.0


def TemCorrecaoUtil(Diag):
    """z5: correcao so conta com ao menos uma edicao (arquivo/linha/coluna/novo) e confianca certa ou provavel; texto solto de 'sugestao' nao basta."""
    Edicoes = Diag.get("correcao")
    return (isinstance(Edicoes, list) and bool(Edicoes) and all(isinstance(E, dict) and "linha" in E and "novo" in E for E in Edicoes)
            and Diag.get("confianca") in ("certa", "provavel"))


def PacoteCompleto(Diag):
    """Campos 1 a 5 do pacote em todo diagnostico (z5: o sem sugestao sai com correcao [] e confianca "so_dica", nao sem os campos)."""
    Campos = ("arquivo", "linha", "coluna", "coluna_fim", "trecho", "causa", "correcao", "confianca", "relacionados")
    return all(C in Diag for C in Campos) and ("spec" in Diag or "spec_ref" in Diag)


def MetricaD1(Sondas):
    Ocorrencias = [D for S in Sondas for D in S["diags"] if D.get("codigo") in LISTA_CORRIGIVEL]
    Com = sum(1 for D in Ocorrencias if TemCorrecaoUtil(D))
    Pct = Percentual(Com, len(Ocorrencias))
    return Metrica("M-D1", {"com_sugestao": Com, "ocorrencias": len(Ocorrencias), "pct": Pct}, ">= 90%", Pct >= 90.0 if Ocorrencias else None)


def MetricaD11(Sondas):
    Todos = [D for S in Sondas for D in S["diags"]]
    Completos = sum(1 for D in Todos if PacoteCompleto(D))
    Pct = Percentual(Completos, len(Todos))
    return Metrica("M-D11", {"completos": Completos, "diagnosticos": len(Todos), "pct": Pct}, "100% dos err_ com campos 1 a 5", Pct == 100.0 if Todos else None)


def TextosDoCatalogo(Forja, Codigos):
    """{codigo: (explicacao, forma_certa)} do `forja explicar`; (None, None) se o codigo nao existe."""
    Textos = {}
    for Codigo in sorted(Codigos):
        Objetos, _ = LinhasJson(Executar([str(Forja), "explicar", Codigo], LANG)[1])
        Textos[Codigo] = (Objetos[0].get("explicacao"), Objetos[0].get("forma_certa")) if Objetos else (None, None)
    return Textos


# ---------- M-D3: forma por ocorrencia ----------
LIMITE_FORMA_PCT = 40.0
MIN_OCORRENCIAS_DO_GRUPO = 5  # o grupo de textos identicos so conta em codigo com 5+ ocorrencias (com 1 ou 2, "100% identicas" nao diz nada)
ASPAS = re.compile(r"'([^']*)'")
IDENTIFICADOR = re.compile(r"[A-Za-z_]\w*")
# palavras que a Forja define (palavras-chave, tipos, embutidos, nomes de argumento e do 'em') e marcadores genericos (T, E...) que uma forma pode citar entre aspas sem estarem no arquivo
PALAVRAS_DA_FORJA = set("""as or_else try print let var return int bool str cstr rawptr f32 f64 i8 i16 i32 i64 u8 u16 u32 u64 uint true false none and or not if else for in while
switch case default break continue defer proc struct enum union unit pub export import foreign link test expect const new make delete append len assert panic format format_c std
cond msg em heap frame escopo Arena Pool para transmute map soa bit_set matrix quat dynamic item key value cap lo hi main T E K V S N R C""".split())


def TextoComIrmaos(Arquivo):
    """O fonte do caso mais o dos .fj da mesma pasta que ele importa pelo nome (multi-modulo): os nomes que existem no projeto do caso."""
    Texto = Arquivo.read_text(encoding="utf-8", errors="replace")
    for Importado in re.findall(r'import\s+"([^"]+)"', Texto):
        if Importado.startswith("std:"):  # 0.2.1: os nomes de parametro dos modulos da std (fachada e subpasta) tambem existem no projeto do caso
            Irmaos = [LANG / "std" / (Importado[4:] + ".fj")] + sorted((LANG / "std" / Importado[4:]).glob("*.fj"))
        else:
            Irmaos = [Arquivo.parent / (Importado if Importado.endswith(".fj") else Importado + ".fj")]
        for Irmao in Irmaos:
            if Irmao.is_file():
                Texto += "\n" + Irmao.read_text(encoding="utf-8", errors="replace")
    return Texto


def NomesForaDoArquivo(Texto, Diag):
    """Palavras entre aspas simples da explicacao/forma_certa que nao estao no arquivo, na causa nem na lista da Forja (ex.: Grifo)."""
    Conhecido = set(IDENTIFICADOR.findall(Texto)) | set(IDENTIFICADOR.findall(Diag.get("causa", ""))) | PALAVRAS_DA_FORJA
    Achados = []
    for Campo in ("explicacao", "forma_certa"):
        for Trecho in ASPAS.findall(Diag.get(Campo, "")):
            Achados += [P for P in IDENTIFICADOR.findall(Trecho) if P not in Conhecido]
    return Achados


def IguaisAoCatalogo(Diags, Textos):
    """Quantas ocorrencias tem a explicacao OU a forma_certa igual ao texto do catalogo do codigo."""
    return sum(1 for D in Diags if D.get("explicacao") == Textos[0] or D.get("forma_certa") == Textos[1])


def ResumirForma(Achados, Catalogos):
    """Medidas da M-D3 sobre [(texto do .fj, diagnostico)] e {codigo: (explicacao, forma)} do catalogo."""
    Total = len(Achados)
    PorCodigo = {}
    for _, D in Achados:
        PorCodigo.setdefault(D["codigo"], []).append(D)
    Global = sum(1 for _, D in Achados if D.get("forma_certa") is not None and D.get("forma_certa") == Catalogos.get(D["codigo"], (None, None))[1])
    PiorCatalogo, PiorGrupo = ("-", 0.0), ("-", 0.0)
    for Codigo, Diags in PorCodigo.items():
        Pct = Percentual(IguaisAoCatalogo(Diags, Catalogos.get(Codigo, (None, None))), len(Diags))
        if Pct > PiorCatalogo[1]:
            PiorCatalogo = (Codigo, Pct)
        Grupos = {}
        for D in Diags:
            Chave = (D.get("explicacao"), D.get("forma_certa"))
            Grupos[Chave] = Grupos.get(Chave, 0) + 1
        PctGrupo = Percentual(max(Grupos.values()), len(Diags))
        if len(Diags) >= MIN_OCORRENCIAS_DO_GRUPO and PctGrupo > PiorGrupo[1]:
            PiorGrupo = (Codigo, PctGrupo)
    Inventados = []
    for Texto, D in Achados:
        Fora = NomesForaDoArquivo(Texto, D)
        if Fora:
            Inventados.append((D.get("codigo"), D.get("linha"), Fora))
    Vazios = [(D.get("codigo"), D.get("linha")) for _, D in Achados if not D.get("explicacao") or not D.get("forma_certa")]
    return {"ocorrencias": Total, "codigos": len(PorCodigo), "pct_forma_igual_ao_catalogo": Percentual(Global, Total), "pior_codigo_igual_ao_catalogo": PiorCatalogo,
            "pior_grupo_identico": PiorGrupo, "com_nome_fora_do_arquivo": len(Inventados), "exemplos_de_nome_fora": Inventados[:3], "sem_explicacao_ou_forma": len(Vazios)}


def MetricaD3(Forja, Sondas):
    Achados = [(S.get("texto", ""), D) for S in Sondas for D in S["diags"]]
    Resumo = ResumirForma(Achados, TextosDoCatalogo(Forja, {D["codigo"] for _, D in Achados}))
    Verde = (Resumo["ocorrencias"] > 0 and Resumo["pct_forma_igual_ao_catalogo"] <= LIMITE_FORMA_PCT and Resumo["pior_codigo_igual_ao_catalogo"][1] <= LIMITE_FORMA_PCT
             and Resumo["pior_grupo_identico"][1] <= LIMITE_FORMA_PCT and Resumo["com_nome_fora_do_arquivo"] == 0 and Resumo["sem_explicacao_ou_forma"] == 0)
    return Metrica("M-D3", Resumo, "<= 40% iguais ao catalogo (total e por codigo), nenhum codigo com 5+ ocorrencias com >40% de textos identicos, 0 nome fora do arquivo", Verde)


def MetricaD5(Sondas):
    Diferentes = [S["arquivo"] for S in Sondas if S["c"]["triplas"] != S["x64"]["triplas"]]
    SujasNoStderr = [S["arquivo"] for S in Sondas if S["c"]["stderr_ruim"] or S["x64"]["stderr_ruim"]]
    Valor = {"fixtures": len(Sondas), "triplas_diferentes": len(Diferentes), "com_stderr_nao_json": len(SujasNoStderr), "exemplos": (Diferentes + SujasNoStderr)[:3]}
    return Metrica("M-D5", Valor, "100% iguais nos 2 backends e stderr so JSON", not Diferentes and not SujasNoStderr if Sondas else None)


# ---------- M-D6: determinismo ----------

def Espalhar(Itens, Quantos):
    """Quantos itens espalhados com passo fixo (deterministico)."""
    if len(Itens) <= Quantos:
        return list(Itens)
    Passo = len(Itens) / Quantos
    return [Itens[int(I * Passo)] for I in range(Quantos)]


def ProgramasRodaveis():
    Candidatos = sorted(EXEMPLOS.glob("0*.fj")) + sorted((LANG / "tests" / "semantica").glob("ok_*.fj"))
    return [P for P in Candidatos if "proc main" in P.read_text(encoding="utf-8", errors="replace")]


def ProgramasComTeste():
    Candidatos = sorted(EXEMPLOS.glob("0*.fj")) + sorted((LANG / "tests" / "semantica").glob("ok_*.fj"))
    return [P for P in Candidatos if re.search(r"^test\s", P.read_text(encoding="utf-8", errors="replace"), re.M)]


def RodarSimples(Forja, Verbo, Arquivo):
    Argumentos = {"check": ["check"], "run": ["run"], "test": ["test"]}[Verbo]
    return Executar([Forja] + Argumentos + [Arquivo.name], Arquivo.parent)


def RodarBuild(Forja, Backend, Arquivo):
    with tempfile.TemporaryDirectory(prefix="a07_d6_") as Tmp:
        Exe = Path(Tmp) / ("x.c" if Backend == "c" else "x.exe")
        Modo = ["--modo", "c"] if Backend == "c" else []
        Rc, Out, Err = Executar([Forja, "build"] + Modo + [Arquivo.name, "-o", str(Exe)], Arquivo.parent)
        Sha = hashlib.sha256(Exe.read_bytes()).hexdigest() if Exe.is_file() else None
    return Rc, Out.replace(Tmp, "<tmp>"), Err.replace(Tmp, "<tmp>"), Sha


def CasosDeterminismo(Forja):
    """Lista de (rotulo, funcao sem argumento): check de err_/ok_, build nos 2 backends, run e test."""
    Roda, Teste = ProgramasRodaveis(), ProgramasComTeste()
    Casos = [("test " + P.name, partial(RodarSimples, Forja, "test", P)) for P in Espalhar(Teste, 5)]
    Casos += [("run " + P.name, partial(RodarSimples, Forja, "run", P)) for P in Espalhar(Roda, 10)]
    for Backend in ("x64", "c"):
        Casos += [("build-%s %s" % (Backend, P.name), partial(RodarBuild, Forja, Backend, P)) for P in Espalhar(Roda, 6)]
    Casos += [("check " + P.name, partial(RodarSimples, Forja, "check", P)) for P in Espalhar(Fixtures("ok_"), 10)]
    Faltam = CASOS_D6 - len(Casos)
    return Casos + [("check " + P.name, partial(RodarSimples, Forja, "check", P)) for P in Espalhar(Fixtures("err_"), max(Faltam, 0))]


def DoisRodadas(Caso):
    Rotulo, Funcao = Caso
    return Rotulo, Funcao() == Funcao()


def MetricaD6(Forja):
    Casos = CasosDeterminismo(str(Forja))
    with ThreadPoolExecutor(max_workers=TRABALHADORES) as Pool:
        Resultados = list(Pool.map(DoisRodadas, Casos))
    Diferentes = [R for R, Igual in Resultados if not Igual]
    Valor = {"casos": len(Resultados), "diferentes": len(Diferentes), "exemplos": Diferentes[:3]}
    return Metrica("M-D6", Valor, "100% de ~60 casos byte a byte iguais (check, build, run, test; 2 backends)", not Diferentes and len(Resultados) >= 50)
