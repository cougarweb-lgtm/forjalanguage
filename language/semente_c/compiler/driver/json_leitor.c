/* json_leitor.c - leitor minimo de JSON do driver (ver json_leitor.h): descida recursiva sobre o texto, valores alocados na Arena.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b05: primeira versao.
 * Verificacao: construir.bat && teste_common.exe
 */
#include <string.h>

#include "common/buf.h"
#include "driver/json_leitor.h"

typedef struct {
    Arena *arena;
    const char *p, *fim;
    int profundidade;
    Buf texto;              /* area de trabalho da decodificacao de textos (reaproveitada) */
} Leitor;

static JsonValor *LerValor(Leitor *l);

static void PularEspacos(Leitor *l)
{
    while (l->p < l->fim && (*l->p == ' ' || *l->p == '\t' || *l->p == '\n' || *l->p == '\r')) l->p++;
}

static JsonValor *NovoValor(Leitor *l, JsonTipo tipo)
{
    JsonValor *v = ArenaAlocar(l->arena, sizeof *v);

    v->tipo = tipo;
    return v;
}

/* ---------- texto ---------- */

/* Valor do digito hexadecimal; -1 se nao e digito. */
static int ValorHex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool LerHex4(Leitor *l, uint32_t *ponto)
{
    uint32_t v = 0;

    if (l->fim - l->p < 4) return false;
    for (int k = 0; k < 4; k++) {
        int digito = ValorHex(*l->p++);

        if (digito < 0) return false;
        v = (v << 4) | (uint32_t)digito;
    }
    *ponto = v;
    return true;
}

static void AnexarUtf8(Buf *b, uint32_t ponto)
{
    char u[4];
    size_t n;

    if (ponto < 0x80) {
        u[0] = (char)ponto;
        n = 1;
    } else if (ponto < 0x800) {
        u[0] = (char)(0xC0 | (ponto >> 6));
        u[1] = (char)(0x80 | (ponto & 0x3F));
        n = 2;
    } else if (ponto < 0x10000) {
        u[0] = (char)(0xE0 | (ponto >> 12));
        u[1] = (char)(0x80 | ((ponto >> 6) & 0x3F));
        u[2] = (char)(0x80 | (ponto & 0x3F));
        n = 3;
    } else {
        u[0] = (char)(0xF0 | (ponto >> 18));
        u[1] = (char)(0x80 | ((ponto >> 12) & 0x3F));
        u[2] = (char)(0x80 | ((ponto >> 6) & 0x3F));
        u[3] = (char)(0x80 | (ponto & 0x3F));
        n = 4;
    }
    BufAnexarN(b, u, n);
}

/* \uXXXX (ja lido o 'u'); par substituto alto exige o baixo logo depois. */
static bool LerEscapeUnicode(Leitor *l)
{
    uint32_t ponto, baixo;

    if (!LerHex4(l, &ponto)) return false;
    if (ponto >= 0xDC00 && ponto <= 0xDFFF) return false;
    if (ponto >= 0xD800 && ponto <= 0xDBFF) {
        if (l->fim - l->p < 2 || l->p[0] != '\\' || l->p[1] != 'u') return false;
        l->p += 2;
        if (!LerHex4(l, &baixo) || baixo < 0xDC00 || baixo > 0xDFFF) return false;
        ponto = 0x10000 + ((ponto - 0xD800) << 10) + (baixo - 0xDC00);
    }
    AnexarUtf8(&l->texto, ponto);
    return true;
}

/* Depois da barra: um dos escapes de caractere unico ou \uXXXX. */
static bool LerEscape(Leitor *l)
{
    static const char DE[] = "\"\\/bfnrt";
    static const char PARA[] = "\"\\/\b\f\n\r\t";
    const char *achou;
    char c;

    if (l->p >= l->fim) return false;
    c = *l->p++;
    if (c == 'u') return LerEscapeUnicode(l);
    achou = c != '\0' ? strchr(DE, c) : NULL;
    if (achou == NULL) return false;
    BufAnexarN(&l->texto, &PARA[achou - DE], 1);
    return true;
}

/* Texto entre aspas (l->p esta na aspa de abertura). NULL = texto torto. */
static const char *LerTexto(Leitor *l)
{
    l->p++;
    l->texto.tam = 0;
    BufAnexarN(&l->texto, "", 0);
    while (l->p < l->fim && *l->p != '"') {
        unsigned char c = (unsigned char)*l->p++;

        if (c < 0x20) return NULL;
        if (c != '\\') BufAnexarN(&l->texto, (const char *)&c, 1);
        else if (!LerEscape(l)) return NULL;
    }
    if (l->p >= l->fim) return NULL;
    l->p++;
    return ArenaCopiarTexto(l->arena, l->texto.dados != NULL ? l->texto.dados : "", l->texto.tam);
}

/* ---------- escalares ---------- */

static JsonValor *LerNumero(Leitor *l)
{
    JsonValor *v = NovoValor(l, JSON_NUMERO);
    bool negativo = *l->p == '-';
    int64_t n = 0;
    int digitos = 0;

    if (negativo) l->p++;
    while (l->p < l->fim && *l->p >= '0' && *l->p <= '9') {
        n = n * 10 + (*l->p++ - '0');
        digitos++;
    }
    if (digitos == 0) return NULL;
    while (l->p < l->fim && strchr("0123456789.eE+-", *l->p) != NULL) l->p++;   /* fracao e expoente: so a parte inteira interessa */
    v->numero = negativo ? -n : n;
    return v;
}

static JsonValor *LerLiteral(Leitor *l)
{
    static const struct { const char *nome; JsonTipo tipo; int64_t valor; } LITERAIS[] = {
        {"null", JSON_NULO, 0}, {"true", JSON_BOOL, 1}, {"false", JSON_BOOL, 0},
    };

    for (size_t k = 0; k < sizeof LITERAIS / sizeof LITERAIS[0]; k++) {
        size_t n = strlen(LITERAIS[k].nome);

        if ((size_t)(l->fim - l->p) >= n && memcmp(l->p, LITERAIS[k].nome, n) == 0) {
            JsonValor *v = NovoValor(l, LITERAIS[k].tipo);

            v->numero = LITERAIS[k].valor;
            l->p += n;
            return v;
        }
    }
    return NULL;
}

/* ---------- lista e objeto ---------- */

/* Membro de objeto: "chave": valor. */
static JsonValor *LerMembro(Leitor *l)
{
    const char *chave;
    JsonValor *v;

    PularEspacos(l);
    if (l->p >= l->fim || *l->p != '"') return NULL;
    chave = LerTexto(l);
    PularEspacos(l);
    if (chave == NULL || l->p >= l->fim || *l->p != ':') return NULL;
    l->p++;
    v = LerValor(l);
    if (v != NULL) v->chave = chave;
    return v;
}

/* Itens separados por virgula ate o fecha; l->p esta logo depois da abertura. */
static JsonValor *LerContainer(Leitor *l, JsonTipo tipo, char fecha)
{
    JsonValor *raiz = NovoValor(l, tipo);
    JsonValor **ponta = &raiz->filho;

    PularEspacos(l);
    if (l->p < l->fim && *l->p == fecha) {
        l->p++;
        return raiz;
    }
    for (;;) {
        JsonValor *item = tipo == JSON_OBJETO ? LerMembro(l) : LerValor(l);

        if (item == NULL) return NULL;
        *ponta = item;
        ponta = &item->prox;
        PularEspacos(l);
        if (l->p >= l->fim) return NULL;
        if (*l->p == fecha) {
            l->p++;
            return raiz;
        }
        if (*l->p++ != ',') return NULL;
    }
}

static JsonValor *LerValor(Leitor *l)
{
    JsonValor *v = NULL;
    char c;

    PularEspacos(l);
    if (l->p >= l->fim || l->profundidade >= JSON_PROFUNDIDADE_MAX) return NULL;
    c = *l->p;
    l->profundidade++;
    if (c == '"') {
        v = NovoValor(l, JSON_TEXTO);
        v->texto = LerTexto(l);
        if (v->texto == NULL) v = NULL;
    } else if (c == '{' || c == '[') {
        l->p++;
        v = LerContainer(l, c == '{' ? JSON_OBJETO : JSON_LISTA, c == '{' ? '}' : ']');
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        v = LerNumero(l);
    } else {
        v = LerLiteral(l);
    }
    l->profundidade--;
    return v;
}

/* ---------- interface ---------- */

JsonValor *JsonLer(Arena *arena, const char *texto, size_t tam)
{
    Leitor l = {.arena = arena, .p = texto, .fim = texto + tam};
    JsonValor *v = LerValor(&l);

    if (v != NULL) {
        PularEspacos(&l);
        if (l.p != l.fim) v = NULL;
    }
    BufLiberar(&l.texto);
    return v;
}

const JsonValor *JsonMembro(const JsonValor *objeto, const char *chave)
{
    if (objeto == NULL || objeto->tipo != JSON_OBJETO) return NULL;
    for (const JsonValor *m = objeto->filho; m != NULL; m = m->prox)
        if (m->chave != NULL && strcmp(m->chave, chave) == 0) return m;
    return NULL;
}

const char *JsonTextoDe(const JsonValor *objeto, const char *chave)
{
    const JsonValor *m = JsonMembro(objeto, chave);

    return m != NULL && m->tipo == JSON_TEXTO ? m->texto : NULL;
}

int64_t JsonNumeroDe(const JsonValor *objeto, const char *chave, int64_t padrao)
{
    const JsonValor *m = JsonMembro(objeto, chave);

    return m != NULL && m->tipo == JSON_NUMERO ? m->numero : padrao;
}
