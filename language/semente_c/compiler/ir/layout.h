/* layout.h - tamanho, alinhamento e offset dos membros dos tipos da Forja em C11: replica a ordem e os tamanhos que o backend x64 usa.
 * Versao: 0.4.1
 * Changelog: 0.4.1 - G3 G0b: quebra de ciclo ir<->layout (movido de layout/ para ir/).
 *            0.4.0 - 0.21.0 F4 passo 2b: texto da Verificacao (--modo c e --prova-layout saíram com o backend C); sem mudar codigo.
 *            0.3.0 - 0.21.0 F4 passo 2a: sai ProvaEntrada e LayoutEscreverProva (layout_prova.c apagado com o backend C); o layout segue no x64 (LayoutConferir, LAYOUT_MAPA_OFF_*).
 *            0.2.1 - M2 P17b: LAYOUT_MAPA_OFF_* (offsets dos membros do forja_mapa que o backend nativo usa).
 *            0.2.0 - M2 P11b: LayoutMembro e LayoutTipo.membros; LayoutConferir (produtor real do X0906 no pipeline nativo).
 *            0.1.1 - M2 P9a: LayoutEscreverProva recebe ProvaEntrada (regra de <= 4 parametros).
 *            0.1.0 - Linguagem v1.10 P1: LayoutCalcular (por valor, na ordem do emit_c), tamanho/alinhamento/offset e
 *                    LayoutEscreverProva (layout_prova.c) com _Static_assert por tipo com nome C.
 * Verificacao: python -I ..\tests\nativo\verificar_layout.py (dependia de build --modo c --prova-layout, aposentados na 0.21.0: o script nao roda mais)
 */
#ifndef FORJA_LAYOUT_H
#define FORJA_LAYOUT_H

#include "common/arena.h"
#include "common/buf.h"
#include "common/diag.h"
#include "common/intern.h"
#include "ir/ir.h"
#include "semantic/tipos.h"

/* Um por TipoId. tam = 0 marca tipo sem forma em C (vazio, veneno, struct opaca); ali vale 1 nesse caso.
 * offs: offset de cada membro da struct C, na ordem dela (resultado sem valor tem 2 membros). */
typedef struct { uint32_t tam, ali; } LayoutMembro;

typedef struct {
    uint32_t tam, ali;
    uint32_t qtd;
    uint32_t *offs;
    LayoutMembro *membros; /* tamanho e alinhamento de cada membro, na ordem de offs (LayoutConferir) */
    bool pronto;
} LayoutTipo;

typedef struct {
    LayoutTipo *tipos; /* indexado por TipoId; o slot 0 nao e usado */
    uint32_t qtd;      /* itens.n da tabela de tipos */
} Layouts;

/* forja_mapa (runtime nativo, forja_rt_nativo.h): o map e opaco no layout\ (sem membros), entao o backend nativo usa estes offsets dos membros que o codigo gerado le
 * (chaves, valores, len, epoca); NativoRtLayoutConferir os confere com o forja_rt_layout do runtime (X0906). */
#define LAYOUT_MAPA_OFF_CHAVES 0u
#define LAYOUT_MAPA_OFF_VALORES 8u
#define LAYOUT_MAPA_OFF_LEN 24u
#define LAYOUT_MAPA_OFF_EPOCA 48u

/* Calcula o layout de todos os tipos da tabela, com a IR para as structs e unions. Sem parametro de alvo:
 * Win e SysV dao o mesmo layout para estes tipos. */
void LayoutCalcular(const Tipos *t, const IrPrograma *p, Arena *a, Layouts *saida);

/* Tipo sem forma em C chama FalhaInterna. k e 0-based, na ordem da struct C. */
uint32_t LayoutTamanho(const Layouts *l, TipoId id);
uint32_t LayoutAlinhamento(const Layouts *l, TipoId id);
uint32_t LayoutOffsetCampo(const Layouts *l, TipoId id, uint32_t k);
uint32_t LayoutQtdMembros(const Layouts *l, TipoId id);

/* Confere a coerencia interna de cada layout com forma: alinhamento potencia de 2 e divisor do tamanho; em cada membro, alinhamento potencia de 2 que divide o offset
 * e offset + tamanho dentro do tamanho do tipo. false = X0906 em *d com o tipo e o campo (o primeiro que reprovar). */
bool LayoutConferir(const Layouts *l, const Tipos *t, Diag *d);

/* Struct e union de um TipoId (NULL se nao houver definicao na IR). */
const IrStruct *LayoutEstrutura(const Tipos *t, const IrPrograma *p, TipoId id);
const IrUniao *LayoutUniao(const Tipos *t, const IrPrograma *p, TipoId id);

#endif
