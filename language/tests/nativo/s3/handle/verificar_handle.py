# -*- coding: utf-8 -*-
"""verificar_handle.py - prova do S3 P5b-1: handle de arquivo do std:file (File, open, read, write, write_str, seek, tell, flush, close, append_str) nos 2 backends (C e x64), dev e release, no Windows.

O que prova (cada programa nos 4 pares backend x modo; a saida esperada e calculada AQUI, e o C tem de bater com o x64 byte a byte):
  1. dados.fj: ida e volta de 0 B, 1 MiB e 64 MiB + 1 B (padrao de bytes conferido byte a byte no retorno).
  2. posicao.fj: seek start/current/end, tell, leitura no fim (0), append 2 vezes e tamanho final.
  3. erros.fj: caminho vazio (not_found), pasta (is_directory), caminho sob arquivo (not_found), create sobre existente (already_exists),
     escrita em modo read (no_permission), operar depois do close (closed), fechar 2 vezes, 33 handles abertos (o 33o falha limpo: other).

Uso: python -I verificar_handle.py [--forja <forja.exe>] [--jobs N]
Saida: linhas de falha (se houver) e "RESULTADO OK" (exit 0) ou "RESULTADO FALHOU" (exit 1).
Versao: 0.2.1
Changelog: 0.2.1 - S3 P5b-1: 12/12 programas (4 pares backend x modo) ok. Esperado de dados em minusculas (a Forja imprime true/false); dados.fj com let (erro nao muda); erros.fj sem f-string fora de print (paths viram literais com @PASTA@) e close(h) sem & (h ja e ^File no for &h).
           0.2.0 - S3 P5b-1: cada variante (programa x backend x modo) ganha a sua pasta de arquivos (as 4 rodavam em paralelo e disputavam o mesmo s.bin: 'posicao erro' por violacao de compartilhamento, nao bug do backend); os 3 programas usam make(alloc = heap) (era em = heap, E0110) e erros.fj guarda cada f"..." numa variavel (aspas dentro de {...} e E0001).
           0.1.0 - S3 P5b-1: primeira versao (3 programas, 4 pares backend x modo).
Verificacao: python -I tests\\nativo\\s3\\handle\\verificar_handle.py
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
MIB = 1024 * 1024
TAMANHO_GRANDE = 64 * MIB + 1

PROGRAMA_DADOS = r'''
import "std:file" as file

proc preencher(dados: []u8) {
    for i in 0..<len(dados) {
        dados[i] = (i % 251) as u8
    }
}

proc conferir(dados: []u8) -> bool {
    for i in 0..<len(dados) {
        if dados[i] != (i % 251) as u8 {
            return false
        }
    }
    return true
}

proc gravar(caminho: str, dados: []u8) -> bool {
    var ok = false
    switch r in file.open(caminho, mode = file.OpenMode.write) {
    case file.File:
        var f = r
        let erro: ?file.FileError = file.write(&f, dados)
        ok = erro == none
        file.close(&f)
    case file.FileError:
        ok = false
    }
    return ok
}

proc ler_tudo(caminho: str, recebido: []u8) -> int {
    var total = 0
    switch r in file.open(caminho, mode = file.OpenMode.read) {
    case file.File:
        var f = r
        var lendo = true
        while lendo {
            switch n in file.read(&f, into = recebido[total..<len(recebido)]) {
            case int:
                if n == 0 {
                    lendo = false
                } else {
                    total += n
                }
            case file.FileError:
                total = -1
                lendo = false
            }
        }
        file.close(&f)
    case file.FileError:
        total = -1
    }
    return total
}

proc ida_e_volta(caminho: str, tamanho: int) {
    var dados = make([dynamic]u8, alloc = heap, len = tamanho)
    defer delete(dados)
    var recebido = make([dynamic]u8, alloc = heap, len = tamanho)
    defer delete(recebido)
    preencher(dados[..])
    let gravou = gravar(caminho, dados[..])
    let lidos = ler_tudo(caminho, recebido[..])
    let igual = conferir(recebido[..])
    print(f"tam={tamanho} gravou={gravou} lidos={lidos} ok={igual}")
}

proc main() {
    ida_e_volta("@PASTA@/d0.bin", 0)
    ida_e_volta("@PASTA@/d1.bin", 1048576)
    ida_e_volta("@PASTA@/d2.bin", 67108865)
}
'''

PROGRAMA_POSICAO = r'''
import "std:file" as file

proc sequencia(caminho: str) -> int ! file.FileError {
    var g = try file.open(caminho, mode = file.OpenMode.create)
    _ = file.write_str(&g, "0123456789")
    file.close(&g)
    var h = try file.open(caminho, mode = file.OpenMode.read)
    let p = try file.seek(&h, 2, from = file.SeekFrom.start)
    print(f"seek_start={p}")
    var bloco: [3]u8 = {0, 0, 0}
    let n = try file.read(&h, into = bloco[..])
    print(f"lido={n} {bloco[0]} {bloco[1]} {bloco[2]}")
    let t = try file.tell(&h)
    print(f"tell={t}")
    let fim = try file.seek(&h, -2, from = file.SeekFrom.end)
    print(f"seek_end={fim}")
    var d: [4]u8 = {0, 0, 0, 0}
    let m = try file.read(&h, into = d[..])
    print(f"fim_lido={m} {d[0]} {d[1]}")
    let zero = try file.read(&h, into = d[..])
    print(f"eof={zero}")
    file.close(&h)
    _ = file.append_str(caminho, "AB")
    _ = file.append_str(caminho, "CD")
    var k = try file.open(caminho, mode = file.OpenMode.read)
    let tamanho = try file.seek(&k, 0, from = file.SeekFrom.end)
    file.close(&k)
    print(f"tamanho={tamanho}")
    return 0
}

proc main() {
    switch r in sequencia("@PASTA@/s.bin") {
    case int:
        print("posicao ok")
    case file.FileError:
        print("posicao erro")
    }
}
'''

PROGRAMA_ERROS = r'''
import "std:file" as file

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

proc abrir_nome(caminho: str, modo: file.OpenMode) -> str {
    var resultado = "ok"
    switch r in file.open(caminho, mode = modo) {
    case file.File:
        var f = r
        file.close(&f)
    case file.FileError:
        resultado = nome_erro(r)
    }
    return resultado
}

proc gravar_em_leitura(caminho: str) -> str {
    var resultado = "ok"
    var um: [2]u8 = {1, 2}
    switch r in file.open(caminho, mode = file.OpenMode.read) {
    case file.File:
        var f = r
        switch e in file.write(&f, um[..]) {
        case file.FileError:
            resultado = nome_erro(e)
        case none:
            resultado = "ok"
        }
        file.close(&f)
    case file.FileError:
        resultado = nome_erro(r)
    }
    return resultado
}

proc depois_do_close(caminho: str) -> str {
    var resultado = "ok"
    var f = file.File{id = 0}
    switch r in file.open(caminho, mode = file.OpenMode.read) {
    case file.File:
        f = r
    case file.FileError:
        resultado = nome_erro(r)
    }
    file.close(&f)
    file.close(&f)
    var um: [2]u8 = {1, 2}
    switch e in file.write(&f, um[..]) {
    case file.FileError:
        resultado = nome_erro(e)
    case none:
        resultado = "ok"
    }
    return resultado
}

proc muitos_abertos(caminho: str) -> str {
    var abertos = make([dynamic]file.File, alloc = heap, cap = 40)
    defer delete(abertos)
    var ultimo = "ok"
    for i in 0..<33 {
        switch r in file.open(caminho, mode = file.OpenMode.read) {
        case file.File:
            append(&abertos, item = r)
        case file.FileError:
            ultimo = nome_erro(r)
        }
    }
    for &h in abertos {
        file.close(h)
    }
    print(f"abertos={len(abertos)}")
    return ultimo
}

proc main() {
    let vazio = abrir_nome("", file.OpenMode.read)
    print(f"vazio={vazio}")
    let pasta_lida = abrir_nome("@PASTA@/pasta", file.OpenMode.read)
    print(f"pasta={pasta_lida}")
    let pasta_escrita = abrir_nome("@PASTA@/pasta", file.OpenMode.write)
    print(f"pasta_escrita={pasta_escrita}")
    let sob_arquivo = abrir_nome("@PASTA@/existe.bin/x.bin", file.OpenMode.read)
    print(f"sob_arquivo={sob_arquivo}")
    let sem_pasta = abrir_nome("@PASTA@/nao_existe/x.bin", file.OpenMode.read)
    print(f"sem_pasta={sem_pasta}")
    let criado = abrir_nome("@PASTA@/existe.bin", file.OpenMode.create)
    print(f"create_existente={criado}")
    let leitura = gravar_em_leitura("@PASTA@/existe.bin")
    print(f"escrita_em_leitura={leitura}")
    let fechado = depois_do_close("@PASTA@/existe.bin")
    print(f"depois_do_close={fechado}")
    let muitos = muitos_abertos("@PASTA@/existe.bin")
    print(f"muitos={muitos}")
}
'''


def Padrao(Tamanho):
    """Bytes 0..250 repetidos (i % 251), o mesmo padrao do fonte."""
    return bytes((I % 251) for I in range(Tamanho))


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


def EsperadoDados(Pasta):
    return [
        "tam=0 gravou=true lidos=0 ok=true",
        f"tam={MIB} gravou=true lidos={MIB} ok=true",
        f"tam={TAMANHO_GRANDE} gravou=true lidos={TAMANHO_GRANDE} ok=true",
    ]


ESPERADO_POSICAO = [
    "seek_start=2",
    "lido=3 50 51 52",
    "tell=5",
    "seek_end=8",
    "fim_lido=2 56 57",
    "eof=0",
    "tamanho=14",
    "posicao ok",
]

ESPERADO_ERROS = [
    "vazio=not_found",
    "pasta=is_directory",
    "pasta_escrita=is_directory",
    "sob_arquivo=not_found",
    "sem_pasta=not_found",
    "create_existente=already_exists",
    "escrita_em_leitura=no_permission",
    "depois_do_close=closed",
    "abertos=32",
    "muitos=other",
]


def PrepararPasta(Pasta):
    (Pasta / "existe.bin").write_bytes(b"12345")
    (Pasta / "pasta").mkdir(exist_ok=True)


def LimparPasta(Pasta):
    def Forcar(Funcao, Caminho, _Info):
        os.chmod(Caminho, stat.S_IWRITE)
        Funcao(Caminho)

    shutil.rmtree(Pasta, onerror=Forcar)


def Conferir(Rotulo, Saida, Esperado, Falhas):
    """Compara as linhas da saida com as esperadas (a linha tam=... sai na ordem)."""
    Linhas = [L.strip() for L in Saida.splitlines() if L.strip()]
    if Linhas != Esperado:
        Falhas.append(f"FALHA {Rotulo}: saida={Linhas[:20]!r} esperado={Esperado!r}"[:800])
        return False
    return True


def Pedidos(Pasta, Fontes):
    """Uma tarefa por (programa, backend, modo), cada uma com a sua pasta de arquivos (as variantes rodam em paralelo e disputavam o mesmo s.bin)."""
    Saidas = []
    for Nome, Texto in Fontes:
        for Backend in BACKENDS:
            for Modo in MODOS:
                Area = Pasta / f"area_{Nome}_{Backend}_{Modo}"
                Area.mkdir()
                PrepararPasta(Area)
                Fonte = Pasta / f"{Nome}_{Backend}_{Modo}.fj"
                Fonte.write_text(Texto.replace("@PASTA@", Area.as_posix()), encoding="utf-8")
                Saidas.append((Nome, Backend, Modo, Fonte))
    return Saidas


def Executar(Forja, Pasta, Item):
    Nome, Backend, Modo, Fonte = Item
    Exe = Pasta / f"{Nome}_{Backend}_{Modo}.exe"
    PastaRun = Pasta / f"run_{Nome}_{Backend}_{Modo}"
    PastaRun.mkdir(exist_ok=True)
    Erro = Construir(Forja, Fonte, Backend, Modo, Exe)
    if Erro is not None:
        return Item, None, Erro
    Rc, Out, Err = Rodar(Exe, PastaRun)
    if Rc != 0:
        return Item, Out, f"rc={Rc} stderr={Err[:300]}"
    return Item, Out, None


def Principal():
    Parser = argparse.ArgumentParser()
    Parser.add_argument("--forja", default=str(FORJA_PADRAO))
    Parser.add_argument("--jobs", type=int, default=4)
    Args = Parser.parse_args()
    Forja = Path(Args.forja).resolve()
    Pasta = Path(tempfile.mkdtemp(prefix="verif_handle_"))
    Falhas = []
    try:
        Fontes = [("dados", PROGRAMA_DADOS), ("posicao", PROGRAMA_POSICAO), ("erros", PROGRAMA_ERROS)]
        Itens = Pedidos(Pasta, Fontes)
        Ok = 0
        Esperados = {"dados": EsperadoDados(Pasta), "posicao": ESPERADO_POSICAO, "erros": ESPERADO_ERROS}
        Resultados = {}
        with ThreadPoolExecutor(max_workers=Args.jobs) as Piscina:
            for Item, Saida, Erro in Piscina.map(lambda I: Executar(Forja, Pasta, I), Itens):
                Rotulo = f"{Item[0]} {Item[1]}/{Item[2]}"
                if Erro is not None:
                    Falhas.append(f"FALHA {Rotulo}: {Erro}"[:800])
                    continue
                Resultados.setdefault(Item[0], {})[(Item[1], Item[2])] = Saida
                if Conferir(Rotulo, Saida, Esperados[Item[0]], Falhas):
                    Ok += 1
        for Nome in Esperados:
            Saidas = Resultados.get(Nome, {})
            if len(set(Saidas.values())) > 1:
                Falhas.append(f"FALHA {Nome}: C e x64 (ou dev e release) diferem na saida")
    finally:
        LimparPasta(Pasta)
    for Linha in Falhas:
        print(Linha)
    Total = len(Itens)
    print(f"verificar_handle: {Ok}/{Total} programas conferidos")
    if Falhas:
        print("RESULTADO FALHOU")
        return 1
    print("RESULTADO OK")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
