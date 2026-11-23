/*
 * oraculo_libmem.c - oraculo fixo da contabilidade por lib (M6d, P92a). Liga a MESMA .a do vendor que a fixture usa
 * (libcjson.a / libz.a) e conta, por hook, os bytes que a lib deixa vivos. Os numeros saem daqui e vao para os .esperado.
 *
 * Uso: oraculo_libmem cjson <arquivo.json> | oraculo_libmem zlib
 *   cjson: cJSON_Parse do texto e NAO apaga a arvore (o vazamento de proposito); imprime bytes e blocos vivos.
 *   zlib : inflateInit_ sem inflateEnd; zalloc/zfree contam (o default do zlib e calloc(items, size), o mesmo numero).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M6d P92a: primeira versao. Cabecalho de 16 bytes so para guardar o tamanho (o contador usa o pedido, nao o real).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "zlib.h"

static long long OraculoBytes = 0;
static long long OraculoBlocos = 0;

static void *OraculoAlocar(size_t Pedido)
{
    unsigned char *Bloco = malloc(Pedido + 16);

    if (Bloco == NULL)
        return NULL;
    *(size_t *)Bloco = Pedido;
    OraculoBytes += (long long)Pedido;
    OraculoBlocos++;
    return Bloco + 16;
}

static void OraculoLiberar(void *Dado)
{
    unsigned char *Bloco;

    if (Dado == NULL)
        return;
    Bloco = (unsigned char *)Dado - 16;
    OraculoBytes -= (long long)*(size_t *)Bloco;
    OraculoBlocos--;
    free(Bloco);
}

static void *OraculoMalloc(size_t Pedido)
{
    return OraculoAlocar(Pedido);
}

static void OraculoFree(void *Dado)
{
    OraculoLiberar(Dado);
}

static voidpf OraculoZalloc(voidpf Opaco, uInt Itens, uInt Tamanho)
{
    (void)Opaco;
    return OraculoAlocar((size_t)Itens * Tamanho);
}

static void OraculoZfree(voidpf Opaco, voidpf Dado)
{
    (void)Opaco;
    OraculoLiberar(Dado);
}

static int OraculoCJson(const char *Arquivo)
{
    char Texto[65536];
    size_t Lidos;
    FILE *Entrada = fopen(Arquivo, "rb");
    cJSON_Hooks Hooks = {OraculoMalloc, OraculoFree};

    if (Entrada == NULL) {
        fprintf(stderr, "oraculo: nao abriu %s\n", Arquivo);
        return 1;
    }
    Lidos = fread(Texto, 1, sizeof Texto - 1, Entrada);
    Texto[Lidos] = '\0';
    fclose(Entrada);
    cJSON_InitHooks(&Hooks);
    (void)cJSON_Parse(Texto); /* sem cJSON_Delete: o vazamento e o que se mede */
    printf("cjson bytes=%lld blocos=%lld\n", OraculoBytes, OraculoBlocos);
    return 0;
}

static int OraculoZlib(void)
{
    z_stream Fluxo;
    int Retorno;

    memset(&Fluxo, 0, sizeof Fluxo);
    Fluxo.zalloc = OraculoZalloc;
    Fluxo.zfree = OraculoZfree;
    Fluxo.opaque = Z_NULL;
    Retorno = inflateInit_(&Fluxo, ZLIB_VERSION, (int)sizeof(z_stream));
    printf("zlib bytes=%lld blocos=%lld ret=%d\n", OraculoBytes, OraculoBlocos, Retorno);
    return 0;
}

int main(int Argc, char **Argv)
{
    if (Argc >= 3 && strcmp(Argv[1], "cjson") == 0)
        return OraculoCJson(Argv[2]);
    if (Argc >= 2 && strcmp(Argv[1], "zlib") == 0)
        return OraculoZlib();
    fprintf(stderr, "uso: oraculo_libmem cjson <arquivo.json> | zlib\n");
    return 2;
}
