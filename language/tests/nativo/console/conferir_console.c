/*
 * conferir_console.c - oraculo do console UTF-8 do runtime nativo da Forja (passo extra do M1).
 *
 * Versao: 1.1.0
 * Changelog:
 *   1.1.0 - 0.21.0 F4 passo 3: sai o controle negativo com o exe do backend C (o backend C foi aposentado); uso de um argumento so.
 *   1.0.0 - primeira versao: console proprio na pagina 850, exe do 01_ola com os handles do console, tela lida com
 *           ReadConsoleOutputCharacterW; caso do pipe (bytes UTF-8 e pagina intacta); controle negativo com o exe do backend C.
 *
 * Uso: conferir_console <ola_x64.exe>
 *   1. console novo (FreeConsole + AllocConsole), SetConsoleOutputCP(850); roda o ola_x64 herdando o console; a linha 0 da tela
 *      tem de ser "Olá, Forja!", as linhas 1 e 2 "42" e "21", e a pagina de saida tem de voltar a 850 depois do exit.
 *   2. o mesmo ola_x64 com stdout e stderr num pipe: bytes brutos "Ol" 0xC3 0xA1 ", Forja!\r\n42\r\n21\r\n" e pagina ainda 850.
 * Saida: uma linha "ok:" ou "FALHA:" por checagem e o resumo "conferir_console: checagens=N ok=M falhas=K". Exit 0 so com falhas=0.
 * Sem console utilizavel (AllocConsole falha) o exit e 2 com o diagnostico.
 * Verificacao: python -I conferir_console.py --forja ..\..\..\compiler\forja.exe
 */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define PAGINA_OEM 850
#define LINHAS_LIDAS 3
#define LARGURA_MAX 512
#define ESPERA_MS 20000
#define BYTES_PIPE 4096

static int Checagens;
static int Falhas;

static const wchar_t *const LinhasEsperadas[LINHAS_LIDAS] = {L"Olá, Forja!", L"42", L"21"};
static const char BytesEsperados[] = "Ol\xC3\xA1, Forja!\r\n42\r\n21\r\n";

typedef struct {
    wchar_t linha[LINHAS_LIDAS][LARGURA_MAX];
    DWORD codigo;
    UINT pagina_depois;
} Tela;

static void Confere(int ok, const char *descricao)
{
    Checagens++;
    if (!ok)
        Falhas++;
    printf("%s: %s\n", ok ? "ok" : "FALHA", descricao);
}

/* Console novo e limpo, com a pagina de saida em 850. Devolve 0 quando o sistema nao deu console. */
static int NovoConsole(void)
{
    HWND janela;

    FreeConsole();
    if (!AllocConsole())
        return 0;
    janela = GetConsoleWindow();
    if (janela != NULL)
        ShowWindow(janela, SW_HIDE);
    return SetConsoleOutputCP(PAGINA_OEM) != 0;
}

static HANDLE AbrirConsole(const char *nome, int herdavel)
{
    SECURITY_ATTRIBUTES sa;

    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = herdavel;
    return CreateFileA(nome, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
}

/* Cria o filho com os 3 handles dados, espera o fim e devolve o exit code (0xFFFFFFFF se nao subiu ou travou). */
static DWORD RodarFilho(const char *exe, HANDLE entrada, HANDLE saida, HANDLE erro)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char comando[MAX_PATH + 4];
    DWORD codigo = 0xFFFFFFFFu;

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = entrada;
    si.hStdOutput = saida;
    si.hStdError = erro;
    snprintf(comando, sizeof comando, "\"%s\"", exe);
    if (!CreateProcessA(NULL, comando, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        return codigo;
    if (WaitForSingleObject(pi.hProcess, ESPERA_MS) == WAIT_OBJECT_0)
        GetExitCodeProcess(pi.hProcess, &codigo);
    else
        TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return codigo;
}

/* Le a linha y da tela sem os espacos finais. */
static void LerLinha(HANDLE tela, SHORT y, wchar_t *destino)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD origem;
    DWORD largura = LARGURA_MAX - 1;
    DWORD lidos = 0;

    origem.X = 0;
    origem.Y = y;
    destino[0] = L'\0';
    /* A leitura continua na linha seguinte: pede exatamente a largura do buffer, que e uma linha. */
    if (GetConsoleScreenBufferInfo(tela, &info) && (DWORD)info.dwSize.X < largura)
        largura = (DWORD)info.dwSize.X;
    if (!ReadConsoleOutputCharacterW(tela, destino, largura, origem, &lidos))
        lidos = 0;
    destino[lidos] = L'\0';
    while (lidos > 0 && destino[lidos - 1] == L' ')
        destino[--lidos] = L'\0';
}

/* Mostra a linha da tela em ASCII puro (\uXXXX fora de 0x20..0x7E): stdout do oraculo vai para pipe e nao tem codificacao fixa. */
static void MostrarLinha(int indice, const wchar_t *texto)
{
    printf("  tela[%d] = \"", indice);
    for (; *texto != L'\0'; texto++) {
        if (*texto >= 0x20 && *texto <= 0x7E)
            putchar((int)*texto);
        else
            printf("\\u%04x", (unsigned)*texto);
    }
    printf("\"\n");
}

/* Console novo em 850, filho com os handles do console, tela das 3 primeiras linhas e a pagina depois do exit. */
static int RodarNoConsole(const char *exe, Tela *t)
{
    HANDLE tela, entrada, saida;
    SHORT y;

    if (!NovoConsole())
        return 0;
    tela = AbrirConsole("CONOUT$", 0);
    entrada = AbrirConsole("CONIN$", 1);
    saida = AbrirConsole("CONOUT$", 1);
    if (tela == INVALID_HANDLE_VALUE || entrada == INVALID_HANDLE_VALUE || saida == INVALID_HANDLE_VALUE)
        return 0;
    t->codigo = RodarFilho(exe, entrada, saida, saida);
    for (y = 0; y < LINHAS_LIDAS; y++)
        LerLinha(tela, y, t->linha[y]);
    t->pagina_depois = GetConsoleOutputCP();
    CloseHandle(tela);
    CloseHandle(entrada);
    CloseHandle(saida);
    return 1;
}

static int CasoTela(const char *exe)
{
    Tela t;
    int i;
    int linhas_ok = 1;

    memset(&t, 0, sizeof t);
    if (!RodarNoConsole(exe, &t)) {
        printf("DIAGNOSTICO: sem console utilizavel (AllocConsole/CONOUT$ falhou, erro %lu)\n", (unsigned long)GetLastError());
        return 0;
    }
    for (i = 0; i < LINHAS_LIDAS; i++) {
        if (wcscmp(t.linha[i], LinhasEsperadas[i]) != 0)
            linhas_ok = 0;
        MostrarLinha(i, t.linha[i]);
    }
    Confere(t.codigo == 0, "console 850, ola nativo: exit 0");
    Confere(linhas_ok, "console 850, ola nativo: tela == \"Ol\\u00e1, Forja!\", \"42\", \"21\"");
    Confere(t.pagina_depois == PAGINA_OEM, "console 850, ola nativo: GetConsoleOutputCP() voltou a 850 depois do exit");
    return 1;
}

/* stdout e stderr no mesmo pipe: sem console, os bytes tem de ser os UTF-8 de sempre e a pagina nao pode mudar. */
static int CasoPipe(const char *exe)
{
    HANDLE leitura, escrita, entrada;
    SECURITY_ATTRIBUTES sa;
    char dados[BYTES_PIPE];
    DWORD total = 0, lidos = 0, codigo;

    if (!NovoConsole())
        return 0;
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&leitura, &escrita, &sa, 0))
        return 0;
    SetHandleInformation(leitura, HANDLE_FLAG_INHERIT, 0);
    entrada = AbrirConsole("CONIN$", 1);
    codigo = RodarFilho(exe, entrada, escrita, escrita);
    CloseHandle(escrita);
    while (total < sizeof dados && ReadFile(leitura, dados + total, (DWORD)(sizeof dados - total), &lidos, NULL) && lidos > 0)
        total += lidos;
    CloseHandle(leitura);
    CloseHandle(entrada);
    Confere(codigo == 0, "pipe, ola nativo: exit 0");
    Confere(total == sizeof BytesEsperados - 1 && memcmp(dados, BytesEsperados, total) == 0,
            "pipe, ola nativo: bytes brutos UTF-8 (0xC3 0xA1) com CRLF, iguais ao BytesEsperados");
    Confere(GetConsoleOutputCP() == PAGINA_OEM, "pipe, ola nativo: pagina de codigo do console intocada (850)");
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "uso: conferir_console <ola_x64.exe>\n");
        return 2;
    }
    if (!CasoTela(argv[1]) || !CasoPipe(argv[1]))
        return 2;
    printf("conferir_console: checagens=%d ok=%d falhas=%d\n", Checagens, Checagens - Falhas, Falhas);
    return Falhas == 0 ? 0 : 1;
}
