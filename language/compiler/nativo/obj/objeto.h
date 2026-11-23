/* objeto.h - objeto relocavel abstrato do backend nativo: secoes, simbolos e relocacoes, antes de ligar.
 * Versao: 0.5.0
 * Changelog: 0.5.0 - M6 P66: OS_DEBUG_ABBREV, OS_DEBUG_INFO, OS_DEBUG_LINE e OS_DEBUG_FRAME (secoes de debug do programa, nao alocadas; o ligador as copia para a Imagem
 *                    e a relocacao delas e ADDR64 absoluto, nunca RELATIVE: D-M6-4). Ficam no fim do enum; os valores dos outros nao mudam.
 *            0.4.0 - M5b P35: tipos novos do leitor ELF. OS_TDATA, OS_TBSS e OS_INIT_ARRAY; OR_ABS32, OR_ABS32S, OR_GOTPCREL, OR_GOTPCRELX, OR_TPOFF32 e OR_GOTTPOFF.
 *                    Amplia o D-R6 (que listava so OR_ABS32S, OR_GOTPCREL, OR_TPOFF32, OR_GOTTPOFF e os OS_): OR_ABS32 entra porque R_X86_64_32 esta no censo do libc.a, e
 *                    OR_GOTPCRELX separa GOTPCRELX/REX_GOTPCRELX (relaxam para lea no P36) de GOTPCREL (ganha celula .got). ObjSimbolo.fraco (binding WEAK do ELF).
 *                    Valores dos existentes inalterados; os novos vao no fim dos enums.
 *            0.3.0 - M3b P23e: ObjTipoReloc ganha OR_SECTION (indice da secao do PE, 2 bytes) e OR_SECREL (offset dentro da secao do PE, 4 bytes), dos objetos de terceiros.
 *            0.2.0 - M2 P17c: o nome do simbolo vai para um Interner (tabela hash FNV-1a): ObjAcharSimbolo, publico, deixa de ser busca linear
 *                    (ObjSimboloDefinir/Referir e o ligador eram O(S^2) com 200 mil funcoes); o indice do simbolo e o id internado.
 *            0.1.0 - backend x64 P3: ObjIniciar/ObjLiberar, ObjSecaoAbrir, ObjSimboloDefinir/Referir e ObjRelocar.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_OBJETO_H
#define FORJA_NATIVO_OBJ_OBJETO_H

#include "common/arena.h"
#include "common/buf.h"
#include "common/intern.h"
#include "common/vec.h"

/* Simbolo que so foi referido e ainda nao tem secao. */
#define OBJ_SECAO_INDEFINIDA (-1)

typedef enum { OS_TEXTO, OS_RDATA, OS_DATA, OS_BSS, OS_PDATA, OS_XDATA, OS_TDATA, OS_TBSS, OS_INIT_ARRAY, OS_DEBUG_ABBREV, OS_DEBUG_INFO, OS_DEBUG_LINE, OS_DEBUG_FRAME } ObjTipoSecao;
typedef enum { OR_REL32, OR_ADDR64, OR_ADDR32NB, OR_SECTION, OR_SECREL, OR_ABS32, OR_ABS32S, OR_GOTPCREL, OR_GOTPCRELX, OR_TPOFF32, OR_GOTTPOFF } ObjTipoReloc;

typedef struct {
    const char *nome;
    ObjTipoSecao tipo;
    uint32_t alinhamento;
    Buf bytes;         /* conteudo da secao; vazio em OS_BSS */
    uint32_t tam_bss;  /* tamanho virtual de OS_BSS (quem monta o objeto preenche) */
} ObjSecao;

typedef struct {
    const char *nome;  /* nome que comeca com '.' e local; sem o ponto, global */
    int32_t secao;     /* OBJ_SECAO_INDEFINIDA enquanto so houver referencia */
    uint32_t off;      /* offset dentro da secao */
    bool global;
    bool fraco;        /* binding WEAK do ELF (COFF: false; o fraco do COFF segue por CoffFraco) */
} ObjSimbolo;

/* Lugar do campo a corrigir: secao e offset dentro dela. */
typedef struct { uint32_t secao; uint32_t off; } ObjAlvoReloc;

/* O que corrigir: tipo, simbolo alvo e adendo. O adendo fica sempre no campo da relocacao, nunca embutido nos bytes. */
typedef struct { ObjTipoReloc tipo; uint32_t simbolo; int64_t adendo; } ObjCorrecao;

typedef struct { ObjAlvoReloc alvo; ObjCorrecao correcao; } ObjReloc;

typedef struct {
    Arena nomes;       /* dono dos nomes de secao e de simbolo */
    Vec secoes;        /* ObjSecao */
    Vec simbolos;      /* ObjSimbolo */
    Vec relocs;        /* ObjReloc */
    Interner indice;   /* nome -> indice do simbolo (um id por simbolo, na ordem de criacao); o texto mora em 'nomes' */
} Objeto;

void ObjIniciar(Objeto *o);
void ObjLiberar(Objeto *o);

/* Abre uma secao e devolve o indice dela. alinhamento tem de ser potencia de 2 (FalhaInterna se nao for). */
uint32_t ObjSecaoAbrir(Objeto *o, const char *nome, ObjTipoSecao tipo, uint32_t alinhamento);

/* Indice do simbolo de mesmo nome, ou -1 se nao existe (tempo constante, nunca cria). */
int64_t ObjAcharSimbolo(const Objeto *o, const char *nome);

/* Define o simbolo (cria se nao existe) e devolve o indice. Definir duas vezes e erro interno (FalhaInterna). */
uint32_t ObjSimboloDefinir(Objeto *o, const char *nome, int32_t secao, uint32_t off);

/* Devolve o indice do simbolo, criando-o indefinido se ainda nao existe. */
uint32_t ObjSimboloReferir(Objeto *o, const char *nome);

/* Registra uma relocacao. simbolo e secao precisam existir (FalhaInterna se nao). */
void ObjRelocar(Objeto *o, ObjAlvoReloc alvo, ObjCorrecao correcao);

#endif
