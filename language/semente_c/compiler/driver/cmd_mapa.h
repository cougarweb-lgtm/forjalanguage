/* cmd_mapa.h - 'forja mapa [<pasta | arq.fj>] [--json | --resumo] [--modulo <caminho>] [--limite <N>]': o mapa do projeto Forja para quem vai ler ou editar (IA).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I3: RodarMapa.
 * Verificacao: construir.bat && python tests\consultar\verificar_mapa.py --forja <forja.exe>
 */
#ifndef FORJA_CMD_MAPA_H
#define FORJA_CMD_MAPA_H

/* argv sem "forja mapa". Compila o projeto como o 'forja consultar' (arquivo raiz com proc main, ou a pasta com main.fj; sem alvo, o main.fj da pasta atual) e escreve:
 *   --json     uma linha de JSON: tamanho, entrada, modulos (caminho, origem, arquivo, linhas, importa, importado_por, links), api (a mesma linha de 'consultar api' para toda
 *              declaracao pub), tipos (struct com campos, enum com membros, union com variantes), testes e dependencias (modulos de fora e links de foreign);
 *   --resumo   texto hierarquico e curto (o padrao sem opcao): modulos, tipos e nomes pub; --limite N = quantos nomes por lista e por modulo;
 *   --modulo X so o modulo X, com as assinaturas da API (no --resumo) ou so as secoes dele (no --json); X e o caminho do import ("main", "jogo/fisica", "std:mem").
 * O std embutido entra so como linha de modulo. Saida deterministica (mesmos bytes a cada execucao). Exit: 0 ok, 1 o projeto nao compila (diagnostico do check em stdout),
 * 2 uso errado, 3 interno. */
int RodarMapa(int argc, char **argv);

#endif
