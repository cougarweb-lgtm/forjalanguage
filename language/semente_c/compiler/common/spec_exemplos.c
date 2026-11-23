/* spec_exemplos.c - leitura do exemplos.txt embutido (ver spec_exemplos.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao.
 * Verificacao: construir.bat && teste_common.exe && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 */
#include "common/spec_exemplos.h"

#include <string.h>

#include "common/spec_doc.h"

enum { PREFIXO_TAM = 2 };

static bool LerNumero(DocTexto *resto, uint32_t *valor)
{
    size_t n = 0;

    *valor = 0;
    while (n < resto->tam && resto->texto[n] >= '0' && resto->texto[n] <= '9') *valor = *valor * 10 + (uint32_t)(resto->texto[n++] - '0');
    resto->texto += n;
    resto->tam -= n;
    return n > 0;
}

/* Palavra de *resto ate o espaco (que e consumido). */
static DocTexto LerPalavra(DocTexto *resto)
{
    DocTexto palavra = {resto->texto, 0};
    size_t consumir;

    while (palavra.tam < resto->tam && resto->texto[palavra.tam] != ' ') palavra.tam++;
    consumir = palavra.tam < resto->tam ? palavra.tam + 1 : palavra.tam;
    resto->texto += consumir;
    resto->tam -= consumir;
    return palavra;
}

/* "@@ codigo fixture linha primeira": false = cabecalho malformado. */
static bool LerCabecalho(DocTexto linha, SpecExemplo *e)
{
    DocTexto codigo;

    linha.texto += 3;
    linha.tam -= 3;
    codigo = LerPalavra(&linha);
    if (codigo.tam == 0 || codigo.tam >= EXEMPLO_CODIGO_MAX) return false;
    memcpy(e->codigo, codigo.texto, codigo.tam);
    e->codigo[codigo.tam] = '\0';
    e->fixture = LerPalavra(&linha);
    if (e->fixture.tam == 0 || !LerNumero(&linha, &e->linha)) return false;
    LerPalavra(&linha); /* o espaco entre os dois numeros */
    return LerNumero(&linha, &e->primeira);
}

static bool EhCabecalho(DocTexto linha)
{
    return linha.tam >= 3 && memcmp(linha.texto, "@@ ", 3) == 0;
}

/* Estende o bloco ate o fim da linha (o 1o byte do bloco e o inicio da 1a linha dele). */
static void AcrescentarLinha(DocTexto *bloco, DocTexto linha)
{
    if (bloco->tam == 0) bloco->texto = linha.texto;
    bloco->tam = (size_t)(linha.texto - bloco->texto) + linha.tam;
}

static void ClassificarLinha(SpecExemplo *e, DocTexto linha)
{
    if (linha.tam == 0) return;
    if (linha.texto[0] == '=') {
        e->forma.texto = linha.texto + (linha.tam >= PREFIXO_TAM ? PREFIXO_TAM : linha.tam);
        e->forma.tam = linha.tam >= PREFIXO_TAM ? linha.tam - PREFIXO_TAM : 0;
    } else if (linha.texto[0] == '<') {
        AcrescentarLinha(&e->errado, linha);
    } else if (linha.texto[0] == '>') {
        AcrescentarLinha(&e->certo, linha);
    }
}

bool SpecProximoExemplo(DocTexto *resto, SpecExemplo *e)
{
    DocTexto linha;
    bool abriu = false;

    memset(e, 0, sizeof *e);
    for (;;) {
        DocTexto antes = *resto;

        if (!SpecProximaLinha(resto, &linha)) break;
        if (EhCabecalho(linha)) {
            if (abriu) {
                *resto = antes;
                break;
            }
            abriu = LerCabecalho(linha, e);
            continue;
        }
        if (abriu) ClassificarLinha(e, linha);
    }
    return abriu;
}

bool SpecExemploDoCodigo(const char *codigo, SpecExemplo *e)
{
    DocTexto resto = DocEmbutida(DOC_EXEMPLOS);

    while (SpecProximoExemplo(&resto, e))
        if (strcmp(e->codigo, codigo) == 0) return true;
    return false;
}

void SpecImprimirExemploJson(FILE *saida, const SpecExemplo *e)
{
    fprintf(saida, "{\"codigo\": \"%s\", \"fixture\": ", e->codigo);
    SpecImprimirTextoJson(saida, e->fixture);
    fprintf(saida, ", \"linha\": %u, \"primeira_linha\": %u, \"errado\": ", e->linha, e->primeira);
    SpecImprimirLinhasJson(saida, e->errado);
    fputs(", \"certo\": ", saida);
    if (e->certo.tam == 0) fputs("null", saida);
    else SpecImprimirLinhasJson(saida, e->certo);
    fputs(", \"forma_certa\": ", saida);
    SpecImprimirTextoJson(saida, e->forma);
    fputc('}', saida);
}

void SpecImprimirLinhasJson(FILE *saida, DocTexto bloco)
{
    DocTexto linha;
    bool primeira = true;

    fputc('[', saida);
    while (SpecProximaLinha(&bloco, &linha)) {
        DocTexto texto = {linha.texto + (linha.tam >= PREFIXO_TAM ? PREFIXO_TAM : linha.tam), linha.tam >= PREFIXO_TAM ? linha.tam - PREFIXO_TAM : 0};

        if (!primeira) fputs(", ", saida);
        SpecImprimirTextoJson(saida, texto);
        primeira = false;
    }
    fputc(']', saida);
}
