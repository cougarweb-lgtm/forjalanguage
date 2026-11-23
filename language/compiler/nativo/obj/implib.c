/* implib.c - leitor de import library: membro curto do MSVC e membro do dlltool/ld do GNU, reduzidos a (DLL, nome) (P19b do M3).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - G3 G0c: ImpComparar desempata por simbolo e tipo (ordem total).
 *            0.1.0 - M3 P19b: ImplibLer. O membro GNU de simbolo tem .idata$5 (slot) e .idata$6 (dica de 2 bytes + nome); o nome da DLL esta no .idata$7 do "tail"
 *                    (nao ha nome de DLL no membro do simbolo). O descritor do MSVC (nome da DLL no .idata$6, sem slot) cai fora das duas formas e e ignorado.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/obj/ar_ler.h"
#include "nativo/obj/implib.h"

#define IMPL_MAQUINA_AMD64 0x8664u
#define IMPL_MAQUINA_I386 0x14Cu
#define IMPL_TAM_CURTO 20u
#define IMPL_OFF_SIG2 2u
#define IMPL_SIG2 0xFFFFu
#define IMPL_OFF_VERSAO 4u
#define IMPL_OFF_MAQUINA_CURTO 6u
#define IMPL_OFF_TAM_DADOS 12u
#define IMPL_OFF_TIPOS 18u
#define IMPL_TIPO_MASCARA 3u
#define IMPL_NOME_DESLOCAMENTO 2u
#define IMPL_NOME_MASCARA 7u
#define IMPL_TIPO_CODIGO 0u
#define IMPL_NOME_ORDINAL 0u
#define IMPL_NOME_NOME 1u
#define IMPL_NOME_SEM_PREFIXO 2u
#define IMPL_NOME_SEM_DECORACAO 3u
#define IMPL_NOME_EXPORTAS 4u
#define IMPL_QTD_TEXTOS 3u
#define IMPL_TAM_COFF 20u
#define IMPL_OFF_NSECOES 2u
#define IMPL_OFF_TAM_OPCIONAL 16u
#define IMPL_TAM_SECAO 40u
#define IMPL_TAM_NOME_SECAO 8u
#define IMPL_OFF_SECAO_TAM 16u
#define IMPL_OFF_SECAO_PTR 20u
#define IMPL_TAM_DICA 2u

typedef struct {
    const uint8_t *p;
    size_t n;
} Faixa;

typedef struct {
    ImplibEntradas *saida;
    Diag *erro;
    const char *dll_gnu;  /* nome da DLL do "tail" GNU (arena da saida) */
    bool varias_dlls_gnu;
} Leitor;

static uint16_t ImpLe16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t ImpLe32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool ImpMalformado(Leitor *L, const char *motivo)
{
    return DiagRelatar(L->erro, X0105, 0, "import library malformada: %s", motivo);
}

/* So AMD64 passa; i386 e a causa conhecida (a mesma frase do PE de 32 bits, pe_ler.c). */
static bool ImpMaquinaOk(Leitor *L, uint16_t maquina)
{
    if (maquina == IMPL_MAQUINA_AMD64)
        return true;
    if (maquina == IMPL_MAQUINA_I386)
        return DiagRelatar(L->erro, X0105, 0, "biblioteca de 32 bits (import library i386); o backend nativo liga so x64");
    return DiagRelatar(L->erro, X0105, 0, "import library para a maquina 0x%x; o backend nativo liga so x64 (0x8664)", maquina);
}

static ImplibEntrada *ImpNovaEntrada(Leitor *L, const char *dll, const char *simbolo, const char *nome)
{
    Arena *a = &L->saida->textos;
    ImplibEntrada *e = VecEmpurrar(&L->saida->itens);
    e->dll = dll == NULL ? NULL : ArenaCopiarTexto(a, dll, strlen(dll));
    e->simbolo = ArenaCopiarTexto(a, simbolo, strlen(simbolo));
    e->nome = nome == simbolo ? e->simbolo : ArenaCopiarTexto(a, nome, strlen(nome));
    return e;
}

/* ---------- membro curto do MSVC ---------- */

/* Separa as ate 3 strings terminadas em NUL do corpo do membro (simbolo, DLL, nome de exportacao); devolve quantas achou, ou -1 se a ultima nao termina. */
static int ImpTextos(const char *ini, size_t tam, const char *textos[IMPL_QTD_TEXTOS])
{
    int qtd = 0;
    const char *fim = ini + tam;
    while (ini < fim && (unsigned)qtd < IMPL_QTD_TEXTOS) {
        const char *nul = memchr(ini, 0, (size_t)(fim - ini));
        if (nul == NULL)
            return -1;
        textos[qtd++] = ini;
        ini = nul + 1;
    }
    return qtd;
}

/* Sem o ? @ _ da frente (um so) e, em UNDECORATE, sem o primeiro @ e o que vem depois. */
static const char *ImpSemPrefixo(const char *simbolo, bool cortar_arroba, size_t *tam)
{
    const char *p = simbolo + (*simbolo == '?' || *simbolo == '@' || *simbolo == '_');
    const char *arroba = cortar_arroba ? strchr(p, '@') : NULL;
    *tam = arroba != NULL ? (size_t)(arroba - p) : strlen(p);
    return p;
}

/* Nome na tabela de exportacao da DLL conforme o NameType. NULL se o NameType e desconhecido ou falta a string do EXPORTAS. */
static const char *ImpNomeImportado(uint32_t tipo_nome, const char *simbolo, const char *extra, size_t *tam)
{
    *tam = strlen(simbolo);
    if (tipo_nome <= IMPL_NOME_NOME)
        return simbolo;
    if (tipo_nome == IMPL_NOME_EXPORTAS) {
        if (extra != NULL)
            *tam = strlen(extra);
        return extra;
    }
    if (tipo_nome == IMPL_NOME_SEM_PREFIXO || tipo_nome == IMPL_NOME_SEM_DECORACAO)
        return ImpSemPrefixo(simbolo, tipo_nome == IMPL_NOME_SEM_DECORACAO, tam);
    return NULL;
}

static ImplibTipo ImpTipoCurto(uint32_t tipo, uint32_t tipo_nome)
{
    if (tipo_nome == IMPL_NOME_ORDINAL)
        return IMPLIB_ORDINAL;
    return tipo == IMPL_TIPO_CODIGO ? IMPLIB_CODIGO : IMPLIB_DADO;
}

static bool ImpCurto(Leitor *L, const ArMembro *m)
{
    const uint8_t *b = m->dados;
    const char *textos[IMPL_QTD_TEXTOS] = {NULL, NULL, NULL};
    uint32_t tipos = ImpLe16(b + IMPL_OFF_TIPOS), tipo_nome = (tipos >> IMPL_NOME_DESLOCAMENTO) & IMPL_NOME_MASCARA;
    size_t tam_dados = ImpLe32(b + IMPL_OFF_TAM_DADOS), tam_nome = 0;
    if (!ImpMaquinaOk(L, ImpLe16(b + IMPL_OFF_MAQUINA_CURTO)))
        return false;
    if (tam_dados > m->tam - IMPL_TAM_CURTO || ImpTextos((const char *)b + IMPL_TAM_CURTO, tam_dados, textos) < 2)
        return ImpMalformado(L, "membro curto sem as strings do simbolo e da DLL");
    const char *nome = ImpNomeImportado(tipo_nome, textos[0], textos[2], &tam_nome);
    if (nome == NULL)
        return ImpMalformado(L, "NameType desconhecido ou EXPORTAS sem o nome de exportacao");
    ImplibEntrada *e = ImpNovaEntrada(L, textos[1], textos[0], textos[0]);
    e->nome = ArenaCopiarTexto(&L->saida->textos, nome, tam_nome);
    e->tipo = ImpTipoCurto(tipos & IMPL_TIPO_MASCARA, tipo_nome);
    return true;
}

/* ---------- membro do dlltool / ld ---------- */

/* A secao de nome exato (8 bytes) do objeto COFF do membro: seus dados gravados dentro do membro. Falso se nao ha, ou se a tabela ou os dados saem do membro. */
static bool ImpSecao(const ArMembro *m, const char *nome, Faixa *faixa)
{
    size_t tabela = IMPL_TAM_COFF + ImpLe16(m->dados + IMPL_OFF_TAM_OPCIONAL), qtd = ImpLe16(m->dados + IMPL_OFF_NSECOES);
    for (size_t i = 0; i < qtd; i++) {
        size_t off = tabela + i * IMPL_TAM_SECAO;
        if (off + IMPL_TAM_SECAO > m->tam)
            return false;
        if (memcmp(m->dados + off, nome, IMPL_TAM_NOME_SECAO) != 0)
            continue;
        size_t tam = ImpLe32(m->dados + off + IMPL_OFF_SECAO_TAM), ptr = ImpLe32(m->dados + off + IMPL_OFF_SECAO_PTR);
        faixa->p = m->dados + ptr;
        faixa->n = tam;
        return ptr <= m->tam && tam <= m->tam - ptr;
    }
    return false;
}

/* .idata$6 = dica (2 bytes) + nome terminado em NUL. */
static bool ImpGnuSimbolo(Leitor *L, const Faixa *s6)
{
    const char *nome = (const char *)s6->p + IMPL_TAM_DICA;
    if (memchr(nome, 0, s6->n - IMPL_TAM_DICA) == NULL)
        return ImpMalformado(L, "nome do simbolo sem NUL no .idata$6");
    ImpNovaEntrada(L, NULL, nome, nome)->tipo = IMPLIB_CODIGO;
    return true;
}

/* .idata$7 do "tail" = nome da DLL; duas DLLs diferentes na mesma biblioteca ficam fora do que este leitor reduz. */
static bool ImpGnuDll(Leitor *L, const Faixa *s7)
{
    const uint8_t *nul = memchr(s7->p, 0, s7->n);
    size_t tam = nul == NULL ? s7->n : (size_t)(nul - s7->p);
    if (L->dll_gnu == NULL)
        L->dll_gnu = ArenaCopiarTexto(&L->saida->textos, (const char *)s7->p, tam);
    else if (strlen(L->dll_gnu) != tam || memcmp(L->dll_gnu, s7->p, tam) != 0)
        L->varias_dlls_gnu = true;
    return true;
}

/* Membro de simbolo: slot no .idata$5 e dica + nome no .idata$6. */
static bool ImpEhSimboloGnu(const ArMembro *m, Faixa *s6)
{
    Faixa s5;
    return ImpSecao(m, ".idata$5", &s5) && s5.n > 0 && ImpSecao(m, ".idata$6", s6) && s6->n > IMPL_TAM_DICA;
}

/* "Tail": o .idata$7 guarda o nome da DLL (o do membro de simbolo e so um ponteiro zerado). */
static bool ImpEhCaudaGnu(const ArMembro *m, Faixa *s7)
{
    return ImpSecao(m, ".idata$7", s7) && s7->n > 0 && s7->p[0] != 0;
}

static bool ImpGnu(Leitor *L, const ArMembro *m)
{
    Faixa s6, s7;
    if (m->tam < IMPL_TAM_COFF)
        return true;
    bool simbolo = ImpEhSimboloGnu(m, &s6);
    if (!simbolo && !ImpEhCaudaGnu(m, &s7))
        return true;
    if (!ImpMaquinaOk(L, ImpLe16(m->dados)))
        return false;
    return simbolo ? ImpGnuSimbolo(L, &s6) : ImpGnuDll(L, &s7);
}

/* Os simbolos GNU nao carregam a DLL: ganham a do "tail". */
static bool ImpAtribuirDllGnu(Leitor *L)
{
    Vec *v = &L->saida->itens;
    for (size_t i = 0; i < v->n; i++) {
        ImplibEntrada *e = VecEm(v, i);
        if (e->dll != NULL)
            continue;
        if (L->dll_gnu == NULL || L->varias_dlls_gnu)
            return ImpMalformado(L, L->dll_gnu == NULL ? "import library GNU sem o nome da DLL (.idata$7 do tail)" : "import library GNU com mais de uma DLL");
        e->dll = L->dll_gnu;
    }
    return true;
}

static int ImpComparar(const void *a, const void *b)
{
    const ImplibEntrada *x = a, *y = b;
    int por_nome = strcmp(x->nome, y->nome);
    int por_dll = por_nome != 0 ? por_nome : strcmp(x->dll, y->dll);
    int por_simbolo = por_dll != 0 ? por_dll : strcmp(x->simbolo, y->simbolo);

    if (por_simbolo != 0) return por_simbolo;
    return x->tipo < y->tipo ? -1 : (x->tipo > y->tipo ? 1 : 0); /* desempate total: ImplibEntrada so tem estes 4 campos */
}

/* Sig1 0 e Sig2 0xFFFF abrem o cabecalho de import (e o de objeto anonimo). */
static bool ImpEhCurto(const ArMembro *m)
{
    return m->tam >= IMPL_TAM_CURTO && ImpLe16(m->dados) == 0 && ImpLe16(m->dados + IMPL_OFF_SIG2) == IMPL_SIG2;
}

/* Versao diferente de 0 e objeto anonimo (/GL): nao e import, fica de fora. */
static bool ImpMembro(Leitor *L, const ArMembro *m)
{
    if (!ImpEhCurto(m))
        return ImpGnu(L, m);
    return ImpLe16(m->dados + IMPL_OFF_VERSAO) != 0 || ImpCurto(L, m);
}

static bool ImpLerMembros(Leitor *L, const uint8_t *bytes, size_t n)
{
    ArIterador it;
    ArMembro m;
    if (!ArIniciar(&it, bytes, n, L->erro))
        return false;
    while (ArProximo(&it, &m, L->erro)) {
        if (m.tipo == AR_OBJETO && !ImpMembro(L, &m))
            return false;
    }
    return !L->erro->tem;
}

bool ImplibLer(const uint8_t *bytes, size_t n, ImplibEntradas *saida, Diag *erro)
{
    Leitor L = {.saida = saida, .erro = erro};
    saida->textos = (Arena){0};
    VecIniciar(&saida->itens, sizeof(ImplibEntrada));
    if (!ImpLerMembros(&L, bytes, n) || !ImpAtribuirDllGnu(&L))
        return false;
    qsort(saida->itens.dados, saida->itens.n, sizeof(ImplibEntrada), ImpComparar);
    return true;
}

void ImplibLiberar(ImplibEntradas *saida)
{
    VecLiberar(&saida->itens);
    ArenaLiberar(&saida->textos);
}
