# -*- coding: utf-8 -*-
"""Confere as regras de C da casa (limites para agente de IA, CROSS-R05 / Emenda 5 do contrato_ia1) no backend nativo.

Escopo: compiler\\nativo\\** (todo o backend), compiler\\ir\\layout.* (layout passou a ir\ no G3 G0b), compiler\\cache\\*, os arquivos do orquestrador em driver\\ (lista literal ArquivosDoOrquestrador) e runtime\\nativo\\* (o rt que vai dentro do exe).
Por arquivo: linhas (> 800).
Por funcao (Alvos / Tetos):
  - linhas        alvo 60, teto 100
  - cognitiva     alvo 15, teto 25   (conferir_regras_cognitiva.py: +1 por if/else if/else/laco/ternario, +1 por sequencia de &&/||
                                      do mesmo operador e +1 a cada troca de operador, mais o nivel de aninhamento; switch inteiro conta 1)
  - niveis        alvo 3,  teto 4    (maximo de construcoes de controle if/for/while/do/switch aninhadas ao redor de uma instrucao, com ou sem chaves)
  - parametros    alvo 4,  teto 7    (so posicionais em C; 5 a 7 exigem 'LIMITE: parametros <motivo>')
  Acima do teto = violacao. Entre alvo e teto exige o comentario 'LIMITE: <metrica> <motivo>' (metricas: linhas, cognitiva, niveis,
  parametros) na janela do cabecalho da funcao (3 linhas acima do cabecalho ate o fim da funcao).
  - DividaRegras (conferir_regras_divida.py): lista literal (arquivo, funcao, metrica, valor_aceito) do que ja violava quando a regra
    nova entrou (acima do teto, ou entre alvo e teto sem LIMITE:). Acima do valor aceito viola; abaixo falha com 'divida paga'
    (reduza o valor da lista para travar a queda). Funcao nova tem que cumprir a regra.
Construcoes proibidas: typeof, statement expression "({", goto, VLA por heuristica (tamanho de array em
minuscula), __attribute__ fora de runtime\\nativo\\rt_compat.h, asm fora de runtime\\nativo\\rt_salto.c
(o unico lugar com asm), varargs "..." e setjmp/longjmp no compilador. Estado mutavel do rt so na lista branca EstadoRtPermitido.

Excecoes literais (so estas): a API publica do rt que espelha o forja_rt.h (AssinaturasRt) e o envelope de
terceiros (fadec_envelope.c). Qualquer outra excecao muda esta lista no mesmo passo e entra no relato.

Uso:
    python -I conferir_regras.py                  (bateria completa; ultima linha: arquivos=N funcoes=F violacoes=V decisoes_violacoes=K;
                                                   V = linhas/parametros/arquivo/construcoes/estado, K = cognitiva + niveis)
    python -I conferir_regras.py --listar-divida  (as linhas prontas para colar em DividaRegras)
    python -I conferir_regras.py --sem-decisoes   (nao mede cognitiva nem niveis; sem o campo decisoes_violacoes)
    python -I conferir_regras.py --decisoes       (sinonimo do padrao, aceito por compatibilidade)
    python -I conferir_regras.py --listar-decisoes (sinonimo de --listar-divida, com aviso)
    python -I conferir_regras.py --selftest       (casos sinteticos de contagem; nao le o disco do projeto)

Versao: 2.3.1
Changelog: 2.3.1 - M8 P1: ArquivosComAsm ganha runtime/nativo/rt_perfil_win.c (o id da thread pelo TEB, gs:[0x48], 1 linha de asm como o slot do RtFio em rt_fio.h).
           2.3.0 - M5b P39-B3: ArquivosComAsm ganha runtime/nativo/rt_linux_entrada_libc.c (o salto do main para forja_main_libc: o main do musl; o gcc do MinGW poe __main, por isso o asm). Mesmo precedente do cru.
           2.2.0 - M5b P39: ArquivosComAsm ganha runtime/nativo/rt_linux_entrada_cru.c (o _start do ELF, asm de sistema como o de rt_sys_linux.c).
2.1.0 - M5 P43 (T0-1): estado mutavel do runtime fora do RtFio so com a marca 'compartilhado:' na linha da declaracao (ou na lista branca); rt_frame, rt_formata, rt_em_teste, rt_teste_falhou,
                   rt_salto e os rt_pilha_* saem da lista (agora campos do RtFio, rt_fio.h); rt_fio_principal e rt_fio_indice entram.
           2.0.4 - M5 P28b-2: prl_tabela (os 8 filhos abertos do std:process no Linux, rt_processo_linux.c) entra em EstadoRtPermitido.
           2.0.3 - M5 P28b-1: sysl_envp_vivo, sysl_ambiente_copiado (ambiente vivo), es_tabela (32 handles) e es_stdin_buffer/pos/fim (stdin) entram em EstadoRtPermitido.
           2.0.2 - F1 P1b: a lista ArquivosDoOrquestrador passa a contar cmd_fmt.c, cmd_fmt.h, cmd_fmt_arquivo.c, cmd_fmt_json.c e cmd_fmt_interno.h.
           2.0.1 - IA1-b12: rt_pilha_topo, rt_quadros, rt_quadros_n, rt_quadros_total, rt_valor_tipo, rt_valor_a, rt_valor_b e rt_valor_c (rt_pilha.c, so na variante --rastreio) entram em EstadoRtPermitido.
           2.0.0 - P-IA1-reg1c: metrica nova da Emenda 5 (alvo/teto por funcao; cognitiva no lugar de decisoes; LIMITE: <metrica>);
                   DividaRegras por metrica (conferir_regras_divida.py) no lugar de DividaDecisoes; a saida lista a divida (a) acima do teto e
                   (b) entre alvo e teto sem LIMITE: e as 20 piores; --listar-divida; flags antigas aceitas; selftest ampliado com plantio.
                   Reversao: conferir_regras.py.v1_8_3.bak (apagar tambem conferir_regras_cognitiva.py e conferir_regras_divida.py).
           1.8.3 - M5 P26d: rt_sys_linux.c e rt_syscall_linux.h entram em ArquivosComAsm; sysl_argc, sysl_argv e sysl_envp entram em EstadoRtPermitido.
           1.8.2 - M5 P26b: rt_proc_args, rt_proc_nargs e rt_proc_prontos entram em EstadoRtPermitido.
           1.8.1 - M3b P23g: printf, fprintf e snprintf em AssinaturasRt.
           1.8.0 - M3b P23f: rt_legado.c entra em ArquivosComAsm e sprintf/sscanf em AssinaturasRt.
           1.0.0 a 1.7.3 - historico completo (decisoes, niveis, G1, orquestrador) em conferir_regras.py.v1_8_3.bak.
Verificacao: python -I conferir_regras.py   (ultima linha: arquivos=N funcoes=F violacoes=0 decisoes_violacoes=0)
             python -I conferir_regras.py --selftest   (todos os casos ok)
"""

import re
import sys
from collections import namedtuple
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[1]
PastaCompiler = PastaLanguage / "compiler"
PastaRuntime = PastaLanguage / "runtime" / "nativo"
sys.path.insert(0, str(PastaAqui))  # -I tira a pasta do script do sys.path

from conferir_regras_cognitiva import MedirCorpo  # noqa: E402
from conferir_regras_divida import DividaRegras  # noqa: E402

LimiteArquivo = 800
Metricas = ("linhas", "cognitiva", "niveis", "parametros")
MetricasDecisao = ("cognitiva", "niveis")
Alvos = {"linhas": 60, "cognitiva": 15, "niveis": 3, "parametros": 4}
Tetos = {"linhas": 100, "cognitiva": 25, "niveis": 4, "parametros": 7}
ReMarcaLimite = re.compile(r"LIMITE:\s*(\w+)")
ReDiretiva = re.compile(r"^\s*#")
QtdPiores = 20
Medida = namedtuple("Medida", "Relat Ini Nome Valores Marcas")
ArquivosComAtributo = ("runtime/nativo/rt_compat.h",)
ArquivoTerceiro = "fadec_envelope.c"
# P22-O: o orquestrador de projeto mora em driver\ (o resto do driver nao entra); lista literal, so os que ja existem sao lidos (orquestrar nasce no O1b, processo_win no O2).
ArquivosDoOrquestrador = ("driver/orquestrar.c", "driver/orquestrar.h", "driver/orquestrar_exec.c", "driver/orquestrar_interno.h", "driver/processo_win.c", "driver/processo_win.h",
                         "driver/orquestrar_tempos.c", "driver/orquestrar_tempos.h", "driver/erro_sistema.c", "driver/erro_sistema.h",
                         "driver/fmt_nucleo.c", "driver/fmt_nucleo.h", "driver/fmt_espaco.c", "driver/fmt_indent.c", "driver/fmt_interno.h",
                         "driver/cmd_fmt.c", "driver/cmd_fmt.h", "driver/cmd_fmt_arquivo.c", "driver/cmd_fmt_json.c", "driver/cmd_fmt_interno.h")
AssinaturasRt = {
    "forja_falha_estouro", "forja_falha_estouro_u", "forja_faixa", "forja_faixa_u64", "forja_fatia_confere",
    "forja_frame_realocar", "forja_indice_lista", "forja_lista_criar", "forja_lista_crescer",
    "forja_soa_criar", "forja_soa_crescer", "forja_mapa_criar", "forja_mapa_achar", "forja_mapa_inserir", "forja_mapa_remover",
    "forja_traco",
    "printf", "fprintf", "snprintf",  # M3b P23g (rt_legado.c): printf, fprintf e snprintf variadicos do C, sobre __stdio_common_vfprintf e vsnprintf
    "sprintf", "sscanf",  # M3b P23f (rt_legado.c): a assinatura variadica do C que os membros de estatica chamam (legacy_stdio_definitions sobre __stdio_common_*)
}
# G1 (plano_concorrencia.txt, secao 6): estado mutavel de arquivo no runtime nativo so com estes nomes, os 14 grupos de hoje (20 nomes). Por thread (a migrar para
# _Thread_local no M5): rt_frame (rt_aloc.c), rt_formata (rt_formata.c), rt_em_teste, rt_teste_falhou, rt_salto (rt_nativo.c). Do processo, com trava: rt_stdout (rt_saida.c),
# rt_alocs/rt_alocs_n/rt_alocs_cap (rt_aloc.c), rt_heap_cabeca/rt_heap_cauda/rt_vivos_n/rt_atexit_ok (rt_rastreio.c), rt_ao_sair/rt_ao_sair_usados (rt_nativo.c). Escritos uma vez:
# sys_pagina_original (rt_sys_win.c), rt_testes/rt_testes_total/rt_teste_atual (rt_teste.c). Atomico: rt_sem_relatorio (rt_nativo.c). Estado novo = nome novo aqui, no mesmo passo.
EstadoRtPermitido = {
    "rt_stdout", "rt_alocs", "rt_alocs_n", "rt_alocs_cap", "rt_heap_cabeca", "rt_heap_cauda", "rt_vivos_n", "rt_atexit_ok", "rt_quar_cabeca", "rt_quar_cauda", "rt_quar_bytes", "rt_ao_sair", "rt_ao_sair_usados",
    "sys_pagina_original", "rt_testes", "rt_testes_total", "rt_teste_atual", "rt_sem_relatorio",
    "rt_falha_anotada", "rt_falha_codigo", "rt_falha_arquivo", "rt_falha_linha", "rt_falha_causa",
    "rt_teste_vivos_antes", "rt_queda_no_teste", "sys_tratador_excecao",
    "rt_proc_args", "rt_proc_nargs", "rt_proc_prontos",
    "sysl_argc", "sysl_argv", "sysl_envp", "sysl_envp_vivo", "sysl_ambiente_copiado",
    "es_tabela", "es_stdin_buffer", "es_stdin_pos", "es_stdin_fim", "prl_tabela",
    "rt_fio_principal", "rt_fio_indice",
}
ReStatic = re.compile(r"^\s*static\b")
ReConst = re.compile(r"\bconst\b")
RePararametroVla = re.compile(r"^\s+(?!return\b)[A-Za-z_][\w \t\*]*\s+\**[A-Za-z_]\w*\s*\[\s*([a-z_]\w*)\s*\]\s*(;|=(?!=))")
ReTypeof = re.compile(r"\btypeof\b|\b__typeof__\b")
ReGoto = re.compile(r"\bgoto\b")
ReExpressao = re.compile(r"\(\s*\{")
ReSetjmp = re.compile(r"\b(setjmp|longjmp)\b")
ReAtributo = re.compile(r"__attribute__")
ReAsm = re.compile(r"\b(__asm__|__asm|asm)\b")
ArquivosComAsm = ("runtime/nativo/rt_salto.c", "runtime/nativo/rt_fio.h", "runtime/nativo/rt_perfil_win.c", "runtime/nativo/rt_legado.c", "compiler/nativo/rt_embutido.c", "runtime/nativo/rt_sys_linux.c", "runtime/nativo/rt_syscall_linux.h", "runtime/nativo/rt_linux_entrada_cru.c", "runtime/nativo/rt_linux_entrada_libc.c")  # rt_linux_entrada_cru.c (M5b P39): o _start do ELF, asm de sistema como o de rt_sys_linux.c  # rt_sys_linux.c e rt_syscall_linux.h: syscall, _start e restaurador do Linux; rt_legado.c: os stubs da toolchain (___chkstk_ms preserva todos os registradores, so asm); rt_embutido.c so embute o rt por .incbin


def LimparCodigo(Texto):
    """Troca comentarios e o conteudo de literais por espacos, mantendo as quebras de linha."""
    Saida = []
    Estado = "codigo"
    Pos = 0
    while Pos < len(Texto):
        Car = Texto[Pos]
        Prox = Texto[Pos + 1] if Pos + 1 < len(Texto) else ""
        if Estado == "codigo":
            if Car == "/" and Prox == "*":
                Estado = "bloco"
                Saida.append("  ")
                Pos += 2
                continue
            if Car == "/" and Prox == "/":
                Estado = "linha"
                Saida.append("  ")
                Pos += 2
                continue
            if Car in "\"'":
                Estado = "texto" if Car == "\"" else "char"
            Saida.append(Car)
            Pos += 1
            continue
        if Estado == "bloco" and Car == "*" and Prox == "/":
            Estado = "codigo"
            Saida.append("  ")
            Pos += 2
            continue
        if Estado == "linha" and Car == "\n":
            Estado = "codigo"
        if Estado in ("texto", "char"):
            Fim = "\"" if Estado == "texto" else "'"
            if Car == "\\":
                Saida.append("  ")
                Pos += 2
                continue
            if Car == Fim or Car == "\n":
                Estado = "codigo"
                Saida.append(Car)
                Pos += 1
                continue
        Saida.append("\n" if Car == "\n" else " ")
        Pos += 1
    return "".join(Saida)


def ParametrosDe(Cabecalho):
    """Texto entre o '(' que abre a lista de parametros e o ')' que a fecha (balanceado)."""
    Inicio = Cabecalho.index("(") + 1
    Nivel = 1
    for Idx in range(Inicio, len(Cabecalho)):
        if Cabecalho[Idx] == "(":
            Nivel += 1
        elif Cabecalho[Idx] == ")":
            Nivel -= 1
            if Nivel == 0:
                return Cabecalho[Inicio:Idx]
    return Cabecalho[Inicio:]


def ContarParametros(Texto):
    if Texto.strip() in ("", "void"):
        return 0
    Nivel = 0
    Partes = 1
    for Car in Texto:
        if Car == "(":
            Nivel += 1
        elif Car == ")":
            Nivel -= 1
        elif Car == "," and Nivel == 0:
            Partes += 1
    return Partes


def EhCabecalhoDeFuncao(Cabecalho):
    return "(" in Cabecalho and Cabecalho.endswith(")") and "=" not in Cabecalho and not Cabecalho.startswith("typedef")


def NomeDaFuncao(Cabecalho):
    return re.match(r".*?(\w+)\s*\(", Cabecalho, re.S).group(1)


def AcharFuncoes(Codigo):
    """Lista (nome, qtd_parametros, texto_parametros, linha_inicial, linha_final) das definicoes no nivel 0."""
    Funcoes = []
    Nivel = 0
    Buffer = []
    Linha = 1
    Inicio = None
    Atual = None
    EmDiretiva = False
    for Car in Codigo:
        if Car == "\n":
            Linha += 1
            if EmDiretiva and not (Buffer and Buffer[-1] == "\\"):
                EmDiretiva = False
            continue
        if Nivel == 0 and Car == "#" and not "".join(Buffer).strip():
            EmDiretiva = True
        if EmDiretiva:
            continue
        if Nivel > 0:
            if Car == "{":
                Nivel += 1
            elif Car == "}":
                Nivel -= 1
                if Nivel == 0 and Atual is not None:
                    Funcoes.append((Atual[0], Atual[1], Atual[2], Atual[3], Linha))
                    Atual = None
            continue
        if Car == "{":
            Cab = re.sub(r"\s+", " ", "".join(Buffer).strip())
            if EhCabecalhoDeFuncao(Cab):
                Par = ParametrosDe(Cab)
                Atual = (NomeDaFuncao(Cab), ContarParametros(Par), Par, Inicio)
            Nivel = 1
            Buffer = []
            Inicio = None
        elif Car == ";":
            Buffer = []
            Inicio = None
        else:
            if Inicio is None and not Car.isspace():
                Inicio = Linha
            Buffer.append(Car)
    return Funcoes


def NomesDeEstado(Declaracao):
    """Nomes de variavel mutavel declarados num statement do nivel 0; [] se e prototipo, typedef, extern, tipo sem declarador ou dado const."""
    Texto = re.sub(r"\s+", " ", Declaracao).strip()
    if not Texto or Texto.startswith(("typedef", "extern", "_Static_assert")):
        return []
    Antes = Texto.split("=")[0]
    if "(" in Antes or ReConst.search(Antes) or re.fullmatch(r"(struct|union|enum)( \w+)?( \{\})?", Texto):
        return []
    Nomes = []
    for Parte in Antes.split(","):
        Casa = re.search(r"(\w+)\s*(\[[^\]]*\]\s*)*$", Parte.strip())
        if Casa:
            Nomes.append(Casa.group(1))
    return Nomes


def VariaveisDeArquivo(Codigo):
    """Lista (linha, nome) das variaveis mutaveis declaradas no nivel 0 (static ou nao); funcoes e initializers `= {...}` nao entram."""
    Achadas = []
    Nivel = 0
    Linha = 1
    Inicio = None
    Buffer = []
    FuncaoAberta = False
    EmDiretiva = False
    Anterior = ""
    for Car in Codigo:
        if Car == "\n":
            Linha += 1
            EmDiretiva = EmDiretiva and Anterior == "\\"
            continue
        Anterior = Car
        if Nivel == 0 and Inicio is None and Car == "#":
            EmDiretiva = True
        if EmDiretiva:
            continue
        if Car == "{":
            if Nivel == 0:
                Cab = re.sub(r"\s+", " ", "".join(Buffer).strip())
                FuncaoAberta = EhCabecalhoDeFuncao(Cab)
            Nivel += 1
        elif Car == "}":
            Nivel -= 1
            if Nivel == 0:
                Buffer = [] if FuncaoAberta else Buffer + [" {} "]
                Inicio = None if FuncaoAberta else Inicio
        elif Nivel == 0:
            if Car == ";":
                Achadas += [(Inicio, Nome) for Nome in NomesDeEstado("".join(Buffer))]
                Buffer = []
                Inicio = None
            else:
                if Inicio is None and not Car.isspace():
                    Inicio = Linha
                Buffer.append(Car)
    return Achadas


def EstaticosEmFuncao(Codigo):
    """Linhas de 'static' nao const declarado dentro de funcao (o estado teria de ser da lista branca do arquivo)."""
    Achadas = []
    Nivel = 0
    for Numero, Linha in enumerate(Codigo.split("\n"), start=1):
        if Nivel > 0 and ReStatic.match(Linha) and not ReConst.search(Linha.split("=")[0]):
            Achadas.append(Numero)
        Nivel += Linha.count("{") - Linha.count("}")
    return Achadas


def ConferirEstadoMutavel(Caminho, Codigo, Relatorio, Bruto=""):
    """G1 (plano de concorrencia, secao 6): estado mutavel de arquivo em runtime\\nativo so na lista branca ou com a marca 'compartilhado:' na linha da declaracao
    (P43: o estado por thread mora no RtFio, rt_fio.h); static em funcao so se for static const."""
    if PastaRuntime not in Caminho.parents:
        return
    Relat = Relativo(Caminho)
    LinhasBrutas = Bruto.split("\n")
    for Linha, Nome in VariaveisDeArquivo(Codigo):
        if Nome in EstadoRtPermitido or "compartilhado:" in LinhasBrutas[Linha - 1]:
            continue
        Relatorio.append(f"{Relat}:{Linha}: estado mutavel de arquivo '{Nome}' fora do RtFio sem a marca 'compartilhado:' nem lista branca (EstadoRtPermitido)")
    for Linha in EstaticosEmFuncao(Codigo):
        Relatorio.append(f"{Relat}:{Linha}: static dentro de funcao so se for static const")


def ArquivosDoEscopo():
    Arquivos = []
    Pastas = ((PastaCompiler / "nativo", "**/*"), (PastaCompiler / "ir", "layout.*"), (PastaRuntime, "*"), (PastaCompiler / "cache", "*"))
    for Pasta, Padrao in Pastas:
        for Caminho in sorted(Pasta.glob(Padrao)):
            if Caminho.is_file() and Caminho.suffix in (".c", ".h"):
                Arquivos.append(Caminho)
    for Nome in ArquivosDoOrquestrador:
        if (PastaCompiler / Nome).is_file():
            Arquivos.append(PastaCompiler / Nome)
    return Arquivos


def Relativo(Caminho):
    return Caminho.relative_to(PastaLanguage).as_posix()


def MarcasDaFuncao(Linhas, Ini, Fim):
    """Metricas com 'LIMITE: <metrica> <motivo>' na janela do cabecalho (3 linhas acima do cabecalho ate o fim da funcao)."""
    Trecho = "\n".join(Linhas[max(Ini - 4, 0):Fim])
    return set(ReMarcaLimite.findall(Trecho))


def CorpoDaFuncao(LinhasCodigo, Ini, Fim):
    """Texto do corpo (da 1a chave ate a linha Fim), sem as linhas de diretiva '#'; o corpo e o nivel 0 de indentacao."""
    Texto = "\n".join(LinhasCodigo[Ini - 1:Fim])
    Corpo = Texto[Texto.index("{"):]
    # A 1a linha do corpo comeca em '{', nunca em '#', entao o filtro so mexe nas linhas de dentro.
    return "\n".join(Linha for Linha in Corpo.split("\n") if not ReDiretiva.match(Linha))


def ValoresDaFuncao(LinhasCodigo, Definicao, ComDecisoes):
    """Metricas da funcao (tupla de AcharFuncoes): linhas e parametros sempre; cognitiva e niveis so com ComDecisoes."""
    Nome, QtdParam, TextoParam, Ini, Fim = Definicao
    Valores = {"linhas": Fim - Ini + 1, "parametros": QtdParam}
    if ComDecisoes:
        Corpo = CorpoDaFuncao(LinhasCodigo, Ini, Fim)
        Valores["cognitiva"], Valores["niveis"] = MedirCorpo(Corpo, Nome)
    return Valores


def MedirArquivo(Relat, NomeArq, Linhas, Codigo, ComDecisoes, Relatorio):  # LIMITE: parametros 6 posicionais: 1 definicao, 2 chamadores (disco e selftest), sem struct para nao criar tipo de uso unico
    """Devolve (medidas, total_de_funcoes); AssinaturasRt e fadec_envelope.c ficam fora das medidas; varargs vai para o Relatorio."""
    LinhasCodigo = Codigo.split("\n")
    Funcoes = AcharFuncoes(Codigo)
    Medidas = []
    for Definicao in Funcoes:
        Nome, QtdParam, TextoParam, Ini, Fim = Definicao
        if Nome in AssinaturasRt or NomeArq == ArquivoTerceiro:
            continue
        if "..." in TextoParam:
            Relatorio.append(f"{Relat}:{Ini}: {Nome}: varargs '...' proibido")
        Medidas.append(Medida(Relat, Ini, Nome, ValoresDaFuncao(LinhasCodigo, Definicao, ComDecisoes), MarcasDaFuncao(Linhas, Ini, Fim)))
    return Medidas, len(Funcoes)


def StatusDaMetrica(Metrica, Valor, Marcas):
    """'acima' (passou do teto), 'entre' (passou do alvo sem LIMITE: <metrica>) ou 'ok'."""
    if Valor > Tetos[Metrica]:
        return "acima"
    if Valor > Alvos[Metrica] and Metrica not in Marcas:
        return "entre"
    return "ok"


def AceitosDaDivida(Divida):
    """{(arquivo, funcao, metrica): valor_aceito}; com entradas repetidas vale o maior."""
    Aceitos = {}
    for Relat, Nome, Metrica, Valor in Divida:
        Chave = (Relat, Nome, Metrica)
        Aceitos[Chave] = max(Valor, Aceitos.get(Chave, 0))
    return Aceitos


def JulgarMetrica(Metrica, Valor, Marcas, Aceito):
    """Mensagem de violacao de uma metrica de uma funcao, ou None. Aceito = valor congelado na DividaRegras (ou None)."""
    if Aceito is not None:
        if Valor > Aceito:
            return f"{Valor} {Metrica} > {Aceito} (DividaRegras)"
        if Valor < Aceito:
            return f"divida paga: reduza o valor de DividaRegras ({Metrica} {Valor} < {Aceito})"
        return None
    Status = StatusDaMetrica(Metrica, Valor, Marcas)
    if Status == "acima":
        return f"{Valor} {Metrica} > teto {Tetos[Metrica]}"
    if Status == "entre":
        return f"{Valor} {Metrica} > alvo {Alvos[Metrica]} (sem LIMITE: {Metrica})"
    return None


def MensagensDaMedida(Item, Aceitos):
    """[(metrica, mensagem completa)] das metricas da funcao que violam a regra (ou a divida congelada)."""
    Achadas = []
    for Metrica, Valor in Item.Valores.items():
        Msg = JulgarMetrica(Metrica, Valor, Item.Marcas, Aceitos.get((Item.Relat, Item.Nome, Metrica)))
        if Msg:
            Achadas.append((Metrica, f"{Item.Relat}:{Item.Ini}: {Item.Nome}: {Msg}"))
    return Achadas


def ConferirMedidas(Medidas, Divida, Relatorio, RelatorioDecisoes):
    """Aplica a regra nova a cada metrica de cada funcao; cognitiva e niveis vao para RelatorioDecisoes, o resto para Relatorio."""
    Aceitos = AceitosDaDivida(Divida)
    for Item in Medidas:
        for Metrica, Msg in MensagensDaMedida(Item, Aceitos):
            (RelatorioDecisoes if Metrica in MetricasDecisao else Relatorio).append(Msg)


def EntradaDaMetrica(Item, Metrica):
    """(medida, metrica, valor, status) se a metrica da funcao esta acima do teto ou entre alvo e teto sem LIMITE:; senao None."""
    Valor = Item.Valores.get(Metrica)
    if Valor is None:
        return None
    Status = StatusDaMetrica(Metrica, Valor, Item.Marcas)
    return None if Status == "ok" else (Item, Metrica, Valor, Status)


def EntradasDaDivida(Medidas):
    """[(medida, metrica, valor, status)] do que esta acima do teto ou entre alvo e teto sem LIMITE:, por arquivo e linha."""
    Saida = []
    for Item in sorted(Medidas, key=lambda Medido: (Medido.Relat, Medido.Ini)):
        Candidatas = (EntradaDaMetrica(Item, Metrica) for Metrica in Metricas)
        Saida += [Entrada for Entrada in Candidatas if Entrada]
    return Saida


def LinhasDaDivida(Medidas):
    """Linhas '("arquivo", "funcao", "metrica", valor),' prontas para colar em DividaRegras; entrada repetida fica com o maior valor."""
    Maiores = {}
    for Item, Metrica, Valor, Status in EntradasDaDivida(Medidas):
        Chave = (Item.Relat, Item.Nome, Metrica)
        Maiores[Chave] = max(Valor, Maiores.get(Chave, 0))
    return [f'    ("{Relat}", "{Nome}", "{Metrica}", {Valor}),' for (Relat, Nome, Metrica), Valor in Maiores.items()]


def PioresFuncoes(Medidas, Quantas):
    """As funcoes que mais passam do teto (maior valor/teto entre as metricas), com o texto das metricas acima do alvo."""
    Fora = {}
    for Item, Metrica, Valor, Status in EntradasDaDivida(Medidas):
        Fora.setdefault((Item.Relat, Item.Ini, Item.Nome), []).append((Valor / Tetos[Metrica], f"{Metrica}={Valor}"))
    Ordem = sorted(Fora.items(), key=lambda Par: -max(Razao for Razao, Texto in Par[1]))
    return [f"{Relat}:{Ini}: {Nome}: " + " ".join(Texto for Razao, Texto in Itens) for (Relat, Ini, Nome), Itens in Ordem[:Quantas]]


def ResumoDaDivida(Medidas):
    """Linhas do resumo: entradas (funcao x metrica) e funcoes em (a) acima do teto e (b) entre alvo e teto sem LIMITE:, mais as piores."""
    Entradas = EntradasDaDivida(Medidas)
    Linhas = []
    for Status, Rotulo in (("acima", "(a) acima do teto"), ("entre", "(b) entre alvo e teto sem LIMITE:")):
        Das = [(Item, Metrica) for Item, Metrica, Valor, Estado in Entradas if Estado == Status]
        Funcoes = {(Item.Relat, Item.Ini) for Item, Metrica in Das}
        Linhas.append(f"divida {Rotulo}: {len(Das)} entradas em {len(Funcoes)} funcoes")
    Linhas.append(f"as {QtdPiores} piores:")
    return Linhas + ["  " + Texto for Texto in PioresFuncoes(Medidas, QtdPiores)]


def ConferirConstrucoes(Caminho, Codigo, Relatorio):
    Relat = Relativo(Caminho)
    NoCompilador = PastaCompiler in Caminho.parents
    for Numero, Linha in enumerate(Codigo.split("\n"), start=1):
        if ReTypeof.search(Linha):
            Relatorio.append(f"{Relat}:{Numero}: typeof proibido")
        if ReExpressao.search(Linha):
            Relatorio.append(f"{Relat}:{Numero}: statement expression proibida")
        if ReGoto.search(Linha):
            Relatorio.append(f"{Relat}:{Numero}: goto proibido")
        if ReAtributo.search(Linha) and not Relat.endswith(ArquivosComAtributo):
            Relatorio.append(f"{Relat}:{Numero}: __attribute__ fora de runtime/nativo/rt_compat.h")
        if ReAsm.search(Linha) and not Relat.endswith(ArquivosComAsm):
            Relatorio.append(f"{Relat}:{Numero}: asm fora de runtime/nativo/rt_salto.c e rt_legado.c (e do .incbin de rt_embutido.c)")
        if ReSetjmp.search(Linha) and NoCompilador:
            Relatorio.append(f"{Relat}:{Numero}: setjmp/longjmp no compilador")
        Casa = RePararametroVla.match(Linha)
        if Casa and not Casa.group(1).isupper():
            Relatorio.append(f"{Relat}:{Numero}: VLA por heuristica (tamanho '{Casa.group(1)}')")


def MedirTexto(Texto, Divida=()):
    """Mede um trecho C sintetico como se fosse x/teste.c; devolve (medidas, mensagens). Nao le o disco."""
    Relatorio = []
    Decisoes = []
    Medidas, Total = MedirArquivo("x/teste.c", "teste.c", Texto.split("\n"), LimparCodigo(Texto), True, Relatorio)
    ConferirMedidas(Medidas, Divida, Relatorio, Decisoes)
    return Medidas, Relatorio + Decisoes


def ConferirCaso(Texto, Divida, Esperado, Mensagens):
    """None se o trecho mede o 'Esperado' (dict metrica -> valor) e acusa 'Mensagens' mensagens; senao o texto da falha."""
    Medidas, Achadas = MedirTexto(Texto, Divida)
    Obtido = {Metrica: Medidas[0].Valores[Metrica] for Metrica in Esperado} if Medidas else {}
    if Obtido != Esperado or len(Achadas) != Mensagens:
        return f"esperado {Esperado} mensagens={Mensagens}, obtido {Obtido} {Achadas}"
    return None


def Funcao(Corpo, Cabecalho="void F(int a)"):
    return f"{Cabecalho} {{\n{Corpo}}}\n"


def CasosDeCognitiva():
    """(nome, texto, divida, esperado, mensagens): contagens conhecidas da cognitiva e dos niveis."""
    Cinco = "switch (a) {\n case 1: b = 1; break;\n case 2: b = 2; break;\n case 3: b = 3; break;\n case 4: b = 4; break;\n default: b = 0;\n}\n"
    Despacho = "switch (a) {\n case 1: x(); break;\n case 2: y(); break;\n default: z();\n}\n"
    Aninhados = "for (;;) {\n while (a) {\n if (a) {\n for (;;) {\n x();\n }\n }\n }\n}\n"
    return (
        ("if=1", Funcao(" if (a) x();\n"), (), {"cognitiva": 1, "niveis": 1}, 0),
        ("if dentro de for=3", Funcao(" for (;;) {\n if (a) x();\n }\n"), (), {"cognitiva": 3, "niveis": 2}, 0),
        ("if/for/if sem chaves = 3 niveis", Funcao(" if (a)\n for (;;)\n if (a)\n x();\n"), (), {"cognitiva": 6, "niveis": 3}, 0),
        ("a && b && c=1", Funcao(" return a && b && c;\n", "int F(int a)"), (), {"cognitiva": 1}, 0),
        ("a && b || c=2", Funcao(" return a && b || c;\n", "int F(int a)"), (), {"cognitiva": 2}, 0),
        ("sequencias separadas por parenteses=2", Funcao(" return a && (b || c);\n", "int F(int a)"), (), {"cognitiva": 2}, 0),
        ("switch de 5 casos=1", Funcao(Cinco), (), {"cognitiva": 1, "niveis": 1}, 0),
        ("switch de despacho (so chama funcao)=0", Funcao(Despacho), (), {"cognitiva": 0}, 0),
        ("else if encadeado=4", Funcao(" if (a) x();\n else if (a > 1) y();\n else if (a > 2) w();\n else z();\n"), (), {"cognitiva": 4}, 0),
        ("ternario aninhado=3", Funcao(" return a ? 1 : a > 1 ? 2 : 3;\n", "int F(int a)"), (), {"cognitiva": 3}, 0),
        ("do-while=1", Funcao(" do {\n x();\n } while (a);\n"), (), {"cognitiva": 1, "niveis": 1}, 0),
        ("recursao soma 1", Funcao(" if (a) F(a - 1);\n"), (), {"cognitiva": 2}, 0),
        ("literal e comentario nao contam", Funcao(' char *s = "if && ||"; /* if (a) */\n'), (), {"cognitiva": 0}, 0),
        ("niveis 4 sem marca acusa", Funcao(Aninhados), (), {"niveis": 4}, 1),
        ("niveis 4 com LIMITE: niveis passa e cognitiva 10 passa", Funcao(Aninhados, "void F(int a) /* LIMITE: niveis teste */"), (), {"niveis": 4}, 0),
        ("AssinaturasRt fica de fora", "int forja_traco(int a) {\n" + "if (a) x();\n" * 30 + "}\n", (), {}, 0),
    )


def CasosDeLimites():
    """(nome, texto, divida, esperado, mensagens): linhas, parametros e divida."""
    L61 = Funcao("x();\n" * 59)
    L61Marcado = Funcao("x();\n" * 59, "void F(int a) /* LIMITE: linhas teste */")
    L101 = Funcao("x();\n" * 99)
    L101Marcado = Funcao("x();\n" * 99, "void F(int a) /* LIMITE: linhas teste */")
    Cabecalho5 = "void F(int a, int b, int c, int d, int e)"
    Cabecalho8 = "void F(int a, int b, int c, int d, int e, int f, int g, int h)"
    return (
        ("60 linhas passa", Funcao("x();\n" * 58), (), {"linhas": 60}, 0),
        ("61 linhas sem LIMITE acusa", L61, (), {"linhas": 61}, 1),
        ("61 linhas com LIMITE passa", L61Marcado, (), {"linhas": 61}, 0),
        ("101 linhas acusa", L101, (), {"linhas": 101}, 1),
        ("101 linhas com LIMITE acusa (acima do teto)", L101Marcado, (), {"linhas": 101}, 1),
        ("5 parametros sem LIMITE acusa", Funcao("x();\n", Cabecalho5), (), {"parametros": 5}, 1),
        ("5 parametros com LIMITE passa", Funcao("x();\n", Cabecalho5 + " /* LIMITE: parametros teste */"), (), {"parametros": 5}, 0),
        ("8 parametros acusa mesmo com LIMITE", Funcao("x();\n", Cabecalho8 + " /* LIMITE: parametros teste */"), (), {"parametros": 8}, 1),
        ("divida no valor atual passa", L61, (("x/teste.c", "F", "linhas", 61),), {"linhas": 61}, 0),
        ("divida paga (medido menor) falha", L61, (("x/teste.c", "F", "linhas", 62),), {"linhas": 61}, 1),
        ("divida cresceu (medido maior) falha", L61, (("x/teste.c", "F", "linhas", 60),), {"linhas": 61}, 1),
        ("divida de outra metrica nao cobre", L61, (("x/teste.c", "F", "cognitiva", 20),), {"linhas": 61}, 2),
    )


def CasoPlantado():
    """Funcao com cognitiva 27 (9 blocos for+if = 3 cada), sem LIMITE e fora da divida: o selftest tem que acusar."""
    return Funcao(" for (;;) {\n if (a) x();\n }\n" * 9)


def Selftest():
    """Roda os casos de contagem conhecida, o plantio de erro e as opcoes. Devolve 0 se todos passam."""
    Falhas = 0
    Casos = CasosDeCognitiva() + CasosDeLimites()
    for Nome, Texto, Divida, Esperado, Mensagens in Casos:
        Erro = ConferirCaso(Texto, Divida, Esperado, Mensagens)
        Falhas += Erro is not None
        print(f"FALHA {Nome}: {Erro}" if Erro else f"ok {Nome}")
    print(f"selftest casos={len(Casos)} falhas={Falhas}")
    Falhas += SelftestPlantio()
    return 0 if Falhas + SelftestOpcoes() == 0 else 1


def SelftestPlantio():
    """O plantio de erro tem que ser acusado (cognitiva 27 > teto 25) e uma expectativa errada tem que reprovar o caso; devolve as falhas."""
    Falhas = 0
    Medidas, Achadas = MedirTexto(CasoPlantado())
    Acusou = len(Achadas) == 1 and "27 cognitiva > teto 25" in Achadas[0]
    Falhas += not Acusou
    print("ok plantio acusado (cognitiva 27 > teto 25)" if Acusou else f"FALHA plantio nao acusado: {Achadas}")
    Reprovou = ConferirCaso(Funcao(" if (a) x();\n"), (), {"cognitiva": 0}, 0) is not None
    Falhas += not Reprovou
    print("ok expectativa errada reprova o caso" if Reprovou else "FALHA expectativa errada passou")
    return Falhas


def DecisoesLigadas(Opcoes):
    """Cognitiva/niveis valem por padrao; so --sem-decisoes desliga (e as listagens da divida sempre medem)."""
    return "--sem-decisoes" not in Opcoes or bool(Opcoes & {"--listar-divida", "--listar-decisoes"})


def SelftestOpcoes():
    """Padrao liga, --decisoes (compatibilidade) liga, --sem-decisoes desliga; devolve o numero de falhas."""
    Casos = ((set(), True), ({"--decisoes"}, True), ({"--sem-decisoes"}, False), ({"--sem-decisoes", "--listar-divida"}, True))
    Falhas = 0
    for Opcoes, Esperado in Casos:
        if DecisoesLigadas(Opcoes) != Esperado:
            Falhas += 1
            print(f"FALHA opcoes {sorted(Opcoes)}: esperado ligado={Esperado}")
    print(f"selftest opcoes casos={len(Casos)} falhas={Falhas}")
    return Falhas


def MedirEscopo(Medir, Relatorio):
    """Le o escopo do disco; devolve (arquivos, total_de_funcoes, medidas); construcoes, estado e tamanho de arquivo vao para o Relatorio."""
    Arquivos = ArquivosDoEscopo()
    TotalFuncoes = 0
    Medidas = []
    for Caminho in Arquivos:
        Bruto = Caminho.read_text(encoding="utf-8", errors="replace")
        Linhas = Bruto.split("\n")
        if len(Linhas) > LimiteArquivo:
            Relatorio.append(f"{Relativo(Caminho)}: {len(Linhas)} linhas > {LimiteArquivo}")
        Codigo = LimparCodigo(Bruto)
        MedidasDoArquivo, Total = MedirArquivo(Relativo(Caminho), Caminho.name, Linhas, Codigo, Medir, Relatorio)
        Medidas += MedidasDoArquivo
        TotalFuncoes += Total
        ConferirConstrucoes(Caminho, Codigo, Relatorio)
        ConferirEstadoMutavel(Caminho, Codigo, Relatorio, Bruto)
    return Arquivos, TotalFuncoes, Medidas


def Principal():
    Opcoes = set(sys.argv[1:])
    if "--selftest" in Opcoes:
        return Selftest()
    Medir = DecisoesLigadas(Opcoes)
    Relatorio = []
    Arquivos, TotalFuncoes, Medidas = MedirEscopo(Medir, Relatorio)
    if Opcoes & {"--listar-divida", "--listar-decisoes"}:
        if "--listar-decisoes" in Opcoes:
            print("aviso: --listar-decisoes e sinonimo de --listar-divida (DividaRegras)", file=sys.stderr)
        print("\n".join(LinhasDaDivida(Medidas)))
        return 0
    DecisoesViolacoes = []
    ConferirMedidas(Medidas, DividaRegras, Relatorio, DecisoesViolacoes)
    Final = f"arquivos={len(Arquivos)} funcoes={TotalFuncoes} violacoes={len(Relatorio)}"
    Final += f" decisoes_violacoes={len(DecisoesViolacoes)}" if Medir else ""
    for Linha in Relatorio + DecisoesViolacoes:
        print(Linha)
    if Medir:
        print("\n".join(ResumoDaDivida(Medidas)))
    print(Final)
    return 0 if not Relatorio and not DecisoesViolacoes else 1


if __name__ == "__main__":
    sys.exit(Principal())
