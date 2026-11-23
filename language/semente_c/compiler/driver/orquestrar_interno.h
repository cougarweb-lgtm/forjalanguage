/* orquestrar_interno.h - o que orquestrar.c (plano e apuracao) divide com orquestrar_exec.c (execucao dos filhos): so para esses dois arquivos (P22-O2 do M3).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M3 F6: Plano ganha 'tempos' (medicao por etapa, FORJA_ORQ_TEMPOS; ver orquestrar_tempos.h).
 *            0.1.0- M3 P22-O2: Plano (saiu do orquestrar.c), OrqPrepararPlano, OrqAvaliarProduto, OrqLinhaDoEstado, OrqCaminhoDasEntradas, OrqCaminhoDoEstado, OrqTextoJson e OrqConstruir.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#ifndef FORJA_DRIVER_ORQUESTRAR_INTERNO_H
#define FORJA_DRIVER_ORQUESTRAR_INTERNO_H

#include <stdint.h>

#include "cache/estado.h"
#include "cache/projeto_ler.h"
#include "driver/opcoes.h"
#include "driver/orquestrar_tempos.h"

#define ORQ_CAMINHO_MAX (PROJETO_CAMINHO_MAX * 2)   /* pasta do projeto + .forja_cache\estado\... */
#define ORQ_MOTIVO_MAX (PROJETO_CAMINHO_MAX + 64)

/* O projeto lido, o estado gravado e o que vale para a execucao toda (id do forja.exe, inicio, modo). camada/ordem: ProjetoCamadas. */
typedef struct {
    const Opcoes *opcoes;
    Projeto projeto;
    Estado estado;
    uint64_t forja_id, inicio;      /* Hash64 do forja.exe; FILETIME do inicio do plano/build (lido ANTES de disparar o 1o filho) */
    const char *modo;               /* "dev" | "release" */
    uint32_t *camada, *ordem;
    Tempos tempos;                  /* desligado sem FORJA_ORQ_TEMPOS; o estado da medicao mora aqui, no plano da execucao */
} Plano;

/* Estado do modo, id do forja.exe e relogio. false = mensagem no stderr. */
bool OrqPrepararPlano(Plano *pl);

/* true = recompila e 'motivo' recebe o texto; false = reusa. Usa o estado EM MEMORIA (o build o atualiza a cada produto pronto: a chave do dependente sai com a hash_interface real). */
bool OrqAvaliarProduto(const Plano *pl, uint32_t k, char *motivo, size_t n);

/* A linha do estado do produto k logo depois do build: chave e ambiente de hoje (com o .entradas novo e a hash_interface atual das dependencias), hash_interface do .h
 * (dll), tamanho e mtime da saida. false = o .entradas, a saida ou o .h nao puderam ser lidos. */
bool OrqLinhaDoEstado(const Plano *pl, uint32_t k, EstadoLinha *linha);

void OrqCaminhoDoEstado(const Plano *pl, char *destino, size_t tam);
void OrqCaminhoDasEntradas(const Plano *pl, const Produto *pr, char *destino, size_t tam);

/* Texto entre aspas, com \" e \\ escapados, no stdout. */
void OrqTextoJson(const char *texto);

/* Executa o plano: trava, faxina, filhos em paralelo, estado, saida e resumo. Devolve o exit do forja. */
int OrqConstruir(Plano *pl);

#endif
