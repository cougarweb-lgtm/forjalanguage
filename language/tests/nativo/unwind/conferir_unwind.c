/*
 * conferir_unwind.c - oraculo de unwind da Forja x64 (P8, M1). Compilar com:
 *   gcc -std=c11 -O2 -Wall -Wextra -Werror conferir_unwind.c -o conferir_unwind.exe
 *
 * Uso:
 *   conferir_unwind.exe <exe> <lista.txt>   confere os pontos da lista
 *   conferir_unwind.exe --autoteste <exe>   so prova que o programa roda: acha todas as
 *                                           entradas de .pdata do exe por RtlLookupFunctionEntry
 *
 * Lista: uma linha por ponto no meio do corpo de uma funcao com quadro rbp, em hexadecimal:
 *   <RVA_do_ponto> <RVA_esperado_do_retorno> <rbp_salvo_esperado> [<bytes_do_rbp_ao_rsp>]
 * Linhas vazias e as que comecam com '#' sao ignoradas.
 *
 * Como confere: monta um CONTEXT sintetico com Rip no ponto, Rbp apontando para um quadro
 * falso (buf[32] = rbp salvo, buf[33] = endereco de retorno) e Rsp abaixo do Rbp. Desenrola
 * com RtlVirtualUnwind e exige Rip = retorno, Rbp = rbp salvo e Rsp = Rbp + 16.
 *
 * Saida: 0 quando todos os pontos batem; 1 quando algum diverge; 2 para erro de uso/arquivo.
 *
 * Versao: 1.1.0
 * Changelog: 1.1.0 - M7a F3: 4o campo opcional da lista (bytes do rbp ao rsp real do ponto) e pilha falsa alocada do tamanho do quadro: o unwinder le os PUSH_NONVOL do alocador de [rsp].
 *            1.0.0 - primeira versao (P8-T do M1).
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAMANHO_QUADRO 64
#define INDICE_RBP 32
#define INDICE_RETORNO 33
#define LINHA_MAX 256

typedef struct {
    ULONG64 base;
    ULONG64 pontos;
    ULONG64 ok;
    ULONG64 falhas;
} Contagem;

/* tam_quadro = bytes entre o rbp e o rsp real do ponto (0 = ponto no prologo: rsp 0x40 abaixo do rbp). Com ALLOC, o unwinder soma o alocado ao rsp e, com PUSH_NONVOL
 * (alocador de registradores), le os callee-saved empurrados de [rsp]: a pilha falsa precisa ter o quadro todo. */
static int DesenrolarPonto(ULONG64 base, ULONG64 rva_ponto, ULONG64 rva_retorno,
                           ULONG64 rbp_salvo, ULONG64 tam_quadro, Contagem *conta)
{
    ULONG64 abaixo = tam_quadro > 0x40 ? tam_quadro : 0x40;
    ULONG64 bytes = abaixo + (INDICE_RETORNO + 2) * sizeof(ULONG64) + 0x100;
    ULONG64 *memoria = calloc(1, (size_t)bytes);
    ULONG64 *quadro;
    ULONG64 base_imagem = 0;
    ULONG64 quadro_estab = 0;
    PVOID dados_handler = NULL;
    CONTEXT ctx;
    PRUNTIME_FUNCTION funcao;
    ULONG64 pc = base + rva_ponto;
    int bate;

    if (memoria == NULL) {
        conta->pontos++;
        conta->falhas++;
        return 0;
    }
    quadro = memoria + (abaixo + 0x80) / sizeof(ULONG64);
    quadro[0] = rbp_salvo;
    quadro[1] = base + rva_retorno;

    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_FULL;
    ctx.Rip = pc;
    ctx.Rbp = (ULONG64)&quadro[0];
    ctx.Rsp = ctx.Rbp - (tam_quadro > 0 ? tam_quadro : 0x40);

    funcao = RtlLookupFunctionEntry(pc, &base_imagem, NULL);
    conta->pontos++;
    if (funcao == NULL || base_imagem != base) {
        printf("falha rva=0x%llx: RtlLookupFunctionEntry nao achou a funcao\n",
               (unsigned long long)rva_ponto);
        conta->falhas++;
        free(memoria);
        return 0;
    }
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, base_imagem, pc, funcao, &ctx, &dados_handler,
                     &quadro_estab, NULL);

    bate = ctx.Rip == base + rva_retorno
        && ctx.Rbp == rbp_salvo
        && ctx.Rsp == (ULONG64)&quadro[2];
    if (bate) {
        conta->ok++;
        printf("ok rva=0x%llx\n", (unsigned long long)rva_ponto);
    } else {
        conta->falhas++;
        printf("falha rva=0x%llx: rip=0x%llx (esperado 0x%llx) rbp=0x%llx (esperado 0x%llx) "
               "rsp-rbp=%lld (esperado 16)\n",
               (unsigned long long)rva_ponto,
               (unsigned long long)ctx.Rip, (unsigned long long)(base + rva_retorno),
               (unsigned long long)ctx.Rbp, (unsigned long long)rbp_salvo,
               (long long)(ctx.Rsp - ctx.Rbp));
    }
    free(memoria);
    return bate;
}

static int ConferirLista(const char *exe, const char *arquivo_lista)
{
    Contagem conta;
    char linha[LINHA_MAX];
    FILE *lista;
    HMODULE modulo;
    wchar_t caminho_w[MAX_PATH];
    ULONG64 base;

    lista = fopen(arquivo_lista, "r");
    if (lista == NULL) {
        fprintf(stderr, "X0001 nao consegui abrir a lista '%s'\n", arquivo_lista);
        return 2;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, exe, -1, caminho_w, MAX_PATH) == 0) {
        fprintf(stderr, "X0001 caminho do exe invalido: '%s'\n", exe);
        fclose(lista);
        return 2;
    }
    modulo = LoadLibraryExW(caminho_w, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (modulo == NULL) {
        fprintf(stderr, "X0001 LoadLibraryExW falhou em '%s' (erro %lu)\n", exe, GetLastError());
        fclose(lista);
        return 2;
    }
    base = (ULONG64)modulo;
    memset(&conta, 0, sizeof(conta));
    conta.base = base;

    while (fgets(linha, sizeof(linha), lista) != NULL) {
        unsigned long long rva_ponto = 0;
        unsigned long long rva_retorno = 0;
        unsigned long long rbp_salvo = 0;
        unsigned long long tam_quadro = 0;
        int campos;
        if (linha[0] == '#' || linha[0] == '\n' || linha[0] == '\r') {
            continue;
        }
        campos = sscanf(linha, "%llx %llx %llx %llx", &rva_ponto, &rva_retorno, &rbp_salvo, &tam_quadro);
        if (campos != 3 && campos != 4) {
            fprintf(stderr, "X0001 linha da lista sem 3 ou 4 campos hex: %s", linha);
            fclose(lista);
            FreeLibrary(modulo);
            return 2;
        }
        DesenrolarPonto(base, rva_ponto, rva_retorno, rbp_salvo, tam_quadro, &conta);
    }
    fclose(lista);
    FreeLibrary(modulo);
    printf("pontos=%llu ok=%llu falhas=%llu\n",
           (unsigned long long)conta.pontos, (unsigned long long)conta.ok,
           (unsigned long long)conta.falhas);
    return conta.falhas == 0 && conta.pontos > 0 ? 0 : 1;
}

static int Autoteste(const char *exe)
{
    HMODULE modulo;
    wchar_t caminho_w[MAX_PATH];
    ULONG64 base;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS64 *nt;
    IMAGE_DATA_DIRECTORY diretorio;
    PRUNTIME_FUNCTION tabela;
    DWORD total;
    DWORD i;
    DWORD achadas = 0;

    if (MultiByteToWideChar(CP_UTF8, 0, exe, -1, caminho_w, MAX_PATH) == 0) {
        fprintf(stderr, "X0001 caminho do exe invalido\n");
        return 2;
    }
    modulo = LoadLibraryExW(caminho_w, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (modulo == NULL) {
        fprintf(stderr, "X0001 LoadLibraryExW falhou em '%s' (erro %lu)\n", exe, GetLastError());
        return 2;
    }
    base = (ULONG64)modulo;
    dos = (IMAGE_DOS_HEADER *)modulo;
    nt = (IMAGE_NT_HEADERS64 *)(base + (ULONG64)dos->e_lfanew);
    diretorio = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (diretorio.VirtualAddress == 0 || diretorio.Size == 0) {
        printf("autoteste: exe sem .pdata\n");
        FreeLibrary(modulo);
        return 1;
    }
    tabela = (PRUNTIME_FUNCTION)(base + diretorio.VirtualAddress);
    total = diretorio.Size / (DWORD)sizeof(RUNTIME_FUNCTION);
    for (i = 0; i < total; i++) {
        ULONG64 base_imagem = 0;
        /* Meio da funcao: o ponto cai no corpo, longe do prologo e do epilogo. */
        ULONG64 pc = base + tabela[i].BeginAddress
            + (ULONG64)((tabela[i].EndAddress - tabela[i].BeginAddress) / 2);
        PRUNTIME_FUNCTION achada = RtlLookupFunctionEntry(pc, &base_imagem, NULL);
        if (achada != NULL && base_imagem == base && achada->BeginAddress == tabela[i].BeginAddress) {
            achadas++;
        }
    }
    printf("autoteste: %lu funcoes em .pdata, %lu achadas por RtlLookupFunctionEntry\n",
           (unsigned long)total, (unsigned long)achadas);
    FreeLibrary(modulo);
    return achadas == total && total > 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--autoteste") == 0) {
        return Autoteste(argv[2]);
    }
    if (argc == 3) {
        return ConferirLista(argv[1], argv[2]);
    }
    fprintf(stderr, "uso: conferir_unwind <exe> <lista.txt>\n");
    fprintf(stderr, "     conferir_unwind --autoteste <exe>\n");
    return 2;
}
