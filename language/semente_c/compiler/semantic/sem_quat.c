/* sem_quat.c - o tipo embutido quat (v1.11 L14a, A05, E0333): quaternion de f32 com os campos x, y, z, w (16 bytes, a ordem do Quaternion da raylib).
 * No IR e uma struct OCULTA {x, y, z, w: f32} (Decl fora da lista do programa, registrada no 1o uso do nome quat: programa sem quat nao ganha struct nova).
 * O literal quat{...}, a leitura e a gravacao de campo e a travessia de foreign por valor saem do caminho das structs. Contas: q + r, q - r, -q, q * k, k * q
 * (k: f32, por componente na ordem x y z w) e conj(q). L14b: q * r (resultado quat), q * [3]f32 (resultado do tipo do vetor) e abs(q) (f32);
 * q / r, ==, ordem, v * q, quat de f64 e unidade nao existem.
 * Interface (sem_interno.h): SemQuatGarantir, SemContaQuat, SemComparacaoQuat, SemChamarConj, SemAbsDeQuat e SemEhQuat.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.11 L14b: q * r, q * [3]f32 e abs(q) deixam de ser E0333 (so a forma v * q e o v que nao e [3]f32 seguem erro).
 *            0.1.0 - Linguagem v1.11 L14a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include <string.h>

enum { QUAT_CAMPOS = 4, VETOR_GIRO_N = 3 };

/* Os campos x, y, z, w: f32 sem padrao (omitido no literal = 0.0). */
static Campo *CamposDoQuat(Semantica *s)
{
    const int32_t nomes[QUAT_CAMPOS] = {s->nome_x, s->nome_y, s->nome_z, s->nome_w};
    Campo *campos = ArenaAlocar(s->arena, QUAT_CAMPOS * sizeof *campos);
    memset(campos, 0, QUAT_CAMPOS * sizeof *campos);
    for (int k = 0; k < QUAT_CAMPOS; k++) {
        campos[k].nome = nomes[k];
        campos[k].tipo.forma = TR_NOME;
        campos[k].tipo.nome = SYM_F32;
        campos[k].tipo.erro = -1;
        campos[k].tipo.tipo = TIPO_F32;
    }
    return campos;
}

void SemQuatGarantir(Semantica *s)
{
    Simbolo *y = &s->globais[SYM_QUAT];
    if (s->tipo_quat != 0 || y->categoria != SIM_NENHUM) return;
    Decl *d = ArenaAlocar(s->arena, sizeof *d);
    memset(d, 0, sizeof *d);
    d->tipo_no = D_STRUCT;
    d->nome = SYM_QUAT;
    d->chave = d->nome;
    d->simbolo = -1;
    d->publico = true;
    d->campos = CamposDoQuat(s);
    d->ncampos = QUAT_CAMPOS;
    d->indice = (int32_t)s->qtd_struct++;
    s->decl_quat = d;
    s->tipo_quat = TipoStruct(&s->tipos, SYM_QUAT);
    *y = (Simbolo){SIM_STRUCT, d->indice, d, s->tipo_quat, 0};
}

static bool ErroQuat(Semantica *s, uint32_t pos, const char *causa)
{
    return SemErro(s, E0333, pos, "%s", causa);
}

/* O vetor que o quat gira: [3]f32 sem unidade. */
static bool EhVetor3F32(const Semantica *s, TipoId tipo)
{
    const TipoInfo *k = TipoDe(&s->tipos, tipo);
    return k->classe == TC_ARRAY && k->a == TIPO_F32 && k->b == VETOR_GIRO_N;
}

/* q * v gira o vetor (resultado do tipo de v); v * q nao existe (a ordem e q * v). */
static bool ProdutoComVetor(Semantica *s, Expr *e, bool a_quat)
{
    char texto[96];
    if (!a_quat) return ErroQuat(s, e->pos_op, "v * q nao existe: o quat gira o vetor, escreva q * v");
    if (EhVetor3F32(s, e->b->tipo)) {
        e->tipo = e->b->tipo;
        return true;
    }
    SemDescrever(s, e->b->tipo, texto, sizeof texto);
    return SemErro(s, E0333, e->pos_op, "q * v pede v: [3]f32 sem unidade, nao %s", texto);
}

/* q * r (Hamilton, resultado quat), q * k e k * q (k: f32 sem unidade) e q * v (v: [3]f32). */
static bool ProdutoQuat(Semantica *s, Expr *e, bool a_quat, bool b_quat)
{
    char texto[96];
    if (a_quat && b_quat) {
        e->tipo = s->tipo_quat;
        return true;
    }
    const Expr *outro = a_quat ? e->b : e->a;
    if (outro->tipo == TIPO_F32) {
        e->tipo = s->tipo_quat;
        return true;
    }
    if (TipoClasseDe(&s->tipos, outro->tipo) == TC_ARRAY) return ProdutoComVetor(s, e, a_quat);
    SemDescrever(s, outro->tipo, texto, sizeof texto);
    return SemErro(s, E0333, e->pos_op, "q * k pede k: f32 sem unidade, nao %s", texto);
}

bool SemContaQuat(Semantica *s, Expr *e)
{
    bool a_quat = SemEhQuat(s, e->a->tipo), b_quat = SemEhQuat(s, e->b->tipo);
    if (e->op == OP_SOMA || e->op == OP_SUB) {
        if (!a_quat || !b_quat) return ErroQuat(s, e->pos_op, "q + r e q - r pedem dois quat (quat com escalar so existe em *)");
        e->tipo = s->tipo_quat;
        return true;
    }
    if (e->op == OP_MUL) return ProdutoQuat(s, e, a_quat, b_quat);
    return ErroQuat(s, e->pos_op, "quat nao tem '/' nem '%': so + - * (q * k e k * q com k: f32)");
}

bool SemComparacaoQuat(Semantica *s, Expr *e)
{
    return ErroQuat(s, e->pos_op, "quat nao tem ==, != nem ordem: compare os campos x, y, z e w um a um");
}

/* conj(q) = {-x, -y, -z, w}. */
bool SemChamarConj(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, s->params_um, 1) || !SemChecar(s, e->args[0].valor, TIPO_VAZIO)) return false;
    const Expr *arg = e->args[0].valor;
    if (!SemEhQuat(s, arg->tipo)) {
        SemDescrever(s, arg->tipo, texto, sizeof texto);
        return SemErro(s, E0333, arg->pos, "conj pede quat, nao %s", texto);
    }
    e->tipo = s->tipo_quat;
    return true;
}

/* abs(q) = a norma sqrt(x*x + y*y + z*z + w*w): f32. */
bool SemAbsDeQuat(Semantica *s, Expr *e)
{
    (void)s;
    e->tipo = TIPO_F32;
    return true;
}
