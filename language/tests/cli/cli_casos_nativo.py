"""Casos de cli do backend nativo x64: --backend, --debug-code, --tempos, --target, explicar X0xxx, X0001 do BIR, X0302, falhas injetadas, debug.jsonl e --suporte.

Importado pelo motor verificar_cli.py (que monta a lista CASOS na ordem original); as funcoes comuns vem de la.

Versao: 1.32.1
Changelog: 1.32.1 - 0.25.1: CasoOlaImportaSoOQueUsa passa o 01_ola relativo ao cwd (ExemploOlaRelativo): o exe dev embute o caminho do fonte como veio e o repo atual (72 caracteres) abria 1 bloco de 512 B no .rdata (24618 B > 24 KB); o caso volta a medir codigo e nao caminho (24106 B).
           1.32.0 - G0d: +2 casos (CasoDebugPassesSecao: item passes do --debug-code, uma secao '== passe <nome>' por passe que rodou; CasoDebugPassesDeterministico: 2 builds no mesmo caminho = .debug.txt e exe iguais).
           1.31.0 - R10a (contrato_rede): +1 caso (CasoTargetLinuxRedeLibsDoWindows: http_local.fj liga no x86_64-linux; ws2_32/crypt32/bcrypt/advapi32/secur32/iphlpapi viram "nada a ligar", libcurl.a de vendor\\linux).
           1.30.0 - 0.21.0 F4 passo 3: saem CasoBackendC e CasoTargetLinuxBackendC (pedido --backend c e alvo linux via C); o LEIAME do pacote de relato tem 4 linhas (texto atual de nativo_erro.c, antes 5); o X0001 com ir nao traz mais sugestao (--backend c saiu).
           1.29.0 - 0.21.0 F3: os casos que comparavam o C com o x64 passam a rodar so o x64 contra valor fixo (ConferirTesteX64IgualAoC, ConferirX64IgualAoC, ConferirPodaSemantica sem o C, CasoTracoEscalarIgual; o stderr do C esta em esperados\\nativo_*.txt).
           1.28.0 - 0.21.0 F3: CasoDebugCodeReleaseSemBackend passa a x64 (--release --debug-code sem --backend: exit 0 e o .debug.txt ao lado do exe, como em dev).
           1.27.1 - M6b P75: CasoDebugJsonlDeterministico grava as 2 builds no mesmo caminho (o RSDS do exe leva o caminho do .pdb).
           1.27.0 - M6 P69: ConferirTesteX64IgualAoC normaliza a linha R (rcode_normalizar) e o 01_ola dev pode ter ate 24 KB (o runtime dev leva o rastreio).
           1.26.0- M5 P29b: +2 casos (CasoTargetLinuxTestOpcoes: --json/--filtro com -o so constroem; CasoTargetLinuxTestStd: std:path com -o grava o exe de teste).
           1.25.0 - M5 P28b-2: CasoTargetLinuxProcessoInterino vira CasoTargetLinuxProcessoLiga (std:process compila para x86_64-linux em dev e release, ELF64 gravado; sem X0003).
           1.24.0 - M5 P28b-1: +2 casos (CasoTargetLinuxProcessoInterino: X0003 'std:process ainda nao liga no Linux: P28b-2'; CasoTargetLinuxFileOsLigam: o handle.fj de tests\\nativo\\linux\\es grava ELF64 em dev e release).
           1.23.0 - M5 P27b (D-M5-9): o x86_64-linux GRAVA o ELF: CasoTargetLinux, CasoTargetLinuxRelease e CasoTargetLinuxTest (com -o) passam a esperar exit 0 e um ELF64 (ConferirElfGerado); +2 casos (CasoTargetLinuxDeterministico, CasoDebugJsonlLinux: esquema 0.7.0, formato ELF64, segmentos, cobertura do .text); CasoSuporteDoisAlvos espera escritor true no linux; CasoDebugJsonl espera o cabecalho 0.7.0; ConferirCobertura aceita Texto.
           1.22.0 - M5 P27a (D-M5-9): CasoTargetLinux passa a conferir o X0003 'escritor ELF: P27b' (sem_teste.fj e 01_ola chegam a Imagem do linux) e +8 casos (ConferirRecusaLinux: --release, --backend c e --modo c, --modo dll, run, test com e sem -o, foreign com link, --projeto; CasoSuporteDoisAlvos).
           1.21.0 - M4 P25b: CasoDebugCodeBackendC vira CasoDebugCodeSemBackendEmDev (sucesso: o padrao do dev e x64) e +1 caso (CasoDebugCodeReleaseSemBackend: X0003 em --release); +1 em 176.
           1.20.0 - M4 P24p: +1 caso (CasoPodaRegistroJsonl: registro poda do .debug.jsonl e erro de tipo em proc podado com o mesmo E-code); CasoDebugJsonl passa a exigir o registro poda e o cabecalho 0.6.0 (esquema 0.6.0).
           1.19.0 - M4 P24a: +2 casos (CasoTemposRegistroJsonl: registro tempos do .debug.jsonl com os 14 campos; CasoTemposArquivoX64: --tempos x64 com os 13 nomes + emit_ns);
                    CasoDebugCodeTemposJson passa a exigir as 10 etapas + outros_ns; esquema do debug.jsonl 0.5.0.
           1.18.0 - M3b P23e: CasoX0103BibliotecaEstatica passa a esperar a estatica LIGADA (exit 0, exe imprime 3): ar deixou de ser X0103; mesma contagem.
           1.17.0 - M3 P22-O2: +2 casos (CasoProjetoComBackendC: build --projeto --backend c = X0003; CasoProjetoComSaida: build --projeto com -o = uso errado).
           1.16.0 - M3 P22-O1a: +3 casos (CasoEntradasListaRaizModulosEDll, CasoEntradasComBackendC, CasoLibPastaAchaDllForaDaPastaDoFonte).
           1.15.0 -M3 P22a: CasoBackendX64Dll deixa de esperar X0003 (a DLL x64 sai com exit 0 e o .h; sem export segue B0003); o esquema do debug.jsonl passa a 0.4.0 (registro "export"); mesma contagem.
           1.14.0 - M3 P21v-c: nenhuma construcao ficou com X0001 real: CasoX0001ComIr volta a usar FORJA_TESTE_INJETAR=X0001 no callback.fj (ir = main instr 0 IR_ARRAY), CasoSuporteJson confere total 0 em callback.fj e proc_ind.fj e CasoProcIndX64IgualAoC (+1).
           1.13.0 - M3 P21v-b: X0001 volta a ser real (proc como valor): CasoX0001ComIr usa x/proc_ind.fj sem injecao; CasoSuporteJson confere 2 ocorrencias (IR_LOCAL de proc e IR_CHAMADA_IND) no proc_ind.fj e total 0 no callback.fj; mesma contagem.
           1.12.0 -M3 P21: callback.fj deixou de ser X0001 (trampolim fjt_). CasoX0001ComIr usa FORJA_TESTE_INJETAR=X0001 (nativo_erro.c 0.2.1) no callback.fj; CasoSuporteJson confere total 0 no callback.fj; mesma contagem de casos.
           1.11.0 - M3 P19b: +1 caso (CasoX0105ImportLibrary32Bits: link que acha tests/nativo/x/fx_lib32.lib, import library i386 do dlltool).
           1.10.0 - M3 P19: +3 casos (CasoX0101SimboloSemBiblioteca, CasoX0102LinkSemArquivo, CasoX0103BibliotecaEstatica); CasoBirForeign vira CasoForeignX64IgualAoC (foreign.fj roda no x64);
                    CasoX0001ComIr e CasoSuporteJson passam para callback.fj (a ultima construcao com X0001: proc da Forja passado a foreign); a tabela de --suporte ja nao tem regra recusada.
           1.9.1 - M3 P19a: o cabecalho do debug.jsonl e o esquema passam a 0.3.0 (registro "import" com descritor); nenhum caso novo.
           1.9.0 - Linguagem v1.11 L16a: +1 caso (CasoTesteJsonX64IgualAoC: forja test --json igual entre c e x64); ConferirTesteX64IgualAoC aceita Extras.
           1.8.0 - P18a: +4 casos (CasoExplicarX0801, CasoExplicarX0802, CasoExplicarX0803 e CasoTracoEscalarIgual: build --traco do traco_escalar.fj, stderr igual entre c e x64).
           1.7.0 - M2 P17c: +1 caso (CasoOlaImportaSoOQueUsa: o .idata so tem os __imp_ alcancaveis; 01_ola sem heap nem relogio e <= 20 KB; lista_append ainda importa HeapAlloc).
           1.6.0 - M2 P13a: CasoBirPrintStr vira CasoPrintStrX64IgualAoC (print_str.fj roda no x64); CasoX0001ComIr passa para foreign.fj (IR_CHAMADA); mesma contagem.
           1.5.0 - M2 P12b: CasoBackendX64Test deixa de ser X0003 (sem_teste.fj roda no x64, exit 0); +2 casos (CasoTesteX64Passa e CasoTesteX64Falha: forja test --backend x64 com os bytes do C).
           1.4.0 - M2 P12a: CasoBirResultado vira CasoResultadoX64IgualAoC (resultado.fj roda no x64).
           1.3.0 - M2 P11a: CasoBirStruct vira CasoStructX64IgualAoC (struct.fj roda no x64); esquema do debug.jsonl 0.2.0.
           1.2.0 - M2 P10b: CasoBirReal vira CasoRealX64IgualAoC (real.fj roda no x64; helper ConferirX64IgualAoC); CasoX0001ComIr passa para print_str.fj.
           1.1.0 - M2 P9: CasoBirDivisao vira CasoDivisaoX64IgualAoC (divisao.fj roda no x64); CasoSuporteJson passa para foreign.fj.
           1.0.0 - P9a: casos movidos do verificar_cli.py 1.21.0 sem mudar nenhum.
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tests\: rcode_normalizar (P69)
from rcode_normalizar import NormalizarLinhasR  # noqa: E402
from verificar_cli import (ExemploOla, PastaAqui, TimeoutSegundos, ConferirExecucao, ConferirJsonCodigo, LinhaJson, Rodar,
                           UltimaLinha)


def CasoBackendX64(Forja, PastaSaidas):
    """O x64 nao gera C: build --modo c --backend x64 da X0003 no stderr e exit 2."""
    Rodada = Rodar(Forja, ["build", "--modo", "c", "sem_teste.fj", "--backend", "x64", "-o", str(PastaSaidas / "x64.c")])
    return ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(Rodada.stderr, "X0003")


def CasoBackendX64Test(Forja, PastaSaidas):
    """O x64 roda 'forja test' desde o M2 P12b: sem nenhum test sai 0 e imprime '0 testes, 0 falharam'."""
    return ConferirExecucao(Rodar(Forja, ["test", "sem_teste.fj", "--backend", "x64"]), 0, "0 testes, 0 falharam")


def ConferirTesteX64IgualAoC(Forja, Fonte, CodigoEsperado, Trechos, SaidaEsperada, Extras=()):
    """forja test <Fonte> [Extras] com --backend x64: stdout (byte a byte, sem tirar CRLF) e stderr iguais ao valor fixo SaidaEsperada=(stdout, stderr), capturado do C antes da F3; exit esperado; Trechos em stdout/stderr."""
    Rodada = subprocess.run([str(Forja), "test", Fonte, *Extras, "--backend", "x64"], capture_output=True, timeout=TimeoutSegundos, cwd=PastaAqui)
    # M6 P69: em dev o x64 traz coluna/valor/pilha nas linhas R; o valor fixo nao leva esses campos: a comparacao tira esses campos (rcode_normalizar, ponto unico)
    Normal = (Rodada.stdout, NormalizarLinhasR(Rodada.stderr))
    Problemas = [] if Normal == SaidaEsperada else [f"x64 difere do valor fixo: stdout={Rodada.stdout!r} stderr={Normal[1]!r}"]
    if Rodada.returncode != CodigoEsperado:
        Problemas.append(f"x64: exit {Rodada.returncode}, esperado {CodigoEsperado}")
    for Fluxo, Trecho in Trechos:
        if Trecho not in (Rodada.stdout, Rodada.stderr)[Fluxo]:
            Problemas.append(f"x64: {'stdout' if Fluxo == 0 else 'stderr'} sem {Trecho!r}")
    return Problemas


def CasoTesteX64Passa(Forja, PastaSaidas):
    """forja test --backend x64 de dois_testes.fj: 2 testes passam, exit 0, bytes fixos do C (capturados na F3)."""
    return ConferirTesteX64IgualAoC(Forja, "dois_testes.fj", 0, [(0, b"ok   soma certa"), (0, b"2 testes, 0 falharam")],
                                    (b"ok   soma certa\r\nok   dobro certo\r\n2 testes, 0 falharam\r\n", b""))


def CasoTesteX64Falha(Forja, PastaSaidas):
    """forja test --backend x64 de teste_falha.fj: o expect falso relata em stderr, FALHOU em stdout, exit 1, bytes fixos do C (capturados na F3)."""
    return ConferirTesteX64IgualAoC(Forja, "teste_falha.fj", 1, [(0, b"FALHOU expect falso"), (0, b"1 testes, 1 falharam"),
                                                                  (1, b"  expect falhou: 1 == 2 (teste_falha.fj:6)")],
                                    (b"FALHOU expect falso\r\n1 testes, 1 falharam\r\n", b"  expect falhou: 1 == 2 (teste_falha.fj:6)\r\n"))


def CasoTesteJsonX64IgualAoC(Forja, PastaSaidas):
    """forja test --json (L16a) com --backend x64: o texto e o mesmo byte a byte do valor fixo capturado do C na F3, com a falha de expect (R0003) e o estouro dentro de teste (R0002)."""
    Problemas = ConferirTesteX64IgualAoC(Forja, "json_expect.fj", 1, [(0, b'{"teste": "passa", "resultado": "ok"}'),
                                                                        (0, b'"codigo": "R0003", "arquivo": "json_expect.fj", "linha": 10, "causa": "expect falhou: 1 == 2"}'),
                                                                        (0, b'{"resumo": {"total": 2, "ok": 1, "falhou": 1, "filtrados": 0}}')],
                                         (b'{"teste": "passa", "resultado": "ok"}\r\n'
                                          b'{"teste": "falha expect", "resultado": "falhou", "codigo": "R0003", "arquivo": "json_expect.fj", "linha": 10, "causa": "expect falhou: 1 == 2"}\r\n'
                                          b'{"resumo": {"total": 2, "ok": 1, "falhou": 1, "filtrados": 0}}\r\n',
                                          b"  expect falhou: 1 == 2 (json_expect.fj:10)\r\n  expect falhou: 2 == 3 (json_expect.fj:11)\r\n"), ("--json",))
    Problemas += ConferirTesteX64IgualAoC(Forja, "json_estouro.fj", 1, [(0, b'"codigo": "R0002", "arquivo": "json_estouro.fj", "linha": 2, "causa": "estouro de inteiro em soma i32: 2147483647 e 1"}'),
                                                                          (0, b'{"resumo": {"total": 2, "ok": 1, "falhou": 1, "filtrados": 0}}')],
                                          (b'{"teste": "soma pequena", "resultado": "ok"}\r\n'
                                           b'{"teste": "soma estoura", "resultado": "falhou", "codigo": "R0002", "arquivo": "json_estouro.fj", "linha": 2, "causa": "estouro de inteiro em soma i32: 2147483647 e 1"}\r\n'
                                           b'{"resumo": {"total": 2, "ok": 1, "falhou": 1, "filtrados": 0}}\r\n',
                                           (PastaAqui / "esperados" / "nativo_json_estouro_err.txt").read_bytes()), ("--json",))
    return Problemas


def CasoBackendX64Dll(Forja, PastaSaidas):
    """M3 P22a: o x64 gera DLL (era X0003): build --modo dll --backend x64 devolve 0 e grava a DLL e o .h; sem 'pub export proc' continua B0003 (exit 1)."""
    Saida = PastaSaidas / "x64_dll.dll"
    Rodada = Rodar(Forja, ["build", "--modo", "dll", "biblioteca.fj", "--backend", "x64", "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Saida.exists() or not Saida.with_suffix(".h").exists():
        Problemas.append("o build x64 da DLL nao gravou a .dll e o .h")
    Sem = Rodar(Forja, ["build", "--modo", "dll", "sem_teste.fj", "--backend", "x64", "-o", str(PastaSaidas / "x64_sem.dll")])
    return Problemas + ConferirExecucao(Sem, 1) + ConferirJsonCodigo(Sem.stderr, "B0003")


def CasoBackendXx(Forja, PastaSaidas):
    """Backend fora da lista (c|x64): X0003 no stderr e exit 2."""
    Rodada = Rodar(Forja, ["build", "sem_teste.fj", "--backend", "xx", "-o", str(PastaSaidas / "xx.exe")])
    return ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(Rodada.stderr, "X0003")


def CasoDebugCodeItemInvalido(Forja, PastaSaidas):
    """--debug-code com item desconhecido ou 'ir' (a IR nao tem impressora): X0003 no stderr, exit 2 e nenhum exe."""
    Problemas = []
    for Item in ("ir", "xyz", "bir,ir"):
        Saida = PastaSaidas / "debug_invalido.exe"
        Rodada = Rodar(Forja, ["build", "--backend", "x64", "--debug-code", Item, str(ExemploOla), "-o", str(Saida)])
        Problemas += [f"{Item}: {P}" for P in ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(Rodada.stderr, "X0003")]
        if Saida.exists():
            Problemas.append(f"{Item}: gravou o exe apesar do X0003")
    return Problemas


def CasoDebugCodeSemBackendEmDev(Forja, PastaSaidas):
    """M4 P25b: --debug-code sem --backend em dev usa o padrao x64: exit 0 e o .debug.txt ao lado do exe."""
    Saida = PastaSaidas / "debug_dev_padrao.exe"
    Rodada = Rodar(Forja, ["build", "--debug-code", "asm", str(ExemploOla), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Path(str(Saida) + ".debug.txt").exists():
        Problemas.append("nao gravou o .debug.txt")
    return Problemas


def CasoDebugCodeReleaseSemBackend(Forja, PastaSaidas):
    """0.21.0 F3: --release --debug-code sem --backend usa o x64 (o padrao do release agora e x64): exit 0 e o .debug.txt ao lado do exe."""
    Saida = PastaSaidas / "debug_release_x64.exe"
    Rodada = Rodar(Forja, ["build", "--release", "--debug-code", "asm", str(ExemploOla), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Path(str(Saida) + ".debug.txt").exists():
        Problemas.append("nao gravou o .debug.txt")
    return Problemas


def CasoDebugPassesSecao(Forja, PastaSaidas):
    """G0d: --release --debug-code passes,otim: a secao passes abre com '== entrada' e traz um '== passe <nome>' por passe que o item otim contou;
    em dev (nenhum passe roda) so a '== entrada'."""
    Problemas = []
    Saida = PastaSaidas / "debug_passes.exe"
    Rodada = Rodar(Forja, ["build", "--release", "--debug-code", "passes,otim", str(ExemploOla), "-o", str(Saida)])
    Problemas += ConferirExecucao(Rodada, 0)
    Debug = Path(str(Saida) + ".debug.txt")
    if not Debug.exists():
        return Problemas + ["nao gravou o .debug.txt"]
    Texto = Debug.read_text(encoding="utf-8")
    Passes = [L for L in Texto.splitlines() if L.startswith("== passe ")]
    Contados = re.search(r"^otim: (\d+) passes$", Texto, re.MULTILINE)
    if "## passes\n== entrada\n" not in Texto:
        Problemas.append("a secao passes nao abre com '== entrada'")
    if Contados is None or len(Passes) != int(Contados.group(1)) or not Passes:
        Problemas.append(f"passes no dump ({len(Passes)}) diferem do item otim ({Contados.group(1) if Contados else '?'})")
    SaidaDev = PastaSaidas / "debug_passes_dev.exe"
    Rodada = Rodar(Forja, ["build", "--debug-code", "passes", str(ExemploOla), "-o", str(SaidaDev)])
    Problemas += ConferirExecucao(Rodada, 0)
    TextoDev = Path(str(SaidaDev) + ".debug.txt").read_text(encoding="utf-8") if Path(str(SaidaDev) + ".debug.txt").exists() else ""
    if "== entrada\n" not in TextoDev or "== passe " in TextoDev:
        Problemas.append("em dev a secao passes deve ter so a '== entrada'")
    return Problemas


def CasoDebugPassesDeterministico(Forja, PastaSaidas):
    """G0d: dois builds no mesmo caminho: o .debug.txt de todos os itens canonicos (bir, locais, passes, otim, asm, quadros, reloc, formato, linhas)
    e o exe saem byte a byte iguais, em --release e em dev (o item tempos mede ns e fica de fora)."""
    Problemas = []
    Itens = "bir,locais,passes,otim,asm,quadros,reloc,formato,linhas"
    for Modo in ([], ["--release"]):
        Gerados = []
        for _ in (1, 2):
            Saida = PastaSaidas / "debug_passes_det.exe"  # mesmo caminho nas 2 builds: o RSDS do exe em dev leva o caminho do .pdb
            Rodada = Rodar(Forja, ["build"] + Modo + ["--debug-code", Itens, str(ExemploOla), "-o", str(Saida)])
            Problemas += ConferirExecucao(Rodada, 0)
            Debug = Path(str(Saida) + ".debug.txt")
            if not Debug.exists():
                return Problemas + ["nao gravou o .debug.txt"]
            Gerados.append((Saida.read_bytes(), Debug.read_bytes()))
        Nome = "release" if Modo else "dev"
        if Gerados[0][1] != Gerados[1][1]:
            Problemas.append(f"os dois .debug.txt diferem ({Nome})")
        if Gerados[0][0] != Gerados[1][0]:
            Problemas.append(f"os dois exes diferem ({Nome})")
    return Problemas


def CasoDebugCodeSoBuild(Forja, PastaSaidas):
    """--debug-code so vale em build: no run e uso errado (exit 2)."""
    Rodada = Rodar(Forja, ["run", "--backend", "x64", "--debug-code", "asm", str(ExemploOla)])
    return ConferirExecucao(Rodada, 2) + ([] if "--debug-code so vale em build" in Rodada.stderr else ["sem a mensagem de uso"])


def CasoDebugCodeSecoes(Forja, PastaSaidas):
    """build x64 com todos os itens grava <saida>.debug.txt com as 6 secoes e as 3 funcoes do 01_ola com unwind."""
    Saida = PastaSaidas / "debug_ola.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "--debug-code", "bir,asm,quadros,reloc,formato,tempos", str(ExemploOla), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    Arquivo = Path(str(Saida) + ".debug.txt")
    if not Arquivo.exists():
        return Problemas + ["nao gravou o .debug.txt"]
    Texto = Arquivo.read_text(encoding="utf-8")
    for Secao in ("## bir", "## asm", "## quadros", "## reloc", "## formato", "## tempos"):
        if Secao not in Texto:
            Problemas.append(f"sem a secao {Secao}")
    if len(re.findall(r"^funcao \S+ texto\+0x[0-9a-f]+ tam=.* unwind=[0-9a-f]+$", Texto, re.MULTILINE)) != 3:
        Problemas.append("esperava 3 linhas de quadro com unwind (fj_dobro, fj_main, forja_programa)")
    if "excecoes: rva=" not in Texto:
        Problemas.append("formato sem o diretorio de excecoes")
    return Problemas


def CasoDebugCodeTemposJson(Forja, PastaSaidas):
    """--tempos no build x64 traz as 10 etapas (lex a escrever) e o outros_ns."""
    Tempos = PastaSaidas / "debug_tempos.json"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", str(ExemploOla), "-o", str(PastaSaidas / "debug_tempos.exe"), "--tempos", str(Tempos)])
    Problemas = ConferirExecucao(Rodada, 0)
    Dados = json.loads(Tempos.read_text(encoding="utf-8")) if Tempos.exists() else {}
    for Chave in EtapasDeTempos + ("outros_ns",):
        if Chave not in Dados:
            Problemas.append(f"--tempos sem {Chave}")
    return Problemas


# O registro "tempos" (P24a): as 10 etapas na ordem do pipeline e o resto; os mesmos 13 nomes no --tempos e no .debug.jsonl.
EtapasDeTempos = ("lex_ns", "parse_ns", "check_ns", "ir_ns", "bir_ns", "isel_ns", "regs_ns", "codificar_ns", "ligar_ns", "escrever_ns")
CamposDeTempos = EtapasDeTempos + ("outros_ns", "total_ns", "linhas")


def CasoTemposRegistroJsonl(Forja, PastaSaidas):
    """O registro tempos do .debug.jsonl tem os 14 campos (tipo e os 13), outros_ns >= 0 e outros_ns + soma das etapas = total_ns."""
    Saida = PastaSaidas / "tempos_registro.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "--debug-code", "tempos,json", str(ExemploOla), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    if not Jsonl.exists():
        return Problemas + ["nao gravou o .debug.jsonl"]
    Registros = [R for R in LerJsonl(Jsonl) if R["tipo"] == "tempos"]
    if len(Registros) != 1:
        return Problemas + [f"esperava 1 registro tempos, achei {len(Registros)}"]
    Registro = Registros[0]
    Esperados = {"tipo"} | set(CamposDeTempos)
    if set(Registro) != Esperados:
        Problemas.append(f"campos do registro tempos diferem: faltam {sorted(Esperados - set(Registro))}, sobram {sorted(set(Registro) - Esperados)}")
        return Problemas
    if Registro["outros_ns"] < 0:
        Problemas.append("outros_ns negativo")
    if Registro["outros_ns"] + sum(Registro[C] for C in EtapasDeTempos) != Registro["total_ns"]:
        Problemas.append("outros_ns + soma das etapas != total_ns")
    if Registro["linhas"] <= 0:
        Problemas.append("linhas zerado")
    return Problemas


SoMainPoda = PastaAqui.parent / "nativo" / "m4" / "poda" / "so_main.fj"
ProcDeadComErro = """proc morto() -> i32 {
    let x: i32 = "texto"
    return x
}

proc main() {
    print(1)
}
"""


def CasoPodaRegistroJsonl(Forja, PastaSaidas):
    """P24p (B07): o registro poda do .debug.jsonl vem logo depois do cabecalho, com total, gerados e podados (so_main: 5, 3, 2); a semantica continua checando o proc podado
    (erro de tipo num proc que ninguem chama da o mesmo E-code no build x64 e no check)."""
    Saida = PastaSaidas / "poda_registro.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "--debug-code", "json", str(SoMainPoda), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    if not Jsonl.exists():
        return Problemas + ["nao gravou o .debug.jsonl"]
    Registros = LerJsonl(Jsonl)
    Podas = [R for R in Registros if R["tipo"] == "poda"]
    if len(Podas) != 1 or Registros[1]["tipo"] != "poda":
        return Problemas + [f"esperava 1 registro poda logo depois do cabecalho, achei {len(Podas)}"]
    Esperado = {"tipo": "poda", "procs_total": 5, "procs_gerados": 3, "procs_podados": 2}
    if Podas[0] != Esperado:
        Problemas.append(f"registro poda {Podas[0]!r}, esperava {Esperado!r}")
    return Problemas + ConferirPodaSemantica(Forja, PastaSaidas)


def ConferirPodaSemantica(Forja, PastaSaidas):
    """O E-code do erro de tipo num proc podado e o mesmo no check e no build x64."""
    Fonte = PastaSaidas / "poda_erro.fj"
    Fonte.write_text(ProcDeadComErro, encoding="utf-8")
    Rodadas = [Rodar(Forja, ["check", str(Fonte)]), Rodar(Forja, ["build", "--backend", "x64", str(Fonte), "-o", str(PastaSaidas / "poda_erro.exe")])]
    Codigos = [(R.returncode, (LinhaJson(R.stdout) or {}).get("codigo")) for R in Rodadas]
    if len(set(Codigos)) != 1 or Codigos[0][0] != 1 or Codigos[0][1] is None:
        return [f"erro de tipo em proc podado: (exit, codigo) de check e x64 = {Codigos!r}, esperava os dois iguais e exit 1"]
    return []


def CasoTemposArquivoX64(Forja, PastaSaidas):
    """O --tempos do build x64 tem os mesmos 13 nomes do registro e mais o emit_ns."""
    Tempos = PastaSaidas / "tempos_arquivo.json"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", str(ExemploOla), "-o", str(PastaSaidas / "tempos_arquivo.exe"), "--tempos", str(Tempos)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Tempos.exists():
        return Problemas + ["nao gravou o --tempos"]
    Dados = json.loads(Tempos.read_text(encoding="utf-8"))
    Esperados = {"emit_ns"} | set(CamposDeTempos)
    if set(Dados) != Esperados:
        Problemas.append(f"campos do --tempos diferem: faltam {sorted(Esperados - set(Dados))}, sobram {sorted(set(Dados) - Esperados)}")
    elif Dados["outros_ns"] < 0 or Dados["outros_ns"] + sum(Dados[C] for C in EtapasDeTempos) != Dados["total_ns"]:
        Problemas.append("--tempos: outros_ns + soma das etapas != total_ns")
    return Problemas


def ConferirRecusaLinux(Forja, Argumentos, Trecho, Saida=None):
    """M5 P27a: o forja com Argumentos sai 2 com X0003 no stderr, a causa traz Trecho e a Saida (se dada) nao foi gravada."""
    Rodada = Rodar(Forja, Argumentos)
    Problemas = ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(Rodada.stderr, "X0003")
    Causa = (LinhaJson(Rodada.stderr) or {}).get("causa", "")
    if Trecho not in Causa:
        Problemas.append(f"causa sem {Trecho!r}: {Causa!r}")
    if Saida is not None and Saida.exists():
        Problemas.append(f"gravou {Saida.name} apesar do X0003")
    return Problemas


def ConferirElfGerado(Forja, Argumentos, Saida):
    """M5 P27b: o forja com Argumentos sai 0, grava Saida e ela e um ELF64 LSB ET_EXEC x86-64 (7f ELF, classe 2, dados 1, e_type 2, e_machine 62)."""
    Rodada = Rodar(Forja, Argumentos)
    Problemas = ConferirExecucao(Rodada, 0)
    if not Saida.exists():
        return Problemas + [f"nao gravou {Saida.name}"]
    Bytes = Saida.read_bytes()
    Cabecalho = Bytes[:20]
    if Cabecalho[:6] != b"\x7fELF\x02\x01" or int.from_bytes(Cabecalho[16:18], "little") != 2 or int.from_bytes(Cabecalho[18:20], "little") != 62:
        Problemas.append(f"{Saida.name} nao e ELF64 LSB ET_EXEC x86-64: {Cabecalho.hex()}")
    return Problemas


def CasoTargetLinux(Forja, PastaSaidas):
    """M5 P27b: --target x86_64-linux implica --backend x64 e grava um ELF64 estatico (sem_teste.fj e o 01_ola, dev); o X0003 'escritor ELF: P27b' do P27a acabou."""
    Problemas = []
    for Fonte in ("sem_teste.fj", str(ExemploOla)):
        Problemas += ConferirElfGerado(Forja, ["build", Fonte, "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux.elf")], PastaSaidas / "linux.elf")
    return Problemas


def CasoTargetLinuxRelease(Forja, PastaSaidas):
    """M5 P27b: --release com --target x86_64-linux fica no x64 (o backend C seria X0003 do backend) e grava o ELF."""
    Saida = PastaSaidas / "linux_release.elf"
    return ConferirElfGerado(Forja, ["build", "--release", str(ExemploOla), "--target", "x86_64-linux", "-o", str(Saida)], Saida)


def CasoTargetLinuxDeterministico(Forja, PastaSaidas):
    """M5 P27b: dois builds do 01_ola para o linux (dev e release) dao bytes iguais; sem o bit x (o Windows nao o tem) e o ELF e menor que 64 KiB."""
    Problemas = []
    for Modo in ([], ["--release"]):
        Gerados = []
        for N in (1, 2):
            Saida = PastaSaidas / f"det_linux_{len(Modo)}_{N}.elf"
            Problemas += ConferirElfGerado(Forja, ["build", *Modo, str(ExemploOla), "--target", "x86_64-linux", "-o", str(Saida)], Saida)
            Gerados.append(Saida.read_bytes() if Saida.exists() else b"")
        if Gerados[0] != Gerados[1]:
            Problemas.append(f"os dois ELF ({Modo or 'dev'}) diferem")
        if len(Gerados[0]) >= 64 * 1024:
            Problemas.append(f"ELF ({Modo or 'dev'}) com {len(Gerados[0])} bytes")
    return Problemas


def CasoTargetLinuxDll(Forja, PastaSaidas):
    """M5 P27a: --modo dll com --target x86_64-linux = X0003 'biblioteca .so: M5c' (antes de ler o .fj: vale ate para arquivo inexistente)."""
    return ConferirRecusaLinux(Forja, ["build", "--modo", "dll", "biblioteca.fj", "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux.so")], "biblioteca .so: M5c", PastaSaidas / "linux.so")


def CasoTargetLinuxRun(Forja, PastaSaidas):
    """M5 P27a: forja run com --target x86_64-linux = X0003 'o Windows nao executa ELF: build e rode no Linux'."""
    return ConferirRecusaLinux(Forja, ["run", "sem_teste.fj", "--target", "x86_64-linux"], "o Windows nao executa ELF: build e rode no Linux")


def CasoTargetLinuxTest(Forja, PastaSaidas):
    """M5 P27b: forja test com --target x86_64-linux so constroi o exe de teste: sem -o = X0003 'passe -o'; com -o grava o ELF de teste (nao o executa)."""
    Problemas = ConferirRecusaLinux(Forja, ["test", "dois_testes.fj", "--target", "x86_64-linux"], "passe -o")
    Saida = PastaSaidas / "linux_teste.elf"
    return Problemas + ConferirElfGerado(Forja, ["test", "dois_testes.fj", "--target", "x86_64-linux", "-o", str(Saida)], Saida)


def CasoTargetLinuxTestOpcoes(Forja, PastaSaidas):
    """M5 P29b: --json e --filtro valem no test do alvo linux e chegam ao exe so por ambiente na hora de rodar: com -o o forja so constroi (ELF64, stdout vazio, nada executado no Windows)."""
    Saida = PastaSaidas / "linux_teste_opcoes.elf"
    Argumentos = ["test", "dois_testes.fj", "--target", "x86_64-linux", "--json", "--filtro", "dois", "-o", str(Saida)]
    Problemas = ConferirElfGerado(Forja, Argumentos, Saida)
    Rodada = Rodar(Forja, Argumentos)
    return Problemas + ([f"stdout nao vazio (o test linux nao executa): {Rodada.stdout[:80]!r}"] if Rodada.stdout else [])


def CasoTargetLinuxTestStd(Forja, PastaSaidas):
    """M5 P29b: forja test std:<modulo> --target x86_64-linux -o grava o exe de teste do modulo (o testes.fj anexado ao texto dele); ELF64 e nada executado."""
    Saida = PastaSaidas / "linux_teste_std.elf"
    return ConferirElfGerado(Forja, ["test", "std:path", "--target", "x86_64-linux", "-o", str(Saida)], Saida)


def CasoTargetLinuxForeign(Forja, PastaSaidas):
    """M5b P42: foreign com 'link' (foreign_libm.fj) LIGA no x86_64-linux pela semente musl (era o X0003 'M5b' do P27a, que acabou no P39): grava um ELF64 em dev e em --release."""
    Problemas = ConferirElfGerado(Forja, ["build", "foreign_libm.fj", "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_foreign_dev.elf")], PastaSaidas / "linux_foreign_dev.elf")
    return Problemas + ConferirElfGerado(Forja, ["build", "--release", "foreign_libm.fj", "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_foreign_rel.elf")], PastaSaidas / "linux_foreign_rel.elf")


def CasoTargetLinuxProcessoLiga(Forja, PastaSaidas):
    """M5 P28b-2: std:process LIGA no x86_64-linux (o X0003 interino do P28b-1 acabou): linux_processo.fj grava um ELF64 em dev e em --release."""
    Problemas = ConferirElfGerado(Forja, ["build", "linux_processo.fj", "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_processo_dev.elf")], PastaSaidas / "linux_processo_dev.elf")
    return Problemas + ConferirElfGerado(Forja, ["build", "--release", "linux_processo.fj", "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_processo_rel.elf")], PastaSaidas / "linux_processo_rel.elf")


def CasoTargetLinuxFileOsLigam(Forja, PastaSaidas):
    """M5 P28b-1: std:file (handle e fs), std:os (A52) e o stdin LIGAM no x86_64-linux (o X0003 de M5/M5b acabou): a fixture handle.fj da suite es\\ grava um ELF64 em dev e em --release."""
    Fonte = str(PastaAqui.parents[1] / "tests" / "nativo" / "linux" / "es" / "handle.fj")
    Problemas = ConferirElfGerado(Forja, ["build", Fonte, "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_es_dev.elf")], PastaSaidas / "linux_es_dev.elf")
    return Problemas + ConferirElfGerado(Forja, ["build", "--release", Fonte, "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_es_rel.elf")], PastaSaidas / "linux_es_rel.elf")


def CasoTargetLinuxRedeLibsDoWindows(Forja, PastaSaidas):
    """R10a (D-REDE-7): o vendor:net/net (link ws2_32, crypt32, bcrypt, advapi32, secur32, iphlpapi, msvcrt + curl) LIGA no x86_64-linux: as libs so do Windows sao 'nada a ligar' e o curl sai de vendor\\linux\\libcurl.a (sem X0102); ELF64 em dev e release."""
    Fonte = str(PastaAqui.parents[1] / "exemplos_libs" / "http_local.fj")
    Problemas = ConferirElfGerado(Forja, ["build", Fonte, "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_rede_dev.elf")], PastaSaidas / "linux_rede_dev.elf")
    return Problemas + ConferirElfGerado(Forja, ["build", "--release", Fonte, "--target", "x86_64-linux", "-o", str(PastaSaidas / "linux_rede_rel.elf")], PastaSaidas / "linux_rede_rel.elf")


def CasoTargetLinuxProjeto(Forja, PastaSaidas):
    """M5 P27a: build --projeto com --target x86_64-linux = X0003 (o orquestrador so gera x86_64-windows nesta versao)."""
    return ConferirRecusaLinux(Forja, ["build", "--projeto", "--target", "x86_64-linux"], "so gera x86_64-windows")


def CasoSuporteDoisAlvos(Forja, PastaSaidas):
    """M5 P27a/P27b: --suporte --json lista os dois alvos (windows: pe, exe/test/dll, escritor; linux: elf, exe/test, escritor desde o P27b), na tabela e no relatorio de um .fj."""
    Esperado = [{"nome": "x86_64-windows", "formato": "pe", "gera": ["exe", "test", "dll"], "escritor": True},
                {"nome": "x86_64-linux", "formato": "elf", "gera": ["exe", "test"], "escritor": True}]
    Problemas = []
    for Extra in ([], ["../nativo/x/callback.fj"]):
        Rodada = Rodar(Forja, ["build", "--backend", "x64", "--suporte", "--json", *Extra])
        Problemas += ConferirExecucao(Rodada, 0)
        Dados = LinhaJson(Rodada.stdout) or {}
        if Dados.get("alvos") != Esperado or Dados.get("alvo") != "x86_64-windows" or Dados.get("suporte_versao") != "0.2.0":
            Problemas.append(f"--suporte {Extra}: alvos {Dados.get('alvos')!r}, alvo {Dados.get('alvo')!r}")
    return Problemas


def CasoTargetXx(Forja, PastaSaidas):
    """Alvo fora da lista: X0002 no stderr e exit 2."""
    Rodada = Rodar(Forja, ["build", "sem_teste.fj", "--target", "xx", "-o", str(PastaSaidas / "xx_alvo.exe")])
    return ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(Rodada.stderr, "X0002")


def CasoExplicarX0001(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0001"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0001")


def CasoExplicarX0002(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0002"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0002")


def CasoExplicarX0003(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0003"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0003")


def CasoExplicarX0302(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0302"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0302")


def CasoExplicarX0901(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0901"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0901")


def CasoExplicarX0902(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0902"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0902")


def CasoExplicarX0903(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0903"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0903")


def CasoExplicarX0904(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0904"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0904")


def CasoExplicarX0905(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0905"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0905")


def CasoExplicarX0906(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "X0906"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "X0906")


def ConferirExplicarDiferencial(Forja, Codigo):
    """forja explicar X080x: 1 linha JSON com o codigo, tipo "diferencial" e explicacao e forma_certa nao vazias (a fonte unica do X0801..X0803 do verificar_nativo)."""
    Rodada = Rodar(Forja, ["explicar", Codigo])
    Dados = LinhaJson(Rodada.stdout) or {}
    Problemas = ConferirExecucao(Rodada, 0) + ConferirJsonCodigo(Rodada.stdout, Codigo)
    if Dados.get("tipo") != "diferencial" or not Dados.get("explicacao") or not Dados.get("forma_certa"):
        Problemas.append(f"{Codigo}: tipo/explicacao/forma_certa inesperados: {Dados!r}")
    return Problemas


def CasoExplicarX0801(Forja, PastaSaidas):
    return ConferirExplicarDiferencial(Forja, "X0801")


def CasoExplicarX0802(Forja, PastaSaidas):
    return ConferirExplicarDiferencial(Forja, "X0802")


def CasoExplicarX0803(Forja, PastaSaidas):
    return ConferirExplicarDiferencial(Forja, "X0803")


# fixture -> (linha, coluna, causa): o X0001 do BIR aponta a construcao da IR que o M1 ainda nao cobre.
X0001_BIR = {
    "resultado": (6, 10, "IR_FUNCAO retorno resultado ainda nao suportado no backend nativo (marco M2)"),
    "foreign": (9, 11, "IR_CHAMADA foreign ainda nao suportado no backend nativo (marco M3)"),
}


def CasoBirX0001(Forja, Nome):
    """teste_nativo.exe bir <fixture>: exit 1 e 1 linha JSON X0001 com arquivo, linha, coluna e causa exatos."""
    Driver = Path(Forja).parent / "teste_nativo.exe"
    if not Driver.exists():
        return [f"{Driver} nao existe (rode construir.bat)"]
    Arquivo = f"../nativo/x/{Nome}.fj"
    Rodada = subprocess.run(
        [str(Driver), "bir", Arquivo], capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=TimeoutSegundos, cwd=PastaAqui,
    )
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "X0001")
    Dados = LinhaJson(Rodada.stdout) or {}
    Linha, Coluna, Causa = X0001_BIR[Nome]
    Esperado = {"tipo": "backend", "arquivo": Arquivo, "linha": Linha, "coluna": Coluna, "causa": Causa}
    for Chave, Valor in Esperado.items():
        if Dados.get(Chave) != Valor:
            Problemas.append(f"{Chave} {Dados.get(Chave)!r}, esperado {Valor!r}")
    return Problemas


def ConferirX64IgualAoC(Forja, PastaSaidas, Nome, Esperado):
    """O exe do x64 de nativo/x/<Nome>.fj sai 0 e imprime exatamente Esperado (valor fixo, o mesmo que o C dava antes da F3)."""
    Saida = PastaSaidas / f"{Nome}_x64.exe"
    Rodada = Rodar(Forja, ["build", f"../nativo/x/{Nome}.fj", "--backend", "x64", "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Saida.exists():
        return Problemas + ["build --backend x64 nao gravou o exe"]
    Execucao = subprocess.run([str(Saida)], capture_output=True, timeout=TimeoutSegundos, cwd=PastaAqui)
    if Execucao.returncode != 0 or Execucao.stdout.replace(b"\r\n", b"\n") != Esperado:
        Problemas.append(f"x64: exit {Execucao.returncode}, stdout {Execucao.stdout!r}, esperado {Esperado!r}")
    return Problemas


def CasoProcIndX64IgualAoC(Forja, PastaSaidas):
    """x/proc_ind.fj (let f = proc; f(21)) deixou de ser X0001 no M3 P21v-c: o exe do x64 imprime 42 e saem 0."""
    return ConferirX64IgualAoC(Forja, PastaSaidas, "proc_ind", b"42\n")


def CasoRealX64IgualAoC(Forja, PastaSaidas):
    """x/real.fj (let f32) deixou de ser X0001 no M2 P10b: o exe do x64 imprime 1 e saem 0."""
    return ConferirX64IgualAoC(Forja, PastaSaidas, "real", b"1\n")


def CasoPrintStrX64IgualAoC(Forja, PastaSaidas):
    """x/print_str.fj (let str e print) deixou de ser X0001 no M2 P13a: o exe do x64 imprime Forja e saem 0."""
    return ConferirX64IgualAoC(Forja, PastaSaidas, "print_str", b"Forja\n")


def CasoStructX64IgualAoC(Forja, PastaSaidas):
    """x/struct.fj (literal de struct e p.x) deixou de ser X0001 no M2 P11a: o exe do x64 imprime 1 e saem 0."""
    return ConferirX64IgualAoC(Forja, PastaSaidas, "struct", b"1\n")


def CasoResultadoX64IgualAoC(Forja, PastaSaidas):
    """x/resultado.fj (T ! E com or_else) deixou de ser X0001 no M2 P12a: o exe do x64 imprime 6 e saem 0."""
    return ConferirX64IgualAoC(Forja, PastaSaidas, "resultado", b"6\n")


def CasoDivisaoX64IgualAoC(Forja, PastaSaidas):
    """x/divisao.fj (84 / 2) deixou de ser X0001 no M2 P9: o exe do x64 imprime 42 e saem 0."""
    return ConferirX64IgualAoC(Forja, PastaSaidas, "divisao", b"42\n")


def CasoForeignX64IgualAoC(Forja, PastaSaidas):
    """x/foreign_crt.fj (labs do CRT, sem 'link') deixou de ser X0001 no M3 P19: o exe do x64 imprime 7 e saem 0.
    x/foreign.fj (link "c", que o gcc deste PC nao liga: sem libc.a) so no x64, onde "c" vira a ucrtbase.dll: imprime 7 e sai 0."""
    Problemas = ConferirX64IgualAoC(Forja, PastaSaidas, "foreign_crt", b"7\n")
    Saida = PastaSaidas / "foreign_link_c.exe"
    Rodada = Rodar(Forja, ["build", "../nativo/x/foreign.fj", "--backend", "x64", "-o", str(Saida)])
    Problemas += ConferirExecucao(Rodada, 0)
    if not Saida.exists():
        return Problemas + ["build --backend x64 de foreign.fj (link \"c\") nao gravou o exe"]
    Execucao = subprocess.run([str(Saida)], capture_output=True, timeout=TimeoutSegundos, cwd=PastaAqui)
    if Execucao.returncode != 0 or Execucao.stdout.replace(b"\r\n", b"\n") != b"7\n":
        Problemas.append(f"x64 link \"c\": exit {Execucao.returncode}, stdout {Execucao.stdout!r}, esperado b'7\\n'")
    return Problemas


def PastaDasFixturesX():
    return (PastaAqui / ".." / "nativo" / "x").resolve()


def ConferirX01xx(Forja, PastaSaidas, Nome, Codigo, Esperado):
    """build --backend x64 de nativo/x/<Nome>.fj: exit 1, 1 linha JSON do Codigo, sem pacote de relato nem sugestao, interno:false e os campos de Esperado exatos."""
    Saida = PastaSaidas / f"{Nome}.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", f"../nativo/x/{Nome}.fj", "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, Codigo)
    Dados = LinhaJson(Rodada.stdout) or {}
    for Chave, Valor in {**Esperado, "tipo": "backend", "alvo": "x86_64-windows", "etapa": "ligar", "interno": False}.items():
        if Dados.get(Chave) != Valor:
            Problemas.append(f"{Chave} {Dados.get(Chave)!r}, esperado {Valor!r}")
    if "relato" in Dados or "sugestao" in Dados:
        Problemas.append(f"{Codigo} nao gera pacote de relato nem sugestao")
    if Saida.exists():
        Problemas.append(f"{Codigo} gravou o exe")
    return Problemas


def PastaSistema():
    return os.environ.get("SystemRoot", "C:\\Windows").replace("\\", "/") + "/System32"


def CasoX0101SimboloSemBiblioteca(Forja, PastaSaidas):
    """x/foreign_x0101.fj (link "kernel32" e um proc que ninguem exporta): X0101 com proc, simbolo e procurei_em = kernel32 e o CRT implicito, na ordem."""
    Sistema = PastaSistema()
    return ConferirX01xx(Forja, PastaSaidas, "foreign_x0101", "X0101", {
        "arquivo": "../nativo/x/foreign_x0101.fj", "linha": 5, "coluna": 1,
        "causa": "simbolo 'FuncaoQueNaoExiste' do proc foreign 'FuncaoQueNaoExiste' nao encontrado nas bibliotecas do 'link'",
        "proc": "FuncaoQueNaoExiste", "simbolo": "FuncaoQueNaoExiste",
        "procurei_em": [f"{Sistema}/kernel32.dll", f"{Sistema}/ucrtbase.dll"]})


def CasoX0102LinkSemArquivo(Forja, PastaSaidas):
    """x/foreign_x0102.fj (link "fx_que_nao_existe"): X0102 com a biblioteca e os 8 candidatos tentados (7 na pasta do .fj e o System32)."""
    Pasta = PastaDasFixturesX().as_posix()
    Nome = "fx_que_nao_existe"
    Candidatos = [f"{Pasta}/{N}" for N in (f"lib{Nome}.dll.a", f"{Nome}.dll.a", f"lib{Nome}.a", f"{Nome}.lib", f"lib{Nome}.lib", f"lib{Nome}.dll", f"{Nome}.dll")]
    Candidatos.append(f"{PastaSistema()}/{Nome}.dll")
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "../nativo/x/foreign_x0102.fj", "-o", str(PastaSaidas / "foreign_x0102.exe")])
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "X0102")
    Dados = LinhaJson(Rodada.stdout) or {}
    Achados = [C.lower() for C in Dados.get("procurei_em", [])]
    if Dados.get("biblioteca") != Nome or Achados != [C.lower() for C in Candidatos]:
        Problemas.append(f"biblioteca {Dados.get('biblioteca')!r} / procurei_em {Dados.get('procurei_em')!r}, esperado {Nome!r} / {Candidatos!r}")
    if Dados.get("linha") != 5 or Dados.get("interno") is not False or "relato" in Dados:
        Problemas.append(f"posicao ou interno inesperados: {Dados.get('linha')!r} {Dados.get('interno')!r}")
    return Problemas


def CasoX0103BibliotecaEstatica(Forja, PastaSaidas):
    """link "fx_estatica" acha libfx_estatica.a (ar criado na hora com o gcc): desde o M3b P23e a estatica LIGA (antes era X0103); o exe sai com exit 0 e imprime 3."""
    Fonte = PastaSaidas / "foreign_x0103.fj"
    Objeto = PastaSaidas / "fx_estatica.o"
    shutil.copy2(PastaDasFixturesX() / "foreign_x0103.fj", Fonte)
    (PastaSaidas / "fx_estatica.c").write_text("int fx_soma(int a, int b) { return a + b; }\n", encoding="utf-8")
    Preparo = [subprocess.run(["gcc", "-c", str(PastaSaidas / "fx_estatica.c"), "-o", str(Objeto)], capture_output=True),
               subprocess.run(["ar", "rcs", str(PastaSaidas / "libfx_estatica.a"), str(Objeto)], capture_output=True)]
    if any(P.returncode != 0 for P in Preparo):
        return ["gcc ou ar nao criou a libfx_estatica.a"]
    Saida = PastaSaidas / "foreign_x0103.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", str(Fonte), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    if Problemas:
        return Problemas
    Execucao = subprocess.run([str(Saida)], capture_output=True, timeout=TimeoutSegundos, cwd=PastaAqui)
    if Execucao.returncode != 0 or Execucao.stdout.decode("utf-8", errors="replace").strip() != "3":
        Problemas.append(f"exe da estatica: exit {Execucao.returncode}, stdout {Execucao.stdout!r}, esperado '3'")
    return Problemas


def CasoX0105ImportLibrary32Bits(Forja, PastaSaidas):
    """x/foreign_x0105.fj: link "fx_lib32" acha fx_lib32.lib (import library i386 do dlltool, artefato versionado): X0105 com a biblioteca e a causa '32 bits', exit 1."""
    Saida = PastaSaidas / "foreign_x0105.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "../nativo/x/foreign_x0105.fj", "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "X0105")
    Dados = LinhaJson(Rodada.stdout) or {}
    if Dados.get("biblioteca") != "fx_lib32" or "32 bits" not in Dados.get("causa", "") or "fx_lib32.lib" not in Dados.get("causa", ""):
        Problemas.append(f"biblioteca {Dados.get('biblioteca')!r}, causa {Dados.get('causa')!r}: esperado fx_lib32 e '32 bits' com o nome do arquivo")
    if Dados.get("linha") != 8 or Dados.get("tipo") != "backend" or Dados.get("interno") is not False or "relato" in Dados:
        Problemas.append(f"posicao ou tipo inesperados: linha {Dados.get('linha')!r}, tipo {Dados.get('tipo')!r}, interno {Dados.get('interno')!r}")
    if Saida.exists():
        Problemas.append("X0105 gravou o exe")
    return Problemas


FIXTURE_ENTRADAS_RAIZ = (
    'import "mod_a"\nimport "mod_b"\n\nforeign {\n    link "fx_basica"\n    proc fx_i32(a: i32, b: i32) -> i32\n}\n\n'
    "proc main() {\n    print(mod_a.um() + mod_b.dois())\n    print(fx_i32(10, b = 3))\n}\n"
)


def MontarProjetoEntradas(PastaSaidas, Nome):
    """<Nome>\\proj (raiz.fj que importa mod_a e mod_b e chama fx_i32 da fx_basica) e <Nome>\\libs (so a fx_basica.dll, fora da pasta do .fj)."""
    Pasta = PastaSaidas / Nome
    (Pasta / "proj").mkdir(parents=True)
    (Pasta / "libs").mkdir()
    (Pasta / "proj" / "raiz.fj").write_text(FIXTURE_ENTRADAS_RAIZ, encoding="utf-8", newline="\n")
    (Pasta / "proj" / "mod_a.fj").write_text("pub proc um() -> int {\n    return 1\n}\n", encoding="utf-8", newline="\n")
    (Pasta / "proj" / "mod_b.fj").write_text("pub proc dois() -> int {\n    return 2\n}\n", encoding="utf-8", newline="\n")
    shutil.copy2(PastaDasFixturesX().parent / "m3" / "libs" / "fx_basica.dll", Pasta / "libs" / "fx_basica.dll")
    return Pasta


def Fnv1a64(Dados):
    """FNV-1a de 64 bits (o Hash64 do compilador: common\\hash.c)."""
    Hash = 0xCBF29CE484222325
    for Byte in Dados:
        Hash = ((Hash ^ Byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return Hash


def LinhaDeEntrada(Tipo, Arquivo):
    """A linha do .entradas que o forja deve gravar para o arquivo: tipo, caminho absoluto, tamanho, mtime (100 ns) e Hash64 em 16 digitos hex."""
    Estado = Arquivo.stat()
    Mtime = Estado.st_mtime_ns // 100 + 116444736000000000  # FILETIME: 100 ns desde 1601
    return f"{Tipo}\t{os.path.normpath(str(Arquivo))}\t{Estado.st_size}\t{Mtime}\t{Fnv1a64(Arquivo.read_bytes()):016x}"


def CasoEntradasListaRaizModulosEDll(Forja, PastaSaidas):
    """M3 P22-O1a: build x64 --entradas --lib-pasta: exit 0 e o .entradas = cabecalho + a DLL (lib) + a raiz e os 2 modulos (fj), em ordem de caminho, com tamanho, mtime e Hash64 certos."""
    Pasta = MontarProjetoEntradas(PastaSaidas, "entradas_lista")
    Entradas = Pasta / "raiz.entradas"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", str(Pasta / "proj" / "raiz.fj"), "-o", str(Pasta / "raiz.exe"),
                           "--entradas", str(Entradas), "--lib-pasta", str(Pasta / "libs")])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Entradas.exists():
        return Problemas + ["o build nao gravou o .entradas"]
    Esperado = "# forja-entradas 1\n" + "".join(L + "\n" for L in sorted(
        [LinhaDeEntrada("lib", Pasta / "libs" / "fx_basica.dll")] + [LinhaDeEntrada("fj", Pasta / "proj" / F) for F in ("raiz.fj", "mod_a.fj", "mod_b.fj")],
        key=lambda Linha: Linha.split("\t")[1]))
    Gravado = Entradas.read_text(encoding="utf-8")
    if Gravado.lower() != Esperado.lower():  # a caixa do caminho depende de como o Windows o devolve
        Problemas.append(f".entradas diferente do esperado:\n--- gravado\n{Gravado}--- esperado\n{Esperado}")
    if list(Pasta.glob("*.tmp*")):
        Problemas.append("sobrou arquivo temporario .tmp<pid> na pasta de saida")
    return Problemas


def CasoEntradasComBackendC(Forja, PastaSaidas):
    """M3 P22-O1a: --entradas (e --lib-pasta) so existem com --backend x64: com --backend c, X0003 no stderr, exit 2, stdout vazio e nada gravado."""
    Pasta = MontarProjetoEntradas(PastaSaidas, "entradas_backend_c")
    Entradas = Pasta / "raiz.entradas"
    Rodada = Rodar(Forja, ["build", "--backend", "c", str(Pasta / "proj" / "raiz.fj"), "-o", str(Pasta / "raiz.exe"), "--entradas", str(Entradas)])
    Problemas = ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(UltimaLinha(Rodada.stderr), "X0003")
    if Rodada.stdout.strip() or Entradas.exists() or (Pasta / "raiz.exe").exists():
        Problemas.append("stdout nao vazio ou o build gravou o .entradas ou o exe")
    return Problemas


def CasoLibPastaAchaDllForaDaPastaDoFonte(Forja, PastaSaidas):
    """M3 P22-O1a: sem --lib-pasta a fx_basica.dll (fora da pasta do .fj) e X0102; com --lib-pasta o exe sai e, com a pasta no PATH, imprime 3 e 7."""
    Pasta = MontarProjetoEntradas(PastaSaidas, "lib_pasta")
    Argumentos = ["build", "--backend", "x64", str(Pasta / "proj" / "raiz.fj"), "-o", str(Pasta / "raiz.exe")]
    Sem = Rodar(Forja, Argumentos)
    Problemas = ConferirExecucao(Sem, 1) + ConferirJsonCodigo(Sem.stdout, "X0102")
    Com = Rodar(Forja, Argumentos + ["--lib-pasta", str(Pasta / "libs")])
    Problemas += ConferirExecucao(Com, 0)
    if not (Pasta / "raiz.exe").exists():
        return Problemas + ["--lib-pasta nao achou a DLL: o exe nao foi gravado"]
    Ambiente = {**os.environ, "PATH": str(Pasta / "libs") + os.pathsep + os.environ.get("PATH", "")}
    Execucao = subprocess.run([str(Pasta / "raiz.exe")], capture_output=True, timeout=TimeoutSegundos, env=Ambiente)
    if Execucao.returncode != 0 or Execucao.stdout.replace(b"\r\n", b"\n") != b"3\n7\n":
        Problemas.append(f"exe: exit {Execucao.returncode}, stdout {Execucao.stdout!r}, esperado b'3\\n7\\n'")
    return Problemas


def MontarProjetoMinimo(PastaSaidas, Nome):
    """<Nome>\\forja.proj com um exe 'app' (app.fj imprime 1); devolve o caminho do forja.proj."""
    Pasta = PastaSaidas / Nome
    Pasta.mkdir(parents=True)
    (Pasta / "app.fj").write_text("proc main() {\n    print(1)\n}\n", encoding="utf-8", newline="\n")
    (Pasta / "forja.proj").write_text("saida = bin\nproduto app exe app.fj\n", encoding="utf-8", newline="\n")
    return Pasta / "forja.proj"


def CasoProjetoComBackendC(Forja, PastaSaidas):
    """M3 P22-O2: build --projeto --backend c e X0003 (o orquestrador so usa o backend x64): exit 2, JSON no stderr, stdout vazio e nada gravado (nem bin nem .forja_cache)."""
    Manifesto = MontarProjetoMinimo(PastaSaidas, "projeto_backend_c")
    Rodada = Rodar(Forja, ["build", "--projeto", str(Manifesto), "--backend", "c"])
    Problemas = ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(UltimaLinha(Rodada.stderr), "X0003")
    if Rodada.stdout.strip() or (Manifesto.parent / "bin").exists() or (Manifesto.parent / ".forja_cache").exists():
        Problemas.append("stdout nao vazio ou o orquestrador criou bin ou .forja_cache")
    return Problemas


def CasoProjetoComSaida(Forja, PastaSaidas):
    """M3 P22-O2: build --projeto com -o e uso errado (a saida vem do forja.proj): exit 2, mensagem no stderr, stdout vazio e nada gravado."""
    Manifesto = MontarProjetoMinimo(PastaSaidas, "projeto_com_saida")
    Rodada = Rodar(Forja, ["build", "--projeto", str(Manifesto), "-o", str(Manifesto.parent / "x.exe")])
    Problemas = ConferirExecucao(Rodada, 2)
    if "--projeto nao aceita -o" not in Rodada.stderr or Rodada.stdout.strip() or (Manifesto.parent / ".forja_cache").exists():
        Problemas.append(f"sem a mensagem de uso, ou stdout nao vazio, ou gravou algo: {Rodada.stderr.strip()[:120]!r}")
    return Problemas


# FORJA_TESTE_INJETAR: o codigo injetado -> a etapa do pipeline em que o erro nasce (so teste; documentado no nativo_erro.c).
ETAPA_DO_INJETADO = {"X0901": "isel", "X0902": "codificar", "X0903": "ligar", "X0904": "bir", "X0905": "escrever", "X0906": "layout"}
ARQUIVOS_DO_RELATO = ("comando.txt", "versao.txt", "bir.txt", "erro.json", "LEIAME.txt", "01_ola.fj")


def CasoX0302PastaExistente(Forja, PastaSaidas):
    """-o apontando para uma pasta que ja existe: X0302 no stderr (ultima linha), exit 2, stdout vazio e nenhum exe gravado."""
    Pasta = PastaSaidas / "saida_e_pasta"
    Pasta.mkdir()
    Rodada = Rodar(Forja, ["build", "--backend", "x64", str(ExemploOla), "-o", str(Pasta)])
    Problemas = ConferirExecucao(Rodada, 2) + ConferirJsonCodigo(UltimaLinha(Rodada.stderr), "X0302")
    if Rodada.stdout.strip():
        Problemas.append(f"stdout nao esta vazio: {Rodada.stdout.strip()[:80]!r}")
    if not Pasta.is_dir():
        Problemas.append("a pasta de saida deixou de ser pasta")
    return Problemas


def ConferirPacoteRelato(Pasta):
    """O pacote de relato do X09xx: arquivos esperados, LEIAME de 4 linhas (texto de nativo_erro.c 0.21.0) e bir.txt com ao menos uma funcao."""
    Problemas = [f"relato sem {Nome}" for Nome in ARQUIVOS_DO_RELATO if not (Pasta / Nome).is_file()]
    Leiame = Pasta / "LEIAME.txt"
    if Leiame.is_file() and len(Leiame.read_text(encoding="utf-8").splitlines()) != 4:
        Problemas.append("LEIAME.txt nao tem 4 linhas")
    Bir = Pasta / "bir.txt"
    if Bir.is_file() and "funcao " not in Bir.read_text(encoding="utf-8"):
        Problemas.append("bir.txt sem nenhuma funcao")
    return Problemas


def ApagarRelato(Pasta):
    """Apaga o pacote so se for uma pasta forja_relato_* do TEMP (nunca outra coisa)."""
    PastaTemporaria = Path(tempfile.gettempdir()).resolve()
    if Pasta.name.startswith("forja_relato_") and Pasta.resolve().parent == PastaTemporaria:
        shutil.rmtree(Pasta, ignore_errors=True)


def CasoInjetado(Forja, PastaSaidas, Codigo):
    """FORJA_TESTE_INJETAR=<Codigo>: exit 3, JSON com interno:true, a etapa certa, o ir da main e o pacote de relato."""
    Ambiente = dict(os.environ, FORJA_TESTE_INJETAR=Codigo)
    Saida = PastaSaidas / f"injetado_{Codigo}.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", str(ExemploOla), "-o", str(Saida)], Ambiente)
    Problemas = ConferirExecucao(Rodada, 3) + ConferirJsonCodigo(Rodada.stdout, Codigo)
    Dados = LinhaJson(Rodada.stdout) or {}
    if Dados.get("interno") is not True:
        Problemas.append("interno nao e true")
    if Dados.get("etapa") != ETAPA_DO_INJETADO[Codigo]:
        Problemas.append(f"etapa {Dados.get('etapa')!r}, esperado {ETAPA_DO_INJETADO[Codigo]!r}")
    if Dados.get("alvo") != "x86_64-windows" or Dados.get("ir", {}).get("funcao") != "main":
        Problemas.append(f"alvo/ir inesperado: {Dados.get('alvo')!r} {Dados.get('ir')!r}")
    Relato = Path(Dados["relato"]) if Dados.get("relato") else None
    if Relato is None or not Relato.is_dir():
        return Problemas + [f"pasta do relato nao existe: {Dados.get('relato')!r}"]
    Problemas += ConferirPacoteRelato(Relato)
    ApagarRelato(Relato)
    return Problemas


def CasoInjetadoX0901(Forja, PastaSaidas):
    return CasoInjetado(Forja, PastaSaidas, "X0901")


def CasoInjetadoX0902(Forja, PastaSaidas):
    return CasoInjetado(Forja, PastaSaidas, "X0902")


def CasoInjetadoX0903(Forja, PastaSaidas):
    return CasoInjetado(Forja, PastaSaidas, "X0903")


def CasoInjetadoX0904(Forja, PastaSaidas):
    return CasoInjetado(Forja, PastaSaidas, "X0904")


def CasoInjetadoX0905(Forja, PastaSaidas):
    return CasoInjetado(Forja, PastaSaidas, "X0905")


def CasoInjetadoX0906(Forja, PastaSaidas):
    return CasoInjetado(Forja, PastaSaidas, "X0906")


def CasoX0001ComIr(Forja, PastaSaidas):
    """build x64 de callback.fj com FORJA_TESTE_INJETAR=X0001 (nao resta construcao com X0001 real: proc como valor e chamada indireta rodam no x64 desde o M3 P21v-c): X0001 (exit 1) com etapa bir, ir da instrucao, interno:false e SEM sugestao (0.21.0 F4 passo 3: o backend C saiu e a forma_certa diz que nao ha backend alternativo)."""
    Ambiente = dict(os.environ, FORJA_TESTE_INJETAR="X0001")
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "../nativo/x/callback.fj", "-o", str(PastaSaidas / "callback_x0001.exe")], Ambiente)
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "X0001")
    Dados = LinhaJson(Rodada.stdout) or {}
    Esperado = {"alvo": "x86_64-windows", "etapa": "bir", "interno": False,
                "ir": {"funcao": "main", "instr": 0, "op": "IR_ARRAY", "op_aux": 0, "tipo": "array"},
                "sugestao": None}
    for Chave, Valor in Esperado.items():
        if Dados.get(Chave) != Valor:
            Problemas.append(f"{Chave} {Dados.get(Chave)!r}, esperado {Valor!r}")
    if "relato" in Dados:
        Problemas.append("X0001 nao gera pacote de relato")
    return Problemas


def SecaoTextoDoPe(Dados):
    """(rva, tamanho_virtual, offset_no_arquivo) da secao .text de um PE32+."""
    Pe = int.from_bytes(Dados[0x3C:0x40], "little")
    Secoes = int.from_bytes(Dados[Pe + 6:Pe + 8], "little")
    Tabela = Pe + 24 + int.from_bytes(Dados[Pe + 20:Pe + 22], "little")
    for K in range(Secoes):
        Cabeca = Dados[Tabela + 40 * K:Tabela + 40 * (K + 1)]
        if Cabeca[:8].rstrip(b"\0") == b".text":
            return (int.from_bytes(Cabeca[12:16], "little"), int.from_bytes(Cabeca[8:12], "little"),
                    int.from_bytes(Cabeca[20:24], "little"))
    return None


def ConferirCobertura(Registros, Dados, Texto=None):
    """Cada byte do .text do exe cai em exatamente 1 instr ou faixa, e os bytes das instr batem com o exe. Texto = (rva, tamanho, offset de arquivo); None = ler do PE."""
    Texto = Texto or SecaoTextoDoPe(Dados)
    if Texto is None:
        return ["exe sem .text"]
    Rva, Tamanho, Arquivo = Texto
    Itens = sorted((R["rva"], len(R["bytes"]) // 2 if R["tipo"] == "instr" else R["tamanho"], R)
                   for R in Registros if R["tipo"] in ("instr", "faixa"))
    Problemas = []
    Proximo = Rva
    for Inicio, Tam, Registro in Itens:
        if Inicio != Proximo:
            Problemas.append(f"buraco ou sobreposicao em {Proximo:#x}: proximo registro em {Inicio:#x}")
        Proximo = Inicio + Tam
        Real = Dados[Arquivo + Inicio - Rva:Arquivo + Inicio - Rva + Tam]
        if Registro["tipo"] == "instr" and Real.hex() != Registro["bytes"]:
            Problemas.append(f"bytes de {Inicio:#x} diferem do exe")
    if Proximo != Rva + Tamanho:
        Problemas.append(f"cobertura acaba em {Proximo:#x}, a secao em {Rva + Tamanho:#x}")
    return Problemas


def LerJsonl(Caminho):
    return [json.loads(Linha) for Linha in Caminho.read_text(encoding="utf-8").splitlines()]


def CasoDebugJsonl(Forja, PastaSaidas):
    """--debug-code json do 01_ola: cada linha valida no esquema 0.2.0, 100% do .text coberto e texto gerado dos mesmos registros."""
    try:
        import jsonschema
    except ImportError:
        return ["jsonschema nao esta instalado (pip install jsonschema); rode com python sem -I"]
    Saida = PastaSaidas / "jsonl_ola.exe"
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "--debug-code", "json", str(ExemploOla), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    if not Jsonl.exists():
        return Problemas + ["nao gravou o .debug.jsonl"]
    Registros = LerJsonl(Jsonl)
    Esquema = json.loads((PastaAqui.parent / "nativo" / "esquema_debug.json").read_text(encoding="utf-8"))
    Validador = jsonschema.Draft202012Validator(Esquema)
    for N, Registro in enumerate(Registros, 1):
        Problemas += [f"linha {N}: {E.message[:100]}" for E in Validador.iter_errors(Registro)]
    Tipos = {R["tipo"] for R in Registros}
    Problemas += [f"sem registros do tipo {T}" for T in ("cabecalho", "poda", "funcao", "instr", "bir", "reloc", "secao", "import", "formato", "faixa", "tempos") if T not in Tipos]
    if Registros[0].get("debug_versao") != "0.7.0":
        Problemas.append("1a linha nao e o cabecalho 0.7.0")
    return Problemas + ConferirCobertura(Registros, Saida.read_bytes())


def CasoDebugJsonlLinux(Forja, PastaSaidas):
    """M5 P27b: --debug-code json no alvo x86_64-linux: cada linha valida no esquema 0.7.0, formato ELF64 sem linker_*, 100% do .text coberto, segmentos R/RX/RW + GNU_STACK e o texto do item formato."""
    try:
        import jsonschema
    except ImportError:
        return ["jsonschema nao esta instalado (pip install jsonschema); rode com python sem -I"]
    Saida = PastaSaidas / "jsonl_ola_linux.elf"
    Rodada = Rodar(Forja, ["build", "--target", "x86_64-linux", "--debug-code", "json", str(ExemploOla), "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    if not Jsonl.exists():
        return Problemas + ["nao gravou o .debug.jsonl"]
    Registros = LerJsonl(Jsonl)
    Validador = jsonschema.Draft202012Validator(json.loads((PastaAqui.parent / "nativo" / "esquema_debug.json").read_text(encoding="utf-8")))
    for N, Registro in enumerate(Registros, 1):
        Problemas += [f"linha {N}: {E.message[:100]}" for E in Validador.iter_errors(Registro)]
    Formato = [R for R in Registros if R["tipo"] == "formato"]
    Segmentos = [(R["tipo_ph"], R["flags"]) for R in Registros if R["tipo"] == "segmento"]
    if len(Formato) != 1 or Formato[0].get("formato") != "ELF64" or Formato[0].get("tamanho_arquivo") != Saida.stat().st_size:
        Problemas.append(f"registro formato do ELF errado: {Formato}")
    if Segmentos != [("LOAD", "R"), ("LOAD", "RX"), ("LOAD", "RW"), ("GNU_STACK", "RW")]:
        Problemas.append(f"segmentos: {Segmentos}")
    if Registros[0].get("alvo") != "x86_64-linux":
        Problemas.append(f"cabecalho com alvo {Registros[0].get('alvo')!r}")
    Texto = [R for R in Registros if R["tipo"] == "secao" and R["nome"] == ".text"]
    Executavel = [R for R in Registros if R["tipo"] == "segmento" and R["flags"] == "RX"]
    if not Texto or not Executavel:
        return Problemas + ["sem a secao .text ou o segmento RX"]
    return Problemas + ConferirCobertura(Registros, Saida.read_bytes(), (Texto[0]["rva"], Texto[0]["tamanho_bruto"], Executavel[0]["offset"]))


def CasoDebugJsonlDeterministico(Forja, PastaSaidas):
    """Duas builds iguais: exe byte a byte igual e jsonl igual (menos o registro tempos)."""
    Gerados = []
    Problemas = []
    for N in (1, 2):
        Saida = PastaSaidas / "det.exe"  # mesmo caminho nas 2 builds: o RSDS do exe em dev leva o caminho absoluto do .pdb (M6b P75)
        Rodada = Rodar(Forja, ["build", "--backend", "x64", "--debug-code", "json", str(ExemploOla), "-o", str(Saida)])
        Problemas += ConferirExecucao(Rodada, 0)
        Jsonl = Path(str(Saida) + ".debug.jsonl")
        if not Jsonl.exists():
            return Problemas + ["nao gravou o .debug.jsonl"]
        Gerados.append((Saida.read_bytes(), [R for R in LerJsonl(Jsonl) if R["tipo"] != "tempos"]))
    if Gerados[0][0] != Gerados[1][0]:
        Problemas.append("os dois exes diferem")
    if Gerados[0][1] != Gerados[1][1]:
        Problemas.append("os dois jsonl diferem alem do registro tempos")
    return Problemas


ImportsDeHeap = {"GetProcessHeap", "HeapAlloc", "HeapReAlloc", "HeapFree", "VirtualAlloc", "VirtualFree"}
ImportsDeTempo = {"QueryPerformanceCounter", "QueryPerformanceFrequency"}
TamanhoMaximoOla = 24 * 1024  # M6 P69: era 20 KB; o runtime dev leva o rastreio (pilha .fj padrao): o 01_ola dev foi de 17962 B para 23082 B
ListaAppend = PastaAqui.parent / "nativo" / "m2" / "memoria" / "lista_append.fj"
# 0.25.1: o exe dev embute o caminho do fonte COMO VEIO na linha de comando (4x: R-code, rastreio FRT1 e DWARF); o PE e alinhado em 512 B e a partir de 60
# caracteres o .rdata ganha 1 bloco (24106 -> 24618 B). O caso passa o fonte relativo ao cwd (27 caracteres, fixo pelo layout do repo), entao o tamanho mede o
# codigo e nao o caminho do checkout (o 72 do repo atual estourava o teto; o antigo C:\ENGINE_GAME3D tinha 35).
ExemploOlaRelativo = os.path.relpath(ExemploOla, PastaAqui)


def ImportsDoExe(Forja, Fonte, Saida, Release):
    """Nomes do registro 'import' do --debug-code json de um build x64 (mais os problemas do build). O cwd e PastaAqui: um Fonte relativo vale a partir dela."""
    Argumentos = ["build", "--backend", "x64", "--debug-code", "json", str(Fonte), "-o", str(Saida)] + (["--release"] if Release else [])
    Problemas = ConferirExecucao(Rodar(Forja, Argumentos, Pasta=PastaAqui), 0)
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    if not Jsonl.exists():
        return set(), Problemas + ["nao gravou o .debug.jsonl"]
    return {R["nome"] for R in LerJsonl(Jsonl) if R["tipo"] == "import"}, Problemas


def CasoOlaImportaSoOQueUsa(Forja, PastaSaidas):
    """P17c (G4): o ligador monta o .idata so dos __imp_ alcancaveis. O 01_ola (dev e release) nao importa heap nem relogio, o nome nem aparece nos bytes
    do exe e ele cabe em 24 KB (TamanhoMaximoOla); o lista_append, que usa heap, continua importando HeapAlloc."""
    Problemas = []
    for Release in (False, True):
        Rotulo = "release" if Release else "dev"
        Saida = PastaSaidas / f"imports_ola_{Rotulo}.exe"
        Nomes, Falhas = ImportsDoExe(Forja, ExemploOlaRelativo, Saida, Release)
        Problemas += Falhas
        if not Saida.exists():
            continue
        Dados = Saida.read_bytes()
        Problemas += [f"ola {Rotulo}: importa {N}, que o programa nao usa" for N in sorted(Nomes & (ImportsDeHeap | ImportsDeTempo))]
        Problemas += [f"ola {Rotulo}: o nome {N} esta nos bytes do exe" for N in sorted((ImportsDeHeap | ImportsDeTempo)) if N.encode() + b"\0" in Dados]
        if "ExitProcess" not in Nomes:
            Problemas.append(f"ola {Rotulo}: sem ExitProcess na tabela de imports ({sorted(Nomes)!r})")
        if len(Dados) > TamanhoMaximoOla:
            Problemas.append(f"ola {Rotulo}: exe com {len(Dados)} B, acima de {TamanhoMaximoOla} B")
    Nomes, Falhas = ImportsDoExe(Forja, ListaAppend, PastaSaidas / "imports_lista_append.exe", False)
    if "HeapAlloc" not in Nomes:
        Problemas.append(f"lista_append nao importa HeapAlloc ({sorted(Nomes)!r})")
    return Problemas + Falhas


def CasoTracoEscalarIgual(Forja, PastaSaidas):
    """build --traco do traco_escalar.fj com --backend x64: o exe sai 0 e o stderr (as linhas "@traco ...") e o valor fixo do C capturado na F3 (esperados\\nativo_traco_escalar_err.txt), byte a byte; sem --traco o stderr fica vazio."""
    Saida = PastaSaidas / "traco_x64.exe"
    Rodada = Rodar(Forja, ["build", "--traco", "../nativo/traco/traco_escalar.fj", "--backend", "x64", "-o", str(Saida)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Saida.exists():
        return Problemas + ["build --traco --backend x64 nao gravou o exe"]
    Execucao = subprocess.run([str(Saida)], capture_output=True, timeout=TimeoutSegundos, cwd=PastaAqui)
    if Execucao.returncode != 0:
        Problemas.append(f"x64: exit {Execucao.returncode}")
    Esperado = (PastaAqui / "esperados" / "nativo_traco_escalar_err.txt").read_bytes()
    if Execucao.stderr != Esperado:
        Problemas.append(f"stderr do x64 difere do valor fixo: {Execucao.stderr[:80]!r} contra {Esperado[:80]!r}")
    if not Execucao.stderr.startswith(b"@traco fj_main 0 29:5 0x0000000000000007\r\n"):
        Problemas.append(f"stderr sem a 1a linha do traco: {Execucao.stderr[:80]!r}")
    Sem = PastaSaidas / "traco_sem.exe"
    Problemas += ConferirExecucao(Rodar(Forja, ["build", "../nativo/traco/traco_escalar.fj", "-o", str(Sem)]), 0)
    if Sem.exists() and subprocess.run([str(Sem)], capture_output=True, timeout=TimeoutSegundos, cwd=PastaAqui).stderr != b"":
        Problemas.append("sem --traco o stderr nao e vazio")
    return Problemas


def CasoSuporteJson(Forja, PastaSaidas):
    """--suporte --json: sem arquivo imprime a tabela REGRAS_IR; com .fj lista cada X0001 com ir; os dois saem 0 e nao gravam nada."""
    Tabela = Rodar(Forja, ["build", "--backend", "x64", "--suporte", "--json"])
    Problemas = ConferirExecucao(Tabela, 0)
    Dados = LinhaJson(Tabela.stdout) or {}
    Regras = Dados.get("regras", [])
    if not Regras or "suportado" not in {R.get("estado") for R in Regras}:
        Problemas.append("tabela sem regras suportadas")
    if any(R["estado"] == "recusado" and R.get("codigo") != "X0001" for R in Regras):
        Problemas.append("regra recusada sem o codigo X0001")
    for Nome in ("callback.fj", "proc_ind.fj"):
        Programa = Rodar(Forja, ["build", "--backend", "x64", "--suporte", "--json", f"../nativo/x/{Nome}"])
        Problemas += ConferirExecucao(Programa, 0)
        Achado = LinhaJson(Programa.stdout) or {}
        if Achado.get("total") != 0 or Achado.get("arquivo") != f"../nativo/x/{Nome}":
            Problemas.append(f"{Nome} deixou de rodar no x64 (M3 P21/P21v-c): esperava total 0, veio {Achado!r}")
    return Problemas
