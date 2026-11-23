/* cmd_migrar.h - subcomando 'forja migrar': reescreve codigo escrito com as formas antigas da tabela de renomeados (common\renomeados.[ch]) para as formas novas em ingles
 * (IA1-L3d1, contrato IA1 item P-IA1-L3).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L3d1: RodarMigrar.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_CMD_MIGRAR_H
#define FORJA_DRIVER_CMD_MIGRAR_H

/* forja migrar <arquivo.fj | pasta> ... [--dry-run | --aplicar] [--comentarios] [--json]; argv sem "forja" e sem "migrar".
 * --dry-run (o padrao) nao grava nada; --aplicar grava cada arquivo de forma atomica (mesma codificacao e quebra de linha do original). --json e o padrao de saida: um documento JSON
 * no stdout, {"arquivos": [{"arquivo", "trocas", "detalhes": [{"linha", "coluna", "coluna_fim", "classe", "antigo", "novo"}], "estado", "diagnostico", "motivo"}], "total_trocas",
 * "arquivos_alterados", "linhas_iguais"}. estado: ok (tem trocas e todas valem), sem_trocas, nao_analisado (o arquivo tem erro: diagnostico traz o codigo; nada e editado) ou revisao (uma
 * troca ficou de fora: motivo diz qual). A migracao e idempotente: a 2a passada acha 0 trocas.
 * Exit: 0 = rodou; 1 = ha arquivo nao_analisado; 2 = uso ou erro de leitura/gravacao. */
int RodarMigrar(int argc, char **argv);

#endif
