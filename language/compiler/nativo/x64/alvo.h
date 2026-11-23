/* alvo.h - alvos do backend nativo: nome e gerador de codigo (BIR -> Objeto).
 * Versao: 0.10.2
 * Changelog: 0.10.2 - G3 G0b: quebra de ciclo x64->nativo (movido de nativo/ para nativo/x64/).
 *            0.10.1 - M7a F3: LISTA_TAM_UNWIND 16 -> 32 (os PUSH_NONVOL do alocador aumentam o UNWIND_INFO).
 *            0.10.0 - M6e P93d: AlvoColheita.guarda (--guarda-memoria: a FRT1 leva a subtabela de linhas).
 *            0.9.0 - M6 P65b: AlvoColheita.dbg (DbgPrograma colhido pelo gerador; NULL = sem coleta; o rastreio exige dbg).
 *            0.8.0 - M5b P39-B1 (D0654): Alvo.mem (nomes de copia e zera do runtime do alvo: memcpy/memset no Windows, forja_memcpy/forja_memset no linux).
 *            0.7.0 - IA1-b12: AlvoColheita.rastreio (o gerador x64 grava a tabela de posicoes do --rastreio no .rdata do programa).
 *            0.6.0 - M5 P27a: Alvo ganha formato (ImagemFormato: o SO do alvo), dll (gera biblioteca) e sem_escritor (NULL = o escritor do formato existe; senao o texto do X0003); AlvoQtd e AlvoPorIndice (--suporte --json lista os alvos).
 *            0.5.0 - M4 P24a: AlvoTempos e AlvoColheita; o 4o parametro de Alvo.gerar deixa de ser a AlvoListagem e vira a AlvoColheita (listagem e tempos por fatia).
 *            0.4.0 - M2 P11a: ListaSlot e ListaFuncao.slots_agregado (os slots de memoria de cada funcao, para o debug.jsonl 0.2.0).
 *            0.3.0 - backend x64 P8b: AlvoListagem passa de texto (codigo, quadros) para registros (ListaFuncao, ListaInstr); Iniciar e Liberar.
 *            0.2.0 - backend x64 P8: AlvoListagem (texto do --debug-code colhido durante a geracao) e quarto parametro de Alvo.gerar.
 *            0.1.0 - backend x64 P7a: Alvo e AlvoPorNome ("x86_64-windows" e o unico do M1).
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 */
#ifndef FORJA_NATIVO_ALVO_H
#define FORJA_NATIVO_ALVO_H

#include "common/buf.h"
#include "common/diag.h"
#include "common/vec.h"
#include "nativo/bir/bir.h"
#include "nativo/dbg/dbg.h"
#include "nativo/fmt/pe.h"
#include "nativo/obj/objeto.h"

/* Registros do --debug-code que so o gerador sabe (nao ha texto aqui: quem formata e nativo_debug.c). Os textos (nome da funcao, asm, simbolo)
 * ficam em 'textos', cada um com NUL, e os registros guardam o offset. Offsets sao em .text do PROGRAMA (a RVA final soma o inicio da secao). */
typedef enum { ORIGEM_PROLOGO, ORIGEM_SONDAGEM, ORIGEM_CODIGO, ORIGEM_FRIO, ORIGEM_EPILOGO } ListaOrigem;

#define LISTA_SEM_INDICE UINT32_MAX
#define LISTA_TAM_UNWIND 32u /* cabecalho de 4 + 14 slots de 2 bytes do x64_unwind.c (UNWIND_MAX_CODIGOS) */

typedef struct {
    uint32_t funcao;            /* indice em AlvoListagem.funcoes */
    uint32_t off, tam;          /* onde a instrucao caiu em .text e quantos bytes ocupa (os bytes finais vem da imagem ligada) */
    uint32_t pos;               /* offset global na fonte ou BIR_SEM_POS */
    uint32_t bloco, indice;     /* BIR: bloco e posicao dentro dele; LISTA_SEM_INDICE no prologo e no epilogo */
    uint32_t texto;             /* offset do asm (Intel) em 'textos' */
    uint32_t simbolo;           /* offset do nome do simbolo em 'textos' mais 1; 0 = a instrucao nao aponta para simbolo */
    uint8_t origem;             /* ListaOrigem */
} ListaInstr;

/* Slot de memoria do quadro (agregado ou escalar com '&'): o endereco e rbp - offset, o slot ocupa tam bytes dali para cima. */
typedef struct { uint32_t tam, alinhamento, offset; } ListaSlot;

typedef struct {
    uint32_t nome;              /* offset em 'textos' */
    uint32_t inicio, fim;       /* em .text; fim sem o padding */
    uint32_t pos;               /* BirFuncao.pos ou BIR_SEM_POS */
    uint32_t quadro, slots, maxargs;
    uint32_t push, fpreg, alocacao;
    uint8_t sondagem, tam_unwind;
    uint8_t unwind[LISTA_TAM_UNWIND];
    ListaSlot *slots_agregado;  /* nslots_agregado entradas na ordem dos slots da BIR; malloc, liberado por AlvoListagemLiberar */
    uint32_t nslots_agregado;
} ListaFuncao;

/* Quem pede chama AlvoListagemIniciar antes e AlvoListagemLiberar depois; o gerador so anexa. */
typedef struct {
    Vec funcoes;                /* ListaFuncao, na ordem do programa */
    Vec instrs;                 /* ListaInstr, na ordem em que sairam */
    Buf textos;
} AlvoListagem;

void AlvoListagemIniciar(AlvoListagem *lista);
void AlvoListagemLiberar(AlvoListagem *lista);

/* Tempo (ns) das fatias do gerador, somadas por FUNCAO (nunca por instrucao): regs = preparo da funcao e alocacao (degrau 0), isel = prologo e
 * corpo (selecao e codificacao instrucao a instrucao, fundidas), codificar = fechamento (saltos, unwind, padding, liberacao) e a montagem dos
 * simbolos e secoes do Objeto. As tres fatias particionam o X64Gerar inteiro. */
typedef struct { uint64_t isel_ns, regs_ns, codificar_ns; } AlvoTempos;

/* O que o gerador colhe alem do Objeto. Os ponteiros sao opcionais: lista NULL = sem listagem (custo zero), tempos NULL = sem medicao, dbg NULL = sem coleta de depuracao. */
typedef struct { AlvoListagem *lista; AlvoTempos *tempos; bool rastreio; DbgPrograma *dbg; bool guarda; } AlvoColheita;

typedef struct Alvo {
    const char *nome;
    /* colheita NULL = nem listagem nem medicao. */
    bool (*gerar)(const BirPrograma *, Objeto *, Diag *, const AlvoColheita *colheita);
    ImagemFormato formato;      /* o SO do alvo: IMAGEM_PE (Windows) ou IMAGEM_ELF (Linux); escolhe runtime, base, secoes e escritor */
    bool dll;                   /* o alvo gera biblioteca (--modo dll) */
    const char *sem_escritor;   /* NULL = o escritor do formato existe; senao a causa do X0003 em que o build para depois de ligar */
    BirNomesMem mem;            /* os nomes que o construtor e o gerador chamam para copiar e zerar bloco grande (D0654) */
} Alvo;

/* NULL se o nome nao e de um alvo conhecido. */
const Alvo *AlvoPorNome(const char *nome);

/* Os alvos na ordem da tabela (Windows primeiro): AlvoPorIndice(k) e NULL fora de 0 <= k < AlvoQtd(). */
uint32_t AlvoQtd(void);
const Alvo *AlvoPorIndice(uint32_t k);

#endif
