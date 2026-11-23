/*
 * rt_perfil_win.c - a parte Windows x64 do perfil (M8 P1, D-M8-5/D-M8-6): id da thread, abrir/gravar/fechar o arquivo de saida.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M8 P1: primeira versao. Id da thread pelo TEB (gs:[0x48], ClientId.UniqueThread; um asm de 1 linha, como rt_fio.h, sem import) e CreateFileW/WriteFile/CloseHandle,
 *           GetEnvironmentVariableW e GetModuleFileNameW de kernel32 (todos ja na tabela fixa do ligador: nenhum import novo).
 *
 * Incluido por rt_perfil.c (que o escolhe por RT_ALVO_WIN); nao compila sozinho. Os prototipos sao a mao, sem windows.h.
 */

#define PERFIL_CAMINHO_MAX 1024u
#define PERFIL_NOME_AMBIENTE_MAX 32u
#define PERFIL_ACESSO_ESCRITA 0x40000000UL /* GENERIC_WRITE */
#define PERFIL_COMPARTILHAR_LEITURA 1UL    /* FILE_SHARE_READ */
#define PERFIL_CRIAR_SEMPRE 2UL            /* CREATE_ALWAYS */
#define PERFIL_ATRIBUTO_NORMAL 0x80UL      /* FILE_ATTRIBUTE_NORMAL */
#define PERFIL_LOTE_ESCRITA 0x40000000UL   /* WriteFile tem DWORD de 32 bits: lotes de 1 GiB */

__declspec(dllimport) unsigned long GetEnvironmentVariableW(const uint16_t *nome, uint16_t *destino, unsigned long n);
__declspec(dllimport) unsigned long GetModuleFileNameW(void *modulo, uint16_t *destino, unsigned long n);
__declspec(dllimport) void *CreateFileW(const uint16_t *caminho, unsigned long acesso, unsigned long compartilhar, void *seguranca, unsigned long criacao,
                                        unsigned long atributos, void *modelo);
__declspec(dllimport) int WriteFile(void *arquivo, const void *dados, unsigned long n, unsigned long *escritos, void *sobreposto);
__declspec(dllimport) int CloseHandle(void *arquivo);

/* Id da thread atual: o ClientId.UniqueThread do TEB (gs:[0x48]), lido direto como o slot do RtFio em rt_fio.h (GetCurrentThreadId nao esta na tabela fixa de imports do ligador). */
static uint32_t perfil_so_tid(void)
{
    uint32_t tid;

    __asm__("movl %%gs:0x48, %0" : "=r"(tid));
    return tid;
}

/* Copia o texto ASCII (com NUL) para UTF-16; devolve quantos caracteres, sem o NUL. */
static uint32_t perfil_so_ampliar(uint16_t *destino, const char *origem)
{
    uint32_t i = 0;

    for (; origem[i] != '\0'; i++)
        destino[i] = (uint16_t)(unsigned char)origem[i];
    destino[i] = 0;
    return i;
}

/* Abre (cria ou zera) "<base><sufixo>" para gravar; a base e FORJA_PERFIL_SAIDA ou, sem ela, o caminho do proprio exe. 0 = falhou. */
static uintptr_t perfil_so_abrir(const char *sufixo)
{
    uint16_t caminho[PERFIL_CAMINHO_MAX];
    uint16_t nome[PERFIL_NOME_AMBIENTE_MAX];
    unsigned long n;
    uint32_t i;
    void *arquivo;

    perfil_so_ampliar(nome, "FORJA_PERFIL_SAIDA");
    n = GetEnvironmentVariableW(nome, caminho, PERFIL_CAMINHO_MAX);
    if (n == 0 || n >= PERFIL_CAMINHO_MAX)
        n = GetModuleFileNameW(NULL, caminho, PERFIL_CAMINHO_MAX);
    if (n == 0 || n >= PERFIL_CAMINHO_MAX - 16u)
        return 0;
    i = perfil_so_ampliar(caminho + n, sufixo);
    if (i == 0)
        return 0;
    arquivo = CreateFileW(caminho, PERFIL_ACESSO_ESCRITA, PERFIL_COMPARTILHAR_LEITURA, NULL, PERFIL_CRIAR_SEMPRE, PERFIL_ATRIBUTO_NORMAL, NULL);
    return arquivo == (void *)(intptr_t)-1 ? 0 : (uintptr_t)arquivo;
}

/* Grava os n bytes (repete ate sair tudo); false em falha. */
static bool perfil_so_gravar(uintptr_t arquivo, const void *dados, int64_t n)
{
    const uint8_t *p = (const uint8_t *)dados;

    while (n > 0) {
        unsigned long pedido = n > (int64_t)PERFIL_LOTE_ESCRITA ? (unsigned long)PERFIL_LOTE_ESCRITA : (unsigned long)n;
        unsigned long escritos = 0;

        if (!WriteFile((void *)arquivo, p, pedido, &escritos, NULL) || escritos == 0)
            return false;
        p += escritos;
        n -= (int64_t)escritos;
    }
    return true;
}

static void perfil_so_fechar(uintptr_t arquivo)
{
    CloseHandle((void *)arquivo);
}
