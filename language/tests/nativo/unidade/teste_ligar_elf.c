/* teste_ligar_elf.c - verificacoes de unidade do ligador ELF (M5b P36), direto no LigarComTabelas, sem executar: .init_array com dois construtores (ADDR64), __init_array_start/end, __fini_array_start/end (ADDR64 do programa).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P36: 6 verificacoes (ponteiros do .init_array = VA dos construtores; tamanho multiplo de 8; __init_array_start/end = limites da secao; __fini_array_start == __fini_array_end).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/fmt/pe.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/objeto.h"

#define LE_TAM_FUNCAO 17u

static void LeConfere(int *total, int *falhas, bool ok, const char *texto, int linha)
{
    (*total)++;
    if (!ok) {
        (*falhas)++;
        fprintf(stderr, "FALHOU teste_ligar_elf.c:%d: %s\n", linha, texto);
    }
}

#define LE_VERIFICA(cond) LeConfere(total, falhas, (cond), #cond, __LINE__)

static uint64_t LeOito(const uint8_t *p)
{
    uint64_t v = 0;
    for (int k = 7; k >= 0; k--)
        v = (v << 8) | p[k];
    return v;
}

static uint32_t LeSecaoComBytes(Objeto *o, const char *nome, ObjTipoSecao tipo, size_t tam)
{
    static const char zeros[64] = {0};
    uint32_t s = ObjSecaoAbrir(o, nome, tipo, 8);
    BufAnexarN(&((ObjSecao *)VecEm(&o->secoes, s))->bytes, zeros, tam);
    return s;
}

static void LeFuncao(Objeto *o, const char *secao, const char *simbolo)
{
    uint32_t s = LeSecaoComBytes(o, secao, OS_TEXTO, LE_TAM_FUNCAO);
    ObjSimboloDefinir(o, simbolo, (int32_t)s, 0);
}

static void LeAdd64(Objeto *o, uint32_t secao, uint32_t off, const char *alvo)
{
    ObjRelocar(o, (ObjAlvoReloc){secao, off}, (ObjCorrecao){OR_ADDR64, ObjSimboloReferir(o, alvo), 0});
}

/* programa: forja_programa e um .data com 4 ADDR64 (inicio/fim do .init_array e do .fini_array). runtime: _start, os dois construtores e a secao .init_array (16 bytes = 2 ponteiros). */
static void MontarInitArray(Objeto *programa, Objeto *runtime)
{
    uint32_t dados = LeSecaoComBytes(programa, ".data", OS_DATA, 32);
    uint32_t init = LeSecaoComBytes(runtime, ".init_array", OS_INIT_ARRAY, 16);
    LeFuncao(programa, ".text", "forja_programa");
    LeFuncao(runtime, ".text", "_start");
    LeFuncao(runtime, ".text$a", "ctor_a");
    LeFuncao(runtime, ".text$b", "ctor_b");
    LeAdd64(runtime, init, 0, "ctor_a");
    LeAdd64(runtime, init, 8, "ctor_b");
    LeAdd64(programa, dados, 0, "__init_array_start");
    LeAdd64(programa, dados, 8, "__init_array_end");
    LeAdd64(programa, dados, 16, "__fini_array_start");
    LeAdd64(programa, dados, 24, "__fini_array_end");
}

static const PeSecao *LeSecaoDaImagem(const Imagem *im, const char *nome)
{
    for (uint32_t k = 0; k < im->qtd_secoes; k++) {
        if (strncmp(im->secoes[k].nome, nome, sizeof im->secoes[k].nome) == 0)
            return &im->secoes[k];
    }
    return NULL;
}

static const ImagemSimbolo *LeSimboloDaImagem(const Imagem *im, const char *nome)
{
    for (size_t k = 0; k < im->simbolos.n; k++) {
        const ImagemSimbolo *s = VecEm(&im->simbolos, k);
        if (strcmp(s->nome, nome) == 0)
            return s;
    }
    return NULL;
}

static void TestaInitArray(int *total, int *falhas)
{
    Objeto programa, runtime;
    Imagem im;
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    MontarInitArray(&programa, &runtime);
    LE_VERIFICA(LigarComTabelas(&(LigarPedido){.programa = &programa, .runtime = &runtime, .formato = IMAGEM_ELF}, &im, &erro));

    const PeSecao *init = LeSecaoDaImagem(&im, ".init_array");
    const PeSecao *dados = LeSecaoDaImagem(&im, ".data");
    const ImagemSimbolo *ca = LeSimboloDaImagem(&im, "ctor_a");
    const ImagemSimbolo *cb = LeSimboloDaImagem(&im, "ctor_b");
    LE_VERIFICA(init != NULL && dados != NULL && ca != NULL && cb != NULL);
    if (init != NULL && dados != NULL && ca != NULL && cb != NULL) {
        const uint8_t *p = (const uint8_t *)init->bytes.dados;
        const uint8_t *d = (const uint8_t *)dados->bytes.dados;
        /* o .init_array tem 2 ponteiros, cada um e a VA do construtor (base do ELF mais o RVA do simbolo) */
        LE_VERIFICA(init->bytes.tam == 16 && init->bytes.tam % 8 == 0);
        LE_VERIFICA(LeOito(p) == IMAGEM_BASE_ELF + ca->rva && LeOito(p + 8) == IMAGEM_BASE_ELF + cb->rva);
        /* __init_array_start/end sao os limites da secao; __fini_array_start == __fini_array_end (vazio) */
        LE_VERIFICA(LeOito(d) == IMAGEM_BASE_ELF + init->rva && LeOito(d + 8) == IMAGEM_BASE_ELF + init->rva + init->bytes.tam);
        LE_VERIFICA(LeOito(d + 16) == LeOito(d + 24));
    }
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
}

/* Entrada chamada pelo teste_nativo (RodarUnidade). */
void LigarElfVerificacoes(int *total, int *falhas)
{
    TestaInitArray(total, falhas);
}
