/* estatica.h - escolher os membros de bibliotecas estaticas (.a GNU e .lib MS) que o programa precisa (P23d do M3b).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6d P92c: EstaticaArquivo.link (o nome do 'link' de cada arquivo; BibsLerEstatica o grava).
 *            0.1.0 - M3b P23d: EstaticaSelecionar resolve ate PONTO FIXO sobre TODAS as estaticas do programa (superconjunto do ld, que varre cada .a uma vez na ordem: Q-M3-6).
 *                    Simbolo definido em duas estaticas = X0106. So seleciona: ligar o resultado no PE e do P23e/P23f.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_estatica.py
 */
#ifndef FORJA_NATIVO_OBJ_ESTATICA_H
#define FORJA_NATIVO_OBJ_ESTATICA_H

#include <stddef.h>
#include <stdint.h>

#include "common/arena.h"
#include "common/diag.h"
#include "common/vec.h"

/* Uma biblioteca estatica ja lida. nome so serve para as mensagens (o caminho). A ordem do vetor e a dos 'link' do programa. Dois itens com o mesmo ponteiro bytes sao o
 * mesmo arquivo (um 'link' repetido): o segundo e ignorado, nao conta como definicao em duas bibliotecas. */
typedef struct {
    const char *nome;
    const uint8_t *bytes;
    size_t n;
    const char *link;  /* o 'link' que trouxe o arquivo, como o .fj escreveu (ou o nome do .drectve; "c" da semente); NULL fora do resolvedor. M6d P92c: o mapa slot -> lib do ligador */
} EstaticaArquivo;

/* arquivos (qtd_arquivos itens) e raizes: os simbolos que o programa exige (os foreign sem DLL, e o que o resto do programa referir). Nada e copiado: tudo tem de viver
 * ate EstaticaLiberar. */
typedef struct {
    const EstaticaArquivo *arquivos;
    uint32_t qtd_arquivos;
    const char *const *raizes;
    uint32_t qtd_raizes;
} EstaticaPedido;

/* Um membro puxado. nome termina em NUL e mora na memoria da selecao; dados aponta para dentro dos bytes da biblioteca (o COFF do membro). */
typedef struct {
    uint32_t arquivo;  /* indice em EstaticaPedido.arquivos */
    const char *nome;
    const uint8_t *dados;
    size_t tam;
} EstaticaMembro;

/* membros: EstaticaMembro na ordem em que foram puxados (raizes na ordem dada, depois o que cada membro puxado referir, em largura). pendentes: const char * dos simbolos
 * (raizes ou referidos por membro) que NENHUMA estatica define, sem repeticao, na ordem em que apareceram: o chamador decide (DLL, CRT, stub, X0101). */
typedef struct {
    Arena memoria;
    Vec membros;
    Vec pendentes;
} EstaticaSelecao;

/* Iniciada aqui; libere com EstaticaLiberar mesmo quando devolve false. Falso com X0105 (ar sem indice, ar ou COFF malformado, membro que nao e COFF AMD64) ou X0106
 * (simbolo exigido definido em duas estaticas: a causa lista as duas, na ordem dos 'link'). Entre duas definicoes na MESMA estatica vale a primeira do indice. */
bool EstaticaSelecionar(const EstaticaPedido *pedido, EstaticaSelecao *sel, Diag *erro);

void EstaticaLiberar(EstaticaSelecao *sel);

#endif
