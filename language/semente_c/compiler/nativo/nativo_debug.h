/* nativo_debug.h - --debug-code do backend nativo: itens pedidos, os REGISTROS (uma fonte so) e os dois formatadores, texto e JSON.
 * Versao: 0.12.0
 * Changelog: 0.12.0 - G0d: item passes (DEBUG_PASSES, REG_PASSES, DebugEntrada.passes); so no texto; o JSONL nao o escreve.
 *            0.11.0 - M8 P2: item perfil (DEBUG_PERFIL, REG_PERFIL, DebugEntrada.programa); so no texto; o include do nativo_perfil.h vem aqui porque o nativo.c esta no teto de 800 linhas.
 *            0.10.0 - M8 H1: item interface (DEBUG_INTERFACE, REG_INTERFACE, DebugEntrada.pedido); so no texto (<saida>.debug.txt); o JSONL nao o escreve.
 *            0.9.0 - M7a F0: item otim (DEBUG_OTIM, REG_OTIM, DebugEntrada.otim); so no texto (<saida>.debug.txt); o JSONL nao o escreve.
 *            0.8.0 - M6 P65b: item linhas (DEBUG_LINHAS, REG_LINHAS, RegLinhas, DebugEntrada.dbg); DEBUG_VERSAO segue 0.7.0 (registro novo so por pedido).
 *            0.7.0 - M6 P65a: item locais (DEBUG_LOCAIS, REG_LOCAIS, DebugEntrada.locais); DEBUG_VERSAO segue 0.7.0 (registro novo so por pedido).
 *            0.6.0 - M5 P27b: DEBUG_VERSAO 0.7.0; RegFormato.elf e o registro "segmento" (RegSegmento: um por cabecalho de programa do ELF, depois do "formato"; so no alvo x86_64-linux).
 *            0.5.0 - M4 P24p: DEBUG_VERSAO 0.6.0 e o registro "poda" (RegPoda: procs_total, procs_gerados; so no JSONL, logo depois do cabecalho); DebugEntrada.procs_total/procs_gerados.
 *            0.4.0 - M4 P24a: DEBUG_VERSAO 0.5.0 (o registro "tempos" ganha lex, parse, check, ir, outros, total e linhas; regs e codificar passam a ser medidos).
 *            0.3.0 - M3 P22a: DEBUG_VERSAO 0.4.0 e o registro "export" (RegExport: nome, rva e ordinal de cada simbolo do export directory de uma DLL; so no JSONL, depois dos "import").
 *            0.2.2 - M3 P19a: DEBUG_VERSAO 0.3.0 (o registro "import" ganha "descritor": indice da DLL na tabela de descritores do .idata; ha um por DLL importada).
 *            0.2.1 -M2 P11a: DEBUG_VERSAO 0.2.0 (o registro "funcao" ganha slots_agregado).
 *            0.2.0 - backend x64 P8b: DebugPercorrer produz os registros (cabecalho, funcao, instr, bir, reloc, secao, import, formato, faixa, tempos) e
 *                    DebugMontar (texto, <saida>.debug.txt) e DebugMontarJson (<saida>.debug.jsonl) so os formatam; item "json" na lista.
 *            0.1.0 - backend x64 P8: DebugItens e DebugMontar (itens bir, asm, quadros, reloc, formato e tempos).
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#ifndef FORJA_NATIVO_NATIVO_DEBUG_H
#define FORJA_NATIVO_NATIVO_DEBUG_H

#include "common/fonte.h"
#include "nativo/x64/alvo.h"
#include "nativo/fmt/pe.h"
#include "nativo/nativo.h"
#include "nativo/nativo_interface.h" /* o nativo.c ja esta no teto de 800 linhas: ele ganha o NativoInterfaceDll por este include, sem linha nova */
#include "nativo/nativo_perfil.h" /* idem: o NativoPerfilLer e o NativoPerfilTabela do nativo.c (M8 P2); traz tambem o BirPrograma do item perfil */

/* Bits da mascara de itens, na ordem em que as secoes saem no texto. DEBUG_JSON pede o .debug.jsonl no lugar do .debug.txt. */
#define DEBUG_BIR 0x01u
#define DEBUG_ASM 0x02u
#define DEBUG_QUADROS 0x04u
#define DEBUG_RELOC 0x08u
#define DEBUG_FORMATO 0x10u
#define DEBUG_TEMPOS 0x20u
#define DEBUG_JSON 0x40u
#define DEBUG_LOCAIS 0x80u /* M6 P65a: locais nomeados do usuario e a casa de cada um (fora de DEBUG_TODOS_ITENS: so por pedido) */
#define DEBUG_LINHAS 0x100u /* M6 P65b: procs, linhas (offset -> posicao), parametros e locais do DbgPrograma (so por pedido; esquema 0.7.0 ganha o registro "linhas") */
#define DEBUG_OTIM 0x200u /* M7a F0: contadores dos passes de otimizacao da BIR (so por pedido; so no texto) */
#define DEBUG_INTERFACE 0x400u /* M8 H1: texto canonico da interface da DLL (o que o hash do forja_dll_info cobre; so por pedido; so no texto) */
#define DEBUG_PERFIL 0x800u /* M8 P2: as procs que o passe `perfil` instrumentou, com o id de cada uma na tabela FPF1 (so por pedido; so no texto) */
#define DEBUG_PASSES 0x1000u /* G0d: a BIR inteira na entrada do otimizador e depois de cada passe que rodou (so por pedido; so no texto) */
#define DEBUG_TODOS_ITENS (DEBUG_BIR | DEBUG_ASM | DEBUG_QUADROS | DEBUG_RELOC | DEBUG_FORMATO | DEBUG_TEMPOS)
#define DEBUG_VERSAO "0.7.0" /* versao do esquema do debug.jsonl (tests\nativo\esquema_debug.json) */

/* Tudo o que os registros leem; so os campos dos itens pedidos precisam estar preenchidos. */
typedef struct {
    uint32_t mascara;
    const char *versao;             /* versao do forja */
    const char *modo;               /* "dev" ou "release" */
    const char *alvo;               /* "x86_64-windows" */
    const Fonte *fonte;             /* arquivo, linha e coluna das instrucoes */
    const Buf *bir;                 /* BirImprimir, colhido antes de a BIR morrer */
    const Buf *otim;                /* item otim: texto do OtimExecutar (cabecalho e uma linha por passe e funcao) */
    const Buf *passes;              /* item passes: BIR depois de cada passe (OtimRelatos.passes) */
    const Buf *locais;            /* BirImprimirLocais (item locais), colhido junto com o bir */
    const DbgPrograma *dbg;         /* item linhas: registro neutro colhido pelo gerador (M6 P65b) */
    const AlvoListagem *lista;      /* funcoes e instrucoes, colhidas pelo gerador */
    const Objeto *codigo;           /* programa: relocacoes e o tamanho do .text */
    const Objeto *runtime;          /* runtime embutido: o tamanho do .text dele */
    const Imagem *imagem;           /* secoes do PE depois de ligar (os bytes finais das instrucoes) */
    const NativoTempos *tempos;
    size_t tam_exe;
    uint32_t procs_total, procs_gerados; /* da BIR (BirPrograma): procs do programa e os que a poda deixou entrar */
    const NativoPedido *pedido;     /* item interface: IR, tipos e nomes de onde sai o texto; NULL = sem o item */
    const BirPrograma *programa;    /* item perfil: a BIR depois dos passes (as procs instrumentadas); NULL = sem o item */
} DebugEntrada;

typedef enum {
    REG_CABECALHO, REG_FUNCAO, REG_INSTR, REG_BIR, REG_RELOC, REG_SECAO, REG_IMPORT, REG_FORMATO, REG_FAIXA, REG_TEMPOS, REG_EXPORT, REG_PODA, REG_SEGMENTO, REG_LOCAIS, REG_LINHAS, REG_OTIM, REG_INTERFACE, REG_PERFIL, REG_PASSES
} DebugTipo;

/* Um proc do DbgPrograma; fonte resolve pos em arquivo, linha e coluna (so quem formata). */
typedef struct { const DbgPrograma *dbg; const DbgProc *proc; const Fonte *fonte; } RegLinhas;

typedef struct { const char *forja, *alvo, *modo; } RegCabecalho;
/* quadro: unwind, push, fpreg... da funcao; arquivo NULL = sem posicao. inicio = offset em .text. */
typedef struct { const char *nome, *arquivo; uint32_t rva, inicio, tamanho, linha; const ListaFuncao *quadro; } RegFuncao;
/* bytes: os do exe final. bloco/indice: LISTA_SEM_INDICE no prologo e no epilogo. linha 0 = sem posicao. simbolo NULL = nao aponta. */
typedef struct {
    const char *funcao, *texto_asm, *simbolo, *arquivo;
    const uint8_t *bytes;
    uint32_t rva, deslocamento, tam, bloco, indice, linha, coluna;
    uint8_t origem;
} RegInstr;
typedef struct { const char *funcao, *texto; size_t tam; } RegBir;           /* funcao "" = o bloco "simbolos:" */
typedef struct { const char *secao, *kind, *simbolo; uint32_t offset, rva; int64_t adendo; } RegReloc;
typedef struct { char nome[9]; uint32_t rva, tam_virtual, tam_bruto, caracteristicas; } RegSecao;
typedef struct { const char *dll, *nome; uint32_t iat_rva, descritor; } RegImport;
/* elf: o executavel e um ELF64 (esquema 0.7.0): sem linker_maior/linker_menor, e os PT_* vem nos registros "segmento" logo depois deste. */
typedef struct { const char *forja; uint64_t imagem_base; uint32_t entrada_rva; size_t tam_arquivo; uint8_t linker_maior, linker_menor; bool elf; } RegFormato;
/* Um cabecalho de programa do ELF (Elf64_Phdr): tipo "LOAD" ou "GNU_STACK"; flags em "R", "RX" ou "RW". */
typedef struct { const char *tipo, *flags; uint64_t offset, vaddr, filesz, memsz, alinhamento; } RegSegmento;
typedef struct { uint32_t rva, tamanho; const char *origem; } RegFaixa;      /* origem "runtime" ou "preenchimento" */
typedef struct { const NativoTempos *tempos; } RegTempos;
typedef struct { const char *nome; uint32_t rva, ordinal; } RegExport;       /* ordinal da tabela de exportacao (base 1); nomes em ordem ASCII */
typedef struct { uint32_t procs_total, procs_gerados; } RegPoda;             /* procs_podados = total - gerados (derivado no formatador) */

typedef struct {
    uint8_t tipo;                   /* DebugTipo */
    union {
        RegCabecalho cabecalho;
        RegFuncao funcao;
        RegInstr instr;
        RegBir bir;       /* tambem serve ao REG_LOCAIS (mesma forma: funcao + texto) */
        RegReloc reloc;
        RegSecao secao;
        RegImport import;
        RegFormato formato;
        RegFaixa faixa;
        RegTempos tempos;
        RegExport exporta;
        RegPoda poda;
        RegSegmento segmento;
        RegLinhas linhas;
        RegBir otim;      /* REG_OTIM: funcao "" + o texto inteiro */
        RegBir texto_interface; /* REG_INTERFACE: funcao "" + o texto inteiro */
        RegBir texto_perfil;    /* REG_PERFIL: funcao "" + o texto inteiro */
        RegBir texto_passes;    /* REG_PASSES: funcao "" + o texto inteiro */
    };
} DebugReg;

typedef void (*DebugEscritor)(void *estado, const DebugReg *reg);

/* "bir,asm,...[,json]" -> mascara. Sozinho, "json" pede todos os itens. false com a causa em causa[n] se algum item e desconhecido (inclusive "ir":
 * a IR nao tem impressora). */
bool DebugItens(const char *lista, uint32_t *mascara, char *causa, size_t n);

/* Nome de ListaOrigem no JSON ("prologo", "sondagem", "codigo", "frio", "epilogo"). */
const char *DebugNomeOrigem(uint8_t origem);

/* Produz os registros dos itens da mascara, em ordem fixa, chamando 'escrever' com cada um (o registro so vale durante a chamada).
 * Ordem: cabecalho, poda, [funcao, instr..., faixa] por funcao, faixas do runtime, bir, reloc, formato, secao, import, tempos. */
void DebugPercorrer(const DebugEntrada *e, DebugEscritor escrever, void *estado);

/* Anexa a saida o texto (<saida>.debug.txt): um cabecalho e uma secao "## <item>" por item pedido. */
void DebugMontar(const DebugEntrada *e, Buf *saida);

/* Anexa a saida o JSONL (<saida>.debug.jsonl): uma linha JSON por registro. */
void DebugMontarJson(const DebugEntrada *e, Buf *saida);

#endif
