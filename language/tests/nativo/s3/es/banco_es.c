/*
 * banco_es.c - prova dos headers forja_rt_es.h e forja_rt_stdin.h no backend C (S3 P5a): disco numa pasta nova e entrada padrao por cano.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - S3 P5a: modo disco (0 B, 1 MiB e 64 MiB+1, seek, append, create, modos, fechado, stat, rename e copy, remover, pastas e read_dir)
 *           e modos de entrada (linhas, bruto, prompt). Uma linha CASO por caso e RESUMO no fim do disco.
 * Compilar (verificar_es.py faz isso): gcc -std=c11 -O2 -Wall -Wextra -Werror -I<runtime> banco_es.c -o banco_es.exe
 * Uso: banco_es.exe disco <pasta nova> | banco_es.exe linhas | banco_es.exe bruto | banco_es.exe prompt
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "forja_rt_es.h"
#include "forja_rt_stdin.h"

#define BANCO_TAM_LOTE (1024 * 1024)
#define BANCO_TAM_GRANDE ((int64_t)64 * 1024 * 1024 + 1)
#define BANCO_CAMINHO 1024
#define BANCO_NOME 1024
#define BANCO_LISTA_MAX 16
#define BANCO_PARTE 65536
#define BANCO_ACUMULO (4 * 1024 * 1024)
#define BANCO_HEX 512

typedef struct {
    char nomes[BANCO_LISTA_MAX][BANCO_NOME];
    int32_t tipos[BANCO_LISTA_MAX];
    int32_t quantos;
} BancoLista;

static int32_t casos_total;
static int32_t casos_falhos;
static uint8_t lote[BANCO_TAM_LOTE];
static uint8_t parte[BANCO_PARTE];
static uint8_t acumulo[BANCO_ACUMULO];

static void caso_conferir(const char *nome, int ok)
{
    casos_total++;
    if (!ok)
        casos_falhos++;
    printf("CASO %s %s\n", nome, ok ? "OK" : "FALHA");
}

/* Caminho dentro da pasta-base: base\nome (o separador e o do Windows). */
static void caminho_de(char *destino, const char *base, const char *nome)
{
    snprintf(destino, BANCO_CAMINHO, "%s\\%s", base, nome);
}

/* Byte do padrao na posicao i: a conta depende so de i, assim o conteudo se confere sem guardar tudo. */
static uint8_t byte_padrao(uint64_t i)
{
    return (uint8_t)((i * 0x9E3779B97F4A7C15ULL) >> 56);
}

/* Grava total bytes do padrao em h, em lotes de 1 MiB. */
static int gravar_padrao(uint64_t h, int64_t total)
{
    int64_t feito = 0;
    int32_t erro = 0;

    while (feito < total) {
        int64_t n = total - feito < BANCO_TAM_LOTE ? total - feito : BANCO_TAM_LOTE;
        int64_t i;

        for (i = 0; i < n; i++)
            lote[i] = byte_padrao((uint64_t)(feito + i));
        if (forja_fs_gravar(h, lote, n, &erro) != n)
            return 0;
        feito += n;
    }
    return 1;
}

/* Confere que h tem exatamente total bytes do padrao (e termina ali). */
static int conferir_padrao(uint64_t h, int64_t total)
{
    int64_t conferido = 0;
    int32_t erro = 0;

    while (conferido < total) {
        int64_t quero = total - conferido < BANCO_TAM_LOTE ? total - conferido : BANCO_TAM_LOTE;
        int64_t r = forja_fs_ler(h, lote, quero, &erro);
        int64_t i;

        if (r <= 0)
            return 0;
        for (i = 0; i < r; i++) {
            if (lote[i] != byte_padrao((uint64_t)(conferido + i)))
                return 0;
        }
        conferido += r;
    }
    return forja_fs_ler(h, lote, 1, &erro) == 0;
}

static int gravar_arquivo_padrao(const char *caminho, int64_t total)
{
    int32_t erro = 0;
    uint64_t h = forja_fs_abrir(caminho, FORJA_FS_MODO_GRAVAR, &erro);
    int ok;

    if (h == 0)
        return 0;
    ok = gravar_padrao(h, total);
    forja_fs_fechar(h);
    return ok;
}

static int conferir_arquivo_padrao(const char *caminho, int64_t total)
{
    int32_t erro = 0;
    uint64_t h = forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro);
    int ok;

    if (h == 0)
        return 0;
    ok = conferir_padrao(h, total);
    forja_fs_fechar(h);
    return ok;
}

static int gravar_texto(const char *caminho, const char *texto)
{
    int32_t erro = 0;
    uint64_t h = forja_fs_abrir(caminho, FORJA_FS_MODO_GRAVAR, &erro);
    int64_t n = (int64_t)strlen(texto);
    int ok;

    if (h == 0)
        return 0;
    ok = forja_fs_gravar(h, (const uint8_t *)texto, n, &erro) == n;
    forja_fs_fechar(h);
    return ok;
}

/* Le o arquivo inteiro para lote (ate 1 MiB): quantos bytes, ou -1 se falhou. */
static int64_t ler_tudo(const char *caminho)
{
    int32_t erro = 0;
    uint64_t h = forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro);
    int64_t total = 0;
    int64_t r = 0;

    if (h == 0)
        return -1;
    while (total < BANCO_TAM_LOTE) {
        r = forja_fs_ler(h, lote + total, BANCO_TAM_LOTE - total, &erro);
        if (r <= 0)
            break;
        total += r;
    }
    forja_fs_fechar(h);
    return r < 0 ? -1 : total;
}

static int conferir_texto(const char *caminho, const char *texto)
{
    int64_t n = ler_tudo(caminho);

    return n == (int64_t)strlen(texto) && memcmp(lote, texto, (size_t)n) == 0;
}

/* Tamanho do caminho, ou -1 se o stat falha. */
static int64_t tamanho_de(const char *caminho)
{
    int64_t tamanho = -1;
    int64_t mtime = 0;
    int32_t tipo = -1;

    if (forja_fs_info(caminho, &tamanho, &mtime, &tipo) != 0)
        return -1;
    return tamanho;
}

/* Tipo do caminho (FileKind), ou -1 se o stat falha. */
static int32_t tipo_de(const char *caminho)
{
    int64_t tamanho = 0;
    int64_t mtime = 0;
    int32_t tipo = -1;

    if (forja_fs_info(caminho, &tamanho, &mtime, &tipo) != 0)
        return -1;
    return tipo;
}

static void disco_arquivos_basicos(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int32_t erro = 0;
    uint64_t h;

    caminho_de(caminho, base, "zero.bin");
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_GRAVAR, &erro);
    caso_conferir("abrir para gravar cria o arquivo", h != 0);
    caso_conferir("gravar 0 B devolve 0", forja_fs_gravar(h, lote, 0, &erro) == 0);
    forja_fs_fechar(h);
    caso_conferir("0 B: stat tamanho 0", tamanho_de(caminho) == 0);
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro);
    caso_conferir("0 B: ler devolve fim (0)", forja_fs_ler(h, lote, 16, &erro) == 0);
    forja_fs_fechar(h);

    caminho_de(caminho, base, "um.bin");
    caso_conferir("gravar 1 MiB", gravar_arquivo_padrao(caminho, BANCO_TAM_LOTE));
    caso_conferir("1 MiB: stat tamanho 1 MiB", tamanho_de(caminho) == BANCO_TAM_LOTE);
    caso_conferir("1 MiB: conteudo confere", conferir_arquivo_padrao(caminho, BANCO_TAM_LOTE));

    caminho_de(caminho, base, "grande.bin");
    caso_conferir("gravar 64 MiB+1", gravar_arquivo_padrao(caminho, BANCO_TAM_GRANDE));
    caso_conferir("64 MiB+1: stat tamanho 64 MiB+1", tamanho_de(caminho) == BANCO_TAM_GRANDE);
    caso_conferir("64 MiB+1: conteudo confere", conferir_arquivo_padrao(caminho, BANCO_TAM_GRANDE));
}

static void disco_seek(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int32_t erro = 0;
    uint64_t h;
    uint8_t b[4];

    caminho_de(caminho, base, "seek.txt");
    gravar_texto(caminho, "0123456789");
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro);
    caso_conferir("seek inicio 3 devolve 3", forja_fs_posicionar(h, 3, FORJA_FS_ORIGEM_INICIO, &erro) == 3);
    caso_conferir("seek: ler 2 bytes = '34'", forja_fs_ler(h, b, 2, &erro) == 2 && b[0] == '3' && b[1] == '4');
    caso_conferir("tell (atual +0) = 5", forja_fs_posicionar(h, 0, FORJA_FS_ORIGEM_ATUAL, &erro) == 5);
    caso_conferir("seek atual -2 devolve 3", forja_fs_posicionar(h, -2, FORJA_FS_ORIGEM_ATUAL, &erro) == 3);
    caso_conferir("seek fim -1 devolve 9", forja_fs_posicionar(h, -1, FORJA_FS_ORIGEM_FIM, &erro) == 9);
    caso_conferir("seek: ler 1 byte no 9 = '9'", forja_fs_ler(h, b, 1, &erro) == 1 && b[0] == '9');
    caso_conferir("tell no fim = 10", forja_fs_posicionar(h, 0, FORJA_FS_ORIGEM_FIM, &erro) == 10);
    caso_conferir("seek negativo no inicio = outro", forja_fs_posicionar(h, -1, FORJA_FS_ORIGEM_INICIO, &erro) == -1 && erro == FORJA_FS_ERR_OUTRO);
    forja_fs_fechar(h);
}

static void disco_append(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int32_t erro = 0;
    uint64_t h;
    uint8_t b[2] = {'a', 'b'};

    caminho_de(caminho, base, "anexo.txt");
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_ANEXAR, &erro);
    caso_conferir("append 1: cria se nao existe", h != 0 && forja_fs_gravar(h, b, 2, &erro) == 2);
    forja_fs_fechar(h);
    b[0] = 'c';
    b[1] = 'd';
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_ANEXAR, &erro);
    caso_conferir("append 2: grava no fim", h != 0 && forja_fs_gravar(h, b, 2, &erro) == 2);
    forja_fs_fechar(h);
    caso_conferir("append 2x: conteudo = 'abcd'", conferir_texto(caminho, "abcd"));
}

static void disco_modos(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int32_t erro = 0;
    uint64_t h;
    uint8_t b[1] = {'x'};

    caminho_de(caminho, base, "existe.bin");
    gravar_texto(caminho, "x");
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_CRIAR, &erro);
    caso_conferir("create sobre existente = ja existe", h == 0 && erro == FORJA_FS_ERR_JA_EXISTE);
    caminho_de(caminho, base, "novo.bin");
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_CRIAR, &erro);
    caso_conferir("create em caminho novo cria", h != 0);
    forja_fs_fechar(h);
    caminho_de(caminho, base, "existe.bin");
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro);
    caso_conferir("escrever em handle de leitura = sem permissao", forja_fs_gravar(h, b, 1, &erro) == -1 && erro == FORJA_FS_ERR_SEM_PERMISSAO);
    forja_fs_fechar(h);
    caso_conferir("abrir pasta = e pasta", forja_fs_abrir(base, FORJA_FS_MODO_LER, &erro) == 0 && erro == FORJA_FS_ERR_DIRETORIO);
    caso_conferir("caminho vazio = nao existe", forja_fs_abrir("", FORJA_FS_MODO_LER, &erro) == 0 && erro == FORJA_FS_ERR_NAO_EXISTE);
    caminho_de(caminho, base, "sumiu.bin");
    caso_conferir("abrir para ler inexistente = nao existe", forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro) == 0 && erro == FORJA_FS_ERR_NAO_EXISTE);
}

static void disco_fechado(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int32_t erro = 0;
    uint64_t velho;
    uint64_t novo;
    uint8_t b[1] = {'z'};

    caminho_de(caminho, base, "fechado.txt");
    gravar_texto(caminho, "abc");
    velho = forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro);
    forja_fs_fechar(velho);
    caso_conferir("ler apos fechar = fechado", forja_fs_ler(velho, b, 1, &erro) == -1 && erro == FORJA_FS_ERR_FECHADO);
    caso_conferir("gravar apos fechar = fechado", forja_fs_gravar(velho, b, 1, &erro) == -1 && erro == FORJA_FS_ERR_FECHADO);
    caso_conferir("posicionar apos fechar = fechado", forja_fs_posicionar(velho, 0, FORJA_FS_ORIGEM_INICIO, &erro) == -1 && erro == FORJA_FS_ERR_FECHADO);
    caso_conferir("descarregar apos fechar = fechado", forja_fs_descarregar(velho, &erro) == -1 && erro == FORJA_FS_ERR_FECHADO);
    forja_fs_fechar(velho);
    novo = forja_fs_abrir(caminho, FORJA_FS_MODO_LER, &erro);
    caso_conferir("handle antigo nao vale na posicao reusada", novo != velho && forja_fs_ler(velho, b, 1, &erro) == -1 && erro == FORJA_FS_ERR_FECHADO);
    caso_conferir("handle novo continua valido", forja_fs_ler(novo, b, 1, &erro) == 1 && b[0] == 'a');
    forja_fs_fechar(novo);
}

static void disco_descarregar(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int32_t erro = 0;
    uint64_t h;
    uint8_t b[3] = {'f', 'l', 's'};

    caminho_de(caminho, base, "flush.bin");
    h = forja_fs_abrir(caminho, FORJA_FS_MODO_GRAVAR, &erro);
    caso_conferir("gravar 3 e descarregar = 0", forja_fs_gravar(h, b, 3, &erro) == 3 && forja_fs_descarregar(h, &erro) == 0);
    forja_fs_fechar(h);
    caso_conferir("flush: arquivo tem 3 bytes", tamanho_de(caminho) == 3);
}

static void disco_info(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int64_t tamanho = -1;
    int64_t mtime = 0;
    int32_t tipo = -1;

    caminho_de(caminho, base, "info.txt");
    gravar_texto(caminho, "12345");
    caso_conferir("stat arquivo: tamanho 5 e tipo arquivo", forja_fs_info(caminho, &tamanho, &mtime, &tipo) == 0 && tamanho == 5 && tipo == FORJA_FS_TIPO_ARQUIVO);
    caso_conferir("stat: mtime entre 2000 e 2100 (ns)", mtime >= 946684800000000000LL && mtime <= 4102444800000000000LL);
    caminho_de(caminho, base, "info_pasta");
    caso_conferir("criar pasta para o stat", forja_fs_criar_pasta(caminho) == 0);
    caso_conferir("stat pasta: tipo pasta", forja_fs_info(caminho, &tamanho, &mtime, &tipo) == 0 && tipo == FORJA_FS_TIPO_PASTA);
    caminho_de(caminho, base, "nao_existe.txt");
    caso_conferir("stat inexistente = nao existe", forja_fs_info(caminho, &tamanho, &mtime, &tipo) == FORJA_FS_ERR_NAO_EXISTE);
    caso_conferir("stat caminho vazio = nao existe", forja_fs_info("", &tamanho, &mtime, &tipo) == FORJA_FS_ERR_NAO_EXISTE);
}

static void disco_renomear_copiar(const char *base)
{
    char a[BANCO_CAMINHO];
    char b[BANCO_CAMINHO];
    char c[BANCO_CAMINHO];
    int64_t tamanho = 0;
    int64_t mtime = 0;
    int32_t tipo = 0;

    caminho_de(a, base, "a.txt");
    caminho_de(b, base, "b.txt");
    caminho_de(c, base, "c.txt");
    gravar_texto(a, "AAA");
    gravar_texto(b, "BBB");
    caso_conferir("renomear sobrescreve o destino", forja_fs_renomear(a, b) == 0 && forja_fs_info(a, &tamanho, &mtime, &tipo) == FORJA_FS_ERR_NAO_EXISTE && conferir_texto(b, "AAA"));
    caso_conferir("copiar para destino novo", forja_fs_copiar(b, c) == 0 && conferir_texto(c, "AAA"));
    gravar_texto(c, "CCC");
    caso_conferir("copiar sobrescreve o destino", forja_fs_copiar(b, c) == 0 && conferir_texto(c, "AAA"));
    caminho_de(a, base, "sumido.txt");
    caso_conferir("renomear origem inexistente = nao existe", forja_fs_renomear(a, c) == FORJA_FS_ERR_NAO_EXISTE);
    caso_conferir("copiar origem inexistente = nao existe", forja_fs_copiar(a, b) == FORJA_FS_ERR_NAO_EXISTE);
}

static void disco_remover(const char *base)
{
    char caminho[BANCO_CAMINHO];
    int64_t tamanho = 0;
    int64_t mtime = 0;
    int32_t tipo = 0;

    caminho_de(caminho, base, "rem_pasta");
    forja_fs_criar_pasta(caminho);
    caso_conferir("remover de pasta = e pasta", forja_fs_remover(caminho) == FORJA_FS_ERR_DIRETORIO);
    caminho_de(caminho, base, "nao_existe.txt");
    caso_conferir("remover inexistente = nao existe", forja_fs_remover(caminho) == FORJA_FS_ERR_NAO_EXISTE);
    caminho_de(caminho, base, "rem.txt");
    gravar_texto(caminho, "x");
    caso_conferir("remover arquivo = 0 e some", forja_fs_remover(caminho) == 0 && forja_fs_info(caminho, &tamanho, &mtime, &tipo) == FORJA_FS_ERR_NAO_EXISTE);
}

static void disco_pastas(const char *base)
{
    char caminho[BANCO_CAMINHO];
    char arquivo[BANCO_CAMINHO];

    caminho_de(caminho, base, "p1");
    caso_conferir("criar_pasta cria", forja_fs_criar_pasta(caminho) == 0);
    caso_conferir("criar_pasta existente = ja existe", forja_fs_criar_pasta(caminho) == FORJA_FS_ERR_JA_EXISTE);
    caminho_de(caminho, base, "pai_inexistente\\sub");
    caso_conferir("criar_pasta com pai inexistente = nao existe", forja_fs_criar_pasta(caminho) == FORJA_FS_ERR_NAO_EXISTE);
    caminho_de(caminho, base, "cheia");
    forja_fs_criar_pasta(caminho);
    caminho_de(arquivo, base, "cheia\\x.txt");
    gravar_texto(arquivo, "x");
    caso_conferir("remover_pasta de pasta cheia = nao vazio", forja_fs_remover_pasta(caminho) == FORJA_FS_ERR_NAO_VAZIO);
    caso_conferir("remover_pasta de arquivo = nao e pasta", forja_fs_remover_pasta(arquivo) == FORJA_FS_ERR_NAO_DIRETORIO);
    caminho_de(caminho, base, "p1");
    caso_conferir("remover_pasta vazia = 0", forja_fs_remover_pasta(caminho) == 0 && tipo_de(caminho) == -1);
    caminho_de(caminho, base, "nao_existe");
    caso_conferir("remover_pasta inexistente = nao existe", forja_fs_remover_pasta(caminho) == FORJA_FS_ERR_NAO_EXISTE);
}

/* Lista a pasta: nomes e tipos em l. 1 = chegou ao fim da busca; 0 = falhou ou o limite de itens estourou. */
static int listar_pasta(const char *caminho, BancoLista *l)
{
    int32_t erro = 0;
    uint64_t h = forja_fs_pasta_abrir(caminho, &erro);
    int64_t r = 0;

    l->quantos = 0;
    if (h == 0)
        return 0;
    while (l->quantos < BANCO_LISTA_MAX) {
        r = forja_fs_pasta_proximo(h, (uint8_t *)l->nomes[l->quantos], BANCO_NOME, &l->tipos[l->quantos]);
        if (r <= 0)
            break;
        l->nomes[l->quantos][r] = '\0';
        l->quantos++;
    }
    forja_fs_pasta_fechar(h);
    return r == 0;
}

static int contem_item(const BancoLista *l, const char *nome, int32_t tipo)
{
    int32_t i;

    for (i = 0; i < l->quantos; i++) {
        if (strcmp(l->nomes[i], nome) == 0 && l->tipos[i] == tipo)
            return 1;
    }
    return 0;
}

static int tem_ponto_ou_ponto_ponto(const BancoLista *l)
{
    int32_t i;

    for (i = 0; i < l->quantos; i++) {
        if (strcmp(l->nomes[i], ".") == 0 || strcmp(l->nomes[i], "..") == 0)
            return 1;
    }
    return 0;
}

static void disco_read_dir(const char *base)
{
    char pasta[BANCO_CAMINHO];
    char caminho[BANCO_CAMINHO];
    BancoLista lista;
    int32_t erro = 0;
    uint64_t h;
    uint8_t b[4];
    int32_t tipo = 0;

    caminho_de(pasta, base, "lista");
    forja_fs_criar_pasta(pasta);
    caminho_de(caminho, base, "lista\\zeta.txt");
    gravar_texto(caminho, "z");
    caminho_de(caminho, base, "lista\\alfa beta.txt");
    gravar_texto(caminho, "a");
    caminho_de(caminho, base, "lista\\a\303\247\303\243o.txt");
    gravar_texto(caminho, "c");
    caminho_de(caminho, base, "lista\\sub pasta");
    forja_fs_criar_pasta(caminho);
    caso_conferir("read_dir: lista chega ao fim", listar_pasta(pasta, &lista));
    caso_conferir("read_dir: 4 itens e sem . nem ..", lista.quantos == 4 && !tem_ponto_ou_ponto_ponto(&lista));
    caso_conferir("read_dir: nomes com espaco e UTF-8", contem_item(&lista, "alfa beta.txt", 0) && contem_item(&lista, "a\303\247\303\243o.txt", 0));
    caso_conferir("read_dir: tipo de pasta e de arquivo", contem_item(&lista, "sub pasta", 1) && contem_item(&lista, "zeta.txt", 0));
    caminho_de(caminho, base, "vazia");
    forja_fs_criar_pasta(caminho);
    h = forja_fs_pasta_abrir(caminho, &erro);
    caso_conferir("read_dir de pasta vazia: fim logo", h != 0 && forja_fs_pasta_proximo(h, b, 4, &tipo) == 0);
    forja_fs_pasta_fechar(h);
    caminho_de(caminho, base, "nao_existe");
    caso_conferir("read_dir de inexistente = nao existe", forja_fs_pasta_abrir(caminho, &erro) == 0 && erro == FORJA_FS_ERR_NAO_EXISTE);
    caminho_de(caminho, base, "lista\\zeta.txt");
    caso_conferir("read_dir de arquivo = nao e pasta", forja_fs_pasta_abrir(caminho, &erro) == 0 && erro == FORJA_FS_ERR_NAO_DIRETORIO);
    h = forja_fs_pasta_abrir(pasta, &erro);
    forja_fs_pasta_fechar(h);
    caso_conferir("pasta_proximo apos fechar = fechado", forja_fs_pasta_proximo(h, b, 4, &tipo) == -FORJA_FS_ERR_FECHADO);
}

static int modo_disco(const char *base)
{
    disco_arquivos_basicos(base);
    disco_seek(base);
    disco_append(base);
    disco_modos(base);
    disco_fechado(base);
    disco_descarregar(base);
    disco_info(base);
    disco_renomear_copiar(base);
    disco_remover(base);
    disco_pastas(base);
    disco_read_dir(base);
    printf("RESUMO %d %d\n", casos_total, casos_falhos);
    return casos_falhos > 0 ? 1 : 0;
}

/* Hex minusculo de n bytes, em blocos de BANCO_HEX caracteres. */
static void imprimir_hex(const uint8_t *dados, int64_t n)
{
    static const char digitos[] = "0123456789abcdef";
    char saida[BANCO_HEX];
    int64_t i;
    int64_t k = 0;

    for (i = 0; i < n; i++) {
        saida[k++] = digitos[dados[i] >> 4];
        saida[k++] = digitos[dados[i] & 0xF];
        if (k == BANCO_HEX) {
            fwrite(saida, 1, (size_t)k, stdout);
            k = 0;
        }
    }
    fwrite(saida, 1, (size_t)k, stdout);
}

/* Junta as partes de uma linha em acumulo. 1 = uma linha (*achou: 1 com LF, 0 na ultima sem LF); 0 = fim; -1 = erro. */
static int acumular_linha(int64_t *len, int32_t *achou)
{
    *len = 0;
    for (;;) {
        int32_t achou_parte = 0;
        int64_t r = forja_stdin_ler_linha(parte, BANCO_PARTE, &achou_parte);

        if (r < 0)
            return -1;
        if (r == 0 && !achou_parte) {
            *achou = 0;
            return *len > 0 ? 1 : 0;
        }
        if (*len + r > BANCO_ACUMULO)
            return -1;
        memcpy(acumulo + *len, parte, (size_t)r);
        *len += r;
        if (achou_parte) {
            *achou = 1;
            return 1;
        }
    }
}

static int modo_linhas(void)
{
    int64_t len = 0;
    int32_t achou = 0;
    int r;

    for (;;) {
        r = acumular_linha(&len, &achou);
        if (r < 0) {
            printf("ERRO\n");
            return 1;
        }
        if (r == 0)
            break;
        printf("LINHA %lld %d ", (long long)len, (int)achou);
        imprimir_hex(acumulo, len);
        printf("\n");
    }
    printf("FIM\n");
    return 0;
}

static int modo_bruto(void)
{
    int64_t total = 0;
    int64_t r;

    for (;;) {
        r = forja_stdin_ler(parte, BANCO_PARTE);
        if (r < 0) {
            printf("ERRO\n");
            return 1;
        }
        if (r == 0)
            break;
        if (total + r > BANCO_ACUMULO)
            return 1;
        memcpy(acumulo + total, parte, (size_t)r);
        total += r;
    }
    printf("BRUTO %lld ", (long long)total);
    imprimir_hex(acumulo, total);
    printf("\nFIM\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "uso: banco_es.exe disco <pasta> | linhas | bruto | prompt\n");
        return 2;
    }
    if (strcmp(argv[1], "disco") == 0 && argc >= 3)
        return modo_disco(argv[2]);
    if (strcmp(argv[1], "linhas") == 0)
        return modo_linhas();
    if (strcmp(argv[1], "bruto") == 0)
        return modo_bruto();
    if (strcmp(argv[1], "prompt") == 0) {
        printf("Nome: ");
        return modo_linhas();
    }
    fprintf(stderr, "modo desconhecido: %s\n", argv[1]);
    return 2;
}
