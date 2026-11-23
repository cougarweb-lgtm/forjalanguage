/* sem_callback.c - proc como valor (v1.8b callback de foreign; v1.12 P21v-a valor de proc e chamada indireta): tipo 'proc(nome: T, ...) -> R',
 * o nome de proc da Forja como valor e a chamada sobre expressao de tipo proc.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - K08a: SemProcComoCallback pula o E0909 (ConferirAssinatura) so quando o nome e o comparador de sort_by (comparador_sort_by); foreign e demais callbacks seguem iguais.
 * Changelog: 0.2.0 - Linguagem v1.12 M3 P21v-a: o tipo proc vale em campo, let/var, parametro e retorno (so no topo do tipo: E0907 em ?proc, colecao, ^ e dentro de outro proc);
 *                    E0909 (assinatura fora da fronteira C), nome de proc como valor em qualquer posicao (sem captura; foreign nao vale), chamada indireta
 *                    (SemChamadaInd, SemConverterEmChamadaInd), E0911 (SemProcSemZero), os nomes dos parametros no tipo. O lowering recusa por enquanto (usa_proc_valor).
 *            0.1.1 - Linguagem v1.9 P3: so o comentario ("fica para depois da v1.9").
 *            0.1.0 - Linguagem v1.8b P5a: SemResolverTipoProc (E0907 fora do parametro de proc foreign) e SemProcComoCallback (E0907 de nome fora de callback, de proc foreign e de assinatura diferente).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 *
 * O tipo proc(...) guarda o NOME de cada parametro (TipoInfo.c da celula da cauda): a compatibilidade entre tipos proc os ignora (TipoProcCompativel) e os argumentos
 * nomeados da chamada indireta usam os nomes do tipo. O parametro de proc foreign (Semantica.tipo_proc_permitido) segue o caminho do callback de sempre;
 * qualquer outro uso marca usa_proc_valor (o lowering so chega no P21v-b).
 */
#include "semantic/sem_interno.h"

#include <stdio.h>

enum { PARAMS_MAX_CALLBACK = 16, MOTIVO_MAX = 256 };

void SemMarcarProcValor(Semantica *s, uint32_t pos)
{
    if (s->usa_proc_valor) return;
    s->usa_proc_valor = true;
    s->pos_proc_valor = pos;
}

/* E0909: a assinatura do tipo proc 'tipo' tem de atravessar a fronteira C. */
static bool ConferirAssinatura(Semantica *s, TipoId tipo, uint32_t pos)
{
    char motivo_buf[MOTIVO_MAX], texto[160];
    const char *motivo = SemMotivoDoProc(s, tipo, motivo_buf, sizeof motivo_buf);

    if (motivo == NULL) return true;
    SemDescrever(s, tipo, texto, sizeof texto);
    return SemErro(s, E0909, pos, "a assinatura '%s' nao atravessa a fronteira C: %s", texto, motivo);
}

/* Parametros e retorno do TipoRef TR_PROC; alvo[0] = retorno (valido se pos_tamanho == 1), alvo[1..n] = parametros, alvo[n + k] = nome do parametro k. */
static bool ResolverAssinatura(Semantica *s, TipoRef *t, TipoId *tipo)
{
    TipoId params[PARAMS_MAX_CALLBACK];
    int32_t nomes[PARAMS_MAX_CALLBACK];
    ProcAssinatura a = {params, nomes, (uint32_t)t->nome, TIPO_VAZIO};

    for (uint32_t k = 1; k <= a.n; k++) {
        if (!SemResolverTipoRef(s, &t->alvo[k])) return false;
        params[k - 1] = t->alvo[k].tipo;
        nomes[k - 1] = t->alvo[a.n + k].nome;
    }
    if (t->pos_tamanho == 1) {
        if (!SemResolverTipoRef(s, &t->alvo[0])) return false;
        a.retorno = t->alvo[0].tipo;
    }
    *tipo = TipoProc(&s->tipos, &a);
    return true;
}

bool SemResolverTipoProc(Semantica *s, TipoRef *t)
{
    bool callback_de_foreign = t == s->tipo_proc_permitido;
    TipoId tipo;

    if (s->prof_tipo > 0) {
        return SemErro(s, E0907, t->pos, "tipo proc(...) so vale inteiro (campo, let/var, parametro, retorno): ?proc, proc dentro de [N], [], [dynamic], map, union, soa, ^ e na assinatura de outro proc nao existem");
    }
    s->prof_tipo++;
    bool resolvido = ResolverAssinatura(s, t, &tipo);
    s->prof_tipo--;
    if (!resolvido) return false;
    t->tipo = tipo;
    if (callback_de_foreign) return true; /* a fronteira C do parametro e conferida com o proc foreign (E0904) */
    SemMarcarProcValor(s, t->pos);
    return s->adia_assinatura_proc || ConferirAssinatura(s, tipo, t->pos);
}

/* Campo proc de struct: o E0909 que CampoDaStruct adiou (as structs de dentro da assinatura ja tem campos). */
bool SemConferirProcsDaStruct(Semantica *s, Decl *d)
{
    for (uint32_t i = 0; i < d->ncampos; i++) {
        const TipoRef *tipo = &d->campos[i].tipo;
        if (tipo->tipo != TIPO_NAO_RESOLVIDO && TipoClasseDe(&s->tipos, tipo->tipo) == TC_PROC && !ConferirAssinatura(s, tipo->tipo, tipo->pos)) return false;
    }
    return true;
}

/* Tipo do proc 'd' como valor (com os nomes dos parametros); TIPO_NAO_RESOLVIDO se tem mais de 16 parametros. */
static TipoId TipoDoProcComoValor(Semantica *s, const Decl *d)
{
    TipoId params[PARAMS_MAX_CALLBACK];
    int32_t nomes[PARAMS_MAX_CALLBACK];
    ProcAssinatura a = {params, nomes, d->nparams, d->tem_retorno ? d->retorno.tipo : TIPO_VAZIO};

    if (d->nparams > PARAMS_MAX_CALLBACK) return TIPO_NAO_RESOLVIDO;
    for (uint32_t k = 0; k < d->nparams; k++) {
        params[k] = d->params[k].tipo.tipo;
        nomes[k] = d->params[k].nome;
    }
    return TipoProc(&s->tipos, &a);
}

static bool AssinaturaDiferente(Semantica *s, Expr *e, const Decl *d, TipoId ctx, TipoId tipo)
{
    char achado[160], pedido[160];

    if (tipo == TIPO_NAO_RESOLVIDO) snprintf(achado, sizeof achado, "proc(%u parametros)", (unsigned)d->nparams);
    else SemDescrever(s, tipo, achado, sizeof achado);
    SemDescrever(s, ctx, pedido, sizeof pedido);
    return SemErro(s, E0907, e->pos_op, "o proc '%s' tem assinatura '%s', mas o parametro pede '%s'", SemNomeTexto(s, e->nome), achado,
                   pedido);
}

/* Nome de proc da Forja usado como valor, em qualquer posicao: o tipo e o da assinatura dele (ou o do contexto, se for o mesmo, que e o do callback). */
bool SemProcComoCallback(Semantica *s, Expr *e, const Simbolo *y, TipoId ctx)
{
    bool de_foreign = s->valor_para_foreign, de_sort_by = s->comparador_sort_by;
    s->valor_para_foreign = false;
    s->comparador_sort_by = false;
    if (y->decl->externo) {
        return SemErro(s, E0907, e->pos_op, "'%s' e proc foreign: como valor (ou callback) precisa ser um proc da Forja (o C chama o trampolim dele); escreva um proc Forja com a assinatura pedida", SemNomeTexto(s, e->nome));
    }
    TipoId tipo = TipoDoProcComoValor(s, y->decl);
    bool ctx_proc = ctx != TIPO_NAO_RESOLVIDO && TipoClasseDe(&s->tipos, ctx) == TC_PROC;
    if (tipo == TIPO_NAO_RESOLVIDO || (ctx_proc && !TipoProcCompativel(&s->tipos, ctx, tipo))) return AssinaturaDiferente(s, e, y->decl, ctx, tipo);
    if (!de_foreign) {
        SemMarcarProcValor(s, e->pos_op);
        if (!de_sort_by && !ConferirAssinatura(s, tipo, e->pos_op)) return false; /* K08a: o comparador de sort_by nao cruza a fronteira C */
    }
    e->ref = REF_PROC;
    e->ref_id = y->indice;
    e->tipo = ctx_proc ? ctx : tipo;
    return true;
}

/* ---------- chamada indireta ---------- */

/* 'f(x)' onde f e local ou parametro de tipo proc: o no vira EX_CHAMADA_IND e o nome vira o chamado (a); false = e uma chamada de proc comum. */
bool SemConverterEmChamadaInd(Semantica *s, Expr *e)
{
    const LocalInfo *local = EscopoBuscar(&s->escopo, e->nome);
    Expr *chamado;

    if (local == NULL || TipoClasseDe(&s->tipos, local->tipo) != TC_PROC) return false;
    chamado = ArenaAlocar(s->arena, sizeof *chamado);
    *chamado = *e;
    chamado->tipo_no = EX_NOME;
    chamado->a = chamado->b = NULL;
    chamado->args = NULL;
    chamado->nargs = 0;
    chamado->flexivel = false;
    chamado->ref = REF_NENHUM;
    chamado->tipo = TIPO_NAO_RESOLVIDO;
    e->tipo_no = EX_CHAMADA_IND;
    e->a = chamado;
    return true;
}

/* Os "parametros" da chamada vem do tipo proc do chamado: nome e tipo de cada um, sem padrao. */
static bool ChamarComTipoProc(Semantica *s, Expr *e, TipoId proc)
{
    Param params[PARAMS_MAX_CALLBACK];
    uint32_t n = TipoProcQtd(&s->tipos, proc);

    for (uint32_t k = 0; k < n; k++) {
        params[k] = (Param){0};
        params[k].nome = TipoProcNome(&s->tipos, proc, k);
        params[k].tipo.tipo = TipoProcParam(&s->tipos, proc, k);
    }
    return SemChamarComParams(s, e, params, n);
}

bool SemChamadaInd(Semantica *s, Expr *e)
{
    char texto[160];

    if (!SemChecar(s, e->a, TIPO_VAZIO)) return false;
    TipoId proc = e->a->tipo;
    if (proc == TIPO_VENENO) {
        e->tipo = SemUsoDeVeneno(s);
        return true;
    }
    if (TipoClasseDe(&s->tipos, proc) != TC_PROC) {
        SemDescrever(s, proc, texto, sizeof texto);
        return SemErro(s, E0301, e->pos_op, "so se chama proc ou valor de tipo proc: este valor e %s", texto);
    }
    SemMarcarProcValor(s, e->pos_op);
    if (!ChamarComTipoProc(s, e, proc)) return false;
    e->tipo = (TipoId)TipoDe(&s->tipos, proc)->a;
    return true;
}

/* ---------- valor zero ---------- */

/* v1.12 (E0911): o tipo nao tem valor zero por causa de um proc: o proprio proc, [N] disso, ou struct por valor com campo sem padrao disso. */
bool SemProcSemZero(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe == TC_PROC) return true;
    if (k->classe == TC_ARRAY) return SemProcSemZero(s, k->a);
    if (k->classe != TC_STRUCT || k->b != 0) return false;
    const Decl *d = s->globais[k->a].decl;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (d->campos[i].padrao == NULL && SemProcSemZero(s, d->campos[i].tipo.tipo)) return true;
    }
    return false;
}
