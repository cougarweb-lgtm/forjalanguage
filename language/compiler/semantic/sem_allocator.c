/* sem_allocator.c - o tipo embutido Allocator (B01a, E0336): o alocador que um proc do usuario recebe do chamador, 'proc nome(..., alloc: Allocator) -> T'.
 * No IR e uma struct OCULTA {id: u32} (Decl fora da lista do programa, registrada no 1o uso do nome Allocator: programa sem Allocator nao ganha struct nova).
 * Em runtime todo alocador ja e um handle u32 (heap = 1, frame = 2, Arena/Pool = .id). So existe como PARAMETRO chamado 'alloc', o ultimo, uma vez, sem padrao.
 * Fora disso e E0336: let, var, campo, retorno, ^Allocator, ?Allocator, array, lista, map, tipo proc(...), o literal Allocator{...} e a leitura de .id.
 * pub export e foreign recusam o tipo pelo E0901 e E0904 (MotivoForaDaAbi, sem_export.c). Esta fatia so DECLARA: o uso no corpo e no chamador vem na B01b e na B01c.
 * Interface (sem_interno.h): SemAllocatorGarantir, SemAllocatorConferirParam, SemAllocatorForaDeParametro, SemAllocatorLerCampo, SemAllocatorLiteral e SemEhAllocator.
 * Versao: 0.5.0
 * Changelog: 0.5.0 - B01 Assess: SemAllocatorOrigemDaChamada olha o valor T de 'T ! E' (antes a origem caia a 0 e try/or_else ficavam sem rastreio de posse).
 *            0.4.0 - B01d: SemAllocatorInserirNoMigrar (chamada sem alloc vale como alloc = heap no forja migrar e registra a insercao ', alloc = heap').
 *            0.3.0 - B01c: o chamador: SemAllocatorConverterArg (heap/frame/Arena/^Arena/alloc -> Allocator{id}; scope = E0316; Pool = E0310), SemAllocatorOrigemDaChamada e SemAllocatorValorSolto (let/var/atribuicao com alloc = E0336).
 *            0.2.0 - B01b: SemAllocatorNomeForaDeParametro (transmute(x, para = Allocator) e case Allocator: = E0336); o uso 'alloc = alloc' no corpo e da sem_memoria.c.
 *            0.1.0 - B01a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include "common/renomeados.h"

#include <stdio.h>
#include <string.h>

enum { ALLOCATOR_CAMPOS = 1, TEXTO_CAUSA = 256 };

/* O unico campo: id: u32, sem padrao. */
static Campo *CamposDoAllocator(Semantica *s)
{
    Campo *campos = ArenaAlocar(s->arena, ALLOCATOR_CAMPOS * sizeof *campos);
    memset(campos, 0, ALLOCATOR_CAMPOS * sizeof *campos);
    campos[0].nome = s->nome_id;
    campos[0].tipo.forma = TR_NOME;
    campos[0].tipo.nome = SYM_U32;
    campos[0].tipo.erro = -1;
    campos[0].tipo.tipo = TIPO_U32;
    return campos;
}

void SemAllocatorGarantir(Semantica *s)
{
    Simbolo *y = &s->globais[SYM_ALLOCATOR];
    if (s->tipo_allocator != 0 || y->categoria != SIM_NENHUM) return;
    Decl *d = ArenaAlocar(s->arena, sizeof *d);
    memset(d, 0, sizeof *d);
    d->tipo_no = D_STRUCT;
    d->nome = SYM_ALLOCATOR;
    d->chave = d->nome;
    d->simbolo = -1;
    d->publico = true;
    d->campos = CamposDoAllocator(s);
    d->ncampos = ALLOCATOR_CAMPOS;
    d->indice = (int32_t)s->qtd_struct++;
    s->decl_allocator = d;
    s->tipo_allocator = TipoStruct(&s->tipos, SYM_ALLOCATOR);
    *y = (Simbolo){SIM_STRUCT, d->indice, d, s->tipo_allocator, 0};
}

/* E0336 com a dica (so_dica: o conserto mexe em mais de um ponto do codigo e depende do corpo). */
static bool ErroDeAllocator(Semantica *s, uint32_t pos, const char *causa, const char *dica)
{
    SemErro(s, E0336, pos, "%s", causa);
    DiagSugerir(&s->diag->sug, DIAG_SO_DICA, "%s", dica);
    return false;
}

/* O parametro Allocator: sem valor padrao, chamado alloc e o ultimo da lista. Os outros tipos passam sem custo. */
bool SemAllocatorConferirParam(Semantica *s, const Decl *d, const Param *p, uint32_t i)
{
    char causa[TEXTO_CAUSA];
    if (!SemEhAllocator(s, p->tipo.tipo)) return true;
    if (p->padrao != NULL) {
        return ErroDeAllocator(s, p->padrao->pos, "o parametro 'alloc: Allocator' nao tem valor padrao: o chamador sempre escolhe o alocador (heap, frame, uma Arena ou o seu alloc)",
                               "tire o valor padrao: o parametro e so alloc: Allocator");
    }
    if (p->nome != s->nome_alloc) {
        snprintf(causa, sizeof causa, "o parametro '%s' e um Allocator e precisa se chamar 'alloc' (alloc: Allocator)", SemNomeTexto(s, p->nome));
        return ErroDeAllocator(s, p->pos, causa, "renomeie o parametro Allocator para alloc");
    }
    if (i + 1 != d->nparams) {
        return ErroDeAllocator(s, p->pos, "o parametro 'alloc: Allocator' vem por ultimo na lista de parametros do proc",
                               "mova alloc: Allocator para o fim da lista de parametros");
    }
    return true;
}

/* Allocator escrito num TipoRef que nao e o do parametro 'alloc' (ParamDoProc grava s->alocador_permitido). */
bool SemAllocatorForaDeParametro(Semantica *s, const TipoRef *t)
{
    if (s->alocador_permitido == t) return true;
    return SemAllocatorNomeForaDeParametro(s, t->pos);
}

/* B01b: o nome Allocator resolvido direto por SemResolverNomeTipo (transmute(x, para = Allocator), case Allocator:), sem TipoRef: nunca e o parametro. */
bool SemAllocatorNomeForaDeParametro(Semantica *s, uint32_t pos)
{
    return ErroDeAllocator(s, pos, "o tipo Allocator so existe como parametro 'alloc: Allocator' de proc; aqui ele aparece fora de parametro (let, var, campo, retorno, ^Allocator, ?Allocator, array, lista, map ou tipo proc)",
                           "receba o alocador como ultimo parametro alloc: Allocator e repasse-o, sem guarda-lo nem devolve-lo");
}

bool SemAllocatorLerCampo(Semantica *s, const Expr *e)
{
    return ErroDeAllocator(s, e->pos_op, "nao se le o campo '.id' de um Allocator: o handle e do runtime e o alocador so se repassa (alloc = alloc)",
                           "repasse o proprio alloc (alloc = alloc) no lugar de ler o .id");
}

bool SemAllocatorLiteral(Semantica *s, const Expr *e)
{
    return ErroDeAllocator(s, e->pos_op, "nao se escreve o literal Allocator{...}: o valor vem do chamador (heap, frame, uma Arena ou o alloc dele)",
                           "receba alloc: Allocator como ultimo parametro e peca ao chamador o alocador");
}

/* B01c: o nome alloc lido fora do rotulo 'alloc =' (let y = alloc, var, atribuicao, argumento de outro tipo, operando...): o Allocator nao se guarda. */
bool SemAllocatorValorSolto(Semantica *s, const Expr *e)
{
    return ErroDeAllocator(s, e->pos, "o Allocator nao se guarda nem se usa como valor: so se repassa no rotulo 'alloc = alloc' de uma chamada (new, make, format, clone ou proc com alloc: Allocator)",
                           "tire o alloc daqui e escreva alloc = alloc na chamada que precisa do alocador");
}

/* ---------- B01c: o chamador ---------- */

/* ids de runtime do alocador: FORJA_ALOC_HEAP e FORJA_ALOC_FRAME (forja_rt.h), os mesmos de ALOCADOR_HEAP/ALOCADOR_FRAME em lower_memoria.c */
enum { ID_ALOCADOR_HEAP = 1, ID_ALOCADOR_FRAME = 2 };

/* Expr nova, zerada, na posicao de 'perto' (o fonte nao a tem: a semantica a cria resolvida, como o indice de v.x). */
static Expr *NoCriado(Semantica *s, ExprTipo tipo_no, const Expr *perto)
{
    Expr *e = ArenaAlocar(s->arena, sizeof *e);
    memset(e, 0, sizeof *e);
    e->tipo_no = tipo_no;
    e->pos = e->pos_op = perto->pos;
    return e;
}

/* O inteiro 1 ou 2 (u32) como EX_INT resolvido: o valor mora em ref_id (REF_COMPONENTE), o lowering o le dali (DobrarLiteralInteiro). */
static Expr *InteiroDoId(Semantica *s, const Expr *perto, int32_t id)
{
    Expr *e = NoCriado(s, EX_INT, perto);
    e->op = 1;
    e->ref = REF_COMPONENTE;
    e->ref_id = id;
    e->tipo = TIPO_U32;
    return e;
}

/* Allocator{id = valor} ja resolvido (SemLiteralStruct recusa o do fonte). */
static Expr *LiteralDoAllocator(Semantica *s, const Expr *perto, Expr *id)
{
    Expr *lit = NoCriado(s, EX_LITERAL_STRUCT, perto);
    lit->nome = SYM_ALLOCATOR;
    lit->ref = REF_STRUCT;
    lit->ref_id = s->decl_allocator->indice;
    lit->tipo = s->tipo_allocator;
    lit->nargs = 1;
    lit->args = ArenaAlocar(s->arena, sizeof(Arg));
    lit->args[0] = (Arg){.nome = s->nome_id, .pos_nome = perto->pos, .valor = id, .indice = 0};
    return lit;
}

/* Arena ou ^Arena: o handle .id vira o Allocator (como o 'alloc = arena' dos embutidos, que troca o valor por .id). */
static bool ArenaComoAllocator(Semantica *s, Expr *chamada, Arg *g)
{
    Expr *campo = NoCriado(s, EX_CAMPO, g->valor);
    campo->a = g->valor;
    campo->nome = s->nome_id;
    chamada->alocacao = ALOC_ARENA;
    g->valor = LiteralDoAllocator(s, campo, campo);
    return SemChecar(s, campo, TIPO_VAZIO);
}

/* Valor que nao e heap, frame nem scope: o proprio alloc (repasse), Arena, ^Arena, Pool ou outro tipo (este fica para o erro de tipo de SemCoagir). */
static bool ConverterValorDoArg(Semantica *s, Expr *chamada, Arg *g)
{
    Expr *v = g->valor;
    s->alocador_lendo = v->tipo_no == EX_NOME; /* o repasse 'alloc = alloc' e a leitura que vale */
    bool lido = SemChecar(s, v, TIPO_VAZIO);
    s->alocador_lendo = false;
    if (!lido) return false;
    if (SemEhAllocator(s, v->tipo)) {
        chamada->alocacao = ALOC_PARAM;
        return true;
    }
    const TipoInfo *k = TipoDe(&s->tipos, v->tipo);
    TipoId base = k->classe == TC_PONTEIRO ? (TipoId)k->a : v->tipo;
    if (s->tipo_pool != 0 && base == s->tipo_pool) {
        return SemErro(s, E0310, v->pos, "Pool so serve new: um proc com 'alloc: Allocator' recebe heap, frame, uma Arena ou o seu alloc");
    }
    if (s->tipo_arena == 0 || base != s->tipo_arena) return true;
    return ArenaComoAllocator(s, chamada, g);
}

/* O argumento do parametro 'alloc: Allocator' de proc do usuario: heap e frame viram Allocator{id = 1|2}, Arena/^Arena Allocator{id = a.id}, o alloc do
 * chamador passa como esta; scope = E0316. Grava chamada->alocacao (a origem do resultado, lida por SemOrigemDoValor). */
bool SemAllocatorConverterArg(Semantica *s, Expr *chamada, Arg *g)
{
    Expr *v = g->valor;
    uint8_t aloc = SemAlocacaoDoNome(s, v);
    if (aloc == ALOC_ESCOPO) {
        return SemErro(s, E0316, v->pos, "a alocacao de scope nao pode ser entregue a proc do usuario na v1: use heap, frame, uma Arena ou o seu alloc");
    }
    if (aloc == ALOC_NENHUM) return ConverterValorDoArg(s, chamada, g);
    chamada->alocacao = aloc;
    g->valor = LiteralDoAllocator(s, v, InteiroDoId(s, v, aloc == ALOC_HEAP ? ID_ALOCADOR_HEAP : ID_ALOCADOR_FRAME));
    return true;
}

/* ---------- B01d: forja migrar ---------- */

static bool CharDeNome(char c)
{
    return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Onde entra o rotulo que falta: logo depois do ultimo argumento escrito (Arg.pos_fim) ou, sem argumentos, logo depois do '(' que segue o nome do proc. 0 = nao achou. */
static uint32_t PosDaInsercaoDoAlloc(const Semantica *s, const Expr *e)
{
    if (e->nargs > 0) return e->args[e->nargs - 1].pos_fim;
    const char *t = s->fonte->texto;
    uint32_t i = e->pos_op;
    while (CharDeNome(t[i])) i++;
    while (t[i] == ' ' || t[i] == '\t') i++;
    return t[i] == '(' ? i + 1 : 0;
}

/* Chamada sem o parametro 'alloc: Allocator' no 'forja migrar' (Diag.trocas ligado): vale como 'alloc = heap' (o comportamento de antes) e registra a insercao do rotulo. Fora do
 * migrar, ou parametro de outro tipo, devolve false e o chamador relata o E0503 de sempre. */
bool SemAllocatorInserirNoMigrar(Semantica *s, Expr *e, const Param *faltante)
{
    if (s->diag->trocas == NULL || !SemEhAllocator(s, faltante->tipo.tipo)) return false;
    uint32_t pos = PosDaInsercaoDoAlloc(s, e);
    if (pos == 0) return false;
    RenomeadoNotar(s->diag, &(RenTroca){.off = pos, .tam = 0, .classe = REN_ARGUMENTO_NOVO, .antigo = "", .novo = e->nargs > 0 ? ", alloc = heap" : "alloc = heap"});
    e->alocacao = ALOC_HEAP;
    return true;
}

/* O resultado da chamada so herda a origem se pode ser memoria dona do alocador: texto, ^T, ?^T, lista, map ou soa dinamica; int, bool, struct... voltam a 0.
 * Em 'T ! E' vale o tipo do valor T: try e or_else leem a origem do resultado da chamada. */
void SemAllocatorOrigemDaChamada(const Semantica *s, Expr *e)
{
    if (e->alocacao == ALOC_NENHUM) return;
    TipoId valor = e->tipo;
    if (TipoClasseDe(&s->tipos, valor) == TC_RESULTADO) valor = TipoDe(&s->tipos, valor)->a;
    TipoClasse classe = TipoClasseDe(&s->tipos, valor);
    if (valor == TIPO_TEXTO || classe == TC_PONTEIRO || classe == TC_ANULAVEL || SemColecaoDona(s, valor)) return;
    e->alocacao = ALOC_NENHUM;
}
