/* mapa_interno.h - o que cmd_mapa.c (JSON) e mapa_resumo.c (texto) dividem no 'forja mapa'.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I3: primeira versao.
 * Verificacao: construir.bat && python tests\consultar\verificar_mapa.py --forja <forja.exe>
 */
#ifndef FORJA_MAPA_INTERNO_H
#define FORJA_MAPA_INTERNO_H

#include "driver/consultar_interno.h"

/* O tamanho do projeto: os modulos que nao sao o std embutido (o std entra so em 'externos'). */
typedef struct {
    uint32_t modulos, externos, simbolos, publicos, tipos, testes;
    uint64_t linhas, bytes;
} MapaTamanho;

void MapaMedir(const Consulta *q, MapaTamanho *t);

/* struct, enum ou union. */
bool MapaEhTipo(const Decl *d);

/* O modulo entra no mapa? 'so' = modulo pedido em --modulo (-1 = nenhum): ele mesmo; sem pedido, todo modulo que nao e o std embutido. */
bool MapaNoEscopo(const Consulta *q, int modulo, int so);

/* Resposta de 'forja mapa --resumo' (texto hierarquico, em q->saida). */
int MapaResumo(Consulta *q, const PedidoConsulta *p, int so);

#endif
