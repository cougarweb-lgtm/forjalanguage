"""Casos de cli do forja_dll_info (M8 H1, D-M8-2): toda DLL Forja exporta a struct {magic, formato, hash FNV-1a 64 da interface, texto, texto_len}.

Importado pelo motor verificar_cli.py. As fontes saem de um molde em Python (FonteDll): cada caso muda uma coisa so e compara o hash lido por ctypes.

Versao: 1.0.0
Changelog: 1.0.0 - M8 H1: primeira versao (3 casos: hash igual, hash diferente, ctypes contra o --debug-code interface).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import ctypes

from verificar_cli import ConferirExecucao, Rodar

MAGIA = 0x494A4446
FORMATO = 1
FNV_BASE = 0xCBF29CE484222325
FNV_PRIMO = 0x100000001B3
MASCARA_64 = 0xFFFFFFFFFFFFFFFF

MOLDE = """struct Ponto {{
    {Campos}
}}

enum Modo {{
    {Membros}
}}

const FATOR: i32 = {Constante}

pub export proc somar_ponto({Parametro}: ^Ponto, m: Modo) -> {Retorno} {{
    return {Corpo}
}}

proc main() {{
}}
"""

BASE = {"Campos": "x: i32\n    y: i32", "Membros": "lento\n    rapido", "Constante": "2", "Parametro": "p", "Retorno": "i32", "Corpo": "p.x * FATOR + p.y"}


def FonteDll(**Mudancas):
    """O molde com a base e as mudancas pedidas."""
    return MOLDE.format(**{**BASE, **Mudancas})


def Fnv1a64(Dados):
    Valor = FNV_BASE
    for Byte in Dados:
        Valor = ((Valor ^ Byte) * FNV_PRIMO) & MASCARA_64
    return Valor


def LerInfo(Dll):
    """(magic, formato, hash, texto, texto_len) que forja_dll_info devolve."""
    Biblioteca = ctypes.CDLL(str(Dll))
    Biblioteca.forja_dll_info.restype = ctypes.c_void_p
    Bruto = ctypes.string_at(Biblioteca.forja_dll_info(), 32)
    Magia, Formato = int.from_bytes(Bruto[0:4], "little"), int.from_bytes(Bruto[4:8], "little")
    Hash, Endereco, Tamanho = (int.from_bytes(Bruto[Posicao:Posicao + 8], "little") for Posicao in (8, 16, 24))
    return Magia, Formato, Hash, ctypes.string_at(Endereco, Tamanho), Tamanho


def Construir(Forja, Pasta, Nome, Fonte, Opcoes=()):
    """Grava a fonte em Pasta, constroi a DLL e devolve (Problemas, Dll)."""
    Pasta.mkdir(parents=True, exist_ok=True)
    Arquivo = Pasta / f"{Nome}.fj"
    Arquivo.write_text(Fonte, encoding="utf-8")
    Dll = Pasta / f"{Nome}.dll"
    Rodada = Rodar(Forja, ["build", "--modo", "dll", *Opcoes, str(Arquivo), "-o", str(Dll)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Dll.exists():
        Problemas.append(f"build --modo dll nao gerou {Dll.name}")
    return Problemas, Dll


def HashDe(Forja, Pasta, Nome, Fonte, Opcoes=("--release",)):
    """(Problemas, hash lido pela DLL); confere magic e formato de passagem."""
    Problemas, Dll = Construir(Forja, Pasta, Nome, Fonte, Opcoes)
    if Problemas:
        return Problemas, None
    Magia, Formato, Hash, _, _ = LerInfo(Dll)
    if (Magia, Formato) != (MAGIA, FORMATO):
        Problemas.append(f"{Nome}: magic {Magia:#x} formato {Formato}, esperado {MAGIA:#x} formato {FORMATO}")
    return Problemas, Hash


def CasoDllInfoHashIgual(Forja, PastaSaidas):
    """2 builds iguais (release e dev), so o corpo mudado, constante mudada e parametro renomeado: o mesmo hash."""
    Pasta = PastaSaidas / "dll_info_igual"
    Variantes = [("base_a", FonteDll(), ("--release",)), ("base_b", FonteDll(), ("--release",)), ("base_dev", FonteDll(), ()),
                 ("corpo", FonteDll(Corpo="p.x * FATOR + p.y * 3 + 1"), ("--release",)), ("constante", FonteDll(Constante="7"), ("--release",)),
                 ("parametro", FonteDll(Parametro="ponto", Corpo="ponto.x * FATOR + ponto.y"), ("--release",))]
    Problemas, Hashes = [], {}
    for Nome, Fonte, Opcoes in Variantes:
        Falhas, Hashes[Nome] = HashDe(Forja, Pasta, Nome, Fonte, Opcoes)
        Problemas += Falhas
    if Problemas:
        return Problemas
    for Nome, Hash in Hashes.items():
        if Hash != Hashes["base_a"]:
            Problemas.append(f"{Nome}: hash {Hash:#x} difere do da base {Hashes['base_a']:#x}, mas a interface nao mudou")
    return Problemas


def CasoDllInfoHashDiferente(Forja, PastaSaidas):
    """Campo acrescentado, campos trocados de ordem, tipo de campo mudado, retorno mudado e enum com membro novo: cada um muda o hash e todos diferem entre si."""
    Pasta = PastaSaidas / "dll_info_diferente"
    Variantes = [("base", FonteDll()), ("campo_novo", FonteDll(Campos="x: i32\n    y: i32\n    z: i32")),
                 ("ordem", FonteDll(Campos="y: i32\n    x: i32")), ("tipo", FonteDll(Campos="x: i32\n    y: i64", Corpo="p.x * FATOR + (p.y as i32)")),
                 ("retorno", FonteDll(Retorno="i64", Corpo="(p.x * FATOR + p.y) as i64")), ("enum_novo", FonteDll(Membros="lento\n    rapido\n    veloz"))]
    Problemas, Hashes = [], {}
    for Nome, Fonte in Variantes:
        Falhas, Hashes[Nome] = HashDe(Forja, Pasta, Nome, Fonte)
        Problemas += Falhas
    if Problemas:
        return Problemas
    for Nome, Hash in Hashes.items():
        if Nome != "base" and Hash == Hashes["base"]:
            Problemas.append(f"{Nome}: o hash e igual ao da base ({Hash:#x}), mas a interface mudou")
    if len(set(Hashes.values())) != len(Hashes):
        Problemas.append(f"hashes repetidos entre variantes diferentes: { {N: f'{H:#x}' for N, H in Hashes.items()} }")
    return Problemas


def TextoDoDebugCode(Pasta, Nome):
    """O corpo da secao '## interface' do <dll>.debug.txt."""
    Corpo = (Pasta / f"{Nome}.dll.debug.txt").read_text(encoding="utf-8")
    Inicio = Corpo.index("## interface\n") + len("## interface\n")
    Fim = Corpo.find("\n## ", Inicio)
    return Corpo[Inicio:] if Fim < 0 else Corpo[Inicio:Fim + 1]


def CasoDllInfoCtypesIgualAoDebugCode(Forja, PastaSaidas):
    """magic, formato, hash, texto e texto_len lidos por ctypes batem com o --debug-code interface; o hash e o FNV-1a 64 do texto; o .h so tem os prototipos."""
    Pasta = PastaSaidas / "dll_info_ctypes"
    Problemas, Dll = Construir(Forja, Pasta, "completo", FonteDll(), ("--debug-code", "interface"))
    if Problemas:
        return Problemas
    Magia, Formato, Hash, Texto, Tamanho = LerInfo(Dll)
    Esperado = TextoDoDebugCode(Pasta, "completo")
    Cabecalho = (Pasta / "completo.h").read_text(encoding="utf-8")
    if (Magia, Formato) != (MAGIA, FORMATO):
        Problemas.append(f"magic {Magia:#x} formato {Formato}, esperado {MAGIA:#x} formato {FORMATO}")
    if Texto.decode("utf-8") != Esperado or Tamanho != len(Esperado.encode("utf-8")):
        Problemas.append(f"texto da DLL ({Tamanho} bytes) difere do --debug-code interface: {Texto.decode('utf-8')!r} contra {Esperado!r}")
    if Hash != Fnv1a64(Esperado.encode("utf-8")):
        Problemas.append(f"hash {Hash:#x} nao e o FNV-1a 64 do texto ({Fnv1a64(Esperado.encode('utf-8')):#x})")
    if "forja_dll_info" in Cabecalho:
        Problemas.append(".h gerado cita forja_dll_info (o .h nao muda)")
    if not Esperado.startswith("somar_ponto(^Ponto, Modo) -> i32\n") or "struct Ponto size 8 align 4 { x: i32 @0; y: i32 @4; }" not in Esperado:
        Problemas.append(f"texto canonico inesperado: {Esperado!r}")
    return Problemas
