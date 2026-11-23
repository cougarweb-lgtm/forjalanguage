#!/usr/bin/env python3
"""verificar_a64_selftest.py - provas do verificar_a64 (--selftest): puras (sem WSL nem compilador) e, com WSL, ELFs plantados no qemu-user.

Provas puras: causa do diagnostico, recusa so no candidato, PosEsperado, --fechar (PULADO aceito) e a classe nan_bits.
Lote (com WSL): 6 ELFs plantados (ok, saida errada, exit errado, timeout, PosEsperado ok e errado) rodam num lote no qemu-user.
Uso: python -I verificar_a64.py --selftest (o harness importa este modulo so nessa opcao; ele importa o harness de volta).
Versao: 1.0.0 (22/11/2026) A4.5 - provas movidas de verificar_a64.py 1.3.2 (teto de 800 linhas); provas da classe nan_bits.
"""
import struct
import tempfile
from pathlib import Path

import conjunto_a64
import wsl_lote
from verificar_a64 import (CAUSA_TAMANHO, FRENTE_IGUAL, TEMPO_SELFTEST, AplicarAceitas, AvaliarElf, TempoConhecido, TempoDoCaso, CausaDoDiagnostico, Esperado, Falha, MotivosDoEstado,
                           Obra, Opcoes, PuladoAceito, RegistroDoCandidato, RodarCandidatos, Saida, Tarefa)
from verificar_a64_nan import NormalizarNan


def GerarElfPrograma(Mensagem: bytes, Saida: int) -> bytes:
    """ELF64 AArch64 minimo: write(1, Mensagem, len); exit(Saida). Mesmos codigos de elf_a64_minimo.py (conferidos a mao)."""
    Tamanho = len(Mensagem)
    Palavras = [
        0xD2800020,  # MOVZ x0,#1
        0x100000E1,  # ADR x1,msg (imm 28: o texto fica 32 bytes depois do inicio do codigo)
        0xD2800000 | (Tamanho << 5) | 2,  # MOVZ x2,#len
        0xD2800808,  # MOVZ x8,#64 (write)
        0xD4000001,  # SVC #0
        0xD2800000 | (Saida << 5),  # MOVZ x0,#saida
        0xD2800BA8,  # MOVZ x8,#93 (exit)
        0xD4000001,  # SVC #0
    ]
    return MontarCabecalhoElf(b"".join(struct.pack("<I", P) for P in Palavras) + Mensagem)


def GerarElfLaco() -> bytes:
    """ELF que nunca termina: B . (serve para o caso de timeout)."""
    return MontarCabecalhoElf(struct.pack("<I", 0x14000000))


def MontarCabecalhoElf(Codigo: bytes) -> bytes:
    """ELF64 AArch64: cabecalho, 1 PT_LOAD RX em 0x400000 com o codigo (offset 120) e tabela de secoes (nula + .shstrtab).
    A tabela de secoes e obrigatoria: sem ela o tradutor ARM do emulador recusa o ELF com 'invalid e_shstrndx' (ver elf_a64_minimo.py 1.1.0)."""
    Entrada = ENDERECO_BASE + DESLOCAMENTO_CODIGO
    Tamanho = DESLOCAMENTO_CODIGO + len(Codigo)
    Nomes = b"\x00.shstrtab\x00"  # indice 1 = ".shstrtab"
    DeslocamentoNomes = Tamanho
    DeslocamentoSecoes = (DeslocamentoNomes + len(Nomes) + 7) // 8 * 8  # tabela de secoes alinhada a 8 bytes
    Ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)  # ELF64, little-endian, versao 1, SYSV
    Ehdr = Ident + struct.pack("<HHIQQQIHHHHHH", 2, MAQUINA_AARCH64, 1, Entrada, TAMANHO_EHDR, DeslocamentoSecoes, 0,
                               TAMANHO_EHDR, TAMANHO_PHDR, 1, TAMANHO_SHDR, 2, 1)
    Phdr = struct.pack("<IIQQQQQQ", 1, 5, 0, ENDERECO_BASE, ENDERECO_BASE, Tamanho, Tamanho, 0x1000)
    SecaoNula = bytes(TAMANHO_SHDR)
    SecaoNomes = struct.pack("<IIQQQQIIQQ", 1, 3, 0, 0, DeslocamentoNomes, len(Nomes), 0, 0, 1, 0)  # SHT_STRTAB
    Enchimento = bytes(DeslocamentoSecoes - DeslocamentoNomes - len(Nomes))
    return Ehdr + Phdr + Codigo + Nomes + Enchimento + SecaoNula + SecaoNomes


ENDERECO_BASE = 0x400000
TAMANHO_EHDR = 64
TAMANHO_PHDR = 56
TAMANHO_SHDR = 64
DESLOCAMENTO_CODIGO = TAMANHO_EHDR + TAMANHO_PHDR
MAQUINA_AARCH64 = 183


def PlantarCasos(Pasta: Path) -> list:
    """(nome, Elf, referencia, status esperado, atributos do caso): um positivo, tres negativos (saida errada, exit errado, timeout) e PosEsperado (igual e errado)."""
    Positivo = GerarElfPrograma(b"ola\n", 7)
    Laco = GerarElfLaco()
    Planta = [
        ("positivo", Positivo, Esperado(7, b"ola\n"), "ok", {}),
        ("saida_errada", Positivo, Esperado(7, b"oi\n"), "diverge", {}),
        ("exit_errado", Positivo, Esperado(6, b"ola\n"), "diverge", {}),
        ("timeout", Laco, Esperado(0, b""), "falha de run", {}),
        ("pos_igual", Positivo, Esperado(7, b"ola\n"), "ok", {"Pos": "echo igual", "PosEsperado": b"igual\n"}),
        ("pos_errado", Positivo, Esperado(7, b"ola\n"), "diverge", {"Pos": "echo igual", "PosEsperado": b"outro\n"}),
    ]
    Casos = []
    for Nome, Dados, Referencia, Status, Atributos in Planta:
        Arquivo = Pasta / f"{Nome}.elf"
        Arquivo.write_bytes(Dados)
        Casos.append((Nome, Arquivo, Referencia, Status, Atributos))
    return Casos


def CasoDoSelftest(Nome: str, Elf: Path, Atributos: dict) -> conjunto_a64.Caso:
    """Caso do selftest: o conjunto a64 nao traz Pos nem PosEsperado; os atributos entram no objeto (CamposDoCaso e MotivosDoEstado leem por getattr)."""
    Caso_ = conjunto_a64.Caso(f"selftest/{Nome}", Elf, "selftest")
    for Chave, Valor in Atributos.items():
        setattr(Caso_, Chave, Valor)
    return Caso_


def FalhaDeCodigo(Codigo: str) -> Falha:
    return Falha(f"build exit 1: {Codigo}", Codigo, Codigo)


def ProvasDeCausa() -> list:
    """Item 3: a causa vem do campo causa do diagnostico (texto inteiro); com runtime 'forja_X' agrupa pelo simbolo, sem ele pelos 80 primeiros caracteres."""
    Sem = "tipo ainda sem suporte no alvo aarch64 com campo de 128 bytes"
    return [
        ("causa com simbolo", CausaDoDiagnostico('{"codigo": "X0001", "causa": "runtime \'forja_panic\' sem suporte"}', "X0001"), "X0001 forja_panic"),
        ("causa sem simbolo", CausaDoDiagnostico(f'{{"codigo": "X0001", "causa": "{Sem}"}}', "X0001"), f"X0001 {Sem[:CAUSA_TAMANHO]}"),
        ("sem causa", CausaDoDiagnostico('{"codigo": "X0003"}', "X0003"), "X0003"),
        ("sem codigo", CausaDoDiagnostico("erro qualquer", None), ""),
    ]


def ProvasDeTempo() -> list:
    """tempo_conhecido: so o caso da tabela ganha o limite maior; fora da tabela o limite segue o padrao; o limite maior nunca encurta o padrao."""
    Tabela = TempoConhecido()
    Nome = next(iter(Tabela), "")
    Dentro = conjunto_a64.Caso(Nome, Path("x.fj"), "selftest")
    Fora = conjunto_a64.Caso("lowering/caso_fora_da_tabela", Path("x.fj"), "selftest")
    return [
        ("tempo: caso da tabela", TempoDoCaso(Dentro, 10), Tabela.get(Nome)),
        ("tempo: caso fora da tabela", TempoDoCaso(Fora, 10), 10),
        ("tempo: padrao maior que a tabela", TempoDoCaso(Dentro, 1000), 1000),
        ("tempo: tabela nomeia o 302", "lowering/302_fmt_ida_e_volta" in Tabela, True),
    ]


def ProvasDeRecusa() -> list:
    """Item 5: E-code so no candidato e falha de build (nao PULADO); X0001 e PULADO; E-code igual nos dois alvos e PULADO; oraculo recusa e candidato compila = diverge."""
    Tarefa_ = Tarefa(CasoDoSelftest("recusa", Path("recusa.fj"), {}), "dev")
    Sozinho = Obra(Tarefa_, 0)
    Front = Obra(Tarefa_, 1, CodigoOraculo="E0301")
    return [
        ("E-code so no candidato", StatusDe(RegistroDoCandidato(Tarefa_, Sozinho, FalhaDeCodigo("E1001"))), "falha de build"),
        ("X0001 do candidato", StatusDe(RegistroDoCandidato(Tarefa_, Sozinho, FalhaDeCodigo("X0001"))), "PULADO"),
        ("E-code igual nos dois alvos", StatusDe(RegistroDoCandidato(Tarefa_, Front, FalhaDeCodigo("E0301"))), "PULADO"),
        ("E-code diferente do oraculo", StatusDe(RegistroDoCandidato(Tarefa_, Front, FalhaDeCodigo("E1001"))), "diverge"),
        ("oraculo recusou e candidato compilou", StatusDe(RegistroDoCandidato(Tarefa_, Front, None)), "diverge"),
        ("candidato compilou sem oraculo", StatusDe(RegistroDoCandidato(Tarefa_, Sozinho, None)), None),
    ]


def StatusDe(Registro: dict | None) -> str | None:
    return None if Registro is None else Registro["status"]


def ProvasDeEstado() -> list:
    """Item 6: PosEsperado igual passa; diferente diverge; caso sem PosEsperado nao confere nada."""
    Com = CasoDoSelftest("estado", Path("estado.fj"), {"PosEsperado": b"igual\n"})
    Sem = CasoDoSelftest("sem_estado", Path("sem.fj"), {})
    return [
        ("PosEsperado igual", bool(MotivosDoEstado(Com, Saida(b"", b"", 0, Pos=b"igual\n"))), False),
        ("PosEsperado errado", bool(MotivosDoEstado(Com, Saida(b"", b"", 0, Pos=b"outro\n"))), True),
        ("sem PosEsperado", bool(MotivosDoEstado(Sem, Saida(b"", b"", 0, Pos=b"outro\n"))), False),
    ]


def ProvasDeFechar() -> list:
    """Item 8: o PULADO so passa no --fechar nas classes aceitas; X0001 sem a causa na lista reprova."""
    return [
        ("front-end igual aceito", PuladoAceito({"causa": f"{FRENTE_IGUAL} (E0301)"}, []), True),
        ("X0003 aceito", PuladoAceito({"causa": "X0003 forja_x"}, []), True),
        ("X0001 sem lista reprova", PuladoAceito({"causa": "X0001 forja_panic"}, []), False),
        ("X0001 listado aceito", PuladoAceito({"causa": "X0001 forja_panic"}, ["X0001 forja_panic"]), True),
    ]


def AvaliarSaidaNan(Referencia: bytes, Obtido: bytes) -> dict:
    """Registro de um caso com stdout esperado Referencia e stdout do candidato Obtido (exit 7 nos dois; sem stderr; sem emulador)."""
    Opc = Opcoes(Path("selftest.exe"), Path("selftest.exe"), Path("."), TEMPO_SELFTEST, True, False)
    Tarefa_ = Tarefa(CasoDoSelftest("nan", Path("nan.elf"), {}), "dev")
    return AvaliarElf(Tarefa_, Path("nan.elf"), Esperado(7, Referencia), Saida(Obtido, b"", 7), Opc)


def StatusComAceitaNan(Registro: dict) -> str:
    """Status depois da lista aceita com a classe nan_bits (o selftest nao le o arquivo: a entrada e montada aqui)."""
    AplicarAceitas([Registro], [{"nome": "nan_bits", "motivo": "selftest"}])
    return Registro["status"]


def ProvasDeNan() -> list:
    """Classe nan_bits: so a troca do sinal de NaN (f64 e f32) e aceita pela entrada; outro digito ou token dentro de numero maior continua divergencia."""
    return [
        ("nan_bits f64 aceito", StatusComAceitaNan(AvaliarSaidaNan(b"fff8000000000000\n", b"7ff8000000000000\n")), "aceita"),
        ("nan_bits f32 aceito", StatusComAceitaNan(AvaliarSaidaNan(b"ffc00000\n", b"7fc00000\n")), "aceita"),
        ("nan_bits com outro digito diverge", StatusComAceitaNan(AvaliarSaidaNan(b"fff8000000000000 1\n", b"7ff8000000000000 2\n")), "diverge"),
        ("token em numero maior nao troca", StatusComAceitaNan(AvaliarSaidaNan(b"1fff8000000000000\n", b"17ff8000000000000\n")), "diverge"),
        ("17 digitos hex nao troca", StatusComAceitaNan(AvaliarSaidaNan(b"0fff8000000000000\n", b"07ff8000000000000\n")), "diverge"),
        ("normalizacao preserva numero maior", NormalizarNan(b"1fff8000000000000\n"), b"1fff8000000000000\n"),
    ]


def ProvasPuras() -> tuple:
    """(falhas, total): provas sem WSL nem compilador; cada uma tem de bater com o valor esperado."""
    Pares = [*ProvasDeCausa(), *ProvasDeRecusa(), *ProvasDeEstado(), *ProvasDeFechar(), *ProvasDeNan(), *ProvasDeTempo()]
    return [f"{Nome}: {Obtido!r} (esperado {Esperado_!r})" for Nome, Obtido, Esperado_ in Pares if Obtido != Esperado_], len(Pares)


def ProvasDoLote() -> list:
    """Com WSL: os ELFs plantados rodam num lote no qemu-user; cada um tem de dar o status esperado (e o timeout, o motivo do limite)."""
    Falhas = []
    with tempfile.TemporaryDirectory(prefix="forja_a64_selftest_") as Pasta:
        Opc = Opcoes(Path(Pasta), Path(Pasta), Path(Pasta), TEMPO_SELFTEST, True, False, Cache=False)
        Planta = PlantarCasos(Path(Pasta))
        Obras = [Obra(Tarefa(CasoDoSelftest(Nome, Elf, Atributos), "dev"), Indice, Elf=Elf) for Indice, (Nome, Elf, _, _, Atributos) in enumerate(Planta)]
        RodarCandidatos(Obras, Opc)  # um lote so: o timeout e as divergencias passam pelo mesmo caminho do harness
        for Obra_, (Nome, Elf, Referencia, StatusEsperado, _) in zip(Obras, Planta):
            Registro = AvaliarElf(Obra_.Tarefa_, Elf, Referencia, Obra_.SaidaQemu, Opc)
            if Registro["status"] != StatusEsperado:
                Falhas.append(f"{Nome}: status {Registro['status']} (esperado {StatusEsperado}) {Registro['motivo'][:200]}")
            elif Nome == "timeout" and "passou de" not in Registro["motivo"]:
                Falhas.append(f"timeout sem o motivo do limite: {Registro['motivo'][:200]}")
    return Falhas


def Selftest() -> int:
    Falhas, Total = ProvasPuras()
    if wsl_lote.DistroDisponivel():
        Falhas += ProvasDoLote()
        Lote = "lote de 6 ELFs no qemu-user"
    else:
        Lote = "lote PULADO (WSL/distro ausente: sem qemu-user)"
    for Linha in Falhas:
        print("FALHA", Linha)
    print(f"verificar_a64 selftest: {'FALHOU ' + str(len(Falhas)) if Falhas else 'VERDE'}; provas puras {Total}; {Lote}")
    return 1 if Falhas else 0
