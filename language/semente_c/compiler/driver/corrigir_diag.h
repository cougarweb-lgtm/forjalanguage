/* corrigir_diag.h - o 'forja corrigir' pergunta ao proprio forja: roda "forja check" num processo filho e le os diagnosticos (JSON) com as correcoes aplicaveis por maquina.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-I4: DiagLido.bruto (a linha JSON do diagnostico) e CorrigirTemCodigo (vinha do cmd_corrigir.c).
 *            0.1.0 - IA1-b05: CorrigirRodarCheck e DiagLido.
 * Verificacao: construir.bat && python tests\ia_first\verificar_corrigir.py --forja <forja.exe>
 *
 * Filho em vez de chamar o front-end aqui dentro: o check tem estado global por execucao (spec impresso uma vez por codigo, modo resumo) e o "depois" da correcao
 * tem de ser medido por uma execucao limpa, a mesma que o agente faria sozinho.
 */
#ifndef FORJA_DRIVER_CORRIGIR_DIAG_H
#define FORJA_DRIVER_CORRIGIR_DIAG_H

#include "common/arena.h"
#include "driver/edicoes.h"

/* Um diagnostico do check. Os textos e as edicoes vivem na Arena de quem chamou CorrigirRodarCheck. */
typedef struct {
    const char *codigo, *arquivo, *confianca, *sugestao, *id;   /* confianca e sugestao NULL = diagnostico sem pacote de correcao */
    const char *bruto;      /* a linha JSON inteira do diagnostico, como o check a imprimiu (sem o LF); o 'forja editar' a devolve como pacote do porque */
    uint32_t linha, coluna;
    Edicao *edicoes;
    uint32_t nedicoes;
    bool edicoes_no_alvo;   /* todas as edicoes sao do arquivo pedido (so estas o corrigir aplica) */
} DiagLido;

typedef struct {
    int saida;              /* exit do check: 0 sem erro, 1 com erro */
    DiagLido *itens;
    uint32_t n;
} ResultadoCheck;

/* Roda "<este forja> check --max-erros 100 <alvo>" no diretorio atual e le os diagnosticos do stdout. false = o check nao rodou ou terminou com exit diferente de 0 e 1
 * (uso, interno): 'erro' recebe a frase em PT-BR com o que o filho escreveu no stderr. */
bool CorrigirRodarCheck(Arena *arena, const char *alvo, ResultadoCheck *resultado, char *erro, size_t tam_erro);

/* O resultado tem algum diagnostico com esse codigo ("E0202")? */
bool CorrigirTemCodigo(const ResultadoCheck *r, const char *codigo);

/* Mesmo arquivo? Barra e contrabarra valem igual, maiusculas e minusculas tambem (Windows). */
bool CorrigirMesmoCaminho(const char *a, const char *b);

#endif
