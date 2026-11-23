/*
 * forja_rt.h - runtime C11 da linguagem Forja. Um arquivo, tudo static/macro.
 *
 * Versao: 0.10.9
 * Changelog:
 *   0.10.9 - O0 P13: forja_nao_nulo, forja_indice, forja_fatia_confere, forja_vetor_c_confere e forja_str_de_cstr existem tambem no release (valem salvo -DFORJA_SEM_CHECAGEM, que o driver passa com --checagem nao); forja_indice_lista do release vira forja_indice.
 *   0.10.8 - Ryu d2fixed P1: forja_fmt_f64_curto/f32_curto (laco %.*e + strtod) saem daqui para forja_rt_ryu.h (gerado, Ryu d2s/f2s; Display sempre posicional, igual ao backend nativo). FORJA_RT_VERSAO segue 0.10.0.
 *   0.10.7 - M6c P88: FORJA_HANDLE_NASCE/SOLTA (rastreio automatico do foreign com creates/destroys/takes): dev = forja_handle_nasce/solta (solta de desconhecido = silencio; untrack agora usa forja_handle_tirar), release = nada.
 *   0.10.6 - M6 P71b: forja_handle_track|untrack (track_handle/untrack_handle de std:mem): lista de handles de lib C; vivo no fim = linha R0009 {"tipo": "handle", "kind": ...} (sem pilha) e exit 101 no mesmo atexit do heap; untrack de desconhecido = R0008. Release: no-op.
 *   0.10.5 - M6 P71a: forja_memsnap_serie|vivos|bytes|relatar (snapshot/report_since de std:mem). Dev: a serie de nascimento vai no campo `alocador` do bloco vivo (cabecalho segue 48 B), contador forja_rt_vivos_bytes; a linha {"tipo": "mem_report"} e a do runtime nativo sem "pilha". Release: no-op.
 *   0.10.4 - M5 P45 (T0-3): threads no oraculo. frame, buffer do f"...", salto e flags do executor de testes viram _Thread_local (FORJA_FIO); ForjaTrava (SRWLOCK real no _WIN32, nula no POSIX) so trava com forja_rt_multithread
 *            (ligado por forja_proc_multithread_ligar do std:os, ANTES do CreateThread: thread ja dentro de secao quando o flag liga segue sem lock) e protege a lista de blocos vivos do dev e o registro de Arena e Pool;
 *            define FORJA_ES_TRAVA_T/PEGAR/SOLTAR para forja_rt_es/processo/stdin.h; print de str e f"..." travam o FILE (linha inteira); forja_falha fora de teste: so a 1a thread relata e sai (panic simultaneo = 1 relatorio).
 *   0.10.3 - IA1-L5a: forja_fmt_f32_curto e forja_fmt_f64_curto (print de real pela forma mais curta que volta igual) e forja_fmt_imprimir_erro (eprint). FORJA_RT_VERSAO segue 0.10.0.
 *   0.10.2 - Linguagem v1.11 L16b: forja test acusa vazamento por teste e queda por teste. Dev: o executor marca a contagem de blocos vivos ao entrar no teste; ao sair, os blocos alocados nele e ainda vivos
 *            fazem o teste falhar com as linhas R0009 ("alocados aqui seguem vivos no fim do teste") e saem da lista do rastreador (nao repetem no fim do programa). Windows: o executor liga
 *            AddVectoredExceptionHandler; EXCEPTION_ACCESS_VIOLATION ou EXCEPTION_ILLEGAL_INSTRUCTION dentro de um teste viram R0011 (com o titulo), o teste falha e o proximo roda. 2a queda no mesmo
 *            teste, queda fora de teste e pilha estourada seguem como antes. FORJA_RT_VERSAO segue 0.10.0 (a macro sobe no L17).
 *   0.10.1 -Linguagem v1.11 L16a: forja test --filtro/--json. forja_testes_rodar le FORJA_TESTE_FILTRO (so os testes cujo titulo contem o trecho; os outros contam em "filtrados")
 *            e FORJA_TESTE_JSON=1 (uma linha JSON por teste e a linha {"resumo": ...}); forja_json_texto_em(FILE *); a 1a falha do teste (forja_falha ou expect) fica anotada
 *            em forja_rt_falha_*. Sem as variaveis a saida e a de antes, byte a byte. FORJA_RT_VERSAO segue 0.10.0 (a macro sobe no L17).
 *   0.10.0 - Linguagem v1.11: forja_vetor_c_confere (slice_from; dev: len negativo R0006, nulo com len > 0 R0005; release: macro vazia).
 *            FORJA_RT_VERSAO 0.10.0.
 *   0.9.1- so comentario (M0-D): lista das funcoes de libc usadas (abaixo), para quem porta o runtime. A macro FORJA_RT_VERSAO fica
 *           "0.9.0" (nenhum byte de codigo muda).
 *   0.9.0 - Linguagem v1.9: soa dinamica (forja_soa_criar/crescer/liberar, P8) e map (forja_mapa, forja_mapa_criar/achar/inserir/remover/liberar, P9).
 *           FORJA_RT_VERSAO 0.9.0 (P12).
 *   0.8.0 - Linguagem v1.8 P5b: alocadores Arena e Pool (registro com handle (geracao << 12) | idx, blocos encadeados, lista livre LIFO,
 *           lista cresce por copia em Arena).
 *   0.8.0 - Linguagem v1.8 P5a: rastreador de vazamento em DEV (cabecalho de 48 B com arquivo/linha, lista dos blocos de heap vivos,
 *           R0009 no fim do programa; FORJA_BIBLIOTECA so relata, sem _Exit) e forja_rt_sem_relatorio (falha ja relatada = silencio).
 *   0.7.0 (P9) - forja_fmt_bytes ignora n <= 0 (o gcc -Werror=stringop-overflow recusava o memcpy com n negativo). Versao global no P10.
 *   0.7.0 -Linguagem v1.7: texto formatado f"..." (forja_fmt_*: buffer unico que cresce, alinhamento, zeros, hex, precisao,
 *           finais str/cstr pelo alocador e impressao). Falta de memoria = R0007. Continua sem <string.h>. O resto fica intacto.
 *   0.6.0 - Linguagem v1.6: alocacao (heap com cabecalho em dev, frame de 16 MiB), listas, R0007 e R0008. Continua sem
 *           <string.h> (__builtin_memset/__builtin_memcpy). O resto fica intacto.
 *   0.5.0 - Linguagem v1.5: R0006 (indice ou fatia fora). Dev: forja_indice e forja_fatia_confere falham com R0006; release: macros
 *           com o valor cru, custo zero. Continua sem <string.h>. O resto fica intacto.
 *   0.4.0 - Linguagem v1.4 P3: FORJA_ROTULO_C(nome), rotulo asm que liga o prototipo fjx_<nome> ao simbolo C real (proc foreign).
 *           Continua sem <string.h>. O resto fica intacto.
 *   0.3.0 - Linguagem v1.3: ponteiros e texto. forja_str ({ptr, len} em bytes UTF-8) com FORJA_STR, forja_str_igual
 *           (memcmp) e forja_print_str; R0005 (ponteiro nulo) com forja_falha_nulo; forja_nao_nulo e
 *           forja_str_de_cstr (dev: NULL => R0005; release: nao confere, str_de_cstr(NULL) da str vazia).
 *           Sem <string.h>: memcmp e strlen pelo __builtin. O que existia fica intacto.
 *   0.2.0 - Linguagem v1.2: tipos numericos. Aritmetica checada de i8 i16 u8 u16 u32 u64
 *           (dev: R0002; release: wrap), divisao e resto sem sinal, resto i8/i16, shl/shr sem sinal,
 *           conversoes 'as' inteiro->inteiro e real->inteiro (dev: R0002; release: trunca ou satura),
 *           forja_faixa_u64 e forja_print_u64. i32/i64 e tudo o que ja existia ficam iguais.
 *   0.1.3 - forja_main_erro: R0004 quando 'main -> ! E' devolve erro (o main gerado sai com 1).
 *   0.1.2 - so comentarios: texto ASCII puro (o runtime e copiado dentro de todo .c gerado).
 *   0.1.1 - forja_print_f32: f32 em stdout com %.9g (ida e volta exata de float) e \n.
 *   0.1.0 - API inicial: falha JSON (R0001 faixa, R0002 estouro, R0003 panic/assert),
 *           prints, aritmetica checada em FORJA_DEV e conta crua em release,
 *           divisao/resto por zero devolvem 0 nos dois modos,
 *           executor de testes com setjmp/longjmp.
 * Verificacao: tests\runtime\rodar_teste_rt.bat (autoteste em DEV e em release).
 *
 * Funcoes de libc usadas (contagem de chamadas "nome(" neste arquivo, por grep; inclui citacoes em comentario; so as que aparecem):
 *   stdio:  snprintf 24 | fputs 11 | printf 7 | fprintf 5 | fputc 2 | fwrite 2 | puts 2 | putchar 1 | fflush 1
 *   stdlib: malloc 4 | realloc 4 | free 3 | exit 1 | _Exit 1 | atexit 2
 *   setjmp: setjmp 1 | longjmp 1 (executor de testes; dev e release)
 *   __builtin_*: memcpy 11 | memset 8 | add_overflow 4 | sub_overflow 4 | mul_overflow 4 | memcmp 3 | strlen 3 | strcmp 1
 * Includes: setjmp.h, stdbool.h, stdint.h, stdio.h, stdlib.h (sem string.h: funcoes mem e str so pelos __builtin_).
 *
 * Uso: o .c gerado inclui este header uma vez. Com -DFORJA_DEV as checagens
 * ligam e falham com erro JSON em stderr. Sem ela, as mesmas funcoes viram conta
 * crua, sem checagem de estouro nem de faixa. Compilar sempre com
 * -std=c11 -Wall -Wextra -Werror -D__USE_MINGW_ANSI_STDIO=1.
 *
 * Contrato das checagens: toda funcao que pode falhar recebe arquivo (const char*)
 * e linha (int), para o erro apontar para o .fj e nao para o C gerado.
 */
#ifndef FORJA_RT_H
#define FORJA_RT_H

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FORJA_RT_VERSAO "0.10.0"
#define FORJA_TESTES_MAX 4096

/* Liga um prototipo de nome C interno ao simbolo real 'nome' (gcc/clang; __USER_LABEL_PREFIX__ e vazio no Windows x64).
 * Usado pelos procs foreign (emit_c_externo.c): o nome interno nunca colide com builtin nem com <stdlib.h>. */
#define FORJA_ROTULO_TXT_(x) #x
#define FORJA_ROTULO_XTXT_(x) FORJA_ROTULO_TXT_(x)
#define FORJA_ROTULO_C(nome) __asm__(FORJA_ROTULO_XTXT_(__USER_LABEL_PREFIX__) nome)

/* M5 P45 (T0-3): threads no oraculo. O estado por fio (frame, buffer do f"...", salto e flags do executor de testes) e _Thread_local; o estado compartilhado (lista de blocos vivos do dev, registro de
 * Arena e Pool, tabelas de arquivo, processo e stdin) fica sob ForjaTrava: SRWLOCK real no _WIN32 e SO com forja_rt_multithread ligado (forja_proc_multithread_ligar do std:os; 1 teste de byte com ele
 * desligado, como o rt_multithread do runtime nativo). POSIX = stub (sem thread, como o Q2 do P28b): trava nula. */
#define FORJA_FIO _Thread_local
static volatile int forja_rt_multithread __attribute__((unused)) = 0; /* pegajoso: so vai de 0 para 1 */
#if defined(_WIN32)
typedef struct { void *srw; } ForjaTrava;
void forja_srw_pegar(void *srw) FORJA_ROTULO_C("AcquireSRWLockExclusive");
void forja_srw_soltar(void *srw) FORJA_ROTULO_C("ReleaseSRWLockExclusive");

static inline int forja_trava_pegar(ForjaTrava *t)
{
    if (!forja_rt_multithread)
        return 0;
    forja_srw_pegar(t);
    return 1;
}

static inline void forja_trava_soltar(ForjaTrava *t, int tomou)
{
    if (tomou)
        forja_srw_soltar(t);
}

#define FORJA_ES_TRAVA_T ForjaTrava
#define FORJA_ES_PEGAR(t) forja_trava_pegar(&(t))
#define FORJA_ES_SOLTAR(t, tomou) forja_trava_soltar(&(t), (tomou))
#define forja_rt_travar_fluxo(f) _lock_file(f)
#define forja_rt_destravar_fluxo(f) _unlock_file(f)

/* Fim de thread (DLL chamada por thread de fora, sem DllMain no oraculo): o frame e o buffer do f"..." de cada fio vao num slot FLS (FlsAlloc de kernel32, sem windows.h) cujo callback os
 * libera quando a thread morre. Slot = indice + 1 (0 = ainda nao alocado); se 2 threads alocam juntas, uma delas perde o indice (vaza 1 slot, nunca memoria). */
unsigned long forja_fls_alocar(void (*retorno)(void *)) FORJA_ROTULO_C("FlsAlloc");
int forja_fls_gravar(unsigned long indice, void *valor) FORJA_ROTULO_C("FlsSetValue");

static void forja_fio_soltar(void *p)
{
    free(p);
}

static inline void forja_fio_guardar(volatile unsigned long *slot, void *p)
{
    if (*slot == 0) {
        unsigned long indice = forja_fls_alocar(forja_fio_soltar);

        if (indice == 0xFFFFFFFFul)
            return;
        (void)__sync_val_compare_and_swap(slot, 0ul, indice + 1ul);
    }
    (void)forja_fls_gravar(*slot - 1ul, p);
}
static volatile unsigned long forja_rt_fls_frame __attribute__((unused)) = 0; /* compartilhado: o indice do slot, escrito 1x */
static volatile unsigned long forja_rt_fls_fmt __attribute__((unused)) = 0; /* compartilhado: idem */
#define FORJA_FIO_GUARDAR_FRAME(p) forja_fio_guardar(&forja_rt_fls_frame, (p))
#define FORJA_FIO_GUARDAR_FMT(p) forja_fio_guardar(&forja_rt_fls_fmt, (p))
#else
#define FORJA_FIO_GUARDAR_FRAME(p) ((void)(p))
#define FORJA_FIO_GUARDAR_FMT(p) ((void)(p))
typedef char ForjaTrava;

static inline int forja_trava_pegar(ForjaTrava *t)
{
    (void)t;
    return 0;
}

static inline void forja_trava_soltar(ForjaTrava *t, int tomou)
{
    (void)t;
    (void)tomou;
}

#define forja_rt_travar_fluxo(f) ((void)(f))
#define forja_rt_destravar_fluxo(f) ((void)(f))
#endif
static ForjaTrava forja_rt_trava_vivos __attribute__((unused)); /* compartilhado: lista de blocos vivos (dev) */
static ForjaTrava forja_rt_trava_alocs __attribute__((unused)); /* compartilhado: registro de Arena e Pool */
static void *forja_rt_falha_dona = NULL; /* compartilhado: o fio que ganhou o relatorio da falha fatal; so vai de NULL para o fio */

/* Codigos de falha de runtime; vao para o campo "codigo" do JSON de stderr. */
#define FORJA_R0001 "R0001" /* faixa violada */
#define FORJA_R0002 "R0002" /* estouro de inteiro */
#define FORJA_R0003 "R0003" /* panic ou assert do programa */
#define FORJA_R0005 "R0005" /* ponteiro nulo onde a Forja exige ^T ou cstr */
#define FORJA_R0006 "R0006" /* indice ou fatia fora */
#define FORJA_R0009 "R0009" /* bloco de heap vivo no fim do programa ou do teste */
#define FORJA_R0011 "R0011" /* excecao do processador dentro de um teste */

/* Funcao de teste: sem argumentos, sem retorno; falha e comunicada por forja_expect/forja_falha. */
typedef void (*ForjaTesteFn)(void);

typedef struct {
    const char *nome;
    ForjaTesteFn fn;
} ForjaTesteEntrada;

/* Estado do executor de testes. Static porque cada .c gerado tem o seu proprio. */
static FORJA_FIO jmp_buf forja_rt_salto;
static FORJA_FIO bool forja_rt_em_teste = false;
static bool forja_rt_sem_relatorio = false; /* true depois de uma falha ou erro de main: o rastreador de vazamento (R0009) cala */
static FORJA_FIO bool forja_rt_teste_falhou = false;
static int forja_rt_teste_atual = 0;
static ForjaTesteEntrada forja_rt_testes[FORJA_TESTES_MAX];
static int forja_rt_testes_total = 0;
/* Falha do teste em curso para a saida --json (forja test): so a 1a conta; o texto e o mesmo do rt nativo (rt_teste.c). */
#define FORJA_TESTE_CAUSA_BYTES 512
static bool forja_rt_falha_anotada = false;
static const char *forja_rt_falha_codigo = NULL;
static const char *forja_rt_falha_arquivo = NULL;
static int forja_rt_falha_linha = 0;
static char forja_rt_falha_causa[FORJA_TESTE_CAUSA_BYTES];
/* L16b: blocos de heap vivos ao entrar no teste em curso (o vazamento por teste) e a queda (excecao do processador) ja tratada nele. */
static int64_t forja_rt_teste_vivos_antes = 0;
static bool forja_rt_queda_no_teste = false;

/* Escreve texto em f escapado para JSON: aspas, barra e controles viram escape. */
static inline void forja_json_texto_em(FILE *f, const char *texto)
{
    const unsigned char *p = (const unsigned char *)(texto != NULL ? texto : "");

    for (; *p != '\0'; p++) {
        switch (*p) {
        case '"':
            fputs("\\\"", f);
            break;
        case '\\':
            fputs("\\\\", f);
            break;
        case '\n':
            fputs("\\n", f);
            break;
        case '\r':
            fputs("\\r", f);
            break;
        case '\t':
            fputs("\\t", f);
            break;
        default:
            if (*p < 0x20)
                fprintf(f, "\\u%04x", (unsigned)*p);
            else
                fputc(*p, f);
        }
    }
}

/* Escreve texto em stderr escapado para JSON. */
static inline void forja_json_texto(const char *texto)
{
    forja_json_texto_em(stderr, texto);
}

/* A 1a falha do teste em curso (forja test --json): R-code ou expect; a causa e copiada (o buffer de quem falhou some no longjmp). */
static inline void forja_rt_anotar_falha(const char *codigo, const char *causa, const char *arquivo, int linha)
{
    if (forja_rt_falha_anotada)
        return;
    forja_rt_falha_anotada = true;
    forja_rt_falha_codigo = codigo;
    forja_rt_falha_arquivo = arquivo;
    forja_rt_falha_linha = linha;
    snprintf(forja_rt_falha_causa, sizeof(forja_rt_falha_causa), "%s", causa != NULL ? causa : "");
}

/* Falha de runtime: uma linha JSON em stderr. Fora de teste sai com 101; dentro de
 * teste marca a falha do teste e volta ao executor por longjmp, sem sair do processo. */
_Noreturn static inline void forja_falha(const char *codigo, const char *causa, const char *arquivo, int linha)
{
    /* P45: fora de teste so a 1a thread a chegar relata e sai; as outras giram ate o exit dela. A mesma thread de novo (handler de saida que falha) passa. */
    if (!forja_rt_em_teste) {
        void *dona = __sync_val_compare_and_swap(&forja_rt_falha_dona, NULL, (void *)&forja_rt_em_teste);

        while (dona != NULL && dona != (void *)&forja_rt_em_teste)
            __builtin_ia32_pause();
    }
    fputs("{\"codigo\": \"", stderr);
    forja_json_texto(codigo);
    fputs("\", \"tipo\": \"runtime\", \"arquivo\": \"", stderr);
    forja_json_texto(arquivo);
    fprintf(stderr, "\", \"linha\": %d, \"causa\": \"", linha);
    forja_json_texto(causa);
    fputs("\"}\n", stderr);

    forja_rt_sem_relatorio = true;
    if (forja_rt_em_teste) {
        forja_rt_anotar_falha(codigo, causa, arquivo, linha);
        forja_rt_teste_falhou = true;
        longjmp(forja_rt_salto, 1);
    }
    exit(101);
}

/* Estouro de inteiro (R0002) com os operandos na causa, para mostrar o valor que estourou. */
_Noreturn static inline void forja_falha_estouro(const char *operacao, long long a, long long b, const char *arquivo, int linha)
{
    char causa[160];

    snprintf(causa, sizeof(causa), "estouro de inteiro em %s: %lld e %lld", operacao, a, b);
    forja_falha(FORJA_R0002, causa, arquivo, linha);
}

/* Estouro de inteiro sem sinal (R0002). Operandos em %llu: forja_falha_estouro usaria %lld e
 * mostraria negativo um u64 acima de INT64_MAX. */
_Noreturn static inline void forja_falha_estouro_u(const char *operacao, unsigned long long a, unsigned long long b, const char *arquivo, int linha)
{
    char causa[160];

    snprintf(causa, sizeof(causa), "estouro de inteiro em %s: %llu e %llu", operacao, a, b);
    forja_falha(FORJA_R0002, causa, arquivo, linha);
}

/* Conversao 'as' inteiro com sinal fora da faixa do destino (R0002). alvo e o nome do tipo, ex. "u8". */
_Noreturn static inline void forja_falha_conversao_i(long long valor, const char *alvo, const char *arquivo, int linha)
{
    char causa[160];

    snprintf(causa, sizeof(causa), "conversao 'as' de %lld para %s fora da faixa", valor, alvo);
    forja_falha(FORJA_R0002, causa, arquivo, linha);
}

/* Conversao 'as' de inteiro sem sinal fora da faixa do destino (R0002). */
_Noreturn static inline void forja_falha_conversao_u(unsigned long long valor, const char *alvo, const char *arquivo, int linha)
{
    char causa[160];

    snprintf(causa, sizeof(causa), "conversao 'as' de %llu para %s fora da faixa", valor, alvo);
    forja_falha(FORJA_R0002, causa, arquivo, linha);
}

/* Conversao 'as' de real para inteiro fora da faixa (R0002). Com FORJA_COM_RYU (o emissor define quando o programa converte real
 * para inteiro) o valor sai pelo Display curto de forja_rt_ryu.h, igual ao runtime nativo (o menor texto posicional); sem ele (so
 * quando o header e incluido por outro caminho) cai em %.17g. */
#ifdef FORJA_COM_RYU
static int64_t forja_curto_texto_f64(double v, unsigned char *out);
#endif
_Noreturn static inline void forja_falha_conversao_f(double valor, const char *alvo, const char *arquivo, int linha)
{
    char causa[480];
#ifdef FORJA_COM_RYU
    unsigned char texto[400];
    int64_t n = forja_curto_texto_f64(valor, texto);

    snprintf(causa, sizeof(causa), "conversao 'as' de %.*s para %s fora da faixa", (int)n, (const char *)texto, alvo);
#else
    snprintf(causa, sizeof(causa), "conversao 'as' de %.17g para %s fora da faixa", valor, alvo);
#endif
    forja_falha(FORJA_R0002, causa, arquivo, linha);
}

/* Imprime int32 em stdout seguido de \n. Mesmo formato que o prototipo do compilador gera. */
static inline void forja_print_i32(int32_t valor)
{
    printf("%d\n", (int)valor);
}

/* Imprime int64 em stdout seguido de \n, com %lld. */
static inline void forja_print_i64(int64_t valor)
{
    printf("%lld\n", (long long)valor);
}

/* Imprime true ou false seguido de \n. */
static inline void forja_print_bool(bool valor)
{
    puts(valor ? "true" : "false");
}

/* Imprime o texto seguido de \n. */
static inline void forja_print_texto(const char *texto)
{
    puts(texto);
}

/* Imprime f32 em stdout com %.9g seguido de \n: 9 digitos bastam para ida e volta exata de float. */
static inline void forja_print_f32(float valor)
{
    printf("%.9g\n", (double)valor);
}

/* Imprime uint64 em stdout seguido de \n, com %llu. Cobre o intervalo de uint sem cair em negativo. */
static inline void forja_print_u64(uint64_t valor)
{
    printf("%llu\n", (unsigned long long)valor);
}

/* str da Forja: texto UTF-8 imutavel, ponteiro + tamanho em BYTES (sem NUL obrigatorio). */
typedef struct forja_str {
    const char *ptr;
    int64_t len;
} forja_str;

/* Literal de str: lit e o literal C e n o numero de bytes decodificados (o compilador conta). */
#define FORJA_STR(lit, n) ((forja_str){(lit), (n)})

/* Igualdade de str por conteudo (nos dois modos): mesmo tamanho e mesmos bytes. */
static inline bool forja_str_igual(forja_str a, forja_str b)
{
    return a.len == b.len && (a.len == 0 || __builtin_memcmp(a.ptr, b.ptr, (size_t)a.len) == 0);
}

/* Imprime o str em stdout seguido de \n (os len bytes, sem exigir NUL), igual ao puts. */
static inline void forja_print_str(forja_str s)
{
    forja_rt_travar_fluxo(stdout);
    if (s.len > 0)
        fwrite(s.ptr, 1, (size_t)s.len, stdout);
    fputc('\n', stdout);
    forja_rt_destravar_fluxo(stdout);
}

/* Ponteiro nulo onde a Forja exige ^T ou cstr: R0005 com "<oque>: ponteiro nulo". */
_Noreturn static inline void forja_falha_nulo(const char *oque, const char *arquivo, int linha)
{
    char causa[256];

    snprintf(causa, sizeof(causa), "%s: ponteiro nulo", oque);
    forja_falha(FORJA_R0005, causa, arquivo, linha);
}

/* O0 P13: as 5 checagens de indice e nulo valem no dev e no release com checagem (padrao); o release compilado com -DFORJA_SEM_CHECAGEM (--checagem nao) as tira. */
#if defined(FORJA_DEV) || !defined(FORJA_SEM_CHECAGEM)

/* ^T ou cstr nao nulo; NULL => R0005. Devolve o proprio ponteiro. */
static inline void *forja_nao_nulo(const void *p, const char *oque, const char *arquivo, int linha)
{
    if (p == NULL)
        forja_falha_nulo(oque, arquivo, linha);
    return (void *)p;
}

/* 'c as str': strlen no cstr; NULL => R0005. */
static inline forja_str forja_str_de_cstr(const char *c, const char *arquivo, int linha)
{
    if (c == NULL)
        forja_falha_nulo("conversao 'as' de cstr para str", arquivo, linha);
    return (forja_str){c, (int64_t)__builtin_strlen(c)};
}

/* a[i]: 0 <= i < n devolve i; senao R0006. */
static inline int64_t forja_indice(int64_t i, int64_t n, const char *arquivo, int linha)
{
    char causa[256];

    if (i >= 0 && i < n)
        return i;
    snprintf(causa, sizeof(causa), "indice %lld fora de 0..<%lld", (long long)i, (long long)n);
    forja_falha(FORJA_R0006, causa, arquivo, linha);
}

/* a[lo..<hi] (exclusiva) ou a[lo..=hi]: limites dentro de 0..=n e inicio ate o fim; senao R0006.
 * Inclusiva sem 'hi + 1' (nao estoura): lo - 1 <= hi. */
static inline void forja_fatia_confere(int64_t lo, int64_t hi, bool exclusiva, int64_t n, const char *arquivo, int linha)
{
    char causa[256];
    bool dentro = exclusiva ? (lo >= 0 && lo <= hi && hi <= n) : (lo >= 0 && hi < n && lo - 1 <= hi);

    if (dentro)
        return;
    snprintf(causa, sizeof(causa), "fatia %lld%s%lld fora de 0..=%lld", (long long)lo, exclusiva ? "..<" : "..=", (long long)hi,
             (long long)n);
    forja_falha(FORJA_R0006, causa, arquivo, linha);
}

/* slice_from(p, len = n): n < 0 => R0006; p nulo com n > 0 => R0005 (nulo com n == 0 e a fatia vazia). */
static inline void forja_vetor_c_confere(const void *p, int64_t n, const char *arquivo, int linha)
{
    char causa[256];

    if (n < 0) {
        snprintf(causa, sizeof(causa), "slice_from com len %lld negativo", (long long)n);
        forja_falha(FORJA_R0006, causa, arquivo, linha);
    }
    if (p == NULL && n > 0)
        forja_falha_nulo("slice_from", arquivo, linha);
}

#else

/* --checagem nao: nao confere ponteiro (custo zero); deref de nulo cai como no C. */
#define forja_nao_nulo(p, oque, arquivo, linha) ((void *)(p))

/* --checagem nao: indice e fatia sem checagem (custo zero); acesso fora e comportamento indefinido do C. */
#define forja_indice(i, n, arquivo, linha) ((int64_t)(i))
#define forja_fatia_confere(lo, hi, exclusiva, n, arquivo, linha) ((void)0)
#define forja_vetor_c_confere(p, n, arquivo, linha) ((void)0)

/* --checagem nao: cstr nulo da str vazia (o strlen ja domina o custo; evita queda). */
static inline forja_str forja_str_de_cstr(const char *c, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (c == NULL)
        return (forja_str){"", 0};
    return (forja_str){c, (int64_t)__builtin_strlen(c)};
}

#endif

#if defined(FORJA_DEV)

/* Soma i32; estouro => R0002 com os operandos. */
static inline int32_t forja_add_i32(int32_t a, int32_t b, const char *arquivo, int linha)
{
    int32_t resultado;

    if (__builtin_add_overflow(a, b, &resultado))
        forja_falha_estouro("soma i32", a, b, arquivo, linha);
    return resultado;
}

/* Subtracao i32; estouro => R0002 com os operandos. */
static inline int32_t forja_sub_i32(int32_t a, int32_t b, const char *arquivo, int linha)
{
    int32_t resultado;

    if (__builtin_sub_overflow(a, b, &resultado))
        forja_falha_estouro("subtracao i32", a, b, arquivo, linha);
    return resultado;
}

/* Multiplicacao i32; estouro => R0002 com os operandos. */
static inline int32_t forja_mul_i32(int32_t a, int32_t b, const char *arquivo, int linha)
{
    int32_t resultado;

    if (__builtin_mul_overflow(a, b, &resultado))
        forja_falha_estouro("multiplicacao i32", a, b, arquivo, linha);
    return resultado;
}

/* Soma i64; estouro => R0002 com os operandos. */
static inline int64_t forja_add_i64(int64_t a, int64_t b, const char *arquivo, int linha)
{
    int64_t resultado;

    if (__builtin_add_overflow(a, b, &resultado))
        forja_falha_estouro("soma i64", a, b, arquivo, linha);
    return resultado;
}

/* Subtracao i64; estouro => R0002 com os operandos. */
static inline int64_t forja_sub_i64(int64_t a, int64_t b, const char *arquivo, int linha)
{
    int64_t resultado;

    if (__builtin_sub_overflow(a, b, &resultado))
        forja_falha_estouro("subtracao i64", a, b, arquivo, linha);
    return resultado;
}

/* Multiplicacao i64; estouro => R0002 com os operandos. */
static inline int64_t forja_mul_i64(int64_t a, int64_t b, const char *arquivo, int linha)
{
    int64_t resultado;

    if (__builtin_mul_overflow(a, b, &resultado))
        forja_falha_estouro("multiplicacao i64", a, b, arquivo, linha);
    return resultado;
}

/* Divisao i32: divisor 0 devolve 0 (nunca erro); INT_MIN/-1 => R0002. */
static inline int32_t forja_div_i32(int32_t a, int32_t b, const char *arquivo, int linha)
{
    if (b == 0)
        return 0;
    if (a == INT32_MIN && b == -1)
        forja_falha_estouro("divisao i32", a, b, arquivo, linha);
    return a / b;
}

/* Resto i32: divisor 0 devolve 0 (nunca erro); INT_MIN%-1 => R0002. */
static inline int32_t forja_rem_i32(int32_t a, int32_t b, const char *arquivo, int linha)
{
    if (b == 0)
        return 0;
    if (a == INT32_MIN && b == -1)
        forja_falha_estouro("resto i32", a, b, arquivo, linha);
    return a % b;
}

/* Divisao i64: divisor 0 devolve 0 (nunca erro); INT64_MIN/-1 => R0002. */
static inline int64_t forja_div_i64(int64_t a, int64_t b, const char *arquivo, int linha)
{
    if (b == 0)
        return 0;
    if (a == INT64_MIN && b == -1)
        forja_falha_estouro("divisao i64", a, b, arquivo, linha);
    return a / b;
}

/* Resto i64: divisor 0 devolve 0 (nunca erro); INT64_MIN%-1 => R0002. */
static inline int64_t forja_rem_i64(int64_t a, int64_t b, const char *arquivo, int linha)
{
    if (b == 0)
        return 0;
    if (a == INT64_MIN && b == -1)
        forja_falha_estouro("resto i64", a, b, arquivo, linha);
    return a % b;
}

/* Faixa inclusiva lo..=hi, ou exclusiva no hi (lo..<hi); fora => R0001. Devolve o valor. */
static inline int64_t forja_faixa(int64_t valor, int64_t lo, int64_t hi, bool exclusiva, const char *campo, const char *arquivo, int linha)
{
    char causa[256];
    bool fora = valor < lo || (exclusiva ? valor >= hi : valor > hi);

    if (!fora)
        return valor;
    snprintf(causa, sizeof(causa), "valor %lld fora da faixa %lld..%s%lld do campo '%s'",
             (long long)valor, (long long)lo, exclusiva ? "<" : "=", (long long)hi, campo);
    forja_falha(FORJA_R0001, causa, arquivo, linha);
}

/* Faixa de inteiro sem sinal: mesma regra de forja_faixa, com %llu para o valor nao sair negativo. */
static inline uint64_t forja_faixa_u64(uint64_t valor, uint64_t lo, uint64_t hi, bool exclusiva, const char *campo, const char *arquivo, int linha)
{
    char causa[256];
    bool fora = valor < lo || (exclusiva ? valor >= hi : valor > hi);

    if (!fora)
        return valor;
    snprintf(causa, sizeof(causa), "valor %llu fora da faixa %llu..%s%llu do campo '%s'",
             (unsigned long long)valor, (unsigned long long)lo, exclusiva ? "<" : "=", (unsigned long long)hi, campo);
    forja_falha(FORJA_R0001, causa, arquivo, linha);
}

/* Gera forja_add_S, forja_sub_S e forja_mul_S de um inteiro com sinal: estouro => R0002 com os operandos. */
#define FORJA_RT_DEV_ARIT_SINAL(S, T) \
static inline T forja_add_##S(T a, T b, const char *arquivo, int linha) \
{ \
    T resultado; \
    if (__builtin_add_overflow(a, b, &resultado)) \
        forja_falha_estouro("soma " #S, a, b, arquivo, linha); \
    return resultado; \
} \
static inline T forja_sub_##S(T a, T b, const char *arquivo, int linha) \
{ \
    T resultado; \
    if (__builtin_sub_overflow(a, b, &resultado)) \
        forja_falha_estouro("subtracao " #S, a, b, arquivo, linha); \
    return resultado; \
} \
static inline T forja_mul_##S(T a, T b, const char *arquivo, int linha) \
{ \
    T resultado; \
    if (__builtin_mul_overflow(a, b, &resultado)) \
        forja_falha_estouro("multiplicacao " #S, a, b, arquivo, linha); \
    return resultado; \
}

/* Igual a FORJA_RT_DEV_ARIT_SINAL, mas o estouro sai por forja_falha_estouro_u (operandos sem sinal). */
#define FORJA_RT_DEV_ARIT_SEM_SINAL(S, T) \
static inline T forja_add_##S(T a, T b, const char *arquivo, int linha) \
{ \
    T resultado; \
    if (__builtin_add_overflow(a, b, &resultado)) \
        forja_falha_estouro_u("soma " #S, a, b, arquivo, linha); \
    return resultado; \
} \
static inline T forja_sub_##S(T a, T b, const char *arquivo, int linha) \
{ \
    T resultado; \
    if (__builtin_sub_overflow(a, b, &resultado)) \
        forja_falha_estouro_u("subtracao " #S, a, b, arquivo, linha); \
    return resultado; \
} \
static inline T forja_mul_##S(T a, T b, const char *arquivo, int linha) \
{ \
    T resultado; \
    if (__builtin_mul_overflow(a, b, &resultado)) \
        forja_falha_estouro_u("multiplicacao " #S, a, b, arquivo, linha); \
    return resultado; \
}

FORJA_RT_DEV_ARIT_SINAL(i8, int8_t)
FORJA_RT_DEV_ARIT_SINAL(i16, int16_t)
FORJA_RT_DEV_ARIT_SEM_SINAL(u8, uint8_t)
FORJA_RT_DEV_ARIT_SEM_SINAL(u16, uint16_t)
FORJA_RT_DEV_ARIT_SEM_SINAL(u32, uint32_t)
FORJA_RT_DEV_ARIT_SEM_SINAL(u64, uint64_t)

/* Divisao i8/i16: divisor 0 devolve 0; MIN/-1 nao cabe no tipo => R0002 (release faz wrap). */
static inline int8_t forja_div_i8(int8_t a, int8_t b, const char *arquivo, int linha)
{
    if (b == 0)
        return 0;
    if (a == INT8_MIN && b == -1)
        forja_falha_estouro("divisao i8", a, b, arquivo, linha);
    return (int8_t)(a / b);
}

static inline int16_t forja_div_i16(int16_t a, int16_t b, const char *arquivo, int linha)
{
    if (b == 0)
        return 0;
    if (a == INT16_MIN && b == -1)
        forja_falha_estouro("divisao i16", a, b, arquivo, linha);
    return (int16_t)(a / b);
}

/* Gera forja_as_<S>_de_i64 com faixa inclusiva MIN..MAX do destino; fora => R0002 com o valor. */
#define FORJA_RT_DEV_AS_DE_I64(S, T, MIN, MAX) \
static inline T forja_as_##S##_de_i64(int64_t v, const char *arquivo, int linha) \
{ \
    if (v < (int64_t)(MIN) || v > (int64_t)(MAX)) \
        forja_falha_conversao_i(v, #S, arquivo, linha); \
    return (T)v; \
}

/* Gera forja_as_<S>_de_u64: so o teto importa, o uint64 nunca fica abaixo de 0. */
#define FORJA_RT_DEV_AS_DE_U64(S, T, MAX) \
static inline T forja_as_##S##_de_u64(uint64_t v, const char *arquivo, int linha) \
{ \
    if (v > (uint64_t)(MAX)) \
        forja_falha_conversao_u(v, #S, arquivo, linha); \
    return (T)v; \
}

/* Gera forja_as_<S>_de_f64 para destinos de 32 bits ou menos: faixa aberta (MIN-1, MAX+1), trunca para zero.
 * NaN falha porque nenhuma comparacao com NaN e verdadeira. */
#define FORJA_RT_DEV_AS_DE_F64(S, T, MIN, MAX) \
static inline T forja_as_##S##_de_f64(double v, const char *arquivo, int linha) \
{ \
    if (!(v > (double)(MIN) - 1.0 && v < (double)(MAX) + 1.0)) \
        forja_falha_conversao_f(v, #S, arquivo, linha); \
    return (T)v; \
}

/* int64 -> u64: todo int64 positivo cabe em uint64, so o negativo fica fora. */
static inline uint64_t forja_as_u64_de_i64(int64_t v, const char *arquivo, int linha)
{
    if (v < 0)
        forja_falha_conversao_i(v, "u64", arquivo, linha);
    return (uint64_t)v;
}

FORJA_RT_DEV_AS_DE_I64(i8, int8_t, INT8_MIN, INT8_MAX)
FORJA_RT_DEV_AS_DE_I64(i16, int16_t, INT16_MIN, INT16_MAX)
FORJA_RT_DEV_AS_DE_I64(i32, int32_t, INT32_MIN, INT32_MAX)
FORJA_RT_DEV_AS_DE_I64(u8, uint8_t, 0, UINT8_MAX)
FORJA_RT_DEV_AS_DE_I64(u16, uint16_t, 0, UINT16_MAX)
FORJA_RT_DEV_AS_DE_I64(u32, uint32_t, 0, UINT32_MAX)
FORJA_RT_DEV_AS_DE_U64(i8, int8_t, INT8_MAX)
FORJA_RT_DEV_AS_DE_U64(i16, int16_t, INT16_MAX)
FORJA_RT_DEV_AS_DE_U64(i32, int32_t, INT32_MAX)
FORJA_RT_DEV_AS_DE_U64(i64, int64_t, INT64_MAX)
FORJA_RT_DEV_AS_DE_U64(u8, uint8_t, UINT8_MAX)
FORJA_RT_DEV_AS_DE_U64(u16, uint16_t, UINT16_MAX)
FORJA_RT_DEV_AS_DE_U64(u32, uint32_t, UINT32_MAX)
FORJA_RT_DEV_AS_DE_F64(i8, int8_t, INT8_MIN, INT8_MAX)
FORJA_RT_DEV_AS_DE_F64(i16, int16_t, INT16_MIN, INT16_MAX)
FORJA_RT_DEV_AS_DE_F64(i32, int32_t, INT32_MIN, INT32_MAX)
FORJA_RT_DEV_AS_DE_F64(u8, uint8_t, 0, UINT8_MAX)
FORJA_RT_DEV_AS_DE_F64(u16, uint16_t, 0, UINT16_MAX)
FORJA_RT_DEV_AS_DE_F64(u32, uint32_t, 0, UINT32_MAX)

/* real -> i64: faixa [-2^63, 2^63), que e o que o (int64_t) aceita sem estouro. NaN cai fora. */
static inline int64_t forja_as_i64_de_f64(double v, const char *arquivo, int linha)
{
    if (!(v >= -0x1p63 && v < 0x1p63))
        forja_falha_conversao_f(v, "i64", arquivo, linha);
    return (int64_t)v;
}

/* real -> u64: faixa (-1, 2^64); quem esta em (-1, 0) trunca para 0 e cabe. NaN cai fora. */
static inline uint64_t forja_as_u64_de_f64(double v, const char *arquivo, int linha)
{
    if (!(v > -1.0 && v < 0x1p64))
        forja_falha_conversao_f(v, "u64", arquivo, linha);
    return (uint64_t)v;
}

#else

/* Release: conta crua com wrap definido (aritmetica sem sinal). Nao checa nada. */
#define forja_add_i32(a, b, arquivo, linha) ((int32_t)((uint32_t)(a) + (uint32_t)(b)))
#define forja_sub_i32(a, b, arquivo, linha) ((int32_t)((uint32_t)(a) - (uint32_t)(b)))
#define forja_mul_i32(a, b, arquivo, linha) ((int32_t)((uint32_t)(a) * (uint32_t)(b)))
#define forja_add_i64(a, b, arquivo, linha) ((int64_t)((uint64_t)(a) + (uint64_t)(b)))
#define forja_sub_i64(a, b, arquivo, linha) ((int64_t)((uint64_t)(a) - (uint64_t)(b)))
#define forja_mul_i64(a, b, arquivo, linha) ((int64_t)((uint64_t)(a) * (uint64_t)(b)))
#define forja_faixa(valor, lo, hi, exclusiva, campo, arquivo, linha) ((int64_t)(valor))
#define forja_faixa_u64(valor, lo, hi, exclusiva, campo, arquivo, linha) ((uint64_t)(valor))

/* Release dos inteiros de 8, 16 e 32 bits: a conta sai em uint32_t e o cast de volta ao tipo trunca
 * em modulo 2^N, que e o wrap pedido. Mesma ideia do i32, sem checagem. */
#define forja_add_i8(a, b, arquivo, linha) ((int8_t)((uint32_t)(a) + (uint32_t)(b)))
#define forja_sub_i8(a, b, arquivo, linha) ((int8_t)((uint32_t)(a) - (uint32_t)(b)))
#define forja_mul_i8(a, b, arquivo, linha) ((int8_t)((uint32_t)(a) * (uint32_t)(b)))
#define forja_add_i16(a, b, arquivo, linha) ((int16_t)((uint32_t)(a) + (uint32_t)(b)))
#define forja_sub_i16(a, b, arquivo, linha) ((int16_t)((uint32_t)(a) - (uint32_t)(b)))
#define forja_mul_i16(a, b, arquivo, linha) ((int16_t)((uint32_t)(a) * (uint32_t)(b)))
#define forja_add_u8(a, b, arquivo, linha) ((uint8_t)((uint32_t)(a) + (uint32_t)(b)))
#define forja_sub_u8(a, b, arquivo, linha) ((uint8_t)((uint32_t)(a) - (uint32_t)(b)))
#define forja_mul_u8(a, b, arquivo, linha) ((uint8_t)((uint32_t)(a) * (uint32_t)(b)))
#define forja_add_u16(a, b, arquivo, linha) ((uint16_t)((uint32_t)(a) + (uint32_t)(b)))
#define forja_sub_u16(a, b, arquivo, linha) ((uint16_t)((uint32_t)(a) - (uint32_t)(b)))
#define forja_mul_u16(a, b, arquivo, linha) ((uint16_t)((uint32_t)(a) * (uint32_t)(b)))
#define forja_add_u32(a, b, arquivo, linha) ((uint32_t)((uint32_t)(a) + (uint32_t)(b)))
#define forja_sub_u32(a, b, arquivo, linha) ((uint32_t)((uint32_t)(a) - (uint32_t)(b)))
#define forja_mul_u32(a, b, arquivo, linha) ((uint32_t)((uint32_t)(a) * (uint32_t)(b)))
#define forja_add_u64(a, b, arquivo, linha) ((uint64_t)((uint64_t)(a) + (uint64_t)(b)))
#define forja_sub_u64(a, b, arquivo, linha) ((uint64_t)((uint64_t)(a) - (uint64_t)(b)))
#define forja_mul_u64(a, b, arquivo, linha) ((uint64_t)((uint64_t)(a) * (uint64_t)(b)))

/* Release: divisor 0 devolve 0 e INT_MIN/-1 da wrap, sem trap do hardware (SIGFPE). */
static inline int32_t forja_rt_div_i32_cru(int32_t a, int32_t b)
{
    if (b == 0)
        return 0;
    if (b == -1)
        return (int32_t)(0u - (uint32_t)a);
    return a / b;
}

/* Release: resto por 0 ou por -1 devolve 0 (INT_MIN%-1 e 0 matematicamente). */
static inline int32_t forja_rt_rem_i32_cru(int32_t a, int32_t b)
{
    if (b == 0 || b == -1)
        return 0;
    return a % b;
}

/* Release: divisao i64 com a mesma regra de forja_rt_div_i32_cru. */
static inline int64_t forja_rt_div_i64_cru(int64_t a, int64_t b)
{
    if (b == 0)
        return 0;
    if (b == -1)
        return (int64_t)(0ull - (uint64_t)a);
    return a / b;
}

/* Release: resto i64 com a mesma regra de forja_rt_rem_i32_cru. */
static inline int64_t forja_rt_rem_i64_cru(int64_t a, int64_t b)
{
    if (b == 0 || b == -1)
        return 0;
    return a % b;
}

#define forja_div_i32(a, b, arquivo, linha) forja_rt_div_i32_cru((a), (b))
#define forja_rem_i32(a, b, arquivo, linha) forja_rt_rem_i32_cru((a), (b))
#define forja_div_i64(a, b, arquivo, linha) forja_rt_div_i64_cru((a), (b))
#define forja_rem_i64(a, b, arquivo, linha) forja_rt_rem_i64_cru((a), (b))

/* Release: divisao i8/i16 com a mesma regra de forja_rt_div_i32_cru; MIN/-1 da wrap sem trap. */
static inline int8_t forja_rt_div_i8_cru(int8_t a, int8_t b)
{
    if (b == 0)
        return 0;
    if (b == -1)
        return (int8_t)(0u - (uint32_t)a);
    return (int8_t)(a / b);
}

static inline int16_t forja_rt_div_i16_cru(int16_t a, int16_t b)
{
    if (b == 0)
        return 0;
    if (b == -1)
        return (int16_t)(0u - (uint32_t)a);
    return (int16_t)(a / b);
}

#define forja_div_i8(a, b, arquivo, linha) forja_rt_div_i8_cru((a), (b))
#define forja_div_i16(a, b, arquivo, linha) forja_rt_div_i16_cru((a), (b))

/* Release: conversao inteiro -> inteiro e so o cast, com wrap em modulo 2^N. */
#define forja_as_i8_de_i64(v, arquivo, linha) ((int8_t)(v))
#define forja_as_i16_de_i64(v, arquivo, linha) ((int16_t)(v))
#define forja_as_i32_de_i64(v, arquivo, linha) ((int32_t)(v))
#define forja_as_u8_de_i64(v, arquivo, linha) ((uint8_t)(v))
#define forja_as_u16_de_i64(v, arquivo, linha) ((uint16_t)(v))
#define forja_as_u32_de_i64(v, arquivo, linha) ((uint32_t)(v))
#define forja_as_u64_de_i64(v, arquivo, linha) ((uint64_t)(v))
#define forja_as_i8_de_u64(v, arquivo, linha) ((int8_t)(v))
#define forja_as_i16_de_u64(v, arquivo, linha) ((int16_t)(v))
#define forja_as_i32_de_u64(v, arquivo, linha) ((int32_t)(v))
#define forja_as_i64_de_u64(v, arquivo, linha) ((int64_t)(v))
#define forja_as_u8_de_u64(v, arquivo, linha) ((uint8_t)(v))
#define forja_as_u16_de_u64(v, arquivo, linha) ((uint16_t)(v))
#define forja_as_u32_de_u64(v, arquivo, linha) ((uint32_t)(v))

/* Release: real -> inteiro SATURA em vez de falhar. Ordem das comparacoes: a ultima devolve 0,
 * e so NaN chega la, porque NaN falsifica toda comparacao. */
#define FORJA_RT_REL_AS_DE_F64_SINAL(S, T, MIN, MAX) \
static inline T forja_as_##S##_de_f64(double v, const char *arquivo, int linha) \
{ \
    (void)arquivo; \
    (void)linha; \
    if (v > (double)(MAX)) \
        return (MAX); \
    if (v >= (double)(MIN)) \
        return (T)v; \
    if (v < (double)(MIN)) \
        return (MIN); \
    return 0; \
}

/* Release: real -> inteiro sem sinal satura: abaixo de 0 vira 0 (inclui negativos e NaN). */
#define FORJA_RT_REL_AS_DE_F64_SEM_SINAL(S, T, MAX) \
static inline T forja_as_##S##_de_f64(double v, const char *arquivo, int linha) \
{ \
    (void)arquivo; \
    (void)linha; \
    if (v > (double)(MAX)) \
        return (MAX); \
    if (v >= 0.0) \
        return (T)v; \
    return 0; \
}

FORJA_RT_REL_AS_DE_F64_SINAL(i8, int8_t, INT8_MIN, INT8_MAX)
FORJA_RT_REL_AS_DE_F64_SINAL(i16, int16_t, INT16_MIN, INT16_MAX)
FORJA_RT_REL_AS_DE_F64_SINAL(i32, int32_t, INT32_MIN, INT32_MAX)
FORJA_RT_REL_AS_DE_F64_SEM_SINAL(u8, uint8_t, UINT8_MAX)
FORJA_RT_REL_AS_DE_F64_SEM_SINAL(u16, uint16_t, UINT16_MAX)
FORJA_RT_REL_AS_DE_F64_SEM_SINAL(u32, uint32_t, UINT32_MAX)

/* Release: real -> i64 satura nas faixas [-2^63, 2^63); NaN devolve 0. */
static inline int64_t forja_as_i64_de_f64(double v, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (v >= 0x1p63)
        return INT64_MAX;
    if (v >= -0x1p63)
        return (int64_t)v;
    if (v < -0x1p63)
        return INT64_MIN;
    return 0;
}

/* Release: real -> u64 satura: acima de 2^64 vira UINT64_MAX, abaixo de 0 vira 0 e NaN devolve 0. */
static inline uint64_t forja_as_u64_de_f64(double v, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (v >= 0x1p64)
        return UINT64_MAX;
    if (v >= 0.0)
        return (uint64_t)v;
    return 0;
}

#endif

/* Resto i8/i16 igual nos dois modos: divisor 0 ou -1 devolve 0 (a % -1 e 0 e nao ha trap). */
static inline int8_t forja_rem_i8(int8_t a, int8_t b, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (b == 0 || b == -1)
        return 0;
    return (int8_t)(a % b);
}

static inline int16_t forja_rem_i16(int16_t a, int16_t b, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (b == 0 || b == -1)
        return 0;
    return (int16_t)(a % b);
}

/* Gera forja_div_S e forja_rem_S sem sinal: divisor 0 devolve 0 nos dois modos, sem erro. */
#define FORJA_RT_DIV_REM_SEM_SINAL(S, T) \
static inline T forja_div_##S(T a, T b, const char *arquivo, int linha) \
{ \
    (void)arquivo; \
    (void)linha; \
    if (b == 0) \
        return 0; \
    return (T)(a / b); \
} \
static inline T forja_rem_##S(T a, T b, const char *arquivo, int linha) \
{ \
    (void)arquivo; \
    (void)linha; \
    if (b == 0) \
        return 0; \
    return (T)(a % b); \
}

FORJA_RT_DIV_REM_SEM_SINAL(u8, uint8_t)
FORJA_RT_DIV_REM_SEM_SINAL(u16, uint16_t)
FORJA_RT_DIV_REM_SEM_SINAL(u32, uint32_t)
FORJA_RT_DIV_REM_SEM_SINAL(u64, uint64_t)

/* Gera forja_shl_S e forja_shr_S sem sinal. Deslocar >= largura da 0, sem trap, nos dois modos;
 * o bit que sai pela esquerda some e nao e estouro. A conta e feita em U (u8 e u16 usam uint32_t)
 * para o shift nao ser feito no tipo estreito, que o C promove para int antes do deslocamento. */
#define FORJA_RT_DESLOCA_SEM_SINAL(S, T, U, LARGURA) \
static inline T forja_shl_##S(T a, T b) \
{ \
    if (b >= (LARGURA)) \
        return 0; \
    return (T)((U)a << b); \
} \
static inline T forja_shr_##S(T a, T b) \
{ \
    if (b >= (LARGURA)) \
        return 0; \
    return (T)((U)a >> b); \
}

FORJA_RT_DESLOCA_SEM_SINAL(u8, uint8_t, uint32_t, 8)
FORJA_RT_DESLOCA_SEM_SINAL(u16, uint16_t, uint32_t, 16)
FORJA_RT_DESLOCA_SEM_SINAL(u32, uint32_t, uint32_t, 32)
FORJA_RT_DESLOCA_SEM_SINAL(u64, uint64_t, uint64_t, 64)

/* Falha deliberada do programa: R0003. Vale em DEV e em release. */
_Noreturn static inline void forja_panic(const char *msg, const char *arquivo, int linha)
{
    forja_falha(FORJA_R0003, msg, arquivo, linha);
}

/* 'proc main() -> ! E' que devolveu erro: escreve R0004 em stderr e volta; o main gerado sai com 1 (o erro e do programa, nao um panic). */
static inline void forja_main_erro(const char *arquivo, int erro)
{
    forja_rt_sem_relatorio = true;
    fputs("{\"codigo\": \"R0004\", \"tipo\": \"runtime\", \"arquivo\": \"", stderr);
    forja_json_texto(arquivo);
    fprintf(stderr, "\", \"linha\": 0, \"causa\": \"main devolveu o erro de indice %d\"}\n", erro);
}

/* assert do programa: cond falsa => R0003. Vale em DEV e em release. */
static inline void forja_assert(bool cond, const char *msg, const char *arquivo, int linha)
{
    if (!cond)
        forja_falha(FORJA_R0003, msg, arquivo, linha);
}

/* expect: dentro de teste registra a falha e segue; fora de teste vira forja_panic. */
static inline void forja_expect(bool cond, const char *texto_cond, const char *arquivo, int linha)
{
    char causa[256];

    if (cond)
        return;
    snprintf(causa, sizeof(causa), "expect falhou: %s", texto_cond);
    if (!forja_rt_em_teste)
        forja_panic(causa, arquivo, linha);
    forja_rt_teste_falhou = true;
    forja_rt_anotar_falha(FORJA_R0003, causa, arquivo, linha);
    fprintf(stderr, "  expect falhou: %s (%s:%d)\n", texto_cond, arquivo, linha);
}

/* Registra um teste para forja_testes_rodar. Estouro da tabela e falha interna (R0003). */
static inline void forja_teste_registrar(const char *nome, ForjaTesteFn fn)
{
    if (forja_rt_testes_total >= FORJA_TESTES_MAX)
        forja_falha(FORJA_R0003, "limite de testes excedido (FORJA_TESTES_MAX)", __FILE__, __LINE__);
    forja_rt_testes[forja_rt_testes_total].nome = nome;
    forja_rt_testes[forja_rt_testes_total].fn = fn;
    forja_rt_testes_total++;
}

/* Definidas com o rastreador de heap (mais abaixo; em release nao rastreiam nada). forja_rt_teste_vazou: blocos alocados no teste e vivos no fim dele (L16b). */
static inline int64_t forja_rt_vivos_qtd(void);
static inline bool forja_rt_teste_vazou(int64_t vivos_antes);

/* Roda um teste. noinline: o setjmp fica isolado do laco do chamador (evita -Wclobbered). Teste que passou e deixou bloco de heap vivo falha (dev, R0009). */
static __attribute__((noinline)) bool forja_rt_executar(void)
{
    forja_rt_teste_falhou = false;
    forja_rt_falha_anotada = false;
    forja_rt_queda_no_teste = false;
    forja_rt_teste_vivos_antes = forja_rt_vivos_qtd();
    forja_rt_em_teste = true;
    if (setjmp(forja_rt_salto) == 0)
        forja_rt_testes[forja_rt_teste_atual].fn();
    forja_rt_em_teste = false;
    if (!forja_rt_teste_falhou && forja_rt_teste_vazou(forja_rt_teste_vivos_antes))
        forja_rt_teste_falhou = true;
    return forja_rt_teste_falhou;
}

#if defined(_WIN32)
/* Queda por teste (L16b): o pedaco do EXCEPTION_POINTERS que interessa (o EXCEPTION_RECORD comeca pelo codigo) e AddVectoredExceptionHandler de kernel32, sem windows.h
 * (que colidiria com nomes do programa); o rotulo liga o nome interno ao simbolo real. */
typedef struct { unsigned long codigo; } ForjaRegistroExcecao;
typedef struct { ForjaRegistroExcecao *registro; void *contexto; } ForjaInfoExcecao;
void *forja_veh_adicionar(unsigned long primeiro, long (*tratador)(ForjaInfoExcecao *)) FORJA_ROTULO_C("AddVectoredExceptionHandler");

/* Acesso a memoria invalida (0xC0000005) ou instrucao ilegal (0xC000001D); outra excecao, NULL. */
static inline const char *forja_rt_queda_motivo(unsigned long codigo)
{
    if (codigo == 0xC0000005UL)
        return "acesso a memoria invalida";
    if (codigo == 0xC000001DUL)
        return "instrucao ilegal";
    return NULL;
}

/* Tratador: dentro de teste a 1a queda vira R0011 (o forja_falha salta para o executor, o teste falha, o proximo roda); 0 = seguir a busca (o processo cai como antes). */
static inline long forja_rt_queda_tratar(ForjaInfoExcecao *info)
{
    const char *motivo = forja_rt_queda_motivo(info->registro->codigo);
    const char *titulo = forja_rt_testes[forja_rt_teste_atual].nome;
    char causa[256];

    if (!forja_rt_em_teste || forja_rt_queda_no_teste || motivo == NULL)
        return 0;
    forja_rt_queda_no_teste = true;
    snprintf(causa, sizeof(causa), "o teste \"%s\" causou uma excecao do processador: %s", titulo != NULL ? titulo : "", motivo);
    forja_falha(FORJA_R0011, causa, "", 0);
}

static inline void forja_rt_queda_ligar(void)
{
    (void)forja_veh_adicionar(1, forja_rt_queda_tratar);
}
#else
static inline void forja_rt_queda_ligar(void)
{
}
#endif

/* forja test --filtro: o titulo contem o trecho (sensivel a caixa); trecho NULL ou vazio casa tudo. */
static inline bool forja_rt_titulo_casa(const char *titulo, const char *trecho)
{
    size_t k;

    if (trecho == NULL || trecho[0] == '\0')
        return true;
    for (; *titulo != '\0'; titulo++) {
        k = 0;
        while (trecho[k] != '\0' && titulo[k] == trecho[k])
            k++;
        if (trecho[k] == '\0')
            return true;
    }
    return false;
}

/* forja test --json: o driver liga FORJA_TESTE_JSON=1 no ambiente do exe. */
static inline bool forja_rt_json_pedido(void)
{
    const char *valor = getenv("FORJA_TESTE_JSON");

    return valor != NULL && valor[0] == '1' && valor[1] == '\0';
}

/* --json: {"teste": "<titulo>", "resultado": "ok"} ou, em falha, "resultado": "falhou" mais codigo, arquivo, linha e causa da 1a falha. */
static inline void forja_rt_teste_json(const char *titulo, bool falhou)
{
    fputs("{\"teste\": \"", stdout);
    forja_json_texto_em(stdout, titulo);
    if (!falhou) {
        fputs("\", \"resultado\": \"ok\"}\n", stdout);
        return;
    }
    fputs("\", \"resultado\": \"falhou\", \"codigo\": \"", stdout);
    forja_json_texto_em(stdout, forja_rt_falha_codigo);
    fputs("\", \"arquivo\": \"", stdout);
    forja_json_texto_em(stdout, forja_rt_falha_arquivo);
    printf("\", \"linha\": %d, \"causa\": \"", forja_rt_falha_linha);
    forja_json_texto_em(stdout, forja_rt_falha_causa);
    fputs("\"}\n", stdout);
}

/* Uma linha por teste: "ok   <titulo>" / "FALHOU <titulo>" ou o JSON do --json. */
static inline void forja_rt_teste_saida(bool json, const char *titulo, bool falhou)
{
    if (json)
        forja_rt_teste_json(titulo, falhou);
    else
        printf("%s%s\n", falhou ? "FALHOU " : "ok   ", titulo);
}

/* Resumo: "<rodados> testes, <f> falharam" (com ", <k> filtrados" so quando o filtro tirou algum) ou {"resumo": {...}} com total = todos os registrados. */
static inline void forja_rt_teste_resumo(bool json, int filtrados, int falhas)
{
    int rodados = forja_rt_testes_total - filtrados;

    if (json) {
        printf("{\"resumo\": {\"total\": %d, \"ok\": %d, \"falhou\": %d, \"filtrados\": %d}}\n",
               forja_rt_testes_total, rodados - falhas, falhas, filtrados);
        return;
    }
    printf("%d testes, %d falharam", rodados, falhas);
    if (filtrados > 0)
        printf(", %d filtrados", filtrados);
    printf("\n");
}

/* Roda os testes registrados (todos, ou so os do FORJA_TESTE_FILTRO). Imprime ok/FALHOU por teste e o resumo. Devolve 0 se tudo passou, 1 se algum falhou. */
static inline int forja_testes_rodar(void)
{
    const char *filtro = getenv("FORJA_TESTE_FILTRO");
    bool json = forja_rt_json_pedido();
    int falhas = 0;
    int filtrados = 0;
    int i;

    forja_rt_queda_ligar();
    for (i = 0; i < forja_rt_testes_total; i++) {
        if (!forja_rt_titulo_casa(forja_rt_testes[i].nome, filtro)) {
            filtrados++;
            continue;
        }
        forja_rt_teste_atual = i;
        bool falhou = forja_rt_executar();
        falhas += falhou ? 1 : 0;
        forja_rt_teste_saida(json, forja_rt_testes[i].nome, falhou);
    }
    forja_rt_teste_resumo(json, filtrados, falhas);
    if (falhas != 0)
        forja_rt_sem_relatorio = true;
    return falhas == 0 ? 0 : 1;
}

/* ---- v1.6: alocacao e listas ---- */
#define FORJA_R0007 "R0007"
#define FORJA_R0008 "R0008"
#ifndef FORJA_FRAME_BYTES
#define FORJA_FRAME_BYTES ((int64_t)16 * 1024 * 1024)
#endif
#define FORJA_ALOC_HEAP 1u
#define FORJA_ALOC_FRAME 2u
#define FORJA_MARCA_VIVA 0x4A524F4656495641ull
#define FORJA_MARCA_MORTA 0x4A524F464D4F5254ull

typedef struct { unsigned char *base; int64_t topo; uint32_t epoca; } ForjaFrame;
static FORJA_FIO ForjaFrame forja_rt_frame = {NULL, 0, 1};

_Noreturn static inline void forja_falha_alocacao(const char *oque, long long valor, const char *arquivo, int linha)
{
    char causa[256];
    snprintf(causa, sizeof(causa), "%s (%lld)", oque, valor);
    forja_falha(FORJA_R0007, causa, arquivo, linha);
}

static inline bool forja_no_frame(const void *p)
{
    uintptr_t u = (uintptr_t)p, b = (uintptr_t)forja_rt_frame.base;
    return forja_rt_frame.base != NULL && u >= b && u < b + (uintptr_t)FORJA_FRAME_BYTES;
}

static inline void *forja_frame_alocar(int64_t bytes, const char *arquivo, int linha)
{
    int64_t alinhado;
    void *p;
    if (bytes > FORJA_FRAME_BYTES)
        forja_falha_alocacao("frame cheio: pedido maior que o frame inteiro em bytes", bytes, arquivo, linha);
    alinhado = (bytes + 15) & ~(int64_t)15;
    if (forja_rt_frame.base == NULL) {
        forja_rt_frame.base = (unsigned char *)malloc((size_t)FORJA_FRAME_BYTES);
        if (forja_rt_frame.base == NULL)
            forja_falha_alocacao("sem memoria para o frame em bytes", FORJA_FRAME_BYTES, arquivo, linha);
        FORJA_FIO_GUARDAR_FRAME(forja_rt_frame.base);
    }
    if (alinhado > FORJA_FRAME_BYTES - forja_rt_frame.topo)
        forja_falha_alocacao("frame cheio: chame frame_reset() a cada quadro; bytes pedidos", bytes, arquivo, linha);
    p = forja_rt_frame.base + forja_rt_frame.topo;
    forja_rt_frame.topo += alinhado;
    return p;
}

static inline void *forja_frame_realocar(void *velho, int64_t bytes_velho, int64_t bytes_novo, const char *arquivo, int linha)
{
    void *novo = forja_frame_alocar(bytes_novo, arquivo, linha);
    if (velho != NULL && bytes_velho > 0)
        __builtin_memcpy(novo, velho, (size_t)bytes_velho);
    return novo;
}

#if defined(FORJA_DEV)
#define FORJA_MARCA_ARENA 0x4A524F4641524E41ull
#define FORJA_MARCA_POOL 0x4A524F46504F4F4Cull
#define FORJA_VAZAMENTOS_MAX 16

/* Cabecalho de cada bloco de heap em DEV: 48 B (mantem o alinhamento de 16). Os blocos vivos formam uma lista (mais antigo primeiro). */
typedef struct ForjaCabecalho {
    uint64_t marca;
    int64_t bytes;
    const char *arquivo;
    int32_t linha;
    uint32_t alocador;
    struct ForjaCabecalho *ant, *prox;
} ForjaCabecalho;
_Static_assert(sizeof(ForjaCabecalho) == 48, "ForjaCabecalho tem de ter 48 bytes (alinhamento de 16)");

static ForjaCabecalho *forja_rt_heap_cabeca = NULL;
static ForjaCabecalho *forja_rt_heap_cauda = NULL;
static int64_t forja_rt_vivos_n = 0;
static int64_t forja_rt_vivos_bytes = 0; /* P71a: soma de bytes dos vivos (snapshot de std:mem), sob forja_rt_trava_vivos */
static uint32_t forja_rt_serie_prox = 0; /* P71a: serie do proximo bloco a nascer; guardada no campo `alocador` do bloco VIVO de heap (ninguem o le), da a volta em 2^32 */
static bool forja_rt_atexit_ok = false;

static inline int64_t forja_rt_vivos_qtd(void)
{
    return forja_rt_vivos_n;
}

typedef struct { const char *arquivo; int32_t linha; int64_t blocos, bytes; } ForjaVazamento;
typedef struct { ForjaVazamento pontos[FORJA_VAZAMENTOS_MAX]; int n; ForjaVazamento outros; } ForjaVazamentos;

/* Soma o bloco no ponto (arquivo, linha) que ja existe; ponto novo se couber; senao em "(outros)". */
static inline void forja_rt_somar_vazamento(ForjaVazamentos *v, const ForjaCabecalho *c)
{
    ForjaVazamento *p = NULL;
    int i;
    for (i = 0; i < v->n && p == NULL; i++)
        if (v->pontos[i].linha == c->linha && __builtin_strcmp(v->pontos[i].arquivo, c->arquivo) == 0)
            p = &v->pontos[i];
    if (p == NULL && v->n < FORJA_VAZAMENTOS_MAX) {
        p = &v->pontos[v->n++];
        p->arquivo = c->arquivo;
        p->linha = c->linha;
    }
    if (p == NULL)
        p = &v->outros;
    p->blocos++;
    p->bytes += c->bytes;
}

/* A causa do R0009: "vazamento: <blocos> blocos (<bytes> bytes) <sufixo>". */
static inline void forja_rt_causa_vazamento(char *destino, size_t n, const ForjaVazamento *v, const char *sufixo)
{
    snprintf(destino, n, "vazamento: %lld blocos (%lld bytes) %s", (long long)v->blocos, (long long)v->bytes, sufixo);
}

/* Uma linha R0009 no formato do forja_falha. */
static inline void forja_rt_linha_vazamento(const ForjaVazamento *v, const char *sufixo)
{
    char causa[256];
    forja_rt_causa_vazamento(causa, sizeof(causa), v, sufixo);
    fputs("{\"codigo\": \"R0009\", \"tipo\": \"runtime\", \"arquivo\": \"", stderr);
    forja_json_texto(v->arquivo);
    fprintf(stderr, "\", \"linha\": %d, \"causa\": \"", (int)v->linha);
    forja_json_texto(causa);
    fputs("\"}\n", stderr);
}

/* Uma linha por ponto de alocacao (sufixo_ponto) e a de "(outros)" se passou do limite. */
static inline void forja_rt_listar_vazamentos(ForjaVazamentos *v, const char *sufixo_ponto)
{
    int i;
    for (i = 0; i < v->n; i++)
        forja_rt_linha_vazamento(&v->pontos[i], sufixo_ponto);
    if (v->outros.blocos > 0) {
        v->outros.arquivo = "(outros)";
        forja_rt_linha_vazamento(&v->outros, "em outros pontos de alocacao");
    }
}

static void forja_rt_relatar_vazamentos(void);

/* ---- P71b: handles genericos de lib C (track_handle/untrack_handle de std:mem); o oraculo do rt_handles.c do runtime nativo: mesmas linhas R0009 "tipo": "handle" e R0008, sem "pilha".
 * kind copiado (ate 47 bytes); h nulo ignorado; track de h ja rastreado atualiza o kind; vivo no fim = R0009 + exit 101 no mesmo atexit do heap. */
typedef struct ForjaHandle { struct ForjaHandle *prox; const void *h; char kind[48]; } ForjaHandle;
static ForjaHandle *forja_rt_hand_cabeca = NULL; /* compartilhado: lista dos handles (mais antigo primeiro), sob forja_rt_trava_vivos */
static ForjaHandle *forja_rt_hand_cauda = NULL; /* compartilhado: idem forja_rt_hand_cabeca */

static inline void forja_handle_track(const char *kind, void *h)
{
    ForjaHandle *n;
    ForjaHandle *achado = NULL;
    int tomou;
    int i;

    if (h == NULL)
        return;
    n = (ForjaHandle *)calloc(1, sizeof *n);
    if (n == NULL)
        forja_falha_alocacao("sem memoria para o rastreio de handle em bytes", (long long)sizeof *n, "std:mem", 0);
    n->h = h;
    if (kind == NULL)
        kind = "handle";
    for (i = 0; i < 47 && kind[i] != '\0'; i++)
        n->kind[i] = kind[i];
    tomou = forja_trava_pegar(&forja_rt_trava_vivos);
    for (ForjaHandle *p = forja_rt_hand_cabeca; p != NULL && achado == NULL; p = p->prox)
        if (p->h == h)
            achado = p;
    if (achado != NULL) {
        __builtin_memcpy(achado->kind, n->kind, sizeof n->kind);
    } else {
        if (forja_rt_hand_cauda != NULL)
            forja_rt_hand_cauda->prox = n;
        else
            forja_rt_hand_cabeca = n;
        forja_rt_hand_cauda = n;
    }
    if (!forja_rt_atexit_ok) {
        forja_rt_atexit_ok = true;
        (void)atexit(forja_rt_relatar_vazamentos);
    }
    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
    if (achado != NULL)
        free(n);
}

static inline int forja_handle_tirar(void *h) /* tira h da lista; 0 se nao estava */
{
    ForjaHandle *ant = NULL;
    ForjaHandle *n;
    int tomou;
    int achou;

    tomou = forja_trava_pegar(&forja_rt_trava_vivos);
    for (n = forja_rt_hand_cabeca; n != NULL && n->h != h; n = n->prox)
        ant = n;
    if (n != NULL) {
        if (ant != NULL)
            ant->prox = n->prox;
        else
            forja_rt_hand_cabeca = n->prox;
        if (forja_rt_hand_cauda == n)
            forja_rt_hand_cauda = ant;
    }
    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
    achou = n != NULL;
    free(n);
    return achou;
}

static inline void forja_handle_untrack(void *h)
{
    if (h != NULL && !forja_handle_tirar(h))
        forja_falha(FORJA_R0008, "untrack_handle(h): o handle nao foi rastreado por track_handle ou ja passou por untrack_handle", "std:mem", 0);
}

/* M6c P88: foreign com clausula creates/destroys/takes (FORJA_HANDLE_NASCE/SOLTA do emit_c_rastreio.c). Nasce: h nulo ignorado, indireto le *h; solta de desconhecido = silencio. */
static inline void forja_handle_nasce(const char *kind, void *h, int indireto, const char *destruidor)
{
    (void)destruidor;
    if (indireto != 0 && h != NULL)
        h = *(void *const *)h;
    forja_handle_track(kind, h);
}

static inline void forja_handle_solta(void *h)
{
    if (h != NULL)
        (void)forja_handle_tirar(h);
}
#define FORJA_HANDLE_NASCE(kind, h, indireto, destruidor) forja_handle_nasce((kind), (void *)(uintptr_t)(h), (indireto), (destruidor))
#define FORJA_HANDLE_SOLTA(h) forja_handle_solta((void *)(uintptr_t)(h))

/* Uma linha R0009 por handle vivo (a mesma forma do runtime nativo, sem pilha); devolve quantos eram. */
static inline int forja_rt_handles_relatar(void)
{
    int vivos = 0;

    for (const ForjaHandle *n = forja_rt_hand_cabeca; n != NULL; n = n->prox) {
        char causa[160];

        snprintf(causa, sizeof causa, "vazamento: handle %s vivo no fim do programa: falta untrack_handle", n->kind);
        fputs("{\"codigo\": \"R0009\", \"tipo\": \"handle\", \"kind\": \"", stderr);
        forja_json_texto(n->kind);
        fputs("\", \"arquivo\": \"std:mem\", \"linha\": 0, \"causa\": \"", stderr);
        forja_json_texto(causa);
        fputs("\"}\n", stderr);
        vivos++;
    }
    return vivos;
}

/* atexit (so DEV): relata os blocos de heap que seguem vivos e os handles sem untrack e sai com 101; em biblioteca (DLL) so relata. */
static void forja_rt_relatar_vazamentos(void)
{
    ForjaVazamentos v;
    const ForjaCabecalho *c;
    int handles;
    if (forja_rt_sem_relatorio)
        return;
    __builtin_memset(&v, 0, sizeof v);
    for (c = forja_rt_heap_cabeca; c != NULL; c = c->prox)
        forja_rt_somar_vazamento(&v, c);
    forja_rt_listar_vazamentos(&v, "alocados aqui seguem vivos no fim do programa");
    handles = forja_rt_handles_relatar();
    if (forja_rt_heap_cabeca == NULL && handles == 0)
        return;
    fflush(NULL);
#if !defined(FORJA_BIBLIOTECA)
    _Exit(101);
#endif
}

static inline void forja_heap_encadear(ForjaCabecalho *c)
{
    int tomou = forja_trava_pegar(&forja_rt_trava_vivos);

    c->prox = NULL;
    c->ant = forja_rt_heap_cauda;
    if (forja_rt_heap_cauda != NULL)
        forja_rt_heap_cauda->prox = c;
    else
        forja_rt_heap_cabeca = c;
    forja_rt_heap_cauda = c;
    forja_rt_vivos_n++;
    forja_rt_vivos_bytes += c->bytes;
    c->alocador = forja_rt_serie_prox++;
    if (!forja_rt_atexit_ok) {
        forja_rt_atexit_ok = true;
        (void)atexit(forja_rt_relatar_vazamentos);
    }
    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
}

static inline void forja_heap_desencadear(ForjaCabecalho *c)
{
    int tomou = forja_trava_pegar(&forja_rt_trava_vivos);

    if (c->ant != NULL)
        c->ant->prox = c->prox;
    else
        forja_rt_heap_cabeca = c->prox;
    if (c->prox != NULL)
        c->prox->ant = c->ant;
    else
        forja_rt_heap_cauda = c->ant;
    forja_rt_vivos_n--;
    forja_rt_vivos_bytes -= c->bytes;
    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
}

/* O mais antigo dos 'novos' ultimos blocos da lista (os alocados no teste ficam no fim dela); NULL se a lista tem menos que isso. */
static inline ForjaCabecalho *forja_rt_teste_primeiro_novo(int64_t novos)
{
    ForjaCabecalho *c = forja_rt_heap_cauda;
    int64_t k;
    for (k = 1; k < novos && c != NULL; k++)
        c = c->ant;
    return c;
}

/* Tira da lista (sem liberar) os blocos de 'c' em diante: o rastreador os esquece e o R0009 do fim do programa nao os repete. */
static inline void forja_rt_teste_esquecer(ForjaCabecalho *c)
{
    ForjaCabecalho *prox;
    for (; c != NULL; c = prox) {
        prox = c->prox;
        forja_heap_desencadear(c);
    }
}

/* L16b: fim de um teste que passou. Os blocos vivos alem dos 'vivos_antes' foram alocados nele e vazaram (a conta e por contagem: um teste que libera bloco de fora e vaza
 * outro em igual numero nao e visto). O teste falha com uma linha R0009 por ponto de alocacao; a 1a vira a falha do --json. Devolve true se vazou. */
static inline bool forja_rt_teste_vazou(int64_t vivos_antes)
{
    int64_t novos = forja_rt_vivos_n - vivos_antes;
    ForjaCabecalho *primeiro = novos > 0 ? forja_rt_teste_primeiro_novo(novos) : NULL;
    const ForjaCabecalho *c;
    ForjaVazamentos v;
    char causa[256];
    if (primeiro == NULL)
        return false;
    __builtin_memset(&v, 0, sizeof v);
    for (c = primeiro; c != NULL; c = c->prox)
        forja_rt_somar_vazamento(&v, c);
    forja_rt_causa_vazamento(causa, sizeof(causa), &v.pontos[0], "alocados aqui seguem vivos no fim do teste");
    forja_rt_anotar_falha(FORJA_R0009, causa, v.pontos[0].arquivo, (int)v.pontos[0].linha);
    forja_rt_listar_vazamentos(&v, "alocados aqui seguem vivos no fim do teste");
    forja_rt_teste_esquecer(primeiro);
    return true;
}

/* Depois de um realloc que pode ter movido o bloco: os vizinhos e a cabeca/cauda passam a apontar para o endereco novo. */
static inline void forja_heap_reencadear(ForjaCabecalho *n)
{
    if (n->ant != NULL)
        n->ant->prox = n;
    else
        forja_rt_heap_cabeca = n;
    if (n->prox != NULL)
        n->prox->ant = n;
    else
        forja_rt_heap_cauda = n;
}

static inline void *forja_heap_alocar(int64_t bytes, const char *arquivo, int linha)
{
    ForjaCabecalho *c;
    if (bytes > INT64_MAX - (int64_t)sizeof(ForjaCabecalho))
        forja_falha_alocacao("sem memoria no heap para bytes", bytes, arquivo, linha);
    c = (ForjaCabecalho *)malloc((size_t)bytes + sizeof(ForjaCabecalho));
    if (c == NULL)
        forja_falha_alocacao("sem memoria no heap para bytes", bytes, arquivo, linha);
    c->marca = FORJA_MARCA_VIVA;
    c->bytes = bytes;
    c->arquivo = arquivo != NULL ? arquivo : "";
    c->linha = (int32_t)linha;
    c->alocador = FORJA_ALOC_HEAP;
    forja_heap_encadear(c); /* registra tambem o atexit do relatorio (1x, sob a trava) */
    return c + 1;
}

static inline ForjaCabecalho *forja_heap_cabecalho_vivo(void *p, const char *oque, const char *arquivo, int linha)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;
    char causa[256];
    if (c->marca == FORJA_MARCA_VIVA)
        return c;
    if (c->marca == FORJA_MARCA_ARENA)
        snprintf(causa, sizeof(causa), "%s: objeto de Arena: arena_reset ou arena_destroy liberam tudo", oque);
    else if (c->marca == FORJA_MARCA_POOL)
        snprintf(causa, sizeof(causa), "%s: objeto de Pool: use delete(p, em = pool)", oque);
    else
        snprintf(causa, sizeof(causa), "%s: o ponteiro nao veio de new(em = heap) ou ja passou por delete", oque);
    forja_falha(FORJA_R0008, causa, arquivo, linha);
}

static inline void *forja_heap_realocar(void *velho, int64_t bytes_novo, const char *arquivo, int linha)
{
    ForjaCabecalho *c, *n;
    int tomou;
    if (velho == NULL)
        return forja_heap_alocar(bytes_novo, arquivo, linha);
    c = forja_heap_cabecalho_vivo(velho, "append", arquivo, linha);
    if (bytes_novo > INT64_MAX - (int64_t)sizeof(ForjaCabecalho))
        forja_falha_alocacao("sem memoria no heap para bytes", bytes_novo, arquivo, linha);
    tomou = forja_trava_pegar(&forja_rt_trava_vivos); /* o realloc pode mover o bloco: a lista nao pode ser lida por outra thread nesse meio tempo */
    n = (ForjaCabecalho *)realloc(c, (size_t)bytes_novo + sizeof(ForjaCabecalho));
    if (n == NULL)
        forja_falha_alocacao("sem memoria no heap para bytes", bytes_novo, arquivo, linha);
    forja_rt_vivos_bytes += bytes_novo - n->bytes;
    n->bytes = bytes_novo;
    forja_heap_reencadear(n);
    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
    return n + 1;
}

static inline void forja_mem_liberar(void *p, const char *arquivo, int linha)
{
    ForjaCabecalho *c;
    if (p == NULL || forja_no_frame(p))
        return;
    c = forja_heap_cabecalho_vivo(p, "delete", arquivo, linha);
    forja_heap_desencadear(c);
    *(volatile uint64_t *)&c->marca = FORJA_MARCA_MORTA; /* volatile: sem isso o gcc -O2 tira a escrita morta antes do free */
    __builtin_memset(p, 0xDD, (size_t)c->bytes);
    free(c);
}

/* ---- P71a: snapshot e report_since de std:mem (o oraculo do rt_rastreio.c do runtime nativo; mesma linha JSON, mas sem "pilha": o backend C so tem arquivo:linha) ---- */
#define FORJA_SNAP_GRUPOS_MAX 128
#define FORJA_SNAP_TOPO 20

static inline uint32_t forja_memsnap_serie(void)
{
    int tomou = forja_trava_pegar(&forja_rt_trava_vivos);
    uint32_t serie = forja_rt_serie_prox;

    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
    return serie;
}

static inline int64_t forja_memsnap_vivos(void)
{
    int tomou = forja_trava_pegar(&forja_rt_trava_vivos);
    int64_t n = forja_rt_vivos_n;

    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
    return n;
}

static inline int64_t forja_memsnap_bytes(void)
{
    int tomou = forja_trava_pegar(&forja_rt_trava_vivos);
    int64_t n = forja_rt_vivos_bytes;

    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
    return n;
}

typedef struct { ForjaVazamento pontos[FORJA_SNAP_GRUPOS_MAX]; int n; ForjaVazamento outros; } ForjaSnapGrupos;

/* Soma o bloco no grupo (arquivo, linha); grupo novo se couber; senao em "outros". */
static inline void forja_snap_somar(ForjaSnapGrupos *v, const ForjaCabecalho *c)
{
    ForjaVazamento *p = NULL;
    int i;

    for (i = 0; i < v->n && p == NULL; i++)
        if (v->pontos[i].linha == c->linha && __builtin_strcmp(v->pontos[i].arquivo, c->arquivo) == 0)
            p = &v->pontos[i];
    if (p == NULL && v->n < FORJA_SNAP_GRUPOS_MAX) {
        p = &v->pontos[v->n++];
        p->arquivo = c->arquivo;
        p->linha = c->linha;
    }
    if (p == NULL)
        p = &v->outros;
    p->blocos++;
    p->bytes += c->bytes;
}

/* Poe nas primeiras posicoes os 'topo' grupos de mais bytes (selecao parcial); devolve quantos. */
static inline int forja_snap_ordenar(ForjaSnapGrupos *v, int topo)
{
    int limite = v->n < topo ? v->n : topo;
    int i, j;

    for (i = 0; i < limite; i++) {
        int maior = i;
        ForjaVazamento troca;

        for (j = i + 1; j < v->n; j++)
            if (v->pontos[j].bytes > v->pontos[maior].bytes)
                maior = j;
        troca = v->pontos[i];
        v->pontos[i] = v->pontos[maior];
        v->pontos[maior] = troca;
    }
    return limite;
}

static inline void forja_memsnap_relatar(uint32_t serie, int64_t vivos_antes, int64_t bytes_antes)
{
    ForjaSnapGrupos *v = (ForjaSnapGrupos *)calloc(1, sizeof *v);
    const ForjaCabecalho *c;
    int64_t novos = 0, novos_bytes = 0, vivos_agora, bytes_agora;
    int topo, i;
    int tomou;

    if (v == NULL)
        forja_falha_alocacao("sem memoria para o relatorio de report_since em bytes", (int64_t)sizeof *v, "std:mem", 0);
    tomou = forja_trava_pegar(&forja_rt_trava_vivos);
    for (c = forja_rt_heap_cabeca; c != NULL; c = c->prox) {
        if ((int32_t)(c->alocador - serie) < 0)
            continue;
        forja_snap_somar(v, c);
        novos++;
        novos_bytes += c->bytes;
    }
    vivos_agora = forja_rt_vivos_n;
    bytes_agora = forja_rt_vivos_bytes;
    forja_trava_soltar(&forja_rt_trava_vivos, tomou);
    topo = forja_snap_ordenar(v, FORJA_SNAP_TOPO);
    for (i = topo; i < v->n; i++) {
        v->outros.blocos += v->pontos[i].blocos;
        v->outros.bytes += v->pontos[i].bytes;
    }
    fprintf(stderr, "{\"tipo\": \"mem_report\", \"vivos_antes\": %lld, \"bytes_antes\": %lld, \"vivos_agora\": %lld, \"bytes_agora\": %lld, \"novos_blocos\": %lld, \"novos_bytes\": %lld, \"grupos\": [",
            (long long)vivos_antes, (long long)bytes_antes, (long long)vivos_agora, (long long)bytes_agora, (long long)novos, (long long)novos_bytes);
    for (i = 0; i < topo; i++) {
        fputs(i == 0 ? "{\"arquivo\": \"" : ", {\"arquivo\": \"", stderr);
        forja_json_texto(v->pontos[i].arquivo);
        fprintf(stderr, "\", \"linha\": %d, \"blocos\": %lld, \"bytes\": %lld}", (int)v->pontos[i].linha, (long long)v->pontos[i].blocos, (long long)v->pontos[i].bytes);
    }
    fprintf(stderr, "], \"outros\": {\"blocos\": %lld, \"bytes\": %lld}}\n", (long long)v->outros.blocos, (long long)v->outros.bytes);
    free(v);
}

static inline void forja_lista_viva(uint32_t epoca, const char *arquivo, int linha)
{
    char causa[256];
    if (epoca == 0 || epoca == forja_rt_frame.epoca)
        return;
    snprintf(causa, sizeof(causa), "lista do frame usada depois do frame_reset() (quadro %u, agora %u)", (unsigned)epoca,
             (unsigned)forja_rt_frame.epoca);
    forja_falha(FORJA_R0008, causa, arquivo, linha);
}

static inline int64_t forja_indice_lista(int64_t i, int64_t n, uint32_t epoca, const char *arquivo, int linha)
{
    forja_lista_viva(epoca, arquivo, linha);
    return forja_indice(i, n, arquivo, linha);
}
#else
static inline int64_t forja_rt_vivos_qtd(void)
{
    return 0;
}

static inline bool forja_rt_teste_vazou(int64_t vivos_antes)
{
    (void)vivos_antes;
    return false;
}

static inline void *forja_heap_alocar(int64_t bytes, const char *arquivo, int linha)
{
    void *p = malloc((size_t)bytes);
    if (p == NULL)
        forja_falha_alocacao("sem memoria no heap para bytes", bytes, arquivo, linha);
    return p;
}

static inline void *forja_heap_realocar(void *velho, int64_t bytes_novo, const char *arquivo, int linha)
{
    void *p = realloc(velho, (size_t)bytes_novo);
    if (p == NULL)
        forja_falha_alocacao("sem memoria no heap para bytes", bytes_novo, arquivo, linha);
    return p;
}

static inline void forja_mem_liberar(void *p, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (p == NULL || forja_no_frame(p))
        return;
    free(p);
}

/* P71a: release nao rastreia; o backend C nao tira a chamada (so o x64 faz): os 4 simbolos de std:mem existem como no-op para o C compilar. */
static inline uint32_t forja_memsnap_serie(void) { return 0; }
static inline int64_t forja_memsnap_vivos(void) { return 0; }
static inline int64_t forja_memsnap_bytes(void) { return 0; }
static inline void forja_memsnap_relatar(uint32_t serie, int64_t vivos_antes, int64_t bytes_antes) { (void)serie; (void)vivos_antes; (void)bytes_antes; }
static inline void forja_handle_track(const char *kind, void *h) { (void)kind; (void)h; } /* P71b */
static inline void forja_handle_untrack(void *h) { (void)h; }
#define FORJA_HANDLE_NASCE(kind, h, indireto, destruidor) ((void)0) /* M6c P88: release nao rastreia */
#define FORJA_HANDLE_SOLTA(h) ((void)0)

#define forja_lista_viva(epoca, arquivo, linha) ((void)0)
#define forja_indice_lista(i, n, epoca, arquivo, linha) forja_indice(i, n, arquivo, linha) /* O0 P13: sem epoca (R0008 so no dev); o forja_indice confere salvo --checagem nao */
#endif

/* ---- v1.8 P5b: Arena e Pool. Handle = (geracao << 12) | idx; idx 0, 1 e 2 reservados (0 = Arena{} zerada, 1 = heap, 2 = frame). ---- */
#define FORJA_ALOCS_MAX 4096u
#define FORJA_ALOC_RESERVADOS 3u
#define FORJA_ALOC_GERACAO_MAX 0xFFFFFu
#define FORJA_TIPO_ARENA 1
#define FORJA_TIPO_POOL 2
#define FORJA_BLOCO_MIN 64
#define FORJA_ARQ_STD "std:mem"

typedef struct {
    uint32_t geracao;
    uint8_t tipo; /* 0 livre, 1 arena, 2 pool */
    int64_t bloco, usado, tam_slot, vivos, por_bloco;
    void *blocos, *livres;
} ForjaAlocador;

typedef struct ForjaBloco {
    struct ForjaBloco *prox;
    int64_t cap, topo, reservado;
} ForjaBloco;
_Static_assert(sizeof(ForjaBloco) == 32, "ForjaBloco tem de ter 32 bytes (alinhamento de 16)");

static ForjaAlocador *forja_rt_alocs = NULL;
static uint32_t forja_rt_alocs_n = 0;
static uint32_t forja_rt_alocs_cap = 0;

#if defined(FORJA_DEV)
#define FORJA_ALOC_EXTRA ((int64_t)sizeof(ForjaCabecalho))

/* p = endereco devolvido ao programa; o cabecalho de 48 B fica logo antes dele. */
static inline void forja_aloc_cabecalho(void *p, uint64_t marca, uint32_t h)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;
    c->marca = marca;
    c->alocador = h;
    c->ant = NULL;
    c->prox = NULL;
}

static inline void forja_aloc_origem(void *p, int64_t bytes, const char *arquivo, int linha)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;
    c->bytes = bytes;
    c->arquivo = arquivo != NULL ? arquivo : "";
    c->linha = (int32_t)linha;
}

static inline void forja_pool_conferir(uint32_t h, void *p, const char *arquivo, int linha)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;
    if (c->marca != FORJA_MARCA_POOL || c->alocador != h)
        forja_falha(FORJA_R0008, "delete(p, em = pool): o objeto nao veio deste Pool", arquivo, linha);
    __builtin_memset(p, 0xDD, (size_t)c->bytes);
    *(volatile uint64_t *)&c->marca = FORJA_MARCA_MORTA;
}
#else
#define FORJA_MARCA_ARENA 0x4A524F4641524E41ull
#define FORJA_MARCA_POOL 0x4A524F46504F4F4Cull
#define FORJA_ALOC_EXTRA ((int64_t)0)
#define forja_aloc_cabecalho(p, marca, h) ((void)(p), (void)(marca), (void)(h))
#define forja_aloc_origem(p, bytes, arquivo, linha) ((void)(p), (void)(bytes), (void)(arquivo), (void)(linha))
#define forja_pool_conferir(h, p, arquivo, linha) ((void)0)
#endif

static inline void forja_aloc_garantir(void)
{
    ForjaAlocador *novo;
    uint32_t cap;
    if (forja_rt_alocs_n < forja_rt_alocs_cap)
        return;
    cap = forja_rt_alocs_cap == 0 ? 8u : forja_rt_alocs_cap * 2u;
    novo = (ForjaAlocador *)realloc(forja_rt_alocs, (size_t)cap * sizeof(ForjaAlocador));
    if (novo == NULL)
        forja_falha_alocacao("sem memoria para o registro de alocadores em entradas", cap, FORJA_ARQ_STD, 0);
    __builtin_memset(novo + forja_rt_alocs_cap, 0, (size_t)(cap - forja_rt_alocs_cap) * sizeof(ForjaAlocador));
    forja_rt_alocs = novo;
    forja_rt_alocs_cap = cap;
}

/* Reserva uma entrada livre (a partir do idx 3) e devolve o handle; a geracao sobe a cada reuso e volta a 1. */
static inline uint32_t forja_aloc_registrar(uint8_t tipo, int64_t bloco)
{
    uint32_t i = FORJA_ALOC_RESERVADOS, ger;
    ForjaAlocador *a;
    int tomou = forja_trava_pegar(&forja_rt_trava_alocs);
    if (forja_rt_alocs_n == 0) {
        forja_aloc_garantir();
        forja_rt_alocs_n = FORJA_ALOC_RESERVADOS;
    }
    while (i < forja_rt_alocs_n && forja_rt_alocs[i].tipo != 0)
        i++;
    if (i == forja_rt_alocs_n) {
        if (forja_rt_alocs_n >= FORJA_ALOCS_MAX)
            forja_falha_alocacao("alocadores demais vivos ao mesmo tempo (limite)", FORJA_ALOCS_MAX, FORJA_ARQ_STD, 0);
        forja_aloc_garantir();
        forja_rt_alocs_n++;
    }
    a = &forja_rt_alocs[i];
    ger = a->geracao >= FORJA_ALOC_GERACAO_MAX ? 1u : a->geracao + 1u;
    *a = (ForjaAlocador){.geracao = ger, .tipo = tipo, .bloco = bloco};
    forja_trava_soltar(&forja_rt_trava_alocs, tomou);
    return (ger << 12) | i;
}

static inline uint32_t forja_aloc_handle(const ForjaAlocador *a)
{
    return (a->geracao << 12) | (uint32_t)(a - forja_rt_alocs);
}

static inline ForjaAlocador *forja_aloc_de(uint32_t h, const char *arquivo, int linha)
{
    uint32_t idx = h & 0xFFFu;
    ForjaAlocador *a;
    char causa[256];
    if (idx < FORJA_ALOC_RESERVADOS || idx >= forja_rt_alocs_n)
        forja_falha_alocacao("alocador desconhecido (id): crie com arena_create ou pool_create", (long long)h, arquivo, linha);
    a = &forja_rt_alocs[idx];
    if (a->tipo == 0 || a->geracao != (h >> 12)) {
        snprintf(causa, sizeof(causa), "Arena ou Pool usado depois do destroy (id %u)", (unsigned)h);
        forja_falha(FORJA_R0008, causa, arquivo, linha);
    }
    return a;
}

static inline ForjaBloco *forja_bloco_novo(ForjaBloco *prox, int64_t cap, const char *arquivo, int linha)
{
    ForjaBloco *b;
    if (cap < 0 || cap > INT64_MAX / 2)
        forja_falha_alocacao("sem memoria para o bloco do alocador em bytes", cap, arquivo, linha);
    b = (ForjaBloco *)malloc((size_t)cap + sizeof(ForjaBloco));
    if (b == NULL)
        forja_falha_alocacao("sem memoria para o bloco do alocador em bytes", cap, arquivo, linha);
    b->prox = prox;
    b->cap = cap;
    b->topo = 0;
    b->reservado = 0;
    return b;
}

static inline unsigned char *forja_bloco_dados(ForjaBloco *b)
{
    return (unsigned char *)(b + 1);
}

static inline void forja_blocos_soltar(ForjaBloco *b)
{
    ForjaBloco *prox;
    for (; b != NULL; b = prox) {
        prox = b->prox;
        free(b);
    }
}

/* Arena: bloco atual = cabeca da cadeia; sem espaco, um bloco novo vira a cabeca. */
static inline ForjaBloco *forja_arena_bloco(ForjaAlocador *a, int64_t alinhado, const char *arquivo, int linha)
{
    ForjaBloco *b = (ForjaBloco *)a->blocos;
    if (b != NULL && alinhado <= b->cap - b->topo)
        return b;
    b = forja_bloco_novo(b, alinhado > a->bloco ? alinhado : a->bloco, arquivo, linha);
    a->blocos = b;
    return b;
}

static inline void *forja_arena_alocar(ForjaAlocador *a, int64_t bytes, const char *arquivo, int linha)
{
    ForjaBloco *b;
    int64_t alinhado;
    unsigned char *p;
    if (bytes < 0 || bytes > INT64_MAX / 2)
        forja_falha_alocacao("sem memoria na Arena para bytes", bytes, arquivo, linha);
    alinhado = (bytes + FORJA_ALOC_EXTRA + 15) & ~(int64_t)15;
    b = forja_arena_bloco(a, alinhado, arquivo, linha);
    p = forja_bloco_dados(b) + b->topo + FORJA_ALOC_EXTRA;
    b->topo += alinhado;
    a->usado += (bytes + 15) & ~(int64_t)15;
    forja_aloc_cabecalho(p, FORJA_MARCA_ARENA, forja_aloc_handle(a));
    forja_aloc_origem(p, bytes, arquivo, linha);
    return p;
}

static inline uint32_t forja_arena_criar(int64_t bloco)
{
    return forja_aloc_registrar(FORJA_TIPO_ARENA, bloco < FORJA_BLOCO_MIN ? FORJA_BLOCO_MIN : bloco);
}

static inline void forja_arena_limpar(uint32_t h)
{
    ForjaAlocador *a = forja_aloc_de(h, FORJA_ARQ_STD, 0);
    ForjaBloco *b = (ForjaBloco *)a->blocos;
    if (b != NULL) {
        forja_blocos_soltar(b->prox);
        b->prox = NULL;
#if defined(FORJA_DEV)
        __builtin_memset(forja_bloco_dados(b), 0xDD, (size_t)b->topo);
#endif
        b->topo = 0;
    }
    a->usado = 0;
}

static inline void forja_arena_destruir(uint32_t h)
{
    ForjaAlocador *a = forja_aloc_de(h, FORJA_ARQ_STD, 0);
    forja_blocos_soltar((ForjaBloco *)a->blocos);
    *a = (ForjaAlocador){.geracao = a->geracao};
}

static inline int64_t forja_arena_usado(uint32_t h)
{
    return forja_aloc_de(h, FORJA_ARQ_STD, 0)->usado;
}

/* Pool: o tamanho do slot fixa no 1o pedido; a lista livre (LIFO) guarda o proximo no proprio slot. */
static inline void forja_pool_fixar_slot(ForjaAlocador *a, int64_t bytes, const char *arquivo, int linha)
{
    char causa[256];
    if (a->tam_slot == 0) {
        if (bytes > INT64_MAX / 2)
            forja_falha_alocacao("sem memoria no Pool para bytes", bytes, arquivo, linha);
        a->tam_slot = (bytes + 15) & ~(int64_t)15;
        if (a->tam_slot < 16)
            a->tam_slot = 16;
        return;
    }
    if (bytes > a->tam_slot) {
        snprintf(causa, sizeof(causa), "pool de %lld bytes por objeto recebeu um pedido maior (%lld)", (long long)a->tam_slot,
                 (long long)bytes);
        forja_falha(FORJA_R0007, causa, arquivo, linha);
    }
}

static inline void forja_pool_bloco(ForjaAlocador *a, const char *arquivo, int linha)
{
    int64_t passo = a->tam_slot + FORJA_ALOC_EXTRA, i;
    ForjaBloco *b;
    unsigned char *p;
    if (a->por_bloco > INT64_MAX / 2 / passo)
        forja_falha_alocacao("Pool com slots demais por bloco", a->por_bloco, arquivo, linha);
    b = forja_bloco_novo((ForjaBloco *)a->blocos, a->por_bloco * passo, arquivo, linha);
    a->blocos = b;
    for (i = a->por_bloco - 1; i >= 0; i--) {
        p = forja_bloco_dados(b) + i * passo + FORJA_ALOC_EXTRA;
        forja_aloc_cabecalho(p, FORJA_MARCA_MORTA, 0u);
        *(void **)p = a->livres;
        a->livres = p;
    }
}

static inline void *forja_pool_alocar(ForjaAlocador *a, int64_t bytes, const char *arquivo, int linha)
{
    void *p;
    forja_pool_fixar_slot(a, bytes, arquivo, linha);
    if (a->livres == NULL)
        forja_pool_bloco(a, arquivo, linha);
    p = a->livres;
    a->livres = *(void **)p;
    a->vivos++;
    forja_aloc_cabecalho(p, FORJA_MARCA_POOL, forja_aloc_handle(a));
    forja_aloc_origem(p, bytes, arquivo, linha);
    return p;
}

static inline uint32_t forja_pool_criar(int64_t por_bloco)
{
    uint32_t h = forja_aloc_registrar(FORJA_TIPO_POOL, 0);
    forja_aloc_de(h, FORJA_ARQ_STD, 0)->por_bloco = por_bloco < 1 ? 1 : por_bloco;
    return h;
}

static inline void forja_pool_devolver(uint32_t h, void *p, const char *arquivo, int linha)
{
    ForjaAlocador *a = forja_aloc_de(h, arquivo, linha);
    if (p == NULL)
        return;
    forja_pool_conferir(h, p, arquivo, linha);
    a->vivos--;
    *(void **)p = a->livres;
    a->livres = p;
}

static inline void forja_pool_destruir(uint32_t h)
{
    ForjaAlocador *a = forja_aloc_de(h, FORJA_ARQ_STD, 0);
    forja_blocos_soltar((ForjaBloco *)a->blocos);
    *a = (ForjaAlocador){.geracao = a->geracao};
}

static inline int64_t forja_pool_vivos(uint32_t h)
{
    return forja_aloc_de(h, FORJA_ARQ_STD, 0)->vivos;
}

static inline void *forja_aloc_alocar(uint32_t h, int64_t bytes, const char *arquivo, int linha)
{
    ForjaAlocador *a = forja_aloc_de(h, arquivo, linha);
    if (a->tipo == FORJA_TIPO_ARENA)
        return forja_arena_alocar(a, bytes, arquivo, linha);
    return forja_pool_alocar(a, bytes, arquivo, linha);
}

static inline void *forja_mem_alocar(uint32_t alocador, int64_t bytes, const char *arquivo, int linha)
{
    if (alocador == FORJA_ALOC_HEAP)
        return forja_heap_alocar(bytes, arquivo, linha);
    if (alocador == FORJA_ALOC_FRAME)
        return forja_frame_alocar(bytes, arquivo, linha);
    return forja_aloc_alocar(alocador, bytes, arquivo, linha);
}

/* Lista em Arena/Pool: so a Arena cresce (copia para um bloco novo); Pool recusa. */
static inline void *forja_aloc_lista(uint32_t h, int64_t bytes, const char *arquivo, int linha)
{
    if (forja_aloc_de(h, arquivo, linha)->tipo == FORJA_TIPO_POOL)
        forja_falha(FORJA_R0007, "lista nao cresce em Pool", arquivo, linha);
    return forja_aloc_alocar(h, bytes, arquivo, linha);
}

static inline void *forja_lista_copiar(void *novo, const void *velho, int64_t bytes)
{
    if (velho != NULL && bytes > 0)
        __builtin_memcpy(novo, velho, (size_t)bytes);
    return novo;
}

static inline void *forja_lista_criar(uint32_t alocador, int64_t cap, int64_t tam_elem, uint32_t *epoca, const char *arquivo, int linha)
{
    *epoca = 0;
    if (cap < 0)
        forja_falha_alocacao("make com cap negativo", cap, arquivo, linha);
    if (cap == 0)
        return NULL;
    if (cap > INT64_MAX / tam_elem)
        forja_falha_alocacao("make com cap grande demais para a memoria", cap, arquivo, linha);
    if (alocador == FORJA_ALOC_FRAME)
        *epoca = forja_rt_frame.epoca;
    return forja_mem_alocar(alocador, cap * tam_elem, arquivo, linha);
}

static inline void *forja_lista_crescer(void *ptr, int64_t len, int64_t *cap, int64_t tam_elem, uint32_t alocador, uint32_t *epoca,
                                        const char *arquivo, int linha)
{
    int64_t novo;
    void *p;
    if (alocador == 0)
        forja_falha_alocacao("append numa lista sem alocador (valor zero): crie com make([dynamic]T, em = heap)", (long long)alocador, arquivo, linha);
    if (*cap > INT64_MAX / 2 / tam_elem)
        forja_falha_alocacao("append: a lista passou do limite de memoria com cap", *cap, arquivo, linha);
    novo = *cap < 4 ? 8 : *cap * 2;
    if (alocador == FORJA_ALOC_FRAME) {
        p = forja_frame_realocar(ptr, len * tam_elem, novo * tam_elem, arquivo, linha);
        *epoca = forja_rt_frame.epoca;
    } else if (alocador == FORJA_ALOC_HEAP) {
        p = forja_heap_realocar(ptr, novo * tam_elem, arquivo, linha);
    } else {
        p = forja_lista_copiar(forja_aloc_lista(alocador, novo * tam_elem, arquivo, linha), ptr, len * tam_elem);
    }
    *cap = novo;
    return p;
}

static inline void forja_lista_liberar(void *ptr, uint32_t alocador, const char *arquivo, int linha)
{
    if (alocador != FORJA_ALOC_HEAP || ptr == NULL)
        return;
    forja_mem_liberar(ptr, arquivo, linha);
}

static inline void forja_frame_reset(void)
{
#if defined(FORJA_DEV)
    if (forja_rt_frame.base != NULL && forja_rt_frame.topo > 0)
        __builtin_memset(forja_rt_frame.base, 0xDD, (size_t)forja_rt_frame.topo);
#endif
    forja_rt_frame.topo = 0;
    forja_rt_frame.epoca = forja_rt_frame.epoca == UINT32_MAX ? 1u : forja_rt_frame.epoca + 1u;
}

/* ---- v1.9: soa dinamica (P8) e map (P9). ---- */
/* soa dinamica: as K colunas moram num bloco unico; a coluna k comeca em off_k = soma de A16(cap * tams[j]) para j < k. */
#define FORJA_SOA_A16(x) (((x) + 15) & ~(int64_t)15)

/* Total de bytes do bloco de cap linhas, ou -1 quando alguma conta passaria do limite (cap grande demais). */
static inline int64_t forja_soa_total(const int64_t *tams, uint32_t ncol, int64_t cap)
{
    int64_t total = 0;
    uint32_t k;
    for (k = 0; k < ncol; k++) {
        if (cap > (INT64_MAX / 4) / tams[k])
            return -1;
        total += FORJA_SOA_A16(cap * tams[k]);
        if (total > INT64_MAX / 2)
            return -1;
    }
    return total;
}

static inline void forja_soa_criar(void **col, const int64_t *tams, uint32_t ncol, int64_t cap, uint32_t alocador, uint32_t *epoca,
                                   const char *arquivo, int linha)
{
    int64_t total, off = 0;
    unsigned char *bloco;
    uint32_t k;
    *epoca = 0;
    if (cap < 0)
        forja_falha_alocacao("make com cap negativo", cap, arquivo, linha);
    if (cap == 0) {
        for (k = 0; k < ncol; k++)
            col[k] = NULL;
        return;
    }
    total = forja_soa_total(tams, ncol, cap);
    if (total < 0)
        forja_falha_alocacao("make com cap grande demais para a memoria", cap, arquivo, linha);
    if (alocador == FORJA_ALOC_FRAME)
        *epoca = forja_rt_frame.epoca;
    bloco = (unsigned char *)forja_mem_alocar(alocador, total, arquivo, linha);
    for (k = 0; k < ncol; k++) {
        col[k] = bloco + off;
        off += FORJA_SOA_A16(cap * tams[k]);
    }
}

static inline void forja_soa_crescer(void **col, const int64_t *tams, uint32_t ncol, int64_t len, int64_t *cap, uint32_t alocador,
                                     uint32_t *epoca, const char *arquivo, int linha)
{
    int64_t novo, total, off = 0;
    unsigned char *bloco;
    uint32_t k;
    if (alocador == 0)
        forja_falha_alocacao("append numa soa sem alocador (valor zero): crie com make(soa[dynamic]S, em = heap)", 0, arquivo, linha);
    total = -1;
    novo = 0;
    if (*cap <= INT64_MAX / 2) {
        novo = *cap < 4 ? 8 : *cap * 2;
        total = forja_soa_total(tams, ncol, novo);
    }
    if (total < 0)
        forja_falha_alocacao("append: a soa passou do limite de memoria com cap", *cap, arquivo, linha);
    if (alocador == FORJA_ALOC_FRAME)
        *epoca = forja_rt_frame.epoca;
    if (alocador == FORJA_ALOC_HEAP || alocador == FORJA_ALOC_FRAME)
        bloco = (unsigned char *)forja_mem_alocar(alocador, total, arquivo, linha);
    else
        bloco = (unsigned char *)forja_aloc_lista(alocador, total, arquivo, linha);
    for (k = 0; k < ncol; k++) {
        if (len > 0)
            __builtin_memcpy(bloco + off, col[k], (size_t)(len * tams[k]));
        off += FORJA_SOA_A16(novo * tams[k]);
    }
    if (alocador == FORJA_ALOC_HEAP && col[0] != NULL)
        forja_mem_liberar(col[0], arquivo, linha);
    off = 0;
    for (k = 0; k < ncol; k++) {
        col[k] = bloco + off;
        off += FORJA_SOA_A16(novo * tams[k]);
    }
    *cap = novo;
}

static inline void forja_soa_liberar(void **col, uint32_t alocador, const char *arquivo, int linha)
{
    if (alocador == FORJA_ALOC_HEAP && col[0] != NULL)
        forja_mem_liberar(col[0], arquivo, linha);
}

/* map (v1.9 P9): chaves e valores densos na ordem de insercao + tabela de slots (enderecamento aberto, sondagem linear,
   fator de carga <= 1/2). Bloco unico: [chaves: cap*tk][alinha 16][valores: cap*tv][alinha 16][slots: nslots*4]. */
typedef struct forja_mapa {
    void *chaves;      /* cap chaves de tam_chave bytes, densas, na ordem de insercao */
    void *valores;     /* cap valores de tam_valor bytes, mesmo indice da chave */
    uint32_t *slots;   /* nslots entradas: 0 = vazio, senao indice denso + 1 */
    int64_t len;
    int64_t cap;
    uint32_t nslots;   /* potencia de 2 >= 2 * cap, minimo 16; 0 sem bloco */
    uint32_t alocador;
    uint32_t epoca;    /* quadro do frame (alocador 2), senao 0 */
} forja_mapa;
_Static_assert(sizeof(forja_mapa) == 56, "forja_mapa: 56 bytes");

static inline uint64_t forja_mapa_mistura(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

static inline uint64_t forja_mapa_hash(const void *chave, int64_t tk, bool texto)
{
    uint64_t h, u = 0;
    if (texto) {
        const forja_str *s = (const forja_str *)chave;
        int64_t i;
        h = 0xcbf29ce484222325ULL;
        for (i = 0; i < s->len; i++) {
            h ^= (unsigned char)s->ptr[i];
            h *= 0x100000001b3ULL;
        }
        return forja_mapa_mistura(h);
    }
    __builtin_memcpy(&u, chave, (size_t)tk);
    return forja_mapa_mistura(u);
}

static inline bool forja_mapa_igual(const void *a, const void *b, int64_t tk, bool texto)
{
    if (texto) {
        const forja_str *x = (const forja_str *)a;
        const forja_str *y = (const forja_str *)b;
        return x->len == y->len && (x->len == 0 || __builtin_memcmp(x->ptr, y->ptr, (size_t)x->len) == 0);
    }
    return __builtin_memcmp(a, b, (size_t)tk) == 0;
}

/* Slot da chave: o do indice denso dela (*indice >= 0) ou o primeiro vazio da sondagem (*indice = -1). Pre: nslots > 0. */
static inline uint32_t forja_mapa_sondar(const forja_mapa *m, const void *chave, int64_t tk, bool texto, int64_t *indice)
{
    uint32_t mask = m->nslots - 1u;
    uint32_t s = (uint32_t)forja_mapa_hash(chave, tk, texto) & mask;
    for (;;) {
        uint32_t v = m->slots[s];
        if (v == 0) {
            *indice = -1;
            return s;
        }
        if (forja_mapa_igual((const unsigned char *)m->chaves + (int64_t)(v - 1u) * tk, chave, tk, texto)) {
            *indice = (int64_t)v - 1;
            return s;
        }
        s = (s + 1u) & mask;
    }
}

/* Slot "de casa" da entrada v (indice denso + 1). */
static inline uint32_t forja_mapa_casa(const forja_mapa *m, uint32_t v, int64_t tk, bool texto)
{
    return (uint32_t)forja_mapa_hash((const unsigned char *)m->chaves + (int64_t)(v - 1u) * tk, tk, texto) & (m->nslots - 1u);
}

static inline void forja_mapa_refazer(forja_mapa *m, int64_t tk, bool texto)
{
    int64_t i, ind;
    __builtin_memset(m->slots, 0, (size_t)m->nslots * 4u);
    for (i = 0; i < m->len; i++) {
        uint32_t s = forja_mapa_sondar(m, (const unsigned char *)m->chaves + i * tk, tk, texto, &ind);
        m->slots[s] = (uint32_t)i + 1u;
    }
}

static inline void *forja_mapa_bloco(uint32_t alocador, int64_t bytes, uint32_t *epoca, const char *arquivo, int linha)
{
    if (alocador == FORJA_ALOC_HEAP)
        return forja_mem_alocar(1u, bytes, arquivo, linha);
    if (alocador == FORJA_ALOC_FRAME) {
        void *b = forja_frame_alocar(bytes, arquivo, linha);
        *epoca = forja_rt_frame.epoca;
        return b;
    }
    return forja_aloc_lista(alocador, bytes, arquivo, linha);
}

/* Aloca um bloco de nc entradas, copia as len atuais, solta o bloco antigo (heap) e refaz os slots. */
static inline void forja_mapa_montar(forja_mapa *m, int64_t nc, int64_t tk, int64_t tv, bool texto, const char *arquivo, int linha)
{
    int64_t maior = tk > tv ? tk : tv, bytes, off_v;
    int64_t ns = 16;
    unsigned char *b;
    /* nc <= 2^30 garante nslots em uint32_t e indices em uint32_t */
    if (nc > INT64_MAX / 4 / maior || nc > 0x40000000LL)
        forja_falha_alocacao("map passou do limite de memoria com cap", m->cap, arquivo, linha);
    while (ns < 2 * nc)
        ns *= 2;
    off_v = (nc * tk + 15) & ~(int64_t)15;
    bytes = off_v + ((nc * tv + 15) & ~(int64_t)15) + ns * 4;
    b = (unsigned char *)forja_mapa_bloco(m->alocador, bytes, &m->epoca, arquivo, linha);
    if (m->len > 0) {
        __builtin_memcpy(b, m->chaves, (size_t)(m->len * tk));
        __builtin_memcpy(b + off_v, m->valores, (size_t)(m->len * tv));
    }
    if (m->alocador == FORJA_ALOC_HEAP)
        forja_mem_liberar(m->chaves, arquivo, linha);
    m->chaves = b;
    m->valores = b + off_v;
    m->slots = (uint32_t *)(b + off_v + ((nc * tv + 15) & ~(int64_t)15));
    m->cap = nc;
    m->nslots = (uint32_t)ns;
    forja_mapa_refazer(m, tk, texto);
}

static inline void forja_mapa_crescer(forja_mapa *m, int64_t tk, int64_t tv, bool texto, const char *arquivo, int linha)
{
    if (m->alocador == 0)
        forja_falha_alocacao("insert num map sem alocador (valor zero): crie com make(map[K]V, em = heap)", 0, arquivo, linha);
    forja_mapa_montar(m, m->cap < 4 ? 8 : m->cap * 2, tk, tv, texto, arquivo, linha);
}

static inline void forja_mapa_criar(forja_mapa *m, uint32_t alocador, int64_t cap, int64_t tam_chave, int64_t tam_valor,
                                    const char *arquivo, int linha)
{
    m->alocador = alocador;
    if (cap < 0)
        forja_falha_alocacao("make com cap negativo", cap, arquivo, linha);
    if (cap == 0)
        return;
    m->cap = 0;
    forja_mapa_montar(m, cap, tam_chave, tam_valor, false, arquivo, linha);
}

static inline int64_t forja_mapa_achar(const forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, const char *arquivo,
                                       int linha)
{
    int64_t indice;
    (void)arquivo;
    (void)linha;
    forja_lista_viva(m->epoca, arquivo, linha);
    if (m->len == 0)
        return -1;
    forja_mapa_sondar(m, chave, tam_chave, texto, &indice);
    return indice;
}

static inline int64_t forja_mapa_inserir(forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, int64_t tam_valor,
                                         const char *arquivo, int linha)
{
    int64_t i;
    uint32_t s;
    forja_lista_viva(m->epoca, arquivo, linha);
    if (m->len == m->cap) {
        i = forja_mapa_achar(m, chave, tam_chave, texto, arquivo, linha);
        if (i >= 0)
            return i;
        forja_mapa_crescer(m, tam_chave, tam_valor, texto, arquivo, linha);
    }
    s = forja_mapa_sondar(m, chave, tam_chave, texto, &i);
    if (i >= 0)
        return i;
    i = m->len;
    m->len += 1;
    __builtin_memcpy((unsigned char *)m->chaves + i * tam_chave, chave, (size_t)tam_chave);
    m->slots[s] = (uint32_t)i + 1u;
    return i;
}

static inline void forja_mapa_remover(forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, int64_t tam_valor,
                                       const char *arquivo, int linha)
{
    int64_t x, ult, iu;
    uint32_t s, mask, i, j, su;
    (void)arquivo;
    (void)linha;
    forja_lista_viva(m->epoca, arquivo, linha);
    if (m->len == 0)
        return;
    s = forja_mapa_sondar(m, chave, tam_chave, texto, &x);
    if (x < 0)
        return;
    /* Algoritmo R de Knuth: fecha o buraco do slot s deslocando para tras as entradas que nao ficam antes da casa delas. */
    mask = m->nslots - 1u;
    i = s;
    j = s;
    for (;;) {
        uint32_t v, k;
        j = (j + 1u) & mask;
        v = m->slots[j];
        if (v == 0)
            break;
        k = forja_mapa_casa(m, v, tam_chave, texto);
        if (i <= j ? (i < k && k <= j) : (i < k || k <= j))
            continue;
        m->slots[i] = v;
        i = j;
    }
    m->slots[i] = 0;
    /* swap-remove: a ultima entrada densa ocupa o lugar da removida. */
    ult = m->len - 1;
    if (x != ult) {
        su = forja_mapa_sondar(m, (const unsigned char *)m->chaves + ult * tam_chave, tam_chave, texto, &iu);
        __builtin_memcpy((unsigned char *)m->chaves + x * tam_chave, (unsigned char *)m->chaves + ult * tam_chave,
                         (size_t)tam_chave);
        __builtin_memcpy((unsigned char *)m->valores + x * tam_valor, (unsigned char *)m->valores + ult * tam_valor,
                         (size_t)tam_valor);
        m->slots[su] = (uint32_t)x + 1u;
    }
    m->len -= 1;
}

static inline void forja_mapa_liberar(forja_mapa *m, const char *arquivo, int linha)
{
    uint32_t a = m->alocador;
    if (a == FORJA_ALOC_HEAP && m->chaves != NULL)
        forja_mem_liberar(m->chaves, arquivo, linha);
    *m = (forja_mapa){0};
    m->alocador = a;
}

/* v1.7: texto formatado f"...". Buffer unico do modulo (uma thread), cresce x2 a partir de 256. Mesmos valores do ast.h. */
#define FORJA_FMT_LARGURA 0xFFu
#define FORJA_FMT_PRECISAO_DESLOC 8
#define FORJA_FMT_PRECISAO 0x1Fu
#define FORJA_FMT_ZERO (1u << 13)
#define FORJA_FMT_HEX (1u << 14)

typedef struct { char *dados; int64_t len, cap; } ForjaFmt;
static FORJA_FIO ForjaFmt forja_rt_fmt;

static inline void forja_fmt_inicio(void)
{
    forja_rt_fmt.len = 0;
}

static inline void forja_fmt_reservar(int64_t n)
{
    int64_t cap = forja_rt_fmt.cap;
    char *novo;
    if (forja_rt_fmt.len + n <= cap)
        return;
    while (forja_rt_fmt.len + n > cap)
        cap = cap * 2 > 256 ? cap * 2 : 256;
    novo = (char *)realloc(forja_rt_fmt.dados, (size_t)cap);
    if (novo == NULL)
        forja_falha_alocacao("sem memoria para o texto formatado em bytes", cap, "forja_fmt", 0);
    forja_rt_fmt.dados = novo;
    forja_rt_fmt.cap = cap;
    FORJA_FIO_GUARDAR_FMT(novo);
}

static inline void forja_fmt_bytes(const char *p, int64_t n)
{
    if (n <= 0)
        return;
    forja_fmt_reservar(n);
    __builtin_memcpy(forja_rt_fmt.dados + forja_rt_fmt.len, p, (size_t)n);
    forja_rt_fmt.len += n;
}

static inline void forja_fmt_alinhado(const char *p, int64_t n, uint32_t fmt)
{
    int64_t w = (int64_t)(fmt & FORJA_FMT_LARGURA);
    bool zero = (fmt & FORJA_FMT_ZERO) != 0;
    if (n >= w) {
        forja_fmt_bytes(p, n);
        return;
    }
    if (zero && p[0] == '-') {
        forja_fmt_bytes("-", 1);
        p++;
        n--;
        w--;
    }
    forja_fmt_reservar(w - n);
    __builtin_memset(forja_rt_fmt.dados + forja_rt_fmt.len, zero ? '0' : ' ', (size_t)(w - n));
    forja_rt_fmt.len += w - n;
    forja_fmt_bytes(p, n);
}

static inline void forja_fmt_str(forja_str s, uint32_t fmt)
{
    forja_fmt_alinhado(s.ptr, s.len, fmt);
}

static inline void forja_fmt_cstr(const char *p, uint32_t fmt)
{
    forja_fmt_alinhado(p, (int64_t)__builtin_strlen(p), fmt);
}

static inline void forja_fmt_bool(bool v, uint32_t fmt)
{
    forja_fmt_alinhado(v ? "true" : "false", v ? 4 : 5, fmt);
}

static inline void forja_fmt_i64(int64_t v, uint32_t fmt)
{
    char t[32];
    int n = snprintf(t, sizeof(t), "%lld", (long long)v);
    forja_fmt_alinhado(t, n, fmt);
}

static inline void forja_fmt_u64(uint64_t v, uint32_t fmt)
{
    char t[32];
    int n = snprintf(t, sizeof(t), (fmt & FORJA_FMT_HEX) ? "%llx" : "%llu", (unsigned long long)v);
    forja_fmt_alinhado(t, n, fmt);
}

static inline void forja_fmt_f32(double v, uint32_t fmt)
{
    char t[384];
    uint32_t pr = (fmt >> FORJA_FMT_PRECISAO_DESLOC) & FORJA_FMT_PRECISAO;
    int n = pr ? snprintf(t, sizeof(t), "%.*f", (int)pr - 1, v) : snprintf(t, sizeof(t), "%.9g", v);
    forja_fmt_alinhado(t, n, fmt);
}

static inline void forja_fmt_f64(double v, uint32_t fmt)
{
    char t[384];
    uint32_t pr = (fmt >> FORJA_FMT_PRECISAO_DESLOC) & FORJA_FMT_PRECISAO;
    int n = pr ? snprintf(t, sizeof(t), "%.*f", (int)pr - 1, v) : snprintf(t, sizeof(t), "%.17g", v);
    forja_fmt_alinhado(t, n, fmt);
}

/* Real pela forma mais curta que volta ao mesmo valor (print de f32/f64, Display sempre posicional): forja_fmt_f64_curto e forja_fmt_f32_curto moram em forja_rt_ryu.h (GERADO por
 * runtime\gerar_rt_ryu.py a partir do Ryu e do rt_real_curto.c do runtime nativo); o emissor o inclui depois deste header so quando o programa imprime real (IrPrograma.usa_real_curto). */

static inline const char *forja_fmt_cstr_final(uint32_t alocador, const char *arquivo, int linha)
{
    char *p = (char *)forja_mem_alocar(alocador, forja_rt_fmt.len + 1, arquivo, linha);
    if (forja_rt_fmt.len > 0)
        __builtin_memcpy(p, forja_rt_fmt.dados, (size_t)forja_rt_fmt.len);
    p[forja_rt_fmt.len] = 0;
    return p;
}

static inline forja_str forja_fmt_str_final(uint32_t alocador, const char *arquivo, int linha)
{
    forja_str s;
    s.ptr = forja_fmt_cstr_final(alocador, arquivo, linha);
    s.len = forja_rt_fmt.len;
    return s;
}

static inline void forja_fmt_imprimir(void)
{
    forja_rt_travar_fluxo(stdout);
    if (forja_rt_fmt.len > 0)
        fwrite(forja_rt_fmt.dados, 1, (size_t)forja_rt_fmt.len, stdout);
    putchar('\n');
    forja_rt_destravar_fluxo(stdout);
}

/* eprint (IA1-L5a): o buffer e '\n' em stderr. */
static inline void forja_fmt_imprimir_erro(void)
{
    forja_rt_travar_fluxo(stderr);
    if (forja_rt_fmt.len > 0)
        fwrite(forja_rt_fmt.dados, 1, (size_t)forja_rt_fmt.len, stderr);
    fputc('\n', stderr);
    forja_rt_destravar_fluxo(stderr);
}

#endif
