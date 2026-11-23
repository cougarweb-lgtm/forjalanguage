# -*- coding: utf-8 -*-
"""construir_rapido.py -- gera forja.exe, teste_nativo.exe e teste_common.exe como o construir.bat, so que compilando cada .c UMA vez, em paralelo,
com cache de objetos. O construir.bat limpo continua o oficial (ele nao e alterado nem substituido): fecho de cada marco e G3.

Fonte unica das flags e das listas de fontes: o proprio construir.bat. Este script le o .bat (set, for /f do gcc, for %%o, for %%f do (... comum_fontes.rsp),
gcc, objcopy) e executa o mesmo plano: 1) objetos do runtime (rt_win_*.o, rt_linux_*.o, fadec_envelope.o) em paralelo, com os objcopy; 2) todo .c das 3
ligacoes, UNICO por (fonte, flags) -- o COMUM sai uma vez para os tres exes --; 3) as 3 ligacoes em paralelo (a lista de objetos vai por arquivo de respostas).
Linha do .bat que o interprete nao conhece = erro fatal citando a linha (nunca some em silencio).
O texto do caminho passado ao gcc e IDENTICO ao do .bat (COMUM com "/", o resto com "\\"): o __FILE__ grava esse texto no .rdata do objeto.
Cache de objeto por chave = sha256 de (versao do gcc, flags, texto do caminho do fonte, conteudo do fonte, objcopy e os arquivos que ele cita); a validade guarda
o sha de cada dependencia do -MMD (e, para o fonte que embute arquivo por .incbin, o hash de runtime, std, doc, tools\\forja_ligar e dos rt_*.o). O hash do
fonte e das deps e refeito DEPOIS de compilar: se mudou durante o build, o objeto vale so para este build e nao entra no cache. Cache em %TEMP%\\forja_obj_cache.
Uso:
    python construir_rapido.py [--saida <pasta>] [--jobs N] [--cache <pasta>] [--sem-cache]
    python construir_rapido.py --verificar [--oficial <pasta com os exes e .o de um construir.bat limpo>]   (~45 s: com cache x sem cache x oficial)
    python construir_rapido.py --comparar-pe <a.exe> <b.exe>      (mesmas secoes? o carimbo de tempo do PE e o checksum nao contam)
    python construir_rapido.py --selftest
--saida: pasta dos exes e dos rt_*.o (padrao: esta pasta, como o .bat; os -D RTN do .bat sao relativos a compiler\\, entao --saida so serve para teste).
Exit 0 verde, 1 se um gcc falhar (stderr do gcc impresso) ou se --verificar achar diferenca.
Versao: 0.2.0
Changelog: 0.2.0 - construir.bat 0.52.1+: o COMUM vem do laco que gera comum_fontes.rsp (caminhos com "/", como o .bat) e "@comum_fontes.rsp" e expandido nas 3
             ligacoes; GCC_SEM_WIDENING (for /f do gcc >= 16) e avaliado; linha do .bat nao reconhecida = erro fatal; selftest compara a contagem de gcc/objcopy
             e o texto de cada caminho com o .bat (e com o comum_fontes.rsp, se existir); cache refaz o hash depois de compilar e nao grava se mudou; arquivos
             citados no objcopy entram na chave; exes ligados em temporario e trocados por os.replace; --verificar (TimeDateStamp e CheckSum mascarados); tempo por fase.
           0.1.2 - M5b P39-B4: Objcopy aceita a forma de copia "objcopy <opcoes> <origem> <destino>" (rt_linux_curto.o); antes, next() sem default estourava StopIteration.
           0.1.1 - IA1-b09: language\\doc (embutida por .incbin, -DFORJA_DOC_PASTA) entra no hash dos embutidos e na deteccao de fonte com .incbin.
           0.1.0 - IA1-b02v (Emenda 4, via melhor Regra #13, reversivel: apagar este arquivo): primeira versao.
Verificacao: python construir_rapido.py --selftest   |   python construir_rapido.py --verificar
"""

import argparse
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

COMPILADOR = Path(os.path.abspath(Path(__file__).parent))
BAT = COMPILADOR / "construir.bat"
RSP_BAT = COMPILADOR / "comum_fontes.rsp"
CACHE_PADRAO = Path(tempfile.gettempdir()) / "forja_obj_cache"
sys.path.insert(0, str(COMPILADOR.parent / "tests"))
import verificacao_manifesto as VM  # noqa: E402  hash de pastas e versao do gcc (compartilhado com a verificacao enxuta)

RE_VARIAVEL = re.compile(r"!(\w+)(?::([^=!]*)=([^!]*))?!|%(\w+)%")
RE_TOKEN = re.compile(r'"[^"]*"|\S+')
RE_INCBIN = re.compile(r"FORJA_(RT\w*_CAMINHO|STD_PASTA|LIGAR_PASTA|DOC_PASTA|RTN_\w+_CAMINHO)")
RE_FOR_LISTA = re.compile(r"^for %%(\w) in \((.*)\) do (.*)$")
RE_FOR_COMUM = re.compile(r'^for %%(\w) in \((.*)\) do \(set "COMUM_ARQ=%%\1" & echo !COMUM_ARQ:\\=/!>>comum_fontes\.rsp\)$')
RE_FOR_GCC_VERSAO = re.compile(r"""^for /f "tokens=1 delims=\." %%v in \('gcc -dumpfullversion 2\^>nul'\) do if %%v geq (\d+) set (\w+)=(.*)$""")
LINHAS_IGNORADAS = ("@echo off", "chcp ", "setlocal ", "cd /d ", "if errorlevel 1 exit /b 1", "exit /b ", "if exist comum_fontes.rsp del comum_fontes.rsp")
RSP_COMUM = "@comum_fontes.rsp"
OPCOES_SO_DE_LIGACAO = ("-s",)
OFFSET_TIMESTAMP, OFFSET_CHECKSUM = 8, 88  # a partir de e_lfanew: TimeDateStamp e CheckSum do PE


class ErroDeBuild(Exception):
    """gcc ou objcopy falhou, ou o construir.bat tem uma linha que este script nao entende; a mensagem leva o comando/linha e a saida."""


@dataclass
class Passo:
    """Um gcc -c do .bat (objeto do runtime) ou um .c das ligacoes: args sem o -o, fonte, saida (se houver) e objcopy a aplicar depois."""
    Args: list
    Fonte: str
    Saida: str = None
    Objcopy: list = field(default_factory=list)
    Copia: bool = False


@dataclass
class Ligacao:
    Exe: str
    Flags: list
    Fontes: list


@dataclass
class Plano:
    Objetos: list = field(default_factory=list)
    Ligacoes: list = field(default_factory=list)
    Comum: list = field(default_factory=list)
    NGcc: int = 0
    NObjcopy: int = 0


def Expandir(Texto, Vars):
    """Expande !NOME!, !NOME:a=b! e %NOME% do cmd com as variaveis lidas do .bat (desconhecida = vazio)."""
    def Troca(Casa):
        Valor = Vars.get(Casa.group(1) or Casa.group(4), "")
        return Valor.replace(Casa.group(2), Casa.group(3)) if Casa.group(2) is not None else Valor
    return RE_VARIAVEL.sub(Troca, Texto)


def Tokenizar(Linha):
    """Separa em argumentos; aspas do cmd saem (o valor nao tem espaco)."""
    return [Token.strip('"') for Token in RE_TOKEN.findall(Linha)]


def ListarPadrao(Padrao):
    """Arquivos de um padrao do 'for %%f in (...)' do .bat, na ordem do cmd (NTFS: nome em maiusculas), com barras invertidas."""
    Achados = sorted(COMPILADOR.glob(Padrao.replace("\\", "/")), key=lambda Arq: Arq.name.upper())
    return [str(Arq.relative_to(COMPILADOR)).replace("/", "\\") for Arq in Achados]


def VersaoMaiorDoGcc():
    """Numero maior do 'gcc -dumpfullversion' (o mesmo teste do for /f do .bat); 0 se o gcc nao responde."""
    try:
        Texto = subprocess.run(["gcc", "-dumpfullversion"], capture_output=True, timeout=30).stdout.decode("utf-8", "replace").strip()
    except (OSError, subprocess.TimeoutExpired):
        return 0
    Primeiro = Texto.split(".")[0]
    return int(Primeiro) if Primeiro.isdigit() else 0


class Interprete:
    """Le o construir.bat linha a linha e monta o Plano; entende so a sintaxe que o .bat usa e recusa o resto."""

    def __init__(self, Saida):
        self.Saida = Saida
        self.Vars = {}
        self.Plano = Plano()

    def Rodar(self, Linhas):
        Pos = 0
        while Pos < len(Linhas):
            Linha = Linhas[Pos]
            if Linha.endswith("do (") and Linha.startswith("for "):
                Fim = next((I for I in range(Pos + 1, len(Linhas)) if Linhas[I] == ")"), None)
                if Fim is None:
                    raise ErroDeBuild(f"construir.bat: bloco sem ')' que o feche: {Linha[:200]}")
                self.Bloco(Linha, Linhas[Pos + 1:Fim])
                Pos = Fim
            else:
                self.Linha(Linha)
            Pos += 1

    def Bloco(self, Cabecalho, Corpo):
        """for %%o in (a.o b.o) do ( ... ): o corpo roda uma vez por item."""
        Casa = RE_FOR_LISTA.match(Cabecalho)
        for Item in Casa.group(2).split():
            for Linha in Corpo:
                self.Linha(Linha.replace("%%" + Casa.group(1), Item))

    def Linha(self, Linha):
        if Linha.startswith(LINHAS_IGNORADAS):
            return
        Comum, Versao = RE_FOR_COMUM.match(Linha), RE_FOR_GCC_VERSAO.match(Linha)
        if Comum:
            self.DefinirComum(Comum.group(2))
        elif Versao:
            if VersaoMaiorDoGcc() >= int(Versao.group(1)):
                self.Vars[Versao.group(2)] = Versao.group(3)
        elif Linha.startswith("set ") and "=" in Linha and not Linha.startswith("set /"):
            Nome, Valor = Linha[4:].split("=", 1)
            self.Vars[Nome] = Expandir(Valor, self.Vars)
        elif Linha.startswith("gcc "):
            self.Gcc(Tokenizar(Expandir(Linha, self.Vars)))
        elif Linha.startswith("objcopy "):
            self.Objcopy(Tokenizar(Expandir(Linha, self.Vars)))
        else:
            raise ErroDeBuild(f"construir.bat: linha nao reconhecida por construir_rapido.py (ensine o interprete ou use o construir.bat): {Linha[:300]}")

    def DefinirComum(self, Padroes):
        """O laco que gera comum_fontes.rsp: a mesma ordem do cmd, caminhos com '/' (como o .bat grava)."""
        Arquivos = [Arq.replace("\\", "/") for Padrao in Padroes.split() for Arq in ListarPadrao(Padrao)]
        if not Arquivos:
            raise ErroDeBuild(f"construir.bat: os padroes do COMUM nao acharam nenhum .c: {Padroes[:200]}")
        self.Plano.Comum = Arquivos

    def Gcc(self, Tokens):
        if RSP_COMUM in Tokens:
            if not self.Plano.Comum:
                raise ErroDeBuild(f"construir.bat: {RSP_COMUM} usado antes do laco que o gera")
            Pos = Tokens.index(RSP_COMUM)
            Tokens = Tokens[:Pos] + self.Plano.Comum + Tokens[Pos + 1:]
        self.Plano.NGcc += 1
        Pos = Tokens.index("-o")
        Saida, Resto = Tokens[Pos + 1], Tokens[1:Pos] + Tokens[Pos + 2:]
        if "-c" in Tokens:
            Fonte = Tokens[Tokens.index("-c") + 1]
            self.Plano.Objetos.append(Passo(list(Resto), Fonte, Saida))
            return
        self.Plano.Ligacoes.append(Ligacao(Saida, Tokens[1:Pos], Tokens[Pos + 2:]))

    def Objcopy(self, Tokens):
        self.Plano.NObjcopy += 1
        Saida = Tokens[-1]
        Alvo = next((P for P in self.Plano.Objetos if P.Saida == Saida), None)
        if Alvo is None:  # copia "objcopy <opcoes> <origem> <destino>" (rt_linux_curto.o sai de rt_win_curto.o): o destino herda o passo da origem
            Origem = next(P for P in self.Plano.Objetos if P.Saida == Tokens[-2])
            Alvo = Passo(list(Origem.Args), Origem.Fonte, Saida, [list(O) for O in Origem.Objcopy], Copia=True)
            self.Plano.Objetos.append(Alvo)
        Alvo.Objcopy.append(Tokens[1:-2])


def LinhasUteisDoBat():
    """Linhas do .bat sem espaco nas pontas, sem vazias e sem comentarios '::'."""
    Linhas = [L.strip() for L in BAT.read_text(encoding="utf-8").splitlines()]
    return [L for L in Linhas if L and not L.startswith("::")]


def LerPlano(Saida):
    """Plano do construir.bat; Saida recebe os .o do runtime e os exes."""
    Interp = Interprete(Saida)
    Interp.Rodar(LinhasUteisDoBat())
    return Interp.Plano


def FlagsDeCompilacao(Flags):
    """Flags da ligacao sem as que so valem na ligacao (-s, -Wl,...)."""
    return [F for F in Flags if F not in OPCOES_SO_DE_LIGACAO and not F.startswith("-Wl,")]


def LerDeps(Arquivo):
    """Dependencias do -MMD (formato Makefile) como caminhos."""
    Texto = Arquivo.read_text(encoding="utf-8", errors="replace").replace("\\\n", " ").replace("\r", " ")
    return [Item for Item in Texto.split()[1:] if not Item.endswith(":")]


def HashEmbutidos(Saida, Plano):
    """Hash do que o .incbin embute: os rt_*.o finais da Saida mais runtime, std, tools\\forja_ligar e doc."""
    Shas = "".join(VM.Sha256Arquivo(Saida / P.Saida) for P in Plano.Objetos)
    Pastas = [COMPILADOR.parent / "runtime", COMPILADOR.parent / "std", COMPILADOR.parent / "tools" / "forja_ligar", COMPILADOR.parent / "doc"]
    return hashlib.sha256((Shas + VM.HashArvore(Pastas)).encode()).hexdigest()


class CacheObjetos:
    """Objeto compilado por chave; valido enquanto cada dependencia tiver o mesmo sha256 (e o hash dos .incbin, se o fonte os usa)."""

    def __init__(self, Raiz, Usar):
        self.Raiz, self.Usar = Path(Raiz), Usar
        self.Shas = {}
        self.Versao = VM.VersaoGcc()
        self.Incbin = ""
        self.RecalcularIncbin = lambda: ""
        self.Trava = threading.Lock()
        self.Reaproveitados = self.Compilados = self.NaoGravados = 0
        self.Provisorios = []
        self.Raiz.mkdir(parents=True, exist_ok=True)

    def Sha(self, Caminho):
        if Caminho not in self.Shas:
            self.Shas[Caminho] = VM.Sha256Arquivo(Caminho)
        return self.Shas[Caminho]

    def ShaDosCitadosNoObjcopy(self, Objcopy):
        """Arquivo citado como --opcao=arquivo (renomear_mem_linux.txt) muda o objeto: entra na chave."""
        Citados = [COMPILADOR / T.split("=", 1)[1] for Opcoes in Objcopy for T in Opcoes if "=" in T]
        return "".join(self.Sha(C) for C in Citados if C.is_file())

    def Chave(self, Args, Fonte, Objcopy):
        Corpo = "\0".join([self.Versao, " ".join(Args), Fonte, self.Sha(COMPILADOR / Fonte), repr(Objcopy), self.ShaDosCitadosNoObjcopy(Objcopy)])
        return hashlib.sha256(Corpo.encode("utf-8")).hexdigest()[:24]

    def Buscar(self, Chave):
        """Caminho do .o se a entrada ainda vale; senao None."""
        Obj, Meta = self.Raiz / f"{Chave}.o", self.Raiz / f"{Chave}.dep"
        if not self.Usar or not Obj.is_file() or not Meta.is_file():
            return None
        Linhas = Meta.read_text(encoding="utf-8").splitlines()
        Incbin = Linhas[0] == "incbin"
        if Incbin and Linhas[1] != self.Incbin:
            return None
        Pares = [L.rsplit("=", 1) for L in Linhas[2:]]
        return Obj if all(self.Sha(COMPILADOR / Dep) == Sha for Dep, Sha in Pares) else None

    def UsaIncbin(self, Deps):
        """True se o fonte ou algum header seu cita os -D de arquivos embutidos por .incbin (o -MMD nao enxerga esses arquivos)."""
        Textos = (( COMPILADOR / Dep).read_text(encoding="utf-8", errors="replace") for Dep in Deps if Dep.endswith((".c", ".h")))
        return any(RE_INCBIN.search(Texto) for Texto in Textos)

    def MudouDuranteOBuild(self, Arquivos, Inicio, Incbin):
        """Refaz o hash do fonte e das deps DEPOIS do gcc (e o hash dos embutidos, se o fonte os usa): True se algo mudou desde que o build os leu."""
        for Arq in Arquivos:
            Caminho = COMPILADOR / Arq
            Antes = self.Sha(Caminho)
            Agora = VM.Sha256Arquivo(Caminho)
            if Agora != Antes or (Caminho.is_file() and Caminho.stat().st_mtime >= Inicio):
                return True
        return Incbin and self.RecalcularIncbin() != self.Incbin

    def Guardar(self, Chave, Temporario, Fonte, Deps, Inicio):
        """Grava o .o e a validade (dependencias com sha256) se nada mudou durante o build; senao o objeto fica provisorio (vale so para este build)."""
        Incbin = self.UsaIncbin(Deps)
        if self.MudouDuranteOBuild([Fonte, *Deps], Inicio, Incbin):
            with self.Trava:
                self.NaoGravados += 1
                self.Provisorios.append(Temporario)
            return Temporario
        Texto = ["incbin" if Incbin else "puro", self.Incbin] + [f"{Dep}={self.Sha(COMPILADOR / Dep)}" for Dep in Deps]
        Meta = self.Raiz / f"{Chave}.dep"
        MetaTemp = Meta.with_name(f"{Chave}.dep.{os.getpid()}.{threading.get_ident()}")
        MetaTemp.write_text("\n".join(Texto) + "\n", encoding="utf-8")
        os.replace(MetaTemp, Meta)
        os.replace(Temporario, self.Raiz / f"{Chave}.o")
        return self.Raiz / f"{Chave}.o"


def Executar(Comando):
    """Roda um comando no cwd do compilador; levanta ErroDeBuild com a saida se falhar."""
    Cp = subprocess.run(Comando, cwd=COMPILADOR, capture_output=True)
    if Cp.returncode != 0:
        raise ErroDeBuild(f"{' '.join(Comando)}\n{(Cp.stdout + Cp.stderr).decode('utf-8', 'replace')}")


def CompilarPasso(Cache, Passo):
    """Objeto do passo: do cache ou compilado agora (gcc -c -MMD, objcopy). Devolve o caminho do .o."""
    Chave = Cache.Chave(Passo.Args, Passo.Fonte, Passo.Objcopy)
    Pronto = Cache.Buscar(Chave)
    if Pronto:
        with Cache.Trava:
            Cache.Reaproveitados += 1
        return Pronto
    Inicio = time.time()
    Temporario = Cache.Raiz / f"{Chave}.{os.getpid()}.{threading.get_ident()}.o"
    Dep = Temporario.with_suffix(".d")
    Executar(["gcc", *Passo.Args, "-MMD", "-MF", str(Dep), "-o", str(Temporario)])
    for Opcoes in Passo.Objcopy:
        Executar(["objcopy", *Opcoes, str(Temporario), str(Temporario)])
    Final = Cache.Guardar(Chave, Temporario, Passo.Fonte, LerDeps(Dep), Inicio)
    Dep.unlink(missing_ok=True)
    with Cache.Trava:
        Cache.Compilados += 1
    return Final


def EntregarRuntime(Cache, Pool, Plano, Saida):
    """Etapa 1: objetos do runtime (gcc -c do .bat) em paralelo; cada .o final vai para Saida (copia so se mudou)."""
    Objetos = list(Pool.map(lambda P: CompilarPasso(Cache, P), Plano.Objetos))
    for Passo, Obj in zip(Plano.Objetos, Objetos):
        Destino = Saida / Passo.Saida
        if VM.Sha256Arquivo(Destino) != VM.Sha256Arquivo(Obj):
            shutil.copy2(Obj, Destino)
    Cache.RecalcularIncbin = lambda: HashEmbutidos(Saida, Plano)
    Cache.Incbin = Cache.RecalcularIncbin()


def PassosDasLigacoes(Plano):
    """Etapa 2: cada .c das ligacoes, unico por (fonte, flags de compilacao)."""
    Unicos = {}
    for Lig in Plano.Ligacoes:
        Flags = FlagsDeCompilacao(Lig.Flags)
        for Fonte in (F for F in Lig.Fontes if F.endswith(".c")):
            Unicos.setdefault((Fonte, tuple(Flags)), Passo([*Flags, "-c", Fonte], Fonte))
    return Unicos


def Ligar(Ligacao, Objetos, Saida, Rsp):
    """Etapa 3: gcc <flags da ligacao> -o <exe> @<objetos> (a ordem e a do .bat), num temporario trocado por os.replace so se a ligacao deu certo."""
    Itens = [Objetos[(F, tuple(FlagsDeCompilacao(Ligacao.Flags)))] if F.endswith(".c") else Saida / F for F in Ligacao.Fontes]
    Rsp.write_text("\n".join(str(Item).replace("\\", "/") for Item in Itens) + "\n", encoding="utf-8")
    Temporario = Saida / f"{Path(Ligacao.Exe).stem}.novo.exe"
    try:
        Executar(["gcc", *Ligacao.Flags, "-o", str(Temporario), f"@{Rsp.as_posix()}"])
        os.replace(Temporario, Saida / Ligacao.Exe)
    finally:
        Temporario.unlink(missing_ok=True)


def Construir(Saida, Jobs, CacheRaiz, UsarCache):
    """Constroi os 3 exes. Devolve (compilados, reaproveitados, nao gravados no cache, segundos por etapa)."""
    Tempos, Inicio = {}, time.monotonic()
    Saida.mkdir(parents=True, exist_ok=True)
    Plano, Cache = LerPlano(Saida), CacheObjetos(CacheRaiz, UsarCache)
    Pasta = Path(tempfile.mkdtemp(prefix="construir_rapido_"))
    try:
        with ThreadPoolExecutor(max_workers=Jobs) as Pool:
            EntregarRuntime(Cache, Pool, Plano, Saida)
            Tempos["runtime"] = time.monotonic() - Inicio
            Passos = PassosDasLigacoes(Plano)
            Objetos = dict(zip(Passos, Pool.map(lambda P: CompilarPasso(Cache, P), Passos.values())))
            Tempos["fontes"] = time.monotonic() - Inicio - Tempos["runtime"]
            list(Pool.map(lambda L: Ligar(L, Objetos, Saida, Pasta / f"{L.Exe}.rsp"), Plano.Ligacoes))
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
        for Provisorio in Cache.Provisorios:
            Provisorio.unlink(missing_ok=True)
    Tempos["ligacao"] = time.monotonic() - Inicio - Tempos["runtime"] - Tempos["fontes"]
    return Cache.Compilados, Cache.Reaproveitados, Cache.NaoGravados, Tempos


def Relatar(Rotulo, Resultado, Total):
    Compilados, Reaproveitados, NaoGravados, Tempos = Resultado
    print(f"{Rotulo}: compilados={Compilados} reaproveitados={Reaproveitados} fora_do_cache={NaoGravados} runtime={Tempos['runtime']:.1f} s "
          f"fontes={Tempos['fontes']:.1f} s ligacao={Tempos['ligacao']:.1f} s total={Total:.1f} s")


def SecoesDoPe(Caminho):
    """[(nome, tamanho virtual, tamanho bruto, sha256 do conteudo bruto)] das secoes do PE, mais o tamanho do arquivo."""
    Dados = Path(Caminho).read_bytes()
    Pe = struct.unpack_from("<I", Dados, 0x3C)[0]
    Numero, = struct.unpack_from("<H", Dados, Pe + 6)
    Opcional, = struct.unpack_from("<H", Dados, Pe + 20)
    Inicio, Secoes = Pe + 24 + Opcional, []
    for Indice in range(Numero):
        Nome, Virtual, _, Bruto, Ponteiro = struct.unpack_from("<8sIIII", Dados, Inicio + 40 * Indice)
        Secoes.append((Nome.rstrip(b"\0").decode("ascii", "replace"), Virtual, Bruto, hashlib.sha256(Dados[Ponteiro:Ponteiro + Bruto]).hexdigest()))
    return len(Dados), Secoes


def CompararPe(A, B):
    """0 se tamanho e todas as secoes (conteudo) sao iguais; 1 com a lista do que difere. TimeDateStamp e CheckSum nao entram."""
    (TamA, SecA), (TamB, SecB) = SecoesDoPe(A), SecoesDoPe(B)
    Difs = [f"tamanho do arquivo {TamA} x {TamB}"] if TamA != TamB else []
    Difs += [f"secao {Sa[0]} / {Sb[0]}: virtual {Sa[1]} x {Sb[1]}, bruto {Sa[2]} x {Sb[2]}, conteudo {'igual' if Sa[3] == Sb[3] else 'DIFERENTE'}"
             for Sa, Sb in zip(SecA, SecB) if Sa != Sb]
    if len(SecA) != len(SecB):
        Difs.append(f"numero de secoes {len(SecA)} x {len(SecB)}")
    print(f"comparar-pe: tamanho {TamA}/{TamB}, secoes {len(SecA)}/{len(SecB)}: " + ("IGUAIS" if not Difs else "DIFERENTES"))
    for Item in Difs:
        print("  ", Item)
    return 1 if Difs else 0


def ZerarCamposDoPe(Dados):
    """Bytes do PE com TimeDateStamp (e_lfanew+8) e CheckSum (e_lfanew+88) zerados: os unicos campos que variam entre dois builds iguais."""
    Copia = bytearray(Dados)
    Pe = struct.unpack_from("<I", Copia, 0x3C)[0]
    for Offset in (OFFSET_TIMESTAMP, OFFSET_CHECKSUM):
        Copia[Pe + Offset:Pe + Offset + 4] = bytes(4)
    return bytes(Copia)


def DiferencaDeBytes(A, B, Mascarar):
    """None se os arquivos sao iguais byte a byte (PE: com os 2 campos zerados); senao o texto da primeira diferenca."""
    DadosA, DadosB = Path(A).read_bytes(), Path(B).read_bytes()
    if Mascarar:
        DadosA, DadosB = ZerarCamposDoPe(DadosA), ZerarCamposDoPe(DadosB)
    if DadosA == DadosB:
        return None
    if len(DadosA) != len(DadosB):
        return f"tamanho {len(DadosA)} x {len(DadosB)}"
    Posicao = next(I for I, (X, Y) in enumerate(zip(DadosA, DadosB)) if X != Y)
    return f"primeiro byte diferente no offset {Posicao}"


def CompararPastas(Rotulo, PastaA, PastaB, Exes, Objetos):
    """Compara exes (campos do PE mascarados) e .o (byte a byte) de duas pastas; devolve a lista de diferencas."""
    Difs = []
    for Nome, Mascarar in [(N, True) for N in Exes] + [(N, False) for N in Objetos]:
        Motivo = DiferencaDeBytes(PastaA / Nome, PastaB / Nome, Mascarar)
        print(f"verificar {Rotulo}: {Nome}: {'IGUAL' if Motivo is None else 'DIFERENTE (' + Motivo + ')'}")
        if Motivo is not None:
            Difs.append(f"{Rotulo}: {Nome}: {Motivo}")
    return Difs


def Verificar(Jobs, CacheRaiz, Oficial):
    """Build com cache, fotografia, build --sem-cache (ambos no lugar), comparacao; com --oficial compara tambem com os arquivos de um construir.bat limpo."""
    Plano = LerPlano(COMPILADOR)
    Exes, Objetos = [L.Exe for L in Plano.Ligacoes], [P.Saida for P in Plano.Objetos]
    Foto = Path(tempfile.gettempdir()) / "forja_verificar"
    shutil.rmtree(Foto, ignore_errors=True)
    Foto.mkdir(parents=True)
    Inicio = time.monotonic()
    Resultado = Construir(COMPILADOR, Jobs, CacheRaiz, True)
    Relatar("verificar 1/2 (com cache)", Resultado, time.monotonic() - Inicio)
    for Nome in Exes + Objetos:
        shutil.copy2(COMPILADOR / Nome, Foto / Nome)
    Inicio = time.monotonic()
    Resultado = Construir(COMPILADOR, Jobs, CacheRaiz, False)
    Relatar("verificar 2/2 (--sem-cache)", Resultado, time.monotonic() - Inicio)
    Difs = CompararPastas("cache x sem-cache", Foto, COMPILADOR, Exes, Objetos)
    if Oficial:
        Difs += CompararPastas("rapido x oficial", Path(Oficial), COMPILADOR, Exes, Objetos)
    print("verificar: " + ("VERDE" if not Difs else "FALHOU: " + "; ".join(Difs)))
    return 1 if Difs else 0


def ContarDoBat():
    """(gcc, objcopy, caminhos .c) contados direto do texto do .bat, sem passar pelo Interprete (o objcopy de dentro de um for conta uma vez por item)."""
    Gcc = Objcopy = 0
    Fontes, Multiplo = set(), 1
    for Linha in LinhasUteisDoBat():
        Comum = RE_FOR_COMUM.match(Linha)
        if Comum:
            Fontes.update(Arq.replace("\\", "/") for Padrao in Comum.group(2).split() for Arq in ListarPadrao(Padrao))
        elif Linha.startswith("for %%") and Linha.endswith("do ("):
            Multiplo = len(RE_FOR_LISTA.match(Linha).group(2).split())
        elif Linha == ")":
            Multiplo = 1
        elif Linha.startswith("gcc "):
            Gcc += 1
            Fontes.update(T.strip('"') for T in Linha.split() if T.strip('"').endswith(".c"))
        elif Linha.startswith("objcopy "):
            Objcopy += Multiplo
    return Gcc, Objcopy, Fontes


def CasosDoPlano(Plano):
    """Casos do selftest que olham o Plano contra o texto do .bat."""
    BatGcc, BatObjcopy, BatFontes = ContarDoBat()
    Compilados = [P for P in Plano.Objetos if not P.Copia]
    Obtidos = {P.Fonte for P in Compilados} | {F for L in Plano.Ligacoes for F in L.Fontes if F.endswith(".c")}
    Comum = set(Plano.Comum)
    Dev = next((P for P in Plano.Objetos if P.Saida == "rt_win_dev.o"), None)
    Passos = PassosDasLigacoes(Plano)
    Total = sum(len([F for F in L.Fontes if F.endswith(".c")]) for L in Plano.Ligacoes)
    DoRsp = [L.strip() for L in RSP_BAT.read_text(encoding="utf-8").splitlines() if L.strip()] if RSP_BAT.is_file() else Plano.Comum
    return [
        ("ligacoes", sorted(L.Exe for L in Plano.Ligacoes) == ["forja.exe", "teste_common.exe", "teste_nativo.exe"]),
        ("objcopy do runtime", Dev is not None and len(Dev.Objcopy) == 1 and "-DFORJA_DEV" in Dev.Args),
        ("COMUM compilado uma vez", bool(Comum) and len([K for K in Passos if K[0] in Comum]) == len(Comum) and len(Passos) < Total),
        ("COMUM expandido nas 3 ligacoes, na ordem", all(L.Fontes[I:I + len(Plano.Comum)] == Plano.Comum for L in Plano.Ligacoes for I in [L.Fontes.index(Plano.Comum[0])])),
        ("ordem do COMUM = comum_fontes.rsp", Plano.Comum == DoRsp),
        ("contagem de gcc = .bat", Plano.NGcc == BatGcc and len(Compilados) + len(Plano.Ligacoes) == BatGcc),
        ("contagem de objcopy = .bat", Plano.NObjcopy == BatObjcopy),
        ("texto dos caminhos = .bat", Obtidos == BatFontes),
        ("COMUM com '/', resto com '\\'", all("\\" not in F for F in Comum) and all("/" not in F for F in Obtidos - Comum)),
        ("flags do -D com caminho", any(T.startswith("-DFORJA_RT_CAMINHO=") and "\\" not in T for T in Plano.Ligacoes[0].Flags)),
        ("flags sem variavel solta", not any("!" in T or "%" in T for L in Plano.Ligacoes for T in L.Flags)),
    ]


def CasosAuxiliares():
    """Casos do selftest que nao dependem do plano: expandir, linha desconhecida, mascara do PE, comparacao de PE."""
    Pasta = Path(tempfile.mkdtemp(prefix="construir_rapido_st_"))
    try:
        Original, Mexido, Outro = Pasta / "a.exe", Pasta / "b.exe", Pasta / "c.exe"
        Dados = bytearray(Path(sys.executable).read_bytes())
        Pe = struct.unpack_from("<I", Dados, 0x3C)[0]
        Original.write_bytes(Dados)
        Dados[Pe + OFFSET_TIMESTAMP] ^= 0xFF
        Dados[Pe + OFFSET_CHECKSUM] ^= 0xFF
        Mexido.write_bytes(Dados)
        Dados[Pe + OFFSET_CHECKSUM + 8] ^= 0xFF
        Outro.write_bytes(Dados)
        try:
            Interprete(Pasta).Rodar(["comando_que_nao_existe /x"])
            Fatal = False
        except ErroDeBuild:
            Fatal = True
        return [
            ("expandir", Expandir("!A:x=y! %B%", {"A": "axb", "B": "z"}) == "ayb z"),
            ("linha desconhecida do .bat = erro fatal", Fatal),
            ("mascara: so TimeDateStamp e CheckSum", DiferencaDeBytes(Original, Mexido, True) is None and DiferencaDeBytes(Original, Outro, True) is not None),
            ("comparar PE de si mesmo", CompararPe(sys.executable, sys.executable) == 0),
        ]
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def Autoteste():
    """Plano do .bat (3 ligacoes, runtime com objcopy, COMUM unico e na ordem do cmd, contagens e caminhos iguais aos do .bat) e os casos auxiliares."""
    Casos = CasosDoPlano(LerPlano(Path(tempfile.gettempdir()) / "construir_rapido_selftest")) + CasosAuxiliares()
    for Nome, Ok in Casos:
        print(f"selftest {Nome}: {'OK' if Ok else 'FALHOU'}")
    return 0 if all(Ok for _, Ok in Casos) else 1


def ParserCli():
    Parser = argparse.ArgumentParser(description="Constroi forja.exe, teste_nativo.exe e teste_common.exe compilando cada .c uma vez (cache de objetos).")
    Parser.add_argument("--saida", default=str(COMPILADOR), help="pasta dos exes e dos rt_*.o (padrao: a pasta do compilador)")
    Parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4, help="gcc em paralelo")
    Parser.add_argument("--cache", default=str(CACHE_PADRAO), help="pasta do cache de objetos")
    Parser.add_argument("--sem-cache", dest="sem_cache", action="store_true", help="compila tudo (grava no cache, mas nao le)")
    Parser.add_argument("--comparar-pe", dest="comparar_pe", nargs=2, metavar=("A.EXE", "B.EXE"), help="compara tamanho e secoes de dois PE")
    Parser.add_argument("--verificar", action="store_true", help="com cache x sem cache (no lugar), exes com 2 campos do PE mascarados e rt_*.o byte a byte")
    Parser.add_argument("--oficial", help="com --verificar: pasta com os exes e .o de um construir.bat limpo, para comparar tambem")
    Parser.add_argument("--selftest", action="store_true")
    return Parser


def Principal():
    Args = ParserCli().parse_args()
    if Args.selftest:
        return Autoteste()
    if Args.comparar_pe:
        return CompararPe(*Args.comparar_pe)
    Inicio = time.monotonic()
    try:
        if Args.verificar:
            return Verificar(Args.jobs, Args.cache, Args.oficial)
        Resultado = Construir(Path(os.path.abspath(Args.saida)), Args.jobs, Args.cache, not Args.sem_cache)
    except ErroDeBuild as Erro:
        print(f"construir_rapido: FALHOU\n{Erro}", file=sys.stderr)
        return 1
    Relatar("construir_rapido", Resultado, time.monotonic() - Inicio)
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
