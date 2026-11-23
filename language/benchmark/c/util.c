/* util.c - arena, buffer, simbolos, diagnosticos JSON, relogio e arquivos.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARENA_CHUNK_BYTES ((size_t)8 << 20)

struct ArenaChunk { struct ArenaChunk *prev; size_t cap; };

typedef struct { const char *codigo, *tipo, *explicacao, *forma; } CatalogItem;

static const CatalogItem CATALOG[CODE_COUNT] = {
    {"E0001", "lexico", "O texto tem um caractere que a Forja nao reconhece ou uma string sem fechar na linha.", "Remova o caractere ou feche a string com \"."},
    {"E0101", "sintaxe", "O token nao cabe aqui pela gramatica da Forja.", "Confira o comando: um por linha, com os delimitadores no lugar."},
    {"E0201", "nome", "O nome nao foi declarado antes do uso (ou falta proc main sem parametros e sem retorno).", "Declare o nome ou corrija a grafia."},
    {"E0301", "tipo", "Os tipos nao pertencem a mesma categoria e a Forja nao converte sozinha.", "Use 'as Tipo' para converter ou ajuste o tipo."},
    {"E0302", "tipo", "Proc com retorno precisa terminar em return.", "Acrescente 'return expr' como ultimo comando do corpo."},
    {"E0412", "unidade", "As dimensoes das unidades nao combinam, ou o resultado nao tem unidade declarada.", "Use a mesma unidade ou declare a unidade do resultado."},
    {"E0420", "faixa", "A constante esta fora da faixa declarada para o campo.", "Use um valor dentro da faixa 'in a..=b' ou 'a..<b'."},
    {"E0501", "argumento", "Argumento nomeado fora da ordem declarada ou repetido.", "Passe os argumentos nomeados na ordem do proc, sem repetir."},
    {"E0502", "argumento", "O 1o argumento e posicional e os demais precisam ser nomeados.", "Escreva proc(valor, nome = valor)."},
    {"E0503", "argumento", "Falta um argumento obrigatorio (sem padrao).", "Passe o argumento que falta ou de um padrao ao parametro."},
    {"E0504", "argumento", "O nome nao existe entre os parametros do proc ou os campos da struct.", "Use um nome declarado."},
};

void *ArenaAlloc(Ctx *c, size_t n)
{
    n = (n + 7) & ~(size_t)7;
    if (c->chunk == NULL || c->used + n > c->chunk->cap) {
        size_t cap = n + sizeof(ArenaChunk) > ARENA_CHUNK_BYTES ? n + sizeof(ArenaChunk) : ARENA_CHUNK_BYTES;
        ArenaChunk *novo = calloc(1, cap);
        if (novo == NULL) {
            fprintf(stderr, "forja: memoria esgotada pedindo %zu bytes na arena\n", n);
            exit(3);
        }
        novo->prev = c->chunk;
        novo->cap = cap;
        c->chunk = novo;
        c->used = sizeof(ArenaChunk);
    }
    void *p = (char *)c->chunk + c->used;
    c->used += n;
    return p;
}

static bool BufReserve(Buf *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap) return true;
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->len + extra + 1) cap *= 2;
    char *novo = realloc(b->data, cap);
    if (novo == NULL) {
        fprintf(stderr, "forja: memoria esgotada no buffer (%zu bytes pedidos)\n", cap);
        return false;
    }
    b->data = novo;
    b->cap = cap;
    return true;
}

bool BufGrowCat(Buf *b, const char *s, size_t n)
{
    if (!BufReserve(b, n)) return false;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

bool BufPrepare(Buf *b, size_t cap)
{
    return b->cap >= cap || BufReserve(b, cap - b->len);
}

bool BufCatU64(Buf *b, uint64_t v)
{
    char digitos[24];
    size_t i = sizeof digitos;
    do {
        digitos[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    return BufCatN(b, digitos + i, sizeof digitos - i);
}

bool BufPrintf(Buf *b, const char *fmt, ...)
{
    char texto[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(texto, sizeof texto, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof texto) {
        fprintf(stderr, "forja: BufPrintf excedeu %zu bytes com o formato '%s'\n", sizeof texto, fmt);
        return false;
    }
    return BufCatN(b, texto, (size_t)n);
}

uint64_t NowNs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint32_t HashBytes(const char *s, uint32_t len)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < len; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
    return h;
}

static bool GrowSymbolTable(Ctx *c)
{
    uint32_t hcap = c->hcap ? c->hcap * 2 : 1024;
    HashSlot *htab = malloc(hcap * sizeof(HashSlot));
    if (htab == NULL) return false;
    memset(htab, 0xFF, hcap * sizeof(HashSlot));
    for (int32_t i = 0; i < c->nsyms; i++) {
        uint32_t slot = c->syms[i].hash & (hcap - 1);
        while (htab[slot].idx >= 0) slot = (slot + 1) & (hcap - 1);
        htab[slot].idx = i;
        htab[slot].hash = c->syms[i].hash;
    }
    free(c->htab);
    c->htab = htab;
    c->hcap = hcap;
    return true;
}

static int32_t AppendSym(Ctx *c, const char *s, uint32_t len, uint32_t hash)
{
    if (c->nsyms == c->capsyms) {
        int32_t cap = c->capsyms ? c->capsyms * 2 : 1024;
        Sym *novo = realloc(c->syms, (size_t)cap * sizeof(Sym));
        if (novo == NULL) return -1;
        c->syms = novo;
        c->capsyms = cap;
    }
    Sym *y = &c->syms[c->nsyms];
    memset(y, 0, sizeof *y);
    y->s = s;
    y->len = len;
    y->hash = hash;
    y->unit_dim = -1;
    y->struct_idx = -1;
    return c->nsyms++;
}

int32_t InternSym(Ctx *c, const char *s, uint32_t len)
{
    uint32_t hash = HashBytes(s, len);
    if ((uint32_t)(c->nsyms + 1) * 2 > c->hcap && !GrowSymbolTable(c)) return -1;
    uint32_t slot = hash & (c->hcap - 1);
    while (c->htab[slot].idx >= 0) {
        if (c->htab[slot].hash == hash) {
            const Sym *y = &c->syms[c->htab[slot].idx];
            if (y->len == len && memcmp(y->s, s, len) == 0) return c->htab[slot].idx;
        }
        slot = (slot + 1) & (c->hcap - 1);
    }
    char *copia = ArenaAlloc(c, (size_t)len + 1); /* terminada em NUL para as mensagens */
    memcpy(copia, s, len);
    int32_t id = AppendSym(c, copia, len, hash);
    if (id >= 0) {
        c->htab[slot].idx = id;
        c->htab[slot].hash = hash;
    }
    return id;
}

bool InitSymbols(Ctx *c)
{
    static const char *const fixos[] = {
        "const", "let", "var", "proc", "struct", "unit", "if", "else", "for", "in", "while",
        "return", "as", "and", "or", "not", "true", "false",
        "i32", "int", "f32", "bool", "print", "main",
    };
    for (size_t i = 0; i < sizeof fixos / sizeof fixos[0]; i++) {
        if (InternSym(c, fixos[i], (uint32_t)strlen(fixos[i])) != (int32_t)i) {
            fprintf(stderr, "forja: falha ao pre-internar o simbolo '%s'\n", fixos[i]);
            return false;
        }
    }
    return true;
}

bool Report(Ctx *c, DiagCode code, uint32_t off, const char *fmt, ...)
{
    va_list ap;
    c->diag.has = true;
    c->diag.code = code;
    c->diag.off = off;
    va_start(ap, fmt);
    vsnprintf(c->diag.causa, sizeof c->diag.causa, fmt, ap);
    va_end(ap);
    return false;
}

static void LocateOffset(const Ctx *c, uint32_t off, uint32_t *line, uint32_t *col)
{
    uint32_t l = 1, ultimo_lf = 0;
    bool viu_lf = false;
    for (uint32_t i = 0; i < off && i < c->src_len; i++) {
        if (c->src[i] == '\n') { l++; ultimo_lf = i; viu_lf = true; }
    }
    *line = l;
    *col = viu_lf ? off - ultimo_lf : off + 1;
}

static void PrintJsonString(const char *s)
{
    putchar('"');
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { putchar('\\'); putchar(ch); }
        else if (ch < 0x20) printf("\\u%04x", ch);
        else putchar(ch);
    }
    putchar('"');
}

void PrintDiag(const Ctx *c)
{
    const CatalogItem *item = &CATALOG[c->diag.code];
    uint32_t line, col;
    LocateOffset(c, c->diag.off, &line, &col);
    printf("{\"codigo\": \"%s\", \"tipo\": \"%s\", \"arquivo\": ", item->codigo, item->tipo);
    char path[1024];
    snprintf(path, sizeof path, "%s", c->path);
    for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
    PrintJsonString(path);
    printf(", \"linha\": %u, \"coluna\": %u, \"causa\": ", line, col);
    PrintJsonString(c->diag.causa);
    printf(", \"explicacao\": ");
    PrintJsonString(item->explicacao);
    printf(", \"forma_certa\": ");
    PrintJsonString(item->forma);
    printf("}\n");
    fflush(stdout);
}

bool ReadWholeFile(const char *path, char **data, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "forja: nao consegui abrir '%s' para leitura\n", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = tam >= 0 ? malloc((size_t)tam + 1) : NULL;
    if (buf == NULL || fread(buf, 1, (size_t)tam, f) != (size_t)tam) {
        fprintf(stderr, "forja: falha lendo '%s' (%ld bytes)\n", path, tam);
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);
    buf[tam] = '\0';
    *data = buf;
    *len = (size_t)tam;
    return true;
}

bool WriteWholeFile(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "forja: nao consegui abrir '%s' para gravacao\n", path);
        return false;
    }
    bool ok = fwrite(data, 1, len, f) == len;
    ok = (fclose(f) == 0) && ok;
    if (!ok) fprintf(stderr, "forja: falha gravando %zu bytes em '%s'\n", len, path);
    return ok;
}

uint64_t CountLines(const char *s, size_t n)
{
    uint64_t linhas = 0;
    for (size_t i = 0; i < n; i++) if (s[i] == '\n') linhas++;
    if (n > 0 && s[n - 1] != '\n') linhas++;
    return linhas;
}
