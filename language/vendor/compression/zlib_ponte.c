/* zlib_ponte.c - implementacao da cola de fluxo zlibf_* (ver zlib_ponte.h). Compilar com:
 *   gcc -O2 -Wall -I zlib -c zlib_ponte.c -o zlib_ponte.o      e depois   ar r ..\libz.a zlib_ponte.o
 * No Linux entra por tests\nativo\linux\vendor_a\construir_vendor_linux.py (lista de arquivos da zlib).
 * Versao 0.1.0 (20/11/2026). Verificar: forja run language\exemplos_libs\compressao_fluxo.fj */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"
#include "zlib_ponte.h"

#define ZLIBF_JANELA 15
#define ZLIBF_MEM_NIVEL 8

struct zlibf_fluxo {
    z_stream z;
    int comprime; /* 1 = deflate, 0 = inflate */
};

static int JanelaDoFormato(int formato) {
    if (formato == ZLIBF_FORMATO_ZLIB) {
        return ZLIBF_JANELA;
    }
    if (formato == ZLIBF_FORMATO_RAW) {
        return -ZLIBF_JANELA;
    }
    return 0;
}

static ZlibfFluxo *NovoFluxo(int comprime) {
    ZlibfFluxo *f = (ZlibfFluxo *)calloc(1, sizeof(ZlibfFluxo));
    if (f != NULL) {
        f->comprime = comprime;
    }
    return f;
}

ZlibfFluxo *zlibf_deflate_novo(int nivel, int formato) {
    int janela = JanelaDoFormato(formato);
    ZlibfFluxo *f;
    if (janela == 0 || nivel < 0 || nivel > 9) {
        return NULL;
    }
    f = NovoFluxo(1);
    if (f == NULL) {
        return NULL;
    }
    if (deflateInit2(&f->z, nivel, Z_DEFLATED, janela, ZLIBF_MEM_NIVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(f);
        return NULL;
    }
    return f;
}

ZlibfFluxo *zlibf_inflate_novo(int formato) {
    int janela = JanelaDoFormato(formato);
    ZlibfFluxo *f;
    if (janela == 0) {
        return NULL;
    }
    f = NovoFluxo(0);
    if (f == NULL) {
        return NULL;
    }
    if (inflateInit2(&f->z, janela) != Z_OK) {
        free(f);
        return NULL;
    }
    return f;
}

/* avail_in/avail_out do z_stream sao de 32 bits: pedacos maiores andam em partes (o chamador reenvia o resto). */
static unsigned int Limitar(size_t n) {
    return n > (size_t)UINT_MAX ? UINT_MAX : (unsigned int)n;
}

static int TraduzirRetorno(int codigo) {
    if (codigo == Z_STREAM_END) {
        return ZLIBF_FIM;
    }
    if (codigo == Z_OK || codigo == Z_BUF_ERROR) {
        return ZLIBF_OK; /* Z_BUF_ERROR = sem progresso possivel agora, nao e falha */
    }
    if (codigo == Z_MEM_ERROR) {
        return ZLIBF_ERRO_MEMORIA;
    }
    return ZLIBF_ERRO_DADOS;
}

int zlibf_processar(ZlibfFluxo *fluxo, const unsigned char *entrada, size_t n_entrada,
                    unsigned char *saida, size_t cap_saida, int terminar,
                    size_t *consumidos, size_t *gravados) {
    unsigned int entra;
    unsigned int sai;
    int codigo;
    if (fluxo == NULL || consumidos == NULL || gravados == NULL || (entrada == NULL && n_entrada > 0) ||
        (saida == NULL && cap_saida > 0)) {
        return ZLIBF_ERRO_PARAMETRO;
    }
    entra = Limitar(n_entrada);
    sai = Limitar(cap_saida);
    fluxo->z.next_in = (Bytef *)entrada;
    fluxo->z.avail_in = entra;
    fluxo->z.next_out = saida;
    fluxo->z.avail_out = sai;
    if (fluxo->comprime) {
        /* Z_FINISH so quando toda a entrada ja cabe nesta chamada; senao o resto entra na proxima */
        codigo = deflate(&fluxo->z, (terminar != 0 && entra == n_entrada) ? Z_FINISH : Z_NO_FLUSH);
    } else {
        codigo = inflate(&fluxo->z, Z_NO_FLUSH);
    }
    *consumidos = (size_t)(entra - fluxo->z.avail_in);
    *gravados = (size_t)(sai - fluxo->z.avail_out);
    return TraduzirRetorno(codigo);
}

void zlibf_fechar(ZlibfFluxo *fluxo) {
    if (fluxo == NULL) {
        return;
    }
    if (fluxo->comprime) {
        deflateEnd(&fluxo->z);
    } else {
        inflateEnd(&fluxo->z);
    }
    free(fluxo);
}
