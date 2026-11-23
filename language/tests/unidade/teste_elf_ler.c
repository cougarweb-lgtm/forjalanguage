/* teste_elf_ler.c - teste do leitor ELF64 relocavel (compiler/nativo/obj/elf_ler.c) e driver de contagem por membro (P33, pista L).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - P33: sem argumentos roda as verificacoes de unidade (ELF sintetico: censo, tipo fora do censo, buffer truncado, magic ruim, nomes);
 *                    "contar <arquivo.a|arquivo.o>" imprime M <membro> <secoes> <simbolos> <relocacoes> por membro, E <membro>: <erro> para o que o leitor
 *                    recusa, e TOTAL <lidos> <erros> (comparar_readelf.py consome); "sintetico <saida.o> [tipo]" grava um .o com 1 relocacao desse tipo.
 * Verificacao: gcc -std=c11 -Wall -Wextra -I compiler compiler/nativo/obj/elf_ler.c tests/unidade/teste_elf_ler.c -o teste_elf_ler.exe && teste_elf_ler.exe
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nativo/obj/elf_ler.h"

#define TESTE_TAM_ASSINATURA_AR 8u
#define TESTE_TAM_CAB_AR 60u
#define TESTE_OFF_TAM_AR 48u
#define TESTE_TAM_NOME_AR 16u
#define TESTE_NOME_MEMBRO 256u
#define TESTE_TAM_SINTETICO 552u
#define TESTE_OFF_SHDR_SINTETICO 168u
#define TESTE_TAM_NOMES_SINTETICO 44u
#define TESTE_TIPO_PADRAO_SINTETICO 26u

static const char TESTE_ASSINATURA_AR[] = "!<arch>\n";
/* Tabela de nomes da secao .shstrtab do objeto sintetico: 44 bytes, com o NUL inicial. */
static const char TESTE_NOMES_SHSTR[] = "\0.text\0.rela.text\0.symtab\0.strtab\0.shstrtab";

static int TesteVerificacoes = 0;

#define TESTE_CHECAR(cond, desc)                                                  \
    do {                                                                          \
        if (!(cond)) {                                                            \
            fprintf(stderr, "FALHOU: %s (teste_elf_ler.c linha %d)\n", desc, __LINE__); \
            return 1;                                                             \
        }                                                                         \
        TesteVerificacoes++;                                                      \
    } while (0)

/* Cabecalho de secao do objeto sintetico, na ordem: null, .text, .rela.text, .symtab, .strtab, .shstrtab. */
typedef struct {
    uint32_t nome;
    uint32_t tipo;
    uint64_t flags;
    uint64_t off;
    uint64_t tam;
    uint32_t link;
    uint32_t info;
    uint64_t align;
    uint64_t entsize;
} TesteSecao;

static const TesteSecao TESTE_SECOES[6] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 6, 64, 8, 0, 0, 1, 0},        /* .text (PROGBITS) */
    {7, 4, 0x40, 72, 24, 3, 1, 8, 24},   /* .rela.text (RELA): sh_link = symtab, sh_info = .text */
    {18, 2, 0, 96, 24, 4, 1, 8, 24},     /* .symtab: sh_link = strtab */
    {26, 3, 0, 120, 1, 0, 0, 1, 0},      /* .strtab */
    {34, 3, 0, 121, 44, 0, 0, 1, 0},     /* .shstrtab */
};

/* Ponteiro de leitura do ar: nomes e a tabela "//" (nomes longos) vao sendo achados no percurso. */
typedef struct {
    const uint8_t *bytes;
    size_t n;
    size_t pos;
    const uint8_t *nomes;
    size_t nomes_tam;
} TesteAr;

static void TestePut(uint8_t *p, uint64_t v, size_t tam)
{
    for (size_t i = 0; i < tam; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

static void TesteGravarShdr(uint8_t *sh, const TesteSecao *s)
{
    TestePut(sh + 0, s->nome, 4);
    TestePut(sh + 4, s->tipo, 4);
    TestePut(sh + 8, s->flags, 8);
    TestePut(sh + 24, s->off, 8);
    TestePut(sh + 32, s->tam, 8);
    TestePut(sh + 40, s->link, 4);
    TestePut(sh + 44, s->info, 4);
    TestePut(sh + 48, s->align, 8);
    TestePut(sh + 56, s->entsize, 8);
}

/* Objeto ELF64 minimo com uma relocacao do tipo dado (simbolo 0, addend 0) em .rela.text. Tudo fica no proprio buffer de 552 bytes. */
static void TesteObjetoSintetico(uint8_t *b, uint32_t tipo_reloc)
{
    memset(b, 0, TESTE_TAM_SINTETICO);
    memcpy(b, "\x7f" "ELF", 4);
    b[4] = 2;
    b[5] = 1;
    b[6] = 1;
    TestePut(b + 16, 1, 2);
    TestePut(b + 18, 62, 2);
    TestePut(b + 20, 1, 4);
    TestePut(b + 40, TESTE_OFF_SHDR_SINTETICO, 8);
    TestePut(b + 52, 64, 2);
    TestePut(b + 58, 64, 2);
    TestePut(b + 60, 6, 2);
    TestePut(b + 62, 5, 2);
    memcpy(b + 121, TESTE_NOMES_SHSTR, TESTE_TAM_NOMES_SINTETICO);
    TestePut(b + 80, tipo_reloc, 8);
    for (size_t i = 0; i < 6; i++)
        TesteGravarShdr(b + TESTE_OFF_SHDR_SINTETICO + i * 64, &TESTE_SECOES[i]);
}

static int TesteCasoCensoOk(void)
{
    uint8_t b[TESTE_TAM_SINTETICO];
    ElfLObjeto obj;
    ElfLErro erro;
    TesteObjetoSintetico(b, 2);
    ElfLCodigo cod = ElfLer(b, sizeof b, &obj, &erro);
    TESTE_CHECAR(cod == ELFL_OK, "relocacao PC32 (dentro do censo) e lida sem erro");
    TESTE_CHECAR(obj.n_secoes == 6, "6 secoes (a nula conta, como em e_shnum)");
    TESTE_CHECAR(obj.n_simbolos == 1, "symtab conta a entrada nula");
    TESTE_CHECAR(obj.simbolos[0].nome[0] == '\0', "entrada nula da symtab tem nome vazio");
    TESTE_CHECAR(obj.n_relocacoes == 1 && obj.relocacoes[0].tipo == 2, "1 relocacao do tipo 2 (PC32)");
    TESTE_CHECAR(strcmp(obj.secoes[2].nome, ".rela.text") == 0, "nome da secao RELA vem de .shstrtab");
    ElfLLiberar(&obj);
    return 0;
}

static int TesteCasoForaDoCenso(void)
{
    uint8_t b[TESTE_TAM_SINTETICO];
    ElfLObjeto obj;
    ElfLErro erro;
    TesteObjetoSintetico(b, TESTE_TIPO_PADRAO_SINTETICO);
    ElfLCodigo cod = ElfLer(b, sizeof b, &obj, &erro);
    TESTE_CHECAR(cod == ELFL_RELOC_FORA_DO_CENSO, "tipo 26 (GOTPC32) fora do censo e erro");
    TESTE_CHECAR(erro.tipo_reloc == 26, "o erro carrega o tipo 26");
    TESTE_CHECAR(strstr(erro.mensagem, "26") != NULL, "mensagem cita o tipo");
    TESTE_CHECAR(strstr(erro.mensagem, ".rela.text") != NULL, "mensagem cita a secao");
    TESTE_CHECAR(obj.n_relocacoes == 1, "a contagem sai inteira mesmo com erro de censo");
    ElfLLiberar(&obj);
    return 0;
}

static int TesteCasoFormato(void)
{
    uint8_t b[TESTE_TAM_SINTETICO];
    ElfLObjeto obj;
    ElfLErro erro;
    TesteObjetoSintetico(b, 2);
    TESTE_CHECAR(ElfLer(b, 100, &obj, &erro) == ELFL_FORMATO, "buffer truncado (100 bytes) e erro de formato");
    ElfLLiberar(&obj);
    b[0] = 'X';
    TESTE_CHECAR(ElfLer(b, sizeof b, &obj, &erro) == ELFL_FORMATO, "magic ruim e erro de formato");
    TESTE_CHECAR(strstr(erro.mensagem, "assinatura") != NULL, "mensagem diz que falta a assinatura");
    ElfLLiberar(&obj);
    return 0;
}

static int TesteCasoNomes(void)
{
    TESTE_CHECAR(strcmp(ElfLNomeReloc(4), "R_X86_64_PLT32") == 0, "tipo 4 tem nome PLT32");
    TESTE_CHECAR(strcmp(ElfLNomeReloc(42), "R_X86_64_REX_GOTPCRELX") == 0, "tipo 42 tem nome REX_GOTPCRELX");
    TESTE_CHECAR(strcmp(ElfLNomeReloc(22), "R_X86_64_GOTTPOFF") == 0, "tipo 22 tem nome GOTTPOFF");
    TESTE_CHECAR(strcmp(ElfLNomeReloc(26), "?") == 0, "tipo fora do censo sem nome");
    return 0;
}

static int TesteRodarUnidades(void)
{
    int falhas = TesteCasoCensoOk() + TesteCasoForaDoCenso() + TesteCasoFormato() + TesteCasoNomes();
    if (falhas != 0)
        return 1;
    printf("OK: %d verificacoes\n", TesteVerificacoes);
    return 0;
}

static int TesteGravarSintetico(const char *saida, uint32_t tipo)
{
    uint8_t b[TESTE_TAM_SINTETICO];
    TesteObjetoSintetico(b, tipo);
    FILE *f = fopen(saida, "wb");
    if (f == NULL) {
        fprintf(stderr, "nao consegui gravar %s\n", saida);
        return 1;
    }
    size_t escritos = fwrite(b, 1, sizeof b, f);
    fclose(f);
    return escritos == sizeof b ? 0 : 1;
}

/* Arquivo inteiro em memoria (malloc). NULL se nao abriu ou nao leu tudo. */
static uint8_t *TesteLerArquivo(const char *caminho, size_t *tam)
{
    FILE *f = fopen(caminho, "rb");
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long lon = ftell(f);
    if (lon < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *b = malloc(lon > 0 ? (size_t)lon : 1u);
    size_t lidos = b != NULL ? fread(b, 1, (size_t)lon, f) : 0;
    fclose(f);
    if (b == NULL || lidos != (size_t)lon) {
        free(b);
        return NULL;
    }
    *tam = lidos;
    return b;
}

/* Campo decimal de tamanho fixo, a direita com espacos (cabecalho do ar). SIZE_MAX se nao ha digito ou ha lixo. */
static size_t TesteDecimal(const uint8_t *campo, size_t tam)
{
    size_t valor = 0, i = 0;
    for (; i < tam && campo[i] >= '0' && campo[i] <= '9'; i++)
        valor = valor * 10 + (size_t)(campo[i] - '0');
    for (size_t j = i; j < tam; j++) {
        if (campo[j] != ' ')
            return SIZE_MAX;
    }
    return i > 0 ? valor : SIZE_MAX;
}

/* Nome do membro pelo cabecalho: 1 = nome em nome; 0 = "/" (indice de simbolos, pula); 2 = "//" (tabela de nomes longos); -1 = nome longo sem tabela. */
static int TesteNomear(const TesteAr *ar, const uint8_t *cab, char *nome)
{
    if (cab[0] != '/') {
        size_t k = 0;
        while (k < TESTE_TAM_NOME_AR && cab[k] != '/')
            k++;
        if (k == TESTE_TAM_NOME_AR) {
            while (k > 0 && cab[k - 1] == ' ')
                k--;
        }
        snprintf(nome, TESTE_NOME_MEMBRO, "%.*s", (int)k, (const char *)cab);
        return 1;
    }
    if (cab[1] == ' ')
        return 0;
    if (cab[1] == '/')
        return 2;
    size_t off = TesteDecimal(cab + 1, TESTE_TAM_NOME_AR - 1);
    if (off == SIZE_MAX || ar->nomes == NULL || off >= ar->nomes_tam)
        return -1;
    size_t k = 0;
    while (off + k < ar->nomes_tam && ar->nomes[off + k] != '/' && ar->nomes[off + k] != '\n')
        k++;
    snprintf(nome, TESTE_NOME_MEMBRO, "%.*s", (int)k, (const char *)(ar->nomes + off));
    return 1;
}

/* Proximo membro com nome: 1 = achou (nome, dados, tam); 0 = fim do arquivo; -1 = ar malformado (cabecalho truncado, tamanho que passa do arquivo). */
static int TesteArProximo(TesteAr *ar, char *nome, const uint8_t **dados, size_t *tam)
{
    while (ar->pos < ar->n) {
        if (ar->n - ar->pos < TESTE_TAM_CAB_AR)
            return -1;
        const uint8_t *cab = ar->bytes + ar->pos;
        size_t t = TesteDecimal(cab + TESTE_OFF_TAM_AR, 10);
        if (t == SIZE_MAX || t > ar->n - ar->pos - TESTE_TAM_CAB_AR)
            return -1;
        int classe = TesteNomear(ar, cab, nome);
        if (classe < 0)
            return -1;
        *dados = cab + TESTE_TAM_CAB_AR;
        *tam = t;
        ar->pos += TESTE_TAM_CAB_AR + t + (t & 1u);   /* membros sao alinhados em par */
        if (classe == 2) {
            ar->nomes = *dados;
            ar->nomes_tam = t;
            continue;
        }
        if (classe == 1)
            return 1;
    }
    return 0;
}

/* Lê um membro e imprime M ou E. Erro do leitor conta em erros e nao para os outros membros. */
static void TesteContarMembro(const char *nome, const uint8_t *dados, size_t tam, size_t *erros)
{
    ElfLObjeto obj;
    ElfLErro erro;
    ElfLCodigo cod = ElfLer(dados, tam, &obj, &erro);
    if (cod == ELFL_OK) {
        printf("M %s %zu %zu %zu\n", nome, obj.n_secoes, obj.n_simbolos, obj.n_relocacoes);
    } else {
        printf("E %s: %s\n", nome, erro.mensagem);
        (*erros)++;
    }
    ElfLLiberar(&obj);
}

static int TesteContarAr(const uint8_t *b, size_t tam, size_t *lidos, size_t *erros)
{
    TesteAr ar = {b, tam, TESTE_TAM_ASSINATURA_AR, NULL, 0};
    char nome[TESTE_NOME_MEMBRO];
    const uint8_t *dados = NULL;
    size_t t = 0;
    int r;
    while ((r = TesteArProximo(&ar, nome, &dados, &t)) == 1) {
        TesteContarMembro(nome, dados, t, erros);
        (*lidos)++;
    }
    if (r < 0) {
        fprintf(stderr, "ar malformado perto do offset %zu\n", ar.pos);
        return 1;
    }
    return 0;
}

static void TesteContarUnico(const char *caminho, const uint8_t *b, size_t tam, size_t *lidos, size_t *erros)
{
    const char *base = caminho;
    for (const char *p = caminho; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    TesteContarMembro(base, b, tam, erros);
    (*lidos)++;
}

static int TesteContar(const char *caminho)
{
    size_t tam = 0, lidos = 0, erros = 0;
    uint8_t *b = TesteLerArquivo(caminho, &tam);
    if (b == NULL) {
        fprintf(stderr, "nao consegui ler %s\n", caminho);
        return 1;
    }
    int rc = 0;
    if (tam >= TESTE_TAM_ASSINATURA_AR && memcmp(b, TESTE_ASSINATURA_AR, TESTE_TAM_ASSINATURA_AR) == 0)
        rc = TesteContarAr(b, tam, &lidos, &erros);
    else
        TesteContarUnico(caminho, b, tam, &lidos, &erros);
    free(b);
    printf("TOTAL %zu %zu\n", lidos, erros);
    if (rc != 0)
        return rc;
    return erros > 0 ? 2 : 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "contar") == 0)
        return TesteContar(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "sintetico") == 0) {
        uint32_t tipo = argc >= 4 ? (uint32_t)strtoul(argv[3], NULL, 10) : TESTE_TIPO_PADRAO_SINTETICO;
        return TesteGravarSintetico(argv[2], tipo);
    }
    return TesteRodarUnidades();
}
