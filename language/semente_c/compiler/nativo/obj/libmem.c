/* libmem.c - contabilidade de memoria por biblioteca estatica em DEV: mapa slot -> lib, redirecionamento da familia de alocacao e tabela FLM1 (M6d P92c, D-M6d-2/3/4).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6d P93h (e): LibmemMontar recebe LibmemEntrada {estaticas, fins, qtd_fins} + arena + saida (3 parametros; 1 chamador, nativo_biblioteca.c).
 *            0.2.1 - M6d P92i (revisor P1): LibmemRedirecionar calcula o simbolo novo ANTES de ler o endereco do reloc (LmAlvoDoSimbolo pode realocar o->relocs). 0.2.0 - M6d P92e (D-M6d-4): LibmemMontar acha, pelo indice do ar (ArLerIndice), o slot que define cada fim `ends`; a FLM1 grava off_fins ("a\0b\0\0") ou 0.
 *            0.1.0 - M6d P92c: primeira versao. So existe com dev; com TerceirosPedido.libmem == NULL nada daqui roda e o objeto de terceiros sai como antes. Desvio do D-M6d-2: o simbolo
 *            DEFINIDO por membro de arquivo NAO contado (o malloc do musl, que no alvo Linux entra na mesma uniao) tambem e redirecionado; so o definido por arquivo contado fica intocado.
 * Verificacao: construir_rapido.py && teste_nativo.exe (OK: N verificacoes)
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "common/buf.h"
#include "nativo/obj/ar_ler.h"
#include "nativo/obj/bibliotecas_nomes.h"
#include "nativo/obj/estatica.h"
#include "nativo/obj/libmem.h"
#include "nativo/obj/terceiros.h"

#define LM_PREFIXO_IMP "__imp_"
#define LM_TAM_PREFIXO_IMP 6u
#define LM_MAGIA 0x314D4C46u /* "FLM1" */
#define LM_SECAO_TABELA ".rdata$libmem"
#define LM_ALINHA_TABELA 4u
#define LM_TAM_NOME 96u

/* A familia fechada (D-M6d-2). */
static const char *const LM_FAMILIA[] = {"malloc", "calloc", "realloc", "free", "strdup", "_strdup"};
#define LM_QTD_FAMILIA (sizeof LM_FAMILIA / sizeof LM_FAMILIA[0])

/* ---------- mapa ---------- */

static const char *LmNomeDoArquivo(const char *caminho)
{
    const char *barra = strrchr(caminho, '/');
    const char *invertida = strrchr(caminho, '\\');

    if (invertida != NULL && (barra == NULL || invertida > barra))
        barra = invertida;
    return barra != NULL ? barra + 1 : caminho;
}

/* O slot do arquivo k: o do mesmo ponteiro de bytes que ja entrou, ou um novo; -1 se nao conta. */
static int LmSlotDoArquivo(const Vec *estaticas, size_t k, const int8_t *slots, TerceirosLibmem *lm)
{
    const EstaticaArquivo *a = VecEm(estaticas, k);

    if (a->link == NULL || BibsCoberta(a->link))
        return -1;
    for (size_t j = 0; j < k; j++) {
        if (slots[j] >= 0 && ((const EstaticaArquivo *)VecEm(estaticas, j))->bytes == a->bytes)
            return slots[j];
    }
    if (lm->qtd_slots >= FORJA_LIBMEM_SLOTS) {
        if (lm->excedentes++ == 0)
            lm->primeiro_excedente = a->link;
        return -1;
    }
    lm->nome[lm->qtd_slots] = a->link;
    lm->arquivo[lm->qtd_slots] = LmNomeDoArquivo(a->nome);
    return (int)lm->qtd_slots++;
}

/* O arquivo (ar) define o simbolo no seu indice? */
static bool LmArquivoDefine(const EstaticaArquivo *a, const char *simbolo)
{
    Vec indice;
    Diag erro = {0};
    bool achou = false;

    if (ArLerIndice(a->bytes, a->n, &indice, &erro)) {
        for (size_t s = 0; s < indice.n && !achou; s++) {
            const ArSimbolo *e = VecEm(&indice, s);

            achou = strlen(simbolo) == e->nome_tam && memcmp(e->nome, simbolo, e->nome_tam) == 0;
        }
    }
    VecLiberar(&indice);
    return achou;
}

/* O texto dos fins do slot: os simbolos que o arquivo dele define, "a\0b\0\0" na arena; NULL se nenhum (a lib nao declara `ends`). */
static const char *LmFinsDoSlot(const EstaticaArquivo *a, const char *const *fins, uint32_t qtd_fins, Arena *arena)
{
    size_t total = 1;
    char *texto;
    size_t pos = 0;

    for (uint32_t f = 0; f < qtd_fins; f++) {
        if (LmArquivoDefine(a, fins[f]))
            total += strlen(fins[f]) + 1;
    }
    if (total == 1)
        return NULL;
    texto = ArenaAlocar(arena, total);
    for (uint32_t f = 0; f < qtd_fins; f++) {
        if (LmArquivoDefine(a, fins[f])) {
            memcpy(texto + pos, fins[f], strlen(fins[f]) + 1);
            pos += strlen(fins[f]) + 1;
        }
    }
    texto[pos] = '\0';
    return texto;
}

/* M6d P93h: a entrada vai numa struct (3 campos) para caber nos 4 parametros; os fins sao os simbolos `ends` do programa. */
void LibmemMontar(const LibmemEntrada *entrada, Arena *arena, TerceirosLibmem *saida)
{
    const Vec *estaticas = entrada->estaticas;
    int8_t *slots = ArenaAlocar(arena, estaticas->n + 1);

    *saida = (TerceirosLibmem){.qtd_arquivos = (uint32_t)estaticas->n, .slot_do_arquivo = slots};
    for (size_t k = 0; k < estaticas->n; k++)
        slots[k] = (int8_t)LmSlotDoArquivo(estaticas, k, slots, saida);
    for (size_t k = 0; k < estaticas->n && entrada->qtd_fins > 0; k++) {
        if (slots[k] >= 0 && saida->fins[slots[k]] == NULL)
            saida->fins[slots[k]] = LmFinsDoSlot(VecEm(estaticas, k), entrada->fins, entrada->qtd_fins, arena);
    }
    if (saida->excedentes > 0)
        fprintf(stderr, "aviso: so as %u primeiras bibliotecas estaticas tem a memoria contada em dev; %s fica de fora\n", FORJA_LIBMEM_SLOTS, saida->primeiro_excedente);
}

/* ---------- redirecionamento ---------- */

/* Indice na familia do nome (com ou sem __imp_); -1 se nao e da familia. *imp diz se veio com o prefixo. */
static int LmFamilia(const char *nome, bool *imp)
{
    *imp = strncmp(nome, LM_PREFIXO_IMP, LM_TAM_PREFIXO_IMP) == 0;
    if (*imp)
        nome += LM_TAM_PREFIXO_IMP;
    for (size_t f = 0; f < LM_QTD_FAMILIA; f++) {
        if (strcmp(nome, LM_FAMILIA[f]) == 0)
            return (int)f;
    }
    return -1;
}

/* slot_da_secao[s]: o slot do arquivo do membro dono da secao s (-1 = nao conta). Cada membro vai de inicios[m] ate inicios[m+1]. */
static int8_t *LmSlotsDasSecoes(const Objeto *o, const Vec *membros, const Vec *inicios, const TerceirosLibmem *lm)
{
    int8_t *slots = malloc(o->secoes.n + 1);

    if (slots == NULL)
        FalhaInterna("libmem: sem memoria para %zu secoes", o->secoes.n);
    memset(slots, -1, o->secoes.n + 1);
    for (size_t m = 0; m < membros->n; m++) {
        const EstaticaMembro *membro = VecEm(membros, m);
        size_t de = *(const uint32_t *)VecEm(inicios, m);
        size_t ate = m + 1 < inicios->n ? *(const uint32_t *)VecEm(inicios, m + 1) : o->secoes.n;
        int8_t slot = membro->arquivo < lm->qtd_arquivos ? lm->slot_do_arquivo[membro->arquivo] : (int8_t)-1;

        for (size_t s = de; s < ate; s++)
            slots[s] = slot;
    }
    return slots;
}

/* O simbolo da relocacao e da familia e NAO e definido por membro de arquivo contado (indefinido, ou definido pelo musl/CRT: o malloc real do alvo Linux entra na mesma uniao) e a
 * secao e de arquivo contado: devolve o indice do simbolo novo (e cria a celula __imp_ se for o caso); senao o proprio. A lib que traz o proprio malloc fica intocada. */
static uint32_t LmAlvoDoSimbolo(Objeto *o, uint32_t simbolo, int slot, const int8_t *slots)
{
    const ObjSimbolo *s = VecEm(&o->simbolos, simbolo);
    char novo[LM_TAM_NOME];
    bool imp;
    int f;

    if (s->secao != OBJ_SECAO_INDEFINIDA && slots[s->secao] >= 0)
        return simbolo;
    if ((f = LmFamilia(s->nome, &imp)) < 0)
        return simbolo;
    snprintf(novo, sizeof novo, "%sforja_libmem_%s_%d", imp ? LM_PREFIXO_IMP : "", LM_FAMILIA[f], slot);
    simbolo = ObjSimboloReferir(o, novo);
    if (imp && ((const ObjSimbolo *)VecEm(&o->simbolos, simbolo))->secao == OBJ_SECAO_INDEFINIDA)
        TerceirosCriarCelula(o, novo + LM_TAM_PREFIXO_IMP);
    return simbolo;
}

void LibmemRedirecionar(Objeto *o, const Vec *membros, const Vec *inicios, const TerceirosLibmem *lm)
{
    int8_t *slots = LmSlotsDasSecoes(o, membros, inicios, lm);
    size_t relocs = o->relocs.n;

    for (size_t r = 0; r < relocs; r++) {
        uint32_t secao = ((const ObjReloc *)VecEm(&o->relocs, r))->alvo.secao;
        uint32_t simbolo;
        uint32_t novo;

        if (secao >= o->secoes.n || slots[secao] < 0)
            continue;
        simbolo = ((const ObjReloc *)VecEm(&o->relocs, r))->correcao.simbolo;
        /* LmAlvoDoSimbolo pode anexar a o->relocs (TerceirosCriarCelula) e realocar o vetor: o destino so e calculado depois. */
        novo = LmAlvoDoSimbolo(o, simbolo, slots[secao], slots);
        ((ObjReloc *)VecEm(&o->relocs, r))->correcao.simbolo = novo;
    }
    free(slots);
}

/* ---------- tabela FLM1 ---------- */

static void LmAnexar32(Buf *b, uint32_t v)
{
    BufAnexarN(b, (const char *)&v, sizeof v);
}

static void LmAnexarTexto(Buf *b, const char *texto)
{
    BufAnexarN(b, texto, strlen(texto) + 1);
}

/* Bytes de "a\0b\0\0" (com o NUL duplo que fecha). */
static size_t LmTamanhoFins(const char *fins)
{
    size_t n = 0;

    while (fins[n] != '\0')
        n += strlen(fins + n) + 1u;
    return n + 1u;
}

void LibmemTabela(Objeto *o, const TerceirosLibmem *lm)
{
    Buf b = {0};
    uint32_t secao = ObjSecaoAbrir(o, LM_SECAO_TABELA, OS_RDATA, LM_ALINHA_TABELA);
    uint32_t pos = 8u + 12u * lm->qtd_slots;
    uint32_t pos_fins = pos;

    for (uint32_t k = 0; k < lm->qtd_slots; k++) /* os textos dos fins vem depois de todos os nomes */
        pos_fins += (uint32_t)(strlen(lm->nome[k]) + strlen(lm->arquivo[k])) + 2u;
    LmAnexar32(&b, LM_MAGIA);
    LmAnexar32(&b, lm->qtd_slots);
    for (uint32_t k = 0; k < lm->qtd_slots; k++) {
        LmAnexar32(&b, pos);
        pos += (uint32_t)strlen(lm->nome[k]) + 1u;
        LmAnexar32(&b, pos);
        pos += (uint32_t)strlen(lm->arquivo[k]) + 1u;
        LmAnexar32(&b, lm->fins[k] != NULL ? pos_fins : 0); /* 0 = a lib nao declara `ends` */
        pos_fins += lm->fins[k] != NULL ? (uint32_t)LmTamanhoFins(lm->fins[k]) : 0u;
    }
    for (uint32_t k = 0; k < lm->qtd_slots; k++) {
        LmAnexarTexto(&b, lm->nome[k]);
        LmAnexarTexto(&b, lm->arquivo[k]);
    }
    for (uint32_t k = 0; k < lm->qtd_slots; k++) {
        if (lm->fins[k] != NULL)
            BufAnexarN(&b, lm->fins[k], LmTamanhoFins(lm->fins[k]));
    }
    BufAnexarN(&((ObjSecao *)VecEm(&o->secoes, secao))->bytes, b.dados, b.tam);
    ObjSimboloDefinir(o, "forja_libmem_tabela", (int32_t)secao, 0);
    BufLiberar(&b);
}
