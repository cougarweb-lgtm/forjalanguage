/*
 * forja_rt_pilha.h - rastreio da pilha .fj nas falhas de runtime do backend C (forja run|test|build --backend c --rastreio, IA1-b13).
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b13: primeira versao. Pilha sombra (uma lista de quadros por thread) com a MESMA linha JSON do backend x64 (runtime\nativo\rt_pilha.c):
 *            coluna do ponto da falha, valor do R0006 (indice e tamanho; limites da fatia) e pilha .fj (ate 8 quadros, "quadros" = total se passar).
 * Verificacao: python -I ..\tests\ia_first\verificar_rastreio_b13.py --forja ..\compiler\forja.exe
 *
 * UNIDADE PROPRIA do runtime (D-IA1-13): o emissor (backend\emit_c_runtime.c) so a escreve no .c gerado com --rastreio (EMIT_RASTREIO), e define antes FORJA_RASTREIO:
 * sem a flag o .c e o exe saem byte a byte iguais. O forja_rt.h NAO muda (ele e copiado inteiro para o .c): em vez de editar a falha dele, esta unidade entra DUAS vezes,
 *   PARTE 1 (FORJA_PILHA_PARTE == 1), antes do forja_rt.h: declara os quadros e liga fputs, fprintf e snprintf aos embrulhos abaixo, so enquanto o texto do forja_rt.h passa;
 *   PARTE 2 (FORJA_PILHA_PARTE == 2), depois dele: desliga os embrulhos e os define.
 * Os embrulhos so mexem em tres formatos exatos do forja_rt.h (forja_falha, forja_main_erro, forja_indice e forja_fatia_confere); qualquer outra chamada passa direto.
 * O codigo gerado abre um quadro no inicio de cada proc (FORJA_QUADRO) e, antes de cada instrucao, grava nele o ponto atual (FORJA_PONTO: arquivo, linha, coluna).
 * Quadro de proc chamado pelo runtime (main e test) e a RAIZ: nao liga ao anterior (o longjmp de um teste que falhou deixa o topo velho). Quadro sem ponto nao entra na pilha
 * (no x64 so chamada com posicao na tabela entra). Falha em outra thread mostra os quadros dessa thread (o x64 nao mostra nenhum).
 */
#ifndef FORJA_RASTREIO
#error "forja_rt_pilha.h so existe com FORJA_RASTREIO (o emissor define antes de escreve-lo)"
#endif
#ifndef FORJA_PILHA_PARTE
#error "defina FORJA_PILHA_PARTE como 1 (antes do forja_rt.h) ou 2 (depois dele)"
#endif

#if FORJA_PILHA_PARTE == 1

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FORJA_PILHA_MAX 8                  /* quadros no JSON; o resto vira "quadros": total */
#define FORJA_PILHA_PASSOS_TOTAL 1000000u  /* teto da contagem (recursao muito funda), igual ao x64 */
#define FORJA_PILHA_VALOR_NENHUM 0
#define FORJA_PILHA_VALOR_INDICE 1
#define FORJA_PILHA_VALOR_FATIA 2

typedef struct ForjaQuadro {
    const char *proc;
    const char *arquivo;
    uint32_t linha, coluna; /* 0 = ainda sem ponto */
    struct ForjaQuadro *abaixo;
} ForjaQuadro;

static _Thread_local ForjaQuadro *forja_pilha_topo;

static __attribute__((unused)) void forja_pilha_sair(ForjaQuadro *q)
{
    forja_pilha_topo = q->abaixo;
}

/* Abre o quadro do proc corrente (variavel local forja_quadro, desfeita na saida da funcao por cleanup). RAIZ = proc chamado pelo runtime (main, test). */
#define FORJA_QUADRO(NOME, RAIZ) \
    ForjaQuadro forja_quadro __attribute__((cleanup(forja_pilha_sair))) = {(NOME), NULL, 0, 0, (RAIZ) ? NULL : forja_pilha_topo}; \
    forja_pilha_topo = &forja_quadro

/* O ponto (arquivo, linha, coluna do .fj) da instrucao que vai rodar no proc corrente. */
#define FORJA_PONTO(ARQ, LINHA, COLUNA) (forja_quadro.arquivo = (ARQ), forja_quadro.linha = (LINHA), forja_quadro.coluna = (COLUNA))

/* R0004: o main ja voltou, entao a pilha e o proprio main (posicao do corpo dele), gravada pelo main gerado antes do forja_main_erro. */
#define FORJA_PONTO_MAIN(ARQ, LINHA, COLUNA) forja_pilha_main_ponto((ARQ), (LINHA), (COLUNA))

static __attribute__((unused)) void forja_pilha_main_ponto(const char *arquivo, uint32_t linha, uint32_t coluna); /* so o main com '-> ! E' a chama */
static int forja_pilha_fputs(const char *texto, FILE *f);
__attribute__((format(gnu_printf, 2, 3))) static int forja_pilha_fprintf(FILE *f, const char *formato, ...);
__attribute__((format(gnu_printf, 3, 4))) static int forja_pilha_snprintf(char *destino, size_t tam, const char *formato, ...);

#undef fputs
#undef fprintf
#undef snprintf
#define fputs forja_pilha_fputs
#define fprintf forja_pilha_fprintf
#define snprintf forja_pilha_snprintf

#else /* FORJA_PILHA_PARTE == 2 */

#undef fputs
#undef fprintf
#undef snprintf

#define FORJA_PILHA_FMT_LINHA "\", \"linha\": %d, \"causa\": \""
#define FORJA_PILHA_FMT_MAIN_ERRO "\", \"linha\": 0, \"causa\": \"main devolveu o erro de indice %d\"}\n"
#define FORJA_PILHA_FMT_INDICE "indice %lld fora de 0..<%lld"
#define FORJA_PILHA_FMT_FATIA "fatia %lld%s%lld fora de 0..=%lld"

static ForjaQuadro forja_pilha_main_quadro; /* o main do R0004 (proc "main") */
static int forja_pilha_valor_tipo;
static int64_t forja_pilha_valor_a, forja_pilha_valor_b, forja_pilha_valor_c;

static void forja_pilha_main_ponto(const char *arquivo, uint32_t linha, uint32_t coluna)
{
    forja_pilha_main_quadro = (ForjaQuadro){"main", arquivo, linha, coluna, NULL};
}

/* O quadro mais interno com ponto (o x64 so conta chamadas com posicao na tabela), ou NULL. */
static const ForjaQuadro *forja_pilha_primeiro(void)
{
    const ForjaQuadro *q = forja_pilha_topo;

    while (q != NULL && q->linha == 0)
        q = q->abaixo;
    return q;
}

/* Quantos quadros com ponto a pilha tem (ate FORJA_PILHA_PASSOS_TOTAL passos). */
static uint32_t forja_pilha_contar(void)
{
    uint32_t total = 0;
    uint32_t passos = 0;

    for (const ForjaQuadro *q = forja_pilha_topo; q != NULL && passos < FORJA_PILHA_PASSOS_TOTAL; q = q->abaixo, passos++)
        total += q->linha != 0;
    return total;
}

static void forja_pilha_quadro(const ForjaQuadro *q)
{
    fputs("{\"proc\": \"", stderr);
    forja_json_texto(q->proc);
    fputs("\", \"arquivo\": \"", stderr);
    forja_json_texto(q->arquivo);
    fprintf(stderr, "\", \"linha\": %u, \"coluna\": %u}", (unsigned)q->linha, (unsigned)q->coluna);
}

/* ", "valor": {...}" do R0006 (indice e tamanho; limites da fatia e tamanho); consome o valor guardado pelo snprintf do forja_rt.h. */
static void forja_pilha_valor(void)
{
    if (forja_pilha_valor_tipo == FORJA_PILHA_VALOR_INDICE)
        fprintf(stderr, ", \"valor\": {\"indice\": %lld, \"tamanho\": %lld}", (long long)forja_pilha_valor_a, (long long)forja_pilha_valor_b);
    else if (forja_pilha_valor_tipo == FORJA_PILHA_VALOR_FATIA)
        fprintf(stderr, ", \"valor\": {\"inicio\": %lld, \"fim\": %lld, \"tamanho\": %lld}", (long long)forja_pilha_valor_a, (long long)forja_pilha_valor_b,
                (long long)forja_pilha_valor_c);
    forja_pilha_valor_tipo = FORJA_PILHA_VALOR_NENHUM;
}

/* O resto da linha R depois da causa: aspas que fecham a causa, valor, pilha (do mais fundo ao main) e o fim da linha. Sem quadro nao ha "pilha". */
static void forja_pilha_fechar(void)
{
    uint32_t total = forja_pilha_contar();
    uint32_t mostrados = 0;

    fputs("\"", stderr);
    forja_pilha_valor();
    if (total > 0) {
        fputs(", \"pilha\": [", stderr);
        for (const ForjaQuadro *q = forja_pilha_topo; q != NULL && mostrados < FORJA_PILHA_MAX; q = q->abaixo) {
            if (q->linha == 0)
                continue;
            fputs(mostrados > 0 ? ", " : "", stderr);
            forja_pilha_quadro(q);
            mostrados++;
        }
        fputs("]", stderr);
        if (total > mostrados)
            fprintf(stderr, ", \"quadros\": %u", (unsigned)total);
    }
    fputs("}\n", stderr);
}

/* O numero da linha e, se o quadro mais interno cai nessa linha, a coluna dele (senao a coluna seria de outro ponto). Depois vem o inicio da causa. */
static int forja_pilha_linha(int linha)
{
    const ForjaQuadro *q = forja_pilha_primeiro();

    fprintf(stderr, "\", \"linha\": %d", linha);
    if (q != NULL && linha > 0 && q->linha == (uint32_t)linha)
        fprintf(stderr, ", \"coluna\": %u", (unsigned)q->coluna);
    fputs(", \"causa\": \"", stderr);
    return 0;
}

/* R0004: o texto da causa como o forja_rt.h o escreve (sem o fim da linha) e depois o fim com a pilha do main. */
static int forja_pilha_main_erro(int erro)
{
    fprintf(stderr, "\", \"linha\": 0, \"causa\": \"main devolveu o erro de indice %d\"", erro);
    if (forja_pilha_main_quadro.linha != 0) {
        fputs(", \"pilha\": [", stderr);
        forja_pilha_quadro(&forja_pilha_main_quadro);
        fputs("]", stderr);
    }
    fputs("}\n", stderr);
    return 0;
}

static int forja_pilha_fputs(const char *texto, FILE *f)
{
    if (f == stderr && strcmp(texto, "\"}\n") == 0) {
        forja_pilha_fechar();
        return 0;
    }
    return fputs(texto, f);
}

static int forja_pilha_fprintf(FILE *f, const char *formato, ...)
{
    va_list args;
    int escritos;

    va_start(args, formato);
    if (f == stderr && strcmp(formato, FORJA_PILHA_FMT_LINHA) == 0)
        escritos = forja_pilha_linha(va_arg(args, int));
    else if (f == stderr && strcmp(formato, FORJA_PILHA_FMT_MAIN_ERRO) == 0)
        escritos = forja_pilha_main_erro(va_arg(args, int));
    else
        escritos = vfprintf(f, formato, args);
    va_end(args);
    return escritos;
}

/* Guarda os numeros que o forja_rt.h poe na causa do R0006 (a mesma conta do x64: o indice e o tamanho; o inicio, o fim e o tamanho da fatia). */
static void forja_pilha_guardar_valor(const char *formato, va_list args)
{
    va_list copia;

    va_copy(copia, args);
    if (strcmp(formato, FORJA_PILHA_FMT_INDICE) == 0) {
        forja_pilha_valor_tipo = FORJA_PILHA_VALOR_INDICE;
        forja_pilha_valor_a = va_arg(copia, long long);
        forja_pilha_valor_b = va_arg(copia, long long);
    } else if (strcmp(formato, FORJA_PILHA_FMT_FATIA) == 0) {
        forja_pilha_valor_tipo = FORJA_PILHA_VALOR_FATIA;
        forja_pilha_valor_a = va_arg(copia, long long);
        (void)va_arg(copia, const char *);
        forja_pilha_valor_b = va_arg(copia, long long);
        forja_pilha_valor_c = va_arg(copia, long long);
    }
    va_end(copia);
}

static int forja_pilha_snprintf(char *destino, size_t tam, const char *formato, ...)
{
    va_list args;
    int escritos;

    va_start(args, formato);
    forja_pilha_guardar_valor(formato, args);
    escritos = vsnprintf(destino, tam, formato, args);
    va_end(args);
    return escritos;
}

#endif
