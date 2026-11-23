# verificar_macho.py - prova local da I2 (Mach-O arm64 dos alvos iOS): parser proprio + assinatura ad-hoc + opcodes + capstone
# Versao: 0.3.0
# Changelog: 0.3.0 - I6: confere todos os LC_LOAD_DYLIB (libSystem primeiro, depois frameworks/libobjc em caminho canonico, sem repetir), o ordinal de cada bind (1..n dylibs)
#                    e que o simbolo foi ao dylib do prefixo do nome (objc_/sel_.. libobjc, UI UIKit, NS Foundation, CF CoreFoundation, CA QuartzCore, CG CoreGraphics; `--dylibs a,b` exige esses dylibs);
#                    a desmontagem so e impressa com `--texto`.
#            0.2.0 - I5: MH_DYLIB (filetype 6): LC_ID_DYLIB `@rpath/<arquivo>`, sem LC_MAIN/PAGEZERO/DYLINKER, flags 0x100085, execSegFlags 0, trie de exports decodificada
#                    (cada nome tem de bater com o symtab e cair no __text; `--exports a,b` exige os nomes) e 0 `svc` no __text de todo binario.
#            0.1.0 - I2: reparseia o binario da Forja E o do oraculo (ld64, gabarito2/hello_opcodes) com o MESMO codigo; recalcula os hashes de pagina de 4 KiB,
#                    confere codeLimit/nCodeSlots/hashOffset/identificador, decodifica rebase/bind/exports/symtab e desmonta o __text (capstone).
# Uso: python -I verificar_macho.py [--oraculo <hello_opcodes>] [--exports a,b,c] [--dylibs objc,UIKit] [--texto] <binario> [<binario> ...]   (exit 0 = tudo certo)
# Depende de: capstone (opcional: sem ele a desmontagem e pulada e dito)
import hashlib
import os
import struct
import sys

sys.path.append(os.path.join(os.environ.get("APPDATA", ""), "Python", "Python313", "site-packages"))
try:
    import capstone
except ImportError:
    capstone = None  # lint-ok: nome-fora-do-padrao e o nome do modulo importado (import tolerante)

PaginaHash = 4096
Frameworks = ("UIKit", "Foundation", "CoreFoundation", "QuartzCore", "CoreGraphics")
CaminhosConhecidos = {"/usr/lib/libobjc.A.dylib"} | {f"/System/Library/Frameworks/{N}.framework/{N}" for N in Frameworks}
PrefixoDylib = [(("objc_", "sel_", "class_", "object_", "method_", "protocol_", "ivar_", "property_", "imp_"), "/usr/lib/libobjc.A.dylib"),
                (("UI",), "/System/Library/Frameworks/UIKit.framework/UIKit"), (("NS",), "/System/Library/Frameworks/Foundation.framework/Foundation"),
                (("CF", "kCF"), "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"),
                (("CA",), "/System/Library/Frameworks/QuartzCore.framework/QuartzCore"), (("CG",), "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
TiposLc = {0x19: "SEGMENT_64", 0x2: "SYMTAB", 0xB: "DYSYMTAB", 0xC: "LOAD_DYLIB", 0xD: "ID_DYLIB", 0xE: "LOAD_DYLINKER", 0x1B: "UUID", 0x1D: "CODE_SIGNATURE",
           0x2A: "SOURCE_VERSION", 0x26: "FUNCTION_STARTS", 0x29: "DATA_IN_CODE", 0x32: "BUILD_VERSION", 0x80000028: "MAIN", 0x80000022: "DYLD_INFO_ONLY"}


class Falhas:
    def __init__(Self):
        Self.Lista = []

    def Exigir(Self, Condicao, Texto):
        if not Condicao:
            Self.Lista.append(Texto)
        return Condicao


def LerUleb(Dados, Pos):
    Valor = 0
    Desl = 0
    while True:
        Byte = Dados[Pos]
        Pos += 1
        Valor |= (Byte & 127) << Desl
        Desl += 7
        if Byte < 128:
            return Valor, Pos


def LerMacho(Dados):
    Magia, CpuTipo, CpuSub, TipoArq, NCmds, TamCmds, Flags, _ = struct.unpack_from("<8I", Dados, 0)
    Info = {"magia": Magia, "cpu": CpuTipo, "cpusub": CpuSub, "tipo": TipoArq, "ncmds": NCmds, "tamcmds": TamCmds, "flags": Flags, "lcs": [], "segs": [], "secs": []}
    Pos = 32
    for _ in range(NCmds):
        Cmd, Tam = struct.unpack_from("<II", Dados, Pos)
        Info["lcs"].append((TiposLc.get(Cmd, hex(Cmd)), Pos, Tam))
        if Cmd == 0x19:
            Nome = Dados[Pos + 8:Pos + 24].rstrip(b"\0").decode()
            VmAddr, VmSize, FileOff, FileSize, MaxProt, InitProt, NSects, _ = struct.unpack_from("<4Q2I2I", Dados, Pos + 24)
            Info["segs"].append({"nome": Nome, "vmaddr": VmAddr, "vmsize": VmSize, "fileoff": FileOff, "filesize": FileSize, "prot": InitProt, "nsects": NSects})
            for K in range(NSects):
                Sp = Pos + 72 + 80 * K
                SecNome = Dados[Sp:Sp + 16].rstrip(b"\0").decode()
                Addr, Tamanho, Off = struct.unpack_from("<QQI", Dados, Sp + 32)
                Info["secs"].append({"nome": SecNome, "seg": Nome, "addr": Addr, "size": Tamanho, "off": Off})
        elif Cmd == 0x1D:
            Info["assina"] = struct.unpack_from("<II", Dados, Pos + 8)
        elif Cmd == 0x80000028:
            Info["entryoff"] = struct.unpack_from("<Q", Dados, Pos + 8)[0]
        elif Cmd == 0x80000022:
            Info["dyld"] = struct.unpack_from("<10I", Dados, Pos + 8)
        elif Cmd == 0x2:
            Info["symtab"] = struct.unpack_from("<4I", Dados, Pos + 8)
        elif Cmd == 0x32:
            Info["build"] = struct.unpack_from("<5I", Dados, Pos + 8)
        elif Cmd == 0xC:
            Info.setdefault("dylibs", []).append(Dados[Pos + 24:Pos + Tam].rstrip(b"\0").decode())
            Info["dylib"] = Info["dylibs"][0]
        elif Cmd == 0xD:
            Info["iddylib"] = Dados[Pos + 24:Pos + Tam].rstrip(b"\0").decode()
            Info["iddylib_versoes"] = struct.unpack_from("<III", Dados, Pos + 12)
        Pos += Tam
    return Info


def EhDylib(Info):
    return Info["tipo"] == 6


def LerTrie(Dados, Base, Tam, Fal):
    """Decodifica a trie de exports: {nome: offset}; recusa ciclo e no fora da trie."""
    Saida = {}
    Visitados = set()

    def No(Off, Prefixo):
        Fal.Exigir(Off < Tam and Off not in Visitados, f"no da trie invalido/ciclico em {Off}")
        if Off >= Tam or Off in Visitados:
            return
        Visitados.add(Off)
        Pos = Base + Off
        TermTam, Pos = LerUleb(Dados, Pos)
        if TermTam:
            _, P2 = LerUleb(Dados, Pos)
            Valor, _ = LerUleb(Dados, P2)
            Saida[Prefixo] = Valor
        Pos += TermTam
        Filhos = Dados[Pos]
        Pos += 1
        for _ in range(Filhos):
            Fim = Dados.index(b"\0", Pos)
            Rotulo = Dados[Pos:Fim].decode()
            Pos = Fim + 1
            Destino, Pos = LerUleb(Dados, Pos)
            No(Destino, Prefixo + Rotulo)
    No(0, "")
    return Saida


def LerSimbolosExternos(Dados, Info):
    """Nomes e valores dos simbolos definidos externos (N_SECT|N_EXT = 0x0F) do symtab."""
    SymOff, NSyms, StrOff, StrTam = Info["symtab"]
    Saida = {}
    for K in range(NSyms):
        NStrx, NTipo, NSect, NDesc, NValor = struct.unpack_from("<IBBHQ", Dados, SymOff + 16 * K)
        if NTipo == 0x0F:
            Inicio = StrOff + NStrx
            Saida[Dados[Inicio:Dados.index(b"\0", Inicio)].decode()] = NValor
    return Saida


def VerificarExports(Dados, Info, Esperados, Fal):
    Dy = Info["dyld"]
    Trie = LerTrie(Dados, Dy[8], Dy[9], Fal)
    Syms = LerSimbolosExternos(Dados, Info)
    Fal.Exigir(Trie == Syms, f"trie {sorted(Trie)} != symtab {sorted(Syms)}")
    Texto = [S for S in Info["secs"] if S["nome"] == "__text"][0]
    for Nome, Valor in Trie.items():
        Fal.Exigir(Texto["addr"] <= Valor < Texto["addr"] + Texto["size"], f"export {Nome} fora do __text")
        Fal.Exigir(Nome.startswith("_"), f"export {Nome} sem `_`")
    for Nome in Esperados:
        Fal.Exigir("_" + Nome in Trie, f"export esperado `_{Nome}` ausente")
    Fal.Exigir(len(Trie) > 0, "dylib sem exports")
    return sorted(Trie)


def VerificarAssinatura(Dados, Info, Nome, Fal):
    Fal.Exigir("assina" in Info, "sem LC_CODE_SIGNATURE")
    if "assina" not in Info:
        return {}
    Off, Tam = Info["assina"]
    Blob = Dados[Off:Off + Tam]
    Magia, Comp, Qtd = struct.unpack_from(">III", Blob, 0)
    Fal.Exigir(Magia == 0xFADE0CC0 and Qtd == 1, "SuperBlob invalido")
    Fal.Exigir(Off + Tam == len(Dados), "assinatura nao termina no fim do arquivo")
    Fal.Exigir(Tam % 16 == 0 and Tam >= Comp and Tam - Comp < 16, "preenchimento da assinatura")
    Tipo, OffCd = struct.unpack_from(">II", Blob, 12)
    Cd = Blob[OffCd:]
    (CdMagia, CdTam, Versao, Flags, HashOff, IdentOff, NEsp, NSlots, CodeLimit, HashSize, HashTipo, Plat, PagLog) = struct.unpack_from(">9I4B", Cd, 0)
    ExecBase, ExecLimite, ExecFlags = struct.unpack_from(">QQQ", Cd, 64)
    Ident = Cd[IdentOff:Cd.index(b"\0", IdentOff)].decode()
    Fal.Exigir(Tipo == 0 and CdMagia == 0xFADE0C02 and Versao == 0x20400 and Flags == 0x20002, f"CodeDirectory: tipo={Tipo} versao={Versao:#x} flags={Flags:#x}")
    Fal.Exigir(HashSize == 32 and HashTipo == 2 and PagLog == 12 and NEsp == 0, "hash/pagina")
    Fal.Exigir(ExecFlags == (0 if EhDylib(Info) else 1), f"execSegFlags {ExecFlags}")
    Fal.Exigir(CodeLimit == Off, f"codeLimit {CodeLimit} != dataoff da assinatura {Off}")
    Fal.Exigir(NSlots == (CodeLimit + PaginaHash - 1) // PaginaHash, "nCodeSlots")
    Fal.Exigir(HashOff == IdentOff + len(Ident) + 1 and IdentOff == 88, "hashOffset/identOffset")
    Fal.Exigir(CdTam == HashOff + 32 * NSlots and Comp == OffCd + CdTam, "comprimentos do CodeDirectory/SuperBlob")
    Fal.Exigir(Ident == Nome, f"identificador '{Ident}' != '{Nome}'")
    Ruins = 0
    for K in range(NSlots):
        Pedaco = Dados[K * PaginaHash:min((K + 1) * PaginaHash, CodeLimit)]
        if hashlib.sha256(Pedaco).digest() != Cd[HashOff + 32 * K:HashOff + 32 * K + 32]:
            Ruins += 1
    Fal.Exigir(Ruins == 0, f"{Ruins} hash(es) de pagina nao batem")
    return {"codeLimit": CodeLimit, "slots": NSlots, "ident": Ident, "execBase": ExecBase, "execLimite": ExecLimite, "execFlags": ExecFlags, "cdhash": hashlib.sha256(Cd[:CdTam]).hexdigest()[:40]}


def VerificarEstrutura(Dados, Info, Fal, Nome=""):
    Dylib = EhDylib(Info)
    Fal.Exigir(Info["magia"] == 0xFEEDFACF and Info["cpu"] == 0x0100000C and Info["tipo"] in (2, 6), "cabecalho Mach-O")
    Fal.Exigir(Info["flags"] == (0x100085 if Dylib else 0x200085), f"flags {Info['flags']:#x}")
    Nomes = [L[0] for L in Info["lcs"]]
    if Dylib:
        Fal.Exigir(Info.get("iddylib") == "@rpath/" + Nome, f"LC_ID_DYLIB {Info.get('iddylib')!r}")
        Fal.Exigir(Info.get("iddylib_versoes") == (1, 65536, 65536), f"versoes do ID_DYLIB {Info.get('iddylib_versoes')}")
        Fal.Exigir("MAIN" not in Nomes and "LOAD_DYLINKER" not in Nomes and Nomes.count("ID_DYLIB") == 1, "dylib com MAIN/DYLINKER ou sem ID_DYLIB unico")
        Fal.Exigir(Info["segs"][0]["nome"] == "__TEXT" and Info["segs"][0]["vmaddr"] == 0 and Info["segs"][0]["fileoff"] == 0, "dylib: __TEXT tem de abrir em vmaddr 0 / fileoff 0")
        Fal.Exigir(all(S["nome"] != "__PAGEZERO" for S in Info["segs"]), "dylib com __PAGEZERO")
    else:
        Fal.Exigir("ID_DYLIB" not in Nomes and "MAIN" in Nomes, "exe com ID_DYLIB ou sem MAIN")
    Fal.Exigir(Info["ncmds"] == len(Info["lcs"]) and Info["tamcmds"] == sum(L[2] for L in Info["lcs"]), "ncmds/sizeofcmds")
    Fal.Exigir(Info.get("dylib") == "/usr/lib/libSystem.B.dylib", "libSystem")
    Dylibs = Info.get("dylibs", [])
    Fal.Exigir(len(set(Dylibs)) == len(Dylibs), f"LC_LOAD_DYLIB repetido {Dylibs}")
    for Caminho in Dylibs[1:]:
        Fal.Exigir(Caminho in CaminhosConhecidos, f"dylib fora da lista conhecida: {Caminho}")
    Fal.Exigir(Info["lcs"][-1][0] == "CODE_SIGNATURE", "LC_CODE_SIGNATURE nao e o ultimo")
    Ant = None
    for Seg in Info["segs"]:
        Fal.Exigir(Seg["vmaddr"] % 16384 == 0 and (Seg["fileoff"] % 16384 == 0), f"{Seg['nome']} fora da pagina de 16 KiB")
        Fal.Exigir(Ant is None or Seg["vmaddr"] >= Ant, f"{Seg['nome']} fora de ordem/sobreposto")
        Ant = Seg["vmaddr"] + Seg["vmsize"]
    Linkedit = Info["segs"][-1]
    Fal.Exigir(Linkedit["nome"] == "__LINKEDIT" and Linkedit["fileoff"] + Linkedit["filesize"] == len(Dados), "__LINKEDIT nao cobre o fim do arquivo")
    Texto = [S for S in Info["secs"] if S["nome"] == "__text"]
    if Dylib:
        Fal.Exigir(len(Texto) == 1, "dylib sem __text unico")
    else:
        Fal.Exigir(len(Texto) == 1 and Texto[0]["off"] <= Info["entryoff"] < Texto[0]["off"] + Texto[0]["size"], "entryoff fora do __text")
    for Sec in Info["secs"]:
        Seg = [S for S in Info["segs"] if S["nome"] == Sec["seg"]][0]
        Fal.Exigir(Seg["vmaddr"] <= Sec["addr"] and Sec["addr"] + Sec["size"] <= Seg["vmaddr"] + Seg["vmsize"], f"secao {Sec['nome']} fora do segmento")


def DecodificarFixups(Dados, Info, Fal):
    Rebase, RebaseTam, Bind, BindTam, _, _, _, _, Export, ExportTam = Info["dyld"]
    Linkedit = Info["segs"][-1]
    for Nome, Off, Tam in (("rebase", Rebase, RebaseTam), ("bind", Bind, BindTam), ("export", Export, ExportTam)):
        Fal.Exigir(Tam == 0 or (Linkedit["fileoff"] <= Off and Off + Tam <= Linkedit["fileoff"] + Linkedit["filesize"]), f"{Nome} fora do __LINKEDIT")
    Binds = []
    Pos, Fim, Simbolo, Ordinal = Bind, Bind + BindTam, None, 0
    while Pos < Fim:
        Op = Dados[Pos]
        Pos += 1
        if Op == 0:
            break
        if Op >> 4 == 1:
            Ordinal = Op & 15
        elif Op >> 4 == 4:
            Fim2 = Dados.index(b"\0", Pos)
            Simbolo = Dados[Pos:Fim2].decode()
            Pos = Fim2 + 1
        elif Op >> 4 == 7:
            Off, Pos = LerUleb(Dados, Pos)
            Binds.append((Op & 15, Off, Simbolo, Ordinal))
        elif Op >> 4 not in (5, 9):
            Fal.Exigir(False, f"opcode de bind desconhecido {Op:#x}")
    Rebases = []
    Pos, Fim = Rebase, Rebase + RebaseTam
    while Pos < Fim and Dados[Pos] != 0:
        Op = Dados[Pos]
        Pos += 1
        if Op >> 4 == 2:
            Off, Pos = LerUleb(Dados, Pos)
            Rebases.append((Op & 15, Off))
    Dt = [S for S in Info["segs"] if S["nome"] == "__DATA"]
    for Seg, Off, _, _ in Binds:
        Fal.Exigir(len(Dt) == 1 and Off + 8 <= Dt[0]["vmsize"], "bind fora do __DATA")
    return Binds, Rebases


def VerificarOrdinais(Info, Binds, Esperados, Fal):
    """Cada bind aponta para um dylib existente (ordinal 1 = libSystem) e, pelo prefixo do simbolo, para o dylib certo quando ele foi ligado."""
    Dylibs = Info.get("dylibs", [])
    for Caminho in Esperados:
        Fal.Exigir(Caminho in Dylibs, f"dylib esperado ausente: {Caminho}")
    for _, _, Simbolo, Ordinal in Binds:
        Fal.Exigir(1 <= Ordinal <= len(Dylibs), f"bind {Simbolo}: ordinal {Ordinal} fora de 1..{len(Dylibs)}")
        if not 1 <= Ordinal <= len(Dylibs):
            continue
        Nome = Simbolo[1:]
        Certo = next((C for P, C in PrefixoDylib if Nome.startswith(P)), None)
        Esperado = Certo if Certo in Dylibs else Dylibs[0]
        Fal.Exigir(Dylibs[Ordinal - 1] == Esperado, f"bind {Simbolo}: dylib {Dylibs[Ordinal - 1]} != {Esperado}")


def DesmontarTexto(Dados, Info, Fal):
    if capstone is None:
        return "capstone ausente: desmontagem pulada"
    Texto = [S for S in Info["secs"] if S["nome"] == "__text"][0]
    Md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    Corpo = Dados[Texto["off"]:Texto["off"] + Texto["size"]]
    Linhas = list(Md.disasm(Corpo, Texto["addr"]))
    Fal.Exigir(len(Linhas) * 4 == len(Corpo), f"__text: {len(Linhas) * 4} de {len(Corpo)} bytes decodificam")
    Fal.Exigir(not [L for L in Linhas if L.mnemonic == "svc"], "__text tem `svc` (o iOS nao emite syscall direto)")
    return [f"{L.mnemonic} {L.op_str}" for L in Linhas]


def VerificarArquivo(Caminho, Oraculo, Esperados=(), Texto=False, Dylibs=()):
    Dados = open(Caminho, "rb").read()
    Fal = Falhas()
    Info = LerMacho(Dados)
    VerificarEstrutura(Dados, Info, Fal, os.path.basename(Caminho))
    Exports = VerificarExports(Dados, Info, Esperados, Fal) if EhDylib(Info) else None
    Assina = VerificarAssinatura(Dados, Info, os.path.basename(Caminho), Fal)
    Binds, Rebases = DecodificarFixups(Dados, Info, Fal)
    VerificarOrdinais(Info, Binds, Dylibs, Fal)
    Disasm = DesmontarTexto(Dados, Info, Fal) if not Oraculo else "(oraculo: desmontagem nao se aplica)"
    print(f"== {Caminho} ({len(Dados)} B){' [ORACULO]' if Oraculo else ''}")
    print("  lcs:", [L[0] for L in Info["lcs"]])
    print("  segs:", [(S["nome"], hex(S["vmaddr"]), S["fileoff"], S["filesize"]) for S in Info["segs"]])
    print("  build:", Info["build"], "entryoff:", hex(Info.get("entryoff", 0)), "binds:", len(Binds), "rebases:", len(Rebases))
    print("  dylibs:", Info.get("dylibs"), "ordinais dos binds:", sorted({(B[3], B[2]) for B in Binds}))
    if Exports is not None:
        print("  dylib:", Info.get("iddylib"), "exports:", Exports)
    print("  assinatura:", Assina)
    if Texto and isinstance(Disasm, list):
        print("  __text:", "; ".join(Disasm))
    for Texto in Fal.Lista:
        print("  FALHA:", Texto)
    return Fal.Lista, Info


def CaminhoDylib(Nome):
    return "/usr/lib/libobjc.A.dylib" if Nome == "objc" else f"/System/Library/Frameworks/{Nome}.framework/{Nome}"


def Principal(Args):
    Oraculo = None
    Esperados = []
    Alvos = []
    Texto = False
    Dylibs = []
    K = 0
    while K < len(Args):
        if Args[K] == "--texto":
            Texto = True
            K += 1
        elif Args[K] == "--dylibs":
            Dylibs = [CaminhoDylib(N) for N in Args[K + 1].split(",") if N]
            K += 2
        elif Args[K] == "--oraculo":
            Oraculo = Args[K + 1]
            K += 2
        elif Args[K] == "--exports":
            Esperados = [N for N in Args[K + 1].split(",") if N]
            K += 2
        else:
            Alvos.append(Args[K])
            K += 1
    Todas = []
    InfoOraculo = None
    if Oraculo:
        FalOra, InfoOraculo = VerificarArquivo(Oraculo, True)
        Todas += FalOra
    for Alvo in Alvos:
        Fal, Info = VerificarArquivo(Alvo, False, Esperados, Texto, Dylibs)
        Todas += Fal
        if InfoOraculo:
            Mesmos = [L[0] for L in Info["lcs"] if L[0] != "SEGMENT_64"] == [L[0] for L in InfoOraculo["lcs"] if L[0] != "SEGMENT_64"]
            print("  load commands (fora os segmentos) iguais em tipo e ordem ao oraculo:", Mesmos)
            Todas += [] if Mesmos else ["LCs diferem do oraculo"]
    print("verificar_macho:", "OK" if not Todas else f"{len(Todas)} falha(s)")
    return 0 if not Todas else 1


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
