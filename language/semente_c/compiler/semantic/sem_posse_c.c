/* sem_posse_c.c - clausula de posse de handle do proc foreign (creates, creates p, destroys p, takes p): confere o TIPO (E0912).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M6d P92b: 'ends' (POSSE_C_ENCERRA) nao exige tipo de handle.
 *            0.1.0 - M6c P85 (D-M6c-3/4): SemChecarPosseC. A sintaxe (palavra, nome existente, uma clausula) o parser ja conferiu.
 * Verificacao: construir_rapido.py && python tests\semantica\verificar_semantica.py (err_E0912_*)
 */
#include "semantic/sem_interno.h"

/* Handle de lib C: ponteiro (^T, ?^T), rawptr, inteiro ou struct por valor. cstr, texto, bool, real e listas nao sao. */
static bool TipoEhHandleDeLibC(const Semantica *s, TipoId t)
{
    if (TipoEhInteiro(t)) return true;
    TipoClasse c = TipoClasseDe(&s->tipos, t);
    return c == TC_PONTEIRO || c == TC_ANULAVEL || c == TC_RAWPTR || c == TC_STRUCT;
}

/* Confere o tipo do alvo da clausula (o retorno, ou o parametro posse_param; em 'creates p' o parametro e ^H e o handle e o H). */
bool SemChecarPosseC(Semantica *s, const Decl *d)
{
    bool saida = d->posse_c == POSSE_C_CRIA_SAIDA;
    if (d->posse_c == POSSE_C_ENCERRA) return true; /* M6d P92b: 'ends' nao tem handle (encerra a lib inteira) */
    TipoId alvo = d->posse_param < 0 ? d->retorno.tipo : d->params[d->posse_param].tipo.tipo;
    char texto[96];

    if (saida) {
        if (TipoClasseDe(&s->tipos, alvo) != TC_PONTEIRO) {
            SemDescrever(s, alvo, texto, sizeof texto);
            return SemErro(s, E0912, d->pos_posse, "'creates %s' pede um parametro de saida ^H (o handle que a lib escreve em *p), mas o parametro e %s",
                           SemNomeTexto(s, d->params[d->posse_param].nome), texto);
        }
        alvo = TipoDe(&s->tipos, alvo)->a;
    }
    if (TipoEhHandleDeLibC(s, alvo)) return true;
    SemDescrever(s, alvo, texto, sizeof texto);
    return SemErro(s, E0912, d->pos_posse, "%s nao e um handle de lib C: a clausula de posse vale para ^T, ?^T, rawptr, inteiro ou struct por valor",
                   texto);
}
