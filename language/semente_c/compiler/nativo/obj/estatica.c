/* estatica.c - escolhe os membros de bibliotecas estaticas (.a GNU, .lib MS) que o programa precisa (P23d do M3b).
 * Versao: 0.2.1
 * Changelog: 0.2.1 - M5b P36: simbolo que o proprio ligador ELF define (_GLOBAL_OFFSET_TABLE_ do -fPIC etc.) nao vira pendente (AlcanceIndiceSimboloDoLigador).
 *            0.2.0 - M5b P35: o membro sai por magica: ELF vai para ElfListarIndefinidos (GLOBAL UND), o resto segue CoffListarIndefinidos (EstListarMembro, um if).
 *            0.1.0 - M3b P23d: EstaticaSelecionar. O indice de simbolos de cada ar (ArLerIndice) diz quem define o que; a fila de simbolos pendentes vai das raizes aos indefinidos
 *                    de cada membro puxado (CoffListarIndefinidos) ate nao sobrar nada que alguma estatica defina: ponto fixo sobre TODAS as estaticas juntas (superconjunto do ld,
 *                    que varre cada .a uma vez na ordem dos -l: Q-M3-6). Os arquivos entram na ordem dos 'link'; a definicao vence pela ordem do arquivo e, dentro dele, do indice.
 *                    Simbolo exigido definido em dois arquivos diferentes = X0106 (lista os dois). Sem definicao em nenhuma estatica = pendente (quem chama decide).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_estatica.py
 */
#include <string.h>

#include "common/intern.h"
#include "nativo/obj/ar_ler.h"
#include "nativo/obj/coff.h"
#include "nativo/obj/elf_ler.h"
#include "nativo/obj/estatica.h"
#include "nativo/obj/ligar_alcance.h"

#define EST_SEM_DEF (-1)

/* Um membro de objeto do ar, na ordem do arquivo (off cresce). puxado: ja entrou na selecao. */
typedef struct {
    size_t off;
    const char *nome;
    const uint8_t *dados;
    size_t tam;
    bool puxado;
} EstMembro;

/* Uma entrada do indice: o simbolo, o arquivo (indice no pedido) e o membro (indice em EstMembro do arquivo) que o definem. prox encadeia as definicoes do mesmo simbolo
 * na ordem em que foram lidas (ordem dos 'link', depois a do indice). */
typedef struct {
    int32_t simbolo;
    uint32_t arquivo;
    uint32_t membro;
    int32_t prox;
} Definicao;

typedef struct {
    const EstaticaPedido *pedido;
    EstaticaSelecao *sel;
    Diag *erro;
    Interner nomes;  /* simbolo -> id; o texto mora em sel->memoria */
    Vec arquivos;    /* um Vec de EstMembro por arquivo do pedido (vazio no repetido) */
    Vec defs;        /* Definicao */
    Vec cabeca;      /* int32_t por id de simbolo: 1a Definicao ou EST_SEM_DEF */
    Vec cauda;       /* int32_t por id de simbolo: ultima Definicao ou EST_SEM_DEF */
    Vec visto;       /* uint8_t por id de simbolo: ja foi para a fila */
    Vec fila;        /* int32_t: ids de simbolo a resolver */
    size_t fila_pos;
} Estatica;

/* ---------- erros: sempre dizem a biblioteca (e o membro) ---------- */

static bool EstRelatarArquivo(Estatica *e, const EstaticaArquivo *arq, const Diag *lido)
{
    return DiagRelatar(e->erro, X0105, 0, "biblioteca estatica '%s': %s", arq->nome, lido->causa);
}

static bool EstRelatarMembro(Estatica *e, const EstaticaArquivo *arq, const EstMembro *m, const Diag *lido)
{
    return DiagRelatar(e->erro, X0105, 0, "biblioteca estatica '%s', membro '%s': %s", arq->nome, m->nome, lido->causa);
}

static EstMembro *EstMembroDe(const Estatica *e, uint32_t arquivo, uint32_t membro)
{
    return VecEm(VecEm(&e->arquivos, arquivo), membro);
}

static bool EstRelatarDuplicado(Estatica *e, int32_t id, const Definicao *a, const Definicao *b)
{
    return DiagRelatar(e->erro, X0106, 0, "o simbolo '%s' e definido em duas bibliotecas estaticas: '%s' (membro '%s') e '%s' (membro '%s')", InternerTexto(&e->nomes, id),
                       e->pedido->arquivos[a->arquivo].nome, EstMembroDe(e, a->arquivo, a->membro)->nome, e->pedido->arquivos[b->arquivo].nome,
                       EstMembroDe(e, b->arquivo, b->membro)->nome);
}

/* ---------- simbolos ---------- */

/* Id do simbolo; os vetores por simbolo crescem junto (ids sao sequenciais). */
static int32_t EstSimbolo(Estatica *e, const char *nome, size_t tam)
{
    int32_t id = Internar(&e->nomes, nome, (uint32_t)tam);
    while (e->cabeca.n <= (size_t)id) {
        *(int32_t *)VecEmpurrar(&e->cabeca) = EST_SEM_DEF;
        *(int32_t *)VecEmpurrar(&e->cauda) = EST_SEM_DEF;
        VecEmpurrar(&e->visto);
    }
    return id;
}

static void EstAnexarDef(Estatica *e, int32_t simbolo, uint32_t arquivo, uint32_t membro)
{
    int32_t idx = (int32_t)e->defs.n;
    *(Definicao *)VecEmpurrar(&e->defs) = (Definicao){simbolo, arquivo, membro, EST_SEM_DEF};
    int32_t *cauda = VecEm(&e->cauda, (size_t)simbolo);
    if (*cauda == EST_SEM_DEF)
        *(int32_t *)VecEm(&e->cabeca, (size_t)simbolo) = idx;
    else
        ((Definicao *)VecEm(&e->defs, (size_t)*cauda))->prox = idx;
    *cauda = idx;
}

/* ---------- carga: membros e indice de cada arquivo ---------- */

static bool EstLerMembros(Estatica *e, const EstaticaArquivo *arq, Vec *membros)
{
    ArIterador it;
    ArMembro m;
    Diag lido = {0};
    if (!ArIniciar(&it, arq->bytes, arq->n, &lido))
        return EstRelatarArquivo(e, arq, &lido);
    while (ArProximo(&it, &m, &lido)) {
        if (m.tipo != AR_OBJETO)
            continue;
        *(EstMembro *)VecEmpurrar(membros) = (EstMembro){m.off, ArenaCopiarTexto(&e->sel->memoria, m.nome, m.nome_tam), m.dados, m.tam, false};
    }
    if (lido.tem)
        return EstRelatarArquivo(e, arq, &lido);
    return true;
}

/* Indice do membro cujo cabecalho esta em off, ou -1 (busca binaria: os membros vem em ordem de offset). */
static int64_t EstAcharMembro(const Vec *membros, size_t off)
{
    size_t lo = 0, hi = membros->n;
    while (lo < hi) {
        size_t meio = lo + (hi - lo) / 2;
        size_t atual = ((const EstMembro *)VecEm(membros, meio))->off;
        if (atual == off)
            return (int64_t)meio;
        if (atual < off)
            lo = meio + 1;
        else
            hi = meio;
    }
    return -1;
}

static bool EstAnexarIndice(Estatica *e, uint32_t k, const Vec *simbolos)
{
    const EstaticaArquivo *arq = &e->pedido->arquivos[k];
    const Vec *membros = VecEm(&e->arquivos, k);
    for (size_t i = 0; i < simbolos->n; i++) {
        const ArSimbolo *s = VecEm(simbolos, i);
        int64_t membro = EstAcharMembro(membros, s->membro_off);
        if (membro < 0)
            return DiagRelatar(e->erro, X0105, 0, "biblioteca estatica '%s': o indice manda o simbolo '%s' para o offset %u, que nao e um membro", arq->nome, s->nome,
                               (unsigned)s->membro_off);
        EstAnexarDef(e, EstSimbolo(e, s->nome, s->nome_tam), k, (uint32_t)membro);
    }
    return true;
}

static bool EstIndexar(Estatica *e, uint32_t k)
{
    const EstaticaArquivo *arq = &e->pedido->arquivos[k];
    Vec simbolos;
    Diag lido = {0};
    bool ok = ArLerIndice(arq->bytes, arq->n, &simbolos, &lido) || EstRelatarArquivo(e, arq, &lido);
    ok = ok && EstAnexarIndice(e, k, &simbolos);
    VecLiberar(&simbolos);
    return ok;
}

/* O mesmo arquivo ja visto antes no pedido (um 'link' repetido): nao entra de novo. */
static bool EstRepetido(const EstaticaPedido *pedido, uint32_t k)
{
    for (uint32_t j = 0; j < k; j++) {
        if (pedido->arquivos[j].bytes == pedido->arquivos[k].bytes)
            return true;
    }
    return false;
}

static bool EstCarregar(Estatica *e)
{
    for (uint32_t k = 0; k < e->pedido->qtd_arquivos; k++) {
        Vec *membros = VecEmpurrar(&e->arquivos);
        VecIniciar(membros, sizeof(EstMembro));
        if (EstRepetido(e->pedido, k))
            continue;
        if (!EstLerMembros(e, &e->pedido->arquivos[k], membros) || !EstIndexar(e, k))
            return false;
    }
    return true;
}

/* ---------- ponto fixo ---------- */

static void EstEnfileirar(Estatica *e, const char *nome)
{
    int32_t id = EstSimbolo(e, nome, strlen(nome));
    uint8_t *visto = VecEm(&e->visto, (size_t)id);
    if (*visto != 0)
        return;
    *visto = 1;
    *(int32_t *)VecEmpurrar(&e->fila) = id;
}

/* Os indefinidos GLOBAL do membro (COFF ou ELF, pela magica): o que o membro pede e o que a selecao procura. */
static bool EstListarMembro(const uint8_t *dados, size_t tam, const CoffDestinoNomes *destino, Diag *lido)
{
    ElfLErro erro = {0};
    if (!ArEhElf(dados, tam))
        return CoffListarIndefinidos(dados, tam, destino, lido);
    const ElfLDestinoNomes elfdest = {destino->arena, destino->lista};
    if (ElfListarIndefinidos(dados, tam, &elfdest, &erro) == ELFL_OK)
        return true;
    DiagRelatar(lido, X0105, 0, "%s", erro.mensagem);
    return false;
}

/* Os indefinidos do membro puxado entram na fila. */
static bool EstEnfileirarIndefinidos(Estatica *e, uint32_t k, const EstMembro *m)
{
    Vec indef;
    Diag lido = {0};
    const CoffDestinoNomes destino = {&e->sel->memoria, &indef};
    VecIniciar(&indef, sizeof(const char *));
    if (!EstListarMembro(m->dados, m->tam, &destino, &lido)) {
        VecLiberar(&indef);
        return EstRelatarMembro(e, &e->pedido->arquivos[k], m, &lido);
    }
    for (size_t i = 0; i < indef.n; i++)
        EstEnfileirar(e, *(const char **)VecEm(&indef, i));
    VecLiberar(&indef);
    return true;
}

static bool EstPuxar(Estatica *e, const Definicao *d)
{
    EstMembro *m = EstMembroDe(e, d->arquivo, d->membro);
    if (m->puxado)
        return true;
    m->puxado = true;
    *(EstaticaMembro *)VecEmpurrar(&e->sel->membros) = (EstaticaMembro){d->arquivo, m->nome, m->dados, m->tam};
    return EstEnfileirarIndefinidos(e, d->arquivo, m);
}

/* Todas as outras definicoes do simbolo tem de ser do mesmo arquivo da primeira (varias no mesmo arquivo: vale a primeira do indice). */
static bool EstConferirUnica(Estatica *e, int32_t id, const Definicao *primeira)
{
    for (int32_t i = primeira->prox; i != EST_SEM_DEF; i = ((const Definicao *)VecEm(&e->defs, (size_t)i))->prox) {
        const Definicao *outra = VecEm(&e->defs, (size_t)i);
        if (outra->arquivo != primeira->arquivo)
            return EstRelatarDuplicado(e, id, primeira, outra);
    }
    return true;
}

static bool EstResolver(Estatica *e, int32_t id)
{
    int32_t cabeca = *(int32_t *)VecEm(&e->cabeca, (size_t)id);
    if (cabeca == EST_SEM_DEF) {
        const char *nome = InternerTexto(&e->nomes, id);
        if (AlcanceIndiceSimboloDoLigador(nome) < 0) /* __init_array_start, _GLOBAL_OFFSET_TABLE_ etc. o ligador ELF define: nao e pendente */
            *(const char **)VecEmpurrar(&e->sel->pendentes) = nome;
        return true;
    }
    const Definicao primeira = *(const Definicao *)VecEm(&e->defs, (size_t)cabeca);
    return EstConferirUnica(e, id, &primeira) && EstPuxar(e, &primeira);
}

static bool EstSelecionarRaizes(Estatica *e)
{
    for (uint32_t i = 0; i < e->pedido->qtd_raizes; i++)
        EstEnfileirar(e, e->pedido->raizes[i]);
    while (e->fila_pos < e->fila.n) {
        int32_t id = *(int32_t *)VecEm(&e->fila, e->fila_pos++);
        if (!EstResolver(e, id))
            return false;
    }
    return true;
}

/* ---------- entrada e saida ---------- */

static void EstIniciar(Estatica *e, const EstaticaPedido *pedido, EstaticaSelecao *sel, Diag *erro)
{
    *e = (Estatica){.pedido = pedido, .sel = sel, .erro = erro};
    *sel = (EstaticaSelecao){0};
    VecIniciar(&sel->membros, sizeof(EstaticaMembro));
    VecIniciar(&sel->pendentes, sizeof(const char *));
    InternerIniciar(&e->nomes, &sel->memoria);
    VecIniciar(&e->arquivos, sizeof(Vec));
    VecIniciar(&e->defs, sizeof(Definicao));
    VecIniciar(&e->cabeca, sizeof(int32_t));
    VecIniciar(&e->cauda, sizeof(int32_t));
    VecIniciar(&e->visto, sizeof(uint8_t));
    VecIniciar(&e->fila, sizeof(int32_t));
}

static void EstLiberar(Estatica *e)
{
    for (size_t k = 0; k < e->arquivos.n; k++)
        VecLiberar(VecEm(&e->arquivos, k));
    VecLiberar(&e->arquivos);
    VecLiberar(&e->defs);
    VecLiberar(&e->cabeca);
    VecLiberar(&e->cauda);
    VecLiberar(&e->visto);
    VecLiberar(&e->fila);
    InternerLiberar(&e->nomes);
}

bool EstaticaSelecionar(const EstaticaPedido *pedido, EstaticaSelecao *sel, Diag *erro)
{
    Estatica e;
    EstIniciar(&e, pedido, sel, erro);
    bool ok = EstCarregar(&e) && EstSelecionarRaizes(&e);
    EstLiberar(&e);
    return ok;
}

void EstaticaLiberar(EstaticaSelecao *sel)
{
    VecLiberar(&sel->membros);
    VecLiberar(&sel->pendentes);
    ArenaLiberar(&sel->memoria);
}
