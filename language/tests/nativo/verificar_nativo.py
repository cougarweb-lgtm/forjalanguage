# -*- coding: utf-8 -*-
"""Compara o backend C com o x64 da Forja em bytes crus (P8, M1).

Para cada .fj de dourado, lowering, exemplos, semantica ok_* e nativo\\m1, e para
{dev, release}: compila com o backend C e com o x64 (--backend x64), executa os dois
exes e compara stdout, stderr e codigo de saida byte a byte, sem tirar CRLF.

Regras:
  - Arquivo sem 'proc main(' e modulo importado, nao raiz: nao e compilado (pulado).
  - C recusa a compilacao: pulado (C <codigo> <causa>). O diferencial so existe com o C compilando.
  - x64 recusa com X0001 (fora do escopo da etapa): pulado (X0001 <causa>), EXCETO em nativo\\m1.
  - x64 sem o backend (X0003, ou 'opcao desconhecida' com exit 2): pulado (X0003 <causa>), EXCETO em nativo\\m1.
  - nativo\\m1 e nativo\\m2\\** sao o escopo declarado (PastasEscopo) e rodam inteiros: X0001/X0003 ali sao FALHA (tipo escopo-pulado).
  - Fixture com "// modos: dev" na 1a linha: so o modo dev roda (o release e indefinido nela); o release nao conta no piso.
  - Modo "teste" (P12b): todo .fj com um bloco 'test "..."' ganha uma rodada extra de 'forja test [--backend x64] <arq>' (stdout, stderr e exit
    byte a byte). Fixture com "// modos: teste" na 1a linha roda SO nesse modo. C recusa = pulado; x64 recusa com X0001/X0003 = pulado
    (falha em nativo\\m1 e m2).
  - Piso de 'iguais' por pasta (Piso*): um forja.exe sem x64 (tudo pulado) tem de FALHAR. nativo\\m1 exige todas as fixtures nos 2 modos.
    Os pisos so valem na rodada completa (sem --so).
  - Cada pulado imprime arquivo, modo e o motivo real; o resumo por pasta agrupa por motivo.
  - Qualquer outro erro do x64 e diferente.
  - Os dois lados recebem o MESMO caminho de arquivo (FORJA_ARQ entra nas mensagens).
  - Divergencia: X0801 para stdout/stderr e X0803 para codigo de saida, em JSON com o primeiro byte diferente.
  - Nivel 2 (P18a): na divergencia (x64 contra C) reconstroi os dois lados com --traco, anda os dois stderr em paralelo (so as linhas
    "@traco <funcao> <instr> <linha>:<coluna> 0x<16 hex>") e a 1a linha diferente vira um X0802 (tipo diferencial; funcao, IrInstr, linha e coluna do .fj).
    As linhas @traco somem da comparacao do nivel 1 (que roda sem --traco).
  - nativo\\traco: as fixtures rodam COM --traco nos dois lados e o stderr (as linhas @traco inclusive) tem de ser identico: prova a numeracao.
  - --congelar (0.21.0 F1 P2, D0720): roda o diferencial de sempre (use --forja gaveta\\g1\\forja_g1.exe, o unico com backend C) e, onde C == x64, grava o lado C
    (exit, stdout, stderr com as raizes trocadas por token) em rapido\\oraculo_congelado_g1.json (BancoG1, classe "nativo", chave com o modo); C que recusa
    a compilacao grava um "pulado"; divergencia NAO grava (e falha como sempre). Em lowering so entram as fixtures com .codigo (as demais tem .esperado).
    O piso de iguais por pasta medido na rodada completa vai para o cabecalho do banco.
  - Padrao desde 0.21.0 F3 (sem flag): nao compila nem roda o backend C (nada de gcc): compila so o x64 e compara com o lado C congelado; chave ausente = falha
    (tipo congelado-ausente); o piso vem do cabecalho do banco. Sem o nivel 2 (X0802 precisa do C). --contra-congelado segue aceita, como no-op.

Uso:
    python -I verificar_nativo.py [--forja <exe>] [--so <trecho>] [--grupo lowering|corpus|escopo] [--congelar] [--contra-congelado (no-op)]

Versao: 1.20.4
Changelog: 1.20.4 - PREPARO 0.24.1: PreparoEstatica de compressao_basico leva compression/zlib_ponte.fj (cola zlib_ponte do libz.a; compression.fj importa vendor:compression/zlib_ponte).
           1.20.3 - L4b: ok_l4b_load_bytes_crus (#load binario com 0x5C) entra em POSTERIORES_AO_SELO.
           1.20.2 - G3 L2a (bloco D): FileError.locked tornou diferente o switch exaustivo de ok_l3c_file_novo (sem o case novo: E0607); a fixture, editada depois do selo, entra em POSTERIORES_AO_SELO (a chave do banco e o sha da fonte) e em EDITADAS_APOS_O_SELO, cujos modos o piso do banco desconta; o .esperado.txt dela segue valendo na suite semantica.
           1.20.1 - G3 L1a: ok_g3_pacote_basico e ok_g3_subpasta_ignorada (modulo de pasta) entram em POSTERIORES_AO_SELO.
           1.20.0 - M8 F0 [D-M8-7]: sai a opcao --c-contra-c (flag, help e checagens); CompararTeste e o ramo do modo teste ficam (sao o comparador do teste no padrao congelado).
           1.19.0 - 0.21.0 F3: o padrao e o congelado (--contra-congelado vira no-op); --c-contra-c e o unico modo com o backend C vivo.
           1.18.0 - 0.21.0 F1 P2: --congelar e --contra-congelado (classe nativo do oraculo_congelado_g1.json; ver Regras).
           1.17.1 - M6 P69: Comparar tira coluna/valor/pilha da linha R dos dois lados (rcode_normalizar): o x64 dev traz a pilha por padrao.
           1.17.0- bateria fatiada: --grupo lowering|corpus|escopo roda so as pastas do grupo (pisos por pasta intactos); a uniao dos 3 grupos e a rodada inteira.
           1.16.2 - IA1-L3c2: os modulos de vendor\\ com os nomes novos (db\\db.fj, compression\\compression.fj, db_server\\mariadb.fj); os casos seguem importando pelos caminhos antigos.
           1.16.1 - M4 P25b: build e test levam sempre --backend c|x64 explicito (o padrao do dev passou a ser x64).
           1.16.0 - M3b P23g: fx_leg (libfx_leg.a) entra em m3\\estatica: printf, fprintf, snprintf, sscanf, strcasecmp, hypotf, sincosf, __popcountdi2, stat64i32 e opendir/readdir/closedir do rt_win_legado.o, x64 igual ao C.
           1.15.1 -M3b P23f: rede_versao (libcurl.a, so curl_version) entra em m3\\estatica: o vsnprintf que o libcurl pede vem do rt_win_legado.o sobre a UCRT; m3\\pendente_p23f some; fx_snp (libfx_snp.a) exercita o vsnprintf do legado.
           1.15.0 - M3b P23f: json_basico volta de m3\\pendente_p23f para m3\\estatica (sprintf e sscanf vem do rt_win_legado.o sobre a UCRT); novas em m3\\estatica: banco_basico (libsqlite3.a), fx_ctor (libfx_ctor.a: construtores
                    .ctors e .CRT$XCU) e abi_j (libfx_cb.a: desenrolar de pilha atraves dos quadros da .a); PreparoEstatica leva as libs e os modulos.
           1.14.0 - M3b P23e: pasta nativo\\m3\\estatica (PastaM3Estatica, em PastasEscopo; --pasta m3/estatica): compressao_basico e json_basico (libz.a e libcjson.a do vendor, com os modulos .fj) e fx_coff (libfx_coff.a
                    de nativo\\m3\\estatica\\libs) ligados ESTATICAMENTE; PrepararEstatica leva as .a e os modulos para a pasta de trabalho. Piso: todas as fixtures nos 2 modos. O json_basico espera o P23f
                    (sprintf e sscanf do cJSON vem da libmingwex, nao da ucrtbase): fica em m3\\pendente_p23f (fora de PastaM3Estatica) e volta para m3\\estatica quando o P23f ligar a libmingwex (piso 4 -> 6).
           1.13.0 - M3 P21v-c: nativo\\m3\\proc_valor entra em PastasEscopo (BIR_CHAMA_IND no x64: X0001/X0003 ali sao FALHA; piso: todas as fixtures nos 2 modos).
           1.12.0 - M3 P21v-b: pasta nativo\\m3\\proc_valor (PastaM3ProcValor; --pasta m3/proc_valor): proc como valor e chamada indireta (vtbl_c, getprocaddress, wndclass, d3d12_warp), com as DLLs de nativo\\m3\\libs (mesmo preparo de m3/callback).
                    FORA de PastasEscopo ate o P21v-c: o x64 recusa com X0001 (IR_CHAMADA_IND) e o caso fica "pulado"; o P21v-c a poe em PastasEscopo (piso: todas as fixtures nos 2 modos).
           1.11.0 -M3 P21: pasta nativo\\m3\\callback (PastaM3Callback, em PastasEscopo; --pasta m3/callback): proc da Forja passado a foreign (trampolim fjt_), com as DLLs de nativo\\m3\\libs (mesmo preparo de m3/foreign). Piso: todas as fixtures nos 2 modos (12).
           1.10.0 - M3 P20: pasta nativo\\m3\\abi (PastaM3Abi, em PastasEscopo; --pasta m3/abi): alinhamento da pilha, registradores sujos e sondagem de pilha, com as DLLs de nativo\\m3\\libs (mesmo preparo de m3/foreign). Piso: todas as fixtures nos 2 modos (6).
           1.9.0 - M3 P19b: pasta nativo\\m3\\implib (PastaM3Implib, em PastasEscopo; --pasta m3/implib): cada fixture leva para a pasta de trabalho so a import library (python313.lib do Python instalado, libs\\fx_basica.dll.a,
                   vendor\\libmariadb.dll.a com o modulo mariadb.fj) e a DLL dela vai so no PATH do run (Executar(..., PastasExtra)); PrepararFonte devolve (fonte, pastas do PATH). Piso: todas as fixtures nos 2 modos (6).
           1.8.0 - M3 P19: pasta nativo\\m3\\foreign (PastaM3Foreign, em PastasEscopo; --pasta m3/foreign): cada fixture roda numa pasta de trabalho com a copia dela e as DLLs de nativo\\m3\\libs
                   (sha256 conferido contra artefatos.sha256 antes), que e o -L do gcc, a busca do x64 e a pasta do exe. Pisos remedidos com foreign por DLL no x64
                   (lowering 326 -> 383, exemplos 11 -> 13, semantica ok_* 61 -> 122; dourado 34).
           1.7.1 -Linguagem v1.11 L11: pasta nativo\\v111 (PastaV111, em PastasEscopo; --pasta v111 / v111/depurar).
           1.7.0 - P18a: nivel 2 do diferencial (X0802 pelo traco, forja build --traco nos dois backends) e a pasta nativo\\traco (3 fixtures em dev, com --traco nos dois lados).
           1.6.1 - M2 P15b: pisos remedidos com f"...", format e format_c no x64 (lowering 318 -> 326, semantica ok_* 51 -> 61; dourado 34 e exemplos 11 iguais); nativo m2 194 -> 208.
           1.6.0 - arquivos em paralelo (ThreadPoolExecutor; --jobs N, padrao min(8, nucleos); --jobs 1 = sequencial). Mesma saida na mesma ordem; medido em nativo/m2/opcional: 15 s -> 3 s.
           1.5.3 - M2 P15a: pisos remedidos com ?T, union, tupla, ?cstr e cstr no x64 (lowering 304 -> 318, semantica ok_* 43 -> 51; dourado 34 e exemplos 11 iguais); nativo m2 180 -> 194.
           1.5.2 - M2 P14b: pisos remedidos com alocacao e lista no x64 (lowering 282 -> 304, semantica ok_* 39 -> 43; dourado 34 e exemplos 11 iguais); nativo m2 157 -> 180.
           1.5.1 - M2 P13b: pisos remedidos com arrays e fatias no x64 (lowering 252 -> 282, semantica ok_* 29 -> 39; dourado 34 e exemplos 11 iguais).
           1.5.0 - M2 P12b: modo "teste" (forja test, C contra x64) para todo .fj com 'test "..."'; cabecalho "// modos: teste"; pisos remedidos com o modo teste (exemplos 10 -> 11 com o 04, semantica ok_* 28 -> 29 com o ok_04_fluxo).
           1.4.3 - M2 P12a: pisos remedidos com resultado, enum, switch, assert e panic no x64 (dourado 34, lowering 184 -> 252, exemplos 8 -> 10 com o 02 e o 03, semantica ok_* 10 -> 28).
           1.4.2 - M2 P11a: pisos remedidos com struct, ponteiro e faixa no x64 (dourado 22 -> 34, lowering 132 -> 184, exemplos 4 -> 8, semantica ok_* 8 -> 10).
           1.4.1 - M2 P10b: pisos remedidos com real no x64 (dourado 16 -> 22, lowering 116 -> 132, semantica ok_* 6 -> 8; exemplos 4).
           1.4.0 - M2 P9: --pasta aceita atalho (m2) e subpasta (m2/conta); sem pasta que bata o resultado e erro (antes saia calado com 0); pisos medidos na rodada do P9.
           1.3.0 - M2 P9a: PastasEscopo (nativo\\m1 e nativo\\m2\\**, com X0001/X0003 = falha); cabecalho "// modos: dev" respeitado; --pasta <nome>.
           1.2.0 - portao M1 (P1-2): X0001/X0003 em nativo\\m1 contam como falha; piso de iguais por pasta (PisoIguais*); motivo real dos pulados
                   (codigo + causa do JSON do erro) no lugar do rotulo fixo.
           1.1.0 - P7b do M1: primeira rodada de verdade (o --backend x64 existe). Pulado so por X0001 (fora do M1) ou por C que nao compila; qualquer
                   X0003 no build exe do x64 (que nao pode ocorrer, ele so vale para test, dll e --modo c) tambem fica como pulado, mas com o motivo
                   impresso; a linha de cada pasta mostra os motivos de pulado e o resumo final soma tudo.
           1.0.0 - primeira versao (P8-T do M1).
Verificacao: python -I verificar_nativo.py
"""

import argparse
import base64
import fnmatch
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from itertools import zip_longest
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[1]
sys.path.insert(0, str(PastaAqui.parent))  # tests\: rcode_normalizar (P69)
from rcode_normalizar import NormalizarLinhasR  # noqa: E402
sys.path.insert(0, str(PastaAqui / "rapido"))  # oraculo_congelado (0.21.0 F1)
import oraculo_congelado as Oraculo  # noqa: E402
PastaExemplos = PastaLanguage.parent / "examples"
ArquivoForjaOrigem = PastaLanguage / "compiler" / "forja.exe"
TentativasCopia = 5
EsperaCopiaSegundos = 30
TimeoutBuildSegundos = 300
TimeoutExecucaoSegundos = 30
Modos = ("dev", "release")
RegexProcMain = re.compile(rb"^\s*proc\s+main\s*\(", re.MULTILINE)
RegexCodigoJson = re.compile(rb'"codigo":\s*"([A-Z]\d{4})"')
RegexCodigoSolto = re.compile(rb"\b(X\d{4})\b")
RegexCausaJson = re.compile(rb'"causa":\s*"((?:[^"\\]|\\.)*)"')
TamanhoContexto = 16
TamanhoMotivoMax = 110
PastaM1 = "nativo/m1"
PastaM2 = "nativo/m2"
PastaTraco = "nativo/traco"
PastaV111 = "nativo/v111"
PastaM3Foreign = "nativo/m3/foreign"
PastaM3Implib = "nativo/m3/implib"
PastaM3Abi = "nativo/m3/abi"
PastaM3Callback = "nativo/m3/callback"
PastaM3ProcValor = "nativo/m3/proc_valor"
PastaM3Estatica = "nativo/m3/estatica"
PastasEscopo = (PastaM1, PastaM2, PastaTraco, PastaV111, PastaM3Foreign, PastaM3Implib, PastaM3Abi, PastaM3Callback, PastaM3ProcValor, PastaM3Estatica)
# M3 P19: a fixture de m3/foreign e as DLLs de m3/libs vao para a pasta de trabalho (o -L do gcc e a busca do x64 olham a pasta do .fj, e o loader acha a DLL ao lado do exe).
PastaLibsM3 = PastaAqui / "m3" / "libs"
PastaVendor = PastaLanguage / "vendor"
ArquivoHashesLibs = "artefatos.sha256"
PrefixoTraco = b"@traco "
RegexLinhaTraco = re.compile(rb"^@traco (\S+) (\d+) (\d+):(\d+) (0x[0-9a-f]{16})$")
AlvoDoDiferencial = "x86_64-windows"
CodigosForaDoEscopo = ("X0001", "X0003")
CabecalhoModoDev = b"// modos: dev"
CabecalhoModoTeste = b"// modos: teste"
ModoTeste = "teste"
RegexBlocoTeste = re.compile(rb'^\s*test\s+"', re.MULTILINE)
RegexResumoTeste = re.compile(rb"^\d+ testes, \d+ falharam", re.MULTILINE)
# Piso de 'iguais' (arquivo x modo) medido na rodada do M2 P15b (forja 0.17.0); nativo/m1 e m2 nao tem piso fixo: exigem todas as fixtures nos 2 modos.
PisoIguaisDourado = 34
PisoIguaisLowering = 383
PisoIguaisExemplos = 13
PisoIguaisSemantica = 122
POSTERIORES_AO_SELO = {
    "ok_r0_opaco_aliasado.fj",     # 0.21.2 R0 (E0202 por ponteiro opaco aliasado): sem lado C congelado
    "ok_g3_pacote_basico.fj",      # G3 L1a (Linguagem v1.13, modulo de pasta): fixture posterior ao selo do oraculo congelado
    "ok_g3_subpasta_ignorada.fj",  # G3 L1a: idem
    "ok_l3c_file_novo.fj",         # G3 L2a: editada depois do selo (case FileError.locked); ver EDITADAS_APOS_O_SELO
    "ok_l4b_load_bytes_crus.fj",   # L4b (#load binario, 0x5C): fixture posterior ao selo do oraculo congelado
    "ok_l4a_const_proc.fj",        # L4a (const com campo proc, 0.27.0): fixture posterior ao selo do oraculo congelado
    "ok_curto_and_or.fj",          # fixture de 20/11/2026 posterior ao selo (sem lado C congelado, X0801 congelado-ausente)
    "ok_origem_campos.fj",         # idem
    "ok_wasm_rt_pacote_usuario.fj",  # idem (runtime wasm como pacote de usuario)
}
EDITADAS_APOS_O_SELO = {"ok_l3c_file_novo.fj"}  # estavam no piso selado do banco: o piso desconta os modos delas
PisosIguais = {
    "dourado": PisoIguaisDourado,
    "lowering": PisoIguaisLowering,
    "exemplos": PisoIguaisExemplos,
    "semantica ok_*": PisoIguaisSemantica,
}


def ListarPastas():
    Testes = PastaLanguage / "tests"
    return [
        ("dourado", Testes / "dourado", "*.fj"),
        ("lowering", Testes / "lowering", "*.fj"),
        ("exemplos", PastaExemplos, "*.fj"),
        ("semantica ok_*", Testes / "semantica", "ok_*.fj"),
        ("nativo/m1", Testes / "nativo" / "m1", "*.fj"),
        ("nativo/m2", Testes / "nativo" / "m2", "*.fj"),
        (PastaTraco, Testes / "nativo" / "traco", "*.fj"),
        (PastaV111, Testes / "nativo" / "v111", "*.fj"),
        (PastaM3Foreign, Testes / "nativo" / "m3" / "foreign", "*.fj"),
        (PastaM3Implib, Testes / "nativo" / "m3" / "implib", "*.fj"),
        (PastaM3Abi, Testes / "nativo" / "m3" / "abi", "*.fj"),
        (PastaM3Callback, Testes / "nativo" / "m3" / "callback", "*.fj"),
        (PastaM3ProcValor, Testes / "nativo" / "m3" / "proc_valor", "*.fj"),
        (PastaM3Estatica, Testes / "nativo" / "m3" / "estatica", "*.fj"),
    ]


GrupoLowering = "lowering"
GrupoCorpus = "corpus"
GrupoEscopo = "escopo"
GrupoDasPastas = {"lowering": GrupoLowering, "dourado": GrupoCorpus, "exemplos": GrupoCorpus, "semantica ok_*": GrupoCorpus}


def GrupoDaPasta(Nome):
    """Fatia da rodada em que a pasta cai (--grupo): lowering, corpus (dourado, exemplos, semantica ok_*) ou escopo (todas as demais); cada pasta cai em exatamente uma."""
    return GrupoDasPastas.get(Nome, GrupoEscopo)


def ConferirHashesDasLibs():
    """Texto do problema se alguma DLL de m3/libs nao bate com o artefatos.sha256 (ou nao existe); None se tudo confere."""
    Lista = PastaLibsM3 / ArquivoHashesLibs
    if not Lista.is_file():
        return f"{Lista} nao existe (rode m3/libs/construir_libs.py)"
    for Linha in Lista.read_text(encoding="utf-8").splitlines():
        Hash, _, Nome = Linha.partition("  ")
        Dll = PastaLibsM3 / Nome
        if not Dll.is_file() or hashlib.sha256(Dll.read_bytes()).hexdigest() != Hash:
            return f"{Nome} nao bate com o sha256 de {ArquivoHashesLibs}"
    return None


def ConferirLibsOuSair():
    Problema = ConferirHashesDasLibs()
    if Problema:
        raise SystemExit(f"X0801 {Problema}")


def CopiarFixture(Arquivo, Trabalho):
    Destino = Trabalho / Path(Arquivo).name
    shutil.copy2(Arquivo, Destino)
    return Destino


def PrepararForeign(Arquivo, Trabalho):
    """m3/foreign: a fixture e as DLLs de m3/libs vao para Trabalho (mesma pasta do exe). Sem PATH extra."""
    ConferirLibsOuSair()
    for Dll in PastaLibsM3.glob("*.dll"):
        shutil.copy2(Dll, Trabalho / Dll.name)
    return str(CopiarFixture(Arquivo, Trabalho)), []


def PrepararPythonMsvc(Arquivo, Trabalho):
    """python_msvc: a python313.lib do Python instalado (sys.base_prefix\\libs) vai para Trabalho; a python313.dll fica la e entra so no PATH do run."""
    Base = Path(sys.base_prefix)
    Biblioteca = Base / "libs" / "python313.lib"
    if not Biblioteca.is_file():
        raise SystemExit(f"X0801 {Biblioteca} nao existe: a fixture python_msvc precisa do Python 3.13 instalado")
    shutil.copy2(Biblioteca, Trabalho / Biblioteca.name)
    return str(CopiarFixture(Arquivo, Trabalho)), [str(Base)]


def PrepararGnuDlltool(Arquivo, Trabalho):
    """gnu_dlltool: so a fx_basica.dll.a vai para Trabalho; a fx_basica.dll fica em m3/libs e entra so no PATH do run."""
    ConferirLibsOuSair()
    shutil.copy2(PastaLibsM3 / "fx_basica.dll.a", Trabalho / "fx_basica.dll.a")
    return str(CopiarFixture(Arquivo, Trabalho)), [str(PastaLibsM3)]


def PrepararMariadbImplib(Arquivo, Trabalho):
    """mariadb_implib: Trabalho\\vendor leva so o modulo db_server\\mariadb.fj e a libmariadb.dll.a; a libmariadb.dll fica em language\\vendor e entra so no PATH do run."""
    Modulo = Trabalho / "vendor" / "db_server"
    Modulo.mkdir(parents=True)
    shutil.copy2(PastaVendor / "db_server" / "mariadb.fj", Modulo / "mariadb.fj")
    shutil.copy2(PastaVendor / "libmariadb.dll.a", Trabalho / "vendor" / "libmariadb.dll.a")
    return str(CopiarFixture(Arquivo, Trabalho)), [str(PastaVendor)]


PastaLibsEstatica = PastaAqui / "m3" / "estatica" / "libs"
# M3b P23e: o que cada fixture de m3/estatica leva para a pasta de trabalho: as estaticas (vendor\ ou libs\) e os modulos .fj do vendor (import "vendor:...").
PreparoEstatica = {
    "compressao_basico": ([PastaVendor / "libz.a"], ["compression/compression.fj", "compression/zlib.fj", "compression/zlib_ponte.fj"]),
    "json_basico": ([PastaVendor / "libcjson.a"], ["json/json.fj", "json/cjson.fj"]),
    "fx_coff": ([PastaLibsEstatica / "libfx_coff.a"], []),
    "banco_basico": ([PastaVendor / "libsqlite3.a"], ["db/db.fj", "db/sqlite3.fj"]),
    "fx_ctor": ([PastaLibsEstatica / "libfx_ctor.a"], []),
    "abi_j": ([PastaLibsEstatica / "libfx_cb.a"], []),
    "fx_snp": ([PastaLibsEstatica / "libfx_snp.a"], []),
    "fx_leg": ([PastaLibsEstatica / "libfx_leg.a"], []),
    "rede_versao": ([PastaVendor / "libcurl.a"], []),}


def PrepararEstatica(Arquivo, Trabalho):
    """m3/estatica: a .a da fixture vai solta para Trabalho (pasta de busca do 'link') e os modulos do vendor para Trabalho\\vendor\\<pacote>."""
    Bibliotecas, Modulos = PreparoEstatica[Path(Arquivo).stem]
    for Biblioteca in Bibliotecas:
        if not Biblioteca.is_file():
            raise SystemExit(f"X0801 {Biblioteca} nao existe")
        shutil.copy2(Biblioteca, Trabalho / Biblioteca.name)
    for Modulo in Modulos:
        Destino = Trabalho / "vendor" / Modulo
        Destino.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PastaVendor / Modulo, Destino)
    return str(CopiarFixture(Arquivo, Trabalho)), []


# M3 P19b: o preparo de cada fixture de m3/implib (import library na pasta de busca, DLL so no PATH do run).
PreparoImplib = {"python_msvc": PrepararPythonMsvc, "gnu_dlltool": PrepararGnuDlltool, "mariadb_implib": PrepararMariadbImplib}


def PrepararFonte(Arquivo, Trabalho):
    """(caminho que os dois backends compilam, pastas extras do PATH do run). m3/foreign e m3/implib copiam o que precisam para Trabalho; as demais seguem no lugar."""
    Pasta = Path(Arquivo).parent
    if Pasta.parent.name != "m3":
        return Arquivo, []
    if Pasta.name in ("foreign", "abi", "callback", "proc_valor"):
        return PrepararForeign(Arquivo, Trabalho)
    if Pasta.name == "estatica":
        return PrepararEstatica(Arquivo, Trabalho)
    return PreparoImplib[Path(Arquivo).stem](Arquivo, Trabalho) if Pasta.name == "implib" else (Arquivo, [])


def ModosDoArquivo(Arquivo):
    """Os modos que a fixture declara: so dev ('// modos: dev'), so teste ('// modos: teste') ou dev e release (mais teste se tem bloco 'test')."""
    with open(Arquivo, "rb") as Arq:
        Primeira = Arq.readline().strip()
        Resto = Arq.read()
    if Primeira == CabecalhoModoDev:
        return ("dev",)
    if Primeira == CabecalhoModoTeste:
        return (ModoTeste,)
    TemTeste = RegexBlocoTeste.search(Primeira + b"\n" + Resto) is not None
    return Modos + (ModoTeste,) if TemTeste else Modos


def ColetarFontes(Pasta, Padrao):
    Achados = []
    if not Pasta.is_dir():
        return Achados
    for Raiz, Subpastas, Nomes in os.walk(Pasta):
        Subpastas[:] = [S for S in Subpastas if not S.startswith(".")]
        for Nome in sorted(Nomes):
            if not fnmatch.fnmatch(Nome, Padrao):
                continue
            Caminho = Path(Raiz) / Nome
            if not RegexProcMain.search(Caminho.read_bytes()):
                continue
            Achados.append(str(Caminho))
    return sorted(Achados)


def CopiarForja(PastaTmp):
    Destino = Path(PastaTmp) / "forja.exe"
    for Tentativa in range(1, TentativasCopia + 1):
        try:
            shutil.copy2(ArquivoForjaOrigem, Destino)
            return str(Destino)
        except OSError as Erro:
            if Tentativa == TentativasCopia:
                raise SystemExit(f"X0001 nao copiei o forja.exe apos {TentativasCopia} tentativas: {Erro}")
            time.sleep(EsperaCopiaSegundos)
    return str(Destino)


def CodigoDoErro(Texto):
    Achado = RegexCodigoJson.search(Texto) or RegexCodigoSolto.search(Texto)
    return Achado.group(1).decode("ascii") if Achado else None


def MotivoDoErro(Erro, Codigo):
    """Codigo + causa real do erro: o campo 'causa' do JSON ou, sem ele, a 1a linha nao vazia do texto."""
    Causa = RegexCausaJson.search(Erro)
    if Causa:
        Texto = Causa.group(1).decode("utf-8", "replace")
    else:
        Linhas = [L.strip() for L in Erro.decode("utf-8", "replace").splitlines() if L.strip()]
        Texto = Linhas[0] if Linhas else "sem mensagem"
    return f"{Codigo or 'erro'}: {Texto[:TamanhoMotivoMax]}"


CONGELAR = "congelar"
CONTRA = "contra"
ClasseCongelado = "nativo"
ModoCongelado = ""  # "", CONGELAR ou CONTRA: Principal decide antes das threads; depois e so leitura
BancoCongelado = None  # BancoG1 quando ModoCongelado != ""
PisosMedidos = {}  # CONGELAR: iguais por pasta da rodada completa, gravado no cabecalho do banco


def TokenizarRaizes(Bytes, Trabalho):
    """Troca a pasta de trabalho e a raiz do projeto (como estao, com barra invertida dobrada do JSON e com barra normal) por token, para o congelado nao depender de onde a rodada roda."""
    for Raiz, Token in ((str(Trabalho), b"<TRAB>"), (str(PastaLanguage), b"<LANG>")):
        for Forma in (Raiz, Raiz.replace("\\", "\\\\"), Raiz.replace("\\", "/")):
            Bytes = Bytes.replace(Forma.encode("utf-8"), Token)
    return Bytes


def ChaveCongelado(Fonte, Modo, Traco):
    return Oraculo.ChaveDe(Path(Fonte), "padrao", ("traco",) if Traco else (), Modo, ClasseCongelado)


def GravarCongelado(Fonte, Modo, Traco, Saida, Trabalho):
    """CONGELAR: guarda o lado C (so chamado quando C == x64). Sem efeito nos outros modos."""
    if ModoCongelado != CONGELAR:
        return
    Registro = {"invalido": False, "origem": BancoCongelado.Referencia.name, "exit": Saida["codigo"], "stdout": base64.b64encode(Saida["stdout"]).decode("ascii"),
                "stderr": [], "stderr_b64": base64.b64encode(TokenizarRaizes(Saida["stderr"], Trabalho)).decode("ascii")}
    with BancoCongelado.Trava:
        BancoCongelado.Itens[ChaveCongelado(Fonte, Modo, Traco)] = Registro


def GravarPulado(Fonte, Modo, Traco, Motivo):
    """CONGELAR: o C recusou a fonte; o contra-congelado repete o 'pulado' sem compilar nada."""
    if ModoCongelado != CONGELAR:
        return
    with BancoCongelado.Trava:
        BancoCongelado.Itens[ChaveCongelado(Fonte, Modo, Traco)] = {"invalido": False, "origem": BancoCongelado.Referencia.name, "pulado": True, "motivo": Motivo, "exit": 0, "stdout": "", "stderr": []}


def LerCongelado(Arquivo, Fonte, Modo, Traco):
    """CONTRA: (SaidaC, None) ou (None, resultado pronto) quando a chave falta (diferente) ou o registro e um pulado."""
    Registro = BancoCongelado.Itens.get(ChaveCongelado(Fonte, Modo, Traco))
    if Registro is None:
        return None, ("diferente", None, {"codigo": "X0801", "tipo": "congelado-ausente", "arquivo": Arquivo, "modo": Modo, "campo": "chave", "byte": 0})
    if Registro.get("pulado"):
        return None, ("pulado", Registro["motivo"], None)
    return {"stdout": base64.b64decode(Registro["stdout"]), "stderr": base64.b64decode(Registro["stderr_b64"]), "codigo": Registro["exit"]}, None


def Compilar(Forja, Arquivo, Release, Saida, Backend64, Cwd, Traco=False):
    Comando = [Forja, "build", "--modo", "exe"]
    if Release:
        Comando.append("--release")
    if Traco:
        Comando.append("--traco")
    Comando += [Arquivo, "-o", str(Saida)]
    Comando += ["--backend", "x64" if Backend64 else "c"]
    Resultado = subprocess.run(Comando, capture_output=True, timeout=TimeoutBuildSegundos, cwd=Cwd)
    # O erro de front-end (E0xxx) sai no stdout; o do backend (X0xxx) no stderr: junta os dois.
    return Resultado.returncode, Resultado.stderr + Resultado.stdout


def AmbienteComPath(PastasExtra):
    """O ambiente do run com as pastas extras na frente do PATH (a DLL de uma fixture de m3/implib so existe la); None = herda o do processo."""
    if not PastasExtra:
        return None
    Ambiente = os.environ.copy()
    Ambiente["PATH"] = os.pathsep.join([*PastasExtra, Ambiente.get("PATH", "")])
    return Ambiente


def Executar(Exe, Cwd, PastasExtra=()):
    try:
        Resultado = subprocess.run([str(Exe)], capture_output=True, stdin=subprocess.DEVNULL,
                                   timeout=TimeoutExecucaoSegundos, cwd=Cwd, env=AmbienteComPath(PastasExtra))
    except subprocess.TimeoutExpired:
        return {"stdout": b"", "stderr": b"", "codigo": "timeout"}
    return {"stdout": Resultado.stdout, "stderr": Resultado.stderr, "codigo": Resultado.returncode}


def PrimeiroDiferente(Bytes1, Bytes2):
    Tamanho = min(len(Bytes1), len(Bytes2))
    for Indice in range(Tamanho):
        if Bytes1[Indice] != Bytes2[Indice]:
            return Indice
    if len(Bytes1) != len(Bytes2):
        return Tamanho
    return -1


def Trecho(Bytes, Indice):
    Inicio = max(0, Indice - TamanhoContexto)
    return Bytes[Inicio:Indice + TamanhoContexto].hex()


def SemTraco(Bytes):
    """O stderr sem as linhas '@traco ...' do nivel 2: o nivel 1 compara o programa, nao o traco."""
    return b"".join(L for L in Bytes.splitlines(keepends=True) if not L.startswith(PrefixoTraco))


def Comparar(Arquivo, Modo, SaidaC, SaidaX, Traco=False):
    """Nivel 1. Traco=True (pasta nativo\\traco, build com --traco) compara o stderr cru: as linhas @traco tem de ser identicas."""
    for Campo in ("stdout", "stderr"):
        BytesC, BytesX = SaidaC[Campo], SaidaX[Campo]
        if Campo == "stderr" and not Traco:
            BytesC, BytesX = SemTraco(NormalizarLinhasR(BytesC)), SemTraco(NormalizarLinhasR(BytesX))  # P69: o x64 dev traz coluna/valor/pilha na linha R; o C nao
        Indice = PrimeiroDiferente(BytesC, BytesX)
        if Indice >= 0:
            return {
                "codigo": "X0801", "tipo": "divergencia", "arquivo": Arquivo, "modo": Modo,
                "campo": Campo, "byte": Indice,
                "c_hex": Trecho(BytesC, Indice), "x64_hex": Trecho(BytesX, Indice),
                "c_tamanho": len(BytesC), "x64_tamanho": len(BytesX),
            }
    if SaidaC["codigo"] != SaidaX["codigo"]:
        return {
            "codigo": "X0803", "tipo": "divergencia", "arquivo": Arquivo, "modo": Modo,
            "campo": "exit", "byte": 0,
            "c_hex": str(SaidaC["codigo"]), "x64_hex": str(SaidaX["codigo"]),
            "c_tamanho": 0, "x64_tamanho": 0,
        }
    return None


def RodarTeste(Forja, Arquivo, Backend64, Cwd):
    """forja test [--backend x64] <arq>: compila e roda os 'test'; devolve a saida do processo (diagnostico de compilacao incluso)."""
    Comando = [Forja, "test", "--backend", "x64" if Backend64 else "c", Arquivo]
    try:
        Resultado = subprocess.run(Comando, capture_output=True, stdin=subprocess.DEVNULL, timeout=TimeoutBuildSegundos, cwd=Cwd)
    except subprocess.TimeoutExpired:
        return {"stdout": b"", "stderr": b"", "codigo": "timeout"}
    return {"stdout": Resultado.stdout, "stderr": Resultado.stderr, "codigo": Resultado.returncode}


def CodigoDeRecusa(Saida):
    """Codigo do diagnostico de COMPILACAO (E, X ou B) numa saida de 'forja test'; None se os testes rodaram (tem o resumo) ou o erro e de runtime (R)."""
    if RegexResumoTeste.search(Saida["stdout"]):
        return None
    Codigo = CodigoDoErro(Saida["stdout"] + Saida["stderr"])
    return Codigo if Codigo and not Codigo.startswith("R") else None


def CompararTeste(Forja, Arquivo, PastaTmp, Backend64):
    Trabalho = Path(tempfile.mkdtemp(prefix="vn_", dir=PastaTmp))
    try:
        if ModoCongelado == CONTRA:
            SaidaC, Pronto = LerCongelado(Arquivo, Arquivo, ModoTeste, False)
            if Pronto:
                return Pronto
        else:
            SaidaC = RodarTeste(Forja, Arquivo, False, Trabalho)
            CodigoC = CodigoDeRecusa(SaidaC)
            if CodigoC:
                Motivo = "C " + MotivoDoErro(SaidaC["stdout"] + SaidaC["stderr"], CodigoC)
                GravarPulado(Arquivo, ModoTeste, False, Motivo)
                return "pulado", Motivo, None
        SaidaX = RodarTeste(Forja, Arquivo, Backend64, Trabalho)
        if ModoCongelado == CONTRA:
            SaidaX["stderr"] = TokenizarRaizes(SaidaX["stderr"], Trabalho)
        CodigoX = CodigoDeRecusa(SaidaX) if Backend64 else None
        if CodigoX in ("X0001", "X0003"):
            return "pulado", MotivoDoErro(SaidaX["stdout"] + SaidaX["stderr"], CodigoX), None
        if CodigoX:
            Detalhe = {"codigo": "X0801", "tipo": "divergencia", "arquivo": Arquivo, "modo": ModoTeste, "campo": "build-x64", "byte": 0,
                       "rc": SaidaX["codigo"], "stderr": (SaidaX["stdout"] + SaidaX["stderr"]).decode("utf-8", "replace")[:400]}
            return "diferente", None, Detalhe
        Divergencia = Comparar(Arquivo, ModoTeste, SaidaC, SaidaX)
        if Divergencia:
            return "diferente", None, Divergencia
        GravarCongelado(Arquivo, ModoTeste, False, SaidaC, Trabalho)
        return "igual", None, None
    finally:
        shutil.rmtree(Trabalho, ignore_errors=True)


def LinhasDoTraco(Bytes):
    return [L.rstrip(b"\r\n") for L in Bytes.splitlines() if L.startswith(PrefixoTraco)]


def PrimeiroTracoDiferente(TracoC, TracoX):
    """(linhaC, linhaX) da 1a posicao em que os dois tracos discordam (None = o traco acabou naquele lado); None se sao iguais."""
    for LinhaC, LinhaX in zip_longest(TracoC, TracoX):
        if LinhaC != LinhaX:
            return LinhaC, LinhaX
    return None


def LerLinhaTraco(Linha):
    """(funcao, instr, linha, coluna, bits) de uma linha @traco; None se a linha nao existe (fim do traco) ou nao tem o formato."""
    Achado = RegexLinhaTraco.match(Linha) if Linha is not None else None
    if not Achado:
        return None
    return Achado.group(1).decode("ascii"), int(Achado.group(2)), int(Achado.group(3)), int(Achado.group(4)), Achado.group(5).decode("ascii")


def TextoDaLinha(Linha):
    return "fim do traco" if Linha is None else Linha.decode("ascii", "replace")


def ExplicacaoDoCodigo(Forja, Codigo):
    """explicacao e forma_certa do catalogo (forja explicar): a unica fonte desses dois textos."""
    try:
        Resultado = subprocess.run([Forja, "explicar", Codigo], capture_output=True, timeout=TimeoutExecucaoSegundos)
        Catalogo = json.loads(Resultado.stdout.decode("utf-8", "replace"))
    except (subprocess.TimeoutExpired, ValueError, OSError):
        return "", ""
    return Catalogo.get("explicacao", ""), Catalogo.get("forma_certa", "")


def DetalheX0802(Forja, Arquivo, Modo, Saidas):
    """X0802 (diferencial, nivel 2) da 1a linha @traco em que o backend C e o x64 discordam; None se os tracos sao iguais."""
    SaidaC, SaidaX = Saidas
    Par = PrimeiroTracoDiferente(LinhasDoTraco(SaidaC["stderr"]), LinhasDoTraco(SaidaX["stderr"]))
    if Par is None:
        return None
    Esperado, Obtido = LerLinhaTraco(Par[0]), LerLinhaTraco(Par[1])
    Local = Esperado or Obtido or ("?", -1, 0, 0, "")
    Mesmo = Esperado is not None and Obtido is not None and Esperado[:4] == Obtido[:4]
    BitsC, BitsX = (Esperado[4] if Esperado else "fim do traco"), (Obtido[4] if Obtido else "fim do traco")
    Causa = (f"valor divergente em '{Local[0]}' (IrInstr {Local[1]}): esperado {BitsC}, obtido {BitsX}" if Mesmo
             else f"gravacoes divergentes no traco: esperado '{TextoDaLinha(Par[0])}', obtido '{TextoDaLinha(Par[1])}'")
    Explicacao, Forma = ExplicacaoDoCodigo(Forja, "X0802")
    return {
        "codigo": "X0802", "tipo": "diferencial", "arquivo": Arquivo, "linha": Local[2], "coluna": Local[3], "causa": Causa,
        "explicacao": Explicacao, "forma_certa": Forma, "alvo": AlvoDoDiferencial, "modo": Modo, "funcao": Local[0], "instr": Local[1],
        "esperado": BitsC, "obtido": BitsX, "byte": PrimeiroDiferente(SaidaC["stderr"], SaidaX["stderr"]),
    }


def LocalizarComTraco(Forja, Arquivo, Modo, Trabalho, PastasExtra=()):
    """Reconstroi os dois lados com --traco, roda e devolve o X0802; None se nao deu para reconstruir ou se os tracos coincidem."""
    Release = Modo == "release"
    Exes = []
    for Nome, Backend64 in (("ct.exe", False), ("xt.exe", True)):
        Exe = Trabalho / Nome
        if Compilar(Forja, Arquivo, Release, Exe, Backend64, Trabalho, Traco=True)[0] != 0:
            return None
        Exes.append(Executar(Exe, Trabalho, PastasExtra))
    return DetalheX0802(Forja, Arquivo, Modo, Exes)


def CompararArquivo(Forja, Arquivo, Modo, PastaTmp, Backend64, Traco=False):
    if Modo == ModoTeste:
        return CompararTeste(Forja, Arquivo, PastaTmp, Backend64)
    Release = Modo == "release"
    Trabalho = Path(tempfile.mkdtemp(prefix="vn_", dir=PastaTmp))
    try:
        Fonte, PastasExtra = PrepararFonte(Arquivo, Trabalho)
        ExeC = Trabalho / "c.exe"
        if ModoCongelado == CONTRA:
            SaidaCongelada, Pronto = LerCongelado(Arquivo, Fonte, Modo, Traco)
            if Pronto:
                return Pronto
        else:
            Codigo, Erro = Compilar(Forja, Fonte, Release, ExeC, False, Trabalho, Traco)
            if Codigo != 0:
                Motivo = "C " + MotivoDoErro(Erro, CodigoDoErro(Erro))
                GravarPulado(Fonte, Modo, Traco, Motivo)
                return "pulado", Motivo, None
        ExeX = Trabalho / "x64.exe"
        Codigo, Erro = Compilar(Forja, Fonte, Release, ExeX, Backend64, Trabalho, Traco)
        if Codigo != 0 and Backend64:
            CodigoX = CodigoDoErro(Erro)
            if CodigoX == "X0001":
                return "pulado", MotivoDoErro(Erro, "X0001"), None
            if CodigoX == "X0003" or (Codigo == 2 and b"opcao desconhecida" in Erro):
                return "pulado", MotivoDoErro(Erro, "X0003"), None
            Detalhe = {"codigo": "X0801", "tipo": "divergencia", "arquivo": Arquivo, "modo": Modo,
                       "campo": "build-x64", "byte": 0, "rc": Codigo,
                       "stderr": Erro.decode("utf-8", "replace")[:400]}
            return "diferente", None, Detalhe
        if Codigo != 0:
            return "pulado", "C " + MotivoDoErro(Erro, CodigoDoErro(Erro)), None
        SaidaX = Executar(ExeX, Trabalho, PastasExtra)
        if ModoCongelado == CONTRA:
            SaidaC = SaidaCongelada
            SaidaX["stderr"] = TokenizarRaizes(SaidaX["stderr"], Trabalho)
        else:
            SaidaC = Executar(ExeC, Trabalho, PastasExtra)
        Divergencia = Comparar(Arquivo, Modo, SaidaC, SaidaX, Traco)
        if Divergencia and Backend64 and not Traco and not ModoCongelado:  # X0802 precisa do backend C: fora do contra-congelado
            X0802 = LocalizarComTraco(Forja, Fonte, Modo, Trabalho, PastasExtra)
            return "diferente", None, [Divergencia] + ([X0802] if X0802 else [])
        if Divergencia:
            return "diferente", None, Divergencia
        GravarCongelado(Fonte, Modo, Traco, SaidaC, Trabalho)
        return "igual", None, None
    finally:
        shutil.rmtree(Trabalho, ignore_errors=True)


def ForaDoEscopo(Nome, Estado, Motivo):
    """Nas pastas de PastasEscopo o x64 tem de compilar tudo: X0001/X0003 ali e falha, nao pulado."""
    return Nome in PastasEscopo and Estado == "pulado" and Motivo.startswith(CodigosForaDoEscopo)


def DetalheForaDoEscopo(Arquivo, Modo, Motivo):
    return {"codigo": "X0801", "tipo": "escopo-pulado", "arquivo": Arquivo, "modo": Modo, "campo": "build-x64", "byte": 0, "motivo": Motivo}


def ContarArquivo(Forja, Nome, Arquivo, PastaTmp, Backend64):
    """Roda os modos de um arquivo; devolve (Contagem por estado, Motivos dos pulados, Linhas a imprimir).
    As linhas voltam em lista (e nao saem por print) para a rodada em threads imprimir na ordem dos arquivos."""
    Contagem, Motivos, Linhas = Counter(), Counter(), []
    for Modo in ModosDoArquivo(Arquivo):
        Estado, Motivo, Detalhe = CompararArquivo(Forja, Arquivo, Modo, PastaTmp, Backend64, Nome == PastaTraco)
        if ForaDoEscopo(Nome, Estado, Motivo):
            Estado, Detalhe = "diferente", DetalheForaDoEscopo(Arquivo, Modo, Motivo)
        Contagem[Estado] += 1
        if Estado == "pulado":
            Motivos[Motivo] += 1
            Linhas.append(f"  pulado [{Modo}] {Arquivo}: {Motivo}")
        for Item in (Detalhe if isinstance(Detalhe, list) else [Detalhe] if Detalhe else []):
            Linhas.append(json.dumps(Item, ensure_ascii=True))
    return Contagem, Motivos, Linhas


def PisoDaPasta(Nome, Fontes):
    """Piso de iguais: as pastas de PastasEscopo exigem todas as fixtures em todos os modos que declaram; as demais usam PisosIguais."""
    if Nome in PastasEscopo:
        return sum(len(ModosDoArquivo(Fonte)) for Fonte in Fontes)
    return PisosIguais.get(Nome, 0)


def SeparaPasta(Pedida):
    """--pasta aceita o nome da pasta (nativo/m2), o atalho sem 'nativo/' (m2) e uma subpasta (m2/conta): devolve (nome, subpasta)."""
    if Pedida is None:
        return None, ""
    Normal = Pedida.replace("\\", "/").strip("/")
    if Normal.startswith(("m1", "m2", "m3", "traco", "v111")):
        Normal = "nativo/" + Normal
    Partes = Normal.split("/")
    Tamanho = 3 if Normal.startswith("nativo/m3/") else 2  # nativo/m3/foreign e nativo/m3/implib tem 3 partes no nome
    return "/".join(Partes[:Tamanho]) if Partes[0] == "nativo" else Normal, "/".join(Partes[Tamanho:])


def VerificarPasta(Forja, Pasta3, PastaTmp, Args):
    """Compara uma pasta do corpus e imprime o resumo; devolve o numero de falhas (diferentes + piso nao atingido)."""
    Nome, Pasta, Padrao = Pasta3
    Fontes = ColetarFontes(Pasta, Padrao)
    Sub = SeparaPasta(Args.pasta)[1]
    if Sub:
        Marca = os.sep + Sub.replace("/", os.sep) + os.sep
        Fontes = [F for F in Fontes if Marca in F]
    Contagem, Motivos = Counter(), Counter()
    Escolhidos = [F for F in Fontes if not Args.so or Args.so in F]
    if ModoCongelado and Nome == "lowering":
        Escolhidos = [F for F in Escolhidos if Path(F).with_suffix(".codigo").is_file()]  # as demais tem .esperado fixo (lowering_x64)
    if ModoCongelado:
        # 0.21.2 R0: fixture nascida depois do selo do oraculo congelado (o backend C nao existe mais para gerar o lado C): nao tem chave, nao se regera
        Escolhidos = [F for F in Escolhidos if Path(F).name not in POSTERIORES_AO_SELO]
    # Quase todo o tempo e espera de subprocesso (gcc, forja, exe): threads bastam; map devolve na ordem dos arquivos.
    with ThreadPoolExecutor(max_workers=max(1, Args.jobs)) as Pool:
        Resultados = Pool.map(lambda F: ContarArquivo(Forja, Nome, F, PastaTmp, True), Escolhidos)
        for ContagemArq, MotivosArq, Linhas in Resultados:
            Contagem.update(ContagemArq)
            Motivos.update(MotivosArq)
            for Linha in Linhas:
                print(Linha)
    print(f"{Nome}: iguais={Contagem['igual']} diferentes={Contagem['diferente']} pulados={Contagem['pulado']}")
    for Motivo, Quantos in sorted(Motivos.items()):
        print(f"  pulados por motivo: {Quantos}x {Motivo}")
    Falhas = Contagem["diferente"]
    Piso = PisoDaPasta(Nome, Fontes)
    if ModoCongelado and not Args.so and not Sub:
        if ModoCongelado == CONGELAR:
            PisosMedidos[Nome] = Contagem["igual"]
            Piso = 0
        else:
            Piso = BancoCongelado.Cabecalho.get("pisos", {}).get(Nome, Piso)
            Piso -= sum(len(ModosDoArquivo(F)) for F in Fontes if Path(F).name in EDITADAS_APOS_O_SELO)
    if not Args.so and Contagem["igual"] < Piso:
        print(json.dumps({"codigo": "X0801", "tipo": "piso", "pasta": Nome, "iguais": Contagem["igual"], "piso": Piso}))
        Falhas += 1
    return Falhas


def IniciarCongelado(Modo):
    """Liga o modo congelado antes das threads: abre o BancoG1; CONTRA confere o selo/cabecalho antes de comparar."""
    global ModoCongelado, BancoCongelado
    ModoCongelado, BancoCongelado = Modo, Oraculo.BancoG1()
    if Modo == CONTRA:
        BancoCongelado.ExigirCabecalho()
    elif BancoCongelado.Selado:
        raise Oraculo.ErroOraculo("banco G1 selado: nao se recongela")


def FecharCongelamento(Falhas):
    """CONGELAR: cabecalho (forja de referencia, gcc, pisos medidos) e gravacao do JSON; com falhas grava assim mesmo o que igualou e avisa."""
    Banco = BancoCongelado
    if not Banco.Cabecalho:
        Banco.Cabecalho = Oraculo.CabecalhoAtual(Banco.Referencia)
    Banco.Cabecalho.setdefault("pisos", {}).update(PisosMedidos)
    Banco.Salvar()
    print(f"congelado: {len(Banco.Itens)} itens em {Banco.Caminho.name}" + (f" (ATENCAO: {Falhas} falha(s) nao gravadas)" if Falhas else ""))


def Principal():
    Parser = argparse.ArgumentParser(description="Compara backend C e x64 da Forja em bytes crus.")
    Parser.add_argument("--forja", default=None, help="exe da Forja (padrao: copia em TEMP)")
    Parser.add_argument("--so", default=None, help="roda so os arquivos cujo caminho contem este trecho")
    Parser.add_argument("--pasta", default=None, help="roda so a pasta com este nome (ex.: nativo/m2)")
    Parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 1),
                        help="arquivos em paralelo (padrao: min(8, nucleos); 1 = sequencial, como antes da 1.6.0)")
    Parser.add_argument("--grupo", choices=(GrupoLowering, GrupoCorpus, GrupoEscopo), default=None,
                        help="roda so as pastas deste grupo (os pisos seguem valendo por pasta); a uniao dos 3 grupos e a rodada inteira")
    Parser.add_argument("--congelar", action="store_true", help="grava o lado C (onde C == x64) em rapido\\oraculo_congelado_g1.json; use --forja gaveta\\g1\\forja_g1.exe")
    Parser.add_argument("--contra-congelado", action="store_true", help="no-op desde a 0.21.0 F3: o padrao ja compara o x64 com o lado C congelado")
    Args = Parser.parse_args()
    if Args.congelar and Args.contra_congelado:
        print("--congelar e --contra-congelado sao excludentes")
        return 2
    if Args.congelar:
        IniciarCongelado(CONGELAR)
    else:
        IniciarCongelado(CONTRA)  # 0.21.0 F3: padrao

    PastaTmp = tempfile.mkdtemp(prefix="verificar_nativo_")
    Falhas = 0
    try:
        Forja = Args.forja or (str(Oraculo.FORJA_G1) if Args.congelar else CopiarForja(PastaTmp))
        Pedida = SeparaPasta(Args.pasta)[0]
        Pastas = [P for P in ListarPastas() if (not Pedida or P[0] == Pedida) and (not Args.grupo or GrupoDaPasta(P[0]) == Args.grupo)]
        if not Pastas:
            print(f"--pasta {Args.pasta!r} nao bate com nenhuma pasta do corpus")
            return 2
        for Pasta3 in Pastas:
            Falhas += VerificarPasta(Forja, Pasta3, PastaTmp, Args)
        if ModoCongelado == CONGELAR:
            FecharCongelamento(Falhas)
        return 0 if Falhas == 0 else 1
    finally:
        shutil.rmtree(PastaTmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Principal())
