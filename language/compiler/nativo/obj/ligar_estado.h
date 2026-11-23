/* ligar_estado.h - estado interno do ligador do backend nativo: o que ligar.c e ligar_elf.c leem e escrevem juntos (interno, nao e API publica).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5c-b P61: LIGAR_SAIDA_TLS (8), a saida unica do bloco TLS (.tdata com bytes e .tbss na cauda); LigacaoEstado.tls_alinhamento e tls_bloco.
 *            0.1.1 - M5b P37d: LIGAR_SAIDA_RDATA (1) e LIGAR_SAIDA_DATA (2), as saidas do ELF por tipo de secao (ligar_elf.c LigarSaidaElf).
 *            0.1.0 - M5b P36: LigacaoEstado, LigarContrib e as constantes de objeto e de saida saem de ligar.c (ligar.c passa de 794 para menos linhas); LigacaoEstado ganha
 *                    celulas (LigarCelula: as celulas .got das relocacoes GOTPCREL do ELF, na ordem da 1a referencia).
 */
#ifndef FORJA_NATIVO_OBJ_LIGAR_ESTADO_H
#define FORJA_NATIVO_OBJ_LIGAR_ESTADO_H

#include "common/vec.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/ligar_alcance.h"

#define LIGAR_QTD_OBJETOS ALCANCE_QTD_OBJETOS
#define LIGAR_PROGRAMA 0
#define LIGAR_RUNTIME 1
#define LIGAR_RYU 2
#define LIGAR_SEM_SAIDA (-1)
#define LIGAR_QTD_SAIDA 9
#define LIGAR_SAIDA_TEXTO 0
#define LIGAR_SAIDA_RDATA 1       /* M5b P37d: .rodata e o que o ELF classifica como leitura (OS_RDATA); no PE e o .rdata */
#define LIGAR_SAIDA_DATA 2        /* M5b P37d: OS_DATA do ELF (escrita, com ou sem .data.rel.ro); no PE e o .data */
#define LIGAR_SAIDA_PDATA 3
#define LIGAR_SAIDA_XDATA 4
#define LIGAR_SAIDA_BSS 5
#define LIGAR_SAIDA_INIT_ARRAY 6  /* M5b P36: .init_array dos membros ELF (RW, com bytes) */
#define LIGAR_SAIDA_GOT 7         /* M5b P36: celulas .got sinteticas do ELF (RW, ADDR64 do simbolo) */
#define LIGAR_SAIDA_TLS 8         /* M5c-b P61: o bloco TLS do ELF (.tdata e .tbss); no PE nunca tem contribuicao */
/* RVA ainda nao existente: simbolo numa secao vazia, que nao vira secao do PE (duas secoes no mesmo RVA recusam o exe). */
#define LIGAR_SEM_RVA UINT32_MAX
/* Raizes da alcancabilidade (G4): o programa inteiro e a entrada. */
#define LIGAR_QTD_RAIZES 2

/* Onde cada secao de objeto foi parar: a saida (LIGAR_SAIDAS de ligar.c) e o offset dentro dela; saida LIGAR_SEM_SAIDA = secao descartada. */
typedef struct {
    int32_t saida;
    uint32_t off;
} LigarContrib;

/* Uma celula .got: o simbolo (objeto e indice no objeto) cujo endereco ela guarda. Global com o mesmo nome em dois objetos e a mesma celula. */
typedef struct {
    int32_t objeto;
    uint32_t simbolo;
} LigarCelula;

/* Estado do ligador: os objetos, a alcancabilidade, a saida de cada secao de objeto e o RVA de cada simbolo resolvido. */
typedef struct {
    const Objeto *obj[LIGAR_QTD_OBJETOS];
    Imagem *im;
    Diag *erro;
    const LigarPedido *pedido;
    uint32_t tam[LIGAR_QTD_SAIDA];
    uint32_t rva[LIGAR_QTD_SAIDA];
    uint32_t idx_im[LIGAR_QTD_SAIDA];
    Vec contrib[LIGAR_QTD_OBJETOS];  /* LigarContrib: uma por secao de cada objeto */
    Vec rva_sim[LIGAR_QTD_OBJETOS];  /* uint32_t: RVA de cada simbolo de cada objeto */
    Vec celulas;                     /* LigarCelula: as celulas .got (so ELF; vazio nos demais) */
    uint32_t tls_alinhamento;        /* M5c-b P61: o maior alinhamento dos membros TLS (p_align do PT_TLS); 0 sem TLS */
    uint32_t tls_bloco;              /* tamanho do bloco TLS arredondado ao alinhamento: o TP fica logo depois dele (variante II do x86-64) */
    AlcanceEntrada entrada;
    Alcance alcance;
    ImportTabela tabelas[IMAGEM_MAX_DLLS];  /* a KERNEL32 do runtime e depois as dos 'link' (LigarPedido), na ordem dos descritores */
    uint32_t qtd_tabelas;
    const char *fundidos[ALCANCE_MAX_IMPORTS];  /* nomes da WS2_32 fixa fundidos com a 'link' da mesma DLL (LigarFundirWs2) */
    const char *plano[ALCANCE_MAX_IMPORTS];  /* os simbolos de todas as tabelas em fila: o indice e o bit de imports_usados */
    uint32_t qtd_plano;
    int32_t slot_import[ALCANCE_MAX_IMPORTS];  /* posicao do import do plano no .idata (so os alcancaveis entram); -1 = fora */
    uint32_t texto_runtime_off;      /* offset no .text da 1a contribuicao viva do runtime (LIGAR_SEM_RVA enquanto nao houve) */
    AlcanceRaiz raizes[LIGAR_QTD_RAIZES];  /* o programa e a entrada do PE (LigarEscolherEntrada) */
    int objeto_entrada;              /* o objeto que define a entrada: o runtime, ou o legado (construtores de terceiros) */
    const char *nome_entrada;
} LigacaoEstado;

#endif
