/* x64_regalloc.c - passe `alocador` (M7a F3/F4): linear scan dos intervalos de x64_regalloc_intervalos.c, um banco por vez: inteiros sobre rbx, rsi, rdi, r12..r15 e r8..r10; reais sobre
 * xmm2..xmm5 e xmm6..xmm15. Ver x64_regalloc.h.
 * Versao: 0.3.3
 * Changelog: 0.3.3 - S1b (D0703): vreg BC_V128 no banco xmm (MascarasDosSalvos por BirClasseEhXmm).
 *            0.3.2 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.3.1 - M7b R3: rastro FORJA_ALOC_RASTRO=1 (stderr, uma linha por despejo da regra 2: funcao, vitima e atual com peso, comprimento e se e redefinida em laco).
 *            A guarda G1 (regra 2 nunca despeja vitima `carregada` pelo laco) foi tentada UMA vez e REVERTIDA: getter_vec3 sobe de 41 para 165 refs de pilha em fj_main (sem a numeracao), matrix4 nao muda, lista_append 59 -> 52.
 *            0.3.0- regra 2 (densidade): sem vitima pelo peso, despeja o ativo de menor densidade (peso / comprimento) que o registrador serve ao atual, se for ORC_DESPEJO_K vezes
 *            menos denso. Muda a alocacao de TODO codigo release (nao so do sroa); a regra 1 fica igual.
 *            0.2.1 - M7b F2: OrdenarElegiveis por contagem sobre o inicio (no lugar do qsort; mesma ordem (inicio, id)).
 *            0.2.0 - M7a F4: scan parametrizado por banco (Banco); banco xmm (volateis xmm2..5, callee-saved xmm6..15); regs_salvos_xmm; funcao com chamada SysV nao usa xmm6..15.
 *            0.1.0 - M7a F3: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F3>
 *
 * Spill do intervalo INTEIRO (sem dividir): sem registrador livre perde quem tem o menor peso (usos x 10^profundidade) entre o atual e os ativos que o registrador serve.
 * Regra 2 (0.3.0): se nenhum ativo pesa menos que o atual, despeja o de menor densidade (peso / comprimento) se ele for ORC_DESPEJO_K vezes menos denso que o atual.
 * Callee-saved so entra se a funcao ja o salva ou se o intervalo pesa >= MIN_PESO_SALVO (o push/pop custa dois acessos a memoria por chamada da funcao; no xmm, movups de ida e volta).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/x64/x64_interno.h"
#include "nativo/x64/x64_regalloc.h"
#include "nativo/x64/x64_regalloc_interno.h"

#define MIN_PESO_SALVO 4u
#define SEM_DONO UINT32_MAX
#define MAX_REGS_BANCO 16u
/* Regra 2: fator de densidade para despejar. K = 16: o unico valor que nao piora instrucoes estaticas dos 12 benches fora de matrix4_f32/getter_vec3
 * (K 2..8 pioram lista_append e transformada_aos_soa). matrix4_f32 fica em ~210 refs_pilha para qualquer K >= 4. */
#define ORC_DESPEJO_K 16u
/* Teto de posicoes para a conta 64 bits da regra 2: peso <= usos x 10^6 e comprimento <= npos; neste teto o produto K x peso x comprimento cabe em uint64_t. */
#define ORC_DESPEJO_NPOS_MAX 262144u

/* Ordem de preferencia: os volateis primeiro (nao custam push), depois os callee-saved (rsi e rdi por ultimo: a chamada SysV os destroi). */
static const uint8_t REGS_VOLATEIS[] = {X64_R8, X64_R9, X64_R10};
static const uint8_t REGS_SALVOS[] = {X64_RBX, X64_R12, X64_R13, X64_R14, X64_R15, X64_RSI, X64_RDI};
static const uint8_t REGS_TODOS[] = {X64_R8, X64_R9, X64_R10, X64_RBX, X64_R12, X64_R13, X64_R14, X64_R15, X64_RSI, X64_RDI};

/* xmm0 e xmm1 sao o rascunho do isel (x64_isel_real.c) e os argumentos k < 4 / o retorno usam xmm0..3 so em torno de uma chamada, onde so intervalos que a cruzam os leem (nao
 * entram em xmm2..5). xmm4 e xmm5 ninguem usa. xmm6..15 sao callee-saved no Win64: o prologo os guarda com movups e o epilogo os restaura (x64_quadro.c). */
static const uint8_t XMM_VOLATEIS[] = {2, 3, 4, 5};
static const uint8_t XMM_SALVOS[] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const uint8_t XMM_TODOS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

#define MASCARA_GPR_VOLATIL ((1u << X64_R8) | (1u << X64_R9) | (1u << X64_R10))
#define MASCARA_GPR_SYSV ((1u << X64_RSI) | (1u << X64_RDI)) /* a chamada SysV os destroi */
#define MASCARA_XMM_VOLATIL (0xFu << 2)
#define MASCARA_XMM_SYSV (0x3FFu << 6) /* a chamada SysV destroi xmm0..15: nenhum callee-saved sobrevive a ela */

typedef struct {
    const uint8_t *todos, *volateis, *salvos;
    uint32_t n_todos, n_volateis, n_salvos;
    uint32_t mascara_volatil;     /* bit = registrador do banco que a chamada destroi */
    uint32_t proibidos_sysv;      /* bit = registrador que a funcao com chamada SysV nao usa */
    bool real;
} Banco;

static const Banco BANCO_INTEIRO = {REGS_TODOS, REGS_VOLATEIS, REGS_SALVOS, sizeof REGS_TODOS, sizeof REGS_VOLATEIS, sizeof REGS_SALVOS, MASCARA_GPR_VOLATIL, MASCARA_GPR_SYSV, false};
static const Banco BANCO_XMM = {XMM_TODOS, XMM_VOLATEIS, XMM_SALVOS, sizeof XMM_TODOS, sizeof XMM_VOLATEIS, sizeof XMM_SALVOS, MASCARA_XMM_VOLATIL, MASCARA_XMM_SYSV, true};

typedef struct { uint32_t inicio, vreg; } EntradaScan;

typedef struct {
    const Intervalos *intervalos;
    const Banco *banco;
    uint32_t proibidos;         /* registradores fora do scan nesta funcao (chamada SysV) */
    uint32_t dono[MAX_REGS_BANCO]; /* por registrador: vreg que o ocupa agora ou SEM_DONO */
    uint32_t mascara;           /* callee-saved ja entregues (bit = numero do registrador) */
    uint8_t *reg_vreg;
} Scan;

static bool EhVolatil(const Scan *s, uint8_t reg)
{
    return ((s->banco->mascara_volatil >> reg) & 1u) != 0;
}

static bool RegistradorServe(const Scan *s, const IntervaloVreg *iv, uint8_t reg)
{
    if (EhVolatil(s, reg))
        return !iv->cruza_chamada;
    if (((s->proibidos >> reg) & 1u) != 0)
        return false;
    return ((s->mascara >> reg) & 1u) != 0 || iv->peso >= MIN_PESO_SALVO;
}

/* Lista de candidatos; so_salvos_usados limita aos callee-saved que a funcao ja salva. */
typedef struct { const uint8_t *regs; uint32_t n; bool so_salvos_usados; } Candidatos;

/* Primeiro registrador livre dos candidatos que serve; -1 se nenhum. */
static int ProcurarLivre(const Scan *s, const IntervaloVreg *iv, Candidatos c)
{
    for (uint32_t k = 0; k < c.n; k++) {
        uint8_t reg = c.regs[k];

        if (s->dono[reg] != SEM_DONO || !RegistradorServe(s, iv, reg))
            continue;
        if (!c.so_salvos_usados || ((s->mascara >> reg) & 1u) != 0)
            return reg;
    }
    return -1;
}

static int EscolherLivre(const Scan *s, const IntervaloVreg *iv)
{
    const Banco *b = s->banco;
    int reg = ProcurarLivre(s, iv, (Candidatos){b->volateis, b->n_volateis, false});

    if (reg < 0)
        reg = ProcurarLivre(s, iv, (Candidatos){b->salvos, b->n_salvos, true});
    if (reg < 0)
        reg = ProcurarLivre(s, iv, (Candidatos){b->salvos, b->n_salvos, false});
    return reg;
}

/* O ativo de menor peso (estritamente menor que o do atual) cujo registrador serve ao atual; -1 se nenhum. */
static int EscolherVitima(const Scan *s, const IntervaloVreg *atual)
{
    int escolhido = -1;
    uint64_t menor = atual->peso;

    for (uint32_t k = 0; k < s->banco->n_todos; k++) {
        uint8_t reg = s->banco->todos[k];
        uint32_t dono = s->dono[reg];

        if (dono == SEM_DONO || !RegistradorServe(s, atual, reg) || s->intervalos->iv[dono].peso >= menor)
            continue;
        menor = s->intervalos->iv[dono].peso;
        escolhido = reg;
    }
    return escolhido;
}

/* Regra 2 (0.3.0): sem vitima pelo peso, despeja o ativo de MENOR densidade (peso / comprimento) entre os que o registrador serve ao atual, se ele for ORC_DESPEJO_K
 * vezes menos denso que o atual; -1 caso contrario. Sem divisao: dens(v) x K < dens(a) vira peso_v x K x comp_a < peso_a x comp_v (uint64_t). */
static int EscolherPorDensidade(const Scan *s, const IntervaloVreg *atual)
{
    const IntervaloVreg *ivs = s->intervalos->iv;
    uint64_t comp_atual = (uint64_t)atual->fim - atual->inicio + 1u;
    uint64_t peso_vitima = 0, comp_vitima = 1u;
    int escolhido = -1;

    if (s->intervalos->npos > ORC_DESPEJO_NPOS_MAX)
        return -1;
    for (uint32_t k = 0; k < s->banco->n_todos; k++) {
        uint8_t reg = s->banco->todos[k];
        uint32_t dono = s->dono[reg];
        uint64_t comp;

        if (dono == SEM_DONO || !RegistradorServe(s, atual, reg))
            continue;
        comp = (uint64_t)ivs[dono].fim - ivs[dono].inicio + 1u;
        if (escolhido < 0 || ivs[dono].peso * comp_vitima < peso_vitima * comp) {
            escolhido = reg;
            peso_vitima = ivs[dono].peso;
            comp_vitima = comp;
        }
    }
    if (escolhido < 0 || peso_vitima * ORC_DESPEJO_K * comp_atual >= atual->peso * comp_vitima)
        return -1;
    return escolhido;
}

static void LiberarExpirados(Scan *s, uint32_t inicio)
{
    for (uint32_t k = 0; k < s->banco->n_todos; k++) {
        uint8_t reg = s->banco->todos[k];
        uint32_t dono = s->dono[reg];

        if (dono != SEM_DONO && s->intervalos->iv[dono].fim < inicio)
            s->dono[reg] = SEM_DONO;
    }
}

static void Entregar(Scan *s, uint32_t vreg, uint8_t reg)
{
    s->dono[reg] = vreg;
    s->reg_vreg[vreg] = reg;
    if (!EhVolatil(s, reg))
        s->mascara |= 1u << reg;
}

/* Rastro neutro (M7b R3): FORJA_ALOC_RASTRO=1 imprime no stderr uma linha por despejo da regra 2; sem a variavel o compilador nao muda. getenv uma vez. */
static const char *gRastroFuncao;

static bool RastroLigado(void)
{
    static int ligado = -1;

    if (ligado < 0) {
        const char *e = getenv("FORJA_ALOC_RASTRO");

        ligado = e != NULL && strcmp(e, "1") == 0;
    }
    return ligado != 0;
}

static void RastrearDespejo(const Scan *s, uint32_t atual, uint32_t vitima)
{
    const IntervaloVreg *a = &s->intervalos->iv[atual], *v = &s->intervalos->iv[vitima];

    fprintf(stderr, "aloc-rastro %s despejo: vitima=v%u peso=%llu comp=%u def_laco=%d carregada=%d | atual=v%u peso=%llu comp=%u\n", gRastroFuncao, (unsigned)vitima,
            (unsigned long long)v->peso, (unsigned)(v->fim - v->inicio + 1u), v->def_em_laco ? 1 : 0, v->carregada ? 1 : 0, (unsigned)atual, (unsigned long long)a->peso,
            (unsigned)(a->fim - a->inicio + 1u));
}

static void AlocarUm(Scan *s, uint32_t vreg)
{
    const IntervaloVreg *iv = &s->intervalos->iv[vreg];
    int reg = EscolherLivre(s, iv);

    if (reg < 0) {
        reg = EscolherVitima(s, iv);
        if (reg < 0) {
            reg = EscolherPorDensidade(s, iv);
            if (reg >= 0 && RastroLigado())
                RastrearDespejo(s, vreg, s->dono[reg]);
        }
        if (reg < 0)
            return; /* o atual e o mais leve e nao e muito mais denso que os vizinhos: vai inteiro para o slot */
        s->reg_vreg[s->dono[reg]] = BIR_SEM_REG;
    }
    Entregar(s, vreg, (uint8_t)reg);
}

/* Vregs elegiveis do banco em ordem (inicio, id); lista vazia (nao NULL) sem nenhum. Ordenacao por contagem sobre o inicio (0..npos): estavel, e os vregs entram em ordem de id.
 * (O qsort custava ~8 ms numa funcao de 109 mil vregs.) */
static EntradaScan *OrdenarElegiveis(const Intervalos *intervalos, bool real, uint32_t *quantos)
{
    EntradaScan *lista = malloc(sizeof *lista * (intervalos->nvregs + 1u));
    uint32_t *posicao = calloc((size_t)intervalos->npos + 2u, sizeof *posicao);
    uint32_t n = 0;

    if (lista == NULL || posicao == NULL)
        FalhaInterna("sem memoria no alocador de registradores");
    for (uint32_t v = 0; v < intervalos->nvregs; v++) {
        if (intervalos->iv[v].elegivel && intervalos->iv[v].real == real) {
            posicao[intervalos->iv[v].inicio + 1u]++;
            n++;
        }
    }
    for (uint32_t p = 1; p <= intervalos->npos + 1u; p++)
        posicao[p] += posicao[p - 1u];
    for (uint32_t v = 0; v < intervalos->nvregs; v++) {
        if (intervalos->iv[v].elegivel && intervalos->iv[v].real == real)
            lista[posicao[intervalos->iv[v].inicio]++] = (EntradaScan){intervalos->iv[v].inicio, v};
    }
    free(posicao);
    *quantos = n;
    return lista;
}

/* Roda o scan de um banco sobre reg_vreg (os vregs do outro banco nao sao tocados). */
static void ExecutarBanco(const Banco *banco, const Intervalos *intervalos, uint8_t *reg_vreg)
{
    Scan s = {.intervalos = intervalos, .banco = banco, .reg_vreg = reg_vreg};
    uint32_t n;
    EntradaScan *ordem = OrdenarElegiveis(intervalos, banco->real, &n);

    if (intervalos->tem_sysv)
        s.proibidos = banco->proibidos_sysv;
    for (uint32_t k = 0; k < MAX_REGS_BANCO; k++)
        s.dono[k] = SEM_DONO;
    for (uint32_t k = 0; k < n; k++) {
        LiberarExpirados(&s, ordem[k].inicio);
        AlocarUm(&s, ordem[k].vreg);
    }
    free(ordem);
}

/* Mascaras exatas dos callee-saved que sobraram em reg_vreg (a vitima pode ter devolvido um registrador ja marcado). */
static void MascarasDosSalvos(BirFuncao *f)
{
    f->regs_salvos = 0;
    f->regs_salvos_xmm = 0;
    for (uint32_t v = 0; v < f->nvregs; v++) {
        uint8_t reg = f->reg_vreg[v];
        bool real = BirClasseEhXmm(f->classe_vreg[v]);

        if (reg == BIR_SEM_REG)
            continue;
        if (real && ((MASCARA_XMM_VOLATIL >> reg) & 1u) == 0)
            f->regs_salvos_xmm |= 1u << reg;
        else if (!real && ((MASCARA_GPR_VOLATIL >> reg) & 1u) == 0)
            f->regs_salvos |= 1u << reg;
    }
}

static bool AlgumEmRegistrador(const BirFuncao *f, const uint8_t *reg_vreg)
{
    for (uint32_t v = 0; v < f->nvregs; v++) {
        if (reg_vreg[v] != BIR_SEM_REG)
            return true;
    }
    return false;
}

/* Roda os dois bancos; devolve reg_vreg (malloc) ou NULL se nenhum vreg ganhou registrador. */
static uint8_t *ExecutarScan(const BirFuncao *f, const Intervalos *intervalos)
{
    uint8_t *reg_vreg = malloc(f->nvregs + 1u);

    if (reg_vreg == NULL)
        FalhaInterna("sem memoria no alocador de registradores");
    memset(reg_vreg, BIR_SEM_REG, f->nvregs + 1u);
    ExecutarBanco(&BANCO_INTEIRO, intervalos, reg_vreg);
    ExecutarBanco(&BANCO_XMM, intervalos, reg_vreg);
    if (!AlgumEmRegistrador(f, reg_vreg)) {
        free(reg_vreg);
        return NULL;
    }
    return reg_vreg;
}

void X64AlocadorAplicar(const BirPrograma *programa, BirFuncao *funcao)
{
    AnalisesBir analises;
    Arena arena = {0};
    Intervalos intervalos = {.arena = &arena};

    gRastroFuncao = programa->simbolos[funcao->simbolo].nome;
    free(funcao->reg_vreg);
    funcao->reg_vreg = NULL;
    funcao->regs_salvos = 0;
    funcao->regs_salvos_xmm = 0;
    if (funcao->nblocos == 0 || funcao->nvregs == 0)
        return;
    AnalisesCalcular(funcao, &analises);
    RegallocIntervalos(programa, funcao, &analises, &intervalos);
    funcao->reg_vreg = ExecutarScan(funcao, &intervalos);
    if (funcao->reg_vreg != NULL)
        MascarasDosSalvos(funcao);
    AnalisesLiberar(&analises);
    ArenaLiberar(&arena);
}
