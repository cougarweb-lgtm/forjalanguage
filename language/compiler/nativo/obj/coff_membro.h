/* coff_membro.h - leitor de membros COFF de terceiros (.a de gcc/mingw e .lib de MSVC) para UM Objeto so: a "uniao" dos membros que a selecao de estaticas puxou (P23d).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M3b P23y-2: CoffUniao.associadas (CoffAssociada) para anular as associadas do lider que perde no LARGEST.
 *            0.1.0 - M3b P23e: CoffUniaoLerMembro (COMDAT ANY/SAME_SIZE/EXACT_MATCH/LARGEST/ASSOCIATIVE e NODUPLICATES = X0106; simbolo local com nome proprio de cada membro; WEAK_EXTERNAL;
 *                    simbolo comum; secao descartavel (.debug_*) e .drectve fora do Objeto; relocacao desconhecida = X0903 com o tipo e o membro) e CoffDiretivasLer / CoffDiretivasDoMembro (.drectve).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_estatica.py
 */
#ifndef FORJA_NATIVO_OBJ_COFF_MEMBRO_H
#define FORJA_NATIVO_OBJ_COFF_MEMBRO_H

#include "common/arena.h"
#include "common/diag.h"
#include "common/vec.h"
#include "nativo/obj/coff.h"
#include "nativo/obj/objeto.h"

/* Uma diretiva do .drectve que o ligador entende. IGNORADA guarda a 1a palavra que ele nao conhece (a unica que o aviso cita). */
typedef enum { DIRETIVA_BIBLIOTECA, DIRETIVA_EXPORTA, DIRETIVA_ALINHA_COMUM, DIRETIVA_IGNORADA } CoffTipoDiretiva;

typedef struct {
    CoffTipoDiretiva tipo;
    const char *nome;  /* na arena dada a CoffDiretivasLer */
    uint32_t valor;    /* ALINHA_COMUM: o log2 do alinhamento */
} CoffDiretiva;

/* Divide o texto do .drectve (n bytes, sem NUL final obrigatorio) em diretivas, anexadas em saida (Vec de CoffDiretiva, iniciado por quem chama). Reconhece, com '/' ou '-' na frente e sem
 * distinguir caixa: defaultlib:<nome> e -l<nome> (BIBLIOTECA), export:<nome>[,...] (EXPORTA), aligncomm:<nome>,<log2> (ALINHA_COMUM); o resto e IGNORADA. */
void CoffDiretivasLer(const char *texto, size_t n, Arena *arena, Vec *saida);

/* As diretivas BIBLIOTECA do .drectve de um membro COFF (b, n), em destino->lista (Vec de const char *, iniciado por quem chama; nomes na arena do destino). Membro sem .drectve: nada.
 * COFF torto ou de outra maquina: false com X0903 em erro. */
bool CoffBibliotecasDoMembro(const uint8_t *b, size_t n, const CoffDestinoNomes *destino, Diag *erro);

typedef struct { uint32_t simbolo; uint32_t secao; uint32_t tam; uint8_t selecao; } CoffComdat;
typedef struct { uint32_t pai; uint32_t filho; } CoffAssociada;  /* secoes do Objeto: filho (.pdata/.xdata) e COMDAT associativo de pai */
typedef struct { uint32_t simbolo; uint32_t tam; } CoffComum;
typedef struct { uint32_t simbolo; uint32_t padrao; } CoffFraco;  /* padrao: simbolo do Objeto ou COFF_SIMBOLO_NAO_MAPEADO (absoluto) */

/* A uniao dos membros lidos. Iniciada por CoffUniaoIniciar sobre um Objeto ja iniciado (ObjIniciar); quem a termina (terceiros.c) usa os vetores. */
typedef struct {
    Objeto *o;
    Vec comdats;       /* CoffComdat, um por lider ja visto */
    Vec comuns;        /* CoffComum */
    Vec fracos;        /* CoffFraco */
    Vec alinhamentos;  /* CoffDiretiva ALINHA_COMUM, na arena do Objeto */
    Vec bibliotecas;   /* const char *: /DEFAULTLIB e -l dos membros */
    Vec exportados;    /* const char *: /EXPORT e -export dos membros */
    Vec anuladas;      /* uint32_t: secoes do Objeto sem efeito (COMDAT perdedor e as associadas a ele) */
    Vec associadas;    /* CoffAssociada: as associativas vivas de membros ja lidos (somem se o lider delas perder para um LARGEST de membro posterior) */
    uint32_t membros;  /* quantos membros ja foram lidos (prefixo dos nomes locais) */
    uint32_t ignoradas;
    const char *primeira_ignorada;
} CoffUniao;

void CoffUniaoIniciar(CoffUniao *u, Objeto *o);
void CoffUniaoLiberar(CoffUniao *u);

/* Um membro a ler: os bytes do COFF e o nome (so para as mensagens). */
typedef struct {
    const uint8_t *dados;
    size_t tam;
    const char *nome;
} CoffMembroEntrada;

/* Le o COFF do membro para a uniao. false com X0903 (formato) ou X0106 (NODUPLICATES, simbolo forte repetido). */
bool CoffUniaoLerMembro(CoffUniao *u, const CoffMembroEntrada *membro, Diag *erro);

#endif
