"""Casos de cli do S4 P1: forja test roda os test dos modulos de origem arquivo (transitivos), com rotulo qualificado.

Importado pelo motor verificar_cli.py (que monta a lista CASOS na ordem original); as funcoes comuns vem de la.
Fixtures: tests\\cli\\s4\\transitivo (raiz -> mods/a -> mods/b) e tests\\cli\\s4\\falha (teste de modulo que falha); o check
de titulo igual em modulos diferentes usa tests\\semantica\\ok_s4_titulo_igual_em_modulos.fj.

Versao: 1.8.4
Changelog: 1.8.4 - PREPARO 0.24.1: std:text passa a 58 testes (+4: to_valid_lossy e utf16be_encode/decode); esperados\\s4_std_text_* ganham as 4 linhas (so acrescimos) e o N exato vai para 58. 1.8.3 - G3-L3: std:text passa a 54 testes (+16 de parse_float com arredondamento correto); esperados\\s4_std_text_* ganham as 16 linhas (so acrescimos). 1.8.2 - D-G3-L2b: std:os passa a 19 testes (+3 de Linux: mem_available, pid_alive, error_text; contam tambem no Windows); esperados\\s4_std_os_* ganham as 3 linhas. 1.8.1 - D-G3-L2a: std:os passa a 16 testes (bloco A: +4) e std:file a 26 (bloco D: +2); esperados\\s4_std_os_* e s4_std_file_* ganham as linhas novas (so acrescimos). 1.8.0 - R10c (rede): std:text passa a 38 testes (+9 de html_escape/html_unescape) e std:time a 23 (+13 de TZif, offset e RFC 5322); N exato e valor fixo ampliado com a saida do x64 nas linhas novas (o C saiu na 0.21.0; as linhas antigas seguem byte a byte iguais).
           1.7.0 - 0.21.0 F3: a perna C sai dos casos de forja test (falha qualificada, std:math, transitivo e os std Igual); o valor fixo vem do C capturado uma vez (literal no caso e tests\\cli\\esperados\\s4_<modulo>_texto/json.txt).
           1.6.0 - 0.21.0 P3: o lado C (--backend c) vai pelo congelado da classe cli (oraculo_congelado_g1_cli.json): IniciarCongelado/FecharCongelado (o motor liga) e o Rodar local troca o forja pelo JSON no CONTRA; sem gcc no CONTRA.
           1.5.1 - M5 P28b-2b: std:os passa a 10 testes (write_stderr/print_stderr e peak_memory). 1.5.0 - S4 P7: +2 casos (std:file com 18 testes e std:os com 3, texto e --json iguais em C e x64, N exato); ConferirStdIgual roda cada execucao com cwd numa pasta temporaria propria (std:file grava no disco) e o caso CasoS4StdSemTestes sai (perdeu o alvo; 'sem test' segue em cli_casos_frontend com sem_teste.fj).
           1.4.0 - S4 P6: +2 casos (std:fmt com 30 testes e std:time com 10, texto e --json iguais em C e x64, N exato).
           1.3.0 - S4 P5: +2 casos (std:text com 29 testes e std:path com 6, texto e --json iguais em C e x64, N exato); o caso "sem testes" passa de std:text para std:file, que segue sem testes.fj.
           1.2.0 - S4 P4: std:math passa a 8 testes (RESUMO_STD_MATH e o caso C); +3 casos (std:math, std:rand com 9 e std:mem com 4: texto e --json iguais em C e x64, N exato).
           1.1.0 - S4 P2 (D5-D6): 5 casos (std:math em C, C x64 com o mesmo --json, std:nada = E1001, xx:y = exit 2, std:text sem testes = 0 testes).
           1.0.0 - S4 P1 (D1-D3): 6 casos (transitivo na ordem da IR, --filtro por rotulo, --json com o rotulo em "teste", c e x64 com a mesma saida, falha qualificada, titulo igual em modulos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

from verificar_cli import ConferirExecucao
from verificar_cli import Rodar as RodarDoMotor

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "nativo" / "rapido"))
import oraculo_congelado as Oraculo  # noqa: E402

PastaAqui = Path(__file__).resolve().parent
CONGELAR = "congelar"
CONTRA = "contra"
ORACULO_CLI = PastaAqui.parent / "nativo" / "rapido" / "oraculo_congelado_g1_cli.json"
CLASSE_CLI = "cli"
VALORES_DE_OPCAO = ("--backend", "-o", "--filtro", "--modo", "--checagem")
MODO = None  # None: o motor roda como antes; CONGELAR ou CONTRA (o motor liga com IniciarCongelado)
ITENS = {}  # so no CONTRA: os itens congelados da classe cli
TravaCongelado = threading.Lock()
GravadasChaves, Divergencias, Ausentes = set(), [], []


def IniciarCongelado(Modo):
    """O motor liga o modo antes dos casos. CONGELAR: recusa banco selado ou cabecalho mudado (o JSON e da classe inteira: nao apaga). CONTRA: confere selo/cabecalho e le os itens, sem gcc."""
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


def FecharCongelado():
    """Fim da rodada: CONGELAR imprime gravados e divergencias; devolve as chaves ausentes do CONTRA (cada uma e FALHA)."""
    if MODO == CONGELAR:
        print(f"congelar: {len(GravadasChaves)} gravados, 0 pulados, {len(Divergencias)} divergencias")
        for Divergencia in Divergencias:
            print("DIVERGENCIA", Divergencia)
    return list(Ausentes)


def ChaveCli(Fonte, Argumentos):
    return Oraculo.ChaveDe(Fonte, "cli", tuple(Argumentos), None, CLASSE_CLI)


def FonteDoPedido(Argumentos, Pasta):
    """O .fj do pedido (no cwd dele: a chave le o arquivo e os modulos que importa) ou o nome do modulo std: (texto)."""
    Posicionais = [Arg for I, Arg in enumerate(Argumentos) if I > 0 and not Arg.startswith("-") and Argumentos[I - 1] not in VALORES_DE_OPCAO]
    Alvo = Posicionais[0]
    return Path(Pasta) / Alvo if Alvo.endswith(".fj") else Alvo


def BackendDoPedido(Argumentos):
    return Argumentos[Argumentos.index("--backend") + 1] if "--backend" in Argumentos else None


def TrocarBackend(Argumentos, Backend):
    Saida = list(Argumentos)
    Saida[Saida.index("--backend") + 1] = Backend
    return Saida


def Rodar(Forja, Argumentos, Pasta=None, **Opcoes):
    """Troca o Rodar do motor: o lado C (--backend c) passa pelo congelado; o resto segue para o forja como antes."""
    if MODO is None or BackendDoPedido(Argumentos) != "c":
        return RodarDoMotor(Forja, Argumentos, Pasta=Pasta, **Opcoes)
    return RodarLadoC(Forja, Argumentos, Pasta, Opcoes)


def RodarLadoC(Forja, Argumentos, Pasta, Opcoes):
    """Lado C de um pedido. CONTRA le o JSON (sem gcc, sem forja_g1). CONGELAR roda o C pelo forja_g1 e grava so se o x64 der o mesmo exit, stdout e stderr."""
    Chave = ChaveCli(FonteDoPedido(Argumentos, Pasta or PastaAqui), Argumentos)
    if MODO == CONTRA:
        return LerCongelado(Chave, Argumentos)
    Lado = RodarDoMotor(str(Oraculo.FORJA_G1), Argumentos, Pasta=Pasta, **Opcoes)
    X64 = RodarDoMotor(Forja, TrocarBackend(Argumentos, "x64"), Pasta=Pasta, **Opcoes)
    if (Lado.returncode, Lado.stdout, Lado.stderr) != (X64.returncode, X64.stdout, X64.stderr):
        with TravaCongelado:
            Divergencias.append(f"{' '.join(Argumentos)}: C exit {Lado.returncode}, x64 exit {X64.returncode}, saida ou stderr difere")
        return Lado
    GravarNoCongelado(Chave, Lado)
    return Lado


def LerCongelado(Chave, Argumentos):
    """CONTRA: a rodada do lado C congelado. Chave ausente = FALHA (exit -1, nada e gravado)."""
    Registro = ITENS.get(Chave)
    if Registro is None:
        with TravaCongelado:
            Ausentes.append(f"chave ausente no oraculo cli: {' '.join(Argumentos)}")
        return subprocess.CompletedProcess(Argumentos, -1, "", "CONGELADO-AUSENTE")
    return subprocess.CompletedProcess(Argumentos, Registro["exit"], Registro["stdout"], Registro["stderr"])


def GravarNoCongelado(Chave, Rodada):
    """CONGELAR: grava o lado C (so quando bate com o x64). Le e grava o JSON a cada item, dentro da trava: o arquivo e compartilhado pela classe cli."""
    Registro = {"invalido": False, "origem": "forja_g1", "exit": Rodada.returncode, "stdout": Rodada.stdout, "stderr": Rodada.stderr}
    with TravaCongelado:
        Oraculo.BancoOraculo(ORACULO_CLI, Oraculo.FORJA_G1).Registrar(Chave, Registro)
        GravadasChaves.add(Chave)


RESUMO_TRANSITIVO = '{"resumo": {"total": 3, "ok": 3, "falhou": 0, "filtrados": 0}}'
RESUMO_STD_MATH = '{"resumo": {"total": 8, "ok": 8, "falhou": 0, "filtrados": 0}}'  # S4 P4: os 8 testes de std:math, todos ok


def TestesDoJson(Texto):
    """As linhas JSON com o campo 'teste' do forja test --json, na ordem em que saem (o resumo nao tem o campo)."""
    Dicionarios = []
    for Linha in Texto.replace("\r\n", "\n").split("\n"):
        if Linha.startswith('{"teste"'):
            Dicionarios.append(json.loads(Linha))
    return Dicionarios


def SaidaFixa(Modulo, Modo):
    """Valor fixo de 'forja test <modulo>' (Modo 'texto' ou 'json'): tests\\cli\\esperados\\s4_<modulo>_<modo>.txt, capturado do C uma vez (UTF-8, LF)."""
    return (PastaAqui / "esperados" / f"s4_{Modulo.replace(':', '_')}_{Modo}.txt").read_text(encoding="utf-8")


def CasoS4Transitivo(Forja, PastaSaidas):
    """test de raiz -> mods/a -> mods/b: os 3 test rodam, o de b antes (dependencias primeiro) e a raiz por ultimo."""
    Rodada = Rodar(Forja, ["test", "s4/transitivo/raiz.fj"])
    Problemas = ConferirExecucao(Rodada, 0, "3 testes, 0 falharam")
    Linhas = Rodada.stdout.replace("\r\n", "\n").split("\n")
    Esperadas = ["ok   [mods/b] triplo", "ok   [mods/a] area", "ok   raiz um"]
    if Linhas[:3] != Esperadas:
        Problemas.append(f"ordem das linhas {Linhas[:3]!r}, esperado {Esperadas!r}")
    return Problemas


def CasoS4Filtro(Forja, PastaSaidas):
    """--filtro "[mods/a]" roda so o test de mods/a (o rotulo e o que o filtro casa); os outros 2 contam como filtrados."""
    Rodada = Rodar(Forja, ["test", "s4/transitivo/raiz.fj", "--filtro", "[mods/a]"])
    Problemas = ConferirExecucao(Rodada, 0, "1 testes, 0 falharam, 2 filtrados", "ok   [mods/a] area")
    if "raiz um" in Rodada.stdout or "triplo" in Rodada.stdout:
        Problemas.append(f"o filtro deixou passar outros testes: {Rodada.stdout!r}")
    return Problemas


def CasoS4JsonRotulo(Forja, PastaSaidas):
    """--json: o campo 'teste' leva o rotulo qualificado dos modulos e o nome puro da raiz; o resumo fecha a saida."""
    Rodada = Rodar(Forja, ["test", "s4/transitivo/raiz.fj", "--json"])
    Problemas = ConferirExecucao(Rodada, 0, RESUMO_TRANSITIVO)
    Nomes = [Dados.get("teste") for Dados in TestesDoJson(Rodada.stdout)]
    if Nomes != ["[mods/b] triplo", "[mods/a] area", "raiz um"]:
        Problemas.append(f"campo teste {Nomes!r}, esperado os 3 rotulos na ordem da IR")
    return Problemas


def CasoS4BackendIgual(Forja, PastaSaidas):
    """--backend x64 do transitivo: exit 0 e o stdout igual ao valor fixo do C (capturado uma vez; rotulos na ordem da IR)."""
    Rodada = Rodar(Forja, ["test", "s4/transitivo/raiz.fj", "--backend", "x64"])
    Problemas = ConferirExecucao(Rodada, 0, "3 testes, 0 falharam")
    Saida = Rodada.stdout.replace("\r\n", "\n")
    if Saida != "ok   [mods/b] triplo\nok   [mods/a] area\nok   raiz um\n3 testes, 0 falharam\n":
        Problemas.append(f"stdout {Saida!r} difere do valor fixo do C")
    return Problemas


def CasoS4FalhaQualificada(Forja, PastaSaidas):
    """test de mods/falha que falha: exit 1 no x64 com 'FALHOU [mods/falha] dois'; o --json traz o arquivo do modulo."""
    Problemas = []
    for Backend in ("x64",):
        Rodada = Rodar(Forja, ["test", "s4/falha/raiz.fj", "--backend", Backend])
        Problemas += ConferirExecucao(Rodada, 1, "2 testes, 1 falharam", "FALHOU [mods/falha] dois")
    Rodada = Rodar(Forja, ["test", "s4/falha/raiz.fj", "--json"])
    Problemas += ConferirExecucao(Rodada, 1)
    Falha = [Dados for Dados in TestesDoJson(Rodada.stdout) if Dados.get("teste") == "[mods/falha] dois"]
    if len(Falha) != 1 or Falha[0].get("resultado") != "falhou" or Falha[0].get("codigo") != "R0003" or Falha[0].get("linha") != 6:
        Problemas.append(f"linha JSON da falha qualificada incorreta: {Falha!r}")
    elif not str(Falha[0].get("arquivo", "")).replace("\\", "/").endswith("s4/falha/mods/falha.fj"):
        Problemas.append(f"arquivo da falha {Falha[0].get('arquivo')!r}, esperado o do modulo falha.fj")
    return Problemas


def CasoS4TituloIgualEmModulos(Forja, PastaSaidas):
    """check: o mesmo titulo "area" na raiz e em dois modulos e aceito (E0802 so vale dentro do mesmo modulo)."""
    return ConferirExecucao(Rodar(Forja, ["check", "../semantica/ok_s4_titulo_igual_em_modulos.fj"]), 0)


def CasoS4StdMathC(Forja, PastaSaidas):
    """forja test std:math no backend x64: os 8 testes do modulo (testes.fj anexado so ao alvo) passam, com o rotulo [std:math]."""
    Rodada = Rodar(Forja, ["test", "std:math", "--backend", "x64"])
    Problemas = ConferirExecucao(Rodada, 0, "8 testes, 0 falharam")
    Linhas = Rodada.stdout.replace("\r\n", "\n").split("\n")
    if Linhas[:1] != ["ok   [std:math] trig de zero e exata"]:
        Problemas.append(f"primeira linha {Linhas[:1]!r}, esperado o rotulo [std:math] da trig")
    return Problemas


def CasoS4StdMathX64Igual(Forja, PastaSaidas):
    """forja test std:math no backend x64: exit 0 e o --json igual ao valor fixo do C (esperados\\s4_std_math_json.txt)."""
    Rodada = Rodar(Forja, ["test", "std:math", "--backend", "x64", "--json"])
    Problemas = ConferirExecucao(Rodada, 0, RESUMO_STD_MATH)
    if Rodada.stdout.replace("\r\n", "\n") != SaidaFixa("std:math", "json"):
        Problemas.append("--json do x64 difere do valor fixo do C (esperados\\s4_std_math_json.txt)")
    return Problemas


def CasoS4StdNadaE1001(Forja, PastaSaidas):
    """forja test std:nada: modulo inexistente e o E1001 que ja existia (exit 1)."""
    return ConferirExecucao(Rodar(Forja, ["test", "std:nada"]), 1, None, "E1001")


def CasoS4PrefixoDesconhecido(Forja, PastaSaidas):
    """forja test xx:y: prefixo que nao e std, vendor nem engine e uso errado (exit 2)."""
    return ConferirExecucao(Rodar(Forja, ["test", "xx:y"]), 2)


def RodarStdEmPasta(Forja, Argumentos):
    """Roda o forja de std com cwd numa pasta temporaria propria desta execucao (std:file grava no disco); a pasta some ao sair."""
    with tempfile.TemporaryDirectory(prefix="cli_std_") as Pasta:
        return Rodar(Forja, Argumentos, Pasta=Pasta)


def ConferirStdIgual(Forja, Modulo, Total):
    """forja test <modulo> no x64, texto e --json: exit 0, 'Total testes, 0 falharam' exato e a saida igual ao valor fixo do C (esperados\\s4_<modulo>_texto/json.txt)."""
    Problemas = []
    Resumo = f'{{"resumo": {{"total": {Total}, "ok": {Total}, "falhou": 0, "filtrados": 0}}}}'
    Texto = RodarStdEmPasta(Forja, ["test", Modulo, "--backend", "x64"])
    Problemas += ConferirExecucao(Texto, 0, f"{Total} testes, 0 falharam")
    if Texto.stdout.replace("\r\n", "\n") != SaidaFixa(Modulo, "texto"):
        Problemas.append("texto do x64 difere do valor fixo do C")
    Json = RodarStdEmPasta(Forja, ["test", Modulo, "--backend", "x64", "--json"])
    Problemas += ConferirExecucao(Json, 0, Resumo)
    if Json.stdout.replace("\r\n", "\n") != SaidaFixa(Modulo, "json"):
        Problemas.append("--json do x64 difere do valor fixo do C")
    return Problemas


def CasoS4StdMathIgual(Forja, PastaSaidas):
    """std:math (8 testes): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:math", 8)


def CasoS4StdRandIgual(Forja, PastaSaidas):
    """std:rand (9 testes): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:rand", 9)


def CasoS4StdMemIgual(Forja, PastaSaidas):
    """std:mem (4 testes): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:mem", 4)


def CasoS4StdTextIgual(Forja, PastaSaidas):
    """std:text (58 testes): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:text", 58)


def CasoS4StdPathIgual(Forja, PastaSaidas):
    """std:path (6 testes): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:path", 6)


def CasoS4StdFmtIgual(Forja, PastaSaidas):
    """std:fmt (30 testes): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:fmt", 30)


def CasoS4StdTimeIgual(Forja, PastaSaidas):
    """std:time (23 testes): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:time", 23)


def CasoS4StdFileIgual(Forja, PastaSaidas):
    """std:file (26 testes, grava em s4_std_file_* dentro da pasta temporaria da execucao): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:file", 27)


def CasoS4StdOsIgual(Forja, PastaSaidas):
    """std:os (19 testes: TARGET/ARCH, args/env, cwd, caminhos, set_env, write_stderr/print_stderr, peak_memory, random_bytes, pid, cpu_count, mem_available, pid_alive, error_text, os 3 testes Linux e write_stdout): texto e --json iguais ao valor fixo do C, com o N exato."""
    return ConferirStdIgual(Forja, "std:os", 20)
