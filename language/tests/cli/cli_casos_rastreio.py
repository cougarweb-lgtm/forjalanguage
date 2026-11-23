"""Casos de cli do --rastreio no backend x64 (IA1-b12).

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la. A prova ampla (recursao, assert, main, test, byte a byte contra a referencia) mora em
benchmark\\ia_first\\verificar_rastreio_b12.py; aqui ficam os casos curtos do motor.

Regras:
  - run --rastreio (x64, dev) enriquece o R0006 com coluna, valor {indice, tamanho} e pilha de >= 2 quadros (proc, arquivo, linha, coluna);
  - (M6 P69) em dev o x64 leva coluna, valor e pilha SEM a flag (padrao); programa sem erro tem a mesma saida e o mesmo exit com e sem a flag;
  - --backend c (b13) da a MESMA linha R do x64 (pilha sombra so com a flag; run e test); check/ast/--suporte rejeitam --rastreio com exit 2.

Versao: 1.10.0
Changelog: 1.10.0 - M8 P2: CasoPerfilOpcao (--perfil: recusa com --modo dll e --suporte; em run a saida e o exit nao mudam e o exe grava os 2 arquivos do perfil) e CasoPerfilTest (test --perfil = mesmo resumo e exit 0).
           1.9.0 - 0.21.0 F4 passo 3: CasoRastreioBackendC migra para o x64 (sem --backend c); CasoChecagemOpcao e CasoSemOtimOpcao perdem os itens --modo c (X0003 vem antes da recusa) e CasoSemOtimOpcao perde o --backend c.
           1.8.0 - 0.21.0 F3: BackendEfetivo devolve x64 sem --backend (o padrao da Forja, dev e release); CasoRastreioUsoEBackendC e CasoRastreioTestBackendC comparam o R0006 do x64 com o valor fixo do C capturado na F3 (esperados\\rastreio_r0006_*.txt), sem a perna C.
           1.7.0 - 0.21.0 P3: o lado C (--backend c) vai pelo congelado da classe cli (oraculo_congelado_g1_cli.json): IniciarCongelado/FecharCongelado (o motor liga) e o Rodar local troca o forja pelo JSON no CONTRA; sem gcc no CONTRA.
           1.6.0 - M7a F9a: ConferirPadraoChecagem (build --release sem flag == --checagem sim byte a byte; o padrao do release e sim). 1.5.0 - M7a F0: CasoSemOtimOpcao (--sem-otim: recusas em dev, passo inventado, --modo c e --projeto; aceita tudo no release). 1.4.1 - O0 revisao: --checagem com --projeto e recusado (os filhos do orquestrador nao a recebem). 1.4.0 - O0 P13: CasoChecagemOpcao (--checagem sim|nao: recusas em dev, --modo c e valor invalido; aceita no release).
           1.3.0 - M6e P93d: CasoGuardaMemoriaOpcao (--guarda-memoria: recusa com --release, --backend c e --modo dll; em dev x64 a saida nao muda).
           1.2.0 - M6 P69: a pilha e padrao em dev (x64): o caso "sem flag" passa a esperar coluna, valor e pilha de 2 quadros; --rastreio fica aceito e redundante.
           1.1.1- E4: docstring cita benchmark\\ia_first (pasta movida).
           1.1.0 - IA1-b13: o backend C deixa de ignorar a flag: CasoRastreioUsoEBackendC compara com o x64; +2 casos (CasoRastreioBackendC, CasoRastreioTestBackendC).
           1.0.0 - IA1-b12: primeira versao (3 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import subprocess
import sys
import threading
from pathlib import Path

from verificar_cli import ConferirExecucao
from verificar_cli import Rodar as RodarDoMotor
from cli_casos_limites import LinhasJsonDe

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "nativo" / "rapido"))
import oraculo_congelado as Oraculo  # noqa: E402

CAMPOS_RASTREIO = ("coluna", "valor", "pilha")
PastaAqui = Path(__file__).resolve().parent
CONGELAR = "congelar"
CONTRA = "contra"
ORACULO_CLI = PastaAqui.parent / "nativo" / "rapido" / "oraculo_congelado_g1_cli.json"
CLASSE_CLI = "cli"
VALORES_DE_OPCAO = ("--backend", "-o", "--filtro", "--modo", "--checagem")
MODO = None  # None: o motor roda como antes; CONGELAR ou CONTRA (o motor liga com IniciarCongelado)
ITENS = {}  # so no CONTRA: os itens congelados da classe cli
TravaCongelado = threading.Lock()
GravadasChaves, PuladasChaves, Divergencias, Ausentes = set(), set(), [], []


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
        print(f"congelar: {len(GravadasChaves)} gravados, {len(PuladasChaves)} pulados, {len(Divergencias)} divergencias")
        for Divergencia in Divergencias:
            print("DIVERGENCIA", Divergencia)
    return list(Ausentes)


def ChaveCli(Fonte, Argumentos):
    return Oraculo.ChaveDe(Fonte, "cli", tuple(SemSaida(Argumentos)), None, CLASSE_CLI)


def SemSaida(Argumentos):
    """Tira o `-o <caminho>`: a pasta de saida muda a cada rodada e nao entra na chave."""
    if "-o" not in Argumentos:
        return list(Argumentos)
    Posicao = Argumentos.index("-o")
    return list(Argumentos[:Posicao] + Argumentos[Posicao + 2:])


def FonteDoPedido(Argumentos, Pasta):
    """O .fj do pedido (no cwd dele: a chave le o arquivo e os modulos que importa) ou o nome do modulo std: (texto)."""
    Posicionais = [Arg for I, Arg in enumerate(Argumentos) if I > 0 and not Arg.startswith("-") and Argumentos[I - 1] not in VALORES_DE_OPCAO]
    Alvo = Posicionais[0]
    return Path(Pasta) / Alvo if Alvo.endswith(".fj") else Alvo


def BackendEfetivo(Argumentos):
    """O backend que o forja usa: --backend explicito; senao x64, o padrao da Forja em dev e em release desde a 0.21.0 (o C sai do padrao)."""
    if "--backend" in Argumentos:
        return Argumentos[Argumentos.index("--backend") + 1]
    return "x64"


def TrocarBackend(Argumentos, Backend):
    """Mesmo pedido com outro backend: troca o valor do --backend ou acrescenta o --backend logo apos o comando."""
    Saida = list(Argumentos)
    if "--backend" not in Saida:
        return [Saida[0], "--backend", Backend] + Saida[1:]
    Saida[Saida.index("--backend") + 1] = Backend
    return Saida


def Rodar(Forja, Argumentos, Pasta=None, **Opcoes):
    """Troca o Rodar do motor: o lado C (backend c, explicito ou o padrao do --release) passa pelo congelado; o resto segue para o forja como antes."""
    if MODO is None or BackendEfetivo(Argumentos) != "c":
        return RodarDoMotor(Forja, Argumentos, Pasta=Pasta, **Opcoes)
    return RodarLadoC(Forja, Argumentos, Pasta, Opcoes)


def RodarLadoC(Forja, Argumentos, Pasta, Opcoes):
    """Lado C de um pedido. CONTRA le o JSON (sem gcc, sem forja_g1). CONGELAR roda o C pelo forja_g1: exit 2 do C com x64 aceitando = pulado (recusa do C, com o motivo); igual ao x64 = grava; diferente = divergencia."""
    Chave = ChaveCli(FonteDoPedido(Argumentos, Pasta or PastaAqui), Argumentos)
    if MODO == CONTRA:
        return LerCongelado(Chave, Argumentos)
    Lado = RodarDoMotor(str(Oraculo.FORJA_G1), Argumentos, Pasta=Pasta, **Opcoes)
    X64 = RodarDoMotor(Forja, TrocarBackend(Argumentos, "x64"), Pasta=Pasta, **Opcoes)
    if Lado.returncode == 2 and X64.returncode != 2:
        GravarNoCongelado(Chave, Lado, PrimeiraLinha(Lado.stderr))
    elif not SaidasIguais(Lado, X64, Argumentos):
        with TravaCongelado:
            Divergencias.append(f"{' '.join(Argumentos)}: C exit {Lado.returncode}, x64 exit {X64.returncode}, saida ou stderr difere")
    else:
        GravarNoCongelado(Chave, Lado)
    return Lado


def PrimeiraLinha(Texto):
    return Texto.strip().splitlines()[0][:200] if Texto.strip() else "sem stderr"


def SaidasIguais(Lado, X64, Argumentos):
    """C == x64 no exit e no stdout; no stderr, igual. Sem --rastreio o x64 (dev) leva coluna, valor e pilha no R0006 e o C nao (M6 P69 / CasoRastreioBackendC): tira esses campos antes de comparar."""
    if (Lado.returncode, Lado.stdout) != (X64.returncode, X64.stdout):
        return False
    if "--rastreio" in Argumentos:
        return Lado.stderr == X64.stderr
    return SemCamposDeRastreio(Lado.stderr) == SemCamposDeRastreio(X64.stderr)


def SemCamposDeRastreio(Texto):
    """As linhas do stderr; o R0006 sem coluna, valor e pilha (a comparacao sem a flag)."""
    Linhas = []
    for Linha in Texto.splitlines():
        try:
            Dados = json.loads(Linha)
        except ValueError:
            Linhas.append(Linha)
            continue
        if isinstance(Dados, dict) and Dados.get("codigo") == "R0006":
            Dados = {Chave: Valor for Chave, Valor in Dados.items() if Chave not in CAMPOS_RASTREIO}
        Linhas.append(json.dumps(Dados, sort_keys=True))
    return Linhas


def LerCongelado(Chave, Argumentos):
    """CONTRA: a rodada do lado C congelado. Chave ausente = FALHA (exit -1, nada e gravado)."""
    Registro = ITENS.get(Chave)
    if Registro is None:
        with TravaCongelado:
            Ausentes.append(f"chave ausente no oraculo cli: {' '.join(Argumentos)}")
        return subprocess.CompletedProcess(Argumentos, -1, "", "CONGELADO-AUSENTE")
    return subprocess.CompletedProcess(Argumentos, Registro["exit"], Registro["stdout"], Registro["stderr"])


def GravarNoCongelado(Chave, Rodada, Pulado=None):
    """CONGELAR: grava o lado C (so quando bate com o x64, ou como pulado com o motivo da recusa). Le e grava o JSON a cada item, dentro da trava: o arquivo e compartilhado pela classe cli."""
    Registro = {"invalido": False, "origem": "forja_g1", "exit": Rodada.returncode, "stdout": Rodada.stdout, "stderr": Rodada.stderr}
    if Pulado is not None:
        Registro["pulado"] = Pulado
    with TravaCongelado:
        Oraculo.BancoOraculo(ORACULO_CLI, Oraculo.FORJA_G1).Registrar(Chave, Registro)
        (PuladasChaves if Pulado is not None else GravadasChaves).add(Chave)


def LinhaR0006(Rodada):
    Linhas = [L for L in LinhasJsonDe(Rodada.stderr) if L.get("codigo") == "R0006"]
    return Linhas[0] if len(Linhas) == 1 else None


def CasoRastreioR0006(Forja, PastaSaidas):
    Problemas = []
    Rodada = Rodar(Forja, ["run", "--backend", "x64", "--rastreio", "b12/indice_2_quadros.fj"])
    Problemas += ConferirExecucao(Rodada, 101)
    Linha = LinhaR0006(Rodada)
    if Linha is None:
        return Problemas + [f"sem uma linha R0006 no stderr: {Rodada.stderr.strip()[:160]!r}"]
    if Linha.get("valor") != {"indice": 7, "tamanho": 3}:
        Problemas.append(f"valor {Linha.get('valor')!r}, esperado indice 7 e tamanho 3")
    if not isinstance(Linha.get("coluna"), int) or Linha["coluna"] < 1:
        Problemas.append(f"coluna {Linha.get('coluna')!r}")
    Procs = [Q.get("proc") for Q in Linha.get("pilha") or []]
    if Procs != ["pegar", "main"]:
        Problemas.append(f"pilha {Procs!r}, esperado ['pegar', 'main']")
    Problemas += [f"R0006 sem {Campo}" for Campo in ("explicacao", "forma_certa", "spec") if not Linha.get(Campo)]
    return Problemas


def CasoRastreioSemFlag(Forja, PastaSaidas):
    Problemas = []
    Sem = Rodar(Forja, ["run", "--backend", "x64", "b12/indice_2_quadros.fj"])
    Problemas += ConferirExecucao(Sem, 101)
    Linha = LinhaR0006(Sem)
    if Linha is None:
        return Problemas + ["sem R0006 no stderr (sem --rastreio)"]
    Problemas += [f"em dev, sem --rastreio, o R0006 nao leva {Campo}" for Campo in CAMPOS_RASTREIO if Campo not in Linha]  # M6 P69: a pilha e padrao em dev
    if [Q.get("proc") for Q in Linha.get("pilha") or []] != ["pegar", "main"]:
        Problemas.append(f"em dev, sem --rastreio, pilha {Linha.get('pilha')!r:.200}, esperado ['pegar', 'main']")
    Limpo = Rodar(Forja, ["run", "--backend", "x64", "b12/sem_erro.fj"])
    Marcado = Rodar(Forja, ["run", "--backend", "x64", "--rastreio", "b12/sem_erro.fj"])
    Problemas += ConferirExecucao(Limpo, 0) + ConferirExecucao(Marcado, 0)
    if Limpo.stdout != Marcado.stdout or Limpo.stderr != Marcado.stderr:
        Problemas.append(f"programa sem erro difere com --rastreio: {Limpo.stdout!r} x {Marcado.stdout!r}")
    return Problemas


def CasoRastreioUsoEBackendC(Forja, PastaSaidas):
    """check e ast recusam --rastreio com exit 2; o run --rastreio do x64 da o R0006 do valor fixo capturado do C na F3 (esperados\\rastreio_r0006_indice.txt)."""
    Problemas = []
    for Pedido in (["check", "--rastreio", "b12/sem_erro.fj"], ["ast", "--rastreio", "b12/sem_erro.fj"]):
        Problemas += ConferirExecucao(Rodar(Forja, Pedido), 2)
    Rodada = Rodar(Forja, ["run", "--backend", "x64", "--rastreio", "b12/indice_2_quadros.fj"])
    Problemas += ConferirExecucao(Rodada, 101)
    Linha = LinhaR0006(Rodada)
    if Linha is None:
        return Problemas + [f"x64 com --rastreio: sem R0006: {Rodada.stderr.strip()[:160]!r}"]
    Fixa = json.loads((PastaAqui / "esperados" / "rastreio_r0006_indice.txt").read_text(encoding="utf-8"))
    if Linha != Fixa:
        Problemas.append(f"x64 com --rastreio: R0006 {Linha!r:.300} difere do valor fixo {Fixa!r:.300}")
    return Problemas


def CasoRastreioBackendC(Forja, PastaSaidas):
    """IA1-b13 no x64 (0.21.0 F4 passo 3: o backend C saiu): run --rastreio da coluna, valor {indice 7, tamanho 3}, pilha pegar/main e R0004 com a pilha do main; sem a flag a linha sai igual (em dev o x64 ja leva os campos, M6 P69)."""
    Problemas = []
    Rodada = Rodar(Forja, ["run", "--rastreio", "b12/indice_2_quadros.fj"])
    Linha = LinhaR0006(Rodada)
    if Linha is None:
        return [f"x64 com --rastreio: sem R0006: {Rodada.stderr.strip()[:160]!r}"]
    if Linha.get("valor") != {"indice": 7, "tamanho": 3} or not isinstance(Linha.get("coluna"), int):
        Problemas.append(f"x64: coluna {Linha.get('coluna')!r} valor {Linha.get('valor')!r}")
    if [Q.get("proc") for Q in Linha.get("pilha") or []] != ["pegar", "main"]:
        Problemas.append(f"x64: pilha {Linha.get('pilha')!r:.200}")
    Sem = LinhaR0006(Rodar(Forja, ["run", "b12/indice_2_quadros.fj"]))
    if Sem != Linha:
        Problemas.append(f"x64 sem --rastreio: R0006 {Sem!r:.200} difere do com --rastreio {Linha!r:.200}")
    Erro = Rodar(Forja, ["run", "--rastreio", "main_erro.fj"])
    Linhas = [L for L in LinhasJsonDe(Erro.stderr) if L.get("codigo") == "R0004"]
    if len(Linhas) != 1 or [Q.get("proc") for Q in Linhas[0].get("pilha") or []] != ["main"]:
        Problemas.append(f"R0004 com --rastreio sem a pilha do main: {Erro.stderr.strip()[:160]!r}")
    return Problemas


def CasoRastreioTestBackendC(Forja, PastaSaidas):
    """forja test --rastreio (x64): o resumo do test nao muda com a flag e o R0006 do teste leva a pilha (proc do teste no fundo), igual ao valor fixo do C capturado na F3 (esperados\\rastreio_r0006_teste.txt)."""
    Problemas = []
    Sem = Rodar(Forja, ["test", "--backend", "x64", "b12/teste_indice.fj"])
    Com = Rodar(Forja, ["test", "--backend", "x64", "--rastreio", "b12/teste_indice.fj"])
    if Com.stdout != Sem.stdout or Com.returncode != Sem.returncode:
        Problemas.append(f"test --backend x64: stdout/exit mudam com --rastreio: {Com.stdout!r:.160} x {Sem.stdout!r:.160}")
    Linha = LinhaR0006(Com)
    Fixa = json.loads((PastaAqui / "esperados" / "rastreio_r0006_teste.txt").read_text(encoding="utf-8"))
    if Linha is None or Linha != Fixa:
        Problemas.append(f"test --backend x64: R0006 {Linha!r:.300} difere do valor fixo {Fixa!r:.300}")
    elif len(Linha.get("pilha") or []) < 2:
        Problemas.append("test --backend x64: pilha com menos de 2 quadros")
    return Problemas


def CasoGuardaMemoriaOpcao(Forja, PastaSaidas):
    """M6e P93d: --guarda-memoria so vale em dev, backend x64, exe: --release, --backend c e --modo dll = exit 2 com o texto do uso; em dev x64 o programa roda igual."""
    Problemas = []
    Texto = "--guarda-memoria so vale em dev, backend x64, exe"
    Recusas = (["run", "--release"], ["run", "--backend", "c"], ["build", "--modo", "dll", "-o", f"{PastaSaidas}/guarda.dll"])
    for Pedido in Recusas:
        Rodada = Rodar(Forja, [Pedido[0], "--guarda-memoria", *Pedido[1:], "b12/sem_erro.fj"])
        Problemas += ConferirExecucao(Rodada, 2)
        if Texto not in Rodada.stderr:
            Problemas.append(f"{' '.join(Pedido)}: stderr sem o texto da recusa: {Rodada.stderr.strip()[:160]!r}")
    Sem = Rodar(Forja, ["run", "b12/sem_erro.fj"])
    Com = Rodar(Forja, ["run", "--guarda-memoria", "b12/sem_erro.fj"])
    Problemas += ConferirExecucao(Com, 0)
    if Com.stdout != Sem.stdout or Com.returncode != Sem.returncode:
        Problemas.append(f"run --guarda-memoria muda a saida: {Com.stdout!r:.160} x {Sem.stdout!r:.160}")
    return Problemas


def CasoPerfilOpcao(Forja, PastaSaidas):
    """M8 P2: --perfil so vale em build, run e test de exe: --modo dll e --suporte = exit 2 com o texto do uso; build --perfil -o grava o exe e o exe grava <exe>.perfil.json e <exe>.perfil.txt sem mudar o exit."""
    Problemas = []
    Texto = "--perfil so vale em build, run e test, exe"
    Recusas = (["build", "--modo", "dll", "-o", f"{PastaSaidas}/perfil.dll"], ["build", "--suporte", "--json"])
    for Pedido in Recusas:
        Rodada = Rodar(Forja, [Pedido[0], "--perfil", *Pedido[1:], "b12/sem_erro.fj"])
        Problemas += ConferirExecucao(Rodada, 2)
        if Texto not in Rodada.stderr:
            Problemas.append(f"{' '.join(Pedido)}: stderr sem o texto da recusa: {Rodada.stderr.strip()[:160]!r}")
    Exe = PastaSaidas / "perfil.exe"
    Problemas += ConferirExecucao(Rodar(Forja, ["build", "--perfil", "b12/sem_erro.fj", "-o", str(Exe)]), 0)
    Execucao = subprocess.run([str(Exe)], capture_output=True, text=True, encoding="utf-8", errors="replace", check=False)
    if Execucao.returncode != 0 or Execucao.stdout.strip() != "2":
        Problemas.append(f"exe --perfil: exit {Execucao.returncode} e stdout {Execucao.stdout!r:.80} (esperado 0 e '2')")
    for Extensao in (".perfil.json", ".perfil.txt"):
        if not Path(f"{Exe}{Extensao}").is_file():
            Problemas.append(f"o exe --perfil nao gravou {Exe.name}{Extensao}")
    return Problemas


def CasoPerfilTest(Forja, PastaSaidas):
    """M8 P2: forja test --perfil de dois_testes.fj = exit 0 e o mesmo resumo de sempre ('2 testes, 0 falharam')."""
    return ConferirExecucao(Rodar(Forja, ["test", "--perfil", "dois_testes.fj"]), 0, "2 testes, 0 falharam")


def CasoChecagemOpcao(Forja, PastaSaidas):
    """O0 P13: --checagem sim|nao: nao em dev, valor invalido = exit 2; build --release com sim e com nao = exit 0; sim em dev e aceito. (0.21.0 F4 passo 3: o item --modo c saiu: X0003 vem antes da recusa.)"""
    Problemas = []
    Recusas = (
        (["run", "--checagem", "nao"], "--checagem nao so vale com --release"),
        (["run", "--release", "--checagem", "talvez"], "--checagem aceita so sim ou nao"),
        (["build", "--projeto", "--release", "--checagem", "sim"], "--projeto nao aceita"),
    )
    for Pedido, Texto in Recusas:
        Rodada = Rodar(Forja, [*Pedido, "b12/sem_erro.fj"])
        Problemas += ConferirExecucao(Rodada, 2)
        if Texto not in Rodada.stderr:
            Problemas.append(f"{' '.join(Pedido)}: stderr sem o texto da recusa {Texto!r}: {Rodada.stderr.strip()[:160]!r}")
    for Valor in ("sim", "nao"):
        Rodada = Rodar(Forja, ["build", "--release", "--checagem", Valor, "b12/sem_erro.fj", "-o", f"{PastaSaidas}/checagem_{Valor}.exe"])
        Problemas += ConferirExecucao(Rodada, 0)
    Problemas += ConferirExecucao(Rodar(Forja, ["run", "--checagem", "sim", "b12/sem_erro.fj"]), 0)
    return Problemas + ConferirPadraoChecagem(Forja, PastaSaidas)


def ConferirPadraoChecagem(Forja, PastaSaidas):
    """M7a F9a: build --release sem flag == --checagem sim byte a byte."""
    Saidas = {}
    for Nome, Flags in (("padrao", []), ("sim", ["--checagem", "sim"])):
        Saida = f"{PastaSaidas}/checagem_padrao_{Nome}.exe"
        Rodada = Rodar(Forja, ["build", "--release", "--backend", "x64", *Flags, "b12/sem_erro.fj", "-o", Saida])  # x64: o PE do gcc leva carimbo de hora
        if Rodada.returncode != 0:
            return [f"build --release {' '.join(Flags)}: exit {Rodada.returncode}"]
        Saidas[Nome] = Path(Saida).read_bytes()
    Problemas = []
    if Saidas["padrao"] != Saidas["sim"]:
        Problemas.append("build --release sem flag difere de --checagem sim (o padrao do release e sim)")
    return Problemas


def CasoSemOtimOpcao(Forja, PastaSaidas):
    """M7a F0: --sem-otim: em dev, passo inventado e --projeto = exit 2; build --release com tudo no x64 = exit 0. (0.21.0 F4 passo 3: os itens --modo c e --backend c saem com o backend C.)"""
    Problemas = []
    Recusas = (
        (["run", "--sem-otim", "tudo"], "--sem-otim so vale com --release"),
        (["run", "--release", "--sem-otim", "inventado"], "passo desconhecido 'inventado'"),
        (["build", "--projeto", "--release", "--sem-otim", "tudo"], "--projeto nao aceita"),
    )
    for Pedido, Texto in Recusas:
        Rodada = Rodar(Forja, [*Pedido, "b12/sem_erro.fj"])
        Problemas += ConferirExecucao(Rodada, 2)
        if Texto not in Rodada.stderr:
            Problemas.append(f"{' '.join(Pedido)}: stderr sem o texto da recusa {Texto!r}: {Rodada.stderr.strip()[:160]!r}")
    Rodada = Rodar(Forja, ["build", "--release", "--backend", "x64", "--sem-otim", "tudo", "b12/sem_erro.fj", "-o", f"{PastaSaidas}/sem_otim_x64.exe"])
    return Problemas + ConferirExecucao(Rodada, 0)
