/* rcode_runtime.c - R-code enriquecido pelo driver (ver rcode_runtime.h).
 * Versao: 0.7.1
 * Changelog: 0.7.1 - M6e P93k: forma_certa do R0008 "acesso alem do fim do bloco" (bloco vivo), nao a do delete.
 *            0.7.0 - M6e P93c: forma_certa do R0008 pelo campo "bloco" (heap, lista, arena).
 *            0.6.0 - M6d P92f: forma_certa do R0009 handle kind forja_fs_abrir (file.close). 0.5.0 - M6d P92d: forma_certa do R0009 "tipo":"lib" (libere o que <lib> devolveu com o proc destroys; o complemento "ends" entra no P92e).
 *            0.4.0 - M6c P88: forma_certa do R0009 "tipo":"handle" automatico (causa "falta fechar (destroys)"): nomeia o destruidor declarado ("destruidor" da linha) ou manda fechar com o proc `destroys` da lib.
 *            0.3.0 - M6 P71b: forma_certa do R0009 "tipo":"handle" (untrack_handle quando liberar <kind>). 0.2.0 - M6 P70: forma_certa do R0009 pela pilha da linha (proc onde o bloco nasceu) e por Arena/Pool vivo (D-IA1-14, D-M6-7). 0.1.0 - IA1-b10: primeira versao.
 * Verificacao: construir.bat && python -I ..\tests\ia_first\verificar_limites_b10.py --forja .\forja.exe
 */
#include "driver/rcode_runtime.h"

#include <stdio.h>
#include <string.h>

#include "common/diag.h"
#include "common/diag_spec.h"

#define PREFIXO_RCODE "{\"codigo\": \"R"
#define PREFIXO_TAM 13              /* strlen(PREFIXO_RCODE) */
#define CODIGO_INICIO 12            /* o 'R' do codigo, logo depois de {"codigo": " */
#define CODIGO_TAM 5                /* R0003 */

/* Texto JSON entre aspas em b: aspas, barra e controles escapados; os bytes UTF-8 passam como vieram. */
static void AnexarTextoJson(Buf *b, const char *s)
{
    BufAnexarN(b, "\"", 1);
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        if (*p == '"' || *p == '\\') BufFormatar(b, "\\%c", *p);
        else if (*p == '\n') BufAnexarN(b, "\\n", 2);
        else if (*p < 0x20) BufFormatar(b, "\\u%04x", *p);
        else BufAnexarN(b, (const char *)p, 1);
    }
    BufAnexarN(b, "\"", 1);
}

static bool Contem(const char *dados, size_t n, const char *trecho)
{
    size_t m = strlen(trecho);

    for (size_t i = 0; i + m <= n; i++) {
        if (memcmp(dados + i, trecho, m) == 0) return true;
    }
    return false;
}

/* O codigo do catalogo que a linha abre (R + 4 digitos entre aspas), ou -1 se a linha nao e JSON de R-code do catalogo. */
static int CodigoDaLinha(const char *linha, size_t fim)
{
    char codigo[CODIGO_TAM + 1];

    if (fim < PREFIXO_TAM + 6 || memcmp(linha, PREFIXO_RCODE, PREFIXO_TAM) != 0) return -1;
    if (linha[CODIGO_INICIO + CODIGO_TAM] != '"' || linha[fim - 1] != '}') return -1;
    memcpy(codigo, linha + CODIGO_INICIO, CODIGO_TAM);
    codigo[CODIGO_TAM] = '\0';
    return DiagCodigoPorTexto(codigo);
}

/* O texto entre aspas que vem logo depois de 'chave' na linha, copiado em dest (tam bytes); false se a chave nao esta ou o texto nao cabe. */
static bool TextoDepois(const char *linha, size_t fim, const char *chave, char *dest, size_t tam)
{
    size_t m = strlen(chave);

    for (size_t i = 0; i + m < fim; i++) {
        if (memcmp(linha + i, chave, m) != 0) continue;
        size_t n = 0;
        while (i + m + n < fim && linha[i + m + n] != '"') n++;
        if (n + 1 > tam || i + m + n >= fim) return false;
        memcpy(dest, linha + i + m, n);
        dest[n] = '\0';
        return true;
    }
    return false;
}

/* D-IA1-14 / M6 P70: forma_certa do R0009 conforme o que vazou. Bloco de heap com pilha: "delete(p) antes de sair de <proc>, ou defer delete(p)"; Arena/Pool vivo: destroy. */
static bool FormaDoR0009(const char *linha, size_t fim, char *forma, size_t tam)
{
    char proc[96];

    if (Contem(linha, fim, "\"tipo\": \"arena\"")) {
        snprintf(forma, tam, "mem.arena_destroy(a) antes de sair do programa, ou defer mem.arena_destroy(a) logo depois do mem.arena_create()");
        return true;
    }
    if (Contem(linha, fim, "\"tipo\": \"pool\"")) {
        snprintf(forma, tam, "mem.pool_destroy(p) antes de sair do programa, ou defer mem.pool_destroy(p) logo depois do mem.pool_create()");
        return true;
    }
    if (Contem(linha, fim, "\"tipo\": \"handle\"")) {
        char kind[48];

        char destruidor[48];

        if (!TextoDepois(linha, fim, "\"kind\": \"", kind, sizeof kind)) snprintf(kind, sizeof kind, "handle");
        if (strcmp(kind, "forja_fs_abrir") == 0) { /* M6d P92f: tabela literal de 1 entrada (std:file) */
            snprintf(forma, tam, "feche com file.close(&f) (ou defer file.close(&f) logo depois do open)");
            return true;
        }
        if (Contem(linha, fim, "falta fechar (destroys)")) { /* M6c P88: rastreio automatico do foreign com clausula */
            if (TextoDepois(linha, fim, "\"destruidor\": \"", destruidor, sizeof destruidor))
                snprintf(forma, tam, "chame %s(h) antes de sair do proc que abriu o handle, ou defer %s(h) logo depois de %s", destruidor, destruidor, kind);
            else
                snprintf(forma, tam, "chame o proc da lib que fecha o handle de %s (o que declara destroys), ou defer essa chamada logo depois dela", kind);
            return true;
        }
        snprintf(forma, tam, "mem.untrack_handle(h) quando liberar %s (depois do free ou do close da lib C), ou defer mem.untrack_handle(h) logo depois do mem.track_handle()", kind);
        return true;
    }
    if (Contem(linha, fim, "\"tipo\": \"lib\"")) { /* M6d P92d: memoria de lib estatica (dev) */
        char lib[48];

        if (!TextoDepois(linha, fim, "\"lib\": \"", lib, sizeof lib)) snprintf(lib, sizeof lib, "da lib");
        snprintf(forma, tam, "libere o que %s devolveu nesta linha com o proc da lib que o destroi (destroys)", lib);
        return true;
    }
    if (!TextoDepois(linha, fim, "\"pilha\": [{\"proc\": \"", proc, sizeof proc)) return false;
    snprintf(forma, tam, "delete(p) antes de sair de %s (onde o bloco nasceu), ou defer delete(p) logo depois do new; se o bloco sai do proc, entregue o ponteiro a quem o apaga", proc);
    return true;
}

/* M6e P93c (D-M6e-4, ERRATA E2/E3): forma_certa do R0008 pelo campo "bloco" (heap|lista|arena); sem o campo (pool, frame) vale a do catalogo. */
static bool FormaDoR0008(const char *linha, size_t fim, char *forma, size_t tam)
{
    if (Contem(linha, fim, "acesso alem do fim do bloco")) { /* P93k: guarda, bloco VIVO: nao e uso depois do delete */
        snprintf(forma, tam, "o acesso passou do fim do bloco: confira o indice ou o tamanho (len) antes de ler ou escrever, ou aumente o bloco");
        return true;
    }
    if (Contem(linha, fim, "\"bloco\": \"heap\"")) {
        snprintf(forma, tam, "o bloco ja foi liberado por outro caminho (campo de estrutura ou outro proc); libere em um lugar so, depois do ultimo uso; delete(p) so zera o nome p");
        return true;
    }
    if (Contem(linha, fim, "\"bloco\": \"lista\"")) {
        snprintf(forma, tam, "guarde o indice, nao ^T, de lista que cresce: o append moveu o buffer");
        return true;
    }
    if (Contem(linha, fim, "\"bloco\": \"arena\"")) {
        snprintf(forma, tam, "o objeto morreu no arena_destroy: copie para o heap antes, ou destrua a arena depois do ultimo uso");
        return true;
    }
    return false;
}

bool RcodeEnriquecerLinha(const char *linha, size_t tam, Buf *saida)
{
    size_t fim = tam;
    char spec[DIAG_SPEC_MAX];
    char forma[256];
    int indice;

    while (fim > 0 && (linha[fim - 1] == '\n' || linha[fim - 1] == '\r')) fim--;
    indice = CodigoDaLinha(linha, fim);
    if (indice < 0 || Contem(linha, fim, "\"forma_certa\"")) {
        BufAnexarN(saida, linha, tam);
        return false;
    }
    DiagSpecTexto(DiagCodigoTexto((DiagCodigo)indice), spec, sizeof spec);
    BufAnexarN(saida, linha, fim - 1);
    BufAnexar(saida, ", \"explicacao\": ");
    AnexarTextoJson(saida, DiagCodigoExplicacao((DiagCodigo)indice));
    BufAnexar(saida, ", \"forma_certa\": ");
    if (strcmp(DiagCodigoTexto((DiagCodigo)indice), "R0009") == 0 && FormaDoR0009(linha, fim, forma, sizeof forma))
        AnexarTextoJson(saida, forma);
    else if (strcmp(DiagCodigoTexto((DiagCodigo)indice), "R0008") == 0 && FormaDoR0008(linha, fim, forma, sizeof forma))
        AnexarTextoJson(saida, forma);
    else
        AnexarTextoJson(saida, DiagCodigoForma((DiagCodigo)indice));
    BufAnexar(saida, ", \"spec\": ");
    AnexarTextoJson(saida, spec);
    BufAnexarN(saida, "}", 1);
    BufAnexarN(saida, linha + fim, tam - fim);
    return true;
}

void RcodeFiltroIniciar(RcodeFiltro *f)
{
    f->n = 0;
    f->passando = false;
}

/* A linha candidata deixou de ser: o que ficou retido sai como veio e o resto da linha passa direto (a menos que este byte tenha fechado a linha). */
static void SoltarRetido(RcodeFiltro *f, char c, Buf *saida)
{
    BufAnexarN(saida, f->linha, f->n);
    f->n = 0;
    f->passando = c != '\n';
}

static void AlimentarByte(RcodeFiltro *f, char c, Buf *saida)
{
    if (f->passando) {
        BufAnexarN(saida, &c, 1);
        f->passando = c != '\n';
        return;
    }
    f->linha[f->n++] = c;
    if (f->n <= PREFIXO_TAM && c != PREFIXO_RCODE[f->n - 1]) {
        SoltarRetido(f, c, saida);
        return;
    }
    if (c == '\n') {
        RcodeEnriquecerLinha(f->linha, f->n, saida);
        f->n = 0;
    } else if (f->n == RCODE_LINHA_MAX) {
        SoltarRetido(f, c, saida);
    }
}

void RcodeFiltroAlimentar(RcodeFiltro *f, const char *dados, size_t n, Buf *saida)
{
    for (size_t k = 0; k < n; k++) AlimentarByte(f, dados[k], saida);
}

void RcodeFiltroFechar(RcodeFiltro *f, Buf *saida)
{
    BufAnexarN(saida, f->linha, f->n);
    f->n = 0;
}
