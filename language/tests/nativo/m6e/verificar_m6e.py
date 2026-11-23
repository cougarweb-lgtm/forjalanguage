"""verificar_m6e.py - suite do M6e: zera do delete, 3 pilhas no R0008, append que move, guarda de memoria e E0913.

Cada fixture de tests\\nativo\\m6e (e as 9 de tests\\semantica com emprestimo) roda com a COPIA do forja em %TEMP%. Dev no
Windows (forja run/test/check) e Linux (ELF no wsl alpine-build). Confere exit, stdout exato, codigos R na linha JSON do
stderr, bloco da causa (campo "bloco") e campos de pilha. Fixture que ainda nao passa (passo C pendente) sai FALHOU: e o esperado do P93a.
--selftest confere o proprio harness sem chamar o compilador (e o aceite do P93a).

Versao: 0.2.3
Changelog:
  0.2.3 - 0.21.0 F4 passo 3: sai o backend C do --release-zera e do ConferirRelease (a perna 'c' e o --modo c, aposentados); os pares de release comparam so windows (x64 PE) e linux (x64 ELF).
  0.2.2 - M6e P93k: Caso ganha Teto (processo que passa dele = falha, nao trava); fixtures lista_cresce, print_apos_delete, thread_duas_falham; o oraculo --text-igual usa guarda/texto_igual.fj (sem append: com a opcao o BIR chama forja_lista_crescer_alvo); a janela do runtime aceita o delta do rel32 em [D - 0x20, D + 0x10] multiplo de 16 (o alvo no meio da tabela cresce 16 a menos; o 0x200 fixo so valia para o guarda_ok) e relata os deltas vistos.
  0.2.1 - M6e P93d' (ERRATA E6): --text-igual = .text do programa igual byte a byte ate a faixa runtime do --debug-code json, listagem asm igual, e no runtime so janelas de 4 bytes com o mesmo delta int32 D (.rdata; PE 0x200).
  0.2.0 - M6e P93d: --text-igual (.text do exe dev com e sem --guarda-memoria byte a byte igual, PE e ELF; relata os bytes do exe).
  0.1.3 - M6e P93c: --grupo aceita lista separada por virgula (pilhas,append).
  0.1.2 - M6e P93b: --release-zera compila os 2 fontes com o mesmo nome/caminho, x64 PE + x64 ELF + .c do backend C (3 alvos).
  0.1.1 - M6e P93a' (ERRATA E1-E2): lista_zera vira controle (exit 0, "3\n0\n0\n1"); append_no_morto removido (E0311:
          lista nao passa por valor, ver CENSO); campo do bloco do R0008 = "bloco" (depois de "causa"), nao "tipo".
  0.1.0 - M6e P93a: primeira versao (grupos zera|pilhas|append|guarda|estatico|tudo; --alvo windows|linux|ambos; --repetir;
          --release-zera; --selftest). --text-igual (PE/ELF) fica para o P93d; --guarda no linux para o P93f.
Uso: python tests\\nativo\\m6e\\verificar_m6e.py --selftest
     python tests\\nativo\\m6e\\verificar_m6e.py --grupo zera --alvo ambos [--repetir N] [--forja <forja.exe>]
     python tests\\nativo\\m6e\\verificar_m6e.py --release-zera [--alvo ambos] [--forja <forja.exe>]
Depende de: stdlib do Python; forja (copia em %TEMP%); wsl alpine-build (alvo linux).
"""

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

PastaM6e = Path(__file__).resolve().parent
PastaTests = PastaM6e.parents[1]
PastaSemantica = PastaTests / "semantica"
sys.path.insert(0, str(PastaTests))
from rcode_normalizar import NormalizarLinhasR  # noqa: E402  (ponto unico dos campos do rastreio, D-M6-6)

ForjaPadrao = Path(os.environ.get("TEMP", tempfile.gettempdir())) / "forja_m6e.exe"
TetoSegundos = 120
GUARDA = ["--guarda-memoria"]
CAMPOS_PILHA = ["pilha_alocacao", "pilha_delete", "pilha"]
GRUPOS = ("zera", "pilhas", "append", "guarda", "estatico")


def Caso(Grupo, Saida, Codigos=(), Bloco=None, Pilhas=(), Opcoes=(), So="ambos", Modo="run", Exit=0, Teto=TetoSegundos):
    """Expectativa de uma fixture: exit, stdout exato, codigos R (em ordem), bloco da causa, campos de pilha; Teto = segundos do processo (autotravamento vira falha)."""
    return {"grupo": Grupo, "saida": Saida, "codigos": list(Codigos), "bloco": Bloco, "pilhas": list(Pilhas),
            "opcoes": list(Opcoes), "so": So, "modo": Modo, "exit": Exit, "teto": Teto}


# arquivo (relativo a tests\\nativo\\m6e) -> expectativa. Tudo o que esta aqui FALHA HOJE (passo C pendente),
# exceto os controles (guarda_ok, delete_duplo_mesmo_nome nao, release_zera_a/b sao par de oraculo).
TABELA = {
    "zera/delete_zera.fj": Caso("zera", "true"),
    "zera/delete_duplo_mesmo_nome.fj": Caso("zera", "7\nok"),
    "zera/lista_zera.fj": Caso("zera", "3\n0\n0\n1"),
    "pilhas/delete_duplo_copia.fj": Caso("pilhas", "", ["R0008"], "heap", CAMPOS_PILHA, Exit=101),
    "append/lista_movida_sem_guarda.fj": Caso("append", "-2459565876494606883"),
    "guarda/uso_apos_delete.fj": Caso("guarda", "", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, Exit=101),
    "guarda/escrita_apos_delete.fj": Caso("guarda", "", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, Exit=101),
    "guarda/delete_duplo_guarda.fj": Caso("guarda", "antes", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, Exit=101),
    "guarda/lista_movida.fj": Caso("guarda", "1", ["R0008"], "lista", CAMPOS_PILHA, GUARDA, Exit=101),
    "guarda/arena_destruida.fj": Caso("guarda", "2", ["R0008"], "arena", CAMPOS_PILHA, GUARDA, Exit=101),
    "guarda/alem_do_fim.fj": Caso("guarda", "", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, "windows", Exit=101),
    "guarda/thread_uso.fj": Caso("guarda", "", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, "windows", Exit=101),
    "guarda/teste_guarda.fj": Caso("guarda", "", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, "windows", Modo="test", Exit=101),
    "guarda/guarda_ok.fj": Caso("guarda", "10000\n9999\n42", Opcoes=GUARDA),
    "guarda/lista_cresce.fj": Caso("guarda", "1000\n5\ntrue\n1003\n2999\n12", Opcoes=GUARDA, Teto=30),
    "guarda/print_apos_delete.fj": Caso("guarda", "", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, "windows", Exit=101, Teto=15),
    "guarda/thread_duas_falham.fj": Caso("guarda", "", ["R0008"], "heap", CAMPOS_PILHA, GUARDA, "windows", Exit=101, Teto=15),
}
# controles: o mesmo stdout com e sem --guarda-memoria (D-M6e-6); sem a opcao o guarda_ok ja passa hoje
CONTROLE_SEM_GUARDA = {"guarda/guarda_ok.fj", "guarda/lista_cresce.fj"}
# par do oraculo D-M6e-10(1): delete(p) == delete(p); p = none (exe release byte a byte)
PARES_RELEASE = [("zera/release_zera_a.fj", "zera/release_zera_b.fj")]
# fixtures de E0913 (tests\\semantica): err_ tem .esperado.jsonl; ok_ nao tem (tem de compilar sem erro)
SEMANTICA_E0913 = [
    "err_emprestimo_append", "err_emprestimo_fatia", "err_emprestimo_for_ref", "err_emprestimo_laco",
    "err_emprestimo_get_ptr", "ok_emprestimo_refetch", "ok_emprestimo_uso_antes", "ok_emprestimo_indice",
    "ok_emprestimo_outra_lista", "ok_emprestimo_if_sai", "err_emprestimo_if",
]


def LerAchadosJson(TextoSaida):
    """Cada achado e uma linha JSON DENTRO da saida (pode vir misturada com texto). So dicts validos."""
    Achados = []
    for Linha in TextoSaida.splitlines():
        Inicio = Linha.find("{")
        if Inicio < 0:
            continue
        try:
            Achados.append(json.loads(Linha[Inicio:]))
        except ValueError:
            continue
    return Achados


def ParaWsl(Caminho):
    """C:/x/y -> /mnt/c/x/y."""
    Texto = Path(Caminho).as_posix()
    return "/mnt/" + Texto[0].lower() + Texto[2:] if len(Texto) > 2 and Texto[1] == ":" else Texto


def ExecutarWindows(Forja, Fixture, Opcoes, Modo, Tmp, Teto=TetoSegundos):
    """Dev no Windows: forja run/test no mesmo arquivo. Pasta propria e limpa por rodada. Devolve (exit, stdout, stderr); passou de Teto = (-1, "", "timeout")."""
    Pasta = Path(Tmp) / ("win_" + Path(Fixture).stem + "_" + Modo)
    shutil.rmtree(Pasta, ignore_errors=True)
    Pasta.mkdir(exist_ok=True)
    Comando = [str(Forja), Modo, *Opcoes, str(PastaM6e / Fixture)]
    try:
        Resultado = subprocess.run(Comando, capture_output=True, text=True, timeout=Teto, cwd=Pasta)
    except subprocess.TimeoutExpired:
        return -1, "", f"timeout de {Teto} s (trava?)"
    return Resultado.returncode, Resultado.stdout.replace("\r\n", "\n").strip(), Resultado.stderr


def ExecutarLinux(Forja, Fixture, Opcoes, Tmp):
    """Dev no Linux: forja build --target x86_64-linux (o forja original) e o ELF roda no wsl alpine-build."""
    Elf = Path(Tmp) / (Path(Fixture).stem + "_linux.elf")
    Build = subprocess.run([str(Forja), "build", *Opcoes, "--target", "x86_64-linux", str(PastaM6e / Fixture), "-o", str(Elf)],
                           capture_output=True, text=True, timeout=TetoSegundos, cwd=Tmp)
    if Build.returncode != 0:
        return Build.returncode, "", "build: " + (Build.stdout + Build.stderr)[:300]
    Pasta = Path(Tmp) / ("linux_" + Path(Fixture).stem)
    shutil.rmtree(Pasta, ignore_errors=True)
    Pasta.mkdir(exist_ok=True)
    Resultado = subprocess.run(["wsl.exe", "-d", "alpine-build", "--cd", ParaWsl(Pasta), "--", ParaWsl(Elf)],
                               capture_output=True, timeout=TetoSegundos)
    Erro = Resultado.stderr.decode("utf-8", errors="replace").replace("\x00", "")
    return Resultado.returncode, Resultado.stdout.decode("utf-8", errors="replace").replace("\r", "").strip(), Erro


def ConferirCaso(Caso_, Exit, Saida, Erro):
    """Lista de problemas (vazia = passou)."""
    Problemas = []
    if Exit != Caso_["exit"]:
        Problemas.append(f"exit {Exit}, esperado {Caso_['exit']}")
    if Saida != Caso_["saida"]:
        Problemas.append(f"stdout {Saida[:80]!r}, esperado {Caso_['saida'][:80]!r}")
    Achados = [A for A in LerAchadosJson(Erro) if str(A.get("codigo", "")).startswith("R")]
    Codigos = [A["codigo"] for A in Achados]
    if Codigos != Caso_["codigos"]:
        Problemas.append(f"R-codes {Codigos}, esperado {Caso_['codigos']}")
    for A in Achados:
        if Caso_["bloco"] and A.get("bloco") != Caso_["bloco"]:
            Problemas.append(f"bloco {A.get('bloco')!r}, esperado {Caso_['bloco']!r}")
        for Campo in Caso_["pilhas"]:
            if Campo not in A:
                Problemas.append(f"R0008 sem o campo {Campo}")
    return Problemas


def RodarCaso(Forja, Rel, Caso_, Alvo, Tmp):
    """Roda uma fixture num alvo; devolve lista de problemas (vazia = passou)."""
    if Alvo == "linux":
        Exit, Saida, Erro = ExecutarLinux(Forja, Rel, Caso_["opcoes"], Tmp)
    else:
        Exit, Saida, Erro = ExecutarWindows(Forja, Rel, Caso_["opcoes"], Caso_["modo"], Tmp, Caso_.get("teto", TetoSegundos))
    return ConferirCaso(Caso_, Exit, Saida, Erro)


def ConferirSemantica(Forja, Nome, Tmp):
    """E0913: o forja check da fixture acha exatamente o que o .esperado.jsonl diz (ok_ = nenhum E0913)."""
    Fonte = PastaSemantica / (Nome + ".fj")
    Saida = subprocess.run([str(Forja), "check", str(Fonte)], capture_output=True, text=True, timeout=TetoSegundos, cwd=Tmp)
    Achados = [A for A in LerAchadosJson(Saida.stdout + Saida.stderr) if A.get("codigo") == "E0913"]
    Arquivo = PastaSemantica / (Nome + ".esperado.jsonl")
    Esperados = [json.loads(L) for L in Arquivo.read_text(encoding="utf-8").splitlines() if L.strip()] if Arquivo.is_file() else []
    Problemas = []
    if [(A.get("linha"), A.get("coluna")) for A in Achados] != [(E["linha"], E["coluna"]) for E in Esperados]:
        Problemas.append(f"E0913 {[(A.get('linha'), A.get('coluna')) for A in Achados]}, esperado {[(E['linha'], E['coluna']) for E in Esperados]}")
    return Problemas


def ConferirRelease(Forja, Par, Alvo, Tmp):
    """Dois exe release (delete(p) e delete(p); p = none) byte a byte iguais (D-M6e-10(1))."""
    # M6e P93b: os 2 fontes entram com o MESMO nome e caminho (o exe guarda o caminho do .fj), um depois do outro; alvos: windows = x64 PE,
    # windows = x64 PE, linux = x64 ELF (o backend C saiu na 0.21.0).
    Fonte = Path(Tmp) / "par_release" / "par.fj"
    Fonte.parent.mkdir(exist_ok=True)
    Saidas = []
    for Rel in Par:
        Saida = Path(Tmp) / ("rel_" + Path(Rel).stem + "_" + Alvo)
        shutil.copyfile(PastaM6e / Rel, Fonte)
        Opcoes = {"windows": ["--release", "--backend", "x64"], "linux": ["--release", "--target", "x86_64-linux"]}[Alvo]
        Build = subprocess.run([str(Forja), "build", *Opcoes, str(Fonte), "-o", str(Saida)], capture_output=True, text=True, timeout=TetoSegundos, cwd=Tmp)
        if Build.returncode != 0:
            return [f"build release de {Rel} falhou: {(Build.stdout + Build.stderr)[:160]}"]
        Saidas.append(Saida)
    return [] if Saidas[0].read_bytes() == Saidas[1].read_bytes() else [f"release {Par[0]} != {Par[1]} ({Alvo})"]


def SecaoPorNome(Dados, Nomes):
    """Bytes da primeira secao de um PE (tabela COFF) ou ELF64 (section headers) cujo nome esta em Nomes; None se nao achar."""
    if Dados[:2] == b"MZ":
        Pe = struct.unpack_from("<I", Dados, 0x3C)[0]
        NSecoes, = struct.unpack_from("<H", Dados, Pe + 6)
        TamOpcional, = struct.unpack_from("<H", Dados, Pe + 20)
        Tabela = Pe + 24 + TamOpcional
        for K in range(NSecoes):
            Nome = Dados[Tabela + 40 * K:Tabela + 40 * K + 8].rstrip(b"\0")
            TamRaw, PtrRaw = struct.unpack_from("<II", Dados, Tabela + 40 * K + 16)
            if Nome in Nomes:
                return Dados[PtrRaw:PtrRaw + TamRaw]
        return None
    if Dados[:4] == b"\x7fELF":
        ShOff, = struct.unpack_from("<Q", Dados, 0x28)
        ShEntSize, ShNum, ShStrNdx = struct.unpack_from("<HHH", Dados, 0x3A)
        Cabecas = [struct.unpack_from("<IIQQQQ", Dados, ShOff + ShEntSize * K) for K in range(ShNum)]
        Strtab = Cabecas[ShStrNdx][4]
        for Nome_, _Tipo, _Flags, _Addr, Off, Tam in Cabecas:
            Fim = Dados.index(b"\0", Strtab + Nome_)
            if Dados[Strtab + Nome_:Fim] in Nomes:
                return Dados[Off:Off + Tam]
    return None


def InicioRuntimeNoTexto(CaminhoJson):
    """Offset da faixa "runtime" dentro do .text (rva da faixa - menor rva de funcao) lido do --debug-code json; None se nao achar."""
    Runtime, Inicio = None, None
    for Linha in Path(CaminhoJson).read_text(encoding="utf-8").splitlines():
        Reg = json.loads(Linha)
        if Reg.get("tipo") == "faixa" and Reg.get("origem") == "runtime":
            Runtime = Reg["rva"]
        elif Reg.get("tipo") == "funcao":
            Inicio = Reg["rva"] if Inicio is None else min(Inicio, Reg["rva"])
    return None if Runtime is None or Inicio is None else Runtime - Inicio


def DeltaValido(Delta, DeltaMax):
    """P93k: o rel32 de um alvo depois da tabela cresce o que a tabela cresceu (a menos do arredondamento do .rdata no arquivo e do alinhamento de 16): multiplo de 16 em [D - 0x20, D + 0x10]."""
    return Delta % 16 == 0 and DeltaMax - 0x20 <= Delta <= DeltaMax + 0x10


def DiferencasRuntime(Sem, Com, Delta, Deltas=None):
    """Toda diferenca entre Sem e Com cabe numa janela de 4 bytes cujo int32 com-sem e um delta valido (<= Delta, multiplo de 16)? Devolve (problemas, n_janelas); Deltas (set) recebe os vistos."""
    if len(Sem) != len(Com):
        return [f"runtime com tamanho diferente ({len(Sem)} x {len(Com)})"], 0
    Janelas, Fim, K = 0, 0, 0
    while K < len(Sem):
        if Sem[K] == Com[K]:
            K += 1
            continue
        Achou = None
        for Ini in range(max(Fim, K - 3), K + 1):
            Visto = struct.unpack_from("<i", Com, Ini)[0] - struct.unpack_from("<i", Sem, Ini)[0] if Ini + 4 <= len(Sem) else None
            if Visto is not None and DeltaValido(Visto, Delta):
                Achou = Ini
                if Deltas is not None:
                    Deltas.add(Visto)
                break
        if Achou is None:
            return [f"diferenca no runtime em +{K:#x} fora de janela com delta <= {Delta:#x}"], Janelas
        Janelas += 1
        Fim = K = Achou + 4
    return [], Janelas


def ConstruirParaTextoIgual(Forja, Alvo, Tmp, Nome, Extra, Item):
    """Constroi texto_igual.fj dev com --debug-code Item; devolve (exe, arquivo de debug) ou (None, motivo)."""
    Saida = Path(Tmp) / f"texto_{Alvo}_{Nome}_{Item}.exe"
    Alvo_ = ["--target", "x86_64-linux"] if Alvo == "linux" else []
    Build = subprocess.run([str(Forja), "build", *Extra, *Alvo_, "--debug-code", Item, str(PastaM6e / "guarda/texto_igual.fj"), "-o", str(Saida)],
                           capture_output=True, text=True, timeout=TetoSegundos, cwd=Tmp)
    if Build.returncode != 0:
        return None, f"build {Nome} {Item} falhou: {(Build.stdout + Build.stderr)[:160]}"
    return Saida, Saida.with_name(Saida.name + (".debug.jsonl" if Item == "json" else ".debug.txt"))


def ConferirTextoIgual(Forja, Alvo, Tmp):
    """D-M6e-10(2) com a ERRATA E6: .text do programa igual byte a byte, listagem asm igual e, no runtime, so rel32 deslocados por um delta unico D
    (D = crescimento do .rdata; no ELF, se D diferir do delta do .rdata, vai no relato). Devolve (problemas, relato)."""
    Exes, Depuracao = {}, {}
    for Nome, Extra in (("sem", []), ("com", GUARDA)):
        for Item in ("json", "asm"):
            Exe, Arq = ConstruirParaTextoIgual(Forja, Alvo, Tmp, Nome, Extra, Item)
            if Exe is None:
                return [Arq], {}
            Exes[Nome], Depuracao[(Nome, Item)] = Exe.read_bytes(), Arq
    Textos = {N: SecaoPorNome(D, (b".text",)) for N, D in Exes.items()}
    Rdatas = {N: SecaoPorNome(D, (b".rdata", b".rodata")) for N, D in Exes.items()}
    Relato = {"bytes": (len(Exes["sem"]), len(Exes["com"]))}
    if None in Textos.values() or None in Rdatas.values():
        return ["secao .text/.rdata nao achada"], Relato
    Problemas = []
    if Depuracao[("sem", "asm")].read_bytes() != Depuracao[("com", "asm")].read_bytes():
        Problemas.append(f"listagem --debug-code asm difere ({Alvo})")
    Corte = InicioRuntimeNoTexto(Depuracao[("sem", "json")])
    if Corte is None or Corte != InicioRuntimeNoTexto(Depuracao[("com", "json")]):
        return Problemas + ["faixa runtime ausente ou em lugar diferente no --debug-code json"], Relato
    if Textos["sem"][:Corte] != Textos["com"][:Corte]:
        Problemas.append(f".text do programa difere ({Alvo})")
    DeltaRdata = len(Rdatas["com"]) - len(Rdatas["sem"])
    Delta = 0x200 if Alvo == "windows" else DeltaRdata
    Relato["delta_rdata"] = DeltaRdata
    if Delta != DeltaRdata:
        Relato["delta_rdata_diverge"] = True
    Vistos = set()
    Dif, Relato["janelas_runtime"] = DiferencasRuntime(Textos["sem"][Corte:], Textos["com"][Corte:], Delta, Vistos)
    Relato["deltas_vistos"] = sorted(hex(V) for V in Vistos)
    return Problemas + Dif, Relato


def ExecutarTextoIgual(Forja, Alvos, Tmp):
    Falhas = []
    for Alvo in Alvos:
        Motivo, Relato = ConferirTextoIgual(Forja, Alvo, Tmp)
        print(f"  exe {Alvo}: {Relato}")
        if Motivo:
            Falhas.append((f"text-igual [{Alvo}]", Motivo))
    return len(Alvos), Falhas


def ListarRelatorio(Nome, Falhas, Total):
    print(f"m6e {Nome}: {Total - len(Falhas)}/{Total} ok")
    for Rel, Motivo in Falhas:
        print(f"  FALHOU {Rel}: {'; '.join(Motivo)}")


def ExecutarGrupo(Forja, Grupo, Alvos, Repetir, Tmp):
    """Roda as fixtures do grupo em cada alvo, Repetir vezes. Devolve (total, falhas)."""
    Falhas, Total = [], 0
    for Rel, Caso_ in TABELA.items():
        if Grupo not in ("tudo", Caso_["grupo"]):
            continue
        for Alvo in Alvos:
            if Caso_["so"] not in ("ambos", Alvo):
                continue
            for _ in range(Repetir):
                Total += 1
                Motivo = RodarCaso(Forja, Rel, Caso_, Alvo, Tmp)
                if Motivo:
                    Falhas.append((f"{Rel} [{Alvo}]", Motivo))
                    break
            if Rel in CONTROLE_SEM_GUARDA and Grupo in ("tudo", "guarda"):
                Total += 1
                Sem = dict(Caso_, opcoes=[])
                Motivo = RodarCaso(Forja, Rel, Sem, Alvo, Tmp)
                if Motivo:
                    Falhas.append((f"{Rel} sem guarda [{Alvo}]", Motivo))
    return Total, Falhas


def ExecutarEstatico(Forja, Tmp):
    Falhas = []
    for Nome in SEMANTICA_E0913:
        Motivo = ConferirSemantica(Forja, Nome, Tmp)
        if Motivo:
            Falhas.append((f"semantica/{Nome}.fj", Motivo))
    return len(SEMANTICA_E0913), Falhas


def ExecutarRelease(Forja, Alvos, Tmp):
    Falhas = []
    for Par in PARES_RELEASE:
        for Alvo in Alvos:
            Motivo = ConferirRelease(Forja, Par, Alvo, Tmp)
            if Motivo:
                Falhas.append((f"{Par[0]} x {Par[1]} [{Alvo}]", Motivo))
    return len(PARES_RELEASE) * len(Alvos), Falhas


def SelfTest():
    """Confere o harness sem compilador: tabela x disco, pares, E0913 x jsonl, leitura do JSON, normalizador."""
    Falhas = []
    for Rel, Caso_ in TABELA.items():
        if not (PastaM6e / Rel).is_file():
            Falhas.append("fixture ausente: " + Rel)
        if Caso_["grupo"] not in GRUPOS:
            Falhas.append("grupo desconhecido: " + Rel)
    for Par in PARES_RELEASE:
        for Rel in Par:
            if not (PastaM6e / Rel).is_file():
                Falhas.append("par ausente: " + Rel)
    for Nome in SEMANTICA_E0913:
        if not (PastaSemantica / (Nome + ".fj")).is_file():
            Falhas.append("semantica ausente: " + Nome)
        if Nome.startswith("err_") and not (PastaSemantica / (Nome + ".esperado.jsonl")).is_file():
            Falhas.append("jsonl ausente: " + Nome)
    Amostra = '{"codigo": "R0008", "tipo": "runtime", "arquivo": "a.fj", "linha": 3, "causa": "x", "bloco": "heap", "pilha_alocacao": [], "pilha_delete": [], "pilha": []}'
    if len(LerAchadosJson("lixo\n" + Amostra + "\n")) != 1:
        Falhas.append("LerAchadosJson nao acha o JSON no meio do texto")
    if "pilha_alocacao" in NormalizarLinhasR(Amostra) or "pilha_delete" in NormalizarLinhasR(Amostra):
        Falhas.append("rcode_normalizar nao tira pilha_alocacao/pilha_delete")
    if Caso("zera", "x", ["R0008"], "heap", CAMPOS_PILHA)["pilhas"] != CAMPOS_PILHA:
        Falhas.append("Caso nao guarda os campos de pilha")
    if ConferirCaso(Caso("zera", "x"), 0, "x", "") != []:
        Falhas.append("ConferirCaso reprova um caso certo")
    if ConferirCaso(Caso("zera", "x"), 1, "y", "") == []:
        Falhas.append("ConferirCaso aprova um caso errado")
    if not (PastaM6e / "CUSTO_BASE.txt").is_file():
        Falhas.append("CUSTO_BASE.txt ausente (rode medir_custo_base.py --gravar)")
    if Falhas:
        for Motivo in Falhas:
            print("selftest: " + Motivo)
        print("verificar_m6e selftest: FALHOU")
        return 1
    print("verificar_m6e selftest: PASSOU")
    return 0


def Principal(Argumentos):
    Parser = argparse.ArgumentParser(description="suite do M6e (P93a)")
    Parser.add_argument("--grupo", default="tudo", help="um ou mais de " + "|".join(GRUPOS + ("tudo",)) + ", separados por virgula")
    Parser.add_argument("--alvo", choices=("windows", "linux", "ambos"), default="ambos")
    Parser.add_argument("--repetir", type=int, default=1)
    Parser.add_argument("--forja", default=str(ForjaPadrao))
    Parser.add_argument("--release-zera", action="store_true")
    Parser.add_argument("--text-igual", action="store_true")
    Parser.add_argument("--selftest", action="store_true")
    Opcoes = Parser.parse_args(Argumentos)
    if Opcoes.selftest:
        return SelfTest()
    Forja = Path(Opcoes.forja)
    if not Forja.is_file():
        print("falta a copia do forja (passe --forja): " + str(Forja))
        return 2
    Alvos = ["windows", "linux"] if Opcoes.alvo == "ambos" else [Opcoes.alvo]
    Tmp = tempfile.mkdtemp(prefix="m6e_")
    try:
        if Opcoes.text_igual:
            Total, Falhas = ExecutarTextoIgual(Forja, Alvos, Tmp)
            ListarRelatorio("text-igual", Falhas, Total)
        elif Opcoes.release_zera:
            Total, Falhas = ExecutarRelease(Forja, Alvos, Tmp)
            ListarRelatorio("release-zera", Falhas, Total)
        else:
            Total, Falhas = 0, []
            if any(N in ("tudo", "estatico") for N in Opcoes.grupo.split(",")):
                T, F = ExecutarEstatico(Forja, Tmp)
                Total, Falhas = Total + T, Falhas + F
                ListarRelatorio("estatico", F, T)
            for Nome in Opcoes.grupo.split(","):
                if Nome == "estatico":
                    continue
                T, F = ExecutarGrupo(Forja, Nome, Alvos, Opcoes.repetir, Tmp)
                Total, Falhas = Total + T, Falhas + F
                ListarRelatorio(Nome, F, T)
    finally:
        shutil.rmtree(Tmp, ignore_errors=True)
    return 0 if not Falhas else 1


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
