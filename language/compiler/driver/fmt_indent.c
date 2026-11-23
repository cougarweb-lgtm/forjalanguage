/* fmt_indent.c - a indentacao do 'forja fmt' (R5 do plano f1/f2): nivel = quantos ( [ { estao abertos no comeco da linha; a linha que comeca com fechamento desce 1;
 * 'case' e 'default' ficam no nivel do 'switch'.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - F1 P1a: primeira versao.
 * Verificacao: construir.bat && python tests\fmt\verificar_fmt.py --forja compiler\forja.exe
 */
#include <stdlib.h>

#include "driver/fmt_interno.h"

typedef enum { ABERTA_COMUM, ABERTA_SWITCH } Aberta;

/* As aberturas ainda sem fechar (abertas[0..topo)) e se um 'switch' espera a sua '{'. */
typedef struct {
    uint8_t *abertas;
    size_t topo;
    bool switch_pendente;
} Pilha;

static bool EhAbertura(uint8_t tipo)
{
    return tipo == TK_LPAREN || tipo == TK_LCOLCHETE || tipo == TK_LBRACE;
}

/* O nivel do token se ele abrir a linha: aberturas pendentes, menos 1 se ele fecha, ou se e 'case'/'default' direto no switch. */
static uint32_t NivelDoToken(const FmtToken *t, const Pilha *p)
{
    uint32_t nivel = (uint32_t)p->topo;
    bool rotulo = (t->tipo == KW_CASE || t->tipo == KW_DEFAULT) && p->topo > 0 && p->abertas[p->topo - 1] == ABERTA_SWITCH;

    if (nivel > 0 && (FmtPapelFecha((FmtPapel)t->papel) || rotulo)) nivel--;
    return nivel;
}

/* Depois do token: abre (a '{' de bloco logo depois de um 'switch' e a do switch) ou fecha. */
static void AtualizarPilha(const FmtToken *t, Pilha *p)
{
    if (t->tipo == KW_SWITCH) p->switch_pendente = true;
    if (EhAbertura(t->tipo)) {
        p->abertas[p->topo++] = t->papel == PAPEL_LK && p->switch_pendente ? ABERTA_SWITCH : ABERTA_COMUM;
        if (t->papel == PAPEL_LK) p->switch_pendente = false;
    } else if (FmtPapelFecha((FmtPapel)t->papel) && p->topo > 0) {
        p->topo--;
    }
}

void FmtCalcularNiveis(FmtToken *t, size_t n)
{
    Pilha p = {.abertas = calloc(n + 1, 1)};

    if (p.abertas == NULL) FalhaInterna("memoria esgotada calculando a indentacao do fmt");
    for (size_t i = 0; i < n; i++) {
        t[i].nivel = NivelDoToken(&t[i], &p);
        AtualizarPilha(&t[i], &p);
    }
    free(p.abertas);
}
