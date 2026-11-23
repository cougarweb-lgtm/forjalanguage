/*
 * teste_rt.c - autoteste do runtime forja_rt.h 0.10.2 (Linguagem v1.11 L16b: + vazamento por teste e queda por teste, 2; Linguagem v1.11 L16a: + forja test --filtro e --json, 2; Linguagem v1.9 P9: + map, 3; v1.9 P8: + rt_soa_ok, 1; v1.8 P5b: + Arena, Pool e lista em Arena, 3; v1.8 P5a: + rastreador de vazamento R0009, 2; v1.7: + texto formatado forja_fmt_*, 3; v1.6: + alocacao heap, lista e frame, 3; v1.3: + str, R0005 e forja_nao_nulo, 8 verificacoes; v1.4 P3: + FORJA_ROTULO_C, 1; v1.5: + forja_indice e forja_fatia_confere, 1).
 * Verificacao: rodar_teste_rt.bat (compila e roda em DEV e em release).
 * Manual: gcc -std=c11 -Wall -Wextra -Werror -D__USE_MINGW_ANSI_STDIO=1 [-DFORJA_DEV] teste_rt.c
 * Ordem de execucao (o bat faz isso): teste_rt --gerar-print; teste_rt --gerar-runner; teste_rt.
 *   --gerar-print e --gerar-runner redirecionam o stdout para arquivo em processo proprio,
 *   assim nao precisam restaurar o console (freopen("CON") nao existe em todo ambiente).
 *   Sem argumento, confere as capturas, os testes e os casos da v1.2 (cada caso em processo proprio).
 *   --caso NOME roda um caso no processo atual: o verificador o chama assim, com stdout e stderr em arquivo.
 * Saida: "OK: N verificacoes" e exit 0; qualquer falha => exit 1.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../runtime/forja_rt.h"

static int verificacoes = 0;
static int falhas = 0;
static char saida_runner[8192];
static char saida_json[512];

/* Conta uma verificacao; cond falsa registra a falha e o programa segue. */
#define CHECA(cond) \
    do { \
        verificacoes++; \
        if (!(cond)) { \
            falhas++; \
            fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

/* Le o arquivo inteiro para buffer (terminado em NUL); devolve bytes lidos ou -1. */
static long ler_arquivo(const char *caminho, char *buffer, size_t cap)
{
    FILE *arquivo = fopen(caminho, "rb");
    size_t lidos;

    if (arquivo == NULL)
        return -1;
    lidos = fread(buffer, 1, cap - 1, arquivo);
    buffer[lidos] = '\0';
    fclose(arquivo);
    return (long)lidos;
}

/* --gerar-print: grava em arquivo binario o que os print_* escrevem. Processo proprio. */
static void gerar_print(void)
{
    freopen("teste_rt_print.tmp", "wb", stdout);
    forja_print_i32(5);
    forja_print_i32(-7);
    forja_print_bool(true);
    forja_print_bool(false);
    forja_print_texto("Olá");
    forja_print_i64(-9000000000LL);
    forja_print_f32(2.25f);
    fflush(stdout);
}

/* print_*: confere os bytes exatos gravados por gerar_print. */
static void verificar_print(void)
{
    const char esperado[] = "5\n-7\ntrue\nfalse\nOlá\n-9000000000\n2.25\n";
    char bytes[256] = {0};
    long lidos = ler_arquivo("teste_rt_print.tmp", bytes, sizeof(bytes));

    CHECA(lidos == (long)(sizeof(esperado) - 1));
    CHECA(memcmp(bytes, esperado, sizeof(esperado) - 1) == 0);
    remove("teste_rt_print.tmp");
}

/* Aritmetica normal, e divisao/resto por zero que devolvem 0 nos dois modos. */
static void testar_aritmetica(void)
{
    CHECA(forja_add_i32(2, 3, __FILE__, __LINE__) == 5);
    CHECA(forja_sub_i32(2, 3, __FILE__, __LINE__) == -1);
    CHECA(forja_mul_i32(-4, 6, __FILE__, __LINE__) == -24);
    CHECA(forja_div_i32(-7, 2, __FILE__, __LINE__) == -3);
    CHECA(forja_rem_i32(-7, 2, __FILE__, __LINE__) == -1);
    CHECA(forja_add_i64(9000000000LL, 1LL, __FILE__, __LINE__) == 9000000001LL);
    CHECA(forja_sub_i64(0LL, 9000000000LL, __FILE__, __LINE__) == -9000000000LL);
    CHECA(forja_mul_i64(3000000000LL, 3LL, __FILE__, __LINE__) == 9000000000LL);
    CHECA(forja_div_i64(-9000000000LL, 1000LL, __FILE__, __LINE__) == -9000000LL);
    CHECA(forja_rem_i64(9000000001LL, 1000LL, __FILE__, __LINE__) == 1LL);
    CHECA(forja_div_i32(7, 0, __FILE__, __LINE__) == 0);
    CHECA(forja_rem_i32(7, 0, __FILE__, __LINE__) == 0);
    CHECA(forja_div_i64(7, 0, __FILE__, __LINE__) == 0);
    CHECA(forja_rem_i64(7, 0, __FILE__, __LINE__) == 0);
}

/* Faixa dentro do intervalo devolve o valor, nas formas inclusiva e exclusiva. */
static void testar_faixa(void)
{
    CHECA(forja_faixa(10, 1, 10, false, "idade", __FILE__, __LINE__) == 10);
    CHECA(forja_faixa(1, 1, 10, false, "idade", __FILE__, __LINE__) == 1);
    CHECA(forja_faixa(9, 1, 10, true, "idade", __FILE__, __LINE__) == 9);
}

#if !defined(FORJA_DEV)
/* Release: estouro so da wrap, e INT_MIN/-1 nao chama forja_falha nem da trap (SIGFPE). */
static void testar_release_sem_falha(void)
{
    CHECA(forja_add_i32(INT32_MAX, 1, __FILE__, __LINE__) == INT32_MIN);
    CHECA(forja_add_i64(INT64_MAX, 1, __FILE__, __LINE__) == INT64_MIN);
    CHECA(forja_div_i32(INT32_MIN, -1, __FILE__, __LINE__) == INT32_MIN);
    CHECA(forja_rem_i32(INT32_MIN, -1, __FILE__, __LINE__) == 0);
    CHECA(forja_div_i64(INT64_MIN, -1, __FILE__, __LINE__) == INT64_MIN);
    CHECA(forja_rem_i64(INT64_MIN, -1, __FILE__, __LINE__) == 0);
}
#endif

/* Testes que passam. */
static void t_passa(void)
{
    forja_expect(1 + 1 == 2, "1 + 1 == 2", __FILE__, __LINE__);
}

static void t_faixa_dentro(void)
{
    forja_expect(forja_faixa(10, 1, 10, false, "idade", __FILE__, __LINE__) == 10,
                 "faixa(10, 1..=10) == 10", __FILE__, __LINE__);
}

static void t_divisor_zero(void)
{
    forja_expect(forja_div_i32(7, 0, __FILE__, __LINE__) == 0, "7 / 0 == 0 (i32)", __FILE__, __LINE__);
    forja_expect(forja_rem_i32(7, 0, __FILE__, __LINE__) == 0, "7 % 0 == 0 (i32)", __FILE__, __LINE__);
    forja_expect(forja_div_i64(7, 0, __FILE__, __LINE__) == 0, "7 / 0 == 0 (i64)", __FILE__, __LINE__);
    forja_expect(forja_rem_i64(7, 0, __FILE__, __LINE__) == 0, "7 % 0 == 0 (i64)", __FILE__, __LINE__);
}

/* Marca na saida que o teste seguinte ao fracasso ainda rodou. */
static void t_depois(void)
{
    printf("marca depois rodou\n");
}

/* expect falso dentro de teste registra a falha e o teste segue ate o fim. */
static void t_expect_nao_aborta(void)
{
    forja_expect(false, "1 == 2", __FILE__, __LINE__);
    printf("marca expect seguiu\n");
}

static void t_assert_falso(void)
{
    forja_assert(false, "assert de teste", __FILE__, __LINE__);
}

static void t_panic(void)
{
    forja_panic("panic de teste", __FILE__, __LINE__);
}

/* forja_falha com aspas, barra e tab na causa: a linha JSON em stderr vai para arquivo. */
static void t_json(void)
{
    freopen("teste_rt_json.tmp", "wb", stderr);
    forja_falha(FORJA_R0001, "valor \"a\" fora\tcaminho \\ x", "arq\"y\\z.fj", 7);
}

#if defined(FORJA_DEV)
/* Falhas deliberadas que so existem em DEV. */
static void t_estouro_i32(void)
{
    forja_add_i32(INT32_MAX, 1, __FILE__, __LINE__);
}

static void t_estouro_i64(void)
{
    forja_mul_i64(INT64_MAX, 2, __FILE__, __LINE__);
}

static void t_intmin_div(void)
{
    forja_div_i32(INT32_MIN, -1, __FILE__, __LINE__);
}

static void t_intmin_rem(void)
{
    forja_rem_i32(INT32_MIN, -1, __FILE__, __LINE__);
}

static void t_faixa_fora(void)
{
    forja_faixa(11, 1, 10, false, "idade", __FILE__, __LINE__);
}

static void t_faixa_fora_excl(void)
{
    forja_faixa(10, 1, 10, true, "idade", __FILE__, __LINE__);
}
#endif

/* Registra todos os testes: os que passam, os que falham nos dois modos e os que falham so em DEV. */
static void registrar_testes(void)
{
    forja_teste_registrar("t_passa", t_passa);
    forja_teste_registrar("t_faixa_dentro", t_faixa_dentro);
    forja_teste_registrar("t_divisor_zero", t_divisor_zero);
#if defined(FORJA_DEV)
    forja_teste_registrar("t_estouro_i32", t_estouro_i32);
    forja_teste_registrar("t_estouro_i64", t_estouro_i64);
    forja_teste_registrar("t_intmin_div", t_intmin_div);
    forja_teste_registrar("t_intmin_rem", t_intmin_rem);
    forja_teste_registrar("t_faixa_fora", t_faixa_fora);
    forja_teste_registrar("t_faixa_fora_excl", t_faixa_fora_excl);
#endif
    forja_teste_registrar("t_expect_nao_aborta", t_expect_nao_aborta);
    forja_teste_registrar("t_assert_falso", t_assert_falso);
    forja_teste_registrar("t_panic", t_panic);
    forja_teste_registrar("t_json", t_json);
    forja_teste_registrar("t_depois", t_depois);
}

/* --gerar-runner: roda o executor com stdout em arquivo. Processo proprio: nao restaura nada. */
static void gerar_runner_com(const char *arquivo, void (*registrar)(void))
{
    registrar();
    freopen(arquivo, "wb", stdout);
    forja_testes_rodar();
    fflush(stdout);
}

static void gerar_runner_em(const char *arquivo)
{
    gerar_runner_com(arquivo, registrar_testes);
}

/* L16b: vazamento por teste. t_vaza_bloco deixa um bloco de heap vivo (DEV: o teste falha com R0009; release: nao ha rastreio e ele passa); os vizinhos nao vazam. */
static void t_vaza_bloco(void)
{
    (void)forja_heap_alocar(24, "vaza.fj", 5);
}

static void t_sem_vazar(void)
{
    forja_mem_liberar(forja_heap_alocar(8, "limpo.fj", 9), "limpo.fj", 10);
}

static void registrar_testes_vazamento(void)
{
    forja_teste_registrar("t_sem_vazar", t_sem_vazar);
    forja_teste_registrar("t_vaza_bloco", t_vaza_bloco);
    forja_teste_registrar("t_depois_vaza", t_sem_vazar);
}

/* L16b: queda por teste (Windows, dev e release). Gravar no endereco 0 e excecao do processador: R0011, o teste falha e o seguinte roda. */
static void t_ponteiro_nulo(void)
{
    volatile int *p = NULL;

    *p = 1;
}

static void registrar_testes_queda(void)
{
    forja_teste_registrar("t_antes_queda", t_passa);
    forja_teste_registrar("t_ponteiro_nulo", t_ponteiro_nulo);
    forja_teste_registrar("t_depois_queda", t_passa);
}

static void gerar_runner(void)
{
    gerar_runner_em("teste_rt_runner.tmp");
}

/* --gerar-runner-filtro e --gerar-runner-json (L16a): o executor com FORJA_TESTE_FILTRO / FORJA_TESTE_JSON no ambiente, como o driver do forja test os poe. */
static void gerar_runner_filtro(void)
{
    _putenv("FORJA_TESTE_FILTRO=t_passa");
    gerar_runner_em("teste_rt_runner_filtro.tmp");
}

static void gerar_runner_json(void)
{
    _putenv("FORJA_TESTE_JSON=1");
    gerar_runner_em("teste_rt_runner_json.tmp");
}

static void gerar_runner_vazamento(void)
{
    gerar_runner_com("teste_rt_runner_vazamento.tmp", registrar_testes_vazamento);
}

static void gerar_runner_queda(void)
{
    gerar_runner_com("teste_rt_runner_queda.tmp", registrar_testes_queda);
}

static const char *const nomes_que_passam[] = {"t_passa", "t_faixa_dentro", "t_divisor_zero", "t_depois"};

static const char *const nomes_que_falham[] = {
#if defined(FORJA_DEV)
    "t_estouro_i32", "t_estouro_i64", "t_intmin_div", "t_intmin_rem",
    "t_faixa_fora", "t_faixa_fora_excl",
#endif
    "t_expect_nao_aborta", "t_assert_falso", "t_panic", "t_json"
};

#define TOTAL_PASSAM (sizeof(nomes_que_passam) / sizeof(nomes_que_passam[0]))
#define TOTAL_FALHAM (sizeof(nomes_que_falham) / sizeof(nomes_que_falham[0]))

/* Confere se a saida do executor tem a linha "<prefixo><nome>" seguida de \n. */
static bool saida_tem_linha(const char *prefixo, const char *nome)
{
    char linha[128];

    snprintf(linha, sizeof(linha), "%s%s\n", prefixo, nome);
    return strstr(saida_runner, linha) != NULL;
}

/* Confere a saida do executor gravada por gerar_runner: cada teste, o resumo e as marcas de continuacao. */
static void verificar_runner(void)
{
    char resumo[64];
    size_t i;
    long lidos = ler_arquivo("teste_rt_runner.tmp", saida_runner, sizeof(saida_runner));

    remove("teste_rt_runner.tmp");
    CHECA(lidos > 0);
    for (i = 0; i < TOTAL_PASSAM; i++)
        CHECA(saida_tem_linha("ok   ", nomes_que_passam[i]));
    for (i = 0; i < TOTAL_FALHAM; i++)
        CHECA(saida_tem_linha("FALHOU ", nomes_que_falham[i]));
    snprintf(resumo, sizeof(resumo), "%d testes, %d falharam\n",
             (int)(TOTAL_PASSAM + TOTAL_FALHAM), (int)TOTAL_FALHAM);
    CHECA(strstr(saida_runner, resumo) != NULL);
    CHECA(strstr(saida_runner, "marca expect seguiu\n") != NULL);
    CHECA(strstr(saida_runner, "marca depois rodou\n") != NULL);
}

/* FORJA_TESTE_FILTRO=t_passa: so o t_passa roda (o filtro casa o titulo inteiro ou um trecho); os outros contam como filtrados e nada deles aparece. */
static void verificar_runner_filtro(void)
{
    char esperado[128];
    long lidos = ler_arquivo("teste_rt_runner_filtro.tmp", saida_runner, sizeof(saida_runner));

    remove("teste_rt_runner_filtro.tmp");
    snprintf(esperado, sizeof(esperado), "ok   t_passa\n1 testes, 0 falharam, %d filtrados\n", (int)(TOTAL_PASSAM + TOTAL_FALHAM) - 1);
    CHECA(lidos > 0 && strcmp(saida_runner, esperado) == 0);
}

/* FORJA_TESTE_JSON=1: uma linha por teste (ok, ou falhou com codigo, arquivo, linha e a causa da 1a falha, escapada) e a linha do resumo. */
static bool runner_json_confere(void)
{
    char resumo[128];
    const char *expect_falso = "{\"teste\": \"t_expect_nao_aborta\", \"resultado\": \"falhou\", \"codigo\": \"R0003\", \"arquivo\": \"";
    const char *causa_json = "\"causa\": \"valor \\\"a\\\" fora\\tcaminho \\\\ x\"}\n";

    snprintf(resumo, sizeof(resumo), "{\"resumo\": {\"total\": %d, \"ok\": %d, \"falhou\": %d, \"filtrados\": 0}}\n",
             (int)(TOTAL_PASSAM + TOTAL_FALHAM), (int)TOTAL_PASSAM, (int)TOTAL_FALHAM);
    return strstr(saida_runner, "{\"teste\": \"t_passa\", \"resultado\": \"ok\"}\n") != NULL
        && strstr(saida_runner, expect_falso) != NULL
        && strstr(saida_runner, "\"causa\": \"expect falhou: 1 == 2\"}\n") != NULL
        && strstr(saida_runner, "{\"teste\": \"t_json\", \"resultado\": \"falhou\", \"codigo\": \"R0001\", \"arquivo\": \"arq\\\"y\\\\z.fj\", \"linha\": 7, ") != NULL
        && strstr(saida_runner, causa_json) != NULL
        && strstr(saida_runner, "ok   ") == NULL && strstr(saida_runner, "FALHOU ") == NULL
        && strstr(saida_runner, resumo) != NULL;
}

static void verificar_runner_json(void)
{
    long lidos = ler_arquivo("teste_rt_runner_json.tmp", saida_runner, sizeof(saida_runner));

    remove("teste_rt_runner_json.tmp");
    CHECA(lidos > 0 && runner_json_confere());
}

/* L16b: o teste que vaza falha so em DEV (R0009 por teste); os vizinhos passam e o resumo conta 1 falha em DEV, nenhuma em release. */
static void verificar_runner_vazamento(void)
{
    long lidos = ler_arquivo("teste_rt_runner_vazamento.tmp", saida_runner, sizeof(saida_runner));

    remove("teste_rt_runner_vazamento.tmp");
#if defined(FORJA_DEV)
    CHECA(lidos > 0 && strcmp(saida_runner, "ok   t_sem_vazar\nFALHOU t_vaza_bloco\nok   t_depois_vaza\n3 testes, 1 falharam\n") == 0);
#else
    CHECA(lidos > 0 && strcmp(saida_runner, "ok   t_sem_vazar\nok   t_vaza_bloco\nok   t_depois_vaza\n3 testes, 0 falharam\n") == 0);
#endif
}

/* L16b: a queda (gravar no endereco 0) falha so o t_ponteiro_nulo; o teste seguinte roda. */
static void verificar_runner_queda(void)
{
    long lidos = ler_arquivo("teste_rt_runner_queda.tmp", saida_runner, sizeof(saida_runner));

    remove("teste_rt_runner_queda.tmp");
    CHECA(lidos > 0 && strcmp(saida_runner, "ok   t_antes_queda\nFALHOU t_ponteiro_nulo\nok   t_depois_queda\n3 testes, 1 falharam\n") == 0);
}

/* forja_falha em teste: a linha JSON em stderr tem aspas, barra e tab escapados. */
static void verificar_json(void)
{
    const char esperado[] = "{\"codigo\": \"R0001\", \"tipo\": \"runtime\", \"arquivo\": \"arq\\\"y\\\\z.fj\", \"linha\": 7, \"causa\": \"valor \\\"a\\\" fora\\tcaminho \\\\ x\"}\n";
    long lidos = ler_arquivo("teste_rt_json.tmp", saida_json, sizeof(saida_json));

    remove("teste_rt_json.tmp");
    CHECA(lidos > 0);
    CHECA(strcmp(saida_json, esperado) == 0);
}

/* Casos da v1.2 (secao 9.2). Cada caso e uma funcao que imprime o valor com forja_print_*.
 * O esperado sai do calculo externo (aritmetica de inteiro e struct), nao da saida do exe. */
static void caso_add_u8_limite(void)
{
    forja_print_u64(forja_add_u8(200, 55, __FILE__, __LINE__));
}

static void caso_add_u8_estouro(void)
{
    forja_print_u64(forja_add_u8(255, 1, __FILE__, __LINE__));
}

static void caso_sub_u8_estouro(void)
{
    forja_print_u64(forja_sub_u8(0, 1, __FILE__, __LINE__));
}

static void caso_mul_u16_estouro(void)
{
    forja_print_u64(forja_mul_u16(300, 300, __FILE__, __LINE__));
}

static void caso_add_u32_estouro(void)
{
    forja_print_u64(forja_add_u32(4294967295U, 1, __FILE__, __LINE__));
}

static void caso_mul_u64_estouro(void)
{
    forja_print_u64(forja_mul_u64(9223372036854775808ULL, 2, __FILE__, __LINE__));
}

static void caso_sub_u64_estouro(void)
{
    forja_print_u64(forja_sub_u64(0, 1, __FILE__, __LINE__));
}

static void caso_add_i8_estouro(void)
{
    forja_print_i64(forja_add_i8(127, 1, __FILE__, __LINE__));
}

static void caso_mul_i16_estouro(void)
{
    forja_print_i64(forja_mul_i16(200, 200, __FILE__, __LINE__));
}

static void caso_sub_i8_estouro(void)
{
    forja_print_i64(forja_sub_i8(-128, 1, __FILE__, __LINE__));
}

static void caso_div_u32_zero(void)
{
    forja_print_u64(forja_div_u32(7, 0, __FILE__, __LINE__));
}

static void caso_rem_u64_zero(void)
{
    forja_print_u64(forja_rem_u64(7, 0, __FILE__, __LINE__));
}

static void caso_div_i8_estouro(void)
{
    forja_print_i64(forja_div_i8(-128, -1, __FILE__, __LINE__));
}

static void caso_rem_i16_menos_um(void)
{
    forja_print_i64(forja_rem_i16(-32768, -1, __FILE__, __LINE__));
}

static void caso_shl_u8_7(void)
{
    forja_print_u64(forja_shl_u8(1, 7));
}

static void caso_shl_u8_8(void)
{
    forja_print_u64(forja_shl_u8(1, 8));
}

static void caso_shl_u32_4(void)
{
    forja_print_u64(forja_shl_u32(4294967295U, 4));
}

static void caso_shr_u64_64(void)
{
    forja_print_u64(forja_shr_u64(18446744073709551615ULL, 64));
}

static void caso_shr_u16_15(void)
{
    forja_print_u64(forja_shr_u16(65535, 15));
}

static void caso_as_u8_i64_255(void)
{
    forja_print_u64(forja_as_u8_de_i64(255, __FILE__, __LINE__));
}

static void caso_as_u8_i64_256(void)
{
    forja_print_u64(forja_as_u8_de_i64(256, __FILE__, __LINE__));
}

static void caso_as_u32_i64_menos_um(void)
{
    forja_print_u64(forja_as_u32_de_i64(-1, __FILE__, __LINE__));
}

static void caso_as_i64_u64_2_63(void)
{
    forja_print_i64(forja_as_i64_de_u64(9223372036854775808ULL, __FILE__, __LINE__));
}

static void caso_as_i8_u64_127(void)
{
    forja_print_i64(forja_as_i8_de_u64(127, __FILE__, __LINE__));
}

static void caso_as_i32_f64_menos_3_9(void)
{
    forja_print_i64(forja_as_i32_de_f64(-3.9, __FILE__, __LINE__));
}

static void caso_as_i32_f64_3e9(void)
{
    forja_print_i64(forja_as_i32_de_f64(3e9, __FILE__, __LINE__));
}

static void caso_as_u8_f64_menos_1(void)
{
    forja_print_u64(forja_as_u8_de_f64(-1.0, __FILE__, __LINE__));
}

/* __builtin_nan("") e NaN positivo: o texto da causa fica "nan" e nao depende do sinal do 0/0. */
static void caso_as_i64_f64_nan(void)
{
    forja_print_i64(forja_as_i64_de_f64(__builtin_nan(""), __FILE__, __LINE__));
}

static void caso_faixa_u64_2_63(void)
{
    forja_print_u64(forja_faixa_u64(9223372036854775808ULL, 0, 100, false, "x", __FILE__, __LINE__));
}

static void caso_print_u64_max(void)
{
    forja_print_u64(18446744073709551615ULL);
}

/* v1.4: prototipo de nome interno ligado ao simbolo real pelo rotulo asm (prova o macro com -Wall -Wextra -Werror). */
extern int64_t fjx_tamanho_rt(const char *) FORJA_ROTULO_C("strlen");

/* v1.3: str e ponteiro nao nulo. Itens 1, 2, 3, 5 e 7 do contrato 9.2 (iguais em DEV e em release). */
static void testar_str_e_ponteiros(void)
{
    int x = 7;
    void *r;

    CHECA(fjx_tamanho_rt("Grifo") == 5);                                   /* v1.4: FORJA_ROTULO_C */
    CHECA(forja_str_igual(FORJA_STR("abc", 3), FORJA_STR("abc", 3)));      /* 1 */
    CHECA(!forja_str_igual(FORJA_STR("abc", 3), FORJA_STR("abcd", 4)));    /* 2 */
    CHECA(forja_str_igual(FORJA_STR("x", 0), FORJA_STR("y", 0)));          /* 3 */
    CHECA(forja_str_de_cstr("abc", __FILE__, __LINE__).len == 3);          /* 5 */
    r = forja_nao_nulo(&x, "teste", __FILE__, __LINE__);
    CHECA(r == (void *)&x);                                                /* 7 */
}

/* v1.5: forja_indice dentro de 0..<n devolve o indice; forja_fatia_confere aceita '0..<3' e '1..=2' em n = 3 (iguais em DEV e em release). */
static void testar_indice_e_fatia(void)
{
    forja_fatia_confere(0, 3, true, 3, "x", 1);
    forja_fatia_confere(1, 2, false, 3, "x", 1);
    CHECA(forja_indice(2, 3, "x", 1) == 2);
}

/* v1.6 (a): heap. Aloca, grava e le 3 int64, libera; liberar NULL nao falha; make com cap 0 devolve NULL e epoca 0. */
static bool rt_mem_heap_ok(void)
{
    int64_t *p = (int64_t *)forja_mem_alocar(1u, 24, "x", 1);
    uint32_t ep = 99;
    bool ok = p != NULL;

    if (ok) {
        p[0] = 11;
        p[1] = 22;
        p[2] = 33;
        ok = p[0] == 11 && p[1] == 22 && p[2] == 33;
        forja_mem_liberar(p, "x", 1);
    }
    forja_mem_liberar(NULL, "x", 1);
    return ok && forja_lista_criar(1u, 0, 8, &ep, "x", 1) == NULL && ep == 0;
}

/* v1.6 (b): lista. Cresce de cap 2 para 8 e para 16 preservando os valores. */
static bool rt_mem_lista_ok(void)
{
    int64_t cap = 2, i;
    uint32_t ep = 99;
    int64_t *ptr = (int64_t *)forja_lista_criar(1u, 2, 8, &ep, "x", 1);
    bool ok = ptr != NULL;

    if (!ok)
        return false;
    ptr[0] = 5;
    ptr[1] = 6;
    ptr = (int64_t *)forja_lista_crescer(ptr, 2, &cap, 8, 1u, &ep, "x", 1);
    ok = cap == 8 && ptr[0] == 5 && ptr[1] == 6;
    for (i = 2; i < 8; i++)
        ptr[i] = i;
    ptr = (int64_t *)forja_lista_crescer(ptr, 8, &cap, 8, 1u, &ep, "x", 1);
    ok = ok && cap == 16 && ptr[0] == 5 && ptr[7] == 7;
    forja_lista_liberar(ptr, 1u, "x", 1);
    return ok;
}

/* v1.6 (c): frame. Blocos de 10 bytes ficam a 16 de distancia e alinhados; frame_reset zera o topo e sobe a epoca. */
static bool rt_mem_frame_ok(void)
{
    unsigned char *a = (unsigned char *)forja_mem_alocar(2u, 10, "x", 1);
    unsigned char *b = (unsigned char *)forja_mem_alocar(2u, 10, "x", 1);
    uint32_t e0, ep = 0;
    bool ok = a != b && (uintptr_t)a % 16 == 0 && (uintptr_t)b % 16 == 0 && b - a == 16 && forja_no_frame(a);

    e0 = forja_rt_frame.epoca;
    forja_frame_reset();
    ok = ok && forja_rt_frame.topo == 0 && forja_rt_frame.epoca == e0 + 1;
    forja_mem_liberar(a, "x", 1);
    (void)forja_lista_criar(2u, 4, 8, &ep, "x", 1);
    return ok && ep == forja_rt_frame.epoca;
}

/* v1.8 P5a (g): rastreador. DEV conta 1 bloco vivo a mais e volta a zero no delete; release devolve 0 nos dois pontos. */
static bool rt_rastreador_ok(void)
{
    int64_t antes = forja_rt_vivos_qtd();
    void *p = forja_mem_alocar(1u, 8, "x", 1);
    int64_t durante = forja_rt_vivos_qtd();

    forja_mem_liberar(p, "x", 1);
#if defined(FORJA_DEV)
    return durante == antes + 1 && forja_rt_vivos_qtd() == antes;
#else
    return antes == 0 && durante == 0 && forja_rt_vivos_qtd() == 0;
#endif
}

/* v1.8 P5b (d): Arena. Handle com idx >= 3, blocos alinhados em 16, usado = 48 + 48 + 1008, limpar zera, destruir libera. */
static bool rt_arena_ok(void)
{
    uint32_t h = forja_arena_criar(64);
    void *p = forja_mem_alocar(h, 40, "x", 1);
    void *q = forja_mem_alocar(h, 40, "x", 1);
    bool ok = (h & 0xFFFu) >= 3 && p != q && (uintptr_t)p % 16 == 0 && (uintptr_t)q % 16 == 0;

    ok = ok && forja_mem_alocar(h, 1000, "x", 1) != NULL && forja_arena_usado(h) == 1104;
    forja_arena_limpar(h);
    ok = ok && forja_arena_usado(h) == 0;
    forja_arena_destruir(h);
    return ok;
}

/* v1.8 P5b (e): Pool. 2 objetos vivos, devolver baixa para 1, o proximo pedido reusa o slot devolvido (LIFO). */
static bool rt_pool_ok(void)
{
    uint32_t h = forja_pool_criar(4);
    void *a = forja_mem_alocar(h, 24, "x", 1);
    void *b = forja_mem_alocar(h, 24, "x", 1);
    bool ok = a != b && forja_pool_vivos(h) == 2;
    void *c;

    forja_pool_devolver(h, a, "x", 1);
    ok = ok && forja_pool_vivos(h) == 1;
    c = forja_mem_alocar(h, 24, "x", 1);
    ok = ok && c == a && forja_pool_vivos(h) == 2;
    forja_pool_destruir(h);
    return ok;
}

/* v1.8 P5b (f): lista em Arena cresce por copia (cap 2 -> 8, valores preservados); liberar nao faz nada. */
static bool rt_lista_arena_ok(void)
{
    uint32_t h = forja_arena_criar(64);
    uint32_t ep = 99;
    int64_t cap = 2;
    int64_t *ptr = (int64_t *)forja_lista_criar(h, 2, 8, &ep, "x", 1);
    bool ok = ptr != NULL && ep == 0;

    ptr[0] = 5;
    ptr[1] = 6;
    ptr = (int64_t *)forja_lista_crescer(ptr, 2, &cap, 8, h, &ep, "x", 1);
    ok = ok && cap == 8 && ptr[0] == 5 && ptr[1] == 6;
    forja_lista_liberar(ptr, h, "x", 1);
    forja_arena_destruir(h);
    return ok;
}

/* v1.9 P8 (a): soa dinamica de 2 colunas (int64_t e uint8_t). Criar com cap 0 nao aloca; 20 appends crescem 0 -> 8 -> 16 -> 32. */
static bool rt_soa_ok(void)
{
    const int64_t tams[2] = {8, 1};
    int64_t len = 0, cap = 0, i;
    uint32_t epoca = 7;
    void *col[2] = {&epoca, &epoca};
    bool ok;

    forja_soa_criar(col, tams, 2u, 0, FORJA_ALOC_HEAP, &epoca, __FILE__, __LINE__);
    ok = col[0] == NULL && col[1] == NULL && epoca == 0;
    for (i = 0; i < 20; i++) {
        if (len == cap)
            forja_soa_crescer(col, tams, 2u, len, &cap, FORJA_ALOC_HEAP, &epoca, __FILE__, __LINE__);
        ((int64_t *)col[0])[len] = i * 7;
        ((uint8_t *)col[1])[len] = (uint8_t)i;
        len += 1;
    }
    ok = ok && cap == 32 && (uintptr_t)col[1] % 16 == 0;
    for (i = 0; i < 20; i++)
        ok = ok && ((int64_t *)col[0])[i] == i * 7 && ((uint8_t *)col[1])[i] == (uint8_t)i;
    forja_soa_liberar(col, FORJA_ALOC_HEAP, __FILE__, __LINE__);
    return ok;
}

/* v1.9 P9 (b): map int32_t -> int64_t contra tabela de referencia, 200000 operacoes aleatorias (insere e remove). */
static bool rt_mapa_confere(const forja_mapa *m, const int64_t *ref)
{
    int64_t presentes = 0, c;
    for (c = 0; c < 4096; c++) {
        int32_t chave = (int32_t)c;
        int64_t i = forja_mapa_achar(m, &chave, 4, false, __FILE__, __LINE__);
        if (ref[c] < 0 ? i != -1 : (i < 0 || ((int64_t *)m->valores)[i] != ref[c]))
            return false;
        presentes += ref[c] >= 0;
    }
    return m->len == presentes;
}

static bool rt_mapa_inteiros_ok(void)
{
    forja_mapa m = {0};
    static int64_t ref[4096];
    uint64_t x = 12345;
    int64_t op, c;
    bool ok = true;
    forja_mapa_criar(&m, FORJA_ALOC_HEAP, 0, 4, 8, __FILE__, __LINE__);
    for (c = 0; c < 4096; c++)
        ref[c] = -1;
    for (op = 1; op <= 200000 && ok; op++) {
        int32_t chave;
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        chave = (int32_t)((x >> 33) % 4096);
        if ((x >> 20) % 3 < 2) {
            int64_t i = forja_mapa_inserir(&m, &chave, 4, false, 8, __FILE__, __LINE__);
            ((int64_t *)m.valores)[i] = op;
            ref[chave] = op;
        } else {
            forja_mapa_remover(&m, &chave, 4, false, 8, __FILE__, __LINE__);
            ref[chave] = -1;
        }
        if (op % 997 == 0)
            ok = rt_mapa_confere(&m, ref);
    }
    forja_mapa_liberar(&m, __FILE__, __LINE__);
    return ok;
}

/* v1.9 P9 (c): chave str (hash FNV-1a sobre os bytes), swap-remove e busca de chave ausente. */
static bool rt_mapa_texto_ok(void)
{
    forja_mapa m = {0};
    forja_str nomes[5] = {FORJA_STR("ana", 3), FORJA_STR("bia", 3), FORJA_STR("caio", 4), FORJA_STR("duda", 4),
                          FORJA_STR("edu", 3)};
    forja_str bi = FORJA_STR("bi", 2);
    int64_t i, ib;
    bool ok;
    forja_mapa_criar(&m, FORJA_ALOC_HEAP, 0, 16, 4, __FILE__, __LINE__);
    for (i = 0; i < 5; i++) {
        int64_t k = forja_mapa_inserir(&m, &nomes[i], 16, true, 4, __FILE__, __LINE__);
        ((int32_t *)m.valores)[k] = (int32_t)(i * 10);
    }
    ib = forja_mapa_achar(&m, &nomes[1], 16, true, __FILE__, __LINE__);
    ok = ib >= 0 && ((int32_t *)m.valores)[ib] == 10;
    ok = ok && forja_mapa_achar(&m, &bi, 16, true, __FILE__, __LINE__) == -1;
    forja_mapa_remover(&m, &nomes[0], 16, true, 4, __FILE__, __LINE__);
    ok = ok && m.len == 4 && ((forja_str *)m.chaves)[0].len == 3 && __builtin_memcmp(((forja_str *)m.chaves)[0].ptr, "edu", 3) == 0;
    ok = ok && forja_mapa_achar(&m, &nomes[4], 16, true, __FILE__, __LINE__) == 0;
    forja_mapa_liberar(&m, __FILE__, __LINE__);
    return ok;
}

/* v1.9 P9 (d): map no frame (cap 4 cresce para 8), ordem densa depois de remover e reinserir chave existente. */
static bool rt_mapa_remove_ordem_ok(void)
{
    forja_mapa m = {0};
    const int64_t ordem1[4] = {1, 5, 3, 4}, ordem2[3] = {4, 5, 3};
    int64_t k, i;
    bool ok;
    forja_mapa_criar(&m, FORJA_ALOC_FRAME, 4, 8, 8, __FILE__, __LINE__);
    for (k = 1; k <= 5; k++) {
        i = forja_mapa_inserir(&m, &k, 8, false, 8, __FILE__, __LINE__);
        ((int64_t *)m.valores)[i] = 10 * k;
    }
    ok = m.cap == 8 && m.epoca == forja_rt_frame.epoca && m.len == 5;
    k = 2;
    forja_mapa_remover(&m, &k, 8, false, 8, __FILE__, __LINE__);
    ok = ok && m.len == 4;
    for (i = 0; i < 4 && ok; i++)
        ok = ((int64_t *)m.chaves)[i] == ordem1[i];
    k = 1;
    forja_mapa_remover(&m, &k, 8, false, 8, __FILE__, __LINE__);
    ok = ok && m.len == 3;
    for (i = 0; i < 3 && ok; i++)
        ok = ((int64_t *)m.chaves)[i] == ordem2[i];
    k = 5;
    ok = ok && forja_mapa_inserir(&m, &k, 8, false, 8, __FILE__, __LINE__) == 1 && m.len == 3;
    k = 1;
    ok = ok && forja_mapa_achar(&m, &k, 8, false, __FILE__, __LINE__) == -1;
    k = 2;
    ok = ok && forja_mapa_achar(&m, &k, 8, false, __FILE__, __LINE__) == -1;
    forja_mapa_liberar(&m, __FILE__, __LINE__);
    return ok && m.alocador == FORJA_ALOC_FRAME && m.chaves == NULL;
}

static void testar_memoria(void)
{
    CHECA(rt_rastreador_ok());
    CHECA(rt_arena_ok());
    CHECA(rt_pool_ok());
    CHECA(rt_lista_arena_ok());
    CHECA(rt_mem_heap_ok());
    CHECA(rt_mem_lista_ok());
    CHECA(rt_mem_frame_ok());
    CHECA(rt_soa_ok());
    CHECA(rt_mapa_inteiros_ok());
    CHECA(rt_mapa_texto_ok());
    CHECA(rt_mapa_remove_ordem_ok());
}

/* v1.7 (a): inteiros e bool, com zeros, hex e largura. */
static bool rt_fmt_inteiros_ok(void)
{
    forja_fmt_inicio();
    forja_fmt_i64(-42, 6 | FORJA_FMT_ZERO);
    forja_fmt_bytes("|", 1);
    forja_fmt_u64(255, 8 | FORJA_FMT_ZERO | FORJA_FMT_HEX);
    forja_fmt_bytes("|", 1);
    forja_fmt_i64(7, 3);
    forja_fmt_bool(true, 0);
    return forja_rt_fmt.len == 23 && memcmp(forja_rt_fmt.dados, "-00042|000000ff|  7true", 23) == 0;
}

/* v1.7 (b): reais sem precisao (%.9g / %.17g) e com precisao (campo guarda precisao + 1). */
static bool rt_fmt_reais_ok(void)
{
    const char *esperado = "2.5 2.50      2.5 0.10000000000000001";
    forja_fmt_inicio();
    forja_fmt_f32(2.5, 0);
    forja_fmt_bytes(" ", 1);
    forja_fmt_f32(2.5, 3u << 8);
    forja_fmt_bytes(" ", 1);
    forja_fmt_f32(2.5, 8 | (2u << 8));
    forja_fmt_bytes(" ", 1);
    forja_fmt_f64(0.1, 0);
    return forja_rt_fmt.len == (int64_t)strlen(esperado) && memcmp(forja_rt_fmt.dados, esperado, strlen(esperado)) == 0;
}

/* v1.7 (c): crescimento do buffer, finais str (heap) e cstr (frame). */
static bool rt_fmt_finais_ok(void)
{
    forja_str s;
    const char *c;
    bool ok;
    int i;

    forja_fmt_inicio();
    for (i = 0; i < 1000; i++)
        forja_fmt_bytes("a", 1);
    ok = forja_rt_fmt.len == 1000 && forja_rt_fmt.cap >= 1000;
    s = forja_fmt_str_final(1u, "x", 1);
    ok = ok && s.len == 1000 && s.ptr[999] == 'a' && s.ptr[1000] == 0;
    forja_mem_liberar((void *)s.ptr, "x", 1);
    forja_fmt_inicio();
    forja_fmt_cstr("ab", 0);
    c = forja_fmt_cstr_final(2u, "x", 1);
    ok = ok && strlen(c) == 2 && forja_no_frame(c);
    forja_frame_reset();
    return ok;
}

static void testar_formato(void)
{
    CHECA(rt_fmt_inteiros_ok());
    CHECA(rt_fmt_reais_ok());
    CHECA(rt_fmt_finais_ok());
}

/* Itens 4, 6 e 8: processo proprio (print e falha R0005 em DEV). */
static void caso_print_str(void)
{
    forja_print_str(FORJA_STR("abcdef", 3));
}

static void caso_str_de_cstr_nulo(void)
{
    forja_print_i64(forja_str_de_cstr(NULL, __FILE__, __LINE__).len);
}

static void caso_nao_nulo_nulo(void)
{
    void *r = forja_nao_nulo(NULL, "teste", __FILE__, __LINE__);

    forja_print_i32(r == NULL ? 1 : 0);
}

/* v1.11: forja_vetor_c_confere (slice_from): vetor valido e nulo com n == 0 passam; nulo com n > 0 e R0005; n < 0 e R0006. */
static void caso_vetor_c_valido(void)
{
    int32_t v[2] = {1, 2};

    (void)v; /* release: a macro vazia nao usa v */
    forja_vetor_c_confere(v, 2, __FILE__, __LINE__);
    forja_vetor_c_confere(NULL, 0, __FILE__, __LINE__);
    forja_print_i32(1);
}

static void caso_vetor_c_nulo(void)
{
    forja_vetor_c_confere(NULL, 3, __FILE__, __LINE__);
    forja_print_i32(1);
}

static void caso_vetor_c_negativo(void)
{
    forja_vetor_c_confere(NULL, -2, __FILE__, __LINE__);
    forja_print_i32(1);
}

/* v1.8 P5a (h): bloco de heap nunca liberado; em DEV o atexit relata R0009 e sai 101, em release sai 0. */
static void caso_vazamento(void)
{
    (void)forja_mem_alocar(1u, 8, "x", 7);
    forja_print_i32(1);
}

/* Um caso: nome (argumento de --caso), funcao e o esperado. saida vale em release e, em DEV, quando o caso
 * nao falha; codigo e causa so existem em DEV, para o caso que sai com exit 101 e JSON R0001/R0002. */
typedef struct {
    const char *nome;
    void (*fn)(void);
    const char *saida;
    const char *codigo;
    const char *causa;
} CasoRuntime;

static const CasoRuntime casos_runtime[] = {
    {"add_u8_limite", caso_add_u8_limite, "255\n", NULL, NULL},
    {"add_u8_estouro", caso_add_u8_estouro, "0\n", "R0002", "estouro de inteiro em soma u8: 255 e 1"},
    {"sub_u8_estouro", caso_sub_u8_estouro, "255\n", "R0002", "estouro de inteiro em subtracao u8: 0 e 1"},
    {"mul_u16_estouro", caso_mul_u16_estouro, "24464\n", "R0002", "estouro de inteiro em multiplicacao u16: 300 e 300"},
    {"add_u32_estouro", caso_add_u32_estouro, "0\n", "R0002", "estouro de inteiro em soma u32: 4294967295 e 1"},
    {"mul_u64_estouro", caso_mul_u64_estouro, "0\n", "R0002", "estouro de inteiro em multiplicacao u64: 9223372036854775808 e 2"},
    {"sub_u64_estouro", caso_sub_u64_estouro, "18446744073709551615\n", "R0002", "estouro de inteiro em subtracao u64: 0 e 1"},
    {"add_i8_estouro", caso_add_i8_estouro, "-128\n", "R0002", "estouro de inteiro em soma i8: 127 e 1"},
    {"mul_i16_estouro", caso_mul_i16_estouro, "-25536\n", "R0002", "estouro de inteiro em multiplicacao i16: 200 e 200"},
    {"sub_i8_estouro", caso_sub_i8_estouro, "127\n", "R0002", "estouro de inteiro em subtracao i8: -128 e 1"},
    {"div_u32_zero", caso_div_u32_zero, "0\n", NULL, NULL},
    {"rem_u64_zero", caso_rem_u64_zero, "0\n", NULL, NULL},
    {"div_i8_estouro", caso_div_i8_estouro, "-128\n", "R0002", "estouro de inteiro em divisao i8: -128 e -1"},
    {"rem_i16_menos_um", caso_rem_i16_menos_um, "0\n", NULL, NULL},
    {"shl_u8_7", caso_shl_u8_7, "128\n", NULL, NULL},
    {"shl_u8_8", caso_shl_u8_8, "0\n", NULL, NULL},
    {"shl_u32_4", caso_shl_u32_4, "4294967280\n", NULL, NULL},
    {"shr_u64_64", caso_shr_u64_64, "0\n", NULL, NULL},
    {"shr_u16_15", caso_shr_u16_15, "1\n", NULL, NULL},
    {"as_u8_i64_255", caso_as_u8_i64_255, "255\n", NULL, NULL},
    {"as_u8_i64_256", caso_as_u8_i64_256, "0\n", "R0002", "conversao 'as' de 256 para u8 fora da faixa"},
    {"as_u32_i64_menos_um", caso_as_u32_i64_menos_um, "4294967295\n", "R0002", "conversao 'as' de -1 para u32 fora da faixa"},
    {"as_i64_u64_2_63", caso_as_i64_u64_2_63, "-9223372036854775808\n", "R0002", "conversao 'as' de 9223372036854775808 para i64 fora da faixa"},
    {"as_i8_u64_127", caso_as_i8_u64_127, "127\n", NULL, NULL},
    {"as_i32_f64_menos_3_9", caso_as_i32_f64_menos_3_9, "-3\n", NULL, NULL},
    {"as_i32_f64_3e9", caso_as_i32_f64_3e9, "2147483647\n", "R0002", "conversao 'as' de 3000000000 para i32 fora da faixa"},
    {"as_u8_f64_menos_1", caso_as_u8_f64_menos_1, "0\n", "R0002", "conversao 'as' de -1 para u8 fora da faixa"},
    {"as_i64_f64_nan", caso_as_i64_f64_nan, "0\n", "R0002", "conversao 'as' de nan para i64 fora da faixa"},
    {"faixa_u64_2_63", caso_faixa_u64_2_63, "9223372036854775808\n", "R0001", "valor 9223372036854775808 fora da faixa 0..=100 do campo 'x'"},
    {"print_u64_max", caso_print_u64_max, "18446744073709551615\n", NULL, NULL},
    {"print_str", caso_print_str, "abc\n", NULL, NULL},
    {"str_de_cstr_nulo", caso_str_de_cstr_nulo, "0\n", "R0005", "conversao 'as' de cstr para str: ponteiro nulo"},
    {"nao_nulo_nulo", caso_nao_nulo_nulo, "1\n", "R0005", "teste: ponteiro nulo"},
    {"vetor_c_valido", caso_vetor_c_valido, "1\n", NULL, NULL},
    {"vetor_c_nulo", caso_vetor_c_nulo, "1\n", "R0005", "slice_from: ponteiro nulo"},
    {"vetor_c_negativo", caso_vetor_c_negativo, "1\n", "R0006", "slice_from com len -2 negativo"},
    {"vazamento", caso_vazamento, "1\n", "R0009", "vazamento: 1 blocos (8 bytes) alocados aqui seguem vivos no fim do programa"},
};

#define TOTAL_CASOS (sizeof(casos_runtime) / sizeof(casos_runtime[0]))

/* Confere a saida do caso filho: em DEV o caso com codigo tem de sair com 101 e a linha JSON certa;
 * sem codigo (e em release) tem de sair com 0 e o stdout esperado. */
static bool caso_passou(const CasoRuntime *caso, int status, const char *saida, const char *erro)
{
#if defined(FORJA_DEV)
    char codigo_linha[64];
    char causa_linha[384];

    if (caso->codigo != NULL) {
        snprintf(codigo_linha, sizeof(codigo_linha), "\"codigo\": \"%s\"", caso->codigo);
        snprintf(causa_linha, sizeof(causa_linha), "\"causa\": \"%s\"}", caso->causa);
        return status == 101 && strstr(erro, codigo_linha) != NULL && strstr(erro, causa_linha) != NULL;
    }
#else
    (void)erro; /* release nao tem falha esperada: o stderr nao entra na conta */
#endif
    return status == 0 && strcmp(saida, caso->saida) == 0;
}

/* Tira os CR: o stdout e o stderr em modo texto do Windows gravam \r\n, e o esperado usa so \n. */
static void tirar_cr(char *texto)
{
    char *leitura = texto;
    char *escrita = texto;

    while (*leitura != '\0') {
        if (*leitura != '\r')
            *escrita++ = *leitura;
        leitura++;
    }
    *escrita = '\0';
}

/* Roda um caso em processo proprio: system() com stdout e stderr em arquivo, porque a falha de runtime
 * termina o processo com exit 101 e nao pode derrubar o verificador. */
static bool verificar_caso(const CasoRuntime *caso, const char *exe)
{
    char comando[512];
    char saida[256] = {0};
    char erro[1024] = {0};
    int status;
    bool ok;

    snprintf(comando, sizeof(comando), "%s --caso %s > teste_rt_caso.tmp 2> teste_rt_caso_err.tmp", exe, caso->nome);
    status = system(comando);
    ler_arquivo("teste_rt_caso.tmp", saida, sizeof(saida));
    ler_arquivo("teste_rt_caso_err.tmp", erro, sizeof(erro));
    remove("teste_rt_caso.tmp");
    remove("teste_rt_caso_err.tmp");
    tirar_cr(saida);
    tirar_cr(erro);
    ok = caso_passou(caso, status, saida, erro);
    if (!ok)
        fprintf(stderr, "caso %s: exit %d, saida [%s], erro [%s]\n", caso->nome, status, saida, erro);
    return ok;
}

/* --caso NOME: roda o caso no processo atual. Devolve 2 se o nome nao existe. */
static int executar_caso(const char *nome)
{
    size_t i;

    for (i = 0; i < TOTAL_CASOS; i++) {
        if (strcmp(casos_runtime[i].nome, nome) == 0) {
            casos_runtime[i].fn();
            fflush(stdout);
            return 0;
        }
    }
    fprintf(stderr, "caso desconhecido: %s\n", nome);
    return 2;
}

/* Um CHECA por caso em cada modo: 33 casos somam 33 verificacoes em DEV e 33 em release. */
static void verificar_casos(const char *exe)
{
    size_t i;

    for (i = 0; i < TOTAL_CASOS; i++)
        CHECA(verificar_caso(&casos_runtime[i], exe));
}

int main(int argc, char **argv)
{
    if (argc > 2 && strcmp(argv[1], "--caso") == 0)
        return executar_caso(argv[2]);
    if (argc > 1 && strcmp(argv[1], "--gerar-print") == 0) {
        gerar_print();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--gerar-runner") == 0) {
        gerar_runner();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--gerar-runner-filtro") == 0) {
        gerar_runner_filtro();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--gerar-runner-json") == 0) {
        gerar_runner_json();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--gerar-runner-vazamento") == 0) {
        gerar_runner_vazamento();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--gerar-runner-queda") == 0) {
        gerar_runner_queda();
        return 0;
    }
#if defined(FORJA_DEV)
    puts("modo: DEV (-DFORJA_DEV)");
#else
    puts("modo: release");
#endif
    verificar_print();
    testar_aritmetica();
    testar_faixa();
#if !defined(FORJA_DEV)
    testar_release_sem_falha();
#endif
    testar_str_e_ponteiros();
    testar_indice_e_fatia();
    testar_memoria();
    testar_formato();
    verificar_runner();
    verificar_runner_filtro();
    verificar_runner_json();
    verificar_runner_vazamento();
    verificar_runner_queda();
    verificar_json();
    verificar_casos(argv[0]);
    if (falhas != 0) {
        fprintf(stderr, "FALHOU: %d de %d verificacoes\n", falhas, verificacoes);
        return 1;
    }
    printf("OK: %d verificacoes\n", verificacoes);
    return 0;
}
