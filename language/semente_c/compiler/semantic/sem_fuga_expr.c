/* sem_fuga_expr.c - origem do valor de uma expressao e do endereco de um lvalor (fuga intraprocedural, K1).
 * Versao: 0.2.3
 * Changelog: 0.2.3 - IA1-L9f: to_str (str dona, bloco do heap) entra na origem heap.
 *            0.2.2- IA1-L9e: keys, values e from_bytes (memoria nova) entram na origem heap; as_bytes herda a origem do str (como slice_from do ponteiro).
 *            0.2.1 - IA1-L9d: clone (lista nova no alocador do argumento) entra com new/make/format na origem heap.
 *            0.2.0 - Linguagem v1.11 L3: [dynamic], map e soa entram no tipo "com ponteiro"; ler elemento de recipiente por valor herda o local;
 *                    coluna de soa aponta para o proprio soa (local) ou para o heap; chamada de proc da Forja usa o resumo (FugaResultadoDaChamada).
 *            0.1.0 - Linguagem v1.11 L2: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_fuga.h"

/* ---------- tipo "com ponteiro" ---------- */

/* Structs e unions em visita: [dynamic]S dentro de S (arvore) fecha o ciclo; o que ja esta na pilha nao traz ponteiro novo. */
enum { VISITA_MAX = 16 };
typedef struct { TipoId pilha[VISITA_MAX]; uint32_t n; } Visita;

static bool TemPonteiro(const Semantica *s, TipoId t, Visita *v);

static bool CamposTemPonteiro(const Semantica *s, const Decl *d, Visita *v)
{
    if (d == NULL) return false;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (TemPonteiro(s, d->campos[i].tipo.tipo, v)) return true;
    }
    return false;
}

static bool TuplaTemPonteiro(const Semantica *s, TipoId t, Visita *v)
{
    uint32_t qtd = TipoTuplaQtd(&s->tipos, t);
    for (uint32_t k = 0; k < qtd; k++) {
        if (TemPonteiro(s, TipoTuplaElemento(&s->tipos, t, k), v)) return true;
    }
    return false;
}

static bool EstruturaTemPonteiro(const Semantica *s, TipoId t, Visita *v)
{
    for (uint32_t i = 0; i < v->n; i++) {
        if (v->pilha[i] == t) return false;
    }
    if (v->n == VISITA_MAX) return false;
    v->pilha[v->n++] = t;
    bool tem = CamposTemPonteiro(s, s->globais[TipoDe(&s->tipos, t)->a].decl, v);
    v->n--;
    return tem;
}

static bool TemPonteiro(const Semantica *s, TipoId t, Visita *v)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    switch (k->classe) {
    case TC_PONTEIRO: case TC_ANULAVEL: case TC_RAWPTR: case TC_FATIA: return true;
    case TC_OPCIONAL: case TC_ARRAY: case TC_RESULTADO: case TC_DINAMICO: case TC_SOA: return TemPonteiro(s, k->a, v);
    case TC_MAPA: return TemPonteiro(s, k->a, v) || TemPonteiro(s, k->b, v);
    case TC_STRUCT: case TC_UNIAO: return EstruturaTemPonteiro(s, t, v);
    case TC_TUPLA: return TuplaTemPonteiro(s, t, v);
    default: return false;
    }
}

/* ^T, ?^T, []T, rawptr e o que os contem por valor (struct, union, array, tupla, ?T, resultado, [dynamic], map, soa). str e cstr nao entram. */
bool SemFugaTipoTemPonteiro(const Semantica *s, TipoId t)
{
    Visita v;
    v.n = 0;
    return TemPonteiro(s, t, &v);
}

/* ---------- endereco de um lvalor ---------- */

static bool BaseEhPonteiro(const Fuga *f, const Expr *x)
{
    return TipoClasseDe(&f->s->tipos, x->a->tipo) == TC_PONTEIRO;
}

static void EnderecoDoNome(Fuga *f, const Expr *x, FugaConj dst)
{
    bool local = (x->ref == REF_LOCAL || x->ref == REF_PARAM) && (uint32_t)x->ref_id < f->nl;
    FugaLigar(dst, local ? (uint32_t)x->ref_id : FugaBitFlag(f, FUGA_DESC));
}

/* '&x.c': campo por valor continua na base; campo de ^struct herda a origem do ponteiro. */
static const Expr *PassoCampo(Fuga *f, const Expr *x, FugaConj dst)
{
    if (!BaseEhPonteiro(f, x)) return x->a;
    FugaAvaliar(f, x->a, dst);
    return NULL;
}

/* '&x[i]': array por valor continua na base; fatia e ^[N]T herdam do ponteiro; lista e o resto cai no heap ou no desconhecido. */
static const Expr *PassoIndice(Fuga *f, const Expr *x, FugaConj dst)
{
    ColecaoForma forma = FORMA_NENHUMA;
    SemElementoDe(f->s, x->a->tipo, &forma);
    if (forma == FORMA_ARRAY) return x->a;
    if (forma == FORMA_FATIA || forma == FORMA_PONTEIRO_ARRAY) FugaAvaliar(f, x->a, dst);
    else FugaLigarFlag(f, dst, forma == FORMA_DINAMICO || forma == FORMA_PONTEIRO_DINAMICO ? FUGA_HEAP : FUGA_DESC);
    return NULL;
}

/* Um passo da cadeia 'nome ('.campo' | '[i]' | '^')*': devolve o no seguinte ou NULL quando a cadeia terminou. */
static const Expr *PassoEndereco(Fuga *f, const Expr *x, FugaConj dst)
{
    if (x->tipo_no == EX_NOME) {
        EnderecoDoNome(f, x, dst);
        return NULL;
    }
    if (x->tipo_no == EX_CAMPO) return PassoCampo(f, x, dst);
    if (x->tipo_no == EX_INDICE) return PassoIndice(f, x, dst);
    if (x->tipo_no == EX_UNARIO && x->op == OP_DEREF) FugaAvaliar(f, x->a, dst);
    else FugaLigarFlag(f, dst, FUGA_DESC);
    return NULL;
}

void FugaEndereco(Fuga *f, const Expr *x, FugaConj dst)
{
    for (const Expr *p = x; p != NULL;) p = PassoEndereco(f, p, dst);
}

/* ---------- valor de uma expressao ---------- */

static void UnirConj(const Fuga *f, FugaConj dst, const FugaConj src)
{
    for (uint32_t w = 0; w < f->pal; w++) dst[w] |= src[w];
}

static void AvaliarNome(Fuga *f, const Expr *e, FugaConj dst)
{
    if ((e->ref == REF_LOCAL || e->ref == REF_PARAM) && (uint32_t)e->ref_id < f->nl) UnirConj(f, dst, FugaConjDe(f, (uint32_t)e->ref_id));
    else if (e->ref == REF_CONST) FugaLigarFlag(f, dst, FUGA_GLOBAL);
}

static void AvaliarArgs(Fuga *f, const Expr *e, FugaConj dst)
{
    for (uint32_t i = 0; i < e->nargs; i++) FugaAvaliar(f, e->args[i].valor, dst);
}

/* 'a[i]': elemento de recipiente por valor (array, lista, map, soa) herda do local; de fatia ou ponteiro e uma leitura de memoria alheia. */
static void AvaliarIndice(Fuga *f, const Expr *e, FugaConj dst)
{
    if (FugaContemPorValor(f, e->a->tipo)) FugaAvaliar(f, e->a, dst);
    else FugaLigarFlag(f, dst, FUGA_DESC);
}

/* 'a.c': campo de ^struct e leitura alheia; coluna de soa e uma fatia que aponta para o proprio soa (array: local; lista: heap; fatia: herda);
 * o resto herda da base. */
static void AvaliarCampo(Fuga *f, const Expr *e, FugaConj dst)
{
    const Tipos *tipos = &f->s->tipos;
    TipoClasse base = TipoClasseDe(tipos, e->a->tipo);
    TipoClasse corpo = base == TC_SOA ? TipoClasseDe(tipos, TipoDe(tipos, e->a->tipo)->a) : TC_VAZIO;
    if (base == TC_PONTEIRO) FugaLigarFlag(f, dst, FUGA_DESC);
    else if (corpo == TC_DINAMICO) FugaLigarFlag(f, dst, FUGA_HEAP);
    else if (corpo == TC_ARRAY) FugaEndereco(f, e->a, dst);
    else FugaAvaliar(f, e->a, dst);
}

/* 'a[x..y]': fatia de array por valor aponta para ele; fatia de fatia e de ^[N]T herdam; lista aponta para o heap. */
static void AvaliarFatia(Fuga *f, const Expr *e, FugaConj dst)
{
    ColecaoForma forma = FORMA_NENHUMA;
    SemElementoDe(f->s, e->a->tipo, &forma);
    if (forma == FORMA_ARRAY) FugaEndereco(f, e->a, dst);
    else if (forma == FORMA_FATIA || forma == FORMA_PONTEIRO_ARRAY) FugaAvaliar(f, e->a, dst);
    else FugaLigarFlag(f, dst, forma == FORMA_DINAMICO || forma == FORMA_PONTEIRO_DINAMICO ? FUGA_HEAP : FUGA_DESC);
}

static void AvaliarUnario(Fuga *f, const Expr *e, FugaConj dst)
{
    if (e->op == OP_ENDERECO) FugaEndereco(f, e->a, dst);
    else if (e->op == OP_DEREF) FugaLigarFlag(f, dst, FUGA_DESC);
}

/* slice_from(p, len = n): a fatia herda a origem de p (o parametro de indice 0, em qualquer ordem de escrita). */
static void AvaliarSliceFrom(Fuga *f, const Expr *e, FugaConj dst)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (e->args[i].indice == 0) FugaAvaliar(f, e->args[i].valor, dst);
    }
}

static void AvaliarEmbutido(Fuga *f, const Expr *e, FugaConj dst)
{
    switch (e->ref_id) {
    case EMB_NEW: case EMB_MAKE: case EMB_FORMAT: case EMB_FORMAT_C: case EMB_GET_PTR: case EMB_CLONE: FugaLigarFlag(f, dst, FUGA_HEAP); break;
    case EMB_KEYS: case EMB_VALUES: case EMB_FROM_BYTES: case EMB_TO_STR: FugaLigarFlag(f, dst, FUGA_HEAP); break;
    case EMB_SLICE_FROM: case EMB_AS_BYTES: AvaliarSliceFrom(f, e, dst); break;
    default: FugaLigarFlag(f, dst, FUGA_DESC); break;
    }
}

/* Chamada: new/make/format = heap; foreign = FOREIGN; proc da Forja = o que o resumo diz chegar ao retorno (sem_fuga_resumo.c). */
static void AvaliarChamada(Fuga *f, const Expr *e, FugaConj dst)
{
    if (e->ref == REF_EMBUTIDO) AvaliarEmbutido(f, e, dst);
    else FugaResultadoDaChamada(f, e, dst);
}

static void AvaliarComposto(Fuga *f, const Expr *e, FugaConj dst)
{
    switch (e->tipo_no) {
    case EX_BINARIO: case EX_OR_ELSE: FugaAvaliar(f, e->a, dst); FugaAvaliar(f, e->b, dst); break;
    case EX_CONVERSAO: case EX_TRY: case EX_EMBRULHA: FugaAvaliar(f, e->a, dst); break;
    case EX_LITERAL_STRUCT: case EX_LITERAL_ARRAY: case EX_TUPLA: AvaliarArgs(f, e, dst); break;
    default: break;
    }
}

void FugaAvaliar(Fuga *f, const Expr *e, FugaConj dst)
{
    if (!FugaRastreia(f, e->tipo)) return;
    switch (e->tipo_no) {
    case EX_NOME: AvaliarNome(f, e, dst); break;
    case EX_CAMPO: AvaliarCampo(f, e, dst); break;
    case EX_INDICE: AvaliarIndice(f, e, dst); break;
    case EX_FATIA: AvaliarFatia(f, e, dst); break;
    case EX_UNARIO: AvaliarUnario(f, e, dst); break;
    case EX_CHAMADA: AvaliarChamada(f, e, dst); break;
    default: AvaliarComposto(f, e, dst); break;
    }
}
