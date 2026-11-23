/* limites.c - forja run/test: --limite-tempo, --limite-mem, --limite-saida, --args-file e a execucao do programa do usuario (ver limites.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b10: primeira versao. Substitui o _spawnvp do run/test (cc.c) pelo filho_win.c: pipe no stderr (R-code enriquecido, D-IA1-14), Job Object nos limites.
 * Verificacao: construir.bat && python -I ..\tests\ia_first\verificar_limites_b10.py --forja .\forja.exe
 */
#include "driver/limites.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "common/diag.h"
#include "driver/erro_sistema.h"
#include "driver/filho_win.h"
#include "nativo/nativo.h"

#define TEMPO_MIN_S 0.001
#define TEMPO_MAX_S 86400.0
#define MEM_MAX_MIB 1048576ull
#define SAIDA_MAX_BYTES (1ull << 40)
#define ARGS_FILE_MAX (1u << 20)        /* 1 MiB de arquivo de argumentos */
#define DIGITOS_MAX 15

static const char *const SEM_ARGUMENTOS[] = {NULL};

/* ---------- conversao das opcoes ---------- */

/* Digitos (1 a 15) no inicio do texto; *fim aponta o primeiro byte depois deles. false = nao comeca por digito. */
static bool LerDigitos(const char *texto, uint64_t *valor, const char **fim)
{
    const char *p = texto;

    *valor = 0;
    while (isdigit((unsigned char)*p) && p - texto < DIGITOS_MAX) *valor = *valor * 10 + (uint64_t)(*p++ - '0');
    *fim = p;
    return p > texto && !isdigit((unsigned char)*p);
}

/* Segundos: "5", "0.5" ou "12.25" (so digitos e um ponto). */
static bool LerTempo(const char *texto, double *segundos)
{
    uint64_t inteiro, fracao;
    const char *fim;

    if (!LerDigitos(texto, &inteiro, &fim)) return false;
    if (*fim == '.' && !LerDigitos(fim + 1, &fracao, &fim)) return false;
    if (*fim != '\0') return false;
    *segundos = strtod(texto, NULL);
    return *segundos >= TEMPO_MIN_S && *segundos <= TEMPO_MAX_S;
}

static bool LerMemoria(const char *texto, uint64_t *bytes)
{
    uint64_t mib;
    const char *fim;

    if (!LerDigitos(texto, &mib, &fim) || *fim != '\0' || mib < 1 || mib > MEM_MAX_MIB) return false;
    *bytes = mib << 20;
    return true;
}

/* Bytes: "65536", "64KiB" ou "1MiB" (KiB e MiB em qualquer caixa). */
static bool LerSaida(const char *texto, uint64_t *bytes)
{
    uint64_t n;
    const char *fim;

    if (!LerDigitos(texto, &n, &fim)) return false;
    if (_stricmp(fim, "KiB") == 0) n <<= 10;
    else if (_stricmp(fim, "MiB") == 0) n <<= 20;
    else if (*fim != '\0') return false;
    *bytes = n;
    return n >= 1 && n <= SAIDA_MAX_BYTES;
}

const char *LimitesLer(const Opcoes *o, Limites *limites)
{
    *limites = (Limites){0};
    if (o->limite_tempo != NULL && !LerTempo(o->limite_tempo, &limites->tempo_s)) return "--limite-tempo aceita segundos, de 0.001 a 86400 (ex.: 5 ou 0.5)";
    if (o->limite_mem != NULL && !LerMemoria(o->limite_mem, &limites->mem_bytes)) return "--limite-mem aceita um numero inteiro de MiB, de 1 a 1048576";
    if (o->limite_saida != NULL && !LerSaida(o->limite_saida, &limites->saida_bytes)) return "--limite-saida aceita bytes, KiB ou MiB (ex.: 65536, 64KiB, 1MiB), de 1 byte a 1 TiB";
    return NULL;
}

/* ---------- --args-file ---------- */

static char MotivoArquivo[ERRO_TEXTO_MAX + 600];

/* O arquivo inteiro em memoria (com NUL depois do fim). NULL = nao leu: o motivo fica em MotivoArquivo. */
static char *LerArquivoInteiro(const char *caminho, size_t *tam)
{
    FILE *f = fopen(caminho, "rb");
    char *dados;
    char causa[ERRO_TEXTO_MAX];

    if (f == NULL) {
        ErroCrtTexto(errno, causa, sizeof causa);
        snprintf(MotivoArquivo, sizeof MotivoArquivo, "--args-file: nao consegui abrir '%s' (%s)", caminho, causa);
        return NULL;
    }
    dados = malloc((size_t)ARGS_FILE_MAX + 1);
    *tam = dados != NULL ? fread(dados, 1, (size_t)ARGS_FILE_MAX + 1, f) : 0;
    fclose(f);
    if (dados == NULL || *tam > ARGS_FILE_MAX) {
        snprintf(MotivoArquivo, sizeof MotivoArquivo, "--args-file: '%s' passa de 1 MiB (ou faltou memoria)", caminho);
        free(dados);
        return NULL;
    }
    dados[*tam] = '\0';
    return dados;
}

/* Escapes da linha, no proprio lugar: \n vira quebra de linha, \\ vira uma barra; outra barra fica como esta. */
static void ResolverEscapes(char *linha)
{
    char *saida = linha;

    for (const char *p = linha; *p != '\0'; p++) {
        if (*p == '\\' && p[1] == 'n') {
            *saida++ = '\n';
            p++;
        } else if (*p == '\\' && p[1] == '\\') {
            *saida++ = '\\';
            p++;
        } else {
            *saida++ = *p;
        }
    }
    *saida = '\0';
}

/* Quantas linhas o texto tem (a ultima conta sem fim de linha; texto vazio = 0). */
static size_t ContarLinhas(const char *texto, size_t tam)
{
    size_t linhas = 0;

    for (size_t k = 0; k < tam; k++) linhas += texto[k] == '\n';
    return linhas + (tam > 0 && texto[tam - 1] != '\n');
}

/* Corta o texto em linhas (viram NUL), tira o CR do fim e resolve os escapes; lista[k] = inicio da linha k. */
static void SepararLinhas(char *texto, size_t tam, const char **lista)
{
    size_t n = 0;
    char *inicio = texto;

    for (size_t k = 0; k <= tam; k++) {
        bool fim_de_linha = texto[k] == '\n' || (k == tam && texto + k > inicio);
        if (!fim_de_linha) continue;
        texto[k] = '\0';
        if (texto + k > inicio && texto[k - 1] == '\r') texto[k - 1] = '\0';
        ResolverEscapes(inicio);
        lista[n++] = inicio;
        inicio = texto + k + 1;
    }
}

const char *LimitesPrepararArgumentos(Opcoes *o)
{
    size_t tam, linhas;
    char *texto;
    const char **lista;

    if (o->args_file == NULL) return NULL;
    texto = LerArquivoInteiro(o->args_file, &tam);
    if (texto == NULL) return MotivoArquivo;
    if (memchr(texto, '\0', tam) != NULL) {
        snprintf(MotivoArquivo, sizeof MotivoArquivo, "--args-file: '%s' tem byte nulo (argumento nao leva NUL)", o->args_file);
        return MotivoArquivo;
    }
    if (tam >= 3 && memcmp(texto, "\xEF\xBB\xBF", 3) == 0) {   /* BOM UTF-8 do Bloco de Notas */
        memmove(texto, texto + 3, tam - 2);
        tam -= 3;
    }
    linhas = ContarLinhas(texto, tam);
    lista = malloc((linhas + (size_t)o->nargs_programa + 1) * sizeof *lista);
    if (lista == NULL) FalhaInterna("memoria esgotada lendo o --args-file");
    SepararLinhas(texto, tam, lista);
    for (int k = 0; k < o->nargs_programa; k++) lista[linhas + (size_t)k] = o->args_programa[k];
    lista[linhas + (size_t)o->nargs_programa] = NULL;
    o->args_programa = lista;
    o->nargs_programa += (int)linhas;
    return NULL;
}

/* ---------- execucao ---------- */

static int LimiteDeTempo(const Opcoes *o)
{
    char causa[400];

    snprintf(causa, sizeof causa, "o programa passou do limite de tempo (--limite-tempo %s s) e foi encerrado com os processos filhos", o->limite_tempo);
    DiagImprimirJsonSemFonte(R0013, causa, stderr);
    return LIMITES_SAIDA_TEMPO;
}

static int LimiteDeMemoria(const Opcoes *o, const FilhoResultado *r)
{
    char causa[400];
    unsigned long long pico_mib = (r->pico_mem + (1ull << 20) - 1) >> 20;

    snprintf(causa, sizeof causa, "o programa passou do limite de memoria (--limite-mem %s MiB; pico medido: %llu MiB) e foi encerrado com os processos filhos", o->limite_mem, pico_mib);
    DiagImprimirJsonSemFonte(R0014, causa, stderr);
    return LIMITES_SAIDA_MEMORIA;
}

static int LimiteDeSaida(const Opcoes *o, const Limites *l, const FilhoResultado *r)
{
    char causa[500];

    snprintf(causa, sizeof causa, "o programa passou do limite de saida (--limite-saida %s = %llu bytes de stdout + stderr): a saida foi cortada nele, %llu bytes foram descartados e o programa foi encerrado",
             o->limite_saida, (unsigned long long)l->saida_bytes, (unsigned long long)r->descartados);
    DiagImprimirJsonSemFonte(R0015, causa, stderr);
    return LIMITES_SAIDA_SAIDA;
}

int LimitesExecutar(const Opcoes *o, const char *arq_exe)
{
    Limites l;
    FilhoResultado r = {0};
    uint32_t erro;
    char causa[ERRO_TEXTO_MAX];
    FilhoPedido pedido = {.exe = arq_exe, .args = o->nargs_programa > 0 ? o->args_programa : SEM_ARGUMENTOS, .enriquecer = !o->resumo};

    LimitesLer(o, &l);      /* ja conferido em OpcoesValidar */
    pedido.limite_tempo_s = l.tempo_s;
    pedido.limite_mem = l.mem_bytes;
    pedido.limite_saida = l.saida_bytes;
    if (!FilhoExecutar(&pedido, &r, &erro)) {
        ErroWinTexto(erro, causa, sizeof causa);
        fprintf(stderr, "forja: nao consegui iniciar %s (%s)\n", arq_exe, causa);
        return SAIDA_INTERNO;
    }
    if (r.fim == FILHO_ESTOUROU_TEMPO) return LimiteDeTempo(o);
    if (r.fim == FILHO_ESTOUROU_MEMORIA) return LimiteDeMemoria(o, &r);
    if (r.fim == FILHO_ESTOUROU_SAIDA) return LimiteDeSaida(o, &l, &r);
    return (int)r.codigo;
}
