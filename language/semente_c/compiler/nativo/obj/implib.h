/* implib.h - leitor de import library (MSVC curta e GNU .dll.a): reduz cada membro a "DLL + nome importado", sem ligar as secoes dele (P19b do M3).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19b: ImplibLer. Membro curto MSVC (IMPORT_OBJECT_HEADER, Sig1 0 / Sig2 0xFFFF; NameType NAME, NOPREFIX, UNDECORATE e EXPORTAS; DATA e ORDINAL
 *                    ficam marcados) e membro GNU (COFF com .idata$5 e .idata$6 = dica + nome; o "tail" com .idata$7 = nome da DLL). Maquina que nao e 0x8664 = X0105.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_IMPLIB_H
#define FORJA_NATIVO_OBJ_IMPLIB_H

#include <stddef.h>
#include <stdint.h>

#include "common/arena.h"
#include "common/diag.h"
#include "common/vec.h"

/* IMPLIB_CODIGO = funcao (o unico tipo que o backend liga); IMPLIB_DADO = variavel ou constante exportada (Type DATA/CONST do MSVC); IMPLIB_ORDINAL = import so por
 * ordinal (NameType ORDINAL: o backend importa por nome). DADO e ORDINAL entram na lista mesmo assim: quem pede o simbolo recebe X0105 e nao um X0101 enganoso. */
typedef enum { IMPLIB_CODIGO, IMPLIB_DADO, IMPLIB_ORDINAL } ImplibTipo;

/* dll = nome da DLL ("python313.dll"); simbolo = o nome que o programa cita (a 1a string do membro curto); nome = o que a tabela de exportacao da DLL tem (igual ao
 * simbolo em NAME; NOPREFIX tira o ? @ _ da frente; UNDECORATE tira tambem do primeiro @ em diante; EXPORTAS traz o nome pela 3a string). Os textos moram na arena. */
typedef struct {
    const char *dll;
    const char *nome;
    const char *simbolo;
    ImplibTipo tipo;
} ImplibEntrada;

typedef struct {
    Arena textos;
    Vec itens;  /* ImplibEntrada, ordenadas por nome (depois por dll) */
} ImplibEntradas;

/* Le os membros do ar. Membro sem forma de import (descritores e "null thunk" do MSVC, indices) e ignorado. Falso com X0105 em erro: ar malformado, membro de maquina
 * diferente de AMD64 (inclusive 32 bits) ou import library GNU sem o nome da DLL ou com mais de uma. saida fica pronta para ImplibLiberar mesmo se devolver falso. */
bool ImplibLer(const uint8_t *bytes, size_t n, ImplibEntradas *saida, Diag *erro);

void ImplibLiberar(ImplibEntradas *saida);

#endif
