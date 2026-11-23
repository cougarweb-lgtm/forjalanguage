/* sem_posse_handle.c - E0317 de handle de lib C: o valor de proc foreign 'creates' (ou escrito por 'creates p') que fica num LOCAL precisa ser fechado
 * ('destroys') ou entregue ('takes', return, campo) antes da saida. Estatico e SEM fluxo, na maquina de sem_posse.c (LocalRastreado, origem ALOC_HANDLE).
 * Versao: 0.1.3
 * Changelog: 0.1.3 - M6d P92b (D-M6d-9): a guarda tambem vale como 'if h == 0' quando o handle e de tipo inteiro (mesma pilha de guardas, then direto).
 *            0.1.2 - M6c P91b: f(p = &h) com 'creates p' e h ainda VIVO = E0317 de reatribuicao (o handle antigo sumia sem aviso); texto compartilhado com SemReatribuicaoDoHandle.
 *            0.1.1 - M6c P88: DestruidorDoTipo vira publica (SemDestruidorDoTipo): o lowering a usa para o nome do destruidor no rastreio automatico.
 *            0.1.0 - M6c P87 (D-M6c-5): SemLetCriaHandle, SemHandleDaSaida, SemPosseDoHandle, SemReatribuicaoDoHandle, SemErroDoHandleVivo, SemEntregaDoArgumento e a guarda 'if h == none'.
 *            Limite aceito (R-M6c-3): handle passado a proc da Forja que o fecha e falso E0317 no chamador; a forma_certa manda fechar onde abriu ou guardar num campo.
 * Verificacao: construir_rapido.py && python tests\semantica\verificar_semantica.py (ok_handle_*, err_handle_*)
 */
#include "semantic/sem_interno.h"

/* A chamada (sem embrulho de ?T nem try) e de proc declarado: devolve a Decl dele; NULL nos demais. */
static const Decl *DeclDaChamada(const Semantica *s, const Expr *e)
{
    if (e->tipo_no == EX_EMBRULHA || e->tipo_no == EX_TRY) e = e->a;
    if (e->tipo_no != EX_CHAMADA || e->ref != REF_PROC) return NULL;
    const Simbolo *y = SemAcharSimbolo(s, e->nome, e->modulo);
    return y == NULL ? NULL : y->decl;
}

/* ?^T e ^T contam como T: o destruidor pode declarar o parametro com ou sem '?'. */
static TipoId BaseDoHandle(const Semantica *s, TipoId t)
{
    for (;;) {
        TipoClasse classe = TipoClasseDe(&s->tipos, t);
        if (classe != TC_PONTEIRO && classe != TC_ANULAVEL) return t;
        t = TipoDe(&s->tipos, t)->a;
    }
}

/* 1o proc foreign visivel com 'destroys' cujo parametro tem o mesmo tipo-base do handle; NULL se nenhum. */
const Decl *SemDestruidorDoTipo(const Semantica *s, TipoId tipo)
{
    TipoId base = BaseDoHandle(s, tipo);
    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no != D_PROC || d->posse_c != POSSE_C_DESTROI) continue;
        if (BaseDoHandle(s, d->params[d->posse_param].tipo.tipo) == base) return d;
    }
    return NULL;
}

bool SemEntregaDoArgumento(const Decl *proc, int32_t indice)
{
    return (proc->posse_c == POSSE_C_DESTROI || proc->posse_c == POSSE_C_ENTREGA) && indice == proc->posse_param;
}

/* E0317 do handle que sai vivo (fim do bloco ou return/break/continue), com a forma_certa concreta: defer <destruidor>(h) logo depois do let. */
bool SemErroDoHandleVivo(Semantica *s, const LocalRastreado *r, uint32_t pos, bool saida)
{
    const char *nome = SemNomeTexto(s, r->nome);
    const Decl *destruidor = SemDestruidorDoTipo(s, r->tipo);
    char forma[160];
    if (r->de_chamada) {
        snprintf(forma, sizeof forma, "feche no proc que abriu ou guarde-o num campo");
    } else if (destruidor != NULL) {
        snprintf(forma, sizeof forma, "defer %s(%s) logo depois do let", SemNomeTexto(s, destruidor->nome), nome);
    } else {
        snprintf(forma, sizeof forma, "chame o proc da lib que fecha o handle de '%s' (destroys) logo depois do let", nome);
    }
    const char *situacao = saida ? "esta saida (return, break ou continue) o deixa aberto" : "nao e fechado nem entregue ate o fim do bloco";
    if (r->de_chamada) {
        SemErro(s, E0317, pos, "'%s' guarda um handle de lib C que foi passado a um proc da Forja e %s: a posse nao atravessa procs; feche onde abriu ou guarde-o num campo", nome, situacao);
    } else {
        SemErro(s, E0317, pos, "'%s' guarda um handle de lib C que %s: feche (destroys), entregue (takes, return, campo) ou use defer", nome, situacao);
    }
    if (s->diag->tem && s->diag->codigo == E0317) DiagForma(s->diag, NULL, forma);
    return false;
}

/* let/var h = f(...) com f 'creates' (retorno): o local e o dono do handle. */
bool SemLetCriaHandle(Semantica *s, const Stmt *st)
{
    const Decl *proc = DeclDaChamada(s, st->expr);
    return proc != NULL && proc->posse_c == POSSE_C_CRIA_RETORNO;
}

/* Handle ainda vivo que seria sobrescrito (h = f(), ou f(p = &h) com h ja aberto): o antigo vazaria. */
static bool ErroHandleAindaAberto(Semantica *s, uint32_t pos, const LocalRastreado *r)
{
    return SemErro(s, E0317, pos, "'%s' ainda guarda um handle de lib C aberto: feche (destroys) ou entregue antes de reatribuir", SemNomeTexto(s, r->nome));
}

/* f(..., p = &h) com f 'creates p': h passa a ser o dono do handle que a lib escreveu em *p. Qualquer outro argumento = E0912. */
bool SemHandleDaSaida(Semantica *s, const Expr *chamada, const Decl *proc)
{
    const Expr *valor = NULL;
    for (uint32_t k = 0; k < chamada->nargs; k++) {
        if (chamada->args[k].indice == proc->posse_param) valor = chamada->args[k].valor;
    }
    if (valor == NULL) return true;
    const Expr *local = valor->tipo_no == EX_UNARIO && valor->op == OP_ENDERECO ? valor->a : NULL;
    if (local == NULL || local->tipo_no != EX_NOME || local->ref != REF_LOCAL || local->modulo != 0) {
        return SemErro(s, E0912, valor->pos, "o handle criado por '%s' vai para um local: &h", SemNomeTexto(s, proc->nome));
    }
    LocalRastreado *r = SemAcharRastreado(s, local);
    if (r != NULL) {
        if (r->origem == ALOC_HANDLE && r->estado == POSSE_VIVO) return ErroHandleAindaAberto(s, local->pos, r);
        r->estado = POSSE_VIVO; /* o mesmo local abre de novo */
        return true;
    }
    r = VecEmpurrar(&s->rastreados);
    *r = (LocalRastreado){local->ref_id, local->nome, local->pos, local->tipo, ALOC_HANDLE, POSSE_VIVO, 0};
    return true;
}

/* O nome rastreado de handle chega a um ponto de posse: copia = dois donos; destroys/takes, return e campo entregam; o resto empresta. */
bool SemPosseDoHandle(Semantica *s, const Expr *e, LocalRastreado *r, PontoDePosse ponto)
{
    if (ponto == PONTO_COPIA) {
        const char *nome = SemNomeTexto(s, r->nome);
        return SemErro(s, E0317, e->pos, "'%s' e dono de um handle de lib C: copiar cria dois donos; entregue (destroys, takes, return, campo) ou passe-o a um proc que so empresta", nome);
    }
    if (ponto == PONTO_EMPRESTIMO_FORJA) r->de_chamada = 1;
    if (ponto == PONTO_EMPRESTIMO || ponto == PONTO_EMPRESTIMO_FORJA) return true;
    r->estado = POSSE_ENTREGUE;
    return true;
}

/* h = valor: handle ainda vivo vazaria; o valor novo de 'creates' reabre a posse. */
bool SemReatribuicaoDoHandle(Semantica *s, const Stmt *st, LocalRastreado *r)
{
    if (r->estado == POSSE_VIVO) {
        return ErroHandleAindaAberto(s, st->alvo->pos, r);
    }
    const Decl *proc = DeclDaChamada(s, st->expr);
    if (proc != NULL && proc->posse_c == POSSE_C_CRIA_RETORNO) r->estado = POSSE_VIVO;
    return true;
}

/* Literal inteiro 0 escrito como um digito so (o "nada" do handle inteiro: 'if h == 0'). */
static bool EhZeroLiteral(const Semantica *s, const Expr *e)
{
    return e->tipo_no == EX_INT && e->op == 1 && s->fonte->texto[e->pos_op] == '0';
}

/* 'if h == none' (handle ponteiro) ou 'if h == 0' (handle inteiro): h e um handle vivo? Ele vira GUARDADO enquanto o then roda (fopen que falhou nao tem o que fechar). */
int32_t SemGuardaNoneAbrir(Semantica *s, const Expr *cond)
{
    if (cond->tipo_no != EX_BINARIO || cond->op != OP_IGUAL) return -1;
    const Expr *lado = cond->a->tipo_no == EX_EMBRULHA ? cond->a->a : cond->a;
    const Expr *nada = cond->b->tipo_no == EX_EMBRULHA ? cond->b->a : cond->b;
    LocalRastreado *r = SemAcharRastreado(s, lado);
    if (r == NULL || r->origem != ALOC_HANDLE || r->estado != POSSE_VIVO) return -1;
    bool zero_de_inteiro = TipoEhInteiro(r->tipo) && EhZeroLiteral(s, nada); /* M6d P92b (D-M6d-9) */
    if (nada->tipo_no != EX_NONE && !zero_de_inteiro) return -1;
    r->estado = POSSE_GUARDADO;
    return r->local_id;
}

/* Fim do then: o handle que ninguem fechou ali volta a VIVO (o resto do bloco ainda o fecha). */
void SemGuardaNoneFechar(Semantica *s, int32_t local_id)
{
    for (size_t k = s->rastreados.n; k > 0; k--) {
        LocalRastreado *r = VecEm(&s->rastreados, k - 1);
        if (r->local_id != local_id) continue;
        if (r->estado == POSSE_GUARDADO) r->estado = POSSE_VIVO;
        return;
    }
}
