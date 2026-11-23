/* coff_interno.h - pecas internas do leitor COFF AMD64 (coff_ler.c) que o leitor de membros de terceiros (coff_membro.c) reaproveita; nao e API do ligador.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3b P23e: nasce da separacao de coff_ler.c (constantes, CoffLeitor, cabecalho/strings/nomes/tipo de secao e a tabela de relocacoes saem do .c).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_COFF_INTERNO_H
#define FORJA_NATIVO_OBJ_COFF_INTERNO_H

#include <stddef.h>
#include <stdint.h>

#include "common/diag.h"
#include "nativo/obj/objeto.h"

#define COFF_MAQUINA_AMD64 0x8664u
#define COFF_TAM_CABECALHO 20u
#define COFF_OFF_CAB_SECOES 2u
#define COFF_OFF_CAB_TAM_OPC 16u
#define COFF_OFF_CAB_SIMBOLOS 8u
#define COFF_OFF_CAB_NSIMBOLOS 12u
#define COFF_TAM_SECAO 40u
#define COFF_OFF_SEC_VSZ 8u
#define COFF_OFF_SEC_RAW 16u
#define COFF_OFF_SEC_PTR 20u
#define COFF_OFF_SEC_PTR_RELOC 24u
#define COFF_OFF_SEC_QTD_RELOC 32u
#define COFF_OFF_SEC_CARAC 36u
#define COFF_TAM_SIMBOLO 18u
#define COFF_OFF_SIM_VALOR 8u
#define COFF_OFF_SIM_SECAO 12u
#define COFF_OFF_SIM_TIPO 14u
#define COFF_OFF_SIM_AUX 17u
#define COFF_OFF_SIM_CLASSE 16u
#define COFF_CLASSE_EXTERNA 2u
#define COFF_CLASSE_ESTATICA 3u
#define COFF_CLASSE_FRACA 105u
#define COFF_TAM_RELOC 10u
#define COFF_TAM_NOME 8u
#define COFF_TAM_CAMPO 4u
#define COFF_SEC_NENHUMA 0
#define COFF_SIMBOLO_NAO_MAPEADO UINT32_MAX
#define COFF_CARAC_CODIGO 0x00000020u
#define COFF_CARAC_BSS 0x00000080u
#define COFF_CARAC_GRAVAVEL 0x80000000u
#define COFF_CARAC_ALINHA_MASCARA 0x00F00000u
#define COFF_CARAC_ALINHA_SHIFT 20u
#define COFF_ALINHA_PADRAO 16u
#define COFF_BITS_BYTE 8u

/* Relocacoes aceitas (AMD64): o codigo e o do COFF; tam e o tamanho do campo (de onde sai o adendo); delta entra no adendo (REL32_n: o campo mede a distancia
 * ate o fim da instrucao, n bytes depois do campo, entao o adendo cai n). */
typedef struct {
    uint16_t codigo;
    ObjTipoReloc tipo;
    uint32_t tam;
    int32_t delta;
} CoffRelocSuportada;

/* Estado da leitura: o arquivo inteiro na memoria e as posicoes ja conferidas no cabecalho. */
typedef struct {
    const uint8_t *b;
    size_t n;
    Objeto *o;
    Diag *erro;
    uint32_t nsec;
    uint32_t nsim;
    size_t ini_simbolos;
    size_t ini_secoes;
    size_t ini_strtab;
    uint32_t tam_strtab;
    Vec mapa;  /* uint32_t por simbolo COFF (inclui os auxiliares): indice no Objeto ou COFF_SIMBOLO_NAO_MAPEADO */
} CoffLeitor;

bool CoffCabe(size_t n, size_t off, size_t tam);
uint64_t CoffLe(const uint8_t *p, uint32_t n);
bool CoffFalha(Diag *erro, const char *motivo);
const char *CoffNomeSecao(const CoffLeitor *L, const uint8_t *campo, char buf[COFF_TAM_NOME + 1]);
const char *CoffNomeSimbolo(const CoffLeitor *L, const uint8_t *campo, char buf[COFF_TAM_NOME + 1]);
uint32_t CoffAlinhamento(uint32_t carac);
ObjTipoSecao CoffTipo(const char *nome, uint32_t carac);
const uint8_t *CoffCabecalhoSecao(const CoffLeitor *L, uint32_t i);
bool CoffPreencherSecao(CoffLeitor *L, uint32_t idx, const uint8_t *h);
bool CoffLerCabecalho(CoffLeitor *L);
void CoffLerTabelaStrings(CoffLeitor *L);

/* Procura o tipo de relocacao pelo codigo do COFF; false se nao esta na tabela (IMAGE_REL_AMD64_ABSOLUTE, 0, nao esta: quem le o ignora antes). */
bool CoffTipoReloc(uint16_t codigo, CoffRelocSuportada *achado);

/* O adendo que ja esta no campo (com sinal para 4 bytes). */
int64_t CoffAdendo(const uint8_t *p, uint32_t tam);

#endif
