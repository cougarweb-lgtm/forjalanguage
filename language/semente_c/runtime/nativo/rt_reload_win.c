/*
 * rt_reload_win.c - std:reload no Windows x64: hot reload de DLL Forja (M8 H2, D-M8-3 e D-M8-4).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M8 H2: primeira versao. forja_reload_estado (GetFileAttributesExW), forja_reload_livre (CreateFileW com share 0), forja_reload_copiar (CreateDirectoryW + CopyFileW),
 *           forja_reload_carregar (LoadLibraryExW), forja_reload_simbolo (GetProcAddress), forja_reload_descarregar (FreeLibrary), forja_reload_info e forja_reload_texto
 *           (forja_dll_info do H1: magic 0x494A4446, formato 1, hash FNV-1a 64 e texto canonico da interface) e forja_reload_limpar (FindFirstFileW + DeleteFileW).
 *           Nenhum import fora de kernel32: LoadLibraryExW, GetProcAddress e FreeLibrary sao o 75o ao 77o do ligar_tabelas.c; o resto ja era da tabela fixa.
 *
 * Contrato dos codigos (todas as procs que falham devolvem int32; 0 = ok): 1 not_found, 2 copy_failed, 3 load_failed (ou outro erro do sistema), 4 unsupported_target (so o Linux),
 * 5 ocupado (o escritor da DLL ainda nao terminou), 6 sem forja_dll_info valido. Caminho = UTF-8 com NUL.
 * Usa rt_os_para_larga (rt_os_win.c) e sys_heap_alocar/sys_heap_liberar: incluido por rt_ryu.c DEPOIS de rt_os_win.c; nao compila sozinho.
 */
#include <stdint.h>

#define RT_RELOAD_OK 0
#define RT_RELOAD_NAO_ACHADO 1
#define RT_RELOAD_COPIA 2
#define RT_RELOAD_OUTRO 3
#define RT_RELOAD_OCUPADO 5
#define RT_RELOAD_SEM_INFO 6

#define RT_RELOAD_ERRO_ARQUIVO_NAO_ACHADO 2UL
#define RT_RELOAD_ERRO_CAMINHO_NAO_ACHADO 3UL
#define RT_RELOAD_ERRO_UNIDADE_INVALIDA 15UL
#define RT_RELOAD_LER 0x80000000UL         /* GENERIC_READ */
#define RT_RELOAD_ABRIR_EXISTENTE 3UL      /* OPEN_EXISTING */
#define RT_RELOAD_ATRIBUTO_NORMAL 0x80UL   /* FILE_ATTRIBUTE_NORMAL */
#define RT_RELOAD_INVALIDO ((void *)(intptr_t)-1) /* INVALID_HANDLE_VALUE */
#define RT_RELOAD_INFO_MAGIC 0x494A4446U   /* "FDJI" */
#define RT_RELOAD_INFO_FORMATO 1U
#define RT_RELOAD_NOME_ARQUIVO_MAX 260

__declspec(dllimport) int GetFileAttributesExW(const unsigned short *nome, int nivel, void *dados);
__declspec(dllimport) void *CreateFileW(const unsigned short *nome, unsigned long acesso, unsigned long compartilha, void *seguranca, unsigned long cria,
                                        unsigned long atributos, void *modelo);
__declspec(dllimport) int CloseHandle(void *h);
__declspec(dllimport) unsigned long GetLastError(void);
__declspec(dllimport) int CopyFileW(const unsigned short *origem, const unsigned short *destino, int falha_se_existe);
__declspec(dllimport) int CreateDirectoryW(const unsigned short *nome, void *seguranca);
__declspec(dllimport) int DeleteFileW(const unsigned short *nome);
__declspec(dllimport) void *FindFirstFileW(const unsigned short *busca, void *dados);
__declspec(dllimport) int FindNextFileW(void *busca, void *dados);
__declspec(dllimport) int FindClose(void *busca);
__declspec(dllimport) void *LoadLibraryExW(const unsigned short *nome, void *reservado, unsigned long flags);
__declspec(dllimport) void *GetProcAddress(void *modulo, const char *nome);
__declspec(dllimport) int FreeLibrary(void *modulo);

/* WIN32_FILE_ATTRIBUTE_DATA: os FILETIME sao 2 DWORD (alinhamento 4), por isso pares de uint32_t e nao uint64_t. */
typedef struct {
    uint32_t atributos;
    uint32_t criacao[2];
    uint32_t acesso[2];
    uint32_t escrita[2];
    uint32_t tamanho_alto;
    uint32_t tamanho_baixo;
} RtReloadAtributos;

/* WIN32_FIND_DATAW (592 bytes): so o nome interessa. */
typedef struct {
    uint32_t cabecalho[11];
    unsigned short nome[RT_RELOAD_NOME_ARQUIVO_MAX];
    unsigned short alternativo[14];
} RtReloadBusca;

/* struct estatica que forja_dll_info devolve (D-M8-2). */
typedef struct {
    uint32_t magic;
    uint32_t formato;
    uint64_t hash_interface;
    uint64_t texto;
    uint64_t texto_len;
} RtReloadInfo;

typedef const RtReloadInfo *(*RtReloadInfoFn)(void);

static int32_t rt_reload_codigo_do_sistema(unsigned long erro)
{
    if (erro == RT_RELOAD_ERRO_ARQUIVO_NAO_ACHADO || erro == RT_RELOAD_ERRO_CAMINHO_NAO_ACHADO || erro == RT_RELOAD_ERRO_UNIDADE_INVALIDA)
        return RT_RELOAD_NAO_ACHADO;
    return RT_RELOAD_OUTRO;
}

static int64_t rt_reload_largura(const unsigned short *larga)
{
    int64_t n = 0;

    while (larga[n] != 0)
        n++;
    return n;
}

/* Tamanho e instante da ultima escrita (FILETIME de 64 bits) do arquivo. */
int32_t forja_reload_estado(const char *caminho, uint64_t *tamanho, uint64_t *escrita)
{
    unsigned short *larga = rt_os_para_larga(caminho);
    RtReloadAtributos a;
    int32_t r;

    if (larga == 0)
        return RT_RELOAD_OUTRO;
    if (GetFileAttributesExW(larga, 0, &a) == 0) {
        r = rt_reload_codigo_do_sistema(GetLastError());
    } else {
        *tamanho = ((uint64_t)a.tamanho_alto << 32) | a.tamanho_baixo;
        *escrita = ((uint64_t)a.escrita[1] << 32) | a.escrita[0];
        r = RT_RELOAD_OK;
    }
    sys_heap_liberar(larga);
    return r;
}

/* 0 = o arquivo abre com share 0 (ninguem mais o tem aberto: o escritor terminou); 5 = ocupado; 1 = nao existe. */
int32_t forja_reload_livre(const char *caminho)
{
    unsigned short *larga = rt_os_para_larga(caminho);
    void *h;
    int32_t r = RT_RELOAD_OK;

    if (larga == 0)
        return RT_RELOAD_OUTRO;
    h = CreateFileW(larga, RT_RELOAD_LER, 0, 0, RT_RELOAD_ABRIR_EXISTENTE, RT_RELOAD_ATRIBUTO_NORMAL, 0);
    if (h == RT_RELOAD_INVALIDO)
        r = rt_reload_codigo_do_sistema(GetLastError()) == RT_RELOAD_NAO_ACHADO ? RT_RELOAD_NAO_ACHADO : RT_RELOAD_OCUPADO;
    else
        CloseHandle(h);
    sys_heap_liberar(larga);
    return r;
}

/* Cria a pasta da sombra (ja existir nao e erro) e copia origem sobre destino. 1 = origem nao existe; 2 = a copia falhou. */
int32_t forja_reload_copiar(const char *origem, const char *pasta, const char *destino)
{
    unsigned short *larga_origem = rt_os_para_larga(origem);
    unsigned short *larga_pasta = rt_os_para_larga(pasta);
    unsigned short *larga_destino = rt_os_para_larga(destino);
    int32_t r = RT_RELOAD_OUTRO;

    if (larga_origem != 0 && larga_pasta != 0 && larga_destino != 0) {
        CreateDirectoryW(larga_pasta, 0);
        if (CopyFileW(larga_origem, larga_destino, 0) != 0)
            r = RT_RELOAD_OK;
        else
            r = rt_reload_codigo_do_sistema(GetLastError()) == RT_RELOAD_NAO_ACHADO ? RT_RELOAD_NAO_ACHADO : RT_RELOAD_COPIA;
    }
    if (larga_origem != 0)
        sys_heap_liberar(larga_origem);
    if (larga_pasta != 0)
        sys_heap_liberar(larga_pasta);
    if (larga_destino != 0)
        sys_heap_liberar(larga_destino);
    return r;
}

/* LoadLibraryExW: handle do modulo ou 0. */
uint64_t forja_reload_carregar(const char *caminho)
{
    unsigned short *larga = rt_os_para_larga(caminho);
    uint64_t h;

    if (larga == 0)
        return 0;
    h = (uint64_t)(uintptr_t)LoadLibraryExW(larga, 0, 0);
    sys_heap_liberar(larga);
    return h;
}

/* GetProcAddress: endereco do simbolo ou NULL. */
void *forja_reload_simbolo(uint64_t modulo, const char *nome)
{
    return (void *)GetProcAddress((void *)(uintptr_t)modulo, nome);
}

void forja_reload_descarregar(uint64_t modulo)
{
    FreeLibrary((void *)(uintptr_t)modulo);
}

/* Chama forja_dll_info do modulo e confere magic e formato; sem o export ou com magic/formato diferente = 6. */
static const RtReloadInfo *rt_reload_info_valida(uint64_t modulo)
{
    RtReloadInfoFn funcao = (RtReloadInfoFn)(uintptr_t)GetProcAddress((void *)(uintptr_t)modulo, "forja_dll_info");
    const RtReloadInfo *info;

    if (funcao == 0)
        return 0;
    info = funcao();
    if (info == 0 || info->magic != RT_RELOAD_INFO_MAGIC || info->formato != RT_RELOAD_INFO_FORMATO)
        return 0;
    return info;
}

/* hash da interface e tamanho do texto canonico. 0 = ok; 6 = a DLL nao tem forja_dll_info valido. */
int32_t forja_reload_info(uint64_t modulo, uint64_t *hash, int64_t *texto_len)
{
    const RtReloadInfo *info = rt_reload_info_valida(modulo);

    if (info == 0)
        return RT_RELOAD_SEM_INFO;
    *hash = info->hash_interface;
    *texto_len = (int64_t)info->texto_len;
    return RT_RELOAD_OK;
}

/* Copia o texto canonico para destino quando cabe (n >= tamanho); devolve o tamanho, ou -6 sem forja_dll_info valido. */
int64_t forja_reload_texto(uint64_t modulo, unsigned char *destino, int64_t n)
{
    const RtReloadInfo *info = rt_reload_info_valida(modulo);
    const unsigned char *texto;

    if (info == 0)
        return -RT_RELOAD_SEM_INFO;
    texto = (const unsigned char *)(uintptr_t)info->texto;
    for (int64_t i = 0; i < (int64_t)info->texto_len && i < n; i++)
        destino[i] = texto[i];
    return (int64_t)info->texto_len;
}

/* Percorre a busca (`pasta\padrao`) e apaga cada achado; alvo e o buffer `pasta\` + nome, ja com a pasta e a barra nas primeiras pasta_n + 1 posicoes. */
static void rt_reload_apagar_achados(const unsigned short *busca, unsigned short *alvo, int64_t pasta_n)
{
    RtReloadBusca achado;
    void *h = FindFirstFileW(busca, &achado);

    if (h == RT_RELOAD_INVALIDO)
        return;
    do {
        int64_t k = 0;

        while (achado.nome[k] != 0) {
            alvo[pasta_n + 1 + k] = achado.nome[k];
            k++;
        }
        alvo[pasta_n + 1 + k] = 0;
        DeleteFileW(alvo);
    } while (FindNextFileW(h, &achado) != 0);
    FindClose(h);
}

/* Monta os dois buffers largos (busca e alvo) e apaga os casados; sem memoria, nao faz nada. */
static void rt_reload_apagar_casados(const unsigned short *larga_pasta, const unsigned short *larga_padrao)
{
    int64_t pasta_n = rt_reload_largura(larga_pasta);
    int64_t padrao_n = rt_reload_largura(larga_padrao);
    unsigned short *busca = sys_heap_alocar((pasta_n + padrao_n + 2) * 2);
    unsigned short *alvo = sys_heap_alocar((pasta_n + RT_RELOAD_NOME_ARQUIVO_MAX + 2) * 2);

    if (busca != 0 && alvo != 0) {
        for (int64_t i = 0; i < pasta_n; i++)
            busca[i] = alvo[i] = larga_pasta[i];
        busca[pasta_n] = alvo[pasta_n] = '\\';
        for (int64_t i = 0; i <= padrao_n; i++)
            busca[pasta_n + 1 + i] = larga_padrao[i];
        rt_reload_apagar_achados(busca, alvo, pasta_n);
    }
    if (busca != 0)
        sys_heap_liberar(busca);
    if (alvo != 0)
        sys_heap_liberar(alvo);
}

/* Apaga em pasta os arquivos que casam com padrao (ex.: "jogo_*.dll"); o que esta carregado (travado) o DeleteFileW recusa e fica. Sempre 0. */
int32_t forja_reload_limpar(const char *pasta, const char *padrao)
{
    unsigned short *larga_pasta = rt_os_para_larga(pasta);
    unsigned short *larga_padrao = rt_os_para_larga(padrao);

    if (larga_pasta != 0 && larga_padrao != 0)
        rt_reload_apagar_casados(larga_pasta, larga_padrao);
    if (larga_pasta != 0)
        sys_heap_liberar(larga_pasta);
    if (larga_padrao != 0)
        sys_heap_liberar(larga_padrao);
    return 0;
}
