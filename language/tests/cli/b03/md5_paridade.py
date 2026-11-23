# -*- coding: utf-8 -*-
"""M-D5 (IA1-b03): paridade dos 2 backends no diagnostico.
 (1) em 100% dos err_*.fj de language\\tests (e das fixtures de b03) o `build --backend c` e o `build --backend x64` dao os mesmos (codigo, linha, coluna);
 (2) o stderr de TODO build (os 2 backends) so tem linhas JSON validas (ou nada);
 (3) link inexistente, simbolo inexistente e alias inexistente: o JSON do x64 e IGUAL ao congelado esperados\\md5_x64_<fixture>.json, menos "alvo", "etapa" e "procurei_em" (e exit 1, stdout);
 (4) `check` e `build` com import inexistente: stderr vazio, uma linha JSON E1001 em stdout, tambem com --resumo;
 (5) ligacao que so o ld recusa (biblioteca estatica corrompida): B0004 em stderr, UMA linha JSON, com ferramenta.nome e ferramenta.saida (texto do ld) e nada solto.
Uso: python -I md5_paridade.py --forja <abs forja.exe> [--saida <pasta de trabalho>] [--congelar | --contra-congelado]
--congelar (0.21.0 P3): grava o lado C (onde C == x64) em rapido\\oraculo_congelado_g1_cli.json; o C roda pelo gaveta\\g1\\forja_g1.exe, o x64 pelo --forja.
--contra-congelado: roda so o x64 e compara com o lado C congelado (sem gcc, sem forja_g1); chave ausente = FALHA.
Versao: 1.2.4 (b03). Changelog: 1.2.4 - L4a: err_E0907_const_proc_assinatura entra em POSTERIORES_AO_CONGELAMENTO. 1.2.3 - L4b: err_E1007_load_absoluto entra em POSTERIORES_AO_CONGELAMENTO. 1.2.2 - G3 L1a: as 11 err_*_g3_* (modulo de pasta) entram em POSTERIORES_AO_CONGELAMENTO. 1.2.1 - 0.21.2 R10c: lista POSTERIORES_AO_CONGELAMENTO tira da varredura as err_*.fj criadas depois do congelamento do lado C (sem chave no oraculo selado). 1.2.0 -0.21.0 F4 passo 3: o caso (3) sai do lado C (o backend C foi aposentado): compara o x64 com esperados\\md5_x64_<fixture>.json (congelado so x64). Os casos (1), (2) e (4) seguem no oraculo congelado (--contra-congelado) sem mudanca.
           1.1.1 - procurei_em (pastas de busca, depende do PATH) sai da comparacao C x x64; a presenca continua exigida. 1.1.0 - 0.21.0 P3: --congelar e --contra-congelado (classe cli); o item 5 (ld) so tem lado C, o x64 da X0105.
           1.0.0 - b03: primeira versao. Saida: uma linha por falha e, no fim, 'M-D5: N casos, K falhas'."""
import argparse
import json
import subprocess
import sys
import tempfile
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "nativo" / "rapido"))
import oraculo_congelado as Oraculo  # noqa: E402

TESTES = Path(r"C:\Users\USUARIO\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language\tests")
FIXTURES = Path(__file__).resolve().parent / "fixtures"
PASTA_ESPERADOS = Path(__file__).resolve().parents[1] / "esperados"  # tests\cli\esperados: o x64 congelado do caso (3)
CAMPOS_DO_X64 = ("alvo", "etapa")
CAMPOS_DO_AMBIENTE = ("procurei_em",)  # lista de pastas de busca: muda com o PATH de quem roda, nao e paridade C x x64
JOBS = 12
ORACULO_CLI = Path(__file__).resolve().parents[2] / "nativo" / "rapido" / "oraculo_congelado_g1_cli.json"
CLASSE_CLI = "cli"
CONGELAR = "congelar"
CONTRA = "contra"
MODO = None  # None = roda o forja como antes; CONGELAR ou CONTRA (ver IniciarCongelado)
ITENS = {}  # so no CONTRA: os itens congelados da classe cli
TravaCongelado = threading.Lock()
GravadasChaves, Divergencias, Ausentes = set(), [], []


def Rodar(Forja, Argumentos, Pasta):
    R = subprocess.run([Forja] + Argumentos, capture_output=True, timeout=180, cwd=str(Pasta))
    return R.returncode, R.stdout.decode("utf-8", "replace"), R.stderr.decode("utf-8", "replace")


def IniciarCongelado(Modo):
    """CONGELAR: recusa banco selado ou cabecalho mudado (o JSON e da classe inteira: nao apaga). CONTRA: confere selo/cabecalho e le os itens, sem gcc."""
    global MODO, ITENS
    Banco = Oraculo.BancoOraculo(ORACULO_CLI, Oraculo.FORJA_G1)
    if Modo == CONTRA:
        Banco.ExigirCabecalho()
        ITENS = Banco.Itens
    elif Banco.Selado:
        raise Oraculo.ErroOraculo("oraculo cli selado: nao se recongela (apague o JSON para refazer)")
    elif Banco.CabecalhoDivergente():
        raise Oraculo.ErroOraculo("cabecalho do oraculo cli mudou: apague o JSON e recongele")
    MODO = Modo


def ChaveCli(Fonte, Argumentos):
    return Oraculo.ChaveDe(Fonte, "cli", tuple(Argumentos), None, CLASSE_CLI)


def SemSaida(Argumentos):
    """Tira o `-o <caminho>`: a pasta de saida muda a cada rodada e nao entra na chave."""
    if "-o" not in Argumentos:
        return list(Argumentos)
    Posicao = Argumentos.index("-o")
    return list(Argumentos[:Posicao] + Argumentos[Posicao + 2:])


def FonteDoPedido(Argumentos, Pasta):
    """O .fj do pedido, no cwd dele: a chave le o arquivo e os modulos que importa."""
    return Path(Pasta) / next(Argumento for Argumento in Argumentos if Argumento.endswith(".fj"))


def BackendDoPedido(Argumentos):
    return Argumentos[Argumentos.index("--backend") + 1] if "--backend" in Argumentos else None


def TrocarBackend(Argumentos, Backend):
    Saida = list(Argumentos)
    Saida[Saida.index("--backend") + 1] = Backend
    return Saida


def VerDiag(Rodada):
    """O que a classe compara no diagnostico: exit e (codigo, linha, coluna) de cada linha JSON do stdout."""
    Rc, Out, _ = Rodada
    Dados, _ = LinhasJson(Out)
    return Rc, ChaveDeDiag(Dados)


def VerJsonSemAlvo(Rodada):
    """O que o item 3 compara: as linhas JSON do stdout iguais, menos alvo e etapa."""
    Rc, Out, _ = Rodada
    Dados, _ = LinhasJson(Out)
    return Rc, [SemAlvoEEtapa(Dado) for Dado in Dados]


def RegistroDe(Rodada):
    Rc, Out, Err = Rodada
    return {"invalido": False, "origem": "forja_g1", "exit": Rc, "stdout": Out, "stderr": Err}


def GravarNoCongelado(Chave, Rodada):
    """CONGELAR: grava o lado C; so e chamado quando ele bate com o x64. O JSON e lido e gravado a cada item, dentro da trava."""
    with TravaCongelado:
        Oraculo.BancoOraculo(ORACULO_CLI, Oraculo.FORJA_G1).Registrar(Chave, RegistroDe(Rodada))
        GravadasChaves.add(Chave)


def LerCongelado(Chave, Argumentos):
    """CONTRA: a rodada do lado C congelado; chave ausente = FALHA (sentinela exit -1, nada e gravado)."""
    Registro = ITENS.get(Chave)
    if Registro is None:
        with TravaCongelado:
            Ausentes.append(f"chave ausente no oraculo cli: {' '.join(Argumentos)}")
        return -1, "", "CONGELADO-AUSENTE"
    return Registro["exit"], Registro["stdout"], Registro["stderr"]


def RodarLadoC(Forja, Argumentos, Pasta, Ver=VerDiag):
    """Lado C de um pedido `--backend c`: sem congelado roda o forja; CONTRA le o JSON (nada de gcc); CONGELAR roda o C pelo forja_g1 e grava so se o x64 der o mesmo Ver."""
    Chave = ChaveCli(FonteDoPedido(Argumentos, Pasta), SemSaida(Argumentos))
    if MODO == CONTRA:
        return LerCongelado(Chave, Argumentos)
    if MODO is None:
        return Rodar(Forja, Argumentos, Pasta)
    Lado = Rodar(str(Oraculo.FORJA_G1), Argumentos, Pasta)
    X64 = Rodar(Forja, TrocarBackend(Argumentos, "x64"), Pasta)
    if Ver(Lado) != Ver(X64):
        with TravaCongelado:
            Divergencias.append(f"{' '.join(SemSaida(Argumentos))}: C {Ver(Lado)!r:.200} != x64 {Ver(X64)!r:.200}")
        return Lado
    GravarNoCongelado(Chave, Lado)
    return Lado


def RodarLadoCSo(Forja, Argumentos, Pasta):
    """Lado C sem gemeo x64 (item 5): CONTRA le o JSON; CONGELAR roda o forja_g1 e quem grava e o chamador, depois de conferir o formato."""
    if MODO == CONTRA:
        return LerCongelado(ChaveCli(FonteDoPedido(Argumentos, Pasta), SemSaida(Argumentos)), Argumentos)
    return Rodar(str(Oraculo.FORJA_G1) if MODO == CONGELAR else Forja, Argumentos, Pasta)


def RodarPedido(Forja, Argumentos, Pasta, Ver=VerDiag):
    """Pedido do forja: o `--backend c` passa pelo lado C (congelado); o resto roda o forja como antes."""
    if BackendDoPedido(Argumentos) == "c":
        return RodarLadoC(Forja, Argumentos, Pasta, Ver)
    return Rodar(Forja, Argumentos, Pasta)


def LinhasJson(Texto):
    """Cada linha nao vazia como dict; devolve (lista, linhas que nao sao JSON)."""
    Boas, Ruins = [], []
    for L in Texto.splitlines():
        if not L.strip():
            continue
        try:
            Boas.append(json.loads(L))
        except json.JSONDecodeError:
            Ruins.append(L[:120])
    return Boas, Ruins


def ChaveDeDiag(Dados):
    return [(D.get("codigo"), D.get("linha"), D.get("coluna")) for D in Dados]


def ConferirParidadeDoFonte(Forja, Fonte, Saida):
    """Caso (1) e (2) de um .fj: os 2 backends dao os mesmos (codigo, linha, coluna) e nenhum escreve texto solto no stderr."""
    Falhas, Chaves = [], {}
    for Backend in ("c", "x64"):
        Exe = Saida / ("%s_%s.exe" % (Fonte.stem, Backend))
        Rc, Out, Err = RodarPedido(Forja, ["build", "--backend", Backend, Fonte.name, "-o", str(Exe)], Fonte.parent)
        Dados, Ruins = LinhasJson(Out)
        _, RuinsErr = LinhasJson(Err)
        if RuinsErr:
            Falhas.append("%s %s: stderr com texto solto %r" % (Fonte.name, Backend, RuinsErr[:1]))
        if Ruins:
            Falhas.append("%s %s: stdout com texto solto %r" % (Fonte.name, Backend, Ruins[:1]))
        Chaves[Backend] = (Rc, ChaveDeDiag(Dados))
    if Chaves["c"] != Chaves["x64"]:
        Falhas.append("%s: c %r != x64 %r" % (Fonte.name, Chaves["c"], Chaves["x64"]))
    return Falhas


def SemAlvoEEtapa(Dados):
    return {K: V for K, V in Dados.items() if K not in CAMPOS_DO_X64 and K not in CAMPOS_DO_AMBIENTE}


def ConferirIgualMenosAlvo(Forja, Nome, Saida):
    """Caso (3) (0.21.0 F4 passo 3: so o x64, o lado C saiu): JSON do x64 igual ao congelado esperados\\md5_x64_<fixture>.json, menos alvo/etapa/procurei_em; exit 1; stderr vazio."""
    Rc, Out, Err = Rodar(Forja, ["build", "--backend", "x64", Nome, "-o", str(Saida / "x64.exe")], FIXTURES)
    Dados, Ruins = LinhasJson(Out)
    if Rc != 1 or len(Dados) != 1 or Ruins or Err.strip():
        return ["%s x64: exit %d, %d linha(s) JSON, stderr %r" % (Nome, Rc, len(Dados), Err[:80])]
    Falhas = []
    if Dados[0].get("codigo") not in ("X0101", "X0102") or not Dados[0].get("procurei_em"):
        Falhas.append("%s: esperava X0101/X0102 com procurei_em, veio %s" % (Nome, Dados[0].get("codigo")))
    Congelado = json.loads((PASTA_ESPERADOS / ("md5_x64_" + Path(Nome).stem + ".json")).read_text(encoding="utf-8"))
    if SemAlvoEEtapa(Dados[0]) != Congelado:
        Falhas.append("%s: JSON do x64 difere do congelado: %s" % (Nome, json.dumps(SemAlvoEEtapa(Dados[0]), ensure_ascii=False)[:200]))
    return Falhas


def ConferirImportInexistente(Forja, Saida):
    """Caso (4): E1001 em JSON no stdout e NADA no stderr (comandos e modos que leem o import)."""
    Falhas = []
    Pedidos = [["check"], ["check", "--resumo"], ["build", "--backend", "c", "-o", str(Saida / "i.exe")], ["build", "--backend", "x64", "-o", str(Saida / "i2.exe")],
               ["build", "--resumo", "--backend", "c", "-o", str(Saida / "i3.exe")]]
    for P in Pedidos:
        Args = [P[0]] + P[1:] + ["import_inexistente.fj"] if P[0] == "check" else P + ["import_inexistente.fj"]
        Rc, Out, Err = RodarPedido(Forja, Args, FIXTURES)
        Dados, Ruins = LinhasJson(Out)
        if Err.strip() or Ruins or Rc != 1 or [D.get("codigo") for D in Dados] != ["E1001"]:
            Falhas.append("import inexistente %s: exit %d, stdout %r, stderr %r" % (" ".join(P), Rc, [D.get("codigo") for D in Dados], Err[:100]))
    return Falhas


def ConferirLigacaoRecusadaPeloLd(Forja, Saida):
    """Caso (5): .a corrompido (o x64 diz X0105 e o gcc/ld decide): B0004 em stderr, 1 linha, com a saida do ld dentro de ferramenta."""
    Pasta = Saida / "corrompida"
    Pasta.mkdir(exist_ok=True)
    (Pasta / "libcorrompida_b03.a").write_bytes(b"!<arch>\nlixo que nao e archive\n")
    (Pasta / "caso.fj").write_text('foreign {\n    link "corrompida_b03"\n    proc algo_b03() -> i32\n}\n\nproc main() {\n    print(algo_b03())\n}\n', encoding="utf-8")
    Argumentos = ["build", "--backend", "c", "caso.fj", "-o", str(Pasta / "caso.exe")]
    Rodada = RodarLadoCSo(Forja, Argumentos, Pasta)
    Rc, Out, Err = Rodada
    Dados, Ruins = LinhasJson(Err)
    Falhas = []
    if Rc != 1 or Out.strip() or Ruins or len(Dados) != 1 or Dados[0].get("codigo") != "B0004":
        Falhas.append("B0004 do ld: exit %d, stdout %r, stderr %r" % (Rc, Out[:60], Err[:160]))
    else:
        Ferramenta = Dados[0].get("ferramenta") or {}
        if not Ferramenta.get("nome") or "undefined reference" not in Ferramenta.get("saida", "") and "ld returned" not in Ferramenta.get("saida", ""):
            Falhas.append("B0004 sem ferramenta.nome/saida do ld: %r" % (Ferramenta,))
    if MODO == CONGELAR and not Falhas:
        GravarNoCongelado(ChaveCli(FonteDoPedido(Argumentos, Pasta), SemSaida(Argumentos)), Rodada)
    return Falhas


# Fixtures criadas DEPOIS do congelamento do lado C (0.21.0): o backend C nao existe mais para gerar a chave delas
# no oraculo selado, e a cobertura delas e do verificar_semantica. Lista literal: cada entrada nova pede motivo.
POSTERIORES_AO_CONGELAMENTO = {
    "err_E0202_r0_largura_inteiro.fj",  # 0.21.2 R0: E0202 de largura de inteiro diferente no mesmo simbolo C
    # G3 L1a (Linguagem v1.13, modulo de pasta): fixtures posteriores ao congelamento do lado C; a cobertura delas e do verificar_semantica
    "err_E0101_g3_erro_no_2o_arquivo.fj",
    "err_E0201_g3_import_nao_compartilhado.fj",
    "err_E0202_g3_nome_repetido_no_pacote.fj",
    "err_E1001_g3_pasta_vazia.fj",
    "err_E1002_g3_ciclo_pacotes.fj",
    "err_E1002_g3_pacote_importa_a_si.fj",
    "err_E1003_g3_pacote_nao_pub.fj",
    "err_E1004_g3_main_no_pacote.fj",
    "err_E1005_g3_import_por_arquivo.fj",
    "err_E1008_g3_arquivo_do_pacote.fj",
    "err_E1007_load_absoluto.fj",  # L4b: E1007 de caminho com unidade (texto novo); fixture posterior ao congelamento do lado C
    "err_E1008_g3_pasta_e_arquivo.fj",
    "err_E1001_std_wasm_rt.fj",  # 0.28.0: std:wasm_rt so existe no alvo wasm; no x64 o import do usuario e E1001 (fixture posterior ao congelamento do lado C)
    "err_E0907_const_proc_assinatura.fj",  # L4a (0.27.0): const com campo proc; o C congelado nem compila a const (E0201), fixture posterior ao congelamento do lado C
    "err_E0202_alias_aridade.fj",  # 0.45.0: alias N por ABI (aridade); o C congelado so aceita 1 alias por simbolo, fixture posterior ao congelamento do lado C
    "err_E0202_alias_i32_x_i64.fj",  # 0.45.0: idem (retorno i32 x 64 bits)
    "err_E0202_alias_f32_x_inteiro.fj",  # 0.45.0: idem (f32 x inteiro)
    "err_E0101_var_global.fj",  # 0.45.0: var global com a mensagem nova; fixture posterior ao congelamento do lado C
}


def FontesDeParidade():
    Fontes = sorted(P for P in TESTES.rglob("err_*.fj")
                    if "__pycache__" not in str(P) and P.name not in POSTERIORES_AO_CONGELAMENTO)
    return Fontes + sorted(FIXTURES.glob("*.fj"))


def Principal():
    Ap = argparse.ArgumentParser()
    Ap.add_argument("--forja", required=True)
    Ap.add_argument("--saida", default=None)
    Ap.add_argument("--congelar", action="store_true", help="grava o lado C (onde C == x64) em rapido\\oraculo_congelado_g1_cli.json; o C roda pelo gaveta\\g1\\forja_g1.exe")
    Ap.add_argument("--contra-congelado", action="store_true", help="roda so o x64 e compara com o lado C congelado (sem gcc)")
    A = Ap.parse_args()
    if A.congelar and A.contra_congelado:
        print("--congelar e --contra-congelado sao excludentes")
        return 2
    if A.congelar or A.contra_congelado:
        try:
            IniciarCongelado(CONGELAR if A.congelar else CONTRA)
        except Oraculo.ErroOraculo as Erro:
            print(f"ERRO: {Erro}")
            return 2
    Saida = Path(A.saida) if A.saida else Path(tempfile.mkdtemp(prefix="forja_md5_"))
    Saida.mkdir(parents=True, exist_ok=True)
    Fontes = FontesDeParidade()
    with ThreadPoolExecutor(max_workers=JOBS) as Pool:
        Rodadas = list(Pool.map(lambda F: ConferirParidadeDoFonte(A.forja, F, Saida), Fontes))
    Falhas = [Linha for Lista in Rodadas for Linha in Lista]
    for Nome in ("link_inexistente.fj", "simbolo_inexistente.fj", "alias_inexistente.fj"):
        Falhas += ConferirIgualMenosAlvo(A.forja, Nome, Saida)
    Falhas += ConferirImportInexistente(A.forja, Saida)
    Falhas += ConferirLigacaoRecusadaPeloLd(A.forja, Saida)
    Falhas += Ausentes
    for Linha in Falhas:
        print("FALHA", Linha)
    if MODO == CONGELAR:
        print("congelar: %d gravados, 0 pulados, %d divergencias" % (len(GravadasChaves), len(Divergencias)))
        for Divergencia in Divergencias:
            print("DIVERGENCIA", Divergencia)
    Casos = len(Fontes) + 3 + 5 + 1
    print("M-D5: %d casos, %d falhas" % (Casos, len(Falhas)))
    return 0 if not Falhas else 1


sys.exit(Principal())
