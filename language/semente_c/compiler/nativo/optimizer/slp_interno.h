/* slp_interno.h - tipos e funcoes comuns dos arquivos do passe `slp` (slp.c, slp_arvore.c, slp_seguro.c, slp_emitir.c). Ver slp.h.
 * Versao: 0.1.2
 * Changelog: 0.1.2 - S4b: SlpCtx.escalariza (veredito real do sroa; a carga contigua so recusa se o sroa escalarizaria o slot).
 *            0.1.1 - S4a: SlpAcesso.desloc em int64; SlpSlotEscalarizavel.
 *            0.1.0 - S3 (D0703): primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 */
#ifndef FORJA_NATIVO_OPTIMIZER_SLP_INTERNO_H
#define FORJA_NATIVO_OPTIMIZER_SLP_INTERNO_H

#include <stdbool.h>
#include <stdint.h>

#include "nativo/bir/bir.h"

#define SLP_SEM UINT32_MAX
#define SLP_LANES_MAX 4u
#define SLP_SOMEM_MAX 128u   /* instrucoes escalares que o grupo apaga */
#define SLP_NOS_MAX 64u      /* nos do grafo vetorial */
#define SLP_PEND_MAX 64u     /* vregs lidos cujo valor precisa ser o mesmo de ponta a ponta */
#define SLP_SLOT_PEQUENO_BYTES 32u /* o sroa aceita ate 64 B e 16 campos; 32 B deixa o mat4 f32 (64 B) vetorizar */
#define SLP_ACESSOS_MAX (SLP_SOMEM_MAX + SLP_LANES_MAX)

/* Contexto de uma funcao: contagens globais por vreg (definicoes e leituras) e a definicao de quem tem uma so. bloco = o bloco em analise. */
typedef struct {
    const BirPrograma *p;
    BirFuncao *f;
    uint32_t bloco;
    uint32_t *ndefs, *nleit, *def_b, *def_k;
    bool *escalariza; /* [f->nslots] veredito do sroa (OtimSroaVeredito), calculado 1x por funcao com semente; NULL = nao calculado */
} SlpCtx;

/* Base de um acesso a memoria: o slot (END_SLOT de definicao unica), o simbolo (END_SIMBOLO de definicao unica) ou o vreg com a posicao da definicao que vale no bloco. */
typedef enum { SB_SLOT, SB_SIMBOLO, SB_VREG } SlpBaseTipo;
typedef struct { uint8_t tipo; uint32_t id, def; int32_t off; } SlpChave; /* off = soma dos `add base, imm` seguidos ate a base (o endereco real e base + off + desloc da instrucao) */

/* Um acesso do grupo (carga lida ou gravacao feita) para a conferencia de alias. */
typedef struct { uint32_t pos; SlpChave chave; int64_t desloc; uint32_t tam; bool escrita; } SlpAcesso;

typedef enum { SN_CARGA, SN_CARGA_REPETIDA, SN_CONSTANTE, SN_VREG, SN_OPERACAO } SlpNoTipo;

/* No do grafo vetorial. modelo = posicao da carga da lane 0 (SN_CARGA, SN_CARGA_REPETIDA); valor = operando difundido (SN_CONSTANTE, SN_VREG); filho[] = nos dos operandos (SN_OPERACAO). */
typedef struct { uint8_t tipo, op; uint32_t filho[2]; uint32_t modelo; BirOperando valor; } SlpNo;

/* Vreg que o grupo le sem definir e que precisa valer o mesmo da primeira leitura ate a gravacao: def = posicao da definicao no bloco (SLP_SEM = definida fora). */
typedef struct { uint32_t vreg, def; } SlpPendente;

typedef struct {
    uint32_t lanes;
    uint8_t classe;                       /* BC_F32 (4 lanes) ou BC_F64 (2 lanes) */
    uint32_t loja[SLP_LANES_MAX];         /* posicao da GRAVA de cada lane, em ordem de deslocamento */
    uint32_t somem[SLP_SOMEM_MAX];        /* posicoes das instrucoes (nao gravacoes) que o vetor substitui */
    uint32_t nsomem;
    uint32_t lidos[SLP_SOMEM_MAX];        /* vreg de cada leitura que consumiu uma definicao da arvore (inclui a leitura da gravacao) */
    uint32_t nlidos;
    SlpNo no[SLP_NOS_MAX];                /* em pos-ordem: filho antes do pai */
    uint32_t nno, raiz;
    SlpPendente pend[SLP_PEND_MAX];
    uint32_t npend;
    const char *motivo;                   /* causa da recusa */
} SlpPlano;

/* ---- slp_arvore.c ---- */
void SlpContar(SlpCtx *c);
void SlpLiberar(SlpCtx *c);
uint32_t SlpDefNoBloco(const SlpCtx *c, uint32_t antes, uint32_t vreg);
SlpChave SlpChaveBase(const SlpCtx *c, uint32_t vreg, uint32_t pos);
bool SlpMesmaChave(const SlpChave *x, const SlpChave *y);
bool SlpSlotEscalarizavel(const SlpCtx *c, const SlpChave *chave); /* base e um slot de ate SLP_SLOT_PEQUENO_BYTES: o sroa o desfaz em escalares */
bool SlpMontarArvore(const SlpCtx *c, SlpPlano *pl); /* pl->classe, lanes e loja[] preenchidos */

/* ---- slp_seguro.c ---- */
bool SlpConferirPlano(const SlpCtx *c, SlpPlano *pl);

/* ---- slp_emitir.c ---- */
void SlpAplicarPlano(SlpCtx *c, const SlpPlano *pl);

#endif
