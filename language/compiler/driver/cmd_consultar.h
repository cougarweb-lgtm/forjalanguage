/* cmd_consultar.h - 'forja consultar <pergunta> ... [--json]': o compilador responde sobre o projeto (tipos, assinaturas, referencias, escopo).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I2: RodarConsultar e as 6 perguntas da parte 1 (simbolos, escopo, refs, tipo, assinatura, campos).
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#ifndef FORJA_CMD_CONSULTAR_H
#define FORJA_CMD_CONSULTAR_H

/* argv sem "forja consultar". Uma linha de JSON em stdout. Compila o projeto (check completo, todos os modulos) e responde; exit 0 ok, 1 se o projeto nao compila (o diagnostico
 * do check sai em stdout), 2 uso errado (pergunta desconhecida lista as perguntas; nome ou posicao que nao existe), 3 interno. A resposta e sempre JSON: --json e aceito e nao muda nada.
 *   forja consultar simbolos [<alvo>] [--modulo <caminho>]    proc, foreign, struct, enum, union, const e unit de cada modulo
 *   forja consultar escopo <arq:L:C> [<alvo>]                 parametros e locais visiveis no ponto, globais do modulo e pub dos importados
 *   forja consultar refs <nome> [<alvo>]                      todos os usos: nome, Tipo.campo, Enum.membro, proc.parametro/local ou embutido
 *   forja consultar tipo <arq:L:C> [<alvo>]                   tipo da menor expressao que cobre o ponto (ou do nome declarado ali)
 *   forja consultar assinatura <nome> [<alvo>]                parametros (nome, tipo, padrao), retorno e erro de um proc
 *   forja consultar campos <Tipo> [<alvo>]                    campos de struct/union ou membros de enum
 * <alvo>: arquivo .fj raiz (precisa de proc main) ou pasta com main.fj; sem <alvo>, escopo e tipo usam o arquivo da posicao e as demais o main.fj da pasta atual. */
int RodarConsultar(int argc, char **argv);

#endif
