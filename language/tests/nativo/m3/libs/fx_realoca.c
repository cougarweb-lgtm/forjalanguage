/* fx_realoca.c - programa de teste da DLL nativa da Forja (M3 P22b; bateria 10.2 g): carrega a DLL FORA da base preferida e chama o que ela exporta.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22b: primeira versao.
 * Uso: fx_realoca.exe <dll> [<dll> ...]
 * Reserva o intervalo que comeca em 0x180000000 (a ImageBase que a Forja e o ld dao a uma DLL) com VirtualAlloc ANTES de cada LoadLibrary: o loader nao pode
 * usar a base preferida e aplica o .reloc. Depois confere (so o que a DLL exporta):
 *   fx_dir64_ler_a/b e fx_dir64_end_a/b (DLL de teste do teste_nativo.exe dll64): o campo ADDR64 relocado tem de valer o endereco real do proprio campo;
 *   realoca_confere (fixture Forja m3\dll\realoca.fj): devolve 0 quando as checagens de dentro da DLL passaram.
 * Saida: uma linha por DLL, sem endereco (a base muda a cada execucao). Codigo 0 so com tudo certo; 3 = nao deu para reservar a base; 4 = a DLL carregou na base preferida.
 * Verificacao: python -I ..\..\abi\verificar_abi.py --lado export --forja <copia>
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BASE_PREFERIDA ((void *)0x180000000ull)
#define TAMANHO_RESERVA (64u * 1024u * 1024u)
#define MENOR_RVA_DADOS 0x2000u

typedef uint64_t (*FuncaoEndereco)(void);
typedef int64_t (*FuncaoConfere)(void);

static const char *NomeDoArquivo(const char *caminho)
{
    const char *nome = caminho;

    for (const char *p = caminho; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/')
            nome = p + 1;
    }
    return nome;
}

/* 1 = campo certo, 0 = errado, -1 = a DLL nao tem estes exports. */
static int ConferirCampos(HMODULE m)
{
    FuncaoEndereco ler_a = (FuncaoEndereco)GetProcAddress(m, "fx_dir64_ler_a");
    FuncaoEndereco ler_b = (FuncaoEndereco)GetProcAddress(m, "fx_dir64_ler_b");
    FuncaoEndereco end_a = (FuncaoEndereco)GetProcAddress(m, "fx_dir64_end_a");
    FuncaoEndereco end_b = (FuncaoEndereco)GetProcAddress(m, "fx_dir64_end_b");
    uint64_t base = (uint64_t)(uintptr_t)m;

    if (ler_a == NULL || ler_b == NULL || end_a == NULL || end_b == NULL)
        return -1;
    if (ler_a() != end_a() || ler_b() != end_b())
        return 0;
    return end_a() - base >= MENOR_RVA_DADOS && end_b() - end_a() == 0x1000u;
}

/* 1 = DLL e fixture, 0 = reprovou, -1 = nao tem o export. */
static int ConferirInterno(HMODULE m)
{
    FuncaoConfere confere = (FuncaoConfere)GetProcAddress(m, "realoca_confere");
    int64_t falha;

    if (confere == NULL)
        return -1;
    falha = confere();
    if (falha != 0)
        printf("    realoca_confere devolveu %lld (a checagem de numero %lld de dentro da DLL falhou)\n", (long long)falha, (long long)falha);
    return falha == 0;
}

static const char *Texto(int estado)
{
    return estado < 0 ? "-" : (estado ? "ok" : "ERRADO");
}

static int CarregarEConferir(const char *caminho)
{
    HMODULE m = LoadLibraryA(caminho);
    int campos, interno;

    if (m == NULL) {
        printf("fx_realoca: LoadLibrary(%s) falhou: %lu\n", NomeDoArquivo(caminho), GetLastError());
        return 1;
    }
    if ((void *)m == BASE_PREFERIDA) {
        printf("fx_realoca: %s carregou na base preferida (a reserva nao valeu)\n", NomeDoArquivo(caminho));
        return 4;
    }
    campos = ConferirCampos(m);
    interno = ConferirInterno(m);
    printf("fx_realoca: %s fora da base preferida; campos ADDR64=%s; realoca_confere=%s\n", NomeDoArquivo(caminho), Texto(campos), Texto(interno));
    return campos == 0 || interno == 0 || (campos < 0 && interno < 0) ? 1 : 0;
}

int main(int argc, char **argv)
{
    int codigo = 0;

    if (argc < 2) {
        fprintf(stderr, "uso: fx_realoca.exe <dll> [<dll> ...]\n");
        return 2;
    }
    if (VirtualAlloc(BASE_PREFERIDA, TAMANHO_RESERVA, MEM_RESERVE, PAGE_NOACCESS) != BASE_PREFERIDA) {
        printf("fx_realoca: nao consegui reservar 0x180000000 (erro %lu)\n", GetLastError());
        return 3;
    }
    for (int k = 1; k < argc; k++) {
        int resultado = CarregarEConferir(argv[k]);

        codigo = codigo != 0 ? codigo : resultado;
    }
    fflush(stdout);
    return codigo;
}
