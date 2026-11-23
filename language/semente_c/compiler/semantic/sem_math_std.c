/* sem_math_std.c - gancho de std:math (S3 P3, L11b): tipo dos argumentos de math.<fn> e a troca pela gemea <fn>_f32.
 * O tipo que manda e o do 1o argumento que nao e literal; literal sozinho vale f64 (D2; o sqrt embutido faz o contrario). Todo argumento que nao e
 * literal tem de ser esse tipo: f32 e f64 misturados, inteiro, unidade, bool ou texto = E0334. Literal de inteiro ou texto tambem = E0334.
 * Com f32 a chamada vai para a gemea <fn>_f32 do mesmo modulo: ela e nao-pub, a busca interna acha mesmo assim (SemAcharSimboloEm).
 * Sem gemea, a chamada segue a decl original e o erro de tipo sai do ConferirTipos.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - S3 P4 (L11b-2, D10): SemLibmNotarDecl e SemLibmNotarChamada, so com o forja migrar ligado (Diag.trocas): foreign de libm que bate com std:math
 *            (aridade, nomes, tipos f32/f64, linha so 'proc') sai e as chamadas viram math.<fn> (sqrt fica embutido); o import "std:math" entra uma vez, so com uma chamada math.*.
 *            0.1.0 - S3 P3 (L11b-1): primeira versao (SemMathChamada, chamado por SemChamada em sem_call.c).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "common/renomeados.h"
#include "semantic/sem_interno.h"

#include <string.h>

#define MATH_SUFIXO_F32 "_f32"
#define MATH_NOME_MAX 96

/* Decl de um proc do modulo std:math (origem MOD_STD, caminho "std:math"). */
static bool EhModuloMath(const Semantica *s, const Decl *d)
{
    const Modulo *m;
    const char *caminho = "std:math";
    size_t tam = strlen(caminho);
    if (d->modulo == 0 || s->modulos == NULL) return false;
    m = &s->modulos->itens[d->modulo];
    return m->origem == MOD_STD && InternerTam(s->nomes, m->caminho) == tam && memcmp(InternerTexto(s->nomes, m->caminho), caminho, tam) == 0;
}

/* Simbolo da gemea <nome>_f32 no mesmo modulo, qualquer que seja a visibilidade; NULL se o modulo nao a declara. */
static const Simbolo *GemeaF32(const Semantica *s, const Decl *d)
{
    char nome[MATH_NOME_MAX];
    uint32_t tam = InternerTam(s->nomes, d->nome);
    size_t sufixo = sizeof MATH_SUFIXO_F32 - 1;
    int32_t id;
    const Simbolo *y;
    if (tam + sufixo >= sizeof nome) return NULL;
    memcpy(nome, InternerTexto(s->nomes, d->nome), tam);
    memcpy(nome + tam, MATH_SUFIXO_F32, sufixo);
    id = InternerAchar(s->nomes, nome, tam + (uint32_t)sufixo);
    if (id < 0) return NULL;
    y = SemAcharSimboloEm(s, id, d->modulo, s->modulo_atual);
    return y->decl != NULL && y->decl->modulo == d->modulo && y->decl->tipo_no == D_PROC ? y : NULL;
}

/* Literal sozinho adota f64 (ou o f32 do contexto, no caso do gancho); aqui so importa que seja real. */
static bool ArgumentoEhRealLiteral(const Semantica *s, const Expr *v)
{
    return v->flexivel && TipoEhReal(&s->tipos, v->tipo);
}

/* Tipa cada argumento sem contexto e confere: o tipo que manda sai em *base (f64 se so ha literais). */
static bool TiparArgumentosMath(Semantica *s, Expr *e, TipoId *base)
{
    bool achou = false;
    char a[96], b[96];
    *base = TIPO_F64;
    for (uint32_t i = 0; i < e->nargs; i++) {
        Expr *v = e->args[i].valor;
        if (!SemChecar(s, v, TIPO_VAZIO)) return false;
        if (ArgumentoEhRealLiteral(s, v)) continue;
        if (v->flexivel) {
            SemDescrever(s, v->tipo, a, sizeof a);
            return SemErro(s, E0334, v->pos, "%s pede f32 ou f64, nao %s (literal de inteiro ou texto nao entra)", SemNomeTexto(s, e->nome), a);
        }
        if (!achou) {
            *base = v->tipo;
            achou = true;
        } else if (v->tipo != *base) {
            SemDescrever(s, *base, a, sizeof a);
            SemDescrever(s, v->tipo, b, sizeof b);
            return SemErro(s, E0334, v->pos, "%s pede o mesmo tipo nos argumentos: %s e %s", SemNomeTexto(s, e->nome), a, b);
        }
    }
    if (*base == TIPO_F32 || *base == TIPO_F64) return true;
    SemDescrever(s, *base, a, sizeof a);
    return SemErro(s, E0334, e->pos_op, "%s pede f32 ou f64 sem unidade, nao %s", SemNomeTexto(s, e->nome), a);
}

/* Chamada de std:math: confere o tipo dos argumentos e, com f32, troca o alvo (*y) pela gemea. Falso = erro ja relatado. */
bool SemMathChamada(Semantica *s, Expr *e, const Simbolo **y)
{
    const Decl *d = (*y)->decl;
    TipoId base;
    const Simbolo *gemea;
    if (d == NULL || d->tipo_no != D_PROC || !EhModuloMath(s, d)) return true;
    if (!TiparArgumentosMath(s, e, &base)) return false;
    if (base != TIPO_F32) return true;
    gemea = GemeaF32(s, d);
    if (gemea != NULL) *y = gemea;
    return true;
}

/* ---------- migracao libm (S3 P4, D10): so o forja migrar liga Diag.trocas; no check/build estas funcoes saem logo ---------- */

/* Funcao de libm de dois argumentos: o simbolo C (sem o f de f32) e os nomes dos parametros em std\math.fj; as outras tem um so, 'x'. */
typedef struct { const char *base, *p0, *p1; } LibmBin;
static const LibmBin LIBM_BIN[] = {{"pow", "x", "y"}, {"atan2", "y", "x"}, {"hypot", "x", "y"}, {"fmod", "x", "y"}, {"copysign", "x", "y"}};

/* A entrada de LIBM_BIN do simbolo C (base ou base + 'f'); NULL = funcao de um argumento. */
static const LibmBin *LibmBinariaDe(const char *csym)
{
    for (size_t k = 0; k < sizeof LIBM_BIN / sizeof LIBM_BIN[0]; k++) {
        size_t n = strlen(LIBM_BIN[k].base);
        if (strncmp(csym, LIBM_BIN[k].base, n) == 0 && (csym[n] == '\0' || (csym[n] == 'f' && csym[n + 1] == '\0'))) return &LIBM_BIN[k];
    }
    return NULL;
}

/* Os nomes dos parametros do foreign batem com os de std:math (a ordem tambem): so assim o rotulo da chamada continua valendo. */
static bool NomesDoLibm(const Semantica *s, const Decl *d, const LibmBin *bin)
{
    const char *p0 = bin != NULL ? bin->p0 : "x";

    if (strcmp(SemNomeTexto(s, d->params[0].nome), p0) != 0) return false;
    return bin == NULL || strcmp(SemNomeTexto(s, d->params[1].nome), bin->p1) == 0;
}

/* Parametros e retorno do tipo da funcao: f32 para o simbolo com sufixo f, f64 para os demais. */
static bool TiposDoLibm(const Decl *d, TipoId tipo)
{
    if (!d->tem_retorno || d->retorno.tipo != tipo) return false;
    for (uint32_t i = 0; i < d->nparams; i++) {
        if (d->params[i].tipo.tipo != tipo) return false;
    }
    return true;
}

/* A linha inteira do foreign (com o LF): so vale se e um 'proc' solto, sem chave e sem outro item. false = nao migra. */
static bool LinhaDoDecl(const Semantica *s, const Decl *d, uint32_t *ini, uint32_t *tam)
{
    const char *t = s->fonte->texto;
    uint32_t fim = (uint32_t)s->fonte->tam, a = d->pos, b = d->pos, ini_linha;

    while (a > 0 && t[a - 1] != '\n') a--;
    while (b < fim && t[b] != '\n') b++;
    ini_linha = a;
    for (uint32_t k = a; k < b; k++) {
        if (t[k] == '{' || t[k] == '}') return false;
    }
    while (a < b && (t[a] == ' ' || t[a] == '\t')) a++;
    if (strncmp(t + a, "proc ", 5) != 0) return false;
    if (b < fim) b++;
    *ini = ini_linha;
    *tam = b - ini_linha;
    return true;
}

/* A entrada da tabela (REN_LIBM) se o foreign 'd' migra para std:math; NULL = nao migra, e *motivo diz por que quando 'd' e de libm (NULL = nao e libm). */
static const Renomeado *LibmMigravel(const Semantica *s, const Decl *d, const char **motivo)
{
    const char *csym = SemNomeTexto(s, d->simbolo >= 0 ? d->simbolo : d->nome);
    const Renomeado *r = RenomeadoBuscar(REN_LIBM, csym);
    const LibmBin *bin = LibmBinariaDe(csym);
    TipoId tipo = csym[strlen(csym) - 1] == 'f' ? TIPO_F32 : TIPO_F64;
    uint32_t ini, tam;

    *motivo = NULL;
    if (r == NULL) return NULL;
    if (d->nparams != (bin != NULL ? 2u : 1u)) *motivo = "a aridade difere de std:math";
    else if (!NomesDoLibm(s, d, bin)) *motivo = "os nomes dos parametros diferem de std:math";
    else if (!TiposDoLibm(d, tipo)) *motivo = "o tipo nao bate: f32 com sufixo f, f64 sem";
    else if (!LinhaDoDecl(s, d, &ini, &tam)) *motivo = "a linha do foreign tem chave ou mais de um item";
    return *motivo == NULL ? r : NULL;
}

/* O import "std:math" ja esta no arquivo raiz (a Fonte tem o modulo importado depois dele)? */
static bool ImportaStdMath(const Semantica *s)
{
    const char *t = s->fonte->texto, *alvo = "import \"std:math\"";
    size_t fim = s->fonte->ntrechos > 0 ? s->fonte->trechos[0].fim : s->fonte->tam, n = strlen(alvo);

    for (size_t k = 0; k + n <= fim; k++) {
        if (memcmp(t + k, alvo, n) == 0) return true;
    }
    return false;
}

/* O import entra uma vez, na linha 1, e so quando uma chamada math.* vai ser migrada (sem uso o E1005 derruba o conjunto). */
static void NotarImportLibm(const Semantica *s)
{
    const RenTrocas *t = s->diag->trocas;
    const RenTroca *itens = t->itens.dados;

    if (ImportaStdMath(s)) return;
    for (size_t k = 0; k < t->itens.n; k++) {
        if (itens[k].classe == REN_LIBM_LINHA && itens[k].tam == 0) return;
    }
    RenomeadoNotar(s->diag, &(RenTroca){.off = 0, .tam = 0, .classe = REN_LIBM_LINHA, .antigo = "", .novo = "import \"std:math\" as math\n"});
}

/* Declaracao do foreign: migra -> a linha inteira sai; nao migra e e de libm -> aviso com o motivo (relato do migrar). */
void SemLibmNotarDecl(const Semantica *s, const Decl *d)
{
    const char *motivo;
    uint32_t ini, tam;

    if (s->diag->trocas == NULL) return;
    if (LibmMigravel(s, d, &motivo) == NULL) {
        if (motivo != NULL) RenomeadoNotar(s->diag, &(RenTroca){.off = d->pos, .tam = 0, .classe = REN_LIBM_AVISO, .antigo = SemNomeTexto(s, d->nome), .novo = motivo});
        return;
    }
    if (!LinhaDoDecl(s, d, &ini, &tam)) return;
    RenomeadoNotar(s->diag, &(RenTroca){.off = ini, .tam = tam, .classe = REN_LIBM_LINHA, .antigo = "", .novo = ""});
}

/* Chamada de foreign de libm que migra: o nome escrito (o da declaracao) vira math.<fn> ou o embutido sqrt. */
void SemLibmNotarChamada(const Semantica *s, const Expr *e, const Decl *d)
{
    const char *motivo, *nome;
    const Renomeado *r;

    if (s->diag->trocas == NULL || !d->externo) return;
    r = LibmMigravel(s, d, &motivo);
    if (r == NULL) return;
    nome = SemNomeTexto(s, d->nome);
    RenomeadoNotar(s->diag, &(RenTroca){.off = e->pos_op, .tam = (uint32_t)strlen(nome), .classe = REN_LIBM, .antigo = nome, .novo = r->novo});
    if (strncmp(r->novo, "math.", 5) == 0) NotarImportLibm(s);
}
