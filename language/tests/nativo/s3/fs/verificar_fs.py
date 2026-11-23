# -*- coding: utf-8 -*-
"""verificar_fs.py - prova do S3 P6 (D8): sistema de arquivos do std:file (exists, is_file, is_dir, stat, remove_file, rename, copy_file,
make_dir, remove_dir, read_dir) nos 2 backends (C e x64), dev e release, no Windows.

O que prova (cada programa nos 4 pares backend x modo; a saida esperada e calculada AQUI, e o C tem de bater com o x64 byte a byte):
  1. ops.fj: rename e copy_file sobrescrevendo; remove_file de pasta = is_directory e de inexistente = not_found; remove_dir de pasta
     cheia = not_empty; make_dir sem all em pai inexistente = not_found; make_dir(all) cria a cadeia; make_dir de pasta existente =
     already_exists (sem all) e ok (com all); remove_dir(all) apaga arvore de 3 niveis.
  2. consulta.fj: stat (size exato de arquivo de 1234 bytes, kind file/dir, mtime perto de agora), exists/is_file/is_dir; read_dir
     ordenado pelos bytes do nome, com espaco, acento e emoji, sem "." e "..", com subpasta; read_dir de arquivo = not_directory.
  3. juncao.fj: a pasta "pai" tem uma juncao (mklink /J) para a pasta-isca "isca" e um arquivo normal. remove_dir(all) de "pai" remove a
     juncao e a pasta, e a isca e o arquivo dela FICAM INTACTOS (conferido no disco depois). kind da juncao em read_dir = link.
     Se mklink /J falhar, o caso vira SKIP com aviso explicito (nao passa em silencio).

Cada variante (programa x backend x modo) tem a sua pasta de arquivos; as variantes rodam em paralelo e nunca compartilham pasta.

Uso: python -I verificar_fs.py [--forja <forja.exe>] [--jobs N]
Saida: linhas de falha (se houver), avisos SKIP e "RESULTADO OK" (exit 0) ou "RESULTADO FALHOU" (exit 1).
Versao: 0.1.0
Changelog: 0.1.0 - S3 P6: primeira versao (3 programas, 4 pares backend x modo; juncao com SKIP explicito se mklink falhar).
Verificacao: python -I tests\\nativo\\s3\\fs\\verificar_fs.py
"""

import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PASTA_AQUI = Path(__file__).resolve().parent
PASTA_LANGUAGE = PASTA_AQUI.parents[3]
FORJA_PADRAO = PASTA_LANGUAGE / "compiler" / "forja.exe"
BACKENDS = ("c", "x64")
MODOS = ("dev", "release")
TEMPO_LIMITE = 600
TAMANHO_ARQUIVO = 1234
LIMITE_MTIME_NS = 600 * 1000 * 1000 * 1000  # 10 minutos: ordem de grandeza do mtime

# Funcoes comuns aos programas: nome do erro, tipo do item e status de um ?FileError (texto de cada caso, ou "ok").
COMUM = r'''
proc nome_erro(e: file.FileError) -> str {
    if e == file.FileError.not_found {
        return "not_found"
    }
    if e == file.FileError.no_permission {
        return "no_permission"
    }
    if e == file.FileError.other {
        return "other"
    }
    if e == file.FileError.already_exists {
        return "already_exists"
    }
    if e == file.FileError.is_directory {
        return "is_directory"
    }
    if e == file.FileError.not_directory {
        return "not_directory"
    }
    if e == file.FileError.not_empty {
        return "not_empty"
    }
    if e == file.FileError.closed {
        return "closed"
    }
    if e == file.FileError.unsupported {
        return "unsupported"
    }
    return "desconhecido"
}

proc nome_tipo(k: file.FileKind) -> str {
    if k == file.FileKind.file {
        return "file"
    }
    if k == file.FileKind.dir {
        return "dir"
    }
    if k == file.FileKind.link {
        return "link"
    }
    return "outro"
}

proc status(r: ?file.FileError) -> str {
    var texto = "ok"
    switch e in r {
    case file.FileError:
        texto = nome_erro(e)
    case none:
        texto = "ok"
    }
    return texto
}
'''

PROGRAMA_OPS = r'''
import "std:file" as file
''' + COMUM + r'''
proc tamanho_de(caminho: str) -> i64 {
    var tam: i64 = -1
    switch r in file.stat(caminho) {
    case file.FileInfo:
        tam = r.size
    case file.FileError:
        tam = -1
    }
    return tam
}

proc main() {
    let a = "@PASTA@/a.txt"
    let b = "@PASTA@/b.txt"
    let c = "@PASTA@/c.txt"
    let pasta = "@PASTA@/pasta"
    let cheia = "@PASTA@/cheia"
    let arvore = "@PASTA@/arvore"
    let fundo = "@PASTA@/arvore/n1/n2/n3/f.txt"
    let cadeia = "@PASTA@/cadeia/a/b/c"
    _ = file.append_str(a, "aaa")
    _ = file.append_str(b, "bbbbbb")
    let r1 = status(file.rename(a, b))
    let t1 = tamanho_de(b)
    let a1 = file.exists(a)
    print(f"rename={r1}")
    print(f"rename_tam={t1}")
    print(f"rename_origem_existe={a1}")
    _ = file.append_str(c, "ccccc")
    let r2 = status(file.copy_file(b, c))
    let t2 = tamanho_de(c)
    print(f"copy={r2}")
    print(f"copy_tam={t2}")
    let r3 = status(file.remove_file(c))
    let c3 = file.exists(c)
    print(f"remove_arquivo={r3}")
    print(f"remove_arquivo_existe={c3}")
    _ = file.make_dir(pasta)
    let r4 = status(file.remove_file(pasta))
    print(f"remove_pasta_erro={r4}")
    let r5 = status(file.remove_file("@PASTA@/nao_existe.txt"))
    print(f"remove_inexistente={r5}")
    _ = file.make_dir(cheia)
    _ = file.append_str("@PASTA@/cheia/x.txt", "x")
    let r6 = status(file.remove_dir(cheia))
    print(f"remove_dir_cheia={r6}")
    let r7 = status(file.make_dir("@PASTA@/sem_pai/filho"))
    print(f"make_dir_sem_pai={r7}")
    let r8 = status(file.make_dir(cadeia, all = true))
    let c8 = file.is_dir(cadeia)
    print(f"make_dir_cadeia={r8}")
    print(f"make_dir_cadeia_existe={c8}")
    let r9 = status(file.make_dir(pasta))
    print(f"make_dir_existente={r9}")
    let r10 = status(file.make_dir(pasta, all = true))
    print(f"make_dir_all_existente={r10}")
    _ = file.make_dir("@PASTA@/arvore/n1/n2/n3", all = true)
    _ = file.append_str(fundo, "f")
    let r11 = status(file.remove_dir(arvore, all = true))
    let c11 = file.exists(arvore)
    print(f"remove_dir_all={r11}")
    print(f"remove_dir_all_existe={c11}")
}
'''

PROGRAMA_CONSULTA = r'''
import "std:file" as file
import "std:os" as sys
''' + COMUM + r'''
proc mtime_de(caminho: str) -> i64 {
    var m: i64 = 0
    switch r in file.stat(caminho) {
    case file.FileInfo:
        m = r.mtime_ns
    case file.FileError:
        m = 0
    }
    return m
}

proc main() {
    let arq = "@PASTA@/tam.bin"
    let lista = "@PASTA@/lista"
    switch r in file.stat(arq) {
    case file.FileInfo:
        let tipo_arq = nome_tipo(r.kind)
        print(f"tam={r.size}")
        print(f"tipo_arquivo={tipo_arq}")
    case file.FileError:
        print("erro")
    }
    let agora = sys.wall_time_ns()
    let m = mtime_de(arq)
    let dif = agora - m
    let perto = m > 0 and dif < 600000000000 and dif > -600000000000
    print(f"mtime_perto={perto}")
    let e1 = file.exists(arq)
    let f1 = file.is_file(arq)
    let d1 = file.is_dir(arq)
    let d2 = file.is_dir(lista)
    print(f"exists_tam={e1}")
    print(f"is_file={f1}")
    print(f"is_dir_arquivo={d1}")
    print(f"is_dir_lista={d2}")
    switch r in file.read_dir(arq, alloc = heap) {
    case file.DirList:
        print("lista_inesperada")
        delete(r.items)
        delete(r.data)
    case file.FileError:
        let e2 = nome_erro(r)
        print(f"arquivo_nao_dir={e2}")
    }
    switch r in file.read_dir(lista, alloc = heap) {
    case file.DirList:
        for item in r.items {
            let nome = item.name
            let k = nome_tipo(item.kind)
            print(f"{nome}|{k}")
        }
        delete(r.items)
        delete(r.data)
    case file.FileError:
        print("erro")
    }
    print("consulta ok")
}
'''

PROGRAMA_JUNCAO = r'''
import "std:file" as file
''' + COMUM + r'''
proc main() {
    let pai = "@PASTA@/pai"
    let isca = "@PASTA@/isca"
    switch r in file.read_dir(pai, alloc = heap) {
    case file.DirList:
        for item in r.items {
            let nome = item.name
            let k = nome_tipo(item.kind)
            print(f"{nome}|{k}")
        }
        delete(r.items)
        delete(r.data)
    case file.FileError:
        print("erro")
    }
    let r1 = status(file.remove_dir(pai, all = true))
    print(f"remove_dir={r1}")
    let e1 = file.exists(pai)
    print(f"pai_existe={e1}")
    let e2 = file.exists("@PASTA@/isca/arq.txt")
    print(f"arq_intacto={e2}")
    let e3 = file.is_dir(isca)
    print(f"isca_dir={e3}")
}
'''

ESPERADO_OPS = [
    "rename=ok",
    "rename_tam=3",
    "rename_origem_existe=false",
    "copy=ok",
    "copy_tam=3",
    "remove_arquivo=ok",
    "remove_arquivo_existe=false",
    "remove_pasta_erro=is_directory",
    "remove_inexistente=not_found",
    "remove_dir_cheia=not_empty",
    "make_dir_sem_pai=not_found",
    "make_dir_cadeia=ok",
    "make_dir_cadeia_existe=true",
    "make_dir_existente=already_exists",
    "make_dir_all_existente=ok",
    "remove_dir_all=ok",
    "remove_dir_all_existe=false",
]

ESPERADO_CONSULTA = [
    f"tam={TAMANHO_ARQUIVO}",
    "tipo_arquivo=file",
    "mtime_perto=true",
    "exists_tam=true",
    "is_file=true",
    "is_dir_arquivo=false",
    "is_dir_lista=true",
    "arquivo_nao_dir=not_directory",
    "Zeta.txt|file",
    "a.txt|file",
    "b.txt|file",
    "com espaco.txt|file",
    "sub|dir",
    "é_acento.txt|file",
    "\U0001F642.txt|file",
    "consulta ok",
]

ESPERADO_JUNCAO = [
    "junc|link",
    "normal.txt|file",
    "remove_dir=ok",
    "pai_existe=false",
    "arq_intacto=true",
    "isca_dir=true",
]

NOMES_CONSULTA = ["Zeta.txt", "a.txt", "b.txt", "com espaco.txt", "é_acento.txt", "\U0001F642.txt"]


def PrepararConsulta(Area):
    """Arquivo de 1234 bytes com mtime agora, pasta lista/ com 6 arquivos (UTF-8, espaco, emoji) e uma subpasta."""
    (Area / "tam.bin").write_bytes(bytes(TAMANHO_ARQUIVO))
    Lista = Area / "lista"
    Lista.mkdir()
    for Nome in NOMES_CONSULTA:
        (Lista / Nome).write_bytes(b"x")
    (Lista / "sub").mkdir()
    return None


def PrepararJuncao(Area):
    """pai/ com arquivo normal e juncao pai/junc -> isca/ (isca com arq.txt). Devolve o erro do mklink, ou None."""
    Pai = Area / "pai"
    Isca = Area / "isca"
    Pai.mkdir()
    (Pai / "normal.txt").write_bytes(b"n")
    Isca.mkdir()
    (Isca / "arq.txt").write_bytes(b"isca")
    Proc = subprocess.run(["cmd", "/c", "mklink", "/J", str(Pai / "junc"), str(Isca)], capture_output=True, stdin=subprocess.DEVNULL)
    if Proc.returncode != 0:
        return f"mklink /J rc={Proc.returncode} {Proc.stdout.decode('mbcs', 'replace')[:200]}{Proc.stderr.decode('mbcs', 'replace')[:200]}"
    return None


PREPARO = {"ops": None, "consulta": PrepararConsulta, "juncao": PrepararJuncao}
ESPERADOS = {"ops": ESPERADO_OPS, "consulta": ESPERADO_CONSULTA, "juncao": ESPERADO_JUNCAO}


def Construir(Forja, Fonte, Backend, Modo, Saida):
    Comando = [str(Forja), "build", "--backend", Backend]
    if Modo == "release":
        Comando.append("--release")
    Comando += [str(Fonte), "-o", str(Saida)]
    Proc = subprocess.run(Comando, capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE)
    if Proc.returncode != 0 or not Saida.exists():
        return Proc.stdout.decode("utf-8", "replace")[:600] + Proc.stderr.decode("utf-8", "replace")[:600]
    return None


def Rodar(Exe, Cwd):
    Proc = subprocess.run([str(Exe)], capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE, cwd=str(Cwd))
    return Proc.returncode, Proc.stdout.decode("utf-8", "replace"), Proc.stderr.decode("utf-8", "replace")


def Conferir(Rotulo, Saida, Esperado, Falhas):
    """Compara as linhas da saida com as esperadas (linha vazia fora)."""
    Linhas = [L.strip() for L in Saida.splitlines() if L.strip()]
    if Linhas != Esperado:
        Falhas.append(f"FALHA {Rotulo}: saida={Linhas[:24]!r} esperado={Esperado!r}"[:900])
        return False
    return True


def ConferirDisco(Nome, Area):
    """Conferencia independente do programa, no disco: a isca e o arquivo dela sobrevivem a remove_dir(all) da juncao."""
    if Nome != "juncao":
        return None
    Arquivo = Area / "isca" / "arq.txt"
    if not Arquivo.is_file() or Arquivo.read_bytes() != b"isca":
        return "isca/arq.txt nao ficou intacto no disco"
    if (Area / "pai").exists():
        return "a pasta pai ainda existe no disco"
    return None


def Pedidos(Pasta, Fontes, Avisos):
    """Uma tarefa por (programa, backend, modo), cada uma com a sua pasta. Juncao sem mklink vira SKIP com aviso."""
    Itens = []
    for Nome, Texto in Fontes:
        for Backend in BACKENDS:
            for Modo in MODOS:
                Area = Pasta / f"area_{Nome}_{Backend}_{Modo}"
                Area.mkdir()
                Erro = PREPARO[Nome](Area) if PREPARO[Nome] else None
                if Erro is not None:
                    Avisos.append(f"SKIP {Nome} {Backend}/{Modo}: {Erro}"[:600])
                    continue
                Fonte = Pasta / f"{Nome}_{Backend}_{Modo}.fj"
                Fonte.write_text(Texto.replace("@PASTA@", Area.as_posix()), encoding="utf-8")
                Itens.append((Nome, Backend, Modo, Fonte, Area))
    return Itens


def Executar(Forja, Pasta, Item):
    Nome, Backend, Modo, Fonte, Area = Item
    Exe = Pasta / f"{Nome}_{Backend}_{Modo}.exe"
    PastaRun = Pasta / f"run_{Nome}_{Backend}_{Modo}"
    PastaRun.mkdir(exist_ok=True)
    Erro = Construir(Forja, Fonte, Backend, Modo, Exe)
    if Erro is not None:
        return Item, None, Erro
    Rc, Out, Err = Rodar(Exe, PastaRun)
    if Rc != 0:
        return Item, Out, f"rc={Rc} stderr={Err[:300]}"
    return Item, Out, ConferirDisco(Nome, Area)


def SoltarJuncoes(Itens):
    """Tira as juncoes com os.rmdir (so o ponto de reparse; o destino fica) antes do rmtree, que nao deve seguir juncao."""
    for Item in Itens:
        Link = Item[4] / "pai" / "junc"
        if Item[0] == "juncao" and os.path.lexists(Link):
            os.rmdir(Link)


def LimparPasta(Pasta):
    def Forcar(Funcao, Caminho, _Info):
        os.chmod(Caminho, stat.S_IWRITE)
        Funcao(Caminho)

    shutil.rmtree(Pasta, onerror=Forcar)


def Principal():
    Parser = argparse.ArgumentParser()
    Parser.add_argument("--forja", default=str(FORJA_PADRAO))
    Parser.add_argument("--jobs", type=int, default=4)
    Args = Parser.parse_args()
    Forja = Path(Args.forja).resolve()
    Pasta = Path(tempfile.mkdtemp(prefix="verif_fs_"))
    Falhas = []
    Avisos = []
    Itens = []
    try:
        Fontes = [("ops", PROGRAMA_OPS), ("consulta", PROGRAMA_CONSULTA), ("juncao", PROGRAMA_JUNCAO)]
        Itens = Pedidos(Pasta, Fontes, Avisos)
        Ok = 0
        Resultados = {}
        with ThreadPoolExecutor(max_workers=Args.jobs) as Piscina:
            for Item, Saida, Erro in Piscina.map(lambda I: Executar(Forja, Pasta, I), Itens):
                Rotulo = f"{Item[0]} {Item[1]}/{Item[2]}"
                if Erro is not None:
                    Falhas.append(f"FALHA {Rotulo}: {Erro}"[:900])
                    continue
                Resultados.setdefault(Item[0], {})[(Item[1], Item[2])] = Saida
                if Conferir(Rotulo, Saida, ESPERADOS[Item[0]], Falhas):
                    Ok += 1
        for Nome in ESPERADOS:
            Saidas = Resultados.get(Nome, {})
            if len(set(Saidas.values())) > 1:
                Falhas.append(f"FALHA {Nome}: C e x64 (ou dev e release) diferem na saida")
    finally:
        SoltarJuncoes(Itens)
        LimparPasta(Pasta)
    for Linha in Avisos:
        print(Linha)
    for Linha in Falhas:
        print(Linha)
    Total = len(Itens)
    print(f"verificar_fs: {Ok}/{Total} programas conferidos, {len(Avisos)} SKIP")
    if Falhas:
        print("RESULTADO FALHOU")
        return 1
    print("RESULTADO OK")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
