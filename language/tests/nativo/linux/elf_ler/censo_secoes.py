# censo_secoes.py - censo das secoes e das relocacoes ELF dos .a do Linux (third\linux\x86_64), lido pelo readelf do WSL alpine-build (M5b P37d).
# Versao: 0.1.0
# Changelog: 0.1.0 - P37d: uma chamada de wsl.exe le os 4 .a; cada secao sai com a classe que o elf_ler.c da (ElfLSecaoDescartada e ElfLTipoSecao) e cada
#                    relocacao com o destino do ligador (ligar_elf.c). Grava censo_secoes.txt ao lado deste script (versionado; so o script escreve).
# Uso: python -I tests\nativo\linux\elf_ler\censo_secoes.py [--saida <arquivo.txt>]
# Depende de: wsl.exe (distro alpine-build com readelf). Sai 0 se o censo foi gravado e nenhuma relocacao ficou fora do censo do leitor; 1 caso contrario.

import argparse
import os
import re
import subprocess
import sys
from collections import Counter

DISTRO = "alpine-build"
PASTA_ESTE = os.path.dirname(os.path.abspath(__file__))
PASTA_LIBS = os.path.normpath(os.path.join(PASTA_ESTE, "..", "..", "..", "..", "third", "linux", "x86_64"))
LIBS = ["zlib/libz.a", "sqlite3/libsqlite3.a", "cjson/libcjson.a", "cgltf/libcgltf.a"]
SAIDA_PADRAO = os.path.join(PASTA_ESTE, "censo_secoes.txt")
PADRAO_FILE = re.compile(r"^File: (.+)$")
PADRAO_SECAO = re.compile(r"^\s*\[\s*(\d+)\](.*)$")
PADRAO_RELSEC = re.compile(r"^Relocation section '(.+?)' at offset")
PADRAO_RELOC = re.compile(r"\b(R_X86_64_\w+)\b")
PADRAO_HEX = re.compile(r"^[0-9a-f]+$")
TIPOS_DESCARTADOS = {"NULL", "SYMTAB", "STRTAB", "RELA", "REL", "NOTE", "GROUP", "X86_64_UNWIND", "SYMTAB_SHNDX"}
# Destino no ligador de cada tipo de relocacao do censo (ElfLTipoNeutro em elf_ler.c e LigarElfAplicar em ligar_elf.c).
DESTINO_RELOC = {
    "R_X86_64_64": "OR_ADDR64", "R_X86_64_PC32": "OR_REL32", "R_X86_64_PLT32": "OR_REL32 (sem PLT)",
    "R_X86_64_32": "OR_ABS32", "R_X86_64_32S": "OR_ABS32S", "R_X86_64_GOTPCREL": "OR_GOTPCREL",
    "R_X86_64_GOTPCRELX": "OR_GOTPCRELX", "R_X86_64_REX_GOTPCRELX": "OR_GOTPCRELX",
    "R_X86_64_TPOFF32": "OR_TPOFF32 (X0111)", "R_X86_64_GOTTPOFF": "OR_GOTTPOFF (X0111)",
}


def CaminhoWsl(Caminho):
    Absoluto = os.path.abspath(Caminho).replace("\\", "/")
    return "/mnt/" + Absoluto[0].lower() + Absoluto[2:]


def RodarReadelf(Caminhos):
    """Um unico wsl.exe para os 4 .a (lote), com timeout."""
    Comando = ["wsl.exe", "-d", DISTRO, "--", "readelf", "-SrW"] + [CaminhoWsl(C) for C in Caminhos]
    Saida = subprocess.run(Comando, capture_output=True, check=False, timeout=300)
    return Saida.stdout.decode("utf-8", errors="replace").replace("\r\n", "\n")


def ParsearSecao(Resto):
    """Uma linha '[Nr] Nome Tipo Addr Off Tam ES Flg Lk Inf Al' (Nome e Flg podem vir vazios). Devolve (nome, tipo, flags)."""
    Corpo = Resto.split()[:-3]  # sem Lk, Inf e Al
    Flags = ""
    if Corpo and not PADRAO_HEX.match(Corpo[-1]):
        Flags = Corpo.pop()
    Corpo.pop()  # ES, sempre presente (hex)
    for _ in range(3):
        Corpo.pop()  # Tam, Off e Addr
    Tipo = Corpo.pop()
    Nome = Corpo[0] if Corpo else ""
    return Nome, Tipo, Flags


def ClasseDaSecao(Nome, Tipo, Flags):
    """Mesma ordem de decisao do elf_ler.c: descarte, FINI_ARRAY (recusa), depois o tipo de OS_*."""
    if Tipo in TIPOS_DESCARTADOS or "A" not in Flags or Nome == ".eh_frame":
        return "descartavel"
    if Tipo == "FINI_ARRAY":
        return "recusa (.fini_array nao tratado)"
    if Tipo == "INIT_ARRAY":
        return "OS_INIT_ARRAY"
    if "T" in Flags:
        return "X0111 (TLS: OS_TDATA/OS_TBSS, recusa so se vivo)"
    if "X" in Flags:
        return "OS_TEXTO"
    if Tipo == "NOBITS":
        return "OS_BSS"
    if "W" in Flags:
        return "OS_DATA"
    return "OS_RDATA"


def DestinoPe(Classe):
    """Saida do ligador para o ELF (LigarSaidaElf). A secao TLS nao tem saida: recusa X0111."""
    Mapa = {"OS_TEXTO": ".text", "OS_RDATA": ".rdata", "OS_DATA": ".data", "OS_BSS": ".bss", "OS_INIT_ARRAY": ".init_array"}
    return Mapa.get(Classe, "-")


def BaseDoNome(Nome):
    Casou = re.match(r"^(\.[A-Za-z_0-9-]+)", Nome)
    if not Casou:
        return Nome or "(sem nome)"
    Base = Casou.group(1)
    return Base if Base == Nome else Base + ".*"


def ParsearLote(Texto):
    """Lista de membros: {arq, membro, secoes: [(nome, tipo, flags)], relocs: Counter por tipo, alvo: {secao alvo: Counter}}."""
    Membros = []
    Atual = None
    Alvo = None
    for Linha in Texto.split("\n"):
        Arquivo = PADRAO_FILE.match(Linha)
        if Arquivo:
            Caminho = Arquivo.group(1)
            Casou = re.match(r"^(.+?)\((.+)\)$", Caminho)
            Arq, Mem = (Casou.group(1), Casou.group(2)) if Casou else (Caminho, Caminho)
            Atual = {"arq": os.path.basename(Arq), "membro": Mem, "secoes": [], "relocs": Counter(), "alvo": {}}
            Membros.append(Atual)
            continue
        if Atual is None:
            continue
        Secao = PADRAO_SECAO.match(Linha)
        if Secao and int(Secao.group(1)) > 0:
            Atual["secoes"].append(ParsearSecao(Secao.group(2)))
            continue
        Rel = PADRAO_RELSEC.match(Linha)
        if Rel:
            Alvo = Rel.group(1)[len(".rela"):] if Rel.group(1).startswith(".rela") else Rel.group(1)
            continue
        for Tipo in PADRAO_RELOC.findall(Linha):
            Atual["relocs"][Tipo] += 1
            Atual["alvo"].setdefault(Alvo, Counter())[Tipo] += 1
    return Membros


def EscreverCenso(Membros, Saida):
    """Relatorio por lib: classe de secao, forma (tipo+flags+base do nome) e relocacao com o destino. Sem linha de dado por membro."""
    Linhas = ["# censo_secoes.txt - gerado por censo_secoes.py 0.1.0 (readelf -SrW no alpine-build). NAO editar a mao.", ""]
    Totais = Counter()
    Formas = Counter()
    Relocs = Counter()
    Tls = []
    for Mem in Membros:
        for Nome, Tipo, Flags in Mem["secoes"]:
            Classe = ClasseDaSecao(Nome, Tipo, Flags)
            Totais[Classe] += 1
            Formas[(Tipo, Flags or "-", BaseDoNome(Nome), Classe, DestinoPe(Classe))] += 1
            if "T" in Flags and Classe.startswith("X0111"):
                Tls.append(f"{Mem['arq']}({Mem['membro']}) {Nome} {Tipo}")
        for Tipo, Quantas in Mem["relocs"].items():
            Relocs[Tipo] += Quantas
    Linhas.append(f"MEMBROS {len(Membros)}")
    Linhas.append("CLASSE DE SECAO (contagem dos 4 .a, secoes vivas ou nao; a descartavel nao entra no Objeto):")
    for Classe, Quantas in sorted(Totais.items()):
        Linhas.append(f"  {Quantas:6d}  {Classe}")
    Linhas.append("FORMA: tipo | flags | base do nome | classe | saida do ligador (PE/ELF)")
    for (Tipo, Flags, Base, Classe, Destino), Quantas in sorted(Formas.items()):
        Linhas.append(f"  {Quantas:6d}  {Tipo:<14}{Flags:<6}{Base:<24}{Classe:<52}{Destino}")
    Linhas.append("RELOCACOES (tipo | contagem | destino no ligador):")
    for Tipo, Quantas in sorted(Relocs.items()):
        Linhas.append(f"  {Quantas:8d}  {Tipo:<24}{DESTINO_RELOC.get(Tipo, 'FORA DO CENSO')}")
    Linhas.append("MEMBROS COM TLS (X0111 se a secao estiver viva):")
    Linhas.extend(f"  {Item}" for Item in Tls)
    with open(Saida, "w", encoding="utf-8", newline="\n") as Arq:
        Arq.write("\n".join(Linhas) + "\n")
    return Totais, Relocs


def Principal(Argumentos):
    Analise = argparse.ArgumentParser(description="Censo das secoes e relocacoes ELF dos .a do Linux.")
    Analise.add_argument("--saida", default=SAIDA_PADRAO)
    Opcoes = Analise.parse_args(Argumentos)
    Caminhos = [os.path.join(PASTA_LIBS, Lib) for Lib in LIBS]
    Membros = ParsearLote(RodarReadelf(Caminhos))
    Totais, Relocs = EscreverCenso(Membros, Opcoes.saida)
    print(f"MEMBROS {len(Membros)} | secoes: " + ", ".join(f"{Chave}={Valor}" for Chave, Valor in sorted(Totais.items())))
    print("RELOCACOES: " + ", ".join(f"{Chave}={Valor}" for Chave, Valor in sorted(Relocs.items())))
    Fora = [T for T in Relocs if T not in DESTINO_RELOC]
    print(f"gravado: {Opcoes.saida}")
    return 1 if Fora else 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
