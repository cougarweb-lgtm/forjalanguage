/* cmd_fmt.h - subcomando 'forja fmt' (F1, plano f1/f2): formata arquivos e pastas .fj pela forma canonica, trocando so os vaos entre os tokens.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - F1 P1b: RodarFmt (arquivo, pasta, dry-run com diff, --aplicar com conferencia, --json e --stdin).
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_CMD_FMT_H
#define FORJA_DRIVER_CMD_FMT_H

/* forja fmt <arq.fj | pasta> ... [--aplicar] [--json] [--stdin]; argv sem "forja" e sem "fmt".
 * Padrao = dry-run: o diff unificado de cada arquivo que mudaria e o rodape 'N mudariam, M nao analisados, K canonicos'. --aplicar grava cada arquivo de forma atomica (LF, sem BOM),
 * relê e confere (0 desvios; se nao der, restaura o original e sai com 2) e o rodape vira 'N gravados, M nao analisados, K canonicos' (cada gravado sai como 'gravado: <arq>').
 * --json: um documento {"arquivos": [{"arquivo", "estado": canonico|mudaria|gravado|nao_analisado, "desvios", "primeiro", "edicoes", "diagnostico", "motivo"}], "totais": {...}}.
 * --stdin: le o fonte da entrada padrao e escreve o texto canonico na saida (sintaxe errada = JSON no stderr, exit 1, stdout vazio); nao vale com --aplicar, --json nem caminho.
 * Arquivo com erro de sintaxe ou token de 65535 bytes ou mais = nao_analisado com o 1o diagnostico, e nao e tocado. Pasta: recursiva (MigrarColetar), em ordem de nome.
 * Exit: 0 = tudo canonico ou gravado; 1 = algum arquivo mudaria (dry-run) ou ficou nao_analisado; 2 = uso ou E/S (caminho ausente, arquivo so-leitura na gravacao, conferencia falhou). */
int RodarFmt(int argc, char **argv);

#endif
