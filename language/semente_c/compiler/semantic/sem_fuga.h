/* sem_fuga.h - fuga de endereco local (K1): nenhum endereco de local nem fatia de array local sai do proc (E0706), com resumo por callee.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.11 L3: resumo por callee (FugaResumo, FugaTabela), ponto fixo (FugaPontoFixo), SUM_CALLEE, recipiente
 *                    contaminado (sem_fuga_chamada.c) e SemFugaPrograma; SemFugaProc recebe a tabela e o modo silencioso.
 *            0.1.0 - Linguagem v1.11 L2: tipos do passe (Fuga, conjunto de origens por bit) e prototipos de sem_fuga.c, sem_fuga_expr.c e sem_fuga_cadeia.c.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 *
 * MODELO (insensivel a fluxo, por proc): cada local de tipo "com ponteiro" tem um CONJUNTO DE ORIGENS (bitset) = uniao de tudo que ja foi
 * gravado nele. Bits: [0, nl) LOCAL(k) (endereco do local k: &k, &k.c, &k[i], k[..]); [nl, nl+np) PARAM(n) (ponteiro vindo do chamador);
 * depois GLOBAL, HEAP, FOREIGN e DESCONHECIDO. Sumidouros: return, gravacao atraves de ponteiro cuja origem tem algum bit nao LOCAL.
 * Origem LOCAL num sumidouro = E0706 com a cadeia. Com laco no proc o passe se repete ate os conjuntos pararem de crescer.
 * Recipiente ([dynamic], map, soa) e um local como outro: append/insert/l[i] = v acumulam a origem de v nele.
 * RESUMO POR CALLEE (L3): cada proc da Forja tem um FugaResumo (retorno_de, flags, guarda); as voltas silenciosas sobre todos os procs
 * repetem ate nenhum resumo crescer (FugaPontoFixo) e so entao vem a volta que relata, em ordem do fonte.
 */
#ifndef FORJA_SEM_FUGA_H
#define FORJA_SEM_FUGA_H

#include "semantic/sem_interno.h"

typedef uint64_t *FugaConj;
typedef enum { FUGA_GLOBAL, FUGA_HEAP, FUGA_FOREIGN, FUGA_DESC, FUGA_FLAGS } FugaFlag;
typedef enum { SUM_RETURN, SUM_PARAM, SUM_HEAP, SUM_GLOBAL, SUM_FOREIGN, SUM_DESC, SUM_CALLEE } FugaSumTipo;
enum { FUGA_PARAMS_MAX = 64 }; /* o resumo guarda os parametros em mascara de 64 bits; do 65o em diante o parametro nao entra no resumo */

typedef struct { int32_t nome; uint32_t pos; bool rastreado; } FugaLocal;
typedef struct { int32_t var, bit, de; } FugaProv; /* 'var' ganhou LOCAL(bit) vindo de 'de' (-1 = o endereco direto) */
/* nome: o parametro (SUM_PARAM) ou o parametro que o callee guarda (SUM_CALLEE); callee: nome do proc que guarda (SUM_CALLEE); pos: o comando */
typedef struct { uint8_t tipo; int32_t nome; uint32_t pos; int32_t callee; } FugaSumidouro;

/* Resumo de um proc da Forja (por Decl.indice). Mascaras: bit n = parametro n. Monotono: so cresce ate o ponto fixo.
 * retorno_de: parametros cuja origem chega ao retorno; flags: bits FugaFlag das origens alheias ao proc que chegam ao retorno;
 * guarda[d] (d < nparams): parametros guardados na memoria apontada pelo parametro d; guarda[nparams]: parametros guardados em
 * memoria alheia ao proc (heap, global, foreign ou desconhecida). foreign e callback ficam zerados: fronteira insegura, o compilador
 * nao enxerga o que o C faz com o ponteiro (documentado; o resultado de foreign continua com a origem FOREIGN). */
typedef struct { uint64_t retorno_de; uint64_t *guarda; uint32_t nparams; uint8_t flags; } FugaResumo;
/* proc: um FugaResumo por Decl.indice; mudou: algum resumo cresceu na volta; rascunho: uint64_t reaproveitados de proc em proc (conjuntos e
 * locais do Fuga em andamento; a arena nao devolve memoria e cada volta do ponto fixo refaz todos os procs). */
typedef struct { FugaResumo *proc; uint32_t n; bool mudou; Vec rascunho; } FugaTabela;

/* Uma gravacao do valor f->v em memoria apontada por f->p. valor: expressao que deu o valor; pos: onde relatar o sumidouro;
 * callee/param: o proc que guarda e o parametro guardado (callee NULL = gravacao do proprio proc). */
typedef struct { const Expr *valor; uint32_t pos; const Decl *callee; int32_t param; } FugaGravacao;

typedef struct {
    Semantica *s;
    const Decl *proc;
    uint32_t nl, np, pal;   /* locais (com parametros), parametros e palavras de 64 bits por conjunto */
    uint64_t *conj;         /* nl conjuntos de pal palavras */
    FugaConj v, p;          /* rascunhos: valor gravado e origem do ponteiro de destino */
    FugaLocal *locais;
    Vec prov;               /* FugaProv */
    FugaTabela *tab;        /* resumos de todos os procs */
    FugaResumo *meu;        /* o resumo deste proc (um provisorio, em test) */
    bool mudou, laco;
    bool silencioso;        /* volta do ponto fixo: so calcula o resumo, nao relata nem registra proveniencia */
    bool candidato;         /* silencioso: um endereco local chegou a um sumidouro (a volta que relata vai dizer qual) */
} Fuga;

static inline FugaConj FugaConjDe(const Fuga *f, uint32_t var) { return f->conj + (size_t)var * f->pal; }
static inline uint32_t FugaBitParam(const Fuga *f, uint32_t n) { return f->nl + n; }
static inline uint32_t FugaBitFlag(const Fuga *f, FugaFlag g) { return f->nl + f->np + (uint32_t)g; }
static inline void FugaLigar(FugaConj c, uint32_t bit) { c[bit >> 6] |= 1ull << (bit & 63); }
static inline bool FugaTem(const FugaConj c, uint32_t bit) { return (c[bit >> 6] >> (bit & 63)) & 1; }
static inline void FugaLigarFlag(const Fuga *f, FugaConj c, FugaFlag g) { FugaLigar(c, FugaBitFlag(f, g)); }
static inline void FugaZerar(const Fuga *f, FugaConj c)
{
    for (uint32_t w = 0; w < f->pal; w++) c[w] = 0;
}
/* [N]T, [dynamic]T, map e soa guardam os elementos por valor: o que se grava neles fica no local, e ler um elemento herda a origem do local. */
static inline bool FugaContemPorValor(const Fuga *f, TipoId t)
{
    TipoClasse c = TipoClasseDe(&f->s->tipos, t);
    return c == TC_ARRAY || c == TC_DINAMICO || c == TC_MAPA || c == TC_SOA;
}

/* ---- sem_fuga.c ---- */
/* Confere o proc/test ja tipado sem erro: E0706 no 1o endereco local que escapa (silencioso = false). Silencioso: so atualiza o
 * resumo em tab (tab->mudou se cresceu), nunca relata e devolve false se algum endereco local chegaria a um sumidouro. */
bool SemFugaProc(Semantica *s, const Decl *d, FugaTabela *tab, bool silencioso);
void FugaUnirEm(Fuga *f, uint32_t var, const FugaConj v, const Expr *origem); /* conj[var] |= v; registra de onde veio cada LOCAL novo */
int32_t FugaPrimeiroLocal(const Fuga *f, const FugaConj c); /* menor k com LOCAL(k) em c, ou -1 */

/* ---- sem_fuga_expr.c ---- */
/* SemFugaTipoTemPonteiro: sem_interno.h */
static inline bool FugaRastreia(const Fuga *f, TipoId t) { return SemFugaTipoTemPonteiro(f->s, t); }
void FugaAvaliar(Fuga *f, const Expr *e, FugaConj dst);   /* dst |= origens do valor de e (so se o tipo de e tem ponteiro) */
void FugaEndereco(Fuga *f, const Expr *x, FugaConj dst);  /* dst |= origens de &x (x lvalor) */

/* ---- sem_fuga_resumo.c ---- */
typedef bool (*FugaRodada)(void *ctx, uint32_t i); /* refaz o resumo do proc i; true = cresceu */
bool FugaPontoFixo(uint32_t n, uint32_t teto, FugaRodada rodada, void *ctx); /* repete as voltas ate nenhuma crescer; false = passou de 'teto' voltas */
/* Corpos com ponteiro (Decl.fuga_relevante), proc e test na ordem do fonte, mais a tabela de resumos. O passe B registra cada corpo logo depois
 * de tipa-lo e chama SemFugaPrograma no fim. 'suspeito[i]': a ultima volta silenciosa viu um endereco local chegar a um sumidouro. */
typedef struct { Semantica *s; FugaTabela tab; Vec corpos; uint32_t nprocs; bool *suspeito; } CorposDaFuga;
void FugaCorposIniciar(CorposDaFuga *c, Semantica *s);
void FugaCorposRegistrar(CorposDaFuga *c, Decl *d);
void FugaCorposLiberar(CorposDaFuga *c);
bool SemFugaPrograma(CorposDaFuga *c); /* fim do passe B, com todos os corpos tipados: resumos ate o ponto fixo, depois a conferencia em ordem do fonte */
uint64_t FugaMascaraParams(const Fuga *f, const FugaConj c); /* bit n = PARAM(n) em c */
uint8_t FugaMascaraFlags(const Fuga *f, const FugaConj c);   /* bit g = flag g em c */
void FugaRegistrarRetorno(Fuga *f, const FugaConj v);        /* o valor devolvido entra no resumo (retorno_de, flags) */
void FugaRegistrarGuarda(Fuga *f, const FugaConj p, const FugaConj v); /* v guardado em memoria apontada por p entra no resumo (guarda) */
void FugaResultadoDaChamada(Fuga *f, const Expr *e, FugaConj dst);    /* dst |= origens do resultado da chamada a proc da Forja */
const FugaResumo *FugaResumoDe(const Fuga *f, const Decl *d);          /* NULL em foreign */

/* ---- sem_fuga_chamada.c ---- */
bool FugaEfeitos(Fuga *f, const Stmt *st);          /* efeitos das chamadas nas expressoes do comando: append/insert e callee que guarda parametro */
bool FugaArmazenar(Fuga *f, const FugaGravacao *g); /* grava f->v em memoria de f->p: contamina os locais apontados, alimenta o resumo, E0706 */

/* ---- sem_fuga_cadeia.c ---- */
int32_t FugaPortador(const Fuga *f, const Expr *e, uint32_t bit, int32_t excluir); /* variavel de e que carrega LOCAL(bit); -1 = nenhuma */
void FugaProvRegistrar(Fuga *f, uint32_t var, uint32_t bit, const Expr *origem);
bool FugaSumidouroDe(const Fuga *f, const FugaConj p, uint32_t pos, FugaSumidouro *saida); /* p tem origem fora do proc? diz qual */
bool FugaReportar(Fuga *f, const Expr *valor, uint32_t bit, const FugaSumidouro *sum); /* E0706 + extra "cadeia"; sempre false */

#endif
