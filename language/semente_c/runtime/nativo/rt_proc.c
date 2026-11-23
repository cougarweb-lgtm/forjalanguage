/*
 * rt_proc.c - std:proc no runtime nativo: argumentos, variavel de ambiente e saida com codigo (M5 P26b).
 *
 * Versao: 0.2.1
 * Changelog:
 *   0.2.1 - rt_proc_anexar_char extraido (corte do buffer em um so lugar): rt_proc_anexar_i64 volta a 5 decisoes ou menos; mesmo texto de saida.
 *   0.2.0 - M5 P26b: sai do rt_win.o e vai para a unidade do rt_win_ryu.o (rt_ryu.c): mais codigo no rt_win.o mudava os bytes de exes release sem std:proc (inlining do gcc). O que
 *           era do rt_nativo.c (RtCausa, rt_fmt) agora e local: a mensagem do R0012 monta o numero aqui. Os simbolos do rt_win.o que usa sao globais (forja_falha, forja_falha_alocacao,
 *           rt_saida_descarregar, sys_*).
 *   0.1.1 - textos em 'static const char[]' (secao propria): sem literais soltos o .rdata dos exes que nao importam std:proc fica byte a byte igual (D-M5-11).
 *   0.1.0 - M5 P26b: forja_proc_argumentos, forja_proc_ambiente e forja_proc_sair, os simbolos que std\proc.fj declara em 'foreign' sem link (mesmo padrao do std:mem).
 *
 * Posse: os argumentos e os valores de ambiente sao EMPRESTADOS ao programa e vivem ate o fim do processo (a Forja nao da delete); vem do heap do sistema
 * (sys_heap_*), por isso o rastreador de vazamento do dev (R0009) nao os ve. Cada forja_proc_ambiente guarda uma copia nova: chame uma vez e guarde o resultado.
 * Codigo de saida fora de 0..255 e R0012 (exit 101): o Windows truncaria em silencio e o Linux so guarda 8 bits.
 * Incluido por rt_ryu.c depois de rt_proc_win.c; nao compila sozinho.
 */
#include <stddef.h>
#include <stdint.h>
#include "rt_sys.h"
#include "forja_rt_nativo.h"

/* Do rt_win.o (rt_saida.c): descarrega o stdout. */
void rt_saida_descarregar(void);

/* Textos como 'static const char[]', nunca literais soltos: com -fdata-sections cada um vai para a propria secao .rdata$nome e o ligador o descarta em quem nao importa
 * std:proc; os literais do C vao todos para a mesma .rdata e mudariam os bytes de todo exe (D-M5-11). */
static const char rt_proc_r0012[] = "R0012";
static const char rt_proc_arquivo[] = "std:proc";
static const char rt_proc_sem_memoria_args[] = "sem memoria para os argumentos do processo em bytes";
static const char rt_proc_sem_memoria_env[] = "sem memoria para o valor da variavel de ambiente em bytes";
static const char rt_proc_causa_inicio[] = "exit com codigo ";
static const char rt_proc_causa_final[] = " fora de 0..255";

#define RT_PROC_CAUSA_BYTES 160

/* Fatia de str dos argumentos, montada na 1a chamada. */
static forja_str *rt_proc_args;
static int64_t rt_proc_nargs;
static bool rt_proc_prontos;

static const forja_str rt_proc_nenhum = {NULL, 0};

static int64_t rt_proc_tamanho(const char *s)
{
    int64_t n = 0;

    while (s[n] != '\0')
        n++;
    return n;
}

static void rt_proc_montar(void)
{
    char **argv = NULL;
    int64_t argc = 0;
    forja_str *lista = NULL;

    sys_argumentos(&argc, &argv);
    if (argc > 0)
        lista = sys_heap_alocar(argc * (int64_t)sizeof(forja_str));
    if (argc > 0 && lista == NULL)
        forja_falha_alocacao(rt_proc_sem_memoria_args, argc * (int64_t)sizeof(forja_str), rt_proc_arquivo, 0);
    for (int64_t i = 0; i < argc; i++) {
        lista[i].ptr = argv[i];
        lista[i].len = rt_proc_tamanho(argv[i]);
    }
    rt_proc_args = lista;
    rt_proc_nargs = argc;
    rt_proc_prontos = true;
}

void *forja_proc_argumentos(int64_t *quantos)
{
    if (!rt_proc_prontos)
        rt_proc_montar();
    *quantos = rt_proc_nargs;
    return rt_proc_nargs > 0 ? (void *)rt_proc_args : (void *)&rt_proc_nenhum;
}

const char *forja_proc_ambiente(const char *nome)
{
    int64_t tam = sys_ambiente_utf8(nome, NULL, 0);
    char *copia;

    if (tam < 0)
        return NULL;
    copia = sys_heap_alocar(tam + 1);
    if (copia == NULL)
        forja_falha_alocacao(rt_proc_sem_memoria_env, tam + 1, rt_proc_arquivo, 0);
    sys_ambiente_utf8(nome, copia, tam + 1);
    return copia;
}

/* Acrescenta um caractere a buf (no maximo RT_PROC_CAUSA_BYTES - 1 bytes, o resto e cortado) e devolve o novo uso. */
static int64_t rt_proc_anexar_char(char *buf, int64_t usado, char c)
{
    if (usado < RT_PROC_CAUSA_BYTES - 1)
        buf[usado++] = c;
    return usado;
}

static int64_t rt_proc_anexar(char *buf, int64_t usado, const char *texto)
{
    while (*texto != '\0')
        usado = rt_proc_anexar_char(buf, usado, *texto++);
    return usado;
}

/* Acrescenta o inteiro em decimal (com '-' se negativo), como o %lld do backend C. */
static int64_t rt_proc_anexar_i64(char *buf, int64_t usado, int64_t valor)
{
    char digitos[24];
    int n = 0;
    uint64_t u = valor < 0 ? 0 - (uint64_t)valor : (uint64_t)valor;

    do {
        digitos[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u != 0);
    if (valor < 0)
        usado = rt_proc_anexar_char(buf, usado, '-');
    while (n > 0)
        usado = rt_proc_anexar_char(buf, usado, digitos[--n]);
    return usado;
}

/* exit(code): descarrega o stdout e sai SEM rodar os handlers de saida (nao ha relatorio R0009). */
void forja_proc_sair(int64_t codigo)
{
    char causa[RT_PROC_CAUSA_BYTES];
    int64_t usado = 0;

    if (codigo < 0 || codigo > 255) {
        usado = rt_proc_anexar(causa, usado, rt_proc_causa_inicio);
        usado = rt_proc_anexar_i64(causa, usado, codigo);
        usado = rt_proc_anexar(causa, usado, rt_proc_causa_final);
        causa[usado] = '\0';
        forja_falha(rt_proc_r0012, causa, rt_proc_arquivo, 0);
    }
    rt_saida_descarregar();
    sys_sair((int)codigo);
}
