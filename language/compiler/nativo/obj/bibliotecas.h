/* bibliotecas.h - resolve os 'link' do foreign em tabelas de imports por DLL: acha cada biblioteca (biblioteca.h), le as exportacoes (pe_ler.h) e entrega, por DLL,
 * so os simbolos que o programa usa (P19 do M3).
 * Versao: 0.6.0
 * Changelog: 0.6.0 - M6d P92c: BibliotecasResolvidas.estaticas e .libmem.
 *            0.5.0 - M3b P23f: BibliotecasPedido.provedores (BibliotecasProvedor) e BibliotecasResolvidas.provedores_usados: o runtime, o Ryu e o legado satisfazem o que um membro de estatica pede sem DLL.
 *            0.4.0 - M3b P23e: biblioteca estatica (.a/.lib de C) resolve: BibliotecasResolvidas.selecao (os membros que EstaticaSelecionar puxou, com o .drectve /DEFAULTLIB e -l virando 'link' implicito;
 *                    X0108 se faltar), celulas (simbolos do programa que uma estatica define) e simbolo_faltando_nome. As tabelas de DLL passam a cobrir tambem o que o codigo de terceiros pede (os pendentes da selecao).
 *            0.3.0 - M3 P22-O1a: BibliotecasResolvidas.lidas (BibliotecaLida: caminho e Hash64 de cada arquivo lido, para o --entradas).
 *            0.2.0 - M3 P19b: so comentario (import library e .exe tambem resolvem; X0105 tambem para dado/ordinal de import library e .exe sem export).
 *            0.1.0 - M3 P19: BibliotecasResolver (a 1a biblioteca do 'link' que exporta o simbolo vence, como o ld; X0101, X0102, X0103 e X0105).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_BIBLIOTECAS_H
#define FORJA_NATIVO_OBJ_BIBLIOTECAS_H

#include "common/arena.h"
#include "common/buf.h"
#include "common/diag.h"
#include "common/vec.h"
#include "nativo/fmt/pe.h"
#include "nativo/obj/biblioteca.h"
#include "nativo/obj/estatica.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/libmem.h"

/* Um objeto que a ligacao ja traz (runtime, Ryu) ou pode trazer (o legado: stubs da toolchain e sprintf/sscanf): o que ele define como global satisfaz o simbolo que um MEMBRO
 * de estatica pede (nao o que o programa pede), sem ir buscar em DLL. traz_imports: se satisfizer algum, os __imp_ indefinidos dele entram na lista de simbolos de DLL. */
typedef struct {
    const Objeto *objeto;
    bool traz_imports;
} BibliotecasProvedor;

/* O que resolver: as pastas de busca, os 'link' na ordem do fonte e os simbolos C usados pelo programa (sem "__imp_"), na ordem da 1a referencia; provedores (opcional, ate 32). */
typedef struct {
    const BibliotecaBusca *busca;
    const char *const *links;
    uint32_t qtd_links;
    const char *const *simbolos;
    uint32_t qtd_simbolos;
    const BibliotecasProvedor *provedores;
    uint32_t qtd_provedores;
} BibliotecasPedido;

/* Um arquivo de biblioteca que o resolvedor leu por inteiro: caminho como a busca o achou e Hash64 dos bytes lidos (sem reler o arquivo). */
typedef struct {
    const char *caminho;
    uint64_t hash;
} BibliotecaLida;

/* Sucesso: tabelas[0..qtd_tabelas) na ordem dos 'link' (so as DLLs que fornecem algum simbolo), prontas para LigarPedido; vivem ate BibliotecasLiberar.
 * Falha: o Diag diz o codigo e a causa; link_falho (X0102, X0103, X0105) ou simbolo_faltando (X0101, indice em BibliotecasPedido.simbolos; -1 nos demais)
 * dizem quem falhou (X0101 com o simbolo pedido por membro de estatica: simbolo_faltando = -1 e simbolo_faltando_nome; X0106/X0105 da selecao e X0108: link_falho = a biblioteca),
 * e procurei_em lista, um por linha, os arquivos tentados (X0102) ou lidos (X0101), na ordem da busca. */
typedef struct {
    Arena memoria;
    ImportTabela tabelas[IMAGEM_MAX_DLLS];
    uint32_t qtd_tabelas;
    const char *link_falho;
    int32_t simbolo_faltando;
    Buf procurei_em;
    Vec lidas;  /* BibliotecaLida, na ordem da leitura: cada DLL, exe, import library ou estatica lida por inteiro (o --entradas); o caminho vive na arena */
    bool tem_estaticas;           /* algum 'link' (ou /DEFAULTLIB de membro) era estatica: selecao e celulas valem */
    EstaticaSelecao selecao;      /* os membros puxados (EstaticaMembro.dados vive na arena de memoria) */
    Vec celulas;                  /* const char *: simbolos do programa que uma estatica define (ganham o slot __imp_ no objeto de terceiros); nomes na arena */
    const char *simbolo_faltando_nome;  /* X0101: o nome do simbolo (o programa ou um membro de estatica o pediu); na arena */
    Vec estaticas;                /* M6d P92c: EstaticaArquivo na ordem do resolvedor; o indice e o de EstaticaMembro.arquivo (o link de cada um em EstaticaArquivo.link) */
    TerceirosLibmem libmem;       /* M6d P92c: o mapa slot -> lib, montado por LibmemMontar (NativoBibliotecas); zerado = nada a contar */
    uint32_t provedores_usados;   /* bit k ligado: o provedor k do pedido satisfez algum simbolo pedido por membro de estatica (entra na ligacao) */
} BibliotecasResolvidas;

/* Chame BibliotecasLiberar depois, ate quando devolver false. Os textos do pedido so precisam viver durante a chamada. */
bool BibliotecasResolver(const BibliotecasPedido *pedido, BibliotecasResolvidas *saida, Diag *erro);

void BibliotecasLiberar(BibliotecasResolvidas *saida);

#endif
