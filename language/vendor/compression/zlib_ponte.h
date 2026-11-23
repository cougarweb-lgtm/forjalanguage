/* zlib_ponte.h 0.1.0 - cola MINIMA de FLUXO entre a Forja e a zlib 1.4 (licenca zlib).
 *
 * Por que existe: deflate/inflate recebem um z_stream com ponteiros de funcao (zalloc/zfree), recusado pelo `forja ligar` (G0003), e
 * deflateInit/inflateInit sao macros. Estas 4 funcoes com prefixo zlibf_ escondem o z_stream no lado C e deixam a Forja comprimir e
 * descomprimir em PEDACOS de qualquer tamanho (PDF, xlsx, arquivo grande), sem buffer unico do tamanho do arquivo.
 * Gere a ponte com:  forja ligar zlib_ponte.h --link z -o zlib_ponte.fj   (depois acrescente a posse: creates/destroys)
 * A implementacao esta em zlib_ponte.c; entra na mesma libz.a (Windows: ar r; Linux: construir_vendor_linux.py).
 * Nunca passe FILE* nem tipo da CRT por aqui (CRT diferente entre gcc/ucrt e Forja/msvcrt). */

#ifndef ZLIB_PONTE_H
#define ZLIB_PONTE_H

#include <stddef.h>

typedef struct zlibf_fluxo ZlibfFluxo;

/* formato do cabecalho do fluxo */
#define ZLIBF_FORMATO_ZLIB 0 /* cabecalho zlib + adler32 (o do PDF FlateDecode e do compress2) */
#define ZLIBF_FORMATO_RAW 1  /* deflate cru, sem cabecalho (o do .zip e do .xlsx) */

/* resultado de zlibf_processar */
#define ZLIBF_OK 0           /* progrediu; chame de novo com mais entrada e/ou mais espaco de saida */
#define ZLIBF_FIM 1          /* o fluxo terminou (deflate: tudo gravado; inflate: marca de fim lida) */
#define ZLIBF_ERRO_DADOS -1  /* inflate: dado corrompido ou dicionario exigido */
#define ZLIBF_ERRO_MEMORIA -2
#define ZLIBF_ERRO_PARAMETRO -3

/* Novo compressor. nivel 0..9 (6 = padrao). Devolve NULL se faltar memoria ou o parametro for invalido. */
ZlibfFluxo *zlibf_deflate_novo(int nivel, int formato);

/* Novo descompressor. Devolve NULL se faltar memoria ou o formato for invalido. */
ZlibfFluxo *zlibf_inflate_novo(int formato);

/* Empurra um pedaco: le ate n_entrada bytes de `entrada` e grava ate cap_saida bytes em `saida`.
 * `terminar` != 0 (so deflate): nao ha mais entrada, feche o fluxo (chame ate vir ZLIBF_FIM).
 * *consumidos e *gravados dizem quanto andou; o chamador reenvia o que sobrou da entrada. Sem progresso nenhum
 * (saida cheia) volta ZLIBF_OK com *gravados == 0: abra mais espaco. Depois de ZLIBF_FIM os bytes que sobraram
 * da entrada nao foram lidos (*consumidos mostra onde o fluxo acabou). */
int zlibf_processar(ZlibfFluxo *fluxo, const unsigned char *entrada, size_t n_entrada,
                    unsigned char *saida, size_t cap_saida, int terminar,
                    size_t *consumidos, size_t *gravados);

/* Libera o fluxo (NULL e aceito). */
void zlibf_fechar(ZlibfFluxo *fluxo);

#endif
