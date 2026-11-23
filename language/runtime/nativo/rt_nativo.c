/*
 * rt_nativo.c - impressao, falha e saida do runtime nativo da Forja (P2a).
 *
 * Versao: 0.20.0 (M6e P93e: forja_entrada le o bit de guarda da FRT1 por RT_GUARDA_LER, na mesma linha de RT_PILHA_TOPO; antes, 0.19.0: M5 P45, forja_falha fora de teste reserva o relatorio, 1 unico com 2 threads falhando juntas; DllMain THREAD_DETACH libera o fio)
 * Changelog: 0.18.0 - M5 P44: rt_print_linha segura rt_saida_trava (a linha inteira); rt_ao_sair e rt_ao_sair_usados passam a rt_ao_sair_trava (forja_rt_ao_sair e rt_ao_sair_tirar; o handler roda fora da trava); o #line 450 mantem o __LINE__ do R0003 de forja_rt_ao_sair. 0.17.0 - M5 P43: rt_em_teste, rt_teste_falhou e rt_salto viram campos do RtFio (rt_fio.h; macros no lugar das globais, mesmas linhas: o __LINE__ do R0003 abaixo nao muda); rt_ao_sair, rt_ao_sair_usados e rt_sem_relatorio ganham a marca "compartilhado:"; forja_entrada e forja_entrada_dll chamam rt_fio_iniciar. 0.16.0 - IA1-b12: ganchos do --rastreio (RT_PILHA_*, RT_FALHA_LINHA, RT_FALHA_FIM, RT_MAIN_ERRO_FIM): na variante -DFORJA_RASTREIO entram coluna, valor e pilha .fj na linha R (rt_pilha.c); sem a macro expandem para o texto de antes e o objeto sai byte a byte igual (o #line 255 mantem o __LINE__ do que vem depois).
 *           0.15.0 - M3 P22a: forja_entrada_dll (DllMain da DLL Forja, no fim do arquivo para nao deslocar as linhas). 0.14.0 - Linguagem v1.11 L16a: forja_falha, dentro de teste, anota a 1a falha (rt_teste_anotar, em rt_teste.c) para o forja test --json.
 *           0.13.0 - P18a: forja_traco (o traco do diferencial, nivel 2: uma linha "@traco ..." em stderr numa escrita so; no fim do arquivo, sem estado novo).
 *           0.12.0 - M2 P17b: forja_rt_layout de 23 palavras (versao 3: acrescenta sizeof e offsets do forja_mapa e o cabecalho da soa dinamica de 1 a 8 colunas; mesmo numero de linhas do arquivo). 0.11.0 - M2 P16: forja_rt_conta_bits (o popcount do bit_set por SWAR em C puro: o __builtin_popcountll do gcc chamaria a libgcc, que o runtime freestanding nao tem; no fim do arquivo).
 *           0.10.0 - M2 P14b: forja_rt_layout passa a 10 palavras (versao 2): alem do str, tamanho, alinhamento e offsets de len, cap, alocador e epoca do [dynamic]T (forja_lista_layout).
 *           0.9.0 - M2 P13b: forja_falha_indice, forja_fatia_confere e forja_vetor_c_confere (R0006/R0005 de indice, fatia e slice_from; no fim do arquivo, e FORJA_R0006 junto das outras constantes).
 *           0.8.0 - M2 P13a: forja_str_igual e forja_str_de_cstr (no fim do arquivo; dev: nulo e R0005, release: str vazia), com as regras do forja_rt.h 0.10.0. forja_print_str ja existia.
 *           0.7.0 - M2 P12b: estado do executor de testes (rt_em_teste, rt_teste_falhou, rt_sem_relatorio, rt_salto); forja_falha, dentro de teste, marca o teste como falho e volta ao
 *           executor por forja_rt_saltar (fora de teste sai com 101 como antes); forja_main_erro marca rt_sem_relatorio. forja_expect passa para rt_teste.c (com o ramo de dentro de teste).
 *           As linhas deste arquivo deslocaram (o __LINE__ do R0003 do limite de handlers mudou de numero).
 *           0.6.0 - M2 P12a: forja_assert (R0003 com a mensagem) e forja_expect fora de teste (R0003 "expect falhou: <texto>", causa de ate 255 bytes como o snprintf de 256 do forja_rt.h:874);
 *           ficam no FIM do arquivo para nao mexer nas linhas dos outros servicos. Dentro de teste (salto ao executor) e o P12b.
 *   0.5.0 - M2 P11b: forja_rt_layout[4] em .rdata = {versao, sizeof, alignof e offsetof(len) do forja_str}; o nativo.c compara com o layout\ do compilador (X0906).
 *   0.4.0 - M2 P11a: forja_faixa, forja_faixa_u64 (R0001, causa de ate 255 bytes como o snprintf de 256 do forja_rt.h), forja_falha_nulo e forja_nao_nulo (R0005); RtCausa ganha o limite
 *           (rt_causa_iniciar: 160 nas falhas de inteiro, 256 na faixa e no nulo).
 *   0.3.1 - M2 P9a: so comentario LIMITE em rt_erro_json (linhas preservadas: o __LINE__ de forja_panic vai no exe).
 *   0.3.0 - forja_entrada liga o console em UTF-8 antes do primeiro print (sys_console_ligar_utf8); sys_sair restaura a pagina.
 *   0.2.1 - P7b do M1: inclui forja_rt_nativo.h (interface publica); sem mudanca de codigo gerado.
 *   0.2.0 - forja_panic(msg, arquivo, linha): R0003 como o forja_rt.h:836, vale em dev e em release (P7a do M1).
 *   0.1.0 - forja_print_i32/i64/u64/bool/texto/str; forja_falha, forja_falha_estouro(_u),
 *           forja_falha_conversao_i/u e forja_main_erro, com os nomes e o JSON do forja_rt.h 0.9.0;
 *           forja_rt_ao_sair (32 entradas, ordem inversa), forja_sair e forja_entrada.
 *           Fora deste passo: forja_falha_conversao_f (%.17g) e setjmp/longjmp (P12).
 *
 * forja_falha e forja_main_erro montam a linha JSON de stderr como o forja_json_texto do forja_rt.h.
 * forja_falha termina com forja_sair(101), como o exit(101) do forja_rt.h.
 * forja_rt_ao_sair, forja_sair e forja_entrada nao existem no forja_rt.h 0.9.0: sao novos aqui.
 * Incluido por rt_win.c; nao compila sozinho.
 */
#include <stddef.h>
#include <stdint.h>
#include "rt_sys.h"
#include "forja_rt_nativo.h"

#define FORJA_R0001 "R0001"
#define FORJA_R0002 "R0002"
#define FORJA_R0003 "R0003"
#define FORJA_R0005 "R0005"
#define FORJA_R0006 "R0006"

#define RT_AO_SAIR_MAX 32
#define RT_CAUSA_BYTES 160
#define RT_CAUSA_FAIXA_BYTES 256
#define RT_ERRO_LOTE_BYTES 256
#define RT_CODIGO_SAIDA_FALHA 101

typedef void (*ForjaSaidaFn)(void);

/* Estado do executor de testes (a logica fica em rt_teste.c, que vem depois na mesma unidade). rt_sem_relatorio: falha ja relatada = silencio (lido pelo R0009, P14a). */
#define rt_em_teste (rt_fio()->em_teste)
#define rt_teste_falhou (rt_fio()->teste_falhou)
static bool rt_sem_relatorio; /* compartilhado: falha ja relatada; so vai de false para true, e depois o processo sai */
#define rt_salto (rt_fio()->salto)
/* A 1a falha do teste em curso, para o --json (L16a); definida em rt_teste.c. */
static void rt_teste_anotar(const char *codigo, const char *causa, const char *arquivo, int linha);

/* forja_str e as declaracoes publicas (inclusive as antecipadas, como forja_sair) vem de forja_rt_nativo.h. */

/* Layout que este runtime usa para str, [dynamic]T, map e soa dinamica, em .rdata (forja_rt_nativo.h): o compilador le pelo COFF e compara com o layout\ dele (X0906 se divergir). */
const uint32_t forja_rt_layout[FORJA_RT_LAYOUT_PALAVRAS] = {FORJA_RT_LAYOUT_VERSAO, (uint32_t)sizeof(forja_str), (uint32_t)_Alignof(forja_str), (uint32_t)offsetof(forja_str, len),
    (uint32_t)sizeof(forja_lista_layout), (uint32_t)_Alignof(forja_lista_layout), (uint32_t)offsetof(forja_lista_layout, len), (uint32_t)offsetof(forja_lista_layout, cap),
    (uint32_t)offsetof(forja_lista_layout, alocador), (uint32_t)offsetof(forja_lista_layout, epoca), (uint32_t)sizeof(forja_mapa), (uint32_t)offsetof(forja_mapa, chaves),
    (uint32_t)offsetof(forja_mapa, valores), (uint32_t)offsetof(forja_mapa, len), (uint32_t)offsetof(forja_mapa, epoca), FORJA_SOA_CABECALHO(1), FORJA_SOA_CABECALHO(2), FORJA_SOA_CABECALHO(3), FORJA_SOA_CABECALHO(4), FORJA_SOA_CABECALHO(5), FORJA_SOA_CABECALHO(6), FORJA_SOA_CABECALHO(7), FORJA_SOA_CABECALHO(8)};

/* ---- impressao em stdout ---- */

/* Imprime n bytes seguidos de 0x0A (que sai como 0x0D 0x0A pela camada de saida) e fecha o print. */
static void rt_print_linha(const void *p, int64_t n)
{
    bool tomou = sys_trava_pegar(&rt_saida_trava); /* a linha inteira sob a trava: nenhuma intercalada entre threads */

    rt_saida_stdout(p, n);
    rt_saida_stdout("\n", 1);
    rt_saida_fim_print();
    sys_trava_soltar(&rt_saida_trava, tomou);
}

void forja_print_i32(int32_t valor)
{
    unsigned char digitos[24];
    int64_t n = rt_fmt_i64(digitos, (int64_t)valor);

    rt_print_linha(digitos, n);
}

void forja_print_i64(int64_t valor)
{
    unsigned char digitos[24];
    int64_t n = rt_fmt_i64(digitos, valor);

    rt_print_linha(digitos, n);
}

void forja_print_u64(uint64_t valor)
{
    unsigned char digitos[24];
    int64_t n = rt_fmt_u64(digitos, valor);

    rt_print_linha(digitos, n);
}

void forja_print_bool(bool valor)
{
    if (valor)
        rt_print_linha("true", 4);
    else
        rt_print_linha("false", 5);
}

void forja_print_texto(const char *texto)
{
    rt_print_linha(texto, (int64_t)strlen(texto));
}

/* len 0 nao le o ponteiro: rt_saida_stdout nao toca nos bytes quando n <= 0. */
void forja_print_str(forja_str s)
{
    rt_print_linha(s.ptr, s.len);
}

/* ---- linha JSON de erro em stderr ---- */

/* Lote da linha de erro: junta os pedacos ate 256 bytes e so entao manda para stderr. */
typedef struct {
    unsigned char dados[RT_ERRO_LOTE_BYTES];
    int64_t usado;
} RtErroLote;

static void rt_erro_byte(RtErroLote *lote, unsigned char b)
{
    if (lote->usado == RT_ERRO_LOTE_BYTES) {
        rt_saida_stderr(lote->dados, lote->usado);
        lote->usado = 0;
    }
    lote->dados[lote->usado++] = b;
}

static void rt_erro_bytes(RtErroLote *lote, const unsigned char *p, int64_t n)
{
    int64_t i;

    for (i = 0; i < n; i++)
        rt_erro_byte(lote, p[i]);
}

static void rt_erro_texto(RtErroLote *lote, const char *s)
{
    for (; *s != '\0'; s++)
        rt_erro_byte(lote, (unsigned char)*s);
}

static void rt_erro_numero(RtErroLote *lote, int64_t valor)
{
    unsigned char digitos[24];
    int64_t n = rt_fmt_i64(digitos, valor);

    rt_erro_bytes(lote, digitos, n);
}

/* Escapa como o forja_json_texto do forja_rt.h (aspas, barra, controles). LIMITE: 34 linhas, 13% acima de 30: uma so tabela de casos. */
static void rt_erro_json(RtErroLote *lote, const char *texto)
{
    const unsigned char *p = (const unsigned char *)(texto != NULL ? texto : "");

    for (; *p != '\0'; p++) {
        unsigned char hex[4];

        switch (*p) {
        case '"':
            rt_erro_texto(lote, "\\\"");
            break;
        case '\\':
            rt_erro_texto(lote, "\\\\");
            break;
        case '\n':
            rt_erro_texto(lote, "\\n");
            break;
        case '\r':
            rt_erro_texto(lote, "\\r");
            break;
        case '\t':
            rt_erro_texto(lote, "\\t");
            break;
        default:
            if (*p < 0x20) {
                rt_erro_texto(lote, "\\u");
                rt_fmt_hex4(hex, *p);
                rt_erro_bytes(lote, hex, 4);
            } else {
                rt_erro_byte(lote, *p);
            }
        }
    }
}

static void rt_erro_fim(RtErroLote *lote)
{
    rt_saida_stderr(lote->dados, lote->usado);
    lote->usado = 0;
}

/* ---- causa textual das falhas de inteiro ---- */

/* Mesmo limite do snprintf(causa, N, ...) do forja_rt.h: no maximo N-1 bytes e o NUL (N = 160; a faixa de campo usa 256). */
typedef struct {
    char dados[RT_CAUSA_FAIXA_BYTES];
    int64_t usado;
    int64_t limite;
} RtCausa;

static void rt_causa_iniciar(RtCausa *causa, int64_t limite)
{
    causa->usado = 0;
    causa->limite = limite;
}

static void rt_causa_byte(RtCausa *causa, char b)
{
    if (causa->usado < causa->limite - 1)
        causa->dados[causa->usado++] = b;
}

static void rt_causa_texto(RtCausa *causa, const char *s)
{
    for (; *s != '\0'; s++)
        rt_causa_byte(causa, *s);
}

static void rt_causa_i64(RtCausa *causa, int64_t valor)
{
    unsigned char digitos[24];
    int64_t n = rt_fmt_i64(digitos, valor);
    int64_t i;

    for (i = 0; i < n; i++)
        rt_causa_byte(causa, (char)digitos[i]);
}

static void rt_causa_u64(RtCausa *causa, uint64_t valor)
{
    unsigned char digitos[24];
    int64_t n = rt_fmt_u64(digitos, valor);
    int64_t i;

    for (i = 0; i < n; i++)
        rt_causa_byte(causa, (char)digitos[i]);
}

static const char *rt_causa_fim(RtCausa *causa)
{
    causa->dados[causa->usado] = '\0';
    return causa->dados;
}

/* ---- rastreio (IA1-b12): so a variante -DFORJA_RASTREIO compila rt_pilha.c; sem ela as macros abaixo somem e o objeto sai byte a byte igual ao de antes ---- */
#ifdef FORJA_RASTREIO
#include "rt_pilha.c"
#else
#define RT_PILHA_TOPO() ((void)0)
#define RT_PILHA_CAPTURAR() ((void)0)
#define RT_PILHA_VALOR_INDICE(i, n) ((void)0)
#define RT_PILHA_VALOR_FATIA(lo, hi, n) ((void)0)
#define RT_FALHA_LINHA(lote, linha) rt_erro_numero((lote), (linha))
#define RT_FALHA_FIM(lote) rt_erro_texto((lote), "\"}\n")
#define RT_MAIN_ERRO_FIM(lote) rt_erro_texto((lote), "\"}\n")
#define RT_GUARDA_LER() ((void)0)
#endif
#line 255
/* ---- falhas ---- */

/* Falha fatal fora de teste: so a 1a thread a chegar relata e sai; as outras (panic simultaneo em outra thread) giram ate o ExitProcess dela (P45). Reentrada da MESMA thread (handler de saida que falha) passa. */
static const void *rt_falha_dona; /* compartilhado: o fio que ganhou o relatorio; so vai de NULL para o fio, e depois o processo sai */

static void rt_falha_reservar(void)
{
    const void *dona = __sync_val_compare_and_swap(&rt_falha_dona, NULL, (const void *)rt_fio());

    while (dona != NULL && dona != (const void *)rt_fio())
        __builtin_ia32_pause();
}

_Noreturn void forja_falha(const char *codigo, const char *causa, const char *arquivo, int linha)
{
    RtErroLote lote;

    if (!rt_em_teste)
        rt_falha_reservar();
    lote.usado = 0; RT_PILHA_CAPTURAR();
    rt_erro_texto(&lote, "{\"codigo\": \"");
    rt_erro_json(&lote, codigo);
    rt_erro_texto(&lote, "\", \"tipo\": \"runtime\", \"arquivo\": \"");
    rt_erro_json(&lote, arquivo);
    rt_erro_texto(&lote, "\", \"linha\": ");
    RT_FALHA_LINHA(&lote, linha);
    rt_erro_texto(&lote, ", \"causa\": \"");
    rt_erro_json(&lote, causa);
    RT_FALHA_FIM(&lote);
    rt_erro_fim(&lote);
    rt_sem_relatorio = true;
    if (rt_em_teste) {
        rt_teste_anotar(codigo, causa, arquivo, linha);
        rt_teste_falhou = true;
        forja_rt_saltar(&rt_salto, 1);
    }
    forja_sair(RT_CODIGO_SAIDA_FALHA);
}

_Noreturn void forja_falha_estouro(const char *operacao, int64_t a, int64_t b, const char *arquivo, int linha)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_BYTES);
    rt_causa_texto(&causa, "estouro de inteiro em ");
    rt_causa_texto(&causa, operacao);
    rt_causa_texto(&causa, ": ");
    rt_causa_i64(&causa, a);
    rt_causa_texto(&causa, " e ");
    rt_causa_i64(&causa, b);
    forja_falha(FORJA_R0002, rt_causa_fim(&causa), arquivo, linha);
}

_Noreturn void forja_falha_estouro_u(const char *operacao, uint64_t a, uint64_t b, const char *arquivo, int linha)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_BYTES);
    rt_causa_texto(&causa, "estouro de inteiro em ");
    rt_causa_texto(&causa, operacao);
    rt_causa_texto(&causa, ": ");
    rt_causa_u64(&causa, a);
    rt_causa_texto(&causa, " e ");
    rt_causa_u64(&causa, b);
    forja_falha(FORJA_R0002, rt_causa_fim(&causa), arquivo, linha);
}

_Noreturn void forja_falha_conversao_i(int64_t valor, const char *alvo, const char *arquivo, int linha)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_BYTES);
    rt_causa_texto(&causa, "conversao 'as' de ");
    rt_causa_i64(&causa, valor);
    rt_causa_texto(&causa, " para ");
    rt_causa_texto(&causa, alvo);
    rt_causa_texto(&causa, " fora da faixa");
    forja_falha(FORJA_R0002, rt_causa_fim(&causa), arquivo, linha);
}

_Noreturn void forja_falha_conversao_u(uint64_t valor, const char *alvo, const char *arquivo, int linha)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_BYTES);
    rt_causa_texto(&causa, "conversao 'as' de ");
    rt_causa_u64(&causa, valor);
    rt_causa_texto(&causa, " para ");
    rt_causa_texto(&causa, alvo);
    rt_causa_texto(&causa, " fora da faixa");
    forja_falha(FORJA_R0002, rt_causa_fim(&causa), arquivo, linha);
}

/* Falha deliberada do programa (panic/assert): R0003. Vale em dev e em release, como no forja_rt.h. */
_Noreturn void forja_panic(const char *msg, const char *arquivo, int linha)
{
    forja_falha(FORJA_R0003, msg, arquivo, linha);
}

/* Os limites e o valor da faixa chegam em 64 bits: sem sinal sai por %llu, com sinal por %lld. */
typedef struct {
    uint64_t valor, lo, hi;
    bool sem_sinal, exclusiva;
    const char *campo, *arquivo;
    int linha;
} RtFaixaFalha;

static void rt_causa_numero(RtCausa *causa, uint64_t v, bool sem_sinal)
{
    if (sem_sinal)
        rt_causa_u64(causa, v);
    else
        rt_causa_i64(causa, (int64_t)v);
}

/* "valor V fora da faixa LO..=HI do campo 'C'" (R0001), como o snprintf(causa, 256, ...) do forja_rt.h:390. */
static _Noreturn void rt_faixa_falha(const RtFaixaFalha *f)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "valor ");
    rt_causa_numero(&causa, f->valor, f->sem_sinal);
    rt_causa_texto(&causa, " fora da faixa ");
    rt_causa_numero(&causa, f->lo, f->sem_sinal);
    rt_causa_texto(&causa, f->exclusiva ? "..<" : "..=");
    rt_causa_numero(&causa, f->hi, f->sem_sinal);
    rt_causa_texto(&causa, " do campo '");
    rt_causa_texto(&causa, f->campo);
    rt_causa_texto(&causa, "'");
    forja_falha(FORJA_R0001, rt_causa_fim(&causa), f->arquivo, f->linha);
}

int64_t forja_faixa(int64_t valor, int64_t lo, int64_t hi, bool exclusiva, const char *campo, const char *arquivo, int linha)
{
    bool fora = valor < lo || (exclusiva ? valor >= hi : valor > hi);

    if (fora)
        rt_faixa_falha(&(RtFaixaFalha){(uint64_t)valor, (uint64_t)lo, (uint64_t)hi, false, exclusiva, campo, arquivo, linha});
    return valor;
}

uint64_t forja_faixa_u64(uint64_t valor, uint64_t lo, uint64_t hi, bool exclusiva, const char *campo, const char *arquivo, int linha)
{
    bool fora = valor < lo || (exclusiva ? valor >= hi : valor > hi);

    if (fora)
        rt_faixa_falha(&(RtFaixaFalha){valor, lo, hi, true, exclusiva, campo, arquivo, linha});
    return valor;
}

/* Ponteiro nulo onde a Forja exige ^T ou cstr: R0005 com "<oque>: ponteiro nulo". */
_Noreturn void forja_falha_nulo(const char *oque, const char *arquivo, int linha)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, oque);
    rt_causa_texto(&causa, ": ponteiro nulo");
    forja_falha(FORJA_R0005, rt_causa_fim(&causa), arquivo, linha);
}

/* ^T ou cstr nao nulo (so dev, como no forja_rt.h:267); devolve o proprio ponteiro. */
void *forja_nao_nulo(const void *p, const char *oque, const char *arquivo, int linha)
{
    if (p == NULL)
        forja_falha_nulo(oque, arquivo, linha);
    return (void *)p;
}

/* 'proc main() -> ! E' que devolveu erro: escreve R0004 e volta; o main gerado sai com 1. */
void forja_main_erro(const char *arquivo, int erro)
{
    RtErroLote lote;

    rt_sem_relatorio = true;
    lote.usado = 0;
    rt_erro_texto(&lote, "{\"codigo\": \"R0004\", \"tipo\": \"runtime\", \"arquivo\": \"");
    rt_erro_json(&lote, arquivo);
    rt_erro_texto(&lote, "\", \"linha\": 0, \"causa\": \"main devolveu o erro de indice ");
    rt_erro_numero(&lote, erro);
    RT_MAIN_ERRO_FIM(&lote);
    rt_erro_fim(&lote);
}

/* ---- saida do programa ---- */

/* Pilha de handlers: o ultimo registrado roda primeiro, como o atexit do CRT. */
static ForjaSaidaFn rt_ao_sair[RT_AO_SAIR_MAX]; /* compartilhado: registrado no inicio; sob o SRW do runtime no P44 */
static int64_t rt_ao_sair_usados; /* compartilhado: idem rt_ao_sair */
static SysTrava rt_ao_sair_trava; /* compartilhado: protege rt_ao_sair e rt_ao_sair_usados quando rt_multithread */

void forja_rt_ao_sair(void (*fn)(void))
{
    bool tomou = sys_trava_pegar(&rt_ao_sair_trava);

    if (rt_ao_sair_usados == RT_AO_SAIR_MAX)
#line 450
        forja_falha(FORJA_R0003, "limite de handlers de saida excedido (32)", __FILE__, __LINE__);
    rt_ao_sair[rt_ao_sair_usados++] = fn;
    sys_trava_soltar(&rt_ao_sair_trava, tomou);
}

/* Tira o proximo handler da pilha (NULL = vazia) sob a trava; quem chama o roda fora dela (o handler pode travar outra coisa). */
static ForjaSaidaFn rt_ao_sair_tirar(void)
{
    bool tomou = sys_trava_pegar(&rt_ao_sair_trava);
    ForjaSaidaFn fn = rt_ao_sair_usados > 0 ? rt_ao_sair[--rt_ao_sair_usados] : NULL;

    sys_trava_soltar(&rt_ao_sair_trava, tomou);
    return fn;
}

_Noreturn void forja_sair(int codigo)
{
    ForjaSaidaFn fn;

    /* Tira o handler da pilha antes de chamar: se ele sair de novo, o mesmo handler nao roda outra vez. */
    while ((fn = rt_ao_sair_tirar()) != NULL)
        fn();
    rt_saida_descarregar();
    sys_sair(codigo);
}

/* Entrada do PE (-Wl,-e,forja_entrada): inicia a saida, roda o programa e sai com o codigo dele. */
_Noreturn void forja_entrada(void)
{
    int codigo;

    rt_fio_iniciar();
    sys_console_ligar_utf8();
    rt_saida_iniciar();
    RT_PILHA_TOPO(); RT_GUARDA_LER(); codigo = forja_programa();
    forja_sair(codigo);
}

/* ---- assert e expect do programa (M2 P12a) ---- */

/* assert do programa: cond falsa => R0003 com a mensagem. Vale em dev e em release. */
void forja_assert(bool cond, const char *msg, const char *arquivo, int linha)
{
    if (!cond)
        forja_falha(FORJA_R0003, msg, arquivo, linha);
}

/* ---- str (M2 P13a) ---- */

/* Igualdade de str por conteudo (nos dois modos): mesmo tamanho e mesmos bytes; len 0 nao le o ponteiro. */
bool forja_str_igual(forja_str a, forja_str b)
{
    return a.len == b.len && (a.len == 0 || memcmp(a.ptr, b.ptr, (size_t)a.len) == 0);
}

/* 'c as str': strlen no cstr. Dev: NULL e R0005 ("conversao 'as' de cstr para str: ponteiro nulo"); release nao confere e NULL da a str vazia (forja_rt.h:275 e :603). */
forja_str forja_str_de_cstr(const char *c, const char *arquivo, int linha)
{
#ifdef FORJA_DEV
    if (c == NULL)
        forja_falha_nulo("conversao 'as' de cstr para str", arquivo, linha);
#else
    (void)arquivo;
    (void)linha;
    if (c == NULL)
        return (forja_str){"", 0};
#endif
    return (forja_str){c, (int64_t)strlen(c)};
}

/* ---- indice, fatia e slice_from (M2 P13b): so o codigo de dev chama; o release x64 so confere com --checagem sim (o backend C do forja_rt.h confere salvo -DFORJA_SEM_CHECAGEM) ---- */

/* a[i] fora de 0..<n: R0006 "indice I fora de 0..<N" (forja_rt.h:561, causa de ate 255 bytes). */
_Noreturn void forja_falha_indice(int64_t i, int64_t n, const char *arquivo, int linha)
{
    RtCausa causa;

    RT_PILHA_VALOR_INDICE(i, n); rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "indice ");
    rt_causa_i64(&causa, i);
    rt_causa_texto(&causa, " fora de 0..<");
    rt_causa_i64(&causa, n);
    forja_falha(FORJA_R0006, rt_causa_fim(&causa), arquivo, linha);
}

/* a[lo..<hi] ou a[lo..=hi]: limites dentro de 0..=n e inicio ate o fim; senao R0006 "fatia LO..<HI fora de 0..=N" (forja_rt.h:567). Inclusiva sem 'hi + 1'. */
void forja_fatia_confere(int64_t lo, int64_t hi, bool exclusiva, int64_t n, const char *arquivo, int linha)
{
    bool dentro = exclusiva ? (lo >= 0 && lo <= hi && hi <= n) : (lo >= 0 && hi < n && lo - 1 <= hi);
    RtCausa causa;

    if (dentro)
        return;
    RT_PILHA_VALOR_FATIA(lo, hi, n); rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "fatia ");
    rt_causa_i64(&causa, lo);
    rt_causa_texto(&causa, exclusiva ? "..<" : "..=");
    rt_causa_i64(&causa, hi);
    rt_causa_texto(&causa, " fora de 0..=");
    rt_causa_i64(&causa, n);
    forja_falha(FORJA_R0006, rt_causa_fim(&causa), arquivo, linha);
}

/* slice_from(p, len = n): n < 0 e R0006; p nulo com n > 0 e R0005 (nulo com n == 0 e a fatia vazia). Volta se esta tudo certo (forja_rt.h:580). */
void forja_vetor_c_confere(const void *p, int64_t n, const char *arquivo, int linha)
{
    RtCausa causa;

    if (n < 0) {
        rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
        rt_causa_texto(&causa, "slice_from com len ");
        rt_causa_i64(&causa, n);
        rt_causa_texto(&causa, " negativo");
        forja_falha(FORJA_R0006, rt_causa_fim(&causa), arquivo, linha);
    }
    if (p == NULL && n > 0)
        forja_falha_nulo("slice_from", arquivo, linha);
}

/* ---- bit_set (M2 P16): conta dos bits ligados, o (int64_t)__builtin_popcountll((uint64_t)a) do emit_c_v19.c. SWAR (soma em arvore de 2, 4 e 8 bits, depois soma dos bytes); reentrante, sem tabela. ---- */
int64_t forja_rt_conta_bits(uint64_t v)
{
    v = v - ((v >> 1) & UINT64_C(0x5555555555555555));
    v = (v & UINT64_C(0x3333333333333333)) + ((v >> 2) & UINT64_C(0x3333333333333333));
    v = (v + (v >> 4)) & UINT64_C(0x0F0F0F0F0F0F0F0F);
    return (int64_t)((v * UINT64_C(0x0101010101010101)) >> 56);
}

/* ---- traco do diferencial (P18a, nivel 2): "@traco <funcao> <instr> <linha>:<coluna> 0x<16 hex>\n" em stderr, UMA escrita (G5) ---- */
#define RT_TRACO_NOME_MAX 160

static void rt_traco_texto(unsigned char *b, int64_t *n, const char *s)
{
    int64_t k;

    for (k = 0; s[k] != '\0' && k < RT_TRACO_NOME_MAX; k++)
        b[(*n)++] = (unsigned char)s[k];
}

static void rt_traco_numero(unsigned char *b, int64_t *n, uint64_t v)
{
    *n += rt_fmt_u64(b + *n, v);
}

/* A linha monta num buffer local de 256 bytes (no maximo ~221 com o CR de rt_saida_stderr): sai inteira numa chamada de escrita, sem intercalar entre threads (G5/G7). O nome da funcao e cortado em 160 bytes, como o "%.160s" do C. */
void forja_traco(const char *funcao, uint32_t instr, uint32_t linha, uint32_t coluna, uint64_t bits)
{
    unsigned char b[256];
    int64_t n = 0;
    int shift;

    rt_traco_texto(b, &n, "@traco ");
    rt_traco_texto(b, &n, funcao);
    b[n++] = ' ';
    rt_traco_numero(b, &n, instr);
    b[n++] = ' ';
    rt_traco_numero(b, &n, linha);
    b[n++] = ':';
    rt_traco_numero(b, &n, coluna);
    rt_traco_texto(b, &n, " 0x");
    for (shift = 60; shift >= 0; shift -= 4)
        b[n++] = (unsigned char)"0123456789abcdef"[(bits >> shift) & 0xFu];
    b[n++] = '\n';
    rt_saida_stderr(b, n);
}

/* ---- DllMain da DLL Forja (M3 P22a): no fim do arquivo para nao deslocar as linhas dos outros servicos ---- */
#define RT_DLL_PROCESS_DETACH 0U
#define RT_DLL_PROCESS_ATTACH 1U
#define RT_DLL_THREAD_DETACH 3U

#ifdef FORJA_DEV
static void rt_vazamentos_relatar_dll(void); /* rt_rastreio.c */
#endif

/* DETACH: descarrega o stdout; por FreeLibrary (reservado == NULL), em dev, relata o R0009 no stderr SEM ExitProcess (D29 da v1.8: o hospedeiro segue vivo).
 * No fim do processo (reservado != NULL) so descarrega: nao ha mais quem leia o relato e o loader esta no meio do desligamento. */
static void rt_dll_descarregar(const void *reservado)
{
#ifdef FORJA_DEV
    if (reservado == NULL)
        rt_vazamentos_relatar_dll();
#else
    (void)reservado;
#endif
    rt_saida_descarregar();
}

/* Entrada do PE de uma DLL Forja (AddressOfEntryPoint, a assinatura do DllMain). ATTACH inicia a saida (o console do hospedeiro nao muda de pagina); falha de
 * runtime dentro da DLL e forja_falha -> forja_sair -> ExitProcess(101), como no backend C (D0002). Sempre devolve TRUE. */
int forja_entrada_dll(void *instancia, uint32_t motivo, void *reservado)
{
    (void)instancia;
    if (motivo == RT_DLL_PROCESS_ATTACH) {
        rt_fio_iniciar();
        rt_saida_iniciar();
    }
#ifdef RT_ALVO_WIN
    else if (motivo == RT_DLL_THREAD_DETACH)
        rt_fio_liberar();
#endif
    else if (motivo == RT_DLL_PROCESS_DETACH)
        rt_dll_descarregar(reservado);
    return 1;
}
