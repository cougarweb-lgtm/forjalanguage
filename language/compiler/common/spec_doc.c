/* spec_doc.c - leitura da doc embutida: linhas, secoes, mapa codigo -> secao e o trecho minimo da spec de um codigo (ver spec_doc.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao. O trecho do codigo e a clausula (separada por ';' ou ". " fora de crases e parenteses) da secao do mapa que cita o codigo.
 * Verificacao: construir.bat && teste_common.exe && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 */
#include "common/spec_doc.h"

#include <stdlib.h>
#include <string.h>

#include "common/diag_pacote.h"

enum { MAPA_CODIGO_TAM = 5 };

bool SpecProximaLinha(DocTexto *resto, DocTexto *linha)
{
    size_t n = 0;
    size_t avancar;

    if (resto->tam == 0) return false;
    while (n < resto->tam && resto->texto[n] != '\n') n++;
    linha->texto = resto->texto;
    linha->tam = n > 0 && resto->texto[n - 1] == '\r' ? n - 1 : n;
    avancar = n < resto->tam ? n + 1 : n;
    resto->texto += avancar;
    resto->tam -= avancar;
    return true;
}

/* "== nome ==" -> nome em 'nome'. false = a linha nao e titulo de secao. */
static bool LerTitulo(DocTexto linha, char nome[SPEC_NOME_MAX])
{
    size_t n;

    if (linha.tam < 7 || memcmp(linha.texto, "== ", 3) != 0 || memcmp(linha.texto + linha.tam - 3, " ==", 3) != 0) return false;
    n = linha.tam - 6;
    if (n >= SPEC_NOME_MAX) n = SPEC_NOME_MAX - 1;
    memcpy(nome, linha.texto + 3, n);
    nome[n] = '\0';
    return true;
}

static void FecharSecao(SpecSecao *s, const char *fim)
{
    s->bloco.tam = (size_t)(fim - s->bloco.texto);
    s->corpo.tam = (size_t)(fim - s->corpo.texto);
}

size_t SpecSecoes(DocArquivo fonte, SpecSecao *saida, size_t cap)
{
    DocTexto total = DocEmbutida(fonte);
    DocTexto resto = total, linha;
    size_t n = 0;

    for (;;) {
        const char *inicio = resto.texto;
        char nome[SPEC_NOME_MAX];

        if (!SpecProximaLinha(&resto, &linha)) break;
        if (!LerTitulo(linha, nome)) continue;
        if (n > 0) FecharSecao(&saida[n - 1], inicio);
        if (n == cap) return n;
        memcpy(saida[n].nome, nome, sizeof nome);
        saida[n].bloco.texto = inicio;
        saida[n].corpo.texto = resto.texto;
        n++;
    }
    if (n > 0) FecharSecao(&saida[n - 1], total.texto + total.tam);
    return n;
}

const SpecSecao *SpecAcharSecao(const SpecSecao *lista, size_t n, const char *nome)
{
    for (size_t k = 0; k < n; k++)
        if (strcmp(lista[k].nome, nome) == 0) return &lista[k];
    return NULL;
}

bool SpecSecaoDoCodigo(const char *codigo, char secao[SPEC_NOME_MAX])
{
    DocTexto resto = DocEmbutida(DOC_MAPA), linha;

    SpecProximaLinha(&resto, &linha); /* cabecalho "codigo<TAB>secao" */
    while (SpecProximaLinha(&resto, &linha)) {
        size_t n = linha.tam > MAPA_CODIGO_TAM + 1 ? linha.tam - MAPA_CODIGO_TAM - 1 : 0;

        if (n == 0 || linha.texto[MAPA_CODIGO_TAM] != '\t' || memcmp(linha.texto, codigo, MAPA_CODIGO_TAM) != 0 || codigo[MAPA_CODIGO_TAM] != '\0') continue;
        if (n >= SPEC_NOME_MAX) n = SPEC_NOME_MAX - 1;
        memcpy(secao, linha.texto + MAPA_CODIGO_TAM + 1, n);
        secao[n] = '\0';
        return true;
    }
    return false;
}

/* ---------- trecho minimo ---------- */

/* Posicao do ';' ou do '.' (seguido de espaco ou do fim da linha) que fecha a clausula que comeca em 'inicio', fora de crases e de parenteses; linha.tam se nenhum. */
static size_t FimDaClausula(DocTexto linha, size_t inicio)
{
    int fundo = 0;
    bool crase = false;

    for (size_t i = inicio; i < linha.tam; i++) {
        char c = linha.texto[i];
        bool ponto_final = (c == ';' || c == '.') && (i + 1 == linha.tam || linha.texto[i + 1] == ' ');

        if (c == '`') crase = !crase;
        else if (crase) continue;
        else if (c == '(') fundo++;
        else if (c == ')' && fundo > 0) fundo--;
        else if (ponto_final && fundo == 0) return i;
    }
    return linha.tam;
}

/* Proxima clausula de *resto (uma linha). false = so sobrou espaco. */
static bool ProximaClausula(DocTexto *resto, DocTexto *clausula)
{
    size_t inicio = 0;
    size_t fim;
    size_t consumido;

    while (inicio < resto->tam && resto->texto[inicio] == ' ') inicio++;
    if (inicio >= resto->tam) return false;
    fim = FimDaClausula(*resto, inicio);
    clausula->texto = resto->texto + inicio;
    clausula->tam = fim - inicio;
    consumido = fim < resto->tam ? fim + 1 : fim;
    resto->texto += consumido;
    resto->tam -= consumido;
    return true;
}

static bool EhAlfanumerico(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool CitaCodigo(DocTexto clausula, const char *codigo)
{
    size_t n = strlen(codigo);

    for (size_t i = 0; i + n <= clausula.tam; i++) {
        bool antes = i > 0 && EhAlfanumerico(clausula.texto[i - 1]);
        bool depois = i + n < clausula.tam && EhAlfanumerico(clausula.texto[i + n]);

        if (!antes && !depois && memcmp(clausula.texto + i, codigo, n) == 0) return true;
    }
    return false;
}

/* Linhas de texto do corpo (sem as de blocos ``` e sem as vazias), uma por chamada. false = acabou. */
static bool ProximaLinhaDeTexto(DocTexto *resto, DocTexto *linha, bool *em_bloco)
{
    while (SpecProximaLinha(resto, linha)) {
        if (linha->tam >= 3 && memcmp(linha->texto, "```", 3) == 0) {
            *em_bloco = !*em_bloco;
            continue;
        }
        if (!*em_bloco && linha->tam > 0) return true;
    }
    return false;
}

/* A clausula do corpo que cita o codigo. */
static bool AcharClausulaDoCodigo(DocTexto corpo, const char *codigo, DocTexto *achada)
{
    DocTexto linha;
    bool em_bloco = false;

    while (ProximaLinhaDeTexto(&corpo, &linha, &em_bloco)) {
        DocTexto clausula;

        while (ProximaClausula(&linha, &clausula)) {
            if (!CitaCodigo(clausula, codigo)) continue;
            *achada = clausula;
            return true;
        }
    }
    return false;
}

/* Sem clausula que cite o codigo: o comeco da 1a linha de texto da secao, clausula por clausula, ate SPEC_TRECHO_MAX bytes (pelo menos uma). */
static bool ComecoDaSecao(DocTexto corpo, DocTexto *achado)
{
    DocTexto linha, clausula, resto;
    bool em_bloco = false;
    size_t fim = 0;

    if (!ProximaLinhaDeTexto(&corpo, &linha, &em_bloco)) return false;
    resto = linha;
    while (ProximaClausula(&resto, &clausula)) {
        size_t ate = (size_t)(clausula.texto - linha.texto) + clausula.tam;

        if (fim > 0 && ate > SPEC_TRECHO_MAX) break;
        fim = ate;
    }
    achado->texto = linha.texto;
    achado->tam = fim;
    return true;
}

/* Copia o texto para saida; acima de SPEC_TRECHO_MAX (ou do que cabe em cap) corta em palavra e termina em "...". Devolve o tamanho. */
static size_t CopiarCortado(DocTexto texto, char *saida, size_t cap)
{
    size_t n = texto.tam;

    if (n > SPEC_TRECHO_MAX) n = SPEC_TRECHO_MAX;
    if (n > cap - 4) n = cap - 4;
    if (n < texto.tam) {
        while (n > 1 && texto.texto[n] != ' ') n--;
        while (n > 1 && ((unsigned char)texto.texto[n] & 0xC0) == 0x80) n--;
    }
    memcpy(saida, texto.texto, n);
    if (n < texto.tam) {
        memcpy(saida + n, "...", 3);
        n += 3;
    }
    saida[n] = '\0';
    return n;
}

size_t SpecTrechoDoCodigo(const char *codigo, const char *secao, char *saida, size_t cap)
{
    SpecSecao lista[SPEC_SECOES_MAX];
    const SpecSecao *s = SpecAcharSecao(lista, SpecSecoes(DOC_SPEC, lista, SPEC_SECOES_MAX), secao);
    DocTexto achado;

    if (s == NULL) return 0;
    if (!AcharClausulaDoCodigo(s->corpo, codigo, &achado) && !ComecoDaSecao(s->corpo, &achado)) return 0;
    return CopiarCortado(achado, saida, cap);
}

void SpecImprimirTextoJson(FILE *saida, DocTexto texto)
{
    char *copia = malloc(texto.tam + 1);
    size_t n = 0;

    if (copia == NULL) {
        fputs("\"\"", saida);
        return;
    }
    for (size_t i = 0; i < texto.tam; i++)
        if (texto.texto[i] != '\r') copia[n++] = texto.texto[i];
    copia[n] = '\0';
    ImprimirTextoJson(saida, copia);
    free(copia);
}
