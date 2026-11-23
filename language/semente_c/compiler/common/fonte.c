/* fonte.c - leitura de .fj, offset -> linha/coluna, gravacao de arquivo.
 * Versao: 0.8.0
 * Changelog: 0.8.0 - Linguagem v1.13 L1a: FonteTrechoDoOffset (busca binaria; FonteLocalizarNoArquivo a usa).
 *            0.7.1 - Portao Z b1: GravarEPublicar recusa destino so-leitura (EACCES, antes do .tmp); o .velho sai apagado quando possivel.
 *            0.7.0 - S4 P2: FonteAdotarTexto (texto em memoria, copiado, como arquivo carregado: raiz sintetica do forja test <alvo>).
 *            0.6.3 - IA1-b03: FonteAnexarArquivo (modulo importado) que nao abre nao escreve no stderr: quem chama devolve o E1001 em JSON; FonteLer segue com a mensagem.
 *            0.6.2 - M3 F8: ArquivoGravarAtomicoSemDescarga (sem _commit nem WRITE_THROUGH, para o estado do orquestrador); a versao com descarga fica igual.
 *            0.6.1- M3 P22-O1b: ACCESS_DENIED (destino carregado) espera so 1 vez (10 ms) antes de renomear o antigo; as 8 esperas ficam para SHARING_VIOLATION (antivirus no .tmp).
 *            0.6.0 - M3 P22-O1a: ArquivoGravarAtomico (temporario + FlushFileBuffers + MoveFileEx; espera crescente se ocupado; destino em uso vira <nome>.velho<pid>) e ArquivoGravarAtomicoAviso.
 *            0.5.0 - Linguagem v1.11 L6: FonteLerDeStream (--stdin); FonteLer e FonteLerDeStream dividem AdotarTexto.
 *            0.4.1 - M2 P9a: ArquivoGravarCalado (gravacao sem mensagem no stderr, para o backend nativo).
 *            0.4.0 - Linguagem v1.8 P1: FonteAnexarTexto/Arquivo, FonteLocalizarNoArquivo; FonteLer separa LerArquivo.
 *            0.3.0 - Fase 5i: FonteLer mede o arquivo em 64 bits (ftell de 32 bits falhava em >= 2 GiB com "-1 bytes"), recusa mais
 *                    de FONTE_TAM_MAX (1 GiB) com mensagem clara e tira o BOM UTF-8 do inicio.
 *            0.2.0 - Fase 5i: FonteIndexarLinhas (tabela de inicios de linha) + busca binaria em FonteLocalizar; sem tabela,
 *                    cai na varredura antiga (que serve de oraculo nos testes).
 *            0.1.0 - Fase 5a: FonteLocalizar mantem a conta do prototipo (coluna em bytes, 1-based).
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/fonte.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

/* Tamanho do arquivo em 64 bits (o long do Windows tem 32: ftell falha em arquivo >= 2 GiB). false = nao deu para medir. */
static bool TamanhoDoArquivo(FILE *arq, int64_t *tam)
{
#ifdef _WIN32
    if (_fseeki64(arq, 0, SEEK_END) != 0) return false;
    *tam = _ftelli64(arq);
    return *tam >= 0 && _fseeki64(arq, 0, SEEK_SET) == 0;
#else
    if (fseeko(arq, 0, SEEK_END) != 0) return false;
    *tam = (int64_t)ftello(arq);
    return *tam >= 0 && fseeko(arq, 0, SEEK_SET) == 0;
#endif
}

/* BOM UTF-8 (EF BB BF) no comeco: sai do texto, para linha e coluna baterem com o que o editor mostra. */
static void TirarBomUtf8(char *buf, int64_t *tam)
{
    if (*tam < 3 || (unsigned char)buf[0] != 0xEF || (unsigned char)buf[1] != 0xBB || (unsigned char)buf[2] != 0xBF) return;
    memmove(buf, buf + 3, (size_t)(*tam - 3));
    *tam -= 3;
    buf[*tam] = '\0';
}

/* Le o arquivo para um buffer de malloc (buf[tam] == '\0', sem BOM). Falha: mensagem no stderr e false.
 * calado_se_nao_abre: o arquivo que nao abre nao escreve nada (o chamador tem o proprio diagnostico JSON: o E1001 do import, IA1-b03). */
static bool LerArquivo(const char *caminho, char **saida, int64_t *saida_tam, bool calado_se_nao_abre)
{
    int64_t tam = 0;
    FILE *arq = fopen(caminho, "rb");
    if (arq == NULL) {
        if (!calado_se_nao_abre) fprintf(stderr, "forja: nao consegui abrir '%s' para leitura\n", caminho);
        return false;
    }
    if (!TamanhoDoArquivo(arq, &tam)) {
        fprintf(stderr, "forja: nao consegui medir o tamanho de '%s'\n", caminho);
        fclose(arq);
        return false;
    }
    if (tam > (int64_t)FONTE_TAM_MAX) {
        fprintf(stderr, "forja: '%s' tem %lld bytes; o limite de um .fj e %u bytes (1 GiB): as posicoes sao de 32 bits\n", caminho,
                (long long)tam, (unsigned)FONTE_TAM_MAX);
        fclose(arq);
        return false;
    }
    char *buf = malloc((size_t)tam + 1);
    if (buf == NULL || fread(buf, 1, (size_t)tam, arq) != (size_t)tam) {
        fprintf(stderr, "forja: falha lendo '%s' (%lld bytes)\n", caminho, (long long)tam);
        free(buf);
        fclose(arq);
        return false;
    }
    fclose(arq);
    buf[tam] = '\0';
    TirarBomUtf8(buf, &tam);
    *saida = buf;
    *saida_tam = tam;
    return true;
}

/* Poe o texto lido (buffer de malloc com tam bytes) na fonte e monta a tabela de linhas. */
static void AdotarTexto(Fonte *f, const char *caminho, char *buf, int64_t tam)
{
    f->caminho = caminho;
    f->texto = buf;
    f->tam = (size_t)tam;
    f->inicio_linha = NULL;
    f->ninicios = 0;
    f->trechos = NULL;
    f->ntrechos = 0;
    FonteIndexarLinhas(f);
}

bool FonteLer(Fonte *f, const char *caminho)
{
    char *buf = NULL;
    int64_t tam = 0;
    if (!LerArquivo(caminho, &buf, &tam, false)) return false;
    AdotarTexto(f, caminho, buf, tam);
    return true;
}

/* Blocos de leitura do stream: o buffer comeca com 64 KiB e dobra quando lota. */
#define FONTE_BLOCO_STREAM 65536u

/* Dobra a capacidade do buffer de um stream. false = ja passou de FONTE_TAM_MAX ou sem memoria (buf segue valido). */
static bool CrescerBufferDoStream(char **buf, size_t *cap)
{
    size_t novo = *cap * 2;
    char *maior;

    if (*cap >= (size_t)FONTE_TAM_MAX) return false;
    maior = realloc(*buf, novo + 1);
    if (maior == NULL) return false;
    *buf = maior;
    *cap = novo;
    return true;
}

/* Enche o buffer com o stream ate o EOF. false = erro de leitura ou nao coube (passou de FONTE_TAM_MAX / sem memoria). */
static bool LerBlocosAteEof(FILE *arq, char **buf, size_t *tam, size_t *cap)
{
    for (;;) {
        if (*tam == *cap && !CrescerBufferDoStream(buf, cap)) return false;
        size_t lidos = fread(*buf + *tam, 1, *cap - *tam, arq);
        *tam += lidos;
        if (lidos == 0) return ferror(arq) == 0;
    }
}

/* Le o stream inteiro para um buffer de malloc (buf[tam] == '\0', sem BOM). false = falha, sem mensagem. */
static bool LerStreamAteEof(FILE *arq, char **saida, int64_t *saida_tam)
{
    size_t tam = 0, cap = FONTE_BLOCO_STREAM;
    int64_t tam_texto;
    char *buf = malloc(cap + 1);

    if (buf == NULL) return false;
    if (!LerBlocosAteEof(arq, &buf, &tam, &cap)) {
        free(buf);
        return false;
    }
    buf[tam] = '\0';
    tam_texto = (int64_t)tam;
    TirarBomUtf8(buf, &tam_texto);
    *saida = buf;
    *saida_tam = tam_texto;
    return true;
}

bool FonteLerDeStream(Fonte *f, FILE *arq, const char *caminho)
{
    char *buf = NULL;
    int64_t tam = 0;

#ifdef _WIN32
    if (_setmode(_fileno(arq), _O_BINARY) == -1) {
        fprintf(stderr, "forja: nao consegui deixar a entrada de '%s' em modo binario\n", caminho);
        return false;
    }
#endif
    if (!LerStreamAteEof(arq, &buf, &tam)) {
        fprintf(stderr, "forja: nao consegui ler o .fj da entrada padrao (--stdin) para '%s'\n", caminho);
        return false;
    }
    AdotarTexto(f, caminho, buf, tam);
    return true;
}

bool FonteAdotarTexto(Fonte *f, const char *caminho, const char *texto, size_t tam)
{
    char *buf = malloc(tam + 1);

    if (buf == NULL) return false;
    memcpy(buf, texto, tam);
    buf[tam] = '\0';
    AdotarTexto(f, caminho, buf, (int64_t)tam);
    return true;
}

Fonte FonteDeTexto(const char *caminho, const char *texto)
{
    Fonte f = {caminho, (char *)texto, strlen(texto), NULL, 0, NULL, 0};
    return f;
}

bool FonteIndexarLinhas(Fonte *f)
{
    uint32_t lfs = 0;
    const char *fim = f->texto + f->tam;

    for (const char *p = memchr(f->texto, '\n', f->tam); p != NULL; p = memchr(p + 1, '\n', (size_t)(fim - (p + 1)))) lfs++;
    uint32_t *tabela = malloc(((size_t)lfs + 1) * sizeof *tabela);
    if (tabela == NULL) return false;
    uint32_t k = 0;
    tabela[k++] = 0;
    for (const char *p = memchr(f->texto, '\n', f->tam); p != NULL; p = memchr(p + 1, '\n', (size_t)(fim - (p + 1))))
        tabela[k++] = (uint32_t)(p - f->texto) + 1;
    free(f->inicio_linha);
    f->inicio_linha = tabela;
    f->ninicios = k;
    return true;
}

/* Ultimo indice lo com inicio_linha[lo] <= chave (inicio_linha[0] = 0, entao sempre existe). */
static uint32_t LinhaDoOffset(const Fonte *f, uint32_t chave)
{
    uint32_t lo = 0, hi = f->ninicios - 1;

    while (lo < hi) {
        uint32_t meio = lo + (hi - lo + 1) / 2;
        if (f->inicio_linha[meio] <= chave) lo = meio;
        else hi = meio - 1;
    }
    return lo;
}

void FonteLocalizar(const Fonte *f, uint32_t off, uint32_t *linha, uint32_t *coluna)
{
    if (f->inicio_linha != NULL) {
        uint32_t chave = off < f->tam ? off : (uint32_t)f->tam;
        uint32_t lo = LinhaDoOffset(f, chave);
        *linha = lo + 1;
        *coluna = off - f->inicio_linha[lo] + 1;
        return;
    }
    uint32_t l = 1, ultimo_lf = 0;
    bool viu_lf = false;
    for (uint32_t i = 0; i < off && i < f->tam; i++) {
        if (f->texto[i] == '\n') {
            l++;
            ultimo_lf = i;
            viu_lf = true;
        }
    }
    *linha = l;
    *coluna = viu_lf ? off - ultimo_lf : off + 1;
}

static uint32_t ContarLfs(const char *texto, size_t tam)
{
    uint32_t lfs = 0;
    for (size_t i = 0; i < tam; i++) lfs += texto[i] == '\n';
    return lfs;
}

/* Aumenta texto, tabela de linhas e trechos para caber o anexo. Cada realloc que da certo ja vale em f (tam, ninicios e
 * ntrechos nao mudam), entao uma falha no meio deixa f com o mesmo conteudo. */
static bool ReservarAnexo(Fonte *f, size_t tam_novo, uint32_t lfs_novos)
{
    uint32_t cap_trechos = f->ntrechos == 0 ? 2 : f->ntrechos + 1;
    uint32_t *tabela = realloc(f->inicio_linha, ((size_t)f->ninicios + lfs_novos + 1) * sizeof *tabela);
    if (tabela == NULL) return false;
    f->inicio_linha = tabela;
    FonteTrecho *trechos = realloc(f->trechos, cap_trechos * sizeof *trechos);
    if (trechos == NULL) return false;
    f->trechos = trechos;
    char *texto = realloc(f->texto, f->tam + 1 + tam_novo + 1);
    if (texto == NULL) return false;
    f->texto = texto;
    return true;
}

bool FonteAnexarTexto(Fonte *f, const char *caminho, const char *texto, size_t tam)
{
    size_t tam_velho = f->tam;
    if (tam_velho + 1 + tam > FONTE_TAM_MAX) return false;
    if (f->inicio_linha == NULL && !FonteIndexarLinhas(f)) return false;
    if (!ReservarAnexo(f, tam, ContarLfs(texto, tam) + 1)) return false;
    if (f->ntrechos == 0) {
        f->trechos[0] = (FonteTrecho){0, (uint32_t)tam_velho, 0, f->caminho};
        f->ntrechos = 1;
    }
    f->texto[tam_velho] = '\n';
    memcpy(f->texto + tam_velho + 1, texto, tam);
    f->texto[tam_velho + 1 + tam] = '\0';
    f->tam = tam_velho + 1 + tam;
    for (size_t i = tam_velho; i < f->tam; i++) {
        if (f->texto[i] == '\n') f->inicio_linha[f->ninicios++] = (uint32_t)i + 1;
    }
    uint32_t inicio = (uint32_t)tam_velho + 1;
    uint32_t linha_base = LinhaDoOffset(f, inicio);
    f->trechos[f->ntrechos++] = (FonteTrecho){inicio, inicio + (uint32_t)tam, linha_base, caminho};
    return true;
}

bool FonteAnexarArquivo(Fonte *f, const char *caminho)
{
    char *buf = NULL;
    int64_t tam = 0;
    if (!LerArquivo(caminho, &buf, &tam, true)) return false;
    bool ok = FonteAnexarTexto(f, caminho, buf, (size_t)tam);
    free(buf);
    return ok;
}

uint32_t FonteTrechoDoOffset(const Fonte *f, uint32_t off)
{
    if (f->ntrechos <= 1) return 0;
    uint32_t lo = 0, hi = f->ntrechos; /* o ultimo trecho cujo inicio <= off (inicio cresce com o indice; trecho 0 comeca em 0) */
    while (hi - lo > 1) {
        uint32_t meio = lo + (hi - lo) / 2;
        if (f->trechos[meio].inicio <= off) lo = meio; else hi = meio;
    }
    return lo;
}

FontePosicao FonteLocalizarNoArquivo(const Fonte *f, uint32_t off)
{
    FontePosicao pos = {f->caminho, 0, 0};
    FonteLocalizar(f, off, &pos.linha, &pos.coluna);
    if (f->ntrechos <= 1) return pos;
    const FonteTrecho *achado = &f->trechos[FonteTrechoDoOffset(f, off)];
    pos.caminho = achado->caminho;
    pos.linha -= achado->linha_base;
    return pos;
}

uint64_t FonteContarLinhas(const Fonte *f)
{
    uint64_t linhas = 0;
    for (size_t i = 0; i < f->tam; i++) {
        if (f->texto[i] == '\n') linhas++;
    }
    if (f->tam > 0 && f->texto[f->tam - 1] != '\n') linhas++;
    return linhas;
}

void FonteLiberar(Fonte *f)
{
    free(f->texto);
    free(f->inicio_linha);
    free(f->trechos);
    f->texto = NULL;
    f->inicio_linha = NULL;
    f->trechos = NULL;
    f->ntrechos = 0;
    f->ninicios = 0;
    f->tam = 0;
}

bool ArquivoGravar(const char *caminho, const char *dados, size_t tam)
{
    FILE *arq = fopen(caminho, "wb");
    if (arq == NULL) {
        fprintf(stderr, "forja: nao consegui abrir '%s' para gravacao\n", caminho);
        return false;
    }
    bool ok = fwrite(dados, 1, tam, arq) == tam;
    ok = (fclose(arq) == 0) && ok;
    if (!ok) fprintf(stderr, "forja: falha gravando %zu bytes em '%s'\n", tam, caminho);
    return ok;
}

bool ArquivoGravarCalado(const char *caminho, const char *dados, size_t tam, int *errno_saida)
{
    FILE *arq = fopen(caminho, "wb");
    bool ok;

    if (arq == NULL) {
        if (errno_saida != NULL) *errno_saida = errno;
        return false;
    }
    ok = fwrite(dados, 1, tam, arq) == tam;
    ok = (fclose(arq) == 0) && ok;
    if (!ok && errno_saida != NULL) *errno_saida = errno;
    return ok;
}

/* ---------- gravacao atomica (Windows: o backend x64, o unico que a usa, so existe nele) ---------- */

#define ATOMICO_TENTATIVAS 8            /* novas tentativas depois da 1a, com a espera dobrando: 10+20+...+1280 = 2550 ms no total */
#define ATOMICO_TENTATIVAS_NEGADO 1     /* ACCESS_DENIED = DLL/exe carregado: 1 espera curta e segue para renomear o antigo */
#define ATOMICO_ESPERA_INICIAL_MS 10
#define ATOMICO_TAM_CAMINHO 1100

/* Os dois nomes auxiliares do destino, com o pid do processo (dois forja gravando no mesmo lugar nao se pisam). */
typedef struct { char temporario[ATOMICO_TAM_CAMINHO]; char velho[ATOMICO_TAM_CAMINHO]; } NomesAtomicos;

static bool MontarNomesAtomicos(const char *caminho, NomesAtomicos *n)
{
    unsigned long pid = (unsigned long)GetCurrentProcessId();
    int a = snprintf(n->temporario, sizeof n->temporario, "%s.tmp%lu", caminho, pid);
    int b = snprintf(n->velho, sizeof n->velho, "%s.velho%lu", caminho, pid);

    return a > 0 && (size_t)a < sizeof n->temporario && b > 0 && (size_t)b < sizeof n->velho;
}

static bool EhPasta(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);

    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/* Destino so-leitura: a gravacao atomica nao o sobrescreve (a troca por renomear passaria por cima da protecao). */
static bool EhSomenteLeitura(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);

    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_READONLY) != 0;
}

/* Erros que o antivirus (segurando o arquivo novo) ou um destino carregado por outro processo produzem. */
static bool ErroDeOcupado(DWORD erro)
{
    return erro == ERROR_SHARING_VIOLATION || erro == ERROR_ACCESS_DENIED;
}

static int ErrnoDoWindows(DWORD erro)
{
    if (erro == ERROR_FILE_NOT_FOUND || erro == ERROR_PATH_NOT_FOUND) return ENOENT;
    return ErroDeOcupado(erro) ? EACCES : EIO;
}

/* O que gravar e se o disco e forcado (FlushFileBuffers + WRITE_THROUGH) ou so o cache do sistema recebe. */
typedef struct { const char *dados; size_t tam; bool descarregar; } Conteudo;

/* Grava o temporario e, com descarregar, o forca para o disco (FlushFileBuffers) antes do rename: a queda no meio nunca deixa o destino pela metade. */
static bool GravarTemporario(const char *tmp, const Conteudo *c, int *err)
{
    FILE *arq = fopen(tmp, "wb");
    bool ok;

    if (arq == NULL) {
        *err = errno;
        return false;
    }
    ok = fwrite(c->dados, 1, c->tam, arq) == c->tam && fflush(arq) == 0 && (!c->descarregar || _commit(_fileno(arq)) == 0);
    if (!ok) *err = errno;
    if (fclose(arq) != 0 && ok) {
        *err = errno;
        ok = false;
    }
    return ok;
}

/* Sharing violation (antivirus no arquivo novo) espera ate 8 vezes; access denied (destino carregado) so 1, porque esperar nao o descarrega. */
static bool DeveEsperar(DWORD erro, int tentativa)
{
    if (erro == ERROR_SHARING_VIOLATION) return tentativa < ATOMICO_TENTATIVAS;
    return erro == ERROR_ACCESS_DENIED && tentativa < ATOMICO_TENTATIVAS_NEGADO;
}

/* Move o temporario sobre o destino; ocupado = espera crescente e tenta de novo. 0 = ok, senao o erro do Windows. */
static DWORD TrocarComEspera(const char *tmp, const char *destino, DWORD descarga)
{
    DWORD espera = ATOMICO_ESPERA_INICIAL_MS;
    DWORD erro = 0;

    for (int tentativa = 0; tentativa <= ATOMICO_TENTATIVAS; tentativa++) {
        if (MoveFileExA(tmp, destino, MOVEFILE_REPLACE_EXISTING | descarga)) return 0;
        erro = GetLastError();
        if (!DeveEsperar(erro, tentativa)) break;
        Sleep(espera);
        espera *= 2;
    }
    return erro;
}

/* Destino em uso (DLL ou exe carregado): o antigo vai para <destino>.velho<pid> (quem o usa segue com o codigo velho) e o novo ocupa o nome. */
static DWORD SubstituirOcupado(const NomesAtomicos *n, const char *destino, DWORD descarga)
{
    DWORD erro;

    if (!MoveFileExA(destino, n->velho, MOVEFILE_REPLACE_EXISTING)) return GetLastError();
    if (MoveFileExA(n->temporario, destino, descarga)) return 0;
    erro = GetLastError();
    MoveFileExA(n->velho, destino, 0); /* desfaz: o destino volta a existir */
    return erro;
}

static DWORD Publicar(const NomesAtomicos *n, const char *destino, DWORD descarga)
{
    DWORD erro = TrocarComEspera(n->temporario, destino, descarga);

    if (erro == 0 || !ErroDeOcupado(erro) || GetFileAttributesA(destino) == INVALID_FILE_ATTRIBUTES) return erro;
    erro = SubstituirOcupado(n, destino, descarga);
    if (erro == 0) DeleteFileA(n->velho); /* falha ignorada: o antigo em uso nao se apaga e fica como .velho */
    return erro;
}

/* errno da falha, ou 0 se gravou. */
static int GravarEPublicar(const char *caminho, const Conteudo *c)
{
    NomesAtomicos n;
    int err = 0;
    DWORD erro;

    if (!MontarNomesAtomicos(caminho, &n)) return ENAMETOOLONG;
    if (EhPasta(caminho)) return EISDIR;
    if (EhSomenteLeitura(caminho)) return EACCES;
    if (!GravarTemporario(n.temporario, c, &err)) {
        remove(n.temporario);
        return err != 0 ? err : EIO;
    }
    erro = Publicar(&n, caminho, c->descarregar ? MOVEFILE_WRITE_THROUGH : 0);
    if (erro == 0) return 0;
    remove(n.temporario);
    return ErrnoDoWindows(erro);
}

static bool GravarAtomicoCom(const char *caminho, const Conteudo *c, int *errno_saida)
{
    int err = GravarEPublicar(caminho, c);

    if (err != 0 && errno_saida != NULL) *errno_saida = err;
    return err == 0;
}

bool ArquivoGravarAtomico(const char *caminho, const char *dados, size_t tam, int *errno_saida)
{
    Conteudo c = {dados, tam, true};

    return GravarAtomicoCom(caminho, &c, errno_saida);
}

bool ArquivoGravarAtomicoSemDescarga(const char *caminho, const char *dados, size_t tam, int *errno_saida)
{
    Conteudo c = {dados, tam, false};

    return GravarAtomicoCom(caminho, &c, errno_saida);
}

bool ArquivoGravarAtomicoAviso(const char *caminho, const char *dados, size_t tam)
{
    int err = 0;

    if (ArquivoGravarAtomico(caminho, dados, tam, &err)) return true;
    fprintf(stderr, "forja: nao consegui gravar '%s': %s\n", caminho, strerror(err));
    return false;
}
