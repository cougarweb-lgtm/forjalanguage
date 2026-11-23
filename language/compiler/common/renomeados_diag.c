/* renomeados_diag.c - o diagnostico da recusa de uma forma antiga da tabela de renomeados (L3f; o flag 'recusar' fica false no L3c). Veja renomeados.h.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - IA1-L3f: RenomeadoRecusa e RenomeadoTolerarAntiga (o forja migrar le a forma antiga mesmo com recusar = true).
 *            0.2.0 - IA1-L3d1: RenTrocasIniciar, RenTrocasLiberar, RenomeadoNotar e RenomeadoNotarTabela (a lista de formas antigas do forja migrar).
 *            0.1.0 - IA1-L3c: RenomeadoRecusar (E0110 "forma antiga; rode forja migrar" com a troca pela forma nova).
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/renomeados.h"

#include <string.h>

static bool TolerarAntiga = false;

bool RenomeadoRecusa(const Renomeado *r)
{
    return r->recusar && !TolerarAntiga;
}

void RenomeadoTolerarAntiga(bool ligado)
{
    TolerarAntiga = ligado;
}

void RenTrocasIniciar(RenTrocas *t)
{
    VecIniciar(&t->itens, sizeof(RenTroca));
}

void RenTrocasLiberar(RenTrocas *t)
{
    VecLiberar(&t->itens);
}

void RenomeadoNotar(Diag *d, const RenTroca *troca)
{
    if (d->trocas == NULL) return;
    *(RenTroca *)VecEmpurrar(&d->trocas->itens) = *troca;
}

void RenomeadoNotarTabela(Diag *d, uint32_t pos, uint32_t tam, const Renomeado *r)
{
    RenomeadoNotar(d, &(RenTroca){.off = pos, .tam = tam, .classe = r->classe, .antigo = r->antigo, .novo = r->novo});
}

bool RenomeadoRecusar(Diag *d, uint32_t pos, uint32_t tam, const Renomeado *r)
{
    DiagRelatar(d, E0110, pos, "forma antiga: '%s' agora e '%s' (%s); rode forja migrar", r->antigo, r->novo, RenomeadoClasseTexto(r->classe));
    DiagSugerir(&d->sug, DIAG_CERTA, "troque '%s' por '%s': o nome foi renomeado; 'forja migrar' faz a troca no projeto todo", r->antigo, r->novo);
    if (strlen(r->novo) < DIAG_NOVO_MAX) DiagEditar(&d->sug, pos, pos + tam, r->novo);
    return false;
}
