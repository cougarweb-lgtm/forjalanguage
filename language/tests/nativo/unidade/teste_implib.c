/* teste_implib.c - verificacoes de unidade do leitor ar e da import library (ImplibVerificacoes, chamada pelo teste_nativo): 5 verificacoes do P19b.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19b: ar "//" e nomes longos; membro curto do MSVC (NAME, NOPREFIX, UNDECORATE, EXPORTAS; DATA, ORDINAL e i386); membro GNU (head e tail, nome da DLL no .idata$7);
 *                    import library em arquivo com indice de simbolos maior que 64 KiB (tipo pelo arquivo e BibliotecasResolver com o X0105 de dado).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/obj/ar_ler.h"
#include "nativo/obj/biblioteca.h"
#include "nativo/obj/bibliotecas.h"
#include "nativo/obj/implib.h"

#define MAQUINA_AMD64 0x8664u
#define MAQUINA_I386 0x14Cu
#define TAM_CABECALHO_AR 60u
#define TAM_COFF 20u
#define TAM_SECAO 40u
#define TAM_ARQUIVO_GRANDE 70000u
#define TAM_CAMINHO 600u

typedef struct {
    int *total;
    int *falhas;
} Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (implib) linha %d: %s\n", linha, texto);
}

#define IMPLIB_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* ---------- montagem de ar e de membros, na mao ---------- */

static void Anexar16(Buf *b, uint32_t v)
{
    const char dois[2] = {(char)(v & 0xFFu), (char)((v >> 8) & 0xFFu)};
    BufAnexarN(b, dois, sizeof dois);
}

static void Anexar32(Buf *b, uint32_t v)
{
    Anexar16(b, v & 0xFFFFu);
    Anexar16(b, v >> 16);
}

/* Cabecalho de 60 bytes + corpo + o byte de preenchimento dos membros de tamanho impar. */
static void AnexarMembroAr(Buf *ar, const char *nome, const Buf *corpo)
{
    char cab[TAM_CABECALHO_AR + 1];
    snprintf(cab, sizeof cab, "%-16s%-12s%-6s%-6s%-8s%-10zu`\n", nome, "0", "0", "0", "100644", corpo->tam);
    BufAnexarN(ar, cab, TAM_CABECALHO_AR);
    if (corpo->tam > 0)
        BufAnexarN(ar, corpo->dados, corpo->tam);
    if ((corpo->tam & 1u) != 0)
        BufAnexarN(ar, "\n", 1);
}

static void AnexarMembroTexto(Buf *ar, const char *nome, const char *dados, size_t tam)
{
    Buf corpo = {0};
    if (tam > 0)
        BufAnexarN(&corpo, dados, tam);
    AnexarMembroAr(ar, nome, &corpo);
    BufLiberar(&corpo);
}

static void IniciarAr(Buf *ar)
{
    BufAnexar(ar, "!<arch>\n");
}

/* Membro curto de import: cabecalho de 20 bytes (Sig1 0, Sig2 0xFFFF, Version 0) e as strings (simbolo, DLL e, no EXPORTAS, o nome de exportacao). */
static void AnexarMembroCurto(Buf *ar, const char *nome, uint16_t maquina, uint32_t tipos, const char *textos, size_t tam_textos)
{
    Buf corpo = {0};
    Anexar16(&corpo, 0);
    Anexar16(&corpo, 0xFFFFu);
    Anexar16(&corpo, 0);
    Anexar16(&corpo, maquina);
    Anexar32(&corpo, 0);
    Anexar32(&corpo, (uint32_t)tam_textos);
    Anexar16(&corpo, 0);
    Anexar16(&corpo, tipos);
    BufAnexarN(&corpo, textos, tam_textos);
    AnexarMembroAr(ar, nome, &corpo);
    BufLiberar(&corpo);
}

typedef struct {
    const char *nome;
    const char *dados;
    size_t tam;
} SecaoTeste;

/* Objeto COFF so com cabecalho, tabela de secoes e os dados delas (sem simbolos nem relocacoes: o leitor de import library nao os le). */
static void AnexarMembroCoff(Buf *ar, const char *nome_ar, uint16_t maquina, const SecaoTeste *secoes, uint32_t qtd)
{
    Buf corpo = {0};
    size_t ptr = TAM_COFF + (size_t)TAM_SECAO * qtd;
    Anexar16(&corpo, maquina);
    Anexar16(&corpo, qtd);
    for (int k = 0; k < 4; k++)  /* data e hora, tabela de simbolos, quantos simbolos, cabecalho opcional e caracteristicas */
        Anexar32(&corpo, 0);
    for (uint32_t i = 0; i < qtd; i++) {
        char nome[8] = {0};
        memcpy(nome, secoes[i].nome, strlen(secoes[i].nome) < sizeof nome ? strlen(secoes[i].nome) : sizeof nome);
        BufAnexarN(&corpo, nome, sizeof nome);
        Anexar32(&corpo, 0);
        Anexar32(&corpo, 0);
        Anexar32(&corpo, (uint32_t)secoes[i].tam);
        Anexar32(&corpo, secoes[i].tam > 0 ? (uint32_t)ptr : 0);
        for (int k = 0; k < 4; k++)  /* relocacoes, linhas, contagens dos dois e caracteristicas */
            Anexar32(&corpo, 0);
        ptr += secoes[i].tam;
    }
    for (uint32_t i = 0; i < qtd; i++) {
        if (secoes[i].tam > 0)
            BufAnexarN(&corpo, secoes[i].dados, secoes[i].tam);
    }
    AnexarMembroAr(ar, nome_ar, &corpo);
    BufLiberar(&corpo);
}

/* ---------- consultas ---------- */

static const ImplibEntrada *AcharEntrada(const ImplibEntradas *e, const char *simbolo)
{
    for (size_t i = 0; i < e->itens.n; i++) {
        const ImplibEntrada *x = VecEm(&e->itens, i);
        if (strcmp(x->simbolo, simbolo) == 0)
            return x;
    }
    return NULL;
}

static bool EntradaEh(const ImplibEntradas *e, const char *simbolo, const char *nome, const char *dll, ImplibTipo tipo)
{
    const ImplibEntrada *x = AcharEntrada(e, simbolo);
    return x != NULL && strcmp(x->nome, nome) == 0 && strcmp(x->dll, dll) == 0 && x->tipo == tipo;
}

/* Le o ar e diz se deu X0105 com a frase na causa. */
static bool RecusaComX0105(const Buf *ar, const char *frase)
{
    ImplibEntradas e;
    Diag erro = {0};
    bool lido = ImplibLer((const uint8_t *)ar->dados, ar->tam, &e, &erro);
    ImplibLiberar(&e);
    return !lido && erro.tem && erro.codigo == X0105 && strstr(erro.causa, frase) != NULL;
}

/* ---------- 1: membro curto, os 4 NameType com nome ---------- */

static bool MsvcNomes(void)
{
    Buf ar = {0};
    ImplibEntradas e;
    Diag erro = {0};
    bool ok;
    IniciarAr(&ar);
    AnexarMembroTexto(&ar, "/", "", 0);
    AnexarMembroCurto(&ar, "a.dll/", MAQUINA_AMD64, 1u << 2, "foo\0a.dll\0", 10);
    AnexarMembroCurto(&ar, "a.dll/", MAQUINA_AMD64, 2u << 2, "_bar\0a.dll\0", 11);
    AnexarMembroCurto(&ar, "b.dll/", MAQUINA_AMD64, 3u << 2, "?baz@@YAXXZ\0b.dll\0", 18);
    AnexarMembroCurto(&ar, "b.dll/", MAQUINA_AMD64, 4u << 2, "qux\0b.dll\0Qux2\0", 15);
    ok = ImplibLer((const uint8_t *)ar.dados, ar.tam, &e, &erro) && e.itens.n == 4;
    ok = ok && EntradaEh(&e, "foo", "foo", "a.dll", IMPLIB_CODIGO) && EntradaEh(&e, "_bar", "bar", "a.dll", IMPLIB_CODIGO);
    ok = ok && EntradaEh(&e, "?baz@@YAXXZ", "baz", "b.dll", IMPLIB_CODIGO) && EntradaEh(&e, "qux", "Qux2", "b.dll", IMPLIB_CODIGO);
    ok = ok && strcmp(((const ImplibEntrada *)VecEm(&e.itens, 0))->nome, "Qux2") == 0;  /* ordenadas por nome */
    ImplibLiberar(&e);
    BufLiberar(&ar);
    return ok;
}

/* ---------- 2: DATA e ORDINAL ficam marcados; i386 e X0105 ---------- */

static bool MsvcDadoOrdinal32Bits(void)
{
    Buf ar = {0}, ar32 = {0};
    ImplibEntradas e;
    Diag erro = {0};
    bool ok;
    IniciarAr(&ar);
    AnexarMembroCurto(&ar, "a.dll/", MAQUINA_AMD64, 1u | (1u << 2), "dado\0a.dll\0", 11);
    AnexarMembroCurto(&ar, "a.dll/", MAQUINA_AMD64, 0, "ord\0a.dll\0", 10);
    AnexarMembroCurto(&ar, "a.dll/", MAQUINA_AMD64, 1u << 2, "fun\0a.dll\0", 10);
    ok = ImplibLer((const uint8_t *)ar.dados, ar.tam, &e, &erro) && e.itens.n == 3;
    ok = ok && EntradaEh(&e, "dado", "dado", "a.dll", IMPLIB_DADO) && EntradaEh(&e, "ord", "ord", "a.dll", IMPLIB_ORDINAL) && EntradaEh(&e, "fun", "fun", "a.dll", IMPLIB_CODIGO);
    ImplibLiberar(&e);
    IniciarAr(&ar32);
    AnexarMembroCurto(&ar32, "a.dll/", MAQUINA_I386, 1u << 2, "_fun\0a.dll\0", 11);
    ok = ok && RecusaComX0105(&ar32, "32 bits");
    BufLiberar(&ar);
    BufLiberar(&ar32);
    return ok;
}

/* ---------- 3: membro GNU, head e tail ---------- */

static size_t HintENome(char *dest, const char *nome)
{
    dest[0] = 0x11;
    dest[1] = 0;
    memcpy(dest + 2, nome, strlen(nome) + 1);
    return 2 + strlen(nome) + 1;
}

/* Membro de simbolo do dlltool: .text (stub), .idata$7/.idata$5/.idata$4 (slots) e .idata$6 (dica + nome). */
static void AnexarSimboloGnu(Buf *ar, const char *nome_ar, uint16_t maquina, const char *simbolo)
{
    static const char zeros[8] = {0};
    char hint[64];
    SecaoTeste s[] = {{".text", zeros, 8}, {".idata$7", zeros, 4}, {".idata$5", zeros, 8}, {".idata$4", zeros, 8}, {".idata$6", hint, 0}};
    s[4].tam = HintENome(hint, simbolo);
    AnexarMembroCoff(ar, nome_ar, maquina, s, 5);
}

/* "head" (so .idata$2, nome de DLL nenhum) e "tail" (.idata$7 = nome da DLL), como o dlltool grava. */
static void AnexarHeadETailGnu(Buf *ar, const char *dll)
{
    static const char zeros[20] = {0};
    SecaoTeste head[] = {{".idata$2", zeros, 20}, {".idata$5", zeros, 0}, {".idata$4", zeros, 0}};
    SecaoTeste tail[] = {{".idata$4", zeros, 8}, {".idata$5", zeros, 8}, {".idata$7", dll, strlen(dll) + 1}};
    AnexarMembroCoff(ar, "head.o/", MAQUINA_AMD64, head, 3);
    AnexarMembroCoff(ar, "tail.o/", MAQUINA_AMD64, tail, 3);
}

static bool GnuHeadETail(void)
{
    Buf ar = {0}, sem_tail = {0}, i386 = {0};
    ImplibEntradas e;
    Diag erro = {0};
    bool ok;
    IniciarAr(&ar);
    AnexarMembroTexto(&ar, "/", "", 0);
    AnexarHeadETailGnu(&ar, "meu.dll");
    AnexarSimboloGnu(&ar, "s1.o/", MAQUINA_AMD64, "fx_b");
    AnexarSimboloGnu(&ar, "s2.o/", MAQUINA_AMD64, "fx_a");
    ok = ImplibLer((const uint8_t *)ar.dados, ar.tam, &e, &erro) && e.itens.n == 2;
    ok = ok && EntradaEh(&e, "fx_a", "fx_a", "meu.dll", IMPLIB_CODIGO) && EntradaEh(&e, "fx_b", "fx_b", "meu.dll", IMPLIB_CODIGO);
    ImplibLiberar(&e);
    IniciarAr(&sem_tail);
    AnexarSimboloGnu(&sem_tail, "s1.o/", MAQUINA_AMD64, "fx_a");
    ok = ok && RecusaComX0105(&sem_tail, "sem o nome da DLL");
    IniciarAr(&i386);
    AnexarHeadETailGnu(&i386, "meu.dll");
    AnexarSimboloGnu(&i386, "s1.o/", MAQUINA_I386, "fx_a");
    ok = ok && RecusaComX0105(&i386, "32 bits");
    BufLiberar(&ar);
    BufLiberar(&sem_tail);
    BufLiberar(&i386);
    return ok;
}

/* ---------- 4: ar, indices, "//" e nomes longos ---------- */

static bool NomeDoMembroEh(const ArMembro *m, const char *nome)
{
    return m->nome_tam == strlen(nome) && memcmp(m->nome, nome, m->nome_tam) == 0;
}

static bool ArNomesLongos(void)
{
    static const char tabela[] = "um_nome_bem_longo.o/\noutro_nome_ainda_maior.o/\n";
    Buf ar = {0};
    ArIterador it;
    ArMembro m;
    Diag erro = {0};
    bool ok;
    IniciarAr(&ar);
    AnexarMembroTexto(&ar, "/", "abcd", 4);
    AnexarMembroTexto(&ar, "/", "efg", 3);
    AnexarMembroTexto(&ar, "//", tabela, sizeof tabela - 1);
    AnexarMembroTexto(&ar, "/0", "x", 1);
    AnexarMembroTexto(&ar, "/21", "yz", 2);
    AnexarMembroTexto(&ar, "curto.o/", "w", 1);
    ok = ArIniciar(&it, (const uint8_t *)ar.dados, ar.tam, &erro);
    ok = ok && ArProximo(&it, &m, &erro) && m.tipo == AR_INDICE && m.tam == 4;
    ok = ok && ArProximo(&it, &m, &erro) && m.tipo == AR_INDICE && m.tam == 3;
    ok = ok && ArProximo(&it, &m, &erro) && m.tipo == AR_NOMES && m.tam == sizeof tabela - 1;
    ok = ok && ArProximo(&it, &m, &erro) && m.tipo == AR_OBJETO && NomeDoMembroEh(&m, "um_nome_bem_longo.o");
    ok = ok && ArProximo(&it, &m, &erro) && m.tipo == AR_OBJETO && NomeDoMembroEh(&m, "outro_nome_ainda_maior.o") && m.tam == 2;
    ok = ok && ArProximo(&it, &m, &erro) && NomeDoMembroEh(&m, "curto.o") && !ArProximo(&it, &m, &erro) && !erro.tem;
    ar.tam -= 3;  /* corta o corpo e 1 byte do cabecalho do ultimo membro: cabecalho truncado */
    ok = ok && ArIniciar(&it, (const uint8_t *)ar.dados, ar.tam, &erro);
    for (int k = 0; ok && k < 8 && ArProximo(&it, &m, &erro); k++) {
        ok = ok && !erro.tem;
    }
    ok = ok && erro.tem && erro.codigo == X0105;
    BufLiberar(&ar);
    return ok;
}

/* ---------- 5: import library em arquivo, indice de 70 KB na frente ---------- */

static bool GravarArquivo(const char *pasta, const char *nome, const Buf *dados)
{
    char caminho[TAM_CAMINHO];
    snprintf(caminho, sizeof caminho, "%s\\%s", pasta, nome);
    FILE *f = fopen(caminho, "wb");
    if (f == NULL)
        return false;
    bool ok = fwrite(dados->dados, 1, dados->tam, f) == dados->tam;
    fclose(f);
    return ok;
}

static void MontarLibGrande(Buf *ar)
{
    char *indice = calloc(TAM_ARQUIVO_GRANDE, 1);
    IniciarAr(ar);
    AnexarMembroTexto(ar, "/", indice, TAM_ARQUIVO_GRANDE);
    AnexarMembroTexto(ar, "/", indice, 8);
    AnexarMembroCurto(ar, "mylib.dll/", MAQUINA_AMD64, 1u << 2, "mfunc\0mylib.dll\0", 16);
    AnexarMembroCurto(ar, "mylib.dll/", MAQUINA_AMD64, 1u | (1u << 2), "mdado\0mylib.dll\0", 16);
    free(indice);
}

/* O tipo vem dos membros lidos do arquivo (o 1o objeto esta depois de 70 KB); a funcao resolve para a DLL de dentro da .lib e o dado vira X0105. */
static bool ImplibEmArquivo(const char *pasta)
{
    Buf ar = {0};
    BibliotecaAchada achada;
    const char *pastas[] = {pasta};
    BibliotecaBusca busca = {pastas, 1, false, NULL};
    const char *links[] = {"forja_p19b_t"}, *func[] = {"mfunc"}, *dado[] = {"mdado"};
    BibliotecasPedido pedido = {.busca = &busca, .links = links, .qtd_links = 1, .simbolos = func, .qtd_simbolos = 1};
    BibliotecasResolvidas r = {0}, r_dado = {0};
    Diag erro = {0}, erro_dado = {0};
    bool ok;
    MontarLibGrande(&ar);
    ok = GravarArquivo(pasta, "forja_p19b_t.lib", &ar) && BibliotecaAchar(&busca, "forja_p19b_t", &achada) && achada.tipo == BIB_IMPORT_MSVC;
    BibliotecaLiberar(&achada);
    ok = ok && BibliotecasResolver(&pedido, &r, &erro) && r.qtd_tabelas == 1 && strcmp(r.tabelas[0].dll, "mylib.dll") == 0 && strcmp(r.tabelas[0].simbolos[0], "mfunc") == 0;
    pedido.simbolos = dado;
    ok = ok && !BibliotecasResolver(&pedido, &r_dado, &erro_dado) && erro_dado.codigo == X0105 && strstr(erro_dado.causa, "dado exportado") != NULL;
    BibliotecasLiberar(&r);
    BibliotecasLiberar(&r_dado);
    BufLiberar(&ar);
    {
        char caminho[TAM_CAMINHO];
        snprintf(caminho, sizeof caminho, "%s\\forja_p19b_t.lib", pasta);
        remove(caminho);
    }
    return ok;
}

void ImplibVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};
    const char *temporaria = getenv("TEMP");
    const char *pasta = temporaria != NULL ? temporaria : ".";

    IMPLIB_VERIFICA(&c, MsvcNomes());
    IMPLIB_VERIFICA(&c, MsvcDadoOrdinal32Bits());
    IMPLIB_VERIFICA(&c, GnuHeadETail());
    IMPLIB_VERIFICA(&c, ArNomesLongos());
    IMPLIB_VERIFICA(&c, ImplibEmArquivo(pasta));
}
