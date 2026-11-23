# -*- coding: utf-8 -*-
"""forja ligar: le um header C (.h) pela libclang e gera o modulo .fj (bloco foreign, tipos e consts) da linguagem Forja.

Objetivo : ligar uma lib C madura a Forja sem digitar prototipo a mao. Recusa por simbolo o que a Forja nao liga com seguranca
           e diz o motivo em JSON no stderr (uma linha por evento, codigos G do catalogo do 'forja explicar').
Versao   : 0.10.0
Uso  : forja ligar <header.h> --link <lib> [-o saida.fj] [--prefixo p] [-I pasta] [-D macro] [--estrito] [--propor] [--simbolos lista|arquivo]
           forja ligar <header.h> --link <lib> -o saida.fj --cola [--macros x.sig] [--compilar-cola]   (cola C: ver --ajuda)
           forja ligar --ajuda                                                (formato do .sig de macros e da cola)
           forja ligar <header.h> --link <lib> -o saida.fj --wrapper-variadicas   (gera tambem <saida>_variadicas.c)
           forja ligar --codigos
           forja ligar --cpp <x.hpp> --link <lib> -o saida.fj   (gera tambem <saida>_c.cpp, o wrapper extern "C")
           forja ligar --dll <x.dll> --assinaturas <x.sig> [-o saida.fj]   (DLL sem header: assinaturas na sintaxe do foreign)
           forja ligar --dll <x.dll> --listar                              (nomes exportados, um por linha)
Verificar: python ..\\..\\tests\\ligar\\verificar_ligar.py --forja ..\\..\\compiler\\forja.exe
Depende de: libclang (pip; modos header e --cpp), pefile (pip; modo --dll), gcc/g++ do MinGW no PATH (so para descobrir as
           pastas de include).
Reversao : voltar ao script 0.1.1 (copia do prototipo) e reconstruir o forja.exe (construir.bat embute este arquivo por .incbin).

Changelog
 0.10.0 Forja 0.45.0 (windows.h): (1) o modulo gerado passa no forja check: campo [N]c_long/c_ulong sai como array do inteiro do tamanho do long do host
        (i32/u32 no Windows; antes E0342), o tipo de callback proc(...) usa o mesmo inteiro (E0342), o GNU `T x[0]` e tratado como membro flexivel
        (antes E0308) e o acessor de union para ponteiro de long (^?^c_long) nao e gerado; (2) --simbolos <a,b,c | arquivo>: so os procs, variaveis,
        macros e constantes de enum com esses nomes C; struct e enum so nascem por referencia dos pedidos. Sem --simbolos a saida e a da 0.9.0 (mesma, so o carimbo).
 0.9.0  M6d P92g: --propor grava `ends` (D-M6d-10) no proc com ZERO parametros cujo nome sem o prefixo e exatamente shutdown, global_cleanup
        ou library_end (lista literal FIM_NOMES; sqlite3_os_end nao: marcaria a lib encerrada sem soltar o cache). Sem --propor a saida e a da 0.8.1.
 0.8.1  M6c P91b: --propor troca '?^' por '^' so no parametro de saida de nome inteiro (rdb: ?^T nao e mais alterado por 'creates db').
 0.8.0  M6c P89: --propor (so modo C, por arquivo, nunca padrao) grava as clausulas creates / creates p / destroys p do D-M6c-9 nos procs que
        casam a heuristica fechada (CRIA_NOMES e DESTROI_NOMES, sem diferenciar caixa, prefixo --prefixo ignorado), com a linha
        '// proposto por forja ligar --propor: confira' antes do proc. creates: retorno ^T/?^T/rawptr (cstr nao: strdup devolve texto) ou parametro
        de saida ?^?^T em proc com nome de criar; destroys p: 1o parametro ^T/?^T/rawptr em proc com nome de destruir (vence quando o nome tem os
        dois, ex. unload). Decisao: o destroys exige o 1o parametro ponteiro/handle (cstr, inteiro, struct por valor e callback ficam sem proposta);
        falso positivo do tipo PQclosePrepared (1o parametro handle, mas nao destroi) NAO e filtrado: o comentario manda o humano conferir.
        'creates p' exige p do tipo ^H (E0912 recusa ?^?^T): so nesse caso o ligar troca o parametro de saida de ?^?^T para ^?^T (T** de saida
        nunca e nulo). Nunca propoe takes, shutdown nem cleanup. Sem --propor a saida e a da 0.7.1 (so muda o carimbo da versao).
 0.7.1  M6c P84 (defeito achado pelo main): struct com c_long/c_ulong (direto, em array ou por struct aninhada por valor) NAO ganha
        static_assert (o tamanho segue o alvo: 72 no Windows, 80 no Linux, em GzHeaderS); o comentario '// layout' fica para a bateria.
 0.7.0  M6c P84: long/unsigned long do C saem como c_long/c_ulong (parametro, retorno, campo, ponteiro de saida); antes i32/u32, que
        davam tamanho errado no Linux (LP64). TAMANHOS ganha c_long/c_ulong (tamanho do host, ctypes). Bitfield de long segue G0006
        (o acessador e pub proc, e c_long nao sai em proc Forja: E0342). Membro de union e flexivel de long ficam sem acessor
        (sem recusa, como o membro que nao mapeia). ulonglong/longlong seguem i64/u64.
 0.6.2  IA1-L3c: o codigo .fj gerado (acessores de bitfield) usa a forma nova transmute(x, to = T) no lugar de para = T; o resto da saida e igual.
 0.6.1  Interop C F1f: parametro C declarado como array (T x[N], const T x[N], T x[]) decai em ponteiro, como no C: sai ?^T
        (const char -> cstr, char -> ?^i8), igual ao parametro ponteiro; antes G0001. Array de ponteiro/array segue G0001.
 0.6.0  Interop C F1e (cola C): --cola gera <lib>_cola.c com um wrapper nao-inline fjcola_<nome> para cada funcao static/inline (G0005) e
        fjcola_get_<nome>/fjcola_set_<nome> para cada variavel global (set so se nao for const; array global so ganha o get, que devolve
        o ponteiro do 1o elemento); a ponte declara o wrapper com alias do nome C (proc nome = "fjcola_nome"(...); variavel: <nome>_get e
        <nome>_set) e ganha o link "<lib>_cola". --macros x.sig (G0004): o usuario da a assinatura da macro-funcao, o ligar gera o wrapper em C;
        sem entrada no .sig a macro segue recusada. --compilar-cola roda 'gcc -O2 -c' + 'ar rcs lib<lib>_cola.a' na hora (erro do gcc vira G0103
        com o texto do gcc dentro da mensagem). --ajuda. Sem --cola a saida e a mesma da 0.5.0 (so muda o numero da versao).
        Arquivo unico de proposito: o forja.exe embute o ligar por UM arquivo (.incbin em driver/ligar.c, gravado como _ligar.py no %TEMP%),
        entao um modulo ao lado nao seria achado; divida de tamanho (acima de 800 linhas) registrada no relato.
 0.5.0  Interop C F1 (2a parte): curl.h abre (libclang falhava com G0103 'definition of builtin function __rdtsc' lendo ia32intrin.h do gcc;
        a 2a tentativa poe antes dos -I uma pasta com os *intrin*.h vazios, so quando esse erro aparece; os demais headers nao mudam).
 0.4.0 Interop C F1b/F1d (plano_interop_c.txt): ponteiro de funcao em CAMPO de struct, em RETORNO e aninhado vira rawptr (antes G0003 e a
        struct caia em G0019 em cascata; ?proc so chega na v1.13); struct/union com nome que falha por outro motivo (bitfield, layout, tipo
        recusado...) vira 'opaque' (cabe atras de ponteiro, sem cascata; por valor continua G0019); tipo recursivo atraves de ponteiro
        (layout ainda desconhecido) vira G0007 em vez de KeyError; cada recusa vira o comentario '// recusado NOME Gxxxx causa' no fim da ponte.
 0.3.1  Linguagem v1.11 (achado do main ao fechar o P19a, teste ligar 13/14): RESERVADAS ganha todos os procs embutidos do compilador
        (semantic.c RegistrarEmbutido: print, assert, panic, transmute, len, expect, new, make, append, delete, frame_reset, format,
        format_c, slice_from, size_of, align_of, offset_of, static_assert, min, max, abs, clamp, sqrt, conj). math.h gerava
        'foreign proc sqrt' e a 0.18.1 recusa com E0202; agora sai 'proc c_sqrt = "sqrt"(...)'.
 0.3.0  Linguagem v1.10 (achados do laboratorio de libs 3D): A6 nome C igual a palavra da Forja (remove, get, has, insert...) vira
        alias 'proc c_remove = "remove"(...)'; A7 macro com expressao constante (casts, ~, sizeof, PI/180.0f) e avaliada pela
        libclang (clang_Cursor_Evaluate) e vira const; A8 #define repetido com o mesmo valor gera UMA const (valor diferente: G0013);
        A9 acessador de union que colide com outro nome ganha o nome <union>__campo_<membro>; A11 --wrapper-variadicas gera
        <saida>_variadicas.c (funcao de parametros fixos que repassa o texto por "%s") e a linha link do wrapper.
 0.2.1  Linguagem v1.9 P12: RESERVADAS com os 6 embutidos da v1.9 (insert, get, get_ptr, has, remove, transpose).
 0.2.0  Linguagem v1.8b P8c: modo --dll. Exportacoes lidas do PE (pefile) + arquivo de assinaturas (opaque/proc); G0025 (nome que
        a DLL nao exporta), G0026 (linha invalida), G0105 (pefile ausente ou DLL invalida); --listar.
 0.2.0  Linguagem v1.8b P8b: modo --cpp. Classe (1 construtor publico, destrutor, metodos publicos) e funcao livre de namespace
        viram opaque + procs <ns>_<Classe>_new/_delete/_<metodo>; o wrapper extern "C" com try/catch sai em <saida>_c.cpp e a
        excecao C++ vira R0010 (exit 101). Recusas G0021, G0022, G0023.
 0.2.0  Linguagem v1.8b P8a: modo C reescrito. Tipo opaco (struct incompleta atras de ponteiro), enum com valor, struct aninhada e
        array em campo, union como armazenamento + acessos, callback (proc(...) em parametro), retorno char * como ?cstr, #define
        e enum anonimo como const, recusas G0001..G0023 (catalogo do diag.c), erros de ferramenta G0101..G0105, --codigos.
 0.1.1  Linguagem v1.8b P7: copia do prototipo 0.1.0 para o projeto, embutido no forja.exe por .incbin (driver\\ligar.c).
 0.1.0  prototipo (C:\\CLAUDE_AGI\\tools\\forja_ligar).
"""
import argparse
import atexit
import ctypes
import glob
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass, field
from decimal import Decimal

VERSAO = "0.10.0"
Ci = None  # modulo clang.cindex: preenchido por CarregarLibclang (so existe depois de importado de verdade)

# Texto do catalogo (forma_certa) de cada codigo G, na ordem do diag.h 0.12.0. Mudou la, muda aqui.
CODIGOS = {
    "G0001": "Escreva um wrapper C com tipos simples (double, int64_t) e gere o modulo dele.",
    "G0003": "Guarde o ponteiro de funcao no lado C (wrapper que recebe o callback por parametro) ou use rawptr no campo.",
    "G0004": "Escreva a const a mao no .fj (pub const NOME: i32 = ...) ou um proc Forja que faz a conta da macro.",
    "G0005": "Exponha a variavel por funcoes get/set num wrapper C; funcao inline: escreva o equivalente em Forja ou um wrapper C "
             "nao inline.",
    "G0006": "Passe a struct por ponteiro opaco e leia os campos por funcoes C.",
    "G0007": "Use a struct por ponteiro (opaque) e acesse por funcoes C.",
    "G0008": 'Escreva uma funcao C de parametros fixos que chama a variadica (ex.: log_texto(const char *s) chamando log("%s", s)).',
    "G0013": 'Use --prefixo para filtrar ou declare o proc a mao com alias do simbolo C: proc c_remove = "remove"(caminho: cstr) '
             '-> i32 (o forja ligar gera o alias sozinho com prefixo).',
    "G0014": "De nome ao tipo de dentro no header (typedef) ou use a struct por ponteiro.",
    "G0016": "O apelido sai do modulo (use o membro original); valor fora de i32: declare a const a mao com i64 ou u32.",
    "G0017": "Passe por ponteiro: struct incompleta atras de ponteiro vira opaque.",
    "G0019": "Resolva a recusa do tipo (wrapper C) e gere de novo; o simbolo volta sozinho.",
    "G0021": "Declare no header uma funcao C++ de nome unico que chama a sobrecarga desejada.",
    "G0022": "Escreva uma funcao ou classe concreta (sem template) que usa a instancia desejada.",
    "G0023": "Exponha funcoes livres e classes com metodos de tipos C (int32_t, double, const char *, ponteiro).",
    "G0025": "Confira a grafia (a caixa vale) com 'forja ligar --dll x.dll --listar'.",
    "G0026": "opaque Janela  -  proc criar_janela(largura: i32) -> ?^Janela  (um item por linha, sintaxe do foreign)",
    "G0101": "Instale o Python 3.10+ (python no PATH) ou aponte FORJA_PYTHON para o python.exe.",
    "G0102": "python -m pip install --user libclang",
    "G0103": "Confira o caminho, passe -I <pasta> para os includes e -D NOME para as macros que o header exige.",
    "G0104": "forja ligar <header.h> --link <lib> [-o saida.fj] [--prefixo p] [-I pasta] [-D macro] [--estrito]  -  "
             "forja ligar --cpp <x.hpp> --link <lib> -o saida.fj  -  forja ligar --dll <x.dll> --assinaturas <arq> "
             "[-o saida.fj]  -  forja ligar --dll <x.dll> --listar",
    "G0105": "python -m pip install --user pefile; confira o caminho da DLL.",
}

# espelho de token.h 0.8.0 (palavras-chave) + nomes de tipo embutidos + embutidos da v1.9
RESERVADAS = frozenset(
    "const let var proc struct enum union unit pub export import as foreign test if else for in while switch case "
    "default break continue return defer try or_else and or not true false none "
    "i8 i16 i32 i64 u8 u16 u32 u64 int uint f32 f64 bool str cstr rawptr "
    "insert get get_ptr has remove transpose "
    "print assert panic transmute len expect new make append delete frame_reset format format_c slice_from "
    "size_of align_of offset_of static_assert min max abs clamp sqrt conj".split())

TAMANHOS = {"bool": (1, 1), "i8": (1, 1), "u8": (1, 1), "i16": (2, 2), "u16": (2, 2), "i32": (4, 4), "u32": (4, 4),
            "f32": (4, 4), "i64": (8, 8), "u64": (8, 8), "f64": (8, 8)}
# 0.7.0: c_long/c_ulong = long do host do ligar (4 no Windows LLP64, 8 no Linux LP64); o layout de struct usa esse tamanho.
TAMANHO_LONG = ctypes.sizeof(ctypes.c_long)
TAMANHOS["c_long"] = TAMANHOS["c_ulong"] = (TAMANHO_LONG, TAMANHO_LONG)
TIPOS_C_LONG = ("c_long", "c_ulong")  # 0.7.0: acessor (pub proc) com c_long seria E0342; esses membros ficam sem acessor
# 0.10.0: array de long dentro de struct (E0342 em [N]c_ulong) sai com o inteiro do tamanho do long do host, o mesmo que o layout usa
LONG_EM_ARRAY = {"c_long": f"i{8 * TAMANHO_LONG}", "c_ulong": f"u{8 * TAMANHO_LONG}"}
ARMAZENAMENTO_UNIAO = {1: "u8", 2: "u16", 4: "u32", 8: "u64"}
LITERAL_TEXTO = re.compile(r'"[ !#-\[\]-~]*"')
LITERAL_REAL = re.compile(r"(\d+\.\d*|\.\d+|\d+)([eE][+-]?\d+)?[fF]?")
CONTEXTOS_PARAMETRO = ("parametro", "callback")
ESCALARES: dict = {}  # TypeKind -> tipo Forja; preenchido por IniciarMapa


# ---------------------------------------------------------------- dados
class ErroTipo(Exception):
    """Recusa de um simbolo ou tipo; '{S}' na causa vira o nome do simbolo de fora."""

    def __init__(self, Codigo: str, Causa: str):
        super().__init__(Causa)
        self.Codigo = Codigo
        self.Causa = Causa


class FalhaFerramenta(Exception):
    """Falha G01xx: a ferramenta nao consegue rodar (nada e gravado, exit 2)."""

    def __init__(self, Codigo: str, Causa: str):
        super().__init__(Causa)
        self.Codigo = Codigo
        self.Causa = Causa


@dataclass
class Recusa:
    Codigo: str
    Arquivo: str
    Linha: int
    Coluna: int
    Simbolo: str
    Causa: str


@dataclass
class Proc:
    Texto: str


@dataclass
class Struct:
    Pos: tuple
    Nome: str
    Campos: list
    Layout: tuple = None  # 0.5.0: (tipo C, tamanho, alinhamento, [(campo Forja, campo C, offset)]) segundo a libclang


@dataclass
class Enum:
    Pos: tuple
    Nome: str
    Membros: list
    ComValor: bool


@dataclass
class Const:
    Pos: tuple
    Nome: str
    Tipo: str
    Valor: str


@dataclass
class Acesso:
    """Acessador de union: o nome do proc so e decidido depois da varredura (ResolverAcessos), quando se sabe o que ja existe."""
    Pos: tuple
    Union: str     # nome Forja da union
    Membro: str    # nome C do membro
    Tipo: str      # tipo Forja do membro
    Linha: int
    Coluna: int
    Texto: str = ""  # preenchido por ResolverAcessos


@dataclass
class Modelo:
    Prefixo: str
    PastaHeader: str
    Recusas: list = field(default_factory=list)
    Procs: list = field(default_factory=list)
    Enums: list = field(default_factory=list)
    Estruturas: list = field(default_factory=list)
    Consts: list = field(default_factory=list)
    Acessos: list = field(default_factory=list)
    Opacos: list = field(default_factory=list)
    Wrappers: list = field(default_factory=list)    # modo --cpp: um bloco extern "C" por proc, na ordem dos procs
    Tipos: dict = field(default_factory=dict)       # usr -> nome Forja (str) ou ErroTipo
    Nomes: dict = field(default_factory=dict)       # nome Forja -> nome C que o reservou
    Layouts: dict = field(default_factory=dict)     # struct/union Forja -> (tamanho, alinhamento)
    NomesEnum: set = field(default_factory=set)
    NomesTypedef: dict = field(default_factory=dict)  # usr da decl anonima -> nome do typedef
    Vistos: set = field(default_factory=set)
    CursorAtual: object = None
    ConstsPorNome: dict = field(default_factory=dict)  # nome da const -> (nome C, tipo, valor): #define repetido com o mesmo valor vale uma vez
    Candidatas: list = field(default_factory=list)     # macros de objeto que nao sao literal: (cursor, nome); avaliadas pela libclang
    WrapperVariadicas: bool = False                    # --wrapper-variadicas
    Variadicas: list = field(default_factory=list)     # blocos C do <saida>_variadicas.c
    LinkVariadicas: str = ""                           # nome da biblioteca do wrapper (linha link antes da principal)
    ArgumentosClang: list = field(default_factory=list)
    Header: str = ""
    Ajudantes: list = field(default_factory=list)      # 0.5.0: (pos, texto) dos procs get/set de bitfield e _ptr de array flexivel
    Rebaixados: set = field(default_factory=set)    # 0.4.0: nomes Forja de struct/union que falhou e virou opaque (por valor continua G0019)
    Cola: bool = False                                 # 0.6.0: --cola
    Propor: bool = False                               # 0.8.0: --propor
    Simbolos: object = None                            # 0.10.0: --simbolos: set com os nomes C pedidos (procs, variaveis, macros, constantes); None = tudo; struct/enum so nascem por referencia
    BlocosCola: list = field(default_factory=list)     # 0.6.0: texto C de cada wrapper da cola
    Sigs: dict = field(default_factory=dict)           # 0.6.0: nome da macro-funcao -> AssinaturaMacro (--macros)
    MacrosPedidas: list = field(default_factory=list)  # 0.6.0: (cursor, nome) das macro-funcoes que tem entrada no .sig
    LinkCola: str = ""                                 # 0.6.0: nome da biblioteca da cola (linha link antes da principal)
    IncludesSig: list = field(default_factory=list)    # 0.6.0: linhas '#include' do .sig, copiadas para a cola


# ---------------------------------------------------------------- CLI e saida de erro
def EscreverJson(Dados: dict) -> None:
    print(json.dumps(Dados, ensure_ascii=False), file=sys.stderr)


def EscreverFalha(Codigo: str, Causa: str) -> None:
    EscreverJson({"codigo": Codigo, "tipo": "ligar", "causa": Causa, "forma_certa": CODIGOS[Codigo]})


def EscreverRecusa(R: Recusa) -> None:
    EscreverJson({"codigo": R.Codigo, "tipo": "ligar", "arquivo": R.Arquivo, "linha": R.Linha, "coluna": R.Coluna,
                  "simbolo": R.Simbolo, "causa": R.Causa, "forma_certa": CODIGOS[R.Codigo]})


class AnalisadorLigar(argparse.ArgumentParser):
    """Erro de uso vira uma linha JSON G0104 no stderr e exit 2 (padrao dos diagnosticos da Forja)."""

    def error(self, message):  # lint-ok: nome-fora-do-padrao nome e parametro herdados de argparse
        EscreverFalha("G0104", message)
        sys.exit(2)


def CriarAnalisador() -> AnalisadorLigar:
    Analisador = AnalisadorLigar(prog="forja ligar", description="header C -> modulo .fj da Forja", allow_abbrev=False)
    Analisador.add_argument("--versao", action="version", version=f"forja ligar {VERSAO}")
    Analisador.add_argument("entrada", nargs="?")
    Analisador.add_argument("--link")
    Analisador.add_argument("-o", "--saida", dest="saida")
    Analisador.add_argument("--prefixo", default="")
    Analisador.add_argument("-I", dest="incs", action="append", default=[])
    Analisador.add_argument("-D", dest="defs", action="append", default=[])
    Analisador.add_argument("--estrito", action="store_true")
    Analisador.add_argument("--cpp", action="store_true")
    Analisador.add_argument("--dll", action="store_true")
    Analisador.add_argument("--assinaturas")
    Analisador.add_argument("--listar", action="store_true")
    Analisador.add_argument("--wrapper-variadicas", dest="wrapper_variadicas", action="store_true")
    Analisador.add_argument("--codigos", action="store_true")
    Analisador.add_argument("--cola", action="store_true")
    Analisador.add_argument("--compilar-cola", dest="compilar_cola", action="store_true")
    Analisador.add_argument("--macros")
    Analisador.add_argument("--propor", action="store_true")
    Analisador.add_argument("--simbolos")
    Analisador.add_argument("--incluir", action="append", default=[])
    Analisador.add_argument("--ajuda", action="store_true")
    return Analisador


def ErroDeUso(Analisador: AnalisadorLigar, Mensagem: str) -> None:
    Analisador.error(Mensagem)


def UsaCola(A) -> bool:
    return bool(A.cola or A.compilar_cola or A.macros)


def ValidarSozinho(A, Analisador: AnalisadorLigar) -> None:
    """--codigos e --ajuda valem sozinhos (nenhuma outra opcao junto, e nao os dois)."""
    Outros = [A.entrada, A.link, A.saida, A.prefixo, A.incs, A.defs, A.estrito, A.cpp, A.dll, A.assinaturas, A.listar,
              A.wrapper_variadicas, UsaCola(A), A.propor]
    if any(Outros) or (A.codigos and A.ajuda):
        ErroDeUso(Analisador, "--codigos e --ajuda valem sozinhos")


def ValidarCola(A, Analisador: AnalisadorLigar) -> None:
    if UsaCola(A) and (A.cpp or A.dll):
        ErroDeUso(Analisador, "--cola, --compilar-cola e --macros valem so no modo C (header .h)")


def ValidarArgumentos(A, Analisador: AnalisadorLigar) -> None:
    if A.codigos or A.ajuda:
        ValidarSozinho(A, Analisador)
        return
    ValidarCola(A, Analisador)
    if A.propor and (A.cpp or A.dll):
        ErroDeUso(Analisador, "--propor vale so no modo C (header .h)")
    if A.dll:
        if not A.entrada or not (A.listar or A.assinaturas) or A.cpp or A.link or A.wrapper_variadicas:
            ErroDeUso(Analisador, "--dll exige a DLL e (--listar ou --assinaturas), e nao aceita --cpp, --link nem --wrapper-variadicas")
        return
    if not A.entrada or not A.link:
        ErroDeUso(Analisador, "informe <header> e --link <lib>")
    if A.cpp and not A.saida:
        ErroDeUso(Analisador, "--cpp exige -o <saida.fj>")
    if A.wrapper_variadicas and (A.cpp or not A.saida):
        ErroDeUso(Analisador, "--wrapper-variadicas vale no modo C e exige -o <saida.fj> (o wrapper sai em <saida>_variadicas.c)")


# ---------------------------------------------------------------- ambiente: libclang, gcc, header
def CarregarLibclang() -> None:
    global Ci
    if os.environ.get("FORJA_LIGAR_SEM_LIBCLANG") == "1":
        raise FalhaFerramenta("G0102", "modulo clang.cindex nao encontrado (FORJA_LIGAR_SEM_LIBCLANG=1)")
    try:
        import clang.cindex as ModuloClang
        ModuloClang.Index.create()
    except Exception as Excecao:  # ImportError ou LibclangError: ambas viram G0102 com o texto da excecao
        raise FalhaFerramenta("G0102", f"modulo clang.cindex nao encontrado ({Excecao})")
    Ci = ModuloClang
    IniciarMapa()


def IniciarMapa() -> None:
    K = Ci.TypeKind
    ESCALARES.update({
        K.BOOL: "bool", K.CHAR_S: "i8", K.SCHAR: "i8", K.CHAR_U: "u8", K.UCHAR: "u8", K.SHORT: "i16", K.USHORT: "u16",
        K.INT: "i32", K.UINT: "u32", K.LONG: "c_long", K.ULONG: "c_ulong", K.LONGLONG: "i64", K.ULONGLONG: "u64",
        K.FLOAT: "f32", K.DOUBLE: "f64", K.WCHAR: "u16", K.CHAR16: "u16", K.CHAR32: "u32"})


def IncludesDoCompilador(Compilador: str = "gcc", Linguagem: str = "c") -> list:
    try:
        Rodada = subprocess.run([Compilador, "-E", "-x", Linguagem, "-", "-v"], input=b"", capture_output=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        raise FalhaFerramenta("G0103", f"{Compilador} nao encontrado para descobrir os includes")
    Texto = Rodada.stderr.decode("utf-8", "replace")
    Achado = re.search(r"#include <\.\.\.> search starts here:\r?\n(.*?)End of search list\.", Texto, re.S)
    if not Achado:
        return []
    return [os.path.normpath(Linha.strip()) for Linha in Achado.group(1).splitlines() if Linha.strip()]


def AcharHeader(Entrada: str, PastasUsuario: list, PastasGcc: list) -> str:
    if os.path.isfile(Entrada):
        return os.path.abspath(Entrada)
    for Pasta in list(PastasUsuario) + list(PastasGcc):
        Candidato = os.path.join(Pasta, Entrada)
        if os.path.isfile(Candidato):
            return os.path.abspath(Candidato)
    raise FalhaFerramenta("G0103", f"header '{Entrada}' nao encontrado")


HEADERS_EXTRAS: list = []  # 0.10.0: headers de --incluir, injetados com -include no clang (o escopo continua a pasta do header principal)


def ArgumentosDoClang(Pastas: list, Macros: list, Cpp: bool = False) -> list:
    Linguagem = ["-x", "c++", "-std=c++17"] if Cpp else ["-x", "c", "-std=gnu11"]
    Argumentos = Linguagem + ["--target=x86_64-w64-windows-gnu"]
    for Extra in HEADERS_EXTRAS:  # 0.10.0: --incluir
        Argumentos += ["-include", Extra]
    return Argumentos + [f"-I{Pasta}" for Pasta in Pastas] + [f"-D{Macro}" for Macro in Macros]


def OpcoesDoClang() -> int:
    return Ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD | Ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES


def AbrirUnidade(Header: str, Pastas: list, Macros: list, Cpp: bool = False):
    try:
        return AbrirUnidadeCom(Header, Pastas, Macros, Cpp)
    except FalhaFerramenta as Falha:
        if "definition of builtin function" not in str(Falha):
            raise
    # 0.5.0: o libclang le os *intrin*.h do gcc (extern inline __rdtsc...) e recusa redefinir builtin: 2a tentativa com eles vazios
    return AbrirUnidadeCom(Header, [PastaDeIntrinsicsVazios(Pastas)] + list(Pastas), Macros, Cpp)


def PastaDeIntrinsicsVazios(Pastas: list) -> str:
    """Pasta temporaria com um arquivo vazio por '*intrin*.h' achado nas pastas de include (tem que vir antes delas no -I)."""
    Destino = tempfile.mkdtemp(prefix="forja_ligar_intrin_")
    atexit.register(shutil.rmtree, Destino, True)
    for Arquivo in [A for P in Pastas for A in glob.glob(os.path.join(P, "*intrin*.h"))]:
        with open(os.path.join(Destino, os.path.basename(Arquivo)), "w", encoding="utf-8") as Saida:
            Saida.write("/* vazio de proposito (forja ligar) */\n")
    return Destino


def AbrirUnidadeCom(Header: str, Pastas: list, Macros: list, Cpp: bool = False):
    Argumentos = ArgumentosDoClang(Pastas, Macros, Cpp)
    Opcoes = OpcoesDoClang()
    try:
        Unidade = Ci.Index.create().parse(Header, args=Argumentos, options=Opcoes)
    except Ci.TranslationUnitLoadError as Excecao:
        raise FalhaFerramenta("G0103", f"{Header}: {Excecao}")
    for Diagnostico in Unidade.diagnostics:
        if Diagnostico.severity >= Ci.Diagnostic.Error:
            L = Diagnostico.location
            Arquivo = L.file.name if L.file else Header
            raise FalhaFerramenta("G0103", f"{Arquivo}:{L.line}:{L.column}: {Diagnostico.spelling}")
    return Unidade


# ---------------------------------------------------------------- nomes
def NomeSnake(Nome: str) -> str:
    return re.sub(r"_+", "_", re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", Nome).lower())


def NomePascal(Nome: str) -> str:
    if Nome.endswith("_t"):
        Nome = Nome[:-2]
    Partes = [P for P in re.split(r"_|(?<=[a-z])(?=[A-Z])", Nome) if P]
    return "".join(P[:1].upper() + (P[1:].lower() if P.isupper() or P.islower() else P[1:]) for P in Partes)


def NomeLivre(Nome: str) -> str:
    return Nome + "_" if Nome in RESERVADAS else Nome


def NomeAscii(Nome: str) -> bool:
    return bool(Nome) and Nome.isascii() and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", Nome) is not None


def NomeDoTipo(M: Modelo, Decl) -> str:
    """Nome C de uma struct/union/enum: o typedef de uma decl anonima ou a tag; vazio se nao tem nome."""
    Nome = M.NomesTypedef.get(Decl.get_usr()) or Decl.spelling
    return "" if (not Nome or "unnamed" in Nome or "anonymous" in Nome) else Nome


def ReservarNome(M: Modelo, NomeForja: str, NomeC: str) -> str:
    Dono = M.Nomes.get(NomeForja, NomeC)
    if Dono != NomeC:
        raise ErroTipo("G0013", f"o nome '{NomeC}' colide com '{Dono}'")
    M.Nomes[NomeForja] = NomeC
    return NomeForja


def NomeDoParametro(Bruto: str, Indice: int, Usados: set) -> str:
    Nome = NomeLivre(NomeSnake(Bruto)) if NomeAscii(Bruto) else f"a{Indice}"
    if Nome in Usados:
        Nome = f"a{Indice}"
    Usados.add(Nome)
    return Nome


def PosicaoDe(Cur) -> tuple:
    L = Cur.location
    return (L.file.name if L.file else "", L.offset)


def Recusar(M: Modelo, Cur, Simbolo: str, Erro: ErroTipo) -> None:
    L = Cur.location
    Arquivo = os.path.basename(L.file.name) if L.file else ""
    M.Recusas.append(Recusa(Erro.Codigo, Arquivo, L.line, L.column, Simbolo, Erro.Causa.replace("{S}", Simbolo)))


# ---------------------------------------------------------------- tipos
def MapearTipo(M: Modelo, T, Contexto: str) -> str:
    """Tipo C -> tipo Forja. Contexto: parametro | callback | retorno | campo | interno. ErroTipo se nao tem forma."""
    C = T.get_canonical()
    K = C.kind
    if K in ESCALARES:
        return ESCALARES[K]
    if K == Ci.TypeKind.POINTER:
        return MapearPonteiro(M, T, Contexto)
    if K == Ci.TypeKind.ENUM:
        return MapearEnum(M, C)
    if K == Ci.TypeKind.RECORD:
        return MapearRecordPorValor(M, C)
    if K == Ci.TypeKind.CONSTANTARRAY and Contexto == "campo":
        Elemento = MapearTipo(M, C.get_array_element_type(), Contexto)
        return f"[{C.get_array_size()}]{LONG_EM_ARRAY.get(Elemento, Elemento)}"
    if K in (Ci.TypeKind.CONSTANTARRAY, Ci.TypeKind.INCOMPLETEARRAY) and Contexto == "parametro":
        return MapearArrayParametro(M, C)
    raise ErroTipo("G0001", f"o tipo C '{T.spelling}' nao tem forma na Forja")


def MapearArrayParametro(M: Modelo, C) -> str:
    """v0.6.1 (F1f): parametro 'T x[N]' decai em 'T *' no C; sai como o ponteiro de MapearPonteiro (?^T, cstr, ?^i8). Array de ponteiro ou de array: G0001."""
    Elemento = C.get_array_element_type().get_canonical()
    K = Elemento.kind
    if K == Ci.TypeKind.CHAR_S:
        return MapearPonteiroChar(Elemento, "parametro")
    if K == Ci.TypeKind.RECORD:
        return "?^" + NomeApontado(M, Elemento)
    if K == Ci.TypeKind.ENUM or K in ESCALARES:
        return "?^" + MapearTipo(M, Elemento, "interno")
    raise ErroTipo("G0001", f"o tipo C '{C.spelling}' nao tem forma na Forja")


def MapearEnum(M: Modelo, C) -> str:
    Decl = C.get_declaration()
    if not NomeDoTipo(M, Decl):
        return "i32"
    return ObterTipo(M, Decl, ConstruirEnum)


def MapearRecordPorValor(M: Modelo, C) -> str:
    Decl = C.get_declaration()
    Definicao = Decl.get_definition()
    if Definicao is None:
        raise ErroTipo("G0017", f"'{Decl.spelling}' e incompleto e aparece por valor")
    Nome = ObterTipo(M, Definicao, ConstruirRecord)
    if Nome in M.Rebaixados:
        raise ErroTipo("G0019", f"'{{S}}' usa '{Nome}' por valor e a struct virou opaque (so por ponteiro)")
    return Nome


def NomeApontado(M: Modelo, Pontudo) -> str:
    Decl = Pontudo.get_declaration()
    Definicao = Decl.get_definition()
    if Definicao is None:
        return ObterOpaque(M, Decl)
    return ObterTipo(M, Definicao, ConstruirRecord)


def MapearPonteiro(M: Modelo, T, Contexto: str) -> str:
    Pontudo = T.get_canonical().get_pointee().get_canonical()
    K = Pontudo.kind
    if K in (Ci.TypeKind.FUNCTIONPROTO, Ci.TypeKind.FUNCTIONNOPROTO):
        return TipoDoCallback(M, T, Contexto)
    if K == Ci.TypeKind.VOID:
        return "rawptr"
    if K == Ci.TypeKind.CHAR_S:
        return MapearPonteiroChar(Pontudo, Contexto)
    if K == Ci.TypeKind.POINTER:
        return MapearPonteiroDuplo(M, Pontudo, Contexto)
    if K == Ci.TypeKind.RECORD:
        return "?^" + NomeApontado(M, Pontudo)
    return "?^" + MapearTipo(M, Pontudo, "interno")


def MapearPonteiroChar(Pontudo, Contexto: str) -> str:
    if Contexto == "campo":
        return "rawptr"
    if Contexto == "retorno":
        return "?cstr"
    if Contexto in CONTEXTOS_PARAMETRO:
        return "cstr" if Pontudo.is_const_qualified() else "?^i8"
    return "?^i8"


def MapearPonteiroDuplo(M: Modelo, Pontudo, Contexto: str) -> str:
    Interno = Pontudo.get_pointee().get_canonical()
    if Interno.kind == Ci.TypeKind.CHAR_S and Contexto != "campo":
        return "?^cstr"
    Apontado = MapearTipo(M, Pontudo, "interno")
    if Apontado in ("?^c_long", "?^c_ulong"):  # 0.7.0: c_long so vale um nivel abaixo do ponteiro da fronteira (E0342)
        raise ErroTipo("G0001", f"o ponteiro duplo '{{S}}' aponta para '{Pontudo.spelling}' (long): a Forja so aceita c_long direto num ponteiro")
    return "?^" + Apontado


def LongDoHost(Tipo: str) -> str:
    """0.10.0: tipo de proc(...) nao aceita c_long/c_ulong (E0342); o callback usa o inteiro do tamanho do long do host (o do layout)."""
    return re.sub(r"\bc_(u?)long\b", lambda Achado: ("u" if Achado.group(1) else "i") + str(8 * TAMANHO_LONG), Tipo)


def TipoDoCallback(M: Modelo, T, Contexto: str) -> str:
    """Ponteiro de funcao em parametro -> proc(nome: T, ...) [-> R]; em campo, retorno ou tipo aninhado -> rawptr (0.4.0; ?proc na v1.13)."""
    if Contexto != "parametro":
        return "rawptr"
    Proto = T.get_canonical().get_pointee().get_canonical()
    if Proto.kind != Ci.TypeKind.FUNCTIONPROTO:
        raise ErroTipo("G0001", f"o tipo C '{T.spelling}' nao tem forma na Forja")
    if Proto.is_function_variadic():
        raise ErroTipo("G0008", "a funcao '{S}' e variadica")
    Nomes = NomesDoCallback(M, T)
    Usados: set = set()
    Itens = []
    for Indice, Arg in enumerate(Proto.argument_types()):
        Bruto = Nomes[Indice] if Indice < len(Nomes) else ""
        Itens.append(f"{NomeDoParametro(Bruto, Indice, Usados)}: {LongDoHost(MapearTipo(M, Arg, 'callback'))}")
    Resultado = Proto.get_result()
    Sufixo = "" if Resultado.get_canonical().kind == Ci.TypeKind.VOID else " -> " + LongDoHost(MapearTipo(M, Resultado, "retorno"))
    return f"proc({', '.join(Itens)}){Sufixo}"


def NomesDoCallback(M: Modelo, T) -> list:
    Decl = T.get_declaration()
    Fonte = Decl if Decl.kind == Ci.CursorKind.TYPEDEF_DECL else M.CursorAtual
    if Fonte is None:
        return []
    return [C.spelling for C in Fonte.get_children() if C.kind == Ci.CursorKind.PARM_DECL]


def ObterTipo(M: Modelo, Decl, Construtor) -> str:
    """Nome Forja do tipo (enum/struct/union); constroi na 1a vez. Falha vira recusa propria + G0019 para quem usa."""
    Usr = Decl.get_usr()
    Conhecido = M.Tipos.get(Usr)
    if isinstance(Conhecido, str):
        return Conhecido
    NomeC = NomeDoTipo(M, Decl)
    if isinstance(Conhecido, ErroTipo):
        raise ErroTipo("G0019", f"'{{S}}' usa o tipo '{NomeC}', recusado antes ({Conhecido.Codigo})")
    try:
        return Construtor(M, Decl)
    except ErroTipo as Erro:
        if PodeVirarOpaque(Decl, NomeC, Erro):
            Recusar(M, Decl, NomeC, ErroTipo(Erro.Codigo, Erro.Causa + "; virou opaque"))
            return RebaixarParaOpaque(M, Decl, NomeC)
        M.Tipos[Usr] = Erro
        Recusar(M, Decl, NomeC, Erro)
        raise ErroTipo("G0019", f"'{{S}}' usa o tipo '{NomeC}', recusado antes ({Erro.Codigo})")


def PodeVirarOpaque(Decl, NomeC: str, Erro: ErroTipo) -> bool:
    """0.4.0: struct/union com nome que falhou (menos colisao de nome, que o opaque repetiria) pode ser opaque em vez de cascata G0019."""
    return Decl.kind in (Ci.CursorKind.STRUCT_DECL, Ci.CursorKind.UNION_DECL) and bool(NomeC) and Erro.Codigo != "G0013"


def RebaixarParaOpaque(M: Modelo, Decl, NomeC: str) -> str:
    """Troca a struct que nao deu por 'opaque <Nome>' (o mesmo nome que os tipos ja construidos usam por ponteiro)."""
    Nome = ReservarNome(M, NomePascal(NomeC), NomeC)
    M.Tipos[Decl.get_usr()] = Nome
    if Nome not in M.Opacos:
        M.Opacos.append(Nome)
    M.Rebaixados.add(Nome)
    return Nome


def ObterOpaque(M: Modelo, Decl) -> str:
    Usr = Decl.get_usr()
    if isinstance(M.Tipos.get(Usr), str):
        return M.Tipos[Usr]
    Nome = ReservarNome(M, NomePascal(Decl.spelling), Decl.spelling)
    M.Tipos[Usr] = Nome
    M.Opacos.append(Nome)
    return Nome


# ---------------------------------------------------------------- enum
def PrefixoDosMembros(Nomes: list) -> str:
    Comum = os.path.commonprefix(Nomes)
    return Comum[:Comum.rfind("_") + 1] if "_" in Comum else ""


def NomesDosMembros(Nomes: list) -> list:
    Prefixo = PrefixoDosMembros(Nomes)
    Restos = [N[len(Prefixo):] for N in Nomes]
    if Prefixo and all(re.match(r"[A-Za-z]", R) and NomeSnake(R) not in RESERVADAS for R in Restos):
        return [NomeSnake(R) for R in Restos]
    return [NomeLivre(NomeSnake(N)) for N in Nomes]


def MembrosSemRepetidos(M: Modelo, Itens: list, NomeC: str) -> list:
    Vistos: dict = {}
    Aceitos = []
    for Cur, Nome, Valor in Itens:
        if Valor in Vistos:
            Recusar(M, Cur, Nome, ErroTipo("G0016", f"o enum '{NomeC}' tem '{Nome}' = {Valor} repetindo '{Vistos[Valor]}'"))
            continue
        Vistos[Valor] = Nome
        Aceitos.append((Nome, Valor))
    return Aceitos


def ConstruirEnum(M: Modelo, Decl) -> str:
    NomeC = NomeDoTipo(M, Decl)
    Itens = [(C, C.spelling, C.enum_value) for C in Decl.get_children() if C.kind == Ci.CursorKind.ENUM_CONSTANT_DECL]
    for _, Nome, Valor in Itens:
        if not -2**31 <= Valor < 2**31:
            raise ErroTipo("G0016", f"o enum '{NomeC}' tem '{Nome}' = {Valor} fora de i32")
    Nome = ReservarNome(M, NomePascal(NomeC), NomeC)
    Aceitos = MembrosSemRepetidos(M, Itens, NomeC)
    Valores = [V for _, V in Aceitos]
    Membros = list(zip(NomesDosMembros([N for N, _ in Aceitos]), Valores))
    M.Tipos[Decl.get_usr()] = Nome
    M.NomesEnum.add(Nome)
    M.Enums.append(Enum(PosicaoDe(Decl), Nome, Membros, Valores != list(range(len(Valores)))))
    return Nome


def LerSimbolosPedidos(Valor):
    """0.10.0: --simbolos nome1,nome2 ou arquivo (um nome por linha; '#' comenta a linha). None = sem filtro."""
    if not Valor:
        return None
    Texto = Valor
    if os.path.isfile(Valor):
        with open(Valor, encoding="utf-8") as Arquivo:
            Texto = "\n".join(Linha.split("#", 1)[0] for Linha in Arquivo)
    return {Nome for Nome in re.split(r"[\s,]+", Texto) if Nome}


def SimboloPedido(M: Modelo, Nome: str) -> bool:
    """Nome exato, ou prefixo quando o pedido termina em '*' (WM_* pede todas as macros que comecam por WM_)."""
    if M.Simbolos is None or Nome in M.Simbolos:
        return True
    return any(Pedido.endswith("*") and Nome.startswith(Pedido[:-1]) for Pedido in M.Simbolos)


def ColetarEnum(M: Modelo, Decl) -> None:
    NomeC = NomeDoTipo(M, Decl)
    if NomeC:
        if NomeC.startswith(M.Prefixo) and M.Simbolos is None:
            TentarTipo(M, Decl, ConstruirEnum)
        return
    for Cur in Decl.get_children():
        if Cur.kind == Ci.CursorKind.ENUM_CONSTANT_DECL:
            ColetarConstDeEnum(M, Cur)


def ColetarConstDeEnum(M: Modelo, Cur) -> None:
    Nome = Cur.spelling
    if Nome.startswith("_") or not NomeAscii(Nome) or not Nome.startswith(M.Prefixo) or not SimboloPedido(M, Nome):
        return
    AdicionarConst(M, Cur, Nome, Cur.enum_value)


def TentarTipo(M: Modelo, Decl, Construtor) -> None:
    try:
        ObterTipo(M, Decl, Construtor)
    except ErroTipo:
        pass  # a recusa ja foi registrada por ObterTipo


# ---------------------------------------------------------------- struct e union
def TamanhoForja(M: Modelo, Tipo: str) -> tuple:
    if Tipo in TAMANHOS:
        return TAMANHOS[Tipo]
    Vetor = re.fullmatch(r"\[(\d+)\](.+)", Tipo)
    if Vetor:
        Tamanho, Alinhamento = TamanhoForja(M, Vetor.group(2))
        return int(Vetor.group(1)) * Tamanho, Alinhamento
    if Tipo[0] in "?^" or Tipo in ("rawptr", "cstr"):
        return 8, 8
    if Tipo in M.NomesEnum:
        return 4, 4
    if Tipo not in M.Layouts:  # tipo que se contem atraves de ponteiro: o layout ainda nao saiu
        raise ErroTipo("G0007", f"o tipo '{Tipo}' ainda nao tem layout (tipo recursivo atraves de ponteiro)")
    return M.Layouts[Tipo]


def AlinharPara(Valor: int, Alinhamento: int) -> int:
    return (Valor + Alinhamento - 1) // Alinhamento * Alinhamento


def LayoutNatural(M: Modelo, Tipos: list) -> tuple:
    """(offsets, tamanho, alinhamento) que a Forja gera para esses campos em sequencia (layout natural do C)."""
    Posicao = 0
    Maior = 1
    Offsets = []
    for Tipo in Tipos:
        Tamanho, Alinhamento = TamanhoForja(M, Tipo)
        Posicao = AlinharPara(Posicao, Alinhamento)
        Offsets.append(Posicao)
        Posicao += Tamanho
        Maior = max(Maior, Alinhamento)
    return Offsets, AlinharPara(Posicao, Maior), Maior


def CamposEstruturais(M: Modelo, Decl, NomeC: str) -> list:
    """FIELD_DECLs sem bitfield, sem tipo anonimo e sem membro flexivel; senao ErroTipo."""
    Campos = []
    for Cur in Decl.get_children():
        if Cur.kind != Ci.CursorKind.FIELD_DECL:
            continue
        if Cur.is_bitfield():
            raise ErroTipo("G0006", f"a struct '{NomeC}' tem bitfield ('{Cur.spelling}')")
        Canonico = Cur.type.get_canonical()
        Interno = Canonico.get_declaration()
        if not Cur.spelling or (Interno.kind in (Ci.CursorKind.STRUCT_DECL, Ci.CursorKind.UNION_DECL)
                                and not NomeDoTipo(M, Interno)):
            raise ErroTipo("G0014", f"a struct '{NomeC}' tem struct ou union anonima")
        if EhFlexivel(Canonico):
            raise ErroTipo("G0014", f"a struct '{NomeC}' tem membro flexivel '{Cur.spelling}'")
        Campos.append(Cur)
    return Campos


def MapearCampo(M: Modelo, Cur, NomeC: str) -> str:
    try:
        return MapearTipo(M, Cur.type, "campo")
    except ErroTipo as Erro:
        raise ErroTipo(Erro.Codigo, Erro.Causa.replace("{S}", NomeC))


def ConstruirRecord(M: Modelo, Decl) -> str:
    NomeC = NomeDoTipo(M, Decl)
    if not NomeC:
        raise ErroTipo("G0014", "a struct '<anonima>' tem struct ou union anonima")
    Nome = ReservarNome(M, NomePascal(NomeC), NomeC)
    M.Tipos[Decl.get_usr()] = Nome
    ConstruirComNome(M, Decl, Nome, NomeC)
    return Nome


def ConstruirComNome(M: Modelo, Decl, Nome: str, NomeC: str) -> None:
    """Union -> armazenamento + acessos; struct com bitfield/anonima/flexivel (0.5.0) -> ConstruirStructEspecial; o resto -> campos."""
    if Decl.kind == Ci.CursorKind.UNION_DECL:
        ConstruirUniao(M, Decl, Nome, NomeC, CamposEstruturais(M, Decl, NomeC))
    elif TemMembroEspecial(M, Decl):
        ConstruirStructEspecial(M, Decl, Nome, NomeC)
    else:
        ConstruirStruct(M, Decl, Nome, NomeC, CamposEstruturais(M, Decl, NomeC))


def ConstruirStruct(M: Modelo, Decl, Nome: str, NomeC: str, Campos: list) -> None:
    Tipos = [MapearCampo(M, Cur, NomeC) for Cur in Campos]
    Offsets, Tamanho, Alinhamento = LayoutNatural(M, Tipos)
    for Cur, Natural in zip(Campos, Offsets):
        Real = Cur.get_field_offsetof() // 8
        if Real != Natural:
            raise ErroTipo("G0007", f"a struct '{NomeC}' nao tem o layout natural do C (campo '{Cur.spelling}' "
                                    f"no offset {Real}, natural {Natural})")
    if Decl.type.get_size() != Tamanho:
        raise ErroTipo("G0007", f"a struct '{NomeC}' nao tem o layout natural do C (campo '(tamanho)' "
                                f"no offset {Decl.type.get_size()}, natural {Tamanho})")
    Nomes = [NomeLivre(NomeSnake(Cur.spelling)) for Cur in Campos]
    M.Layouts[Nome] = (Tamanho, Alinhamento)
    Medidas = [(N, Cur.spelling, Cur.get_field_offsetof() // 8) for N, Cur in zip(Nomes, Campos)]
    M.Estruturas.append(Struct(PosicaoDe(Decl), Nome, list(zip(Nomes, Tipos)), (TipoCDoLayout(M, Decl), Tamanho, Alinhamento, Medidas)))


def ConstruirUniao(M: Modelo, Decl, Nome: str, NomeC: str, Campos: list) -> None:
    Tamanho, Alinhamento = Decl.type.get_size(), Decl.type.get_align()
    if Alinhamento not in ARMAZENAMENTO_UNIAO:
        raise ErroTipo("G0007", f"a struct '{NomeC}' nao tem o layout natural do C (alinhamento {Alinhamento})")
    Armazenamento = f"[{Tamanho // Alinhamento}]{ARMAZENAMENTO_UNIAO[Alinhamento]}"
    M.Layouts[Nome] = (Tamanho, Alinhamento)
    M.Estruturas.append(Struct(PosicaoDe(Decl), Nome, [("armazenamento", Armazenamento)], (TipoCDoLayout(M, Decl), Tamanho, Alinhamento, [])))
    for Cur in Campos:
        ColetarAcesso(M, Decl, Nome, Cur)


# ---------------------------------------------------------------- bitfield, anonima e flexivel (0.5.0, F1c)
INTEIROS_BITFIELD = ("bool", "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64")


@dataclass
class Membro:
    """Campo da struct Forja: nome Forja, tipo Forja, offset em bytes e nome C ('-' = gerado pelo ligar, sem equivalente em C)."""
    Nome: str
    Tipo: str
    Offset: int
    NomeC: str


@dataclass
class Bitfield:
    Campo: str         # nome C
    Tipo: str          # tipo Forja do valor (get/set)
    Armazenamento: str  # campo Forja que guarda a unidade
    Deslocamento: int  # bits dentro da unidade
    Largura: int
    Bits: int          # bits da unidade (8, 16, 32, 64)


@dataclass
class Coleta:
    M: Modelo
    Decl: object
    Nome: str   # nome Forja da struct
    NomeC: str
    Membros: list = field(default_factory=list)
    Bitfields: list = field(default_factory=list)
    Flexiveis: list = field(default_factory=list)  # (nome C, tipo Forja do elemento, offset em bytes)
    Unidades: dict = field(default_factory=dict)   # (offset, bytes) da unidade -> nome Forja do armazenamento
    Anonimas: int = 0


def EhRecordAnonimo(M: Modelo, Decl) -> bool:
    return Decl.kind in (Ci.CursorKind.STRUCT_DECL, Ci.CursorKind.UNION_DECL) and not NomeDoTipo(M, Decl)


def TipoCDoLayout(M: Modelo, Decl) -> str:
    """Tipo C que o gcc enxerga (sizeof/offsetof na prova da bateria); '-' para struct/union anonima, que o C nao nomeia."""
    return "-" if EhRecordAnonimo(M, Decl) else Decl.type.spelling


def TemMembroEspecial(M: Modelo, Decl) -> bool:
    """Bitfield, array flexivel ou struct/union anonima (como membro sem nome ou como tipo de campo)."""
    return any(MembroEspecial(M, Cur) for Cur in Decl.get_children())


def MembroEspecial(M: Modelo, Cur) -> bool:
    if Cur.kind != Ci.CursorKind.FIELD_DECL:
        return EhRecordAnonimo(M, Cur)
    Canonico = Cur.type.get_canonical()
    return Cur.is_bitfield() or EhFlexivel(Canonico) or EhRecordAnonimo(M, Canonico.get_declaration())


def EhFlexivel(Canonico) -> bool:
    """0.10.0: membro flexivel = T x[] (INCOMPLETEARRAY) ou o GNU T x[0] (CONSTANTARRAY de tamanho 0, que a Forja recusa com E0308)."""
    return Canonico.kind == Ci.TypeKind.INCOMPLETEARRAY or (Canonico.kind == Ci.TypeKind.CONSTANTARRAY and Canonico.get_array_size() == 0)


def ConstruirAninhado(M: Modelo, Interno, NomeForja: str, Descricao: str) -> str:
    """Struct/union anonima de dentro de outra vira um tipo proprio com nome gerado (ex.: BitsDemoAnon0)."""
    ReservarNome(M, NomeForja, Descricao)
    M.Tipos[Interno.get_usr()] = NomeForja
    ConstruirComNome(M, Interno, NomeForja, Descricao)
    return NomeForja


def AdicionarBitfield(C: Coleta, Cur) -> None:
    """Agrupa os bitfields pela unidade de armazenamento que a libclang (layout do alvo) usa e guarda deslocamento e largura de cada um."""
    if not Cur.spelling:
        return  # bitfield sem nome so empurra o layout
    NomeC = C.NomeC
    Tipo = MapearTipo(C.M, Cur.type, "campo")
    Bytes, Largura, Bits = Cur.type.get_size(), Cur.get_bitfield_width(), Cur.get_field_offsetof()
    Unidade = Bits // (8 * Bytes) * Bytes
    if Tipo not in INTEIROS_BITFIELD or Bits + Largura > (Unidade + Bytes) * 8 or (Tipo[0] == "i" and 62 < Largura < 8 * Bytes):
        raise ErroTipo("G0006", f"a struct '{NomeC}' tem bitfield '{Cur.spelling}' que a ponte nao acessa (tipo {Tipo}, largura {Largura})")
    Chave = (Unidade, Bytes)
    if Chave not in C.Unidades:
        C.Unidades[Chave] = f"bits_{len(C.Unidades)}"
        C.Membros.append(Membro(C.Unidades[Chave], f"u{8 * Bytes}", Unidade, "-"))
    C.Bitfields.append(Bitfield(Cur.spelling, Tipo, C.Unidades[Chave], Bits - Unidade * 8, Largura, 8 * Bytes))


def AdicionarFlexivel(C: Coleta, Cur) -> None:
    Elemento = MapearTipo(C.M, Cur.type.get_canonical().get_array_element_type(), "campo")
    if Elemento in TIPOS_C_LONG:
        return  # 0.7.0: o _ptr seria pub proc com ^c_long (E0342): sem acessor, como o membro de union que nao mapeia
    C.Flexiveis.append((Cur.spelling, Elemento, Cur.get_field_offsetof() // 8))


def AdicionarCampoComum(C: Coleta, Cur, Pendentes: dict) -> None:
    Interno = Cur.type.get_canonical().get_declaration()
    if not Cur.spelling:
        raise ErroTipo("G0014", f"a struct '{C.NomeC}' tem campo sem nome")
    if EhRecordAnonimo(C.M, Interno):  # campo nomeado cujo tipo e uma struct/union sem nome
        Pendentes.pop(Interno.get_usr(), None)
        Tipo = ConstruirAninhado(C.M, Interno, f"{C.Nome}{NomePascal(Cur.spelling)}", f"{C.NomeC}.{Cur.spelling}")
    else:
        Tipo = MapearCampo(C.M, Cur, C.NomeC)
    C.Membros.append(Membro(NomeLivre(NomeSnake(Cur.spelling)), Tipo, Cur.get_field_offsetof() // 8, Cur.spelling))


def AdicionarAnonimoSemNome(C: Coleta, Interno) -> None:
    """Membro anonimo (union { int i; float f; };): campo _anonN; o offset sai do 1o membro nomeado de dentro (libclang nao da o do anonimo)."""
    Indice, C.Anonimas = C.Anonimas, C.Anonimas + 1
    Primeiro = next((F for F in Interno.get_children() if F.kind == Ci.CursorKind.FIELD_DECL and F.spelling), None)
    if Primeiro is None:
        raise ErroTipo("G0014", f"a struct '{C.NomeC}' tem struct ou union anonima sem membro nomeado")
    Offset = (C.Decl.type.get_offset(Primeiro.spelling) - Interno.type.get_offset(Primeiro.spelling)) // 8
    Tipo = ConstruirAninhado(C.M, Interno, f"{C.Nome}Anon{Indice}", f"{C.NomeC}._anon{Indice}")
    C.Membros.append(Membro(f"_anon{Indice}", Tipo, Offset, "-"))


def ColetarFilho(C: Coleta, Cur, Pendentes: dict) -> None:
    """Despacha um filho da struct: decl anonima (fica pendente ate um campo reclamar), bitfield, flexivel ou campo comum."""
    if EhRecordAnonimo(C.M, Cur):
        Pendentes[Cur.get_usr()] = Cur
        return
    if Cur.kind != Ci.CursorKind.FIELD_DECL:
        return
    if Cur.is_bitfield():
        AdicionarBitfield(C, Cur)
    elif EhFlexivel(Cur.type.get_canonical()):
        AdicionarFlexivel(C, Cur)
    else:
        AdicionarCampoComum(C, Cur, Pendentes)


def ColetarMembros(M: Modelo, Decl, Nome: str, NomeC: str) -> Coleta:
    C = Coleta(M, Decl, Nome, NomeC)
    Pendentes: dict = {}
    for Cur in Decl.get_children():
        ColetarFilho(C, Cur, Pendentes)
    for Interno in Pendentes.values():
        AdicionarAnonimoSemNome(C, Interno)
    C.Membros.sort(key=lambda X: X.Offset)
    return C


def MascaraDe(Largura: int) -> int:
    return (1 << Largura) - 1


def CorpoDoBitfieldAssinado(B: Bitfield, Valor: str) -> list:
    """Largura cheia: bits crus; senao estende o sinal (valor >= 2^(w-1) vira valor - 2^w)."""
    if B.Largura == B.Bits:
        return [f"    return transmute(p.{B.Armazenamento}, to = {B.Tipo})"]
    return [f"    let u = {Valor}", f"    if u >= {1 << (B.Largura - 1)} {{", f"        return ((u as i64) - {1 << B.Largura}) as {B.Tipo}",
            "    }", f"    return u as {B.Tipo}"]


def TextoBitfieldLer(Nome: str, Proc: str, B: Bitfield) -> str:
    Valor = f"(p.{B.Armazenamento} >> {B.Deslocamento}) & {MascaraDe(B.Largura)}"
    Corpo = {"b": [f"    return ({Valor}) != 0"], "i": CorpoDoBitfieldAssinado(B, Valor)}.get(B.Tipo[0], [f"    return ({Valor}) as {B.Tipo}"])
    return "\n".join([f"pub proc {Proc}_get(p: ^{Nome}) -> {B.Tipo} {{"] + Corpo + ["}"])


def TextoBitfieldGravar(Nome: str, Proc: str, B: Bitfield) -> str:
    Mascara, Unidade = MascaraDe(B.Largura), MascaraDe(B.Bits)
    Mantem = ~(Mascara << B.Deslocamento) & Unidade
    Armazem = f"u{B.Bits}"
    Antes = []
    Origem = "v"
    if B.Tipo == "bool":
        Antes = [f"    var x: {Armazem} = 0", "    if v {", "        x = 1", "    }"]
        Origem = "x"
    elif B.Tipo[0] == "i":
        Origem = f"transmute(v, to = {Armazem})"
    Gravar = f"    p.{B.Armazenamento} = (p.{B.Armazenamento} & {Mantem}) | (({Origem} & {Mascara}) << {B.Deslocamento})"
    return "\n".join([f"pub proc {Proc}_set(p: ^{Nome}, v: {B.Tipo}) {{"] + Antes + [Gravar, "}"])


def TextoFlexivel(Nome: str, Proc: str, Elemento: str, Offset: int) -> str:
    return "\n".join([f"pub proc {Proc}_ptr(p: ^{Nome}) -> ^{Elemento} {{",
                      f"    let b = slice_from((p as rawptr) as ^u8, len = {Offset + 1})",
                      f"    return ((&b[{Offset}]) as rawptr) as ^{Elemento}", "}"])


def PedidosDeAjudante(C: Coleta) -> list:
    """(campo C, nomes dos procs, textos) de cada bitfield (get+set) e de cada array flexivel (ptr)."""
    Base = NomeSnake(C.Nome)
    Pedidos = []
    for B in C.Bitfields:
        Proc = f"{Base}_{NomeSnake(B.Campo)}"
        Pedidos.append((B.Campo, [Proc + "_get", Proc + "_set"], [TextoBitfieldLer(C.Nome, Proc, B), TextoBitfieldGravar(C.Nome, Proc, B)]))
    for Campo, Tipo, Offset in C.Flexiveis:
        Proc = f"{Base}_{NomeSnake(Campo)}"
        Pedidos.append((Campo, [Proc + "_ptr"], [TextoFlexivel(C.Nome, Proc, Tipo, Offset)]))
    return Pedidos


def AjudantesDaStruct(C: Coleta) -> None:
    """Procs get/set por bitfield e _ptr por array flexivel; nome ja usado vira recusa G0013 so desse ajudante."""
    for Campo, Procs, Textos in PedidosDeAjudante(C):
        if ReservarProcs(C, Campo, Procs):
            C.M.Ajudantes += [(PosicaoDe(C.Decl), Texto) for Texto in Textos]


def ReservarProcs(C: Coleta, Campo: str, Procs: list) -> bool:
    try:
        for Proc in Procs:
            ReservarNome(C.M, Proc, f"{C.NomeC}.{Campo}")
    except ErroTipo as Erro:
        Recusar(C.M, C.Decl, f"{C.NomeC}.{Campo}", Erro)
        return False
    return True


def ConstruirStructEspecial(M: Modelo, Decl, Nome: str, NomeC: str) -> None:
    C = ColetarMembros(M, Decl, Nome, NomeC)
    Offsets, Tamanho, Alinhamento = LayoutNatural(M, [X.Tipo for X in C.Membros])
    for X, Natural in zip(C.Membros, Offsets):
        if X.Offset != Natural:
            raise ErroTipo("G0007", f"a struct '{NomeC}' nao tem o layout natural do C (campo '{X.Nome}' "
                                    f"no offset {X.Offset}, natural {Natural})")
    if Decl.type.get_size() != Tamanho:
        raise ErroTipo("G0007", f"a struct '{NomeC}' nao tem o layout natural do C (campo '(tamanho)' "
                                f"no offset {Decl.type.get_size()}, natural {Tamanho})")
    M.Layouts[Nome] = (Tamanho, Alinhamento)
    Medidas = [(X.Nome, X.NomeC, X.Offset) for X in C.Membros]
    M.Estruturas.append(Struct(PosicaoDe(Decl), Nome, [(X.Nome, X.Tipo) for X in C.Membros],
                               (TipoCDoLayout(M, Decl), Tamanho, Alinhamento, Medidas)))
    AjudantesDaStruct(C)


def ColetarAcesso(M: Modelo, Decl, Nome: str, Cur) -> None:
    try:
        Tipo = MapearTipo(M, Cur.type, "campo")
    except ErroTipo:
        return  # membro que nao mapeia fica sem acesso, sem recusa propria
    if Tipo in TIPOS_C_LONG or re.search(r"\bc_u?long\b", Tipo):
        return  # 0.7.0: o acessor seria pub proc com ^c_long (E0342): sem acesso, igual ao membro que nao mapeia (0.10.0: tambem ponteiro para long, ^?^c_long)
    M.Acessos.append(Acesso(PosicaoDe(Cur), Nome, Cur.spelling, Tipo, Cur.location.line, Cur.location.column))


def NomeDoAcesso(M: Modelo, A: Acesso) -> str:
    """<union>_<membro>; se ja existe outro nome igual (funcao C nk_handle_ptr, const, outro acessador), <union>__campo_<membro>."""
    Natural = f"{NomeSnake(A.Union)}_{NomeSnake(A.Membro)}"
    if Natural not in M.Nomes:
        return Natural
    return f"{NomeSnake(A.Union)}__campo_{NomeSnake(A.Membro)}"


def ResolverAcessos(M: Modelo) -> None:
    """v1.10 (A9): decide o nome de cada acessador so depois da varredura inteira (a funcao C pode vir antes ou depois da union)."""
    Prontos = []
    for A in sorted(M.Acessos, key=lambda X: X.Pos):
        Nome = NomeDoAcesso(M, A)
        if Nome in M.Nomes:
            M.Recusas.append(Recusa("G0013", os.path.basename(A.Pos[0]), A.Linha, A.Coluna, f"{A.Union}.{A.Membro}",
                                    f"o acessador '{Nome}' colide com '{M.Nomes[Nome]}'"))
            continue
        M.Nomes[Nome] = f"{A.Union}.{A.Membro}"
        A.Texto = f"pub proc {Nome}(u: ^{A.Union}) -> ^{A.Tipo} {{\n    return (u as rawptr) as ^{A.Tipo}\n}}"
        Prontos.append(A)
    M.Acessos = Prontos


# ---------------------------------------------------------------- macros e consts
def TipoInteiro(Valor: int):
    if -2**31 <= Valor < 2**31:
        return "i32"
    if -2**63 <= Valor < 2**63:
        return "i64"
    return "u64" if 0 <= Valor < 2**64 else None


def GuardarConst(M: Modelo, Cur, Nome: str, Tipo: str, Valor: str) -> None:
    """Uma const por nome. v1.10 (A8): #define repetido com o mesmo valor e legal em C e vale uma vez; com outro valor, G0013."""
    NomeConst = Nome if re.fullmatch(r"[A-Z][A-Z0-9_]*", Nome) else NomeSnake(Nome).upper()
    try:
        ReservarNome(M, NomeConst, Nome)
    except ErroTipo as Erro:
        Recusar(M, Cur, Nome, Erro)
        return
    Anterior = M.ConstsPorNome.get(NomeConst)
    if Anterior == (Tipo, Valor):
        return
    if Anterior is not None:
        Recusar(M, Cur, Nome, ErroTipo("G0013", f"a macro '{Nome}' foi redefinida com outro valor ({Anterior[1]} e {Valor})"))
        return
    M.ConstsPorNome[NomeConst] = (Tipo, Valor)
    M.Consts.append(Const(PosicaoDe(Cur), NomeConst, Tipo, Valor))


def AdicionarConst(M: Modelo, Cur, Nome: str, Valor: int) -> None:
    Tipo = TipoInteiro(Valor)
    if Tipo is None:
        return
    GuardarConst(M, Cur, Nome, Tipo, str(Valor))


def LerInteiro(Token: str):
    Achado = re.fullmatch(r"(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*", Token)
    if not Achado:
        return None
    Corpo = Achado.group(1)
    Base = 16 if Corpo[:2] in ("0x", "0X") else (8 if len(Corpo) > 1 and Corpo[0] == "0" else 10)
    try:
        return int(Corpo, Base)
    except ValueError:
        return None


def TextoReal(Token: str, Negativo: bool):
    Valor = float(Token.rstrip("fF"))
    if math.isinf(Valor):
        return None
    Texto = format(Decimal(repr(Valor)), "f")
    Texto = Texto if "." in Texto else Texto + ".0"
    return ("f32" if Token[-1] in "fF" else "f64"), ("-" if Negativo else "") + Texto


def SemParenteses(Corpo: list) -> list:
    while len(Corpo) >= 2 and Corpo[0] == "(" and Corpo[-1] == ")":
        Corpo = Corpo[1:-1]
    return Corpo


def ValorDaMacro(Corpo: list):
    """Tokens do corpo -> (tipo, texto) ou None se nao e literal inteiro, real ou texto simples."""
    Corpo = SemParenteses(Corpo)
    Negativo = len(Corpo) == 2 and Corpo[0] == "-"
    if len(Corpo) != 1 and not Negativo:
        return None
    Literal = Corpo[-1]
    if not Negativo and LITERAL_TEXTO.fullmatch(Literal):
        return "cstr", Literal
    Inteiro = LerInteiro(Literal)
    if Inteiro is not None:
        Valor = -Inteiro if Negativo else Inteiro
        Tipo = TipoInteiro(Valor)
        return (Tipo, str(Valor)) if Tipo else None
    if LITERAL_REAL.fullmatch(Literal) and re.search(r"[.eE]", Literal):
        return TextoReal(Literal, Negativo)
    return None


def LerMacro(M: Modelo, Cur) -> None:
    Nome = Cur.spelling
    if Nome.startswith("_") or not NomeAscii(Nome) or not Nome.startswith(M.Prefixo) or not SimboloPedido(M, Nome):
        return
    Tokens = list(Cur.get_tokens())
    if len(Tokens) < 2:
        return
    Parametros = Tokens[1].spelling == "(" and Tokens[1].extent.start.offset == Tokens[0].extent.end.offset
    Valor = None if Parametros else ValorDaMacro([T.spelling for T in Tokens[1:]])
    if Valor is not None:
        GuardarConst(M, Cur, Nome, Valor[0], Valor[1])
    elif Parametros:
        RegistrarMacroFuncao(M, Cur, Nome)
    else:
        M.Candidatas.append((Cur, Nome, [T.spelling for T in Tokens[1:]]))  # v1.10 (A7): a libclang tenta avaliar (AvaliarMacros)


def RegistrarMacroFuncao(M: Modelo, Cur, Nome: str) -> None:
    """0.6.0: macro-funcao com assinatura no .sig espera a cola (ColarMacros); sem assinatura segue G0004."""
    if Nome in M.Sigs:
        M.MacrosPedidas.append((Cur, Nome))
    else:
        Recusar(M, Cur, Nome, ErroTipo("G0004", CausaDaMacro(Nome)))


def CausaDaMacro(Nome: str) -> str:
    return f"a macro '{Nome}' nao e literal inteiro, real ou texto (macro com parametros ou expressao)"


# ---------------------------------------------------------------- v1.10 (A7): macro com expressao constante, avaliada pela libclang
TIPOS_INTEIROS_CLANG = ("BOOL", "CHAR_S", "SCHAR", "CHAR_U", "UCHAR", "SHORT", "USHORT", "INT", "UINT", "LONG", "ULONG",
                        "LONGLONG", "ULONGLONG", "ENUM")


def CorpoPodeSerExpressao(Corpo: list) -> bool:
    """Filtro barato antes de pedir ao clang: sem { } ; e com parenteses balanceados (um corpo torto derrubaria as linhas seguintes)."""
    Fundo = 0
    for Token in Corpo:
        if Token in ("{", "}", ";"):
            return False
        Fundo += (Token == "(") - (Token == ")")
        if Fundo < 0:
            return False
    return Fundo == 0


def RegistrarFuncoesDeAvaliacao():
    """clang_Cursor_Evaluate nao tem ligacao no clang.cindex do Python: registra os tipos de C (ctypes) uma vez. None = indisponivel."""
    import ctypes
    Lib = Ci.conf.lib
    try:
        Lib.clang_Cursor_Evaluate.argtypes = [Ci.Cursor]
        Lib.clang_Cursor_Evaluate.restype = ctypes.c_void_p
        for Nome, Retorno in (("getKind", ctypes.c_int), ("getAsLongLong", ctypes.c_longlong), ("getAsUnsigned", ctypes.c_ulonglong),
                              ("isUnsignedInt", ctypes.c_uint), ("getAsDouble", ctypes.c_double)):
            Funcao = getattr(Lib, f"clang_EvalResult_{Nome}")
            Funcao.argtypes = [ctypes.c_void_p]
            Funcao.restype = Retorno
        Lib.clang_EvalResult_dispose.argtypes = [ctypes.c_void_p]
    except AttributeError:
        return None
    return Lib


def TextoDoReal(Valor: float, Simples: bool):
    """Menor decimal que volta ao mesmo f32 (ou o repr do f64), sem expoente; None se infinito ou NaN."""
    if math.isinf(Valor) or math.isnan(Valor):
        return None
    Texto = repr(Valor)
    if Simples:
        for Casas in range(6, 10):
            Curto = f"{Valor:.{Casas}g}"
            if struct.unpack("f", struct.pack("f", float(Curto)))[0] == Valor:
                Texto = Curto
                break
    Texto = format(Decimal(Texto), "f")
    return Texto if "." in Texto else Texto + ".0"


def ValorAvaliado(Lib, Var):
    """(tipo Forja, texto) do valor constante da variavel de prova, ou None se o clang nao consegue dobrar."""
    Tipo = Var.type.get_canonical().kind
    Inteiro = Tipo.name in TIPOS_INTEIROS_CLANG
    if not Inteiro and Tipo not in (Ci.TypeKind.FLOAT, Ci.TypeKind.DOUBLE):
        return None
    Resultado = Lib.clang_Cursor_Evaluate(Var)
    if not Resultado:
        return None
    try:
        Especie = Lib.clang_EvalResult_getKind(Resultado)
        if Especie == 1 and Inteiro:  # CXEval_Int
            Sem = Lib.clang_EvalResult_isUnsignedInt(Resultado)
            Valor = Lib.clang_EvalResult_getAsUnsigned(Resultado) if Sem else Lib.clang_EvalResult_getAsLongLong(Resultado)
            Forja = TipoInteiro(Valor)
            return (Forja, str(Valor)) if Forja else None
        if Especie == 2 and not Inteiro:  # CXEval_Float
            Simples = Tipo == Ci.TypeKind.FLOAT
            Texto = TextoDoReal(Lib.clang_EvalResult_getAsDouble(Resultado), Simples)
            return ("f32" if Simples else "f64", Texto) if Texto else None
    finally:
        Lib.clang_EvalResult_dispose(Resultado)
    return None


def LinhasComErro(Unidade, Arquivo: str) -> set:
    return {D.location.line for D in Unidade.diagnostics
            if D.severity >= Ci.Diagnostic.Error and D.location.file and os.path.basename(D.location.file.name) == Arquivo}


def AvaliarMacros(M: Modelo) -> None:
    """Macro de objeto que nao e literal (casts, ~, sizeof, PI / 180.0f, outra macro): uma linha de prova
    'static const __typeof__((M)) __fl_k = (M);' num arquivo virtual que inclui o header; o clang dobra a constante.
    Sem valor constante, a macro continua recusada com G0004 como antes."""
    Provaveis = [C for C in M.Candidatas if CorpoPodeSerExpressao(C[2])]
    Valores: dict = {}
    Lib = RegistrarFuncoesDeAvaliacao() if Provaveis else None
    if Lib is not None:
        Nome = "__forja_ligar_prova.c"
        Arquivo = os.path.join(M.PastaHeader, Nome)
        Linhas = [f'#include "{M.Header}"'] + [f"static const __typeof__(({C[1]})) __fl_{K} = ({C[1]});" for K, C in enumerate(Provaveis)]
        try:
            Unidade = Ci.Index.create().parse(Arquivo, args=M.ArgumentosClang, unsaved_files=[(Arquivo, "\n".join(Linhas) + "\n")],
                                              options=OpcoesDoClang())
        except Ci.TranslationUnitLoadError:
            Unidade = None
        if Unidade is not None:
            Erros = LinhasComErro(Unidade, Nome)
            for Var in Unidade.cursor.get_children():
                if Var.kind != Ci.CursorKind.VAR_DECL or not Var.spelling.startswith("__fl_") or Var.location.line in Erros:
                    continue
                Achado = ValorAvaliado(Lib, Var)
                if Achado:
                    Valores[int(Var.spelling[5:])] = Achado
    Indices = {id(C): K for K, C in enumerate(Provaveis)}
    for C in M.Candidatas:
        Achado = Valores.get(Indices.get(id(C)))
        if Achado:
            GuardarConst(M, C[0], C[1], Achado[0], Achado[1])
        else:
            Recusar(M, C[0], C[1], ErroTipo("G0004", CausaDaMacro(C[1])))
    M.Candidatas = []


# ---------------------------------------------------------------- funcoes e variaveis
def ValidarNomeDaFuncao(M: Modelo, Nome: str) -> None:
    if Nome in RESERVADAS:
        raise ErroTipo("G0013", f"o nome '{Nome}' e palavra da Forja")
    if Nome.startswith(("fj_", "forja_")):
        raise ErroTipo("G0013", f"o nome '{Nome}' comeca com fj_ ou forja_")
    if Nome == "main":
        raise ErroTipo("G0013", f"o nome '{Nome}' e main")
    ReservarNome(M, Nome, Nome)


def ParametrosDaFuncao(M: Modelo, Cur) -> str:
    Itens = []
    Usados: set = set()
    for Indice, Arg in enumerate(Cur.get_arguments()):
        M.CursorAtual = Arg
        Nome = NomeDoParametro(Arg.spelling, Indice, Usados)
        Itens.append(f"{Nome}: {MapearTipo(M, Arg.type, 'parametro')}")
    return ", ".join(Itens)


def NomeDoProc(M: Modelo, Nome: str) -> str:
    """Nome Forja do proc foreign. v1.10 (A6): palavra da Forja (remove, get, has, insert...) vira o alias c_<nome>; o simbolo C fica entre aspas."""
    if Nome in RESERVADAS:
        return ReservarNome(M, f"c_{Nome}", Nome)
    ValidarNomeDaFuncao(M, Nome)
    return Nome


def AssinaturaDoProc(M: Modelo, Nome: str, Forja: str, Parametros: str, Resultado) -> Proc:
    Sufixo = ""
    if Resultado.get_canonical().kind != Ci.TypeKind.VOID:
        Sufixo = " -> " + MapearTipo(M, Resultado, "retorno")
    Alias = f' = "{Nome}"' if Forja != Nome else ""
    return Proc(f"    proc {Forja}{Alias}({Parametros}){Sufixo}")


# ---------------------------------------------------------------- v1.10 (A11): wrapper de parametros fixos para variadica
def EhFormatoC(Tipo) -> bool:
    """O ultimo parametro fixo de printf e companhia: const char *."""
    Canonico = Tipo.get_canonical()
    if Canonico.kind != Ci.TypeKind.POINTER:
        return False
    Apontado = Canonico.get_pointee()
    return Apontado.is_const_qualified() and Apontado.kind in (Ci.TypeKind.CHAR_S, Ci.TypeKind.SCHAR)


def MontarWrapperVariadica(M: Modelo, Cur) -> Proc:
    """F(p0, ..., const char *fmt, ...) vira F_texto(p0, ..., cstr texto): o texto ja formatado entra por "%s"."""
    Nome = Cur.spelling
    Fixos = list(Cur.get_arguments())
    if not Fixos or not EhFormatoC(Fixos[-1].type):
        raise ErroTipo("G0008", f"a funcao '{Nome}' e variadica e o ultimo parametro fixo nao e const char * (formato)")
    NomeWrapper = f"{Nome}_texto"
    ValidarNomeDaFuncao(M, NomeWrapper)
    Itens, ParamsC, Chamada = [], [], []
    Usados: set = {"texto"}
    for Indice, Arg in enumerate(Fixos[:-1]):
        M.CursorAtual = Arg
        Itens.append(f"{NomeDoParametro(Arg.spelling, Indice, Usados)}: {MapearTipo(M, Arg.type, 'parametro')}")
        ParamsC.append(f"{Arg.type.spelling} a{Indice}")
        Chamada.append(f"a{Indice}")
    Itens.append("texto: cstr")
    Proc = AssinaturaDoProc(M, NomeWrapper, NomeWrapper, ", ".join(Itens), Cur.result_type)  # pode recusar o tipo: antes de gravar o C
    Retorno = "" if Cur.result_type.get_canonical().kind == Ci.TypeKind.VOID else "return "
    ParamsC.append("const char *texto")
    Argumentos = ", ".join(Chamada + ['"%s"', "texto"])
    M.Variadicas.append(f"{Cur.result_type.spelling} {NomeWrapper}({', '.join(ParamsC)})\n{{\n    {Retorno}{Nome}({Argumentos});\n}}")
    return Proc


def MontarProc(M: Modelo, Cur) -> Proc:
    Nome = Cur.spelling
    if Cur.storage_class == Ci.StorageClass.STATIC or Cur.is_definition():
        if not M.Cola:
            raise ErroTipo("G0005", f"'{Nome}' nao tem simbolo exportado (variavel global ou funcao static/inline)")
        if Cur.type.is_function_variadic():
            raise ErroTipo("G0008", f"a funcao '{Nome}' e variadica")
        return MontarProcCola(M, Cur)
    if Cur.type.is_function_variadic():
        if M.WrapperVariadicas:
            return MontarWrapperVariadica(M, Cur)
        raise ErroTipo("G0008", f"a funcao '{Nome}' e variadica")
    Forja = NomeDoProc(M, Nome)
    return AssinaturaDoProc(M, Nome, Forja, ParametrosDaFuncao(M, Cur), Cur.result_type)


def ColetarFuncao(M: Modelo, Cur) -> None:
    Nome = Cur.spelling
    if Nome in M.Vistos or not Nome.startswith(M.Prefixo) or not SimboloPedido(M, Nome):
        return
    M.Vistos.add(Nome)
    try:
        M.Procs.append(MontarProc(M, Cur))
    except ErroTipo as Erro:
        Recusar(M, Cur, Nome, Erro)


def ColetarVariavel(M: Modelo, Cur) -> None:
    Nome = Cur.spelling
    if Nome in M.Vistos or not Nome.startswith(M.Prefixo) or not SimboloPedido(M, Nome):
        return
    M.Vistos.add(Nome)
    try:
        if not M.Cola:
            raise ErroTipo("G0005", f"'{Nome}' nao tem simbolo exportado (variavel global ou funcao static/inline)")
        ColarVariavel(M, Cur)
    except ErroTipo as Erro:
        Recusar(M, Cur, Nome, Erro)


# ---------------------------------------------------------------- 0.6.0 (F1e): cola C
PREFIXO_COLA = "fjcola_"
PALAVRAS_DE_TIPO_C = frozenset("void char short int long float double signed unsigned _Bool const volatile struct union enum".split())
LINHA_SIG = re.compile(r"^([A-Za-z_]\w*)\s*\((.*)\)\s*(?:->\s*(.+?))?$")
PROVA_MACROS = "__forja_ligar_macros.c"
AJUDA = """forja ligar @VERSAO@ - cola C (--cola, --compilar-cola, --macros)

  --cola              static inline, variavel global e (com .sig) macro-funcao viram wrappers nao-inline em <lib>_cola.c:
                        funcao      proc nome = "fjcola_nome"(...)          (mesmo nome, tipos e parametros do original)
                        variavel    <nome>_get() e <nome>_set(v)             (set so se nao for const; array global: so o get, que devolve o
                                                                              ponteiro do 1o elemento) = fjcola_get_<nome>, fjcola_set_<nome>
                        macro       proc NOME = "fjcola_NOME"(...)           (so com entrada no .sig)
                      A ponte ganha 'link "<lib>_cola"'. Funcao variadica continua G0008.
  --compilar-cola     compila a cola agora: gcc -O2 -c + ar rcs lib<lib>_cola.a (gcc e ar do PATH, mesmos -I e -D do ligar); erro do gcc
                      vira G0103 com o texto do gcc. O 'forja build' continua sem toolchain. Implica --cola.
  --propor            (modo C, por arquivo) grava creates / creates p / destroys p nos procs que casam a heuristica fechada do D-M6c-9
                      (open|create|new|init|load|parse|dup|alloc, close|destroy|free|delete|release|unload|finalize), com a linha
                      '// proposto por forja ligar --propor: confira'; nunca propoe takes. Sem --propor a saida e a de sempre.
  --macros x.sig      assinaturas das macro-funcoes (G0004) que o usuario quer ligar. Implica --cola.

Formato do .sig (UTF-8; uma macro por linha; vazias e comentarios '//' ignorados):
    NOME(tipo p1, tipo p2) -> tipo
  - tipo e tipo C (o mesmo texto vai para o wrapper): 'float', 'Vector2', 'const char *', 'unsigned int'...; cada parametro tem tipo e nome.
  - '-> tipo' e opcional; sem ele o retorno e void. Macro sem parametros: NOME() -> int.
  - linha '#include <x.h>' (ou "x.h") e copiada para a cola, depois do include do header: para a macro que usa funcao que o header nao inclui.
  - ponteiro para funcao, '...' e va_list nao entram (linha invalida: G0026). A macro tem que existir no header como macro-funcao;
    sem linha no .sig ela segue '// recusado ... G0004'. O ligar NAO infere tipo de macro.
  exemplo:   Clamp(float value, float lo, float hi) -> float
             Vector2Add(Vector2 a, Vector2 b) -> Vector2
"""


@dataclass
class AssinaturaMacro:
    """Uma linha do .sig: macro-funcao NOME(tipo p1, ...) -> tipo, com os textos de tipo C que o usuario deu."""
    Nome: str
    Arquivo: str
    Linha: int
    Retorno: str      # texto C ('void' quando a linha nao tem '->')
    Parametros: list  # (texto do tipo C, nome)


def LerParametroDaMacro(Texto: str, Contexto: str) -> tuple:
    """'const char *s' -> ('const char *', 's'); ErroTipo G0026 se faltar nome, tipo ou se for ponteiro de funcao."""
    if "(" in Texto or ")" in Texto or "..." in Texto:
        raise ErroTipo("G0026", f"{Contexto}: parametro '{Texto.strip()}' (ponteiro de funcao e '...' nao entram no .sig)")
    Achado = re.fullmatch(r"(.*?[\s*])\s*([A-Za-z_]\w*)", Texto.strip())
    if not Achado or Achado.group(2) in PALAVRAS_DE_TIPO_C or Achado.group(1).split()[-1:] in (["struct"], ["union"], ["enum"]):
        raise ErroTipo("G0026", f"{Contexto}: parametro '{Texto.strip()}' sem tipo ou sem nome (formato 'tipo nome')")
    return Achado.group(1).strip(), Achado.group(2)


def AssinaturaDaLinha(Linha: str, Arquivo: str, Numero: int) -> AssinaturaMacro:
    """'NOME(tipo p1, tipo p2) -> tipo' -> AssinaturaMacro; ErroTipo G0026 se a linha nao segue o formato."""
    Contexto = f"linha {Numero} do .sig"
    Achado = LINHA_SIG.match(Linha)
    if not Achado:
        raise ErroTipo("G0026", f"{Contexto} nao e 'NOME(tipo p1, tipo p2) -> tipo': {Linha}")
    Nome, Lista, Retorno = Achado.group(1), Achado.group(2).strip(), (Achado.group(3) or "void").strip()
    Textos = [] if Lista in ("", "void") else Lista.split(",")
    Parametros = [LerParametroDaMacro(T, Contexto) for T in Textos]
    if "(" in Retorno or "..." in Retorno or "va_list" in Retorno:
        raise ErroTipo("G0026", f"{Contexto}: retorno '{Retorno}' nao entra no .sig")
    return AssinaturaMacro(Nome, Arquivo, Numero, Retorno, Parametros)


def LerAssinaturasDeMacro(Caminho: str) -> tuple:
    """(assinaturas por nome, linhas '#include' do .sig, recusas G0026) do arquivo .sig; G0104 se nao deu para ler."""
    try:
        with open(Caminho, encoding="utf-8") as Arquivo:
            Linhas = Arquivo.read().splitlines()
    except OSError as Erro:
        raise FalhaFerramenta("G0104", f"nao foi possivel ler o arquivo de macros '{Caminho}' ({Erro})")
    L = LeituraSig(os.path.basename(Caminho))
    for Numero, Bruta in enumerate(Linhas, 1):
        Linha = Bruta.strip() if Bruta.strip().startswith("#") else Bruta.split("//", 1)[0].strip()
        if Linha:
            AcrescentarLinhaDoSig(L, Linha, Numero)
    return L.Sigs, L.Includes, L.Recusas


@dataclass
class LeituraSig:
    Arquivo: str
    Sigs: dict = field(default_factory=dict)
    Includes: list = field(default_factory=list)
    Recusas: list = field(default_factory=list)


def AcrescentarLinhaDoSig(L: LeituraSig, Linha: str, Numero: int) -> None:
    """Uma linha util do .sig: #include, ou assinatura de macro; linha invalida ou repetida vira recusa G0026 (as outras seguem)."""
    try:
        if Linha.startswith("#"):
            L.Includes.append(IncludeDaLinha(Linha, Numero))
            return
        Sig = AssinaturaDaLinha(Linha, L.Arquivo, Numero)
        if Sig.Nome in L.Sigs:
            raise ErroTipo("G0026", f"linha {Numero} do .sig repete a macro '{Sig.Nome}' (ja na linha {L.Sigs[Sig.Nome].Linha})")
        L.Sigs[Sig.Nome] = Sig
    except ErroTipo as Erro:
        L.Recusas.append(Recusa(Erro.Codigo, L.Arquivo, Numero, 1, Linha, Erro.Causa))


def IncludeDaLinha(Linha: str, Numero: int) -> str:
    """Linha '#include <x.h>' ou '#include "x.h"' do .sig (a macro pode usar uma funcao que o header nao inclui); G0026 se for outra diretiva."""
    if not re.fullmatch(r'#\s*include\s*(<[^<>"]+>|"[^<>"]+")', Linha):
        raise ErroTipo("G0026", f"linha {Numero} do .sig: so a diretiva #include e aceita ({Linha})")
    return Linha


def TipoParaC(Tipo) -> str:
    """Texto C do tipo para o wrapper; ponteiro de funcao e array saem por __typeof__(...) (que aceita o nome de tipo abstrato)."""
    Texto = Tipo.spelling
    if "va_list" in Texto:
        raise ErroTipo("G0001", f"o tipo C '{Texto}' (va_list) nao atravessa a Forja")
    return f"__typeof__({Texto})" if ("(" in Texto or "[" in Texto) else Texto


def TipoDeRetornoC(Tipo) -> str:
    return "void" if Tipo.get_canonical().kind == Ci.TypeKind.VOID else TipoParaC(Tipo)


def BlocoCola(Retorno: str, Simbolo: str, Parametros: list, Chamada: str) -> str:
    """Wrapper nao-inline: 'RETORNO simbolo(T a0, ...) { return CHAMADA; }' (Retorno 'void' sai sem return)."""
    Corpo = f"{Chamada};" if Retorno == "void" else f"return {Chamada};"
    return f"{Retorno} {Simbolo}({', '.join(Parametros) or 'void'})\n{{\n    {Corpo}\n}}"


def BlocoDaFuncao(Cur, Simbolo: str) -> str:
    Tipos = [TipoParaC(Arg.type) for Arg in Cur.get_arguments()]
    Parametros = [f"{T} a{I}" for I, T in enumerate(Tipos)]
    Chamada = f"{Cur.spelling}({', '.join(f'a{I}' for I in range(len(Tipos)))})"
    return BlocoCola(TipoDeRetornoC(Cur.result_type), Simbolo, Parametros, Chamada)


def ProcComSimbolo(M: Modelo, Forja: str, Simbolo: str, Assinatura: tuple) -> Proc:
    """proc <Forja> = "<Simbolo>"(...) [-> R]: o nome que a Forja usa e o simbolo da cola. Assinatura = (texto dos parametros, tipo C do retorno)."""
    Parametros, Resultado = Assinatura
    Sufixo = ""
    if Resultado.get_canonical().kind != Ci.TypeKind.VOID:
        Sufixo = " -> " + MapearTipo(M, Resultado, "retorno")
    return Proc(f'    proc {Forja} = "{Simbolo}"({Parametros}){Sufixo}')


def NomeForjaDaCola(M: Modelo, Nome: str) -> str:
    """Nome Forja do wrapper: o nome C em snake_case (a Forja so aceita proc com alias em snake_case: E0102); G0013 se colide."""
    Candidato = NomeLivre(NomeSnake(Nome))
    if Candidato.startswith(("fj_", "forja_")) or Candidato == "main":
        raise ErroTipo("G0013", f"o nome '{Candidato}' comeca com fj_ ou forja_, ou e main")
    return ReservarNome(M, Candidato, Nome)


def MontarProcCola(M: Modelo, Cur) -> Proc:
    """Funcao static/inline (G0005 sem --cola): a ponte declara fjcola_<nome>, que a cola define chamando a original."""
    Nome = Cur.spelling
    Forja = NomeForjaDaCola(M, Nome)
    Simbolo = PREFIXO_COLA + Nome
    Proc_ = ProcComSimbolo(M, Forja, Simbolo, (ParametrosDaFuncao(M, Cur), Cur.result_type))
    M.BlocosCola.append(BlocoDaFuncao(Cur, Simbolo))
    return Proc_


def ElementoDaGlobalArray(M: Modelo, Elemento, Constante: bool) -> str:
    """Tipo Forja do ponteiro para o 1o elemento de um array global: const char -> ?cstr, char -> ?^i8, T -> ?^T."""
    if Elemento.get_canonical().kind == Ci.TypeKind.CHAR_S:
        return "?cstr" if Constante else "?^i8"
    return "?^" + MapearTipo(M, Elemento, "interno")


def TipoDoPonteiroDeArray(Cur) -> tuple:
    """(elemento, const?, texto C do ponteiro para o 1o elemento) de um array global; a libclang perde o const no tipo canonico."""
    Tipo = Cur.type if Cur.type.kind in (Ci.TypeKind.CONSTANTARRAY, Ci.TypeKind.INCOMPLETEARRAY) else Cur.type.get_canonical()
    Elemento = Tipo.get_array_element_type()
    Constante = Elemento.is_const_qualified() or Cur.type.get_canonical().is_const_qualified()
    Texto = TipoParaC(Elemento)
    return Elemento, Constante, ("const " if Constante and not Texto.startswith("const ") else "") + Texto + " *"


def TipoCDoGetter(Cur) -> str:
    """Texto C do tipo devolvido: sem o const de cima ('const int' devolve 'int': qualificador no retorno nao vale nada em C)."""
    Texto = TipoParaC(Cur.type)
    if Cur.type.get_canonical().is_const_qualified():
        Texto = re.sub(r"^const\s+|\s+const$", "", Texto)
    return Texto


def ProcGetterDaGlobal(M: Modelo, Cur, TipoForja: str, TipoC: str) -> tuple:
    Nome = Cur.spelling
    Forja = ReservarNome(M, f"{NomeLivre(NomeSnake(Nome))}_get", Nome)
    Simbolo = f"{PREFIXO_COLA}get_{Nome}"
    return Proc(f'    proc {Forja} = "{Simbolo}"() -> {TipoForja}'), BlocoCola(TipoC, Simbolo, [], Nome)


def ProcSetterDaGlobal(M: Modelo, Cur, TipoForja: str, TipoC: str) -> tuple:
    Nome = Cur.spelling
    Forja = ReservarNome(M, f"{NomeLivre(NomeSnake(Nome))}_set", Nome)
    Simbolo = f"{PREFIXO_COLA}set_{Nome}"
    return (Proc(f'    proc {Forja} = "{Simbolo}"(v: {TipoForja})'),
            f"void {Simbolo}({TipoC} v)\n{{\n    {Nome} = v;\n}}")


def ParesDaGlobal(M: Modelo, Cur) -> list:
    """[(Proc, bloco C)] do getter e, se a variavel nao for const nem array, do setter. ErroTipo se o tipo nao mapeia."""
    Canonico = Cur.type.get_canonical()
    M.CursorAtual = Cur
    if Canonico.kind in (Ci.TypeKind.CONSTANTARRAY, Ci.TypeKind.INCOMPLETEARRAY):
        Elemento, Constante, TipoC = TipoDoPonteiroDeArray(Cur)
        return [ProcGetterDaGlobal(M, Cur, ElementoDaGlobalArray(M, Elemento, Constante), TipoC)]
    Pares = [ProcGetterDaGlobal(M, Cur, MapearTipo(M, Cur.type, "retorno"), TipoCDoGetter(Cur))]
    if not Canonico.is_const_qualified():
        TipoC = TipoParaC(Cur.type)
        Contexto = "campo" if "(*)" in TipoC else "parametro"  # ponteiro de funcao vira rawptr, como no retorno
        Pares.append(ProcSetterDaGlobal(M, Cur, MapearTipo(M, Cur.type, Contexto), TipoC))
    return Pares


def ColarVariavel(M: Modelo, Cur) -> None:
    for Proc_, Bloco in ParesDaGlobal(M, Cur):
        M.Procs.append(Proc_)
        M.BlocosCola.append(Bloco)


def PrototipoDaMacro(S: AssinaturaMacro) -> str:
    Lista = ", ".join(f"{T} {N}" for T, N in S.Parametros) or "void"
    return f"{S.Retorno} {PREFIXO_COLA}{S.Nome}({Lista});"


def BlocoDaMacro(S: AssinaturaMacro) -> str:
    Lista = [f"{T} {N}" for T, N in S.Parametros]
    return BlocoCola(S.Retorno, PREFIXO_COLA + S.Nome, Lista, f"{S.Nome}({', '.join(N for _, N in S.Parametros)})")


def DeclararMacrosNoClang(M: Modelo, Nomes: list) -> tuple:
    """Parseia o prototipo de cada macro num arquivo virtual que inclui o header: a libclang entrega os tipos ja resolvidos.
    Devolve (cursor do prototipo por nome da macro, texto do erro do clang por nome)."""
    Arquivo = os.path.join(M.PastaHeader, PROVA_MACROS)
    Primeira = 2 + len(M.IncludesSig)  # linha do 1o prototipo: #include do header + os do .sig
    Linhas = [f'#include "{M.Header}"'] + M.IncludesSig + [PrototipoDaMacro(M.Sigs[N]) for N in Nomes]
    try:
        Unidade = Ci.Index.create().parse(Arquivo, args=M.ArgumentosClang, unsaved_files=[(Arquivo, "\n".join(Linhas) + "\n")],
                                          options=OpcoesDoClang())
    except Ci.TranslationUnitLoadError as Erro:
        return {}, {N: str(Erro) for N in Nomes}
    Textos = {D.location.line: D.spelling for D in Unidade.diagnostics
              if D.severity >= Ci.Diagnostic.Error and D.location.file and os.path.basename(D.location.file.name) == PROVA_MACROS}
    Problemas = {N: Textos[K + Primeira] for K, N in enumerate(Nomes) if K + Primeira in Textos}
    Cursores = {C.spelling[len(PREFIXO_COLA):]: C for C in Unidade.cursor.get_children()
                if C.kind == Ci.CursorKind.FUNCTION_DECL and C.spelling.startswith(PREFIXO_COLA) and C.location.line not in Textos}
    return Cursores, Problemas


def MontarProcDaMacro(M: Modelo, Prototipo, Sig: AssinaturaMacro) -> Proc:
    for Arg in Prototipo.get_arguments():
        TipoParaC(Arg.type)  # so para recusar va_list
    Forja = NomeForjaDaCola(M, Sig.Nome)
    Proc_ = ProcComSimbolo(M, Forja, Prototipo.spelling, (ParametrosDaFuncao(M, Prototipo), Prototipo.result_type))
    M.BlocosCola.append(BlocoDaMacro(Sig))
    return Proc_


def RecusaDaSig(M: Modelo, Sig: AssinaturaMacro, Causa: str) -> None:
    M.Recusas.append(Recusa("G0026", Sig.Arquivo, Sig.Linha, 1, Sig.Nome, Causa))


def ColarMacros(M: Modelo) -> None:
    """Macro-funcao com entrada no .sig vira wrapper (a macro e expandida pelo compilador C na cola); o que o .sig lista e o header nao
    tem como macro-funcao vira G0026."""
    Pedidas: dict = {}
    for Cur, Nome in M.MacrosPedidas:
        Pedidas.setdefault(Nome, Cur)
    for Nome, Sig in M.Sigs.items():
        if Nome not in Pedidas:
            RecusaDaSig(M, Sig, f"o header nao define '{Nome}' como macro-funcao (na pasta do header e dentro do --prefixo)")
    if not Pedidas:
        return
    Cursores, Problemas = DeclararMacrosNoClang(M, list(Pedidas))
    for Nome, Cur in Pedidas.items():
        if Nome in Problemas or Nome not in Cursores:
            RecusaDaSig(M, M.Sigs[Nome], f"a assinatura de '{Nome}' nao compila contra o header: {Problemas.get(Nome, 'sem prototipo')}")
            continue
        try:
            M.Procs.append(MontarProcDaMacro(M, Cursores[Nome], M.Sigs[Nome]))
        except ErroTipo as Erro:
            Recusar(M, Cur, Nome, Erro)


MODELO_COLA = """// gerado por 'forja ligar' @VERSAO@ a partir de @HEADER@; nao edite a mao (gere de novo).
// Cola C (--cola): wrapper nao-inline de funcao static/inline, getter/setter de variavel global e macro-funcao com assinatura (.sig).
// gcc -O2 -c @FLAGS@ @BASE@_cola.c -o @BASE@_cola.o && ar rcs lib@BASE@_cola.a @BASE@_cola.o
#include "@HEADER@"
"""


def FlagsDaCola(A, Header: str) -> list:
    """-I e -D da cola: a pasta do header (quando o header veio por caminho), os -I do usuario e os -D."""
    Pasta = [f"-I{os.path.dirname(Header)}"] if os.path.isfile(A.entrada) else []
    return Pasta + [f"-I{P}" for P in A.incs] + [f"-D{D}" for D in A.defs]


def EmitirCola(M: Modelo, A, Header: str, Base: str) -> str:
    Cabecalho = (MODELO_COLA.replace("@VERSAO@", VERSAO).replace("@HEADER@", os.path.basename(Header))
                 .replace("@BASE@", Base).replace("@FLAGS@", " ".join(FlagsDaCola(A, Header))))
    Includes = "".join(f"{Linha}\n" for Linha in M.IncludesSig)
    return Cabecalho + Includes + "\n" + "\n\n".join(M.BlocosCola) + "\n"


def RodarFerramentaDaCola(Comando: list) -> None:
    """Roda gcc/ar; G0103 com o texto da ferramenta dentro da mensagem se faltar ou falhar (nunca no stdout do ligar)."""
    try:
        Rodada = subprocess.run(Comando, capture_output=True, timeout=600)
    except (OSError, subprocess.SubprocessError) as Erro:
        raise FalhaFerramenta("G0103", f"cola C: {Comando[0]} nao rodou ({Erro}); --compilar-cola precisa de gcc e ar no PATH")
    if Rodada.returncode != 0:
        Texto = (Rodada.stderr + Rodada.stdout).decode("utf-8", "replace").strip()
        raise FalhaFerramenta("G0103", f"cola C: {Comando[0]} falhou (exit {Rodada.returncode}): {Texto[:2000]}")


def CompilarCola(Fonte: str, Base: str, Flags: list) -> None:
    """gcc -O2 -c + ar rcs lib<Base>_cola.a ao lado da fonte; o .o intermediario some."""
    Pasta = os.path.dirname(Fonte)
    Objeto = os.path.join(Pasta, f"{Base}_cola.o")
    Biblioteca = os.path.join(Pasta, f"lib{Base}_cola.a")
    if os.path.exists(Biblioteca):
        os.remove(Biblioteca)  # ar rcs acumula membros; a cola nova substitui a antiga inteira
    RodarFerramentaDaCola(["gcc", "-O2", "-c"] + Flags + [Fonte, "-o", Objeto])
    RodarFerramentaDaCola(["ar", "rcs", Biblioteca, Objeto])
    os.remove(Objeto)


def GravarCola(M: Modelo, A, Header: str, Saida: str) -> None:
    """Grava <link>_cola.c ao lado da ponte (e compila com --compilar-cola); sem wrapper nenhum, nao grava e a ponte fica sem o link."""
    if not M.BlocosCola:
        return
    Base = re.sub(r"\W", "_", A.link)
    Fonte = os.path.join(os.path.dirname(os.path.abspath(Saida)), f"{Base}_cola.c")
    M.LinkCola = f"{Base}_cola"
    GravarArquivo(Fonte, EmitirCola(M, A, Header, Base))
    if A.compilar_cola:
        CompilarCola(Fonte, Base, FlagsDaCola(A, Header))


# ---------------------------------------------------------------- varredura do header
def NoEscopo(M: Modelo, Cur) -> bool:
    Arquivo = Cur.location.file
    if Arquivo is None:
        return False
    Pasta = os.path.dirname(os.path.normcase(os.path.abspath(Arquivo.name)))
    try:
        return os.path.commonpath([M.PastaHeader, Pasta]) == M.PastaHeader
    except ValueError:  # discos diferentes
        return False


def ColetarTypedefs(M: Modelo, Unidade) -> None:
    K = Ci.CursorKind
    for Cur in Unidade.cursor.get_children():
        if Cur.kind != K.TYPEDEF_DECL:
            continue
        Decl = Cur.underlying_typedef_type.get_canonical().get_declaration()
        if Decl.kind in (K.STRUCT_DECL, K.UNION_DECL, K.ENUM_DECL) and not NomeDoTipo(M, Decl):
            M.NomesTypedef.setdefault(Decl.get_usr(), Cur.spelling)


def ColetarRecordDeTopo(M: Modelo, Cur) -> None:
    NomeC = NomeDoTipo(M, Cur)
    if NomeC and NomeC.startswith(M.Prefixo) and M.Simbolos is None:
        TentarTipo(M, Cur, ConstruirRecord)


def Despachar(M: Modelo, Cur) -> None:
    K = Ci.CursorKind
    if Cur.kind == K.FUNCTION_DECL:
        ColetarFuncao(M, Cur)
    elif Cur.kind == K.VAR_DECL:
        ColetarVariavel(M, Cur)
    elif Cur.kind == K.MACRO_DEFINITION:
        LerMacro(M, Cur)
    elif Cur.kind == K.ENUM_DECL and Cur.is_definition():
        ColetarEnum(M, Cur)
    elif Cur.kind in (K.STRUCT_DECL, K.UNION_DECL) and Cur.is_definition():
        ColetarRecordDeTopo(M, Cur)


def Coletar(M: Modelo, Unidade) -> None:
    ColetarTypedefs(M, Unidade)
    for Cur in Unidade.cursor.get_children():
        if NoEscopo(M, Cur):
            Despachar(M, Cur)


# ---------------------------------------------------------------- texto do modulo
def TextoEnum(E: Enum) -> str:
    Linhas = [f"    {N} = {V}" if E.ComValor else f"    {N}" for N, V in E.Membros]
    return "\n".join([f"pub enum {E.Nome} {{"] + Linhas + ["}"])


def TextoStruct(S: Struct) -> str:
    return "\n".join([f"pub struct {S.Nome} {{"] + [f"    {N}: {T}" for N, T in S.Campos] + ["}"])


def TipoDependeDeCLong(M: Modelo, Tipo: str, Visitados: set) -> bool:
    """0.7.1: o tipo guarda c_long/c_ulong por valor (direto, em array ou em struct aninhada). Ponteiro tem tamanho fixo: nao conta."""
    Vetor = re.fullmatch(r"\[\d+\](.+)", Tipo)
    if Vetor:
        return TipoDependeDeCLong(M, Vetor.group(1), Visitados)
    if Tipo in TIPOS_C_LONG:
        return True
    if Tipo in Visitados or Tipo[:1] in ("?", "^") or Tipo in ("rawptr", "cstr"):
        return False
    Visitados.add(Tipo)
    return any(TipoDependeDeCLong(M, T, Visitados) for E in M.Estruturas if E.Nome == Tipo for _, T in E.Campos)


def TextoLayout(M: Modelo, S: Struct) -> str:
    """0.5.0 (F1a): comentario '// layout' com os numeros da libclang (a bateria --layout le) e static_assert que a Forja confere a cada build.
    0.7.1: struct com c_long/c_ulong (o tamanho segue o alvo) fica sem static_assert; o comentario fica para a bateria --layout."""
    if S.Layout is None:
        return ""
    TipoC, Tamanho, Alinhamento, Campos = S.Layout
    Linha = f"// layout {S.Nome} {TipoC.replace(' ', '~')} tamanho={Tamanho} alinhamento={Alinhamento}"
    Linhas = [Linha + "".join(f" {F}:{C}={O}" for F, C, O in Campos)]
    if TipoDependeDeCLong(M, S.Nome, set()):
        Linhas.append("// 0.7.1: struct com c_long/c_ulong: o tamanho segue o alvo (4 no Windows, 8 no Linux), sem static_assert; "
                      "a prova nos 2 alvos e tests\\nativo\\m6c\\clong\\gzheader_layout.fj")
        return "\n".join(Linhas)
    Linhas.append(f'static_assert(size_of({S.Nome}) == {Tamanho}, msg = "layout de {S.Nome}: tamanho difere da libclang")')
    Linhas.append(f'static_assert(align_of({S.Nome}) == {Alinhamento}, msg = "layout de {S.Nome}: alinhamento difere da libclang")')
    Linhas += [f'static_assert(offset_of({S.Nome}.{F}) == {O}, msg = "layout de {S.Nome}: offset de {F} difere da libclang")' for F, _, O in Campos]
    return "\n".join(Linhas)


def TextoForeign(M: Modelo, Link: str) -> str:
    if not M.Procs and not M.Opacos:
        return ""
    Linhas = ["pub foreign {"]
    if M.LinkVariadicas:  # o wrapper vem antes da lib que ele chama (ordem do ld com .a)
        Linhas.append(f'    link "{M.LinkVariadicas}"')
    if M.LinkCola:  # a cola pode chamar a lib: vem antes dela (ordem do ld com .a)
        Linhas.append(f'    link "{M.LinkCola}"')
    Linhas += [f'    link "{Link}"'] + [f"    opaque {N}" for N in M.Opacos]
    Procs = [TextoComProposta(M, P.Texto) for P in M.Procs] if M.Propor else [P.Texto for P in M.Procs]
    return "\n".join(Linhas + Procs + ["}"])


# ---------------------------------------------------------------- 0.8.0 (M6c P89): --propor
CRIA_NOMES = ("open", "create", "new", "init", "load", "parse", "dup", "alloc")
DESTROI_NOMES = ("close", "destroy", "free", "delete", "release", "unload", "finalize")
FIM_NOMES = ("shutdown", "global_cleanup", "library_end")
PROPOSTA = "    // proposto por forja ligar --propor: confira"
CABECA_PROC = re.compile(r'^    proc (\w+)(?: = "(\w+)")?\(')


def ParametrosESaida(Texto: str, Inicio: int) -> tuple:
    """Divide 'a: T, b: proc(x: i32) -> i32) -> R' no ')' que fecha a lista (callback aninhado nao conta): (parametros, resto)."""
    Nivel, Itens, Atual = 0, [], ""
    for Posicao in range(Inicio, len(Texto)):
        Letra = Texto[Posicao]
        if Letra == ")" and Nivel == 0:
            return Itens + ([Atual.strip()] if Atual.strip() else []), Texto[Posicao + 1:]
        Nivel += (Letra == "(") - (Letra == ")")
        if Letra == "," and Nivel == 0:
            Itens.append(Atual.strip())
            Atual = ""
        else:
            Atual += Letra
    return Itens, ""


def EhHandle(Tipo: str) -> bool:
    return Tipo.startswith(("^", "?^")) or Tipo == "rawptr"


def ClausulaProposta(Nomes: list, Parametros: list, Resto: str) -> str:
    """Clausula do D-M6c-9 para o proc (nomes Forja e C em minusculas, sem o prefixo) ou '' quando a heuristica nao casa."""
    if not Parametros and any(N in FIM_NOMES for N in Nomes):
        return " ends"
    Tipos = [P.split(":", 1)[1].strip() for P in Parametros]
    if Parametros and EhHandle(Tipos[0]) and any(D in N for N in Nomes for D in DESTROI_NOMES):
        return f" destroys {Parametros[0].split(':', 1)[0].strip()}"
    if not any(C in N for N in Nomes for C in CRIA_NOMES):
        return ""
    Retorno = Resto.replace("->", "", 1).strip()
    if Retorno and EhHandle(Retorno):
        return " creates"
    for Parametro, Tipo in zip(Parametros, Tipos):
        if Tipo.startswith(("?^?^", "^?^", "^^", "?^^")):
            return f" creates {Parametro.split(':', 1)[0].strip()}"
    return ""


def TextoComProposta(M: Modelo, Texto: str) -> str:
    Cabeca = CABECA_PROC.match(Texto)
    if not Cabeca:
        return Texto
    Pfx = M.Prefixo.lower()
    Nomes = [N.lower()[len(Pfx):] if N.lower().startswith(Pfx) else N.lower() for N in Cabeca.groups() if N]
    Parametros, Resto = ParametrosESaida(Texto, Cabeca.end())
    if any(":" not in P for P in Parametros):
        return Texto
    Clausula = ClausulaProposta(Nomes, Parametros, Resto)
    if Clausula.startswith(" creates "):  # o E0912 de 'creates p' exige o parametro de saida ^H (nao anulavel): ?^?^T vira ^?^T
        Saida = Clausula.split()[1]
        Texto = re.sub(rf"(?<!\w){re.escape(Saida)}: \?\^", f"{Saida}: ^", Texto, count=1)  # 0.8.1: nome inteiro (rdb: nao casa com db:)
    return f"{PROPOSTA}\n{Texto}{Clausula}" if Clausula else Texto


def TextoRecusas(M: Modelo) -> str:
    """0.4.0 (F1d): uma linha '// recusado NOME Gxxxx causa' por recusa, para a ponte dizer o que nao ligou."""
    return "\n".join(f"// recusado {R.Simbolo or '-'} {R.Codigo} {' '.join(R.Causa.split())}" for R in M.Recusas)


def EmitirModulo(M: Modelo, NomeHeader: str, Link: str) -> str:
    ResolverAcessos(M)
    Tipos = len(M.Enums) + len(M.Estruturas) + len(M.Opacos)
    Acessos = len(M.Acessos) + len(M.Ajudantes)
    Total = len(M.Procs) + Tipos + len(M.Consts) + Acessos
    Cabecalho = (f"// gerado por 'forja ligar' {VERSAO} a partir de {NomeHeader}; nao edite a mao (gere de novo).\n"
                 f"// gerados: {Total} (procs {len(M.Procs)}, tipos {Tipos}, consts {len(M.Consts)}, acessos {Acessos}); "
                 f"recusados: {len(M.Recusas)}")
    if M.LinkCola:
        Cabecalho += f"\n// cola C: {M.LinkCola}.c ({len(M.BlocosCola)} wrappers; --compilar-cola gera lib{M.LinkCola}.a)"
    Blocos = [TextoEnum(E) for E in sorted(M.Enums, key=lambda E: E.Pos)]
    Blocos += [TextoStruct(S) for S in sorted(M.Estruturas, key=lambda S: S.Pos)]
    Blocos += [TextoLayout(M, S) for S in sorted(M.Estruturas, key=lambda S: S.Pos)]
    if M.Consts:
        Blocos.append("\n".join(f"pub const {C.Nome}: {C.Tipo} = {C.Valor}" for C in sorted(M.Consts, key=lambda C: C.Pos)))
    Blocos.append(TextoForeign(M, Link))
    Blocos += [A.Texto for A in sorted(M.Acessos, key=lambda A: A.Pos)]
    Blocos += [Texto for _, Texto in sorted(M.Ajudantes, key=lambda X: X[0])]
    Blocos.append(TextoRecusas(M))
    return Cabecalho + "\n\n" + "\n\n".join(B for B in Blocos if B) + "\n"


def GravarArquivo(Caminho: str, Texto: str) -> None:
    Temporario = Caminho + ".tmp"
    with open(Temporario, "w", encoding="utf-8", newline="\n") as Saida:
        Saida.write(Texto)
    os.replace(Temporario, Caminho)


# ---------------------------------------------------------------- modo C++ (wrapper extern "C" em <saida>_c.cpp)
# Cabecalho fixo do wrapper; @NOME@ e trocado em EmitirWrapperCpp. fl_excecao escreve o R0010 e encerra com 101.
MODELO_WRAPPER = r"""// gerado por 'forja ligar' @VERSAO@ a partir de @HEADER@; nao edite a mao (gere de novo).
// g++ -std=c++17 -O2 -shared -static-libgcc -static-libstdc++ -I @PASTA@ -o @LIB@.dll <seus .cpp> @BASE@_c.cpp
#include "@HEADER@"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

static void fl_texto_json(const char *texto)
{
    for (; *texto != '\0'; texto++) {
        unsigned char ch = (unsigned char)*texto;
        if (ch == '"' || ch == '\\') std::fprintf(stderr, "\\%c", ch);
        else if (ch < 0x20) std::fprintf(stderr, "\\u%04x", ch);
        else std::fputc(ch, stderr);
    }
}

[[noreturn]] static void fl_excecao(const char *onde, int linha, const char *texto)
{
    std::fprintf(stderr, "{\"codigo\": \"R0010\", \"tipo\": \"runtime\", \"arquivo\": \"@HEADER@\", \"linha\": %d, \"causa\": \"excecao C++ em ", linha);
    fl_texto_json(onde);
    std::fputs(": ", stderr);
    fl_texto_json(texto);
    std::fputs("\"}\n", stderr);
    std::exit(101);
}
"""
TIPOS_FUNCAO_CPP = ("FUNCTION_DECL", "FUNCTION_TEMPLATE")


@dataclass
class ClasseCpp:
    Nome: str   # Contador
    Qual: str   # teste_cpp::Contador
    Pfx: str    # teste_cpp_


def EPublico(Cur) -> bool:
    return Cur.access_specifier == Ci.AccessSpecifier.PUBLIC


def TipoCpp(T, Contexto: str) -> str:
    """Tipo C++ -> tipo Forja; so o que existe em C atravessa (escalar, void*, char*, ponteiro de escalar). Resto = G0023."""
    C = T.get_canonical()
    if C.kind in ESCALARES:
        return ESCALARES[C.kind]
    if C.kind == Ci.TypeKind.POINTER:
        Pontudo = C.get_pointee().get_canonical()
        if Pontudo.kind == Ci.TypeKind.VOID:
            return "rawptr"
        if Pontudo.kind == Ci.TypeKind.CHAR_S:
            return MapearPonteiroChar(Pontudo, Contexto)
        if Pontudo.kind in ESCALARES:
            return "?^" + ESCALARES[Pontudo.kind]
    raise ErroTipo("G0023", f"'{{S}}': tipo nao C '{T.spelling}'")


def ParametrosCpp(Cur, Reservados: set) -> tuple:
    """(itens Forja 'a: T', itens C++ 'T a', nomes) dos parametros; Reservados = nomes que o wrapper ja usa."""
    Forja, Cpp, Nomes = [], [], []
    Usados = set(Reservados)
    for Indice, Arg in enumerate(Cur.get_arguments()):
        Nome = NomeDoParametro(Arg.spelling, Indice, Usados)
        Forja.append(f"{Nome}: {TipoCpp(Arg.type, 'parametro')}")
        Cpp.append(f"{Arg.type.spelling} {Nome}")
        Nomes.append(Nome)
    return Forja, Cpp, Nomes


def BlocoWrapper(Retorno: str, Nome: str, Params: list, Expressao: str, Onde: str, Linha: int, Devolve: bool) -> str:
    Devolucao = "return " if Devolve else ""
    return "\n".join([
        f'extern "C" __declspec(dllexport) {Retorno} {Nome}({", ".join(Params)})',
        "{",
        f"    try {{ {Devolucao}{Expressao}; }}",
        f'    catch (const std::exception &e) {{ fl_excecao("{Onde}", {Linha}, e.what()); }}',
        f'    catch (...) {{ fl_excecao("{Onde}", {Linha}, "excecao desconhecida"); }}',
        "}"])


def ItemDeFuncao(M: Modelo, Cur, Nome: str, Chamada: str, Onde: str, Classe) -> tuple:
    """Funcao livre (Classe None) ou metodo: (linha 'proc' do foreign, bloco do wrapper). ErroTipo se algum tipo nao e C."""
    ValidarNomeDaFuncao(M, Nome)
    Forja, Cpp, Nomes = ParametrosCpp(Cur, {"objeto"} if Classe else set())
    Resultado = Cur.result_type
    Void = Resultado.get_canonical().kind == Ci.TypeKind.VOID
    Sufixo = "" if Void else " -> " + TipoCpp(Resultado, "retorno")
    if Classe:
        Forja.insert(0, f"objeto: ?^{Classe.Nome}")
        Cpp.insert(0, f"{Classe.Qual} *objeto")
    Expressao = f"{Chamada}({', '.join(Nomes)})"
    Bloco = BlocoWrapper("void" if Void else Resultado.spelling, Nome, Cpp, Expressao, Onde, Cur.location.line, not Void)
    return f"    proc {Nome}({', '.join(Forja)}){Sufixo}", Bloco


def ProibirTemplate(Cur) -> None:
    if Cur.kind in (Ci.CursorKind.FUNCTION_TEMPLATE, Ci.CursorKind.CLASS_TEMPLATE):
        raise ErroTipo("G0022", "'{S}' e template")


def ProibirSobrecarga(Repeticoes: int) -> None:
    if Repeticoes > 1:
        raise ErroTipo("G0021", "'{S}' e sobrecarregado")


def ProibirConstrucaoDoMetodo(Cur) -> None:
    if Cur.kind == Ci.CursorKind.CONVERSION_FUNCTION or Cur.spelling.startswith("operator"):
        raise ErroTipo("G0023", "'{S}': operador")
    if Cur.is_static_method():
        raise ErroTipo("G0023", "'{S}': metodo estatico")
    if Cur.is_virtual_method():
        raise ErroTipo("G0023", "'{S}': metodo virtual")


def ItemDoMetodo(M: Modelo, Cur, Classe: ClasseCpp, Repeticoes: int) -> tuple:
    ProibirTemplate(Cur)
    ProibirConstrucaoDoMetodo(Cur)
    ProibirSobrecarga(Repeticoes)
    Nome = f"{Classe.Pfx}{Classe.Nome}_{Cur.spelling}"
    return ItemDeFuncao(M, Cur, Nome, f"objeto->{Cur.spelling}", f"{Classe.Qual}::{Cur.spelling}", Classe)


def ItemConstrutor(M: Modelo, Filhos: list, Classe: ClasseCpp) -> tuple:
    Publicos = [F for F in Filhos if F.kind == Ci.CursorKind.CONSTRUCTOR and EPublico(F)]
    if len(Publicos) != 1:
        raise ErroTipo("G0023", "'{S}': precisa de exatamente 1 construtor publico")
    Nome = f"{Classe.Pfx}{Classe.Nome}_new"
    ValidarNomeDaFuncao(M, Nome)
    Forja, Cpp, Nomes = ParametrosCpp(Publicos[0], set())
    Bloco = BlocoWrapper(f"{Classe.Qual} *", Nome, Cpp, f"new {Classe.Qual}({', '.join(Nomes)})",
                         f"{Classe.Qual}::{Classe.Nome}", Publicos[0].location.line, True)
    return f"    proc {Nome}({', '.join(Forja)}) -> ?^{Classe.Nome}", Bloco


def ItemDestrutor(M: Modelo, Cur, Filhos: list, Classe: ClasseCpp) -> tuple:
    Destrutor = next((F for F in Filhos if F.kind == Ci.CursorKind.DESTRUCTOR), Cur)
    Nome = f"{Classe.Pfx}{Classe.Nome}_delete"
    ValidarNomeDaFuncao(M, Nome)
    Bloco = BlocoWrapper("void", Nome, [f"{Classe.Qual} *objeto"], "delete objeto", f"{Classe.Qual}::~{Classe.Nome}",
                         Destrutor.location.line, False)
    return f"    proc {Nome}(objeto: ?^{Classe.Nome})", Bloco


def ConstrucaoProibidaDaClasse(Filho) -> str:
    K = Ci.CursorKind
    if Filho.kind == K.CXX_BASE_SPECIFIER:
        return "heranca"
    if Filho.kind in (K.CLASS_DECL, K.STRUCT_DECL, K.CLASS_TEMPLATE):
        return "classe aninhada"
    if Filho.kind == K.FIELD_DECL and EPublico(Filho):
        return "campo publico"
    return ""


def VerificarClasse(Filhos: list) -> None:
    for Filho in Filhos:
        Proibida = ConstrucaoProibidaDaClasse(Filho)
        if Proibida:
            raise ErroTipo("G0023", f"'{{S}}': {Proibida}")


def ItensDosMetodos(M: Modelo, Filhos: list, Classe: ClasseCpp) -> list:
    K = Ci.CursorKind
    Metodos = [F for F in Filhos if F.kind in (K.CXX_METHOD, K.FUNCTION_TEMPLATE, K.CONVERSION_FUNCTION) and EPublico(F)]
    Contagem = Counter(F.spelling for F in Metodos)
    Itens = []
    for Metodo in Metodos:
        try:
            Itens.append(ItemDoMetodo(M, Metodo, Classe, Contagem[Metodo.spelling]))
        except ErroTipo as Erro:
            Recusar(M, Metodo, f"{Classe.Qual}::{Metodo.spelling}", Erro)
    return Itens


def MontarClasse(M: Modelo, Cur, Classe: ClasseCpp) -> list:
    """Itens (proc, bloco) da classe: new, delete e os metodos aceitos. ErroTipo = a classe inteira sai."""
    Filhos = list(Cur.get_children())
    VerificarClasse(Filhos)
    ReservarNome(M, Classe.Nome, Classe.Qual)
    Itens = [ItemConstrutor(M, Filhos, Classe), ItemDestrutor(M, Cur, Filhos, Classe)]
    return Itens + ItensDosMetodos(M, Filhos, Classe)


def AdicionarItens(M: Modelo, Itens: list) -> None:
    for Proc_, Bloco in Itens:
        M.Procs.append(Proc(Proc_))
        M.Wrappers.append(Bloco)


def ColetarClasse(M: Modelo, Cur, Ns: list) -> None:
    Classe = ClasseCpp(Cur.spelling, "::".join(Ns + [Cur.spelling]), "".join(N + "_" for N in Ns))
    if not (Classe.Pfx + Classe.Nome).startswith(M.Prefixo):
        return
    try:
        Itens = MontarClasse(M, Cur, Classe)
    except ErroTipo as Erro:
        Recusar(M, Cur, Classe.Qual, Erro)
        return
    M.Opacos.append(Classe.Nome)
    AdicionarItens(M, Itens)


def ColetarLivre(M: Modelo, Cur, Ns: list, Contagem: Counter) -> None:
    Usr = Cur.get_usr()
    Pfx = "".join(N + "_" for N in Ns)
    if Usr in M.Vistos or not (Pfx + Cur.spelling).startswith(M.Prefixo):
        return
    M.Vistos.add(Usr)
    Qual = "::".join(Ns + [Cur.spelling])
    try:
        ProibirTemplate(Cur)
        ProibirSobrecarga(Contagem[Cur.spelling])
        Item = ItemDeFuncao(M, Cur, Pfx + Cur.spelling, Qual, Qual, None)
    except ErroTipo as Erro:
        Recusar(M, Cur, Qual, Erro)
        return
    AdicionarItens(M, [Item])


def ContarNomes(Filhos: list) -> Counter:
    """Quantas declaracoes distintas (por USR) cada nome de funcao tem no escopo: >1 = sobrecarga."""
    Unicos = {(C.spelling, C.get_usr()) for C in Filhos if C.kind.name in TIPOS_FUNCAO_CPP}
    return Counter(Nome for Nome, _ in Unicos)


def ColetarEscopoCpp(M: Modelo, Pai, Ns: list) -> None:
    K = Ci.CursorKind
    Filhos = [C for C in Pai.get_children() if NoEscopo(M, C)]
    Contagem = ContarNomes(Filhos)
    for Cur in Filhos:
        if Cur.kind == K.NAMESPACE and Cur.spelling:
            ColetarEscopoCpp(M, Cur, Ns + [Cur.spelling])
        elif Cur.kind in (K.CLASS_DECL, K.STRUCT_DECL) and Cur.is_definition():
            ColetarClasse(M, Cur, Ns)
        elif Cur.kind == K.CLASS_TEMPLATE:
            Recusar(M, Cur, "::".join(Ns + [Cur.spelling]), ErroTipo("G0022", "'{S}' e template"))
        elif Cur.kind.name in TIPOS_FUNCAO_CPP:
            ColetarLivre(M, Cur, Ns, Contagem)


def EmitirWrapperCpp(M: Modelo, Header: str, Link: str, Base: str) -> str:
    Trocas = {"@VERSAO@": VERSAO, "@HEADER@": os.path.basename(Header), "@PASTA@": os.path.dirname(Header),
              "@LIB@": Link, "@BASE@": Base}
    Texto = MODELO_WRAPPER
    for Marca, Valor in Trocas.items():
        Texto = Texto.replace(Marca, Valor)
    return Texto + "\n" + "\n\n".join(M.Wrappers) + "\n"


# ---------------------------------------------------------------- modo --dll (exportacoes do PE + arquivo de assinaturas)
LINHA_OPACO = re.compile(r"^opaque ([A-Z][A-Za-z0-9]*)$")
LINHA_PROC = re.compile(r"^proc ([A-Za-z_][A-Za-z0-9_]*)\(.*\)( -> .+)?$")


def LerExportacoes(Dll: str) -> list:
    """Nomes exportados pela DLL na ordem da tabela de exportacao; G0105 se pefile falta ou a DLL nao e PE valida."""
    try:
        import pefile
    except ImportError:
        raise FalhaFerramenta("G0105", "modulo pefile nao encontrado (pip install --user pefile)")
    try:
        Pe = pefile.PE(Dll, fast_load=True)
        Pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        return [S.name.decode("ascii") for S in Pe.DIRECTORY_ENTRY_EXPORT.symbols if S.name]
    except (OSError, pefile.PEFormatError, AttributeError):
        raise FalhaFerramenta("G0105", f"'{os.path.basename(Dll)}' nao e uma DLL PE valida")


def ClassificarLinhaDll(M: Modelo, Numero: int, Linha: str, Exportados: list, Contexto: tuple) -> None:
    """Uma linha do arquivo de assinaturas: opaque, proc (confere o nome na DLL) ou G0026."""
    ArquivoSig, NomeDll = Contexto
    Opaco = LINHA_OPACO.match(Linha)
    if Opaco:
        M.Opacos.append(Opaco.group(1))
        return
    Proc_ = LINHA_PROC.match(Linha)
    if not Proc_:
        Causa = f"linha {Numero} nao e proc, opaque, comentario nem vazia: {Linha}"
        M.Recusas.append(Recusa("G0026", ArquivoSig, Numero, 1, Linha, Causa))
    elif Proc_.group(1) not in Exportados:
        Causa = f"a DLL '{NomeDll}' nao exporta '{Proc_.group(1)}'"
        M.Recusas.append(Recusa("G0025", ArquivoSig, Numero, 1, Proc_.group(1), Causa))
    else:
        M.Procs.append(Proc("    " + re.sub(r" +", " ", Linha)))


def LerAssinaturas(Caminho: str, Exportados: list, NomeDll: str) -> Modelo:
    """Le o .sig (sintaxe do foreign, um item por linha) e devolve o Modelo com procs, opacos e recusas G0025/G0026."""
    M = Modelo("", "")
    try:
        with open(Caminho, encoding="utf-8") as Arquivo:
            Linhas = Arquivo.read().splitlines()
    except OSError as Erro:
        raise FalhaFerramenta("G0104", f"nao foi possivel ler o arquivo de assinaturas '{Caminho}' ({Erro})")
    for Numero, Bruta in enumerate(Linhas, 1):
        Linha = Bruta.strip()
        if Linha and not Linha.startswith("//"):
            ClassificarLinhaDll(M, Numero, Linha, Exportados, (os.path.basename(Caminho), NomeDll))
    return M


def EmitirModuloDll(M: Modelo, Dll: str, Assinaturas: str, Exportados: list) -> str:
    """Modulo do --dll: o de EmitirModulo mais a linha 3 com os exportados que nenhum proc aceito usa."""
    Usados = {P.Texto.split("(")[0].split()[-1] for P in M.Procs}
    Sem = len([N for N in Exportados if N not in Usados])
    Origem = f"{os.path.basename(Dll)} + {os.path.basename(Assinaturas)}"
    Link = os.path.splitext(os.path.basename(Dll))[0]
    Linha3 = f"// exportados sem assinatura: {Sem} (forja ligar --dll {os.path.basename(Dll)} --listar)"
    return EmitirModulo(M, Origem, Link).replace("\n\n", "\n" + Linha3 + "\n\n", 1)


def GerarModuloDll(A) -> int:
    Exportados = LerExportacoes(A.entrada)
    if A.listar:
        print("\n".join(Exportados))
        return 0
    NomeDll = os.path.basename(A.entrada)
    M = LerAssinaturas(A.assinaturas, Exportados, NomeDll)
    Saida = A.saida or os.path.splitext(NomeDll)[0] + ".fj"
    GravarArquivo(Saida, EmitirModuloDll(M, A.entrada, A.assinaturas, Exportados))
    for R in M.Recusas:
        EscreverRecusa(R)
    return 1 if (A.estrito and M.Recusas) else 0


# ---------------------------------------------------------------- principal
def GerarModuloCpp(A) -> int:
    CarregarLibclang()
    PastasGpp = IncludesDoCompilador("g++", "c++")
    Header = AcharHeader(A.entrada, A.incs, PastasGpp)
    Unidade = AbrirUnidade(Header, list(A.incs) + PastasGpp, A.defs, True)
    M = Modelo(A.prefixo, os.path.dirname(os.path.normcase(Header)))
    ColetarEscopoCpp(M, Unidade.cursor, [])
    Base = os.path.splitext(A.saida)[0]
    GravarArquivo(Base + "_c.cpp", EmitirWrapperCpp(M, Header, A.link, os.path.basename(Base)))
    Origem = f"{os.path.basename(Header)} (C++ por wrapper C em {os.path.basename(Base)}_c.cpp)"
    GravarArquivo(A.saida, EmitirModulo(M, Origem, A.link))
    for R in M.Recusas:
        EscreverRecusa(R)
    return 1 if (A.estrito and M.Recusas) else 0


MODELO_VARIADICAS = """// gerado por 'forja ligar' @VERSAO@ a partir de @HEADER@; nao edite a mao (gere de novo).
// Wrapper de parametros fixos das funcoes variadicas (--wrapper-variadicas): o texto ja formatado entra por "%s".
// gcc -O2 -c -I <pasta do header> @BASE@_variadicas.c -o @BASE@_variadicas.o && ar rcs lib@BASE@_variadicas.a @BASE@_variadicas.o
#include "@HEADER@"
"""


def EmitirVariadicas(M: Modelo, Header: str, Base: str) -> str:
    Cabecalho = MODELO_VARIADICAS.replace("@VERSAO@", VERSAO).replace("@HEADER@", os.path.basename(Header)).replace("@BASE@", Base)
    return Cabecalho + "\n" + "\n\n".join(M.Variadicas) + "\n"


def GerarModuloC(A) -> int:
    CarregarLibclang()
    PastasGcc = IncludesDoCompilador("gcc")
    Header = AcharHeader(A.entrada, A.incs, PastasGcc)
    HEADERS_EXTRAS[:] = ([Header] if A.incluir else []) + [AcharHeader(Extra, A.incs, PastasGcc) for Extra in A.incluir]  # o principal entra 1o: os extras costumam precisar dos tipos dele
    Argumentos = ArgumentosDoClang(list(A.incs) + PastasGcc, A.defs)
    Unidade = AbrirUnidade(Header, list(A.incs) + PastasGcc, A.defs)
    M = Modelo(A.prefixo, os.path.dirname(os.path.normcase(Header)))
    M.Header, M.ArgumentosClang, M.WrapperVariadicas = Header, Argumentos, A.wrapper_variadicas
    M.Cola, M.Propor = UsaCola(A), A.propor
    M.Simbolos = LerSimbolosPedidos(A.simbolos)
    if A.macros:
        M.Sigs, M.IncludesSig, Invalidas = LerAssinaturasDeMacro(A.macros)
        M.Recusas += Invalidas
    Coletar(M, Unidade)
    AvaliarMacros(M)
    ColarMacros(M)
    Saida = A.saida or os.path.splitext(os.path.basename(Header))[0] + ".fj"
    if M.Variadicas:
        Base = os.path.splitext(os.path.basename(Saida))[0]
        M.LinkVariadicas = f"{Base}_variadicas"
        GravarArquivo(os.path.splitext(Saida)[0] + "_variadicas.c", EmitirVariadicas(M, Header, Base))
    GravarCola(M, A, Header, Saida)
    GravarArquivo(Saida, EmitirModulo(M, os.path.basename(Header), A.link))
    for R in M.Recusas:
        EscreverRecusa(R)
    return 1 if (A.estrito and M.Recusas) else 0


def Principal(Argv=None) -> int:
    Analisador = CriarAnalisador()
    A = Analisador.parse_args(Argv)
    ValidarArgumentos(A, Analisador)
    if A.codigos:
        print("\n".join(CODIGOS))
        return 0
    if A.ajuda:
        print(AJUDA.replace("@VERSAO@", VERSAO))
        return 0
    try:
        if A.dll:
            return GerarModuloDll(A)
        return GerarModuloCpp(A) if A.cpp else GerarModuloC(A)
    except FalhaFerramenta as Falha:
        EscreverFalha(Falha.Codigo, Falha.Causa)
        return 2


if __name__ == "__main__":
    sys.exit(Principal())
