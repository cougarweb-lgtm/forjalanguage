/* ligar_alcance.h - alcancabilidade do ligador do backend nativo: quais secoes dos dois objetos e quais __imp_ entram no exe.
 * Versao: 0.17.0
 * Changelog: 0.17.0 - M8 P2: ALCANCE_QTD_OBJETOS 16 (o runtime do --perfil, LIGAR_PERFIL, no 16o slot, depois da semente).
 *            0.16.0 - M5b P39-B2: ALCANCE_QTD_OBJETOS 15 (crt1.o do musl, LIGAR_SEMENTE, no 15o slot, depois da entrada).
 *            0.15.0 - M5b P39: ALCANCE_QTD_OBJETOS 14 (o objeto de entrada do ELF, rt_linux_entrada_cru.o, no 14o slot, depois do net).
 *            0.14.0 - M5b P36: AlcanceIndiceSimboloDoLigador (simbolos que o ligador ELF define).
 *            0.13.0 - M5 P53: ALCANCE_QTD_OBJETOS 13 (rt_win_net.o, std:net UDP, depois do varena).
 *            0.12.0 - M5 P52: ALCANCE_QTD_OBJETOS 12 (rt_win_varena.o, std:mem VirtualArena, depois do atomic).
 *            0.11.0 - M5 P50: ALCANCE_QTD_OBJETOS 11 (rt_win_atomic.o, std:atomic, depois do sync).
 *            0.10.0 - M5 P49: ALCANCE_QTD_OBJETOS 10 (rt_win_sync.o, std:sync, depois da thread).
 *            0.8.1 - POS-Z A52: ALCANCE_MAX_IMPORTS de 256 para 320 (a KERNEL32 do runtime ja tem 60 nomes e a ADVAPI32 1; os 200 do verificar_abi nao cabiam).
 *            0.9.0 - M5 P47: ALCANCE_QTD_OBJETOS 9 (rt_win_thread.o, std:thread, depois do es).
 *            0.8.0 - S3 P5b-1: ALCANCE_QTD_OBJETOS 8 (o handle e o fs do std:file, rt_win_es.o, depois da libm).
 *            0.7.0 - L11a-2: ALCANCE_QTD_OBJETOS 7 (a libm do musl, rt_math.o, depois do curto).
 *            0.6.0 - IA1-L5a: ALCANCE_QTD_OBJETOS 6 (o curto, rt_win_curto.o, depois do legado).
 *            0.5.0 - M3b P23f: ALCANCE_QTD_OBJETOS 5 (o legado, rt_win_legado.o, depois dos terceiros).
 *            0.4.0 - M3b P23e: ALCANCE_QTD_OBJETOS 4 (programa, runtime, Ryu, terceiros); um __imp_X definido por outro objeto (o slot que o terceiros cria para uma estatica) vence a tabela de imports.
 *            0.3.0 - M3 F5: ALCANCE_QTD_OBJETOS 3 (programa, runtime, Ryu).
 *            0.2.0 - M3 P19: imports_usados vira bitmap de 256 bits (era a mascara de 32: a KERNEL32 do runtime ja usa 17 e as DLLs dos 'link' somam); AlcanceImportUsado.
 *            0.1.0 - M2 P17c (gancho G4): AlcanceCalcular parte de uma tabela de raizes e segue as relocacoes COFF; .pdata sobe com o codigo que descreve.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#ifndef FORJA_NATIVO_OBJ_LIGAR_ALCANCE_H
#define FORJA_NATIVO_OBJ_LIGAR_ALCANCE_H

#include "nativo/obj/objeto.h"

#define ALCANCE_QTD_OBJETOS 16
#define ALCANCE_PREFIXO_IMP "__imp_"
#define ALCANCE_TAM_PREFIXO_IMP 6
/* imports_usados e um bitmap de palavras de 32 bits: a tabela de imports tem de caber nele (igual a IMAGEM_MAX_IMPORTS do pe.h; o ligar.c confere). */
#define ALCANCE_MAX_IMPORTS 320
#define ALCANCE_BITS_PALAVRA 32u
#define ALCANCE_PALAVRAS_IMPORTS (ALCANCE_MAX_IMPORTS / ALCANCE_BITS_PALAVRA)

/* Uma raiz: o simbolo global do objeto (a secao dele entra) ou, com simbolo NULL, todas as secoes do objeto. */
typedef struct {
    int objeto;
    const char *simbolo;
} AlcanceRaiz;

/* O que entra no calculo: os dois objetos (o 0 e o 1 se resolvem entre si), a tabela de raizes e a tabela de __imp_ aceitos. */
typedef struct {
    const Objeto *obj[ALCANCE_QTD_OBJETOS];
    const AlcanceRaiz *raizes;
    uint32_t qtd_raizes;
    const char *const *imports;
    uint32_t qtd_imports;
} AlcanceEntrada;

typedef struct {
    const AlcanceEntrada *entrada;
    Vec vivo[ALCANCE_QTD_OBJETOS];  /* uint8_t por secao: 1 = alcancavel */
    Vec ini[ALCANCE_QTD_OBJETOS];   /* uint32_t, secoes+1: primeira posicao de ord[] das relocacoes de cada secao */
    Vec ord[ALCANCE_QTD_OBJETOS];   /* uint32_t: indices de relocacao agrupados por secao, na ordem original */
    Vec pilha;                      /* secoes vivas ainda sem as relocacoes seguidas */
    uint32_t imports_usados[ALCANCE_PALAVRAS_IMPORTS];  /* bit k (palavra k/32, bit k%32) ligado: entrada->imports[k] e referido por secao viva; use AlcanceImportUsado */
    const char *indefinido;         /* 1o simbolo referido por secao viva sem definicao em nenhum objeto (nem __imp_); NULL se nao houve */
    const char *import_fora;        /* 1o __imp_ referido por secao viva que nao esta em entrada->imports; NULL se nao houve */
} Alcance;

/* Marca as secoes alcancaveis a partir das raizes. entrada tem de viver ate AlcanceLiberar. */
void AlcanceCalcular(const AlcanceEntrada *entrada, Alcance *a);

/* A secao sec do objeto o (0 ou 1) e alcancavel? */
bool AlcanceVivo(const Alcance *a, int o, uint32_t sec);

/* O import k da tabela (entrada->imports[k], k < ALCANCE_MAX_IMPORTS) e referido por alguma secao viva? */
bool AlcanceImportUsado(const Alcance *a, uint32_t k);

void AlcanceLiberar(Alcance *a);

/* Simbolo que o proprio ligador define (ELF: __init_array_start e similares): indice 0..4 na lista de alcance_ligar.c, ou -1 se o nome nao e um deles. */
int AlcanceIndiceSimboloDoLigador(const char *nome);

#endif
