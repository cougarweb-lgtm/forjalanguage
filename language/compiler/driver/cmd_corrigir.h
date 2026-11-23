/* cmd_corrigir.h - subcomando 'forja corrigir': aplica as correcoes que o proprio diagnostico sugere (IA1-b05, contrato 1.1 item 3, M-D2 e M-D13).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b05: RodarCorrigir.
 * Verificacao: construir.bat && python tests\ia_first\verificar_corrigir.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_CMD_CORRIGIR_H
#define FORJA_DRIVER_CMD_CORRIGIR_H

/* forja corrigir <arq.fj> [--codigo CODIGO] [--aplicar] [--tambem-provavel]; argv sem "forja" e sem "corrigir".
 * Sem --aplicar nada e gravado: o stdout traz o diff unificado das correcoes de confianca "certa" (com --tambem-provavel, tambem as "provavel").
 * Com --aplicar grava, roda o check de novo e DESFAZ (devolve o arquivo byte a byte) se piorou: o erro original continua, o numero de erros subiu ou apareceu
 * erro de etapa anterior (sintaxe onde so havia semantica).
 * Exit: 0 = ok (inclusive "nada a corrigir"); 1 = a correcao piorou e foi desfeita; 2 = uso (arquivo ou opcao invalida); 3 = erro interno (check nao rodou, gravacao falhou). */
int RodarCorrigir(int argc, char **argv);

#endif
