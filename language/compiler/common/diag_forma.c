/* diag_forma.c - a explicacao e a forma_certa de cada OCORRENCIA, com os nomes do arquivo (contrato IA1, D-IA1-5, passo b08): a tabela FORMAS_DA_CAUSA
 * (diag_forma_tab.c) diz, por codigo e trecho da causa, o que aconteceu e o jeito certo, e as marcas $O $L $T $C $1 $2 trazem a linha, a causa e os nomes do .fj.
 * Versao: 0.1.3
 * Changelog: 0.1.3 - P72: NomeEntreAspas vira DiagNomeEntreAspas (declarada no diag_corrige.h, que a reaproveita).
 *            0.1.2 - IA1-L1: ProcurarRegra tambem procura o E0110 na tabela IDIOMAS (common/idiomas.c).
 *            0.1.1 - IA1-b08: Anexar e Expandir recebem a Montagem (texto, cap, uso) em vez de 5 parametros posicionais (CROSS-R05).
 *            0.1.0 - IA1-b08: primeira versao.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_forma_b08.py --forja compiler\forja.exe
 */
#include "common/diag_forma.h"

#include "common/idiomas.h"

#include <stdio.h>
#include <string.h>

enum { LINHA_MAX = 104, TRECHO_MAX = 64, NOME_MAX = 64, ONDE_MAX = 192 };

/* O que as marcas dos textos precisam saber da ocorrencia. */
typedef struct {
    char onde[ONDE_MAX], linha[LINHA_MAX], trecho[TRECHO_MAX], nome1[NOME_MAX], nome2[NOME_MAX];
    const char *causa;
} Ocorrencia;

/* ---------- pedacos do fonte ---------- */

/* Recua 'n' ate cair no inicio de um caractere UTF-8 (nunca corta um caractere no meio). */
static size_t InicioDeCaractere(const char *t, size_t n)
{
    while (n > 0 && ((unsigned char)t[n] & 0xC0) == 0x80) n--;
    return n;
}

static void TirarEspacosDoFim(char *s, size_t n)
{
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    s[n] = '\0';
}

/* A linha do fonte que contem 'off', sem o recuo e sem o comentario de fim de linha, cortada em LINHA_MAX - 1 bytes. */
static void LinhaDoOffset(const Fonte *f, uint32_t off, char *saida)
{
    const char *t = f->texto;
    uint32_t i = off < f->tam ? off : f->tam;
    size_t n = 0;

    while (i > 0 && t[i - 1] != '\n') i--;
    while (t[i] == ' ' || t[i] == '\t') i++;
    for (; t[i] != '\0' && t[i] != '\n' && t[i] != '\r'; i++) {
        if (t[i] == '/' && t[i + 1] == '/') break;
        if (n == LINHA_MAX - 1) {
            n = InicioDeCaractere(t + i - n, n);
            break;
        }
        saida[n++] = t[i];
    }
    TirarEspacosDoFim(saida, n);
}

/* O trecho que comeca em 'off': ate virgula, fecha-parenteses, fecha-colchete, fecha-chave, '//' ou o fim da linha. */
static void TrechoDoOffset(const Fonte *f, uint32_t off, char *saida)
{
    const char *t = f->texto;
    size_t n = 0;

    saida[0] = '\0';
    if (off >= f->tam) return;
    for (uint32_t i = off; t[i] != '\0' && t[i] != '\n' && t[i] != '\r' && n < TRECHO_MAX - 1; i++) {
        if (strchr(",)]}", t[i]) != NULL || (t[i] == '/' && t[i + 1] == '/')) break;
        saida[n++] = t[i];
    }
    TirarEspacosDoFim(saida, n);
}

/* O k-esimo (0, 1) texto entre aspas simples da causa; vazio se nao ha. */
void DiagNomeEntreAspas(const char *causa, int k, char *saida)
{
    const char *p = causa;

    saida[0] = '\0';
    for (int visto = 0; (p = strchr(p, '\'')) != NULL; visto++) {
        const char *fim = strchr(p + 1, '\'');
        size_t n;
        if (fim == NULL) return;
        if (visto == k) {
            n = (size_t)(fim - p - 1);
            if (n >= NOME_MAX) n = InicioDeCaractere(p + 1, NOME_MAX - 1);
            memcpy(saida, p + 1, n);
            saida[n] = '\0';
            return;
        }
        p = fim + 1;
    }
}

static void PrepararOcorrencia(Ocorrencia *o, const Diag *d, const Fonte *f)
{
    DiagDescreverOnde(f, d->off, o->onde, sizeof o->onde);
    LinhaDoOffset(f, d->off, o->linha);
    TrechoDoOffset(f, d->off, o->trecho);
    DiagNomeEntreAspas(d->causa, 0, o->nome1);
    DiagNomeEntreAspas(d->causa, 1, o->nome2);
    o->causa = d->causa;
}

/* ---------- texto ---------- */

/* Saida em montagem: 'cap' bytes, 'uso' ja escritos. */
typedef struct {
    char *texto;
    size_t cap, uso;
} Montagem;

/* Acrescenta 'texto' (ate 'n' bytes) a montagem; nunca passa de cap - 1. */
static void Anexar(Montagem *m, const char *texto, size_t n)
{
    size_t cabe = m->cap - 1 - m->uso;

    if (n > cabe) n = cabe;
    memcpy(m->texto + m->uso, texto, n);
    m->uso += n;
    m->texto[m->uso] = '\0';
}

static const char *ValorDaMarca(const Ocorrencia *o, char marca)
{
    switch (marca) {
    case 'O': return o->onde;
    case 'L': return o->linha;
    case 'T': return o->trecho;
    case 'C': return o->causa;
    case '1': return o->nome1[0] != '\0' ? o->nome1 : o->trecho;
    case '2': return o->nome2[0] != '\0' ? o->nome2 : o->trecho;
    default: return NULL;
    }
}

/* Expande as marcas de 'modelo' e acrescenta o resultado a montagem. */
static void Expandir(const char *modelo, const Ocorrencia *o, Montagem *m)
{
    for (const char *p = modelo; *p != '\0'; p++) {
        const char *valor = p[0] == '$' ? ValorDaMarca(o, p[1]) : NULL;
        if (p[0] == '$' && p[1] == '$') {
            Anexar(m, "$", 1);
            p++;
        } else if (valor != NULL) {
            Anexar(m, valor, strlen(valor));
            p++;
        } else {
            Anexar(m, p, 1);
        }
    }
}

static const FormaDaCausa *ProcurarRegra(const Diag *d)
{
    for (size_t i = 0; i < FORMAS_DA_CAUSA_QTD; i++) {
        const FormaDaCausa *r = &FORMAS_DA_CAUSA[i];
        if (r->codigo == d->codigo && strstr(d->causa, r->trecho_da_causa) != NULL) return r;
    }
    if (d->codigo != E0110) return NULL;
    for (size_t i = 0; i < IDI_QTD; i++) { /* IA1-L1: os textos dos idiomas de outras linguagens vivem em idiomas.c */
        if (strstr(d->causa, IDIOMAS[i].forma.trecho_da_causa) != NULL) return &IDIOMAS[i].forma;
    }
    return NULL;
}

/* Frase geral (codigo sem regra na tabela): a causa e a linha do .fj na explicacao; a forma do catalogo e a linha na forma. */
static void FrasesGerais(const Ocorrencia *o, const char *forma_catalogo, FormaTextos *saida)
{
    Montagem explicacao = {saida->explicacao, sizeof saida->explicacao, 0};
    Montagem forma = {saida->forma, sizeof saida->forma, 0};

    saida->explicacao[0] = saida->forma[0] = '\0';
    Expandir("Na $O a Forja recusou a linha '$L': $C.", o, &explicacao);
    Anexar(&forma, forma_catalogo, strlen(forma_catalogo));
    Expandir(" (na linha '$L')", o, &forma);
}

void DiagFormaDaOcorrencia(const Diag *d, const Fonte *f, const char *forma_catalogo, FormaTextos *saida)
{
    Ocorrencia o;
    const FormaDaCausa *regra = ProcurarRegra(d);
    Montagem explicacao = {saida->explicacao, sizeof saida->explicacao, 0};
    Montagem forma = {saida->forma, sizeof saida->forma, 0};

    PrepararOcorrencia(&o, d, f);
    if (regra != NULL) {
        saida->explicacao[0] = saida->forma[0] = '\0';
        Expandir(regra->explicacao, &o, &explicacao);
        Expandir(regra->forma, &o, &forma);
    } else {
        FrasesGerais(&o, forma_catalogo, saida);
    }
    if (d->explicacao[0] != '\0') snprintf(saida->explicacao, sizeof saida->explicacao, "%s", d->explicacao);
    if (d->forma[0] != '\0') snprintf(saida->forma, sizeof saida->forma, "%s", d->forma);
}
