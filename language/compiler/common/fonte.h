/* fonte.h - arquivo-fonte inteiro em memoria, mapa offset -> linha/coluna e E/S de arquivo.
 * Versao: 0.8.0
 * Changelog: 0.8.0 - Linguagem v1.13 L1a: FonteTrechoDoOffset (indice do trecho que contem o offset; modulo de pasta = varios trechos por modulo).
 *            0.7.0 - S4 P2: FonteAdotarTexto (texto em memoria copiado, sem disco; a raiz sintetica do forja test <alvo>).
 *            0.6.3 - IA1-b03: so o comentario de FonteAnexarArquivo (arquivo que nao abre nao escreve no stderr).
 *            0.6.2 - M3 F8: ArquivoGravarAtomicoSemDescarga.
 *            0.6.1- M3 P22-O1b: ACCESS_DENIED (destino carregado) espera 1 vez so antes de renomear o antigo; as 8 esperas ficam para SHARING_VIOLATION (so o comentario de ArquivoGravarAtomico mudou).
 *            0.6.0 - M3 P22-O1a: ArquivoGravarAtomico e ArquivoGravarAtomicoAviso.
 *            0.5.0 - Linguagem v1.11 L6: FonteLerDeStream (--stdin: le o .fj de um stream ate o EOF, binario).
 *            0.4.1 - M2 P9a: ArquivoGravarCalado.
 *            0.4.0 - Linguagem v1.8 P1: trechos (varios modulos num texto so), FonteAnexarTexto/Arquivo e FonteLocalizarNoArquivo.
 *            0.3.0 - Fase 5i: FONTE_TAM_MAX (1 GiB), contrato do NUL final e BOM UTF-8 descartado na leitura.
 *            0.2.0 - Fase 5i: tabela de inicios de linha montada na leitura; FonteLocalizar passa de O(n) para O(log n)
 *                    (a varredura desde o byte 0 tornava a emissao do C quadratica: 3,7 s em 20 mil linhas).
 *            0.1.0 - Fase 5a: posicao continua sendo offset em bytes; linha/coluna so se calculam ao reportar.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_FONTE_H
#define FORJA_FONTE_H

#include <stdio.h>

#include "common/base.h"

typedef struct { uint32_t inicio, fim, linha_base; const char *caminho; } FonteTrecho;

/* ntrechos 0 = um trecho so (o arquivo inteiro, f->caminho). inicio_linha[k] = offset do 1o byte da linha k+1 (inicio_linha[0] = 0); NULL = sem tabela (FonteDeTexto), e
 * FonteLocalizar varre o texto. ninicios = quantidade de entradas (LFs + 1). */
typedef struct {
    const char *caminho;
    char *texto;
    size_t tam;
    uint32_t *inicio_linha;
    uint32_t ninicios;
    FonteTrecho *trechos;
    uint32_t ntrechos;
} Fonte;

/* Arquivo de origem de uma faixa de offsets quando varios modulos dividem o mesmo texto. linha_base = linha global do
 * inicio do trecho menos 1 (linha local = linha global - linha_base). */
typedef struct { const char *caminho; uint32_t linha, coluna; } FontePosicao;

/* Maior .fj aceito: as posicoes (Token.off, Diag.off) sao uint32_t. */
#define FONTE_TAM_MAX (1u << 30)

/* Le o arquivo todo. CONTRATO: texto[tam] == '\0' (o lexer le um caractere alem do fim sem checar). Um BOM UTF-8 no inicio
 * e descartado. Falha (nao abriu, > FONTE_TAM_MAX, leitura curta): mensagem no stderr e devolve false (exit 2 no driver). */
bool FonteLer(Fonte *f, const char *caminho);

/* Le o stream ate o EOF (check --stdin, L6): binario (no Windows, _setmode antes), BOM UTF-8 descartado, NUL final; caminho
 * e so o nome que vai ao JSON e a pasta base dos imports (nao e aberto). Falha (ilegivel, > FONTE_TAM_MAX): mensagem no stderr e false. */
bool FonteLerDeStream(Fonte *f, FILE *arq, const char *caminho);

/* Adota uma COPIA do texto (tam bytes) como o arquivo caminho, sem ler disco (S4 P2: raiz sintetica do forja test <alvo>). false = sem memoria. */
bool FonteAdotarTexto(Fonte *f, const char *caminho, const char *texto, size_t tam);

/* Fonte ja em memoria (testes); nao copia, nao libera. */
Fonte FonteDeTexto(const char *caminho, const char *texto);

/* Monta a tabela de inicios de linha de f->texto (FonteLer ja chama). Falha de memoria: false, e FonteLocalizar
 * segue correto pela varredura. */
bool FonteIndexarLinhas(Fonte *f);

/* Linha e coluna (1-based, coluna em bytes) do offset. Offset alem do fim vale como o fim. */
void FonteLocalizar(const Fonte *f, uint32_t off, uint32_t *linha, uint32_t *coluna);

/* Anexa o texto de outro arquivo (modulo) ao fim de f->texto, que deve vir de malloc (FonteLer). Um '\n' separador entra
 * antes do texto, que comeca no inicio de uma linha. O texto novo vira o ultimo trecho; caminho nao e copiado. Total acima
 * de FONTE_TAM_MAX ou sem memoria: false e f fica intacta. */
bool FonteAnexarTexto(Fonte *f, const char *caminho, const char *texto, size_t tam);

/* Le o arquivo (como FonteLer, sem BOM) e o anexa. Arquivo ilegivel: false sem mexer em f; arquivo que nao abre nao escreve nada no stderr (o chamador diagnostica). */
bool FonteAnexarArquivo(Fonte *f, const char *caminho);

/* Arquivo, linha e coluna LOCAIS ao arquivo do trecho que contem o offset. Sem trechos: igual a FonteLocalizar. */
FontePosicao FonteLocalizarNoArquivo(const Fonte *f, uint32_t off);

/* Indice do trecho que contem o offset (0 sem trechos ou com um so); o arquivo de um offset num modulo de pasta. */
uint32_t FonteTrechoDoOffset(const Fonte *f, uint32_t off);

/* Quantidade de linhas (ultima linha sem LF conta). */
uint64_t FonteContarLinhas(const Fonte *f);

void FonteLiberar(Fonte *f);

/* Grava o arquivo inteiro. Falha: mensagem no stderr e devolve false. */
bool ArquivoGravar(const char *caminho, const char *dados, size_t tam);

/* Igual a ArquivoGravar, mas calada: quem chama decide a mensagem. errno_saida (NULL = nao quer) recebe o errno da falha. */
bool ArquivoGravarCalado(const char *caminho, const char *dados, size_t tam, int *errno_saida);

/* Grava de forma atomica (M3 P22-O1a): <caminho>.tmp<pid> + FlushFileBuffers + MoveFileEx(REPLACE_EXISTING | WRITE_THROUGH); quem le o destino ve o antigo inteiro ou o
 * novo inteiro. Destino ocupado por sharing violation (antivirus segurando o novo): ate 8 novas tentativas com espera 10, 20, 40 ... 1280 ms (2,55 s no total); access denied
 * (DLL/exe carregado) espera 1 vez (10 ms); persistindo e o destino existindo, o antigo vira <caminho>.velho<pid> e o novo ocupa o nome. Calada: errno_saida (NULL = nao quer) recebe o errno (EACCES = em uso, ENOENT = pasta inexistente, EISDIR
 * = destino e uma pasta). Falha: nada do destino muda e o temporario some. Windows. */
bool ArquivoGravarAtomico(const char *caminho, const char *dados, size_t tam, int *errno_saida);

/* Igual a ArquivoGravarAtomico (temporario + MoveFileEx REPLACE_EXISTING, mesmas esperas e .velho), mas SEM FlushFileBuffers e SEM WRITE_THROUGH: so o cache do sistema recebe.
 * Uma queda de energia pode deixar o destino com o conteudo antigo, vazio ou truncado: para dado que quem le sabe descartar e refazer (o estado do orquestrador). */
bool ArquivoGravarAtomicoSemDescarga(const char *caminho, const char *dados, size_t tam, int *errno_saida);

/* Igual a ArquivoGravarAtomico, mas falha com mensagem no stderr (como ArquivoGravar). */
bool ArquivoGravarAtomicoAviso(const char *caminho, const char *dados, size_t tam);

#endif
