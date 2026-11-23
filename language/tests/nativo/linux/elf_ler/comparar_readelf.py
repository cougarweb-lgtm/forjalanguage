# comparar_readelf.py - compara, membro a membro, a contagem do leitor elf_ler (teste_elf_ler.exe contar) com o readelf do WSL alpine-build (P33, pista L).
# Versao: 0.1.1
# Changelog: 0.1.1 - nomes locais em CamelCase (PY-R01), sem mudar a logica.
#            0.1.0 - P33: secoes = e_shnum (inclui a nula); simbolos = entradas da .symtab (inclui a nula; sem .symtab = 0); relocacoes = soma dos "contains N entries" das secoes RELA/REL.
# Uso: python -I tests\nativo\linux\elf_ler\comparar_readelf.py [--leitor <teste_elf_ler.exe>] <arquivo.a|arquivo.o> ...
#      --leitor padrao: %TEMP%\p33\teste_elf_ler.exe. Sai 0 so com todos os membros iguais; 1 com divergencia.
# Depende de: wsl.exe (distro alpine-build com readelf) e teste_elf_ler.exe compilado.

import os
import re
import subprocess
import sys

DISTRO = "alpine-build"
PADRAO_SECOES = re.compile(r"^\s*Number of section headers:\s+(\d+)", re.M)
PADRAO_SYMTAB = re.compile(r"^Symbol table '\.symtab' contains (\d+) entr", re.M)
PADRAO_RELOC = re.compile(r"^Relocation section .* contains (\d+) entr", re.M)
PADRAO_FILE = re.compile(r"^File: (.+)$", re.M)
PADRAO_LEITOR_E = re.compile(r"^E (.+?): (.*)$")


def CaminhoWsl(Caminho):
    Absoluto = os.path.abspath(Caminho).replace("\\", "/")
    return "/mnt/" + Absoluto[0].lower() + Absoluto[2:]


def RodarReadelf(Caminho):
    Saida = subprocess.run(
        ["wsl.exe", "-d", DISTRO, "--", "readelf", "-hsrW", CaminhoWsl(Caminho)],
        capture_output=True, check=False,
    )
    return Saida.stdout.decode("utf-8", errors="replace").replace("\r\n", "\n")


def DividirBlocos(Texto, Caminho):
    """Um bloco por membro (readelf imprime 'File: x.a(membro.o)' antes de cada um). Arquivo unico: sem linha File, um bloco so."""
    Marcas = list(PADRAO_FILE.finditer(Texto))
    if not Marcas:
        return [(os.path.basename(Caminho), Texto)]
    Blocos = []
    for Indice, Marca in enumerate(Marcas):
        Fim = Marcas[Indice + 1].start() if Indice + 1 < len(Marcas) else len(Texto)
        Dentro = re.search(r"\((.+)\)$", Marca.group(1))
        Nome = Dentro.group(1) if Dentro else os.path.basename(Marca.group(1))
        Blocos.append((Nome, Texto[Marca.end():Fim]))
    return Blocos


def ContarReadelf(Bloco):
    """(secoes, simbolos, relocacoes) como o leitor conta: e_shnum, entradas da symtab (com a nula), soma das entradas RELA/REL."""
    Secoes = PADRAO_SECOES.search(Bloco)
    Simbolos = PADRAO_SYMTAB.search(Bloco)
    Relocacoes = sum(int(Numero) for Numero in PADRAO_RELOC.findall(Bloco))
    return (int(Secoes.group(1)), int(Simbolos.group(1)) if Simbolos else 0, Relocacoes)


def LerLeitor(Leitor, Caminho):
    """Lista (nome, 'M', (secoes, simbolos, relocacoes)) ou (nome, 'E', mensagem), na ordem do arquivo."""
    Saida = subprocess.run([Leitor, "contar", Caminho], capture_output=True, check=False)
    Itens = []
    for Linha in Saida.stdout.decode("utf-8", errors="replace").splitlines():
        if Linha.startswith("M "):
            Partes = Linha.split(" ")
            Itens.append((Partes[1], "M", tuple(int(Valor) for Valor in Partes[2:5])))
            continue
        Erro = PADRAO_LEITOR_E.match(Linha)
        if Erro:
            Itens.append((Erro.group(1), "E", Erro.group(2)))
    return Itens


def CompararArquivo(Leitor, Caminho):
    """Devolve (iguais, total, divergencias) para um arquivo .a ou .o."""
    Leitura = LerLeitor(Leitor, Caminho)
    Blocos = DividirBlocos(RodarReadelf(Caminho), Caminho)
    Divergencias = []
    Iguais = 0
    if len(Leitura) != len(Blocos):
        Divergencias.append(f"quantidade de membros: leitor {len(Leitura)}, readelf {len(Blocos)}")
    for (NomeLeitor, Tipo, Dado), (NomeReadelf, Bloco) in zip(Leitura, Blocos):
        Esperado = ContarReadelf(Bloco)
        if NomeLeitor != NomeReadelf:
            Divergencias.append(f"ordem: leitor {NomeLeitor}, readelf {NomeReadelf}")
        elif Tipo == "E":
            Divergencias.append(f"{NomeLeitor}: leitor recusou ({Dado}); readelf {Esperado}")
        elif Dado == Esperado:
            Iguais += 1
        else:
            Divergencias.append(f"{NomeLeitor}: leitor {Dado} != readelf {Esperado}")
    return Iguais, len(Blocos), Divergencias


def Principal(Argumentos):
    Leitor = os.path.join(os.environ.get("TEMP", "."), "p33", "teste_elf_ler.exe")
    if len(Argumentos) >= 2 and Argumentos[0] == "--leitor":
        Leitor = Argumentos[1]
        Argumentos = Argumentos[2:]
    if not Argumentos:
        print("uso: comparar_readelf.py [--leitor exe] <arquivo.a|arquivo.o> ...")
        return 1
    Falhou = False
    for Caminho in Argumentos:
        Iguais, Total, Divergencias = CompararArquivo(Leitor, Caminho)
        print(f"{os.path.basename(Caminho)}: {Iguais}/{Total} iguais")
        for Item in Divergencias:
            print(f"  DIVERGENCIA {Item}")
        Falhou = Falhou or Iguais != Total
    return 1 if Falhou else 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
