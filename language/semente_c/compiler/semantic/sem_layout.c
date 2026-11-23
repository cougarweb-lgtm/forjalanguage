/* sem_layout.c - size_of(T), align_of(T), offset_of(Struct.campo) e static_assert (Linguagem v1.11 L9, O10).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.12 M3 P21v-a: tipo proc tem tamanho (8 bytes, layout.c): sai da tabela SEM_LAYOUT; CONSTANTES[EX_CHAMADA_IND + 1].
 *            0.1.0 - Linguagem v1.11 L9: primeira versao. A semantica tipa os tres primeiros como int e confere o tipo (E0328: opaco, proc, tupla,
 *                    '! E' sem valor); o VALOR sai no lowering (lower_layout.c), que tem a IR das structs. static_assert confere que a condicao
 *                    e constante (E0329) e liga Semantica.usa_layout.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

typedef bool (*ConstanteNo)(const Expr *e);

/* Classes de tipo sem layout (por tabela); o opaco e o '! E' sem valor dependem do tipo e saem em MotivoSemLayout. */
static const char *const SEM_LAYOUT[TC_QTD] = {
    [TC_VAZIO] = "um tipo sem valor nao tem tamanho",
    [TC_TUPLA] = "retorno multiplo nao e um tipo com tamanho",
};

/* Por que o tipo nao tem layout; NULL = tem. */
static const char *MotivoSemLayout(Semantica *s, TipoId t)
{
    TipoClasse classe = TipoClasseDe(&s->tipos, t);
    if (SEM_LAYOUT[classe] != NULL) return SEM_LAYOUT[classe];
    if (classe == TC_STRUCT && SemTipoEhOpaco(s, t)) return "tipo opaco: so a biblioteca C conhece o tamanho";
    if (classe == TC_RESULTADO && TipoClasseDe(&s->tipos, TipoDe(&s->tipos, t)->a) == TC_VAZIO) {
        return "'! E' sem tipo de valor nao tem tamanho";
    }
    return NULL;
}

/* O tipo escrito de size_of/align_of. O opaco por nome simples passa pelo resolvedor (que o recusaria com E0908): o E0328 o recusa depois. */
static bool ResolverTipoDoLayout(Semantica *s, TipoRef *ref)
{
    if (s->tem_opaco) s->sob_ponteiro = ref->forma == TR_NOME;
    bool ok = SemResolverTipoRef(s, ref);
    s->sob_ponteiro = false;
    return ok;
}

/* size_of(T) / align_of(T): 1 argumento, um tipo. */
static bool ChamarTamanhoOuAlinhamento(Semantica *s, Expr *e)
{
    char texto[96];
    if (e->nargs == 0) return SemErro(s, E0503, e->pos_op, "%s recebe um tipo: %s(Vertice)", SemNomeTexto(s, e->nome), SemNomeTexto(s, e->nome));
    if (e->nargs > 1) return SemErro(s, E0504, e->args[1].valor->pos, "%s recebe so o tipo", SemNomeTexto(s, e->nome));
    Expr *no = e->args[0].valor;
    if (!ResolverTipoDoLayout(s, no->alvo)) return false;
    no->tipo = no->alvo->tipo;
    const char *motivo = MotivoSemLayout(s, no->tipo);
    if (motivo != NULL) {
        SemDescrever(s, no->tipo, texto, sizeof texto);
        return SemErro(s, E0328, no->pos, "%s(%s): %s", SemNomeTexto(s, e->nome), texto, motivo);
    }
    e->args[0].indice = 0;
    e->tipo = TIPO_INT;
    s->usa_layout = true;
    return true;
}

/* Struct de 'Struct.campo' do offset_of: precisa ser struct declarada, visivel e nao opaca. */
static const Simbolo *StructDoOffset(Semantica *s, const Expr *base)
{
    const Simbolo *y = SemAcharSimbolo(s, base->nome, base->modulo);
    NomeUsado uso = {base->nome, base->modulo != 0 ? base->pos_op : base->pos, base->modulo};
    if (!SemVisivel(s, y, &uso)) return NULL;
    if (y->categoria != SIM_STRUCT) {
        SemErro(s, E0328, base->pos, "offset_of(Struct.campo): '%s' nao e uma struct", SemNomeTexto(s, base->nome));
        return NULL;
    }
    if (!SemSimboloVeneno(y) && y->decl->externo) {
        SemErro(s, E0328, base->pos, "offset_of(%s.campo): tipo opaco: so a biblioteca C conhece o layout", SemNomeTexto(s, base->nome));
        return NULL;
    }
    return y;
}

/* Campo direto da struct: indice do campo, ou erro (E0201 com os parecidos). */
static bool CampoDoOffset(Semantica *s, const Simbolo *y, const Expr *campo, int32_t *indice)
{
    *indice = SemAcharCampo(y->decl, campo->nome);
    if (*indice >= 0) return true;
    SemParecido sp;
    SemParecidoIniciar(s, E0201, campo->pos_op, campo->nome, &sp);
    SemParecidosDeCampos(s, y->decl, &sp);
    return SemErroParecido(s, &sp, "a struct '%s' nao tem o campo '%s'", SemNomeTexto(s, y->decl->nome), SemNomeTexto(s, campo->nome));
}

/* offset_of(Struct.campo): campo direto; aninhado se soma na mao (offset_of(A.b) + offset_of(B.c)). */
static bool ChamarOffsetOf(Semantica *s, Expr *e)
{
    if (e->nargs == 0) return SemErro(s, E0503, e->pos_op, "offset_of recebe Struct.campo: offset_of(Vertice.uv)");
    if (e->nargs > 1) return SemErro(s, E0504, e->args[1].valor->pos, "offset_of recebe so Struct.campo");
    Expr *campo = e->args[0].valor;
    if (campo->tipo_no != EX_CAMPO || campo->a->tipo_no != EX_NOME) {
        return SemErro(s, E0328, campo->pos, "offset_of recebe Struct.campo, com um campo direto da struct (aninhado: some os offsets na mao)");
    }
    const Simbolo *y = StructDoOffset(s, campo->a);
    if (y == NULL) return false;
    if (SemSimboloVeneno(y)) {
        e->tipo = SemUsoDeVeneno(s);
        return true;
    }
    int32_t indice;
    if (!CampoDoOffset(s, y, campo, &indice)) return false;
    campo->a->ref = REF_STRUCT;
    campo->a->ref_id = y->indice;
    campo->a->tipo = y->tipo;
    campo->ref = REF_CAMPO;
    campo->ref_id = indice;
    campo->tipo = TIPO_INT;
    e->args[0].indice = 0;
    e->tipo = TIPO_INT;
    s->usa_layout = true;
    return true;
}

bool SemChamarLayout(Semantica *s, Expr *e, int32_t embutido)
{
    if (embutido == EMB_OFFSET_OF) return ChamarOffsetOf(s, e);
    return ChamarTamanhoOuAlinhamento(s, e);
}

/* ---------- static_assert: a condicao so pode ter o que o lowering sabe dobrar ---------- */

static bool ConstanteLiteral(const Expr *e)
{
    (void)e;
    return true;
}

static bool ConstanteConst(const Expr *e)
{
    return e->ref == REF_CONST;
}

static bool ConstanteMembro(const Expr *e)
{
    return e->ref == REF_MEMBRO;
}

static bool ConstanteLayout(const Expr *e)
{
    return e->ref == REF_EMBUTIDO && e->ref_id >= EMB_SIZE_OF && e->ref_id <= EMB_OFFSET_OF;
}

static bool EhConstanteDeLayout(const Expr *e);

static bool ConstanteUnario(const Expr *e)
{
    return (e->op == OP_NEGAR || e->op == OP_NAO || e->op == OP_BIT_NAO) && EhConstanteDeLayout(e->a);
}

static bool ConstanteBinario(const Expr *e)
{
    return EhConstanteDeLayout(e->a) && EhConstanteDeLayout(e->b);
}

static bool ConstanteConversao(const Expr *e)
{
    return EhConstanteDeLayout(e->a);
}

static const ConstanteNo CONSTANTES[EX_CHAMADA_IND + 1] = {
    [EX_INT] = ConstanteLiteral, [EX_REAL] = ConstanteLiteral, [EX_BOOL] = ConstanteLiteral, [EX_NOME] = ConstanteConst,
    [EX_CAMPO] = ConstanteMembro, [EX_CHAMADA] = ConstanteLayout, [EX_UNARIO] = ConstanteUnario, [EX_BINARIO] = ConstanteBinario,
    [EX_CONVERSAO] = ConstanteConversao,
};

/* Literal, const, membro de enum, size_of/align_of/offset_of e contas entre eles. */
static bool EhConstanteDeLayout(const Expr *e)
{
    ConstanteNo confere = CONSTANTES[e->tipo_no];
    return confere != NULL && confere(e);
}

/* static_assert(cond, msg = "..."): chamado depois do ligador de argumentos do assert (cond e o args[0]); a condicao tem de ser constante (E0329). */
bool SemConferirStaticAssert(Semantica *s, const Expr *e)
{
    const Expr *cond = e->args[0].valor;
    s->usa_layout = true;
    if (EhConstanteDeLayout(cond)) return true;
    return SemErro(s, E0329, cond->pos, "a condicao do static_assert nao e constante: use literal, const, size_of, align_of, offset_of e contas entre eles");
}

/* static_assert no topo do arquivo (D_ASSERT): a chamada sozinha, conferida como o comando de proc. */
bool SemDeclAssert(Semantica *s, Decl *d)
{
    Expr *chamada = d->init;
    if (chamada->tipo_no != EX_CHAMADA || chamada->nome != SYM_STATIC_ASSERT) {
        return SemErro(s, E0329, chamada->pos, "no topo do arquivo static_assert e a chamada sozinha: static_assert(cond, msg = \"...\")");
    }
    return SemExpr(s, chamada, TIPO_VAZIO);
}

/* Texto do init de uma const: size_of/align_of/offset_of dentro dele (a const nao vale como N de [N]T). */
bool SemUsaLayoutNoTexto(const Expr *e)
{
    if (e == NULL) return false;
    if (e->tipo_no == EX_CHAMADA && (e->nome == SYM_SIZE_OF || e->nome == SYM_ALIGN_OF || e->nome == SYM_OFFSET_OF)) return true;
    if (SemUsaLayoutNoTexto(e->a) || SemUsaLayoutNoTexto(e->b)) return true;
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (SemUsaLayoutNoTexto(e->args[i].valor)) return true;
    }
    return false;
}
