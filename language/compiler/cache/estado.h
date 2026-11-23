/* estado.h - estado incremental do orquestrador de projeto (<pasta>\.forja_cache\estado\<dev|release>.tsv), chave de cache do produto e motivo de recompilar (P22-O1b do M3).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22-O1b: EstadoLer, EstadoGravarAtomico, EstadoAchar, EstadoChave (+ texto canonico, ambiente e deps) e EstadoMotivo.
 * Verificacao: construir.bat && teste_common.exe  (e python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe)
 */
#ifndef FORJA_CACHE_ESTADO_H
#define FORJA_CACHE_ESTADO_H

#include "cache/projeto_ler.h"
#include "common/buf.h"
#include "common/vec.h"

#define ESTADO_DEPS_MAX (PROJETO_DEPENDE_MAX * (PROJETO_NOME_MAX + 19))   /* "nome=<16 hex>," por dependencia */

/* Uma linha do estado. Arquivo texto UTF-8, LF, cabecalho "# forja-estado 1", uma linha por produto, 8 colunas separadas por TAB:
 *   nome | chave (16 hex) | hash_interface (16 hex, "-" no exe) | saida (absoluta) | tamanho | mtime (FILETIME) | ambiente (16 hex) | deps ("nome=<16 hex>,..." ou "-")
 * As 6 primeiras sao as do contrato (v2 5.2); ambiente (Hash64 de forja.exe, alvo, modo, nivel, tipo, entrada e lib-pasta) e deps (a hash_interface de cada dll de que
 * o produto depende, na hora do build) sao as 2 que deixam dizer POR QUE a chave mudou. */
typedef struct {
    char nome[PROJETO_NOME_MAX];
    uint64_t chave;
    bool tem_interface;
    uint64_t hash_interface;
    char saida[PROJETO_CAMINHO_MAX];
    uint64_t tamanho, mtime;
    uint64_t ambiente;
    char deps[ESTADO_DEPS_MAX];
} EstadoLinha;

/* Vec de EstadoLinha, na ordem do arquivo. */
typedef struct { Vec linhas; } Estado;

/* Le o estado. Arquivo ausente, cabecalho desconhecido ou linha torta = estado vazio (ou sem a linha torta): o pior caso e recompilar, nunca reusar errado.
 * false so se o arquivo existe e nao pode ser lido. O Estado e sempre iniciado (EstadoLiberar). */
bool EstadoLer(const char *caminho, Estado *e);

/* Grava o estado inteiro de forma atomica (ArquivoGravarAtomico). false = nao gravou; *errno_saida (NULL = nao quer) recebe o errno. */
bool EstadoGravarAtomico(const char *caminho, const Estado *e, int *errno_saida);

/* A linha do produto, ou NULL. */
const EstadoLinha *EstadoAchar(const Estado *e, const char *nome);

void EstadoLiberar(Estado *e);

/* A hash_interface de uma dependencia na hora de montar a chave. tem = false (exe, ou dll que nunca foi construida) entra como "-". */
typedef struct { const char *nome; bool tem; uint64_t hash; } EstadoDep;

/* Um arquivo que o build leu (linha do .entradas) com o Hash64 de hoje. */
typedef struct { const char *caminho; uint64_t hash; } EstadoArquivo;

/* Tudo o que entra na chave de um produto. deps em ordem de nome, arquivos em ordem de caminho (quem chama ordena). */
typedef struct {
    uint64_t forja;                 /* Hash64 dos bytes do proprio forja.exe */
    const char *alvo, *modo, *tipo, *entrada, *lib_pasta;     /* "x86_64-windows", "dev"|"release", "exe"|"dll", caminhos absolutos */
    uint32_t nivel;                 /* 0 por enquanto (otimizar_dev do 0.22.0) */
    const EstadoDep *deps;
    size_t ndeps;
    const EstadoArquivo *arquivos;
    size_t narquivos;
} ChavePedido;

/* Texto canonico da chave, uma linha por item, LF:
 *   forja=<16 hex> / alvo=<alvo> / modo=<modo> / nivel=<n> / tipo=<tipo> / entrada=<caminho> / lib-pasta=<caminho>
 *   dep:<nome>=<16 hex ou -> (uma por dependencia)  e  <caminho>=<16 hex> (uma por arquivo). */
void EstadoTextoCanonico(const ChavePedido *p, Buf *texto);

/* Hash64 do texto canonico inteiro: a chave do produto. */
uint64_t EstadoChave(const ChavePedido *p);

/* Hash64 so das 7 primeiras linhas do texto canonico (forja, alvo, modo, nivel, tipo, entrada, lib-pasta). */
uint64_t EstadoAmbiente(const ChavePedido *p);

/* "nome=<16 hex>,nome=<16 hex>" (o "-" no lugar do hash de quem nao tem) ou "-" sem dependencias: a coluna deps. Cabe em ESTADO_DEPS_MAX. */
void EstadoDepsTexto(const ChavePedido *p, char *destino, size_t tam);

/* Os fatos que decidem se o produto recompila. */
typedef struct {
    bool forcado;               /* --tudo */
    bool primeira_vez;          /* sem linha no estado ou sem .entradas */
    bool chave_igual;           /* a chave de hoje e a gravada */
    bool ambiente_igual;        /* o ambiente de hoje e o gravado */
    bool saida_ok;              /* saida (e .h da dll) existe como foi gravada */
    const char *dep_mudou;      /* nome da 1a dll cuja hash_interface difere da gravada; "" = nenhuma */
    const char *arquivo_mudou;  /* 1a entrada (em ordem de caminho) que mudou ou sumiu, relativa a pasta do projeto quando cabe; "" = nenhuma */
    bool arquivo_e_lib;         /* arquivo_mudou e uma "lib" do .entradas */
} MotivoPedido;

/* true = recompila e 'texto' recebe o motivo (1 dos 6 fixos: "primeira vez" | "fonte mudou: <caminho>" | "biblioteca mudou: <caminho>" | "interface de <dll> mudou" |
 * "forja.exe, modo ou opcoes mudaram" | "saida ausente ou alterada" | "forcado (--tudo)"). false = reusa. Prioridade: forcado, primeira vez, chave igual (reusa ou saida
 * ausente), ambiente, interface, arquivo, e por fim "forja.exe, modo ou opcoes mudaram". */
bool EstadoMotivo(const MotivoPedido *p, char *texto, size_t n);

#endif
