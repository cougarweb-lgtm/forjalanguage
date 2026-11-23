/* amostrador.c - amostrador de RIP por suspensao da thread principal (PLANO B do P24b do M4).
 * Uso so na COPIA instrumentada em %TEMP%\forja_perfil (nunca em compiler\): o construir.bat da copia linka este arquivo no forja.exe.
 * Com FORJA_AMOSTRAS=<arquivo> definido, uma thread acorda a cada ~1 ms, suspende a thread principal e le o RIP.
 * RIP fora da imagem (ntdll, ucrtbase...): varre a pilha a partir de RSP (4 KB, passos de 8) e grava o 1o endereco de retorno
 * valido dentro do .text do forja.exe (o byte antes e um call: E8 rel32, ou FF /2 com modrm). Sem achado, chamador = 0.
 * No atexit grava "base <hex> tam <hex> text <rva hex> <tam hex>" e, por amostra, "<RIP absoluto hex> <chamador RVA hex>".
 * Estado global: os callbacks (constructor, thread, atexit) nao recebem contexto; e ferramenta de medida da copia, nao do compiler.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M4 P24b: varredura de pilha para o chamador de RIP fora da imagem (call validado; formato com chamador).
 *            0.1.0 - M4 P24b: primeira versao (thread de 1 ms, SuspendThread + GetThreadContext, RIP gravado no atexit).
 * Verificacao: set FORJA_AMOSTRAS=%TEMP%\am.txt && forja build --modo exe examples\01_ola.fj -o %TEMP%\ola.exe --backend x64
 *              (o arquivo tem a linha "base ..." e pelo menos 1 par de RIP e chamador).
 */
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AMOSTRAS_MAX (1u << 21)
#define PRAZO_PARADA_MS 3000
#define PILHA_VARRIDA 4096

static HANDLE gThreadPrincipal;
static HANDLE gThreadAmostra;
static volatile LONG gParar;
static uint64_t *gRips;
static uint64_t *gChamadores;
static size_t gQuantidade;
static uint64_t gBase;
static uint64_t gTamanho;
static uint64_t gTextoRva;
static uint64_t gTextoTam;
static const char *gCaminho;

static int EstaNoTexto(uint64_t Endereco)
{
    return Endereco >= gBase + gTextoRva + 8 && Endereco < gBase + gTextoRva + gTextoTam;
}

static int EhFimDeCall(uint64_t Endereco)
{
    const uint8_t *Bytes = (const uint8_t *)(uintptr_t)Endereco;
    if (Bytes[-5] == 0xE8)
        return 1;
    if (Bytes[-2] == 0xFF && ((Bytes[-1] >> 3) & 7) == 2)
        return 1;
    if (Bytes[-3] == 0xFF && ((Bytes[-2] >> 3) & 7) == 2)
        return 1;
    return Bytes[-6] == 0xFF && ((Bytes[-5] >> 3) & 7) == 2;
}

static uint64_t AcharChamador(uint64_t Rsp)
{
    uint8_t pilha[PILHA_VARRIDA];
    SIZE_T lidos = 0;
    uint64_t candidato;
    size_t indice;
    ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)Rsp, pilha, sizeof pilha, &lidos);
    for (indice = 0; indice + sizeof candidato <= lidos; indice += sizeof candidato) {
        memcpy(&candidato, pilha + indice, sizeof candidato);
        if (EstaNoTexto(candidato) && EhFimDeCall(candidato))
            return candidato - gBase;
    }
    return 0;
}

static void RegistrarAmostra(const CONTEXT *Contexto)
{
    uint64_t rip = (uint64_t)Contexto->Rip;
    uint64_t chamador = 0;
    if (gQuantidade >= AMOSTRAS_MAX)
        return;
    if (rip < gBase || rip >= gBase + gTamanho)
        chamador = AcharChamador((uint64_t)Contexto->Rsp);
    gRips[gQuantidade] = rip;
    gChamadores[gQuantidade] = chamador;
    gQuantidade++;
}

static void AmostrarUmaVez(void)
{
    CONTEXT contexto;
    if (SuspendThread(gThreadPrincipal) == (DWORD)-1)
        return;
    contexto.ContextFlags = CONTEXT_CONTROL;
    if (GetThreadContext(gThreadPrincipal, &contexto))
        RegistrarAmostra(&contexto);
    ResumeThread(gThreadPrincipal);
}

static DWORD WINAPI LacoDeAmostras(LPVOID Parametro)
{
    (void)Parametro;
    while (InterlockedCompareExchange(&gParar, 0, 0) == 0) {
        Sleep(1);
        AmostrarUmaVez();
    }
    return 0;
}

static void GravarRips(void)
{
    FILE *arquivo = fopen(gCaminho, "w");
    if (arquivo == NULL)
        return;
    fprintf(arquivo, "base %llx tam %llx text %llx %llx\n", (unsigned long long)gBase, (unsigned long long)gTamanho,
            (unsigned long long)gTextoRva, (unsigned long long)gTextoTam);
    for (size_t i = 0; i < gQuantidade; i++)
        fprintf(arquivo, "%llx %llx\n", (unsigned long long)gRips[i], (unsigned long long)gChamadores[i]);
    fclose(arquivo);
}

static void Descarregar(void)
{
    InterlockedExchange(&gParar, 1);
    WaitForSingleObject(gThreadAmostra, PRAZO_PARADA_MS);
    GravarRips();
    timeEndPeriod(1);
}

static void LerCabecalhoDaImagem(void)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)(uintptr_t)gBase;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(uintptr_t)(gBase + (uint64_t)dos->e_lfanew);
    const IMAGE_SECTION_HEADER *secao = IMAGE_FIRST_SECTION(nt);
    gTamanho = nt->OptionalHeader.SizeOfImage;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(secao[i].Name, ".text", 6) == 0) {
            gTextoRva = secao[i].VirtualAddress;
            gTextoTam = secao[i].Misc.VirtualSize;
            return;
        }
    }
}

static int PrepararAmostragem(void)
{
    gCaminho = getenv("FORJA_AMOSTRAS");
    if (gCaminho == NULL)
        return 0;
    gRips = malloc(AMOSTRAS_MAX * sizeof(uint64_t));
    gChamadores = malloc(AMOSTRAS_MAX * sizeof(uint64_t));
    if (gRips == NULL || gChamadores == NULL)
        return 0;
    gBase = (uint64_t)(uintptr_t)GetModuleHandleW(NULL);
    LerCabecalhoDaImagem();
    return DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &gThreadPrincipal,
                           THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, 0);
}

__attribute__((constructor)) static void IniciarAmostragem(void)
{
    if (!PrepararAmostragem())
        return;
    timeBeginPeriod(1);
    gThreadAmostra = CreateThread(NULL, 0, LacoDeAmostras, NULL, 0, NULL);
    if (gThreadAmostra == NULL)
        return;
    atexit(Descarregar);
}
