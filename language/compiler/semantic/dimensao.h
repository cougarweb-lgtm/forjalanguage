/* dimensao.h - dimensao de unidade: vetor esparso de (unidade base, expoente), internado (DimId comparavel por inteiro).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Fase 5c: substitui o vetor denso de 16 bases do prototipo por termos esparsos; sem limite de bases.
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\semantica\verificar_semantica.py
 *
 * DimId 0 = adimensional (f32 puro). Duas unidades de mesma dimensao compartilham o DimId (o mesmo tipo).
 * Uma dimensao so e "declarada" se alguma 'unit' lhe deu nome; o produto/quociente de declaradas pode nao ser.
 */
#ifndef FORJA_DIMENSAO_H
#define FORJA_DIMENSAO_H

#include "common/arena.h"
#include "common/vec.h"

typedef uint32_t DimId;
enum { DIM_ADIMENSIONAL = 0 };

typedef struct { uint16_t base; int16_t expoente; } DimTermo;
typedef struct { uint32_t inicio, qtd; int32_t nome; } DimItem; /* termos [inicio, inicio+qtd) em 'termos'; nome = 1a unit */

typedef struct {
    Vec itens;     /* DimItem, indexado por DimId */
    Vec termos;    /* DimTermo, ordenados por base dentro de cada item */
    Vec rascunho;  /* DimTermo temporario do DimCombinar */
    uint32_t *tabela, cap; /* hash aberto: DimId + 1 (0 = vazio) */
    uint32_t qtd_bases;
} DimTabela;

void DimIniciar(DimTabela *t);

/* Dimensao de uma unidade base nova (expoente 1 numa base nova), nomeada por 'nome'. */
DimId DimNovaBase(DimTabela *t, int32_t nome);

/* sinal +1 multiplica as dimensoes, -1 divide. Se for dimensao nova, nasce sem nome. */
DimId DimCombinar(DimTabela *t, DimId a, DimId b, int sinal);

/* Da o nome 'nome' a dimensao, se ela ainda nao tem (a 1a unit declarada vence nas mensagens). */
void DimNomear(DimTabela *t, DimId d, int32_t nome);

/* Nome da 1a unit declarada com essa dimensao, ou -1. */
int32_t DimNome(const DimTabela *t, DimId d);

/* Adimensional ou com unit declarada: pode ser tipo de variavel e resultado de * e /. */
bool DimDeclarada(const DimTabela *t, DimId d);

void DimLiberar(DimTabela *t);

#endif
