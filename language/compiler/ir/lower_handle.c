/* lower_handle.c - lowering do rastreio automatico de handle de lib C: IR_HANDLE_SOLTA antes e IR_HANDLE_NASCE depois da chamada a foreign com clausula creates/destroys/takes.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6d P92e: LwLibEncerra (IR_LIB_ENCERRA depois da chamada de foreign `ends`).
 *            0.1.0 - M6c P88 (D-M6c-6): LwHandleSolta e LwHandleNasce. Nenhum valor novo e calculado: os operandos sao os temps que a propria chamada ja usa; sem clausula nao emite nada.
 *            Handle struct por valor fica sem rastreio (o runtime nao guarda identidade de struct).
 * Verificacao: construir_rapido.py && python tests\nativo\m6c\verificar_m6c.py --forja compiler\forja.exe --alvo ambos --grupo handle
 */
#include "ir/lower_interno.h"
#include "semantic/sem_interno.h"

/* Handle que cabe num registrador: ponteiro (^T, ?^T), rawptr ou inteiro. */
static bool HandleEscalar(const LowerCtx *c, TipoId tipo)
{
    TipoClasse classe = TipoClasseDe(&c->sem->tipos, tipo);

    return TipoEhInteiro(tipo) || classe == TC_PONTEIRO || classe == TC_ANULAVEL || classe == TC_RAWPTR;
}

/* DEPOIS da chamada de um foreign `ends` (M6d P92e): IR_LIB_ENCERRA com o simbolo C; a lib passa a "encerrada" no runtime dev. */
void LwLibEncerra(LowerCtx *c, const Decl *proc, uint32_t pos)
{
    if (proc->posse_c != POSSE_C_ENCERRA)
        return;
    LwNovaInstr(c, IR_LIB_ENCERRA, pos)->nome = proc->simbolo >= 0 ? proc->simbolo : proc->nome;
}

/* ANTES da chamada: o argumento do parametro `destroys`/`takes` deixa de ser rastreado. */
void LwHandleSolta(LowerCtx *c, const Decl *proc, const Valor *args, uint32_t pos)
{
    if (proc->posse_c != POSSE_C_DESTROI && proc->posse_c != POSSE_C_ENTREGA)
        return;
    if (!HandleEscalar(c, proc->params[proc->posse_param].tipo.tipo))
        return;
    LwNovaInstr(c, IR_HANDLE_SOLTA, pos)->a = args[proc->posse_param];
}

/* DEPOIS da chamada (`chamada` ja esta no bloco): `creates` rastreia o valor devolvido; `creates p` rastreia o que a lib escreveu em *p. */
void LwHandleNasce(LowerCtx *c, const Decl *proc, const Valor *args, const IrInstr *chamada, uint32_t pos)
{
    bool retorno = proc->posse_c == POSSE_C_CRIA_RETORNO;
    TipoId tipo;
    IrInstr *nasce;
    const Decl *destruidor;

    if (!retorno && proc->posse_c != POSSE_C_CRIA_SAIDA)
        return;
    tipo = retorno ? proc->retorno.tipo : TipoDe(&c->sem->tipos, proc->params[proc->posse_param].tipo.tipo)->a;
    if (!HandleEscalar(c, tipo) || (retorno && chamada->destino < 0))
        return;
    nasce = LwNovaInstr(c, IR_HANDLE_NASCE, pos);
    nasce->nome = proc->simbolo >= 0 ? proc->simbolo : proc->nome; /* kind = o nome do criador na lib C (o alias aponta para ele) */
    nasce->op_aux = retorno ? 0 : 1;
    nasce->a = retorno ? LwValorTemp(chamada->destino, chamada->tipo) : args[proc->posse_param];
    destruidor = SemDestruidorDoTipo(c->sem, tipo);
    if (destruidor != NULL)
        nasce->b = LwValorTexto(destruidor->nome, TIPO_CSTR);
}
