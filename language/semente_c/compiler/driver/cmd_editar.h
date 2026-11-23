/* cmd_editar.h - subcomando 'forja editar': edicao estrutural do projeto por operacoes sobre nomes e nos (IA1-I4, contrato 1.2 item M-E1/M-E2, D-IA1-22 e D-IA1-23).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-I5: inserir_decl, remover_decl e mover_decl no ops.json (o JSON de saida segue em editar_versao 1.0.0, sem campo novo).
 *            0.1.0 - IA1-I4: RodarEditar com renomear, trocar_no e trocar_corpo.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe> && python tests\editar\verificar_editar_i5.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_CMD_EDITAR_H
#define FORJA_DRIVER_CMD_EDITAR_H

/* forja editar [<arq.fj | pasta>] --ops <ops.json> [--aplicar]; argv sem "forja" e sem "editar". O alvo e o do 'forja consultar' (arquivo raiz, ou a pasta com main.fj; sem alvo, o main.fj
 * da pasta atual) e o projeto tem de passar no check ANTES (senao o diagnostico do check sai em stdout, exit 1).
 * ops.json: uma lista de operacoes, ou {"operacoes": [...]}; todas valem sobre o estado original do projeto e entram juntas ou nenhuma entra:
 *   {"op": "renomear", "id": "<nome ou id>", "novo": "<nome>"}   a declaracao e todas as referencias do projeto (inclusive modulo.nome, argumento nomeado e campo de literal);
 *                                                                 nome em string ou comentario nao muda; recusa foreign, main, test, import e a biblioteca padrao
 *   {"op": "trocar_no", "id": "<id do forja ast>", "texto": "<texto novo do no>"}
 *   {"op": "trocar_corpo", "id": "<proc ou test>", "texto": "<miolo das chaves>"}   (aceita "proc" no lugar de "id")
 *   {"op": "inserir_decl", "texto": "<declaracao>", UMA posicao: "depois_de"|"antes_de": "<id de declaracao>" ou "modulo"|"arquivo": "<modulo>" com "no_fim": true |
 *                                                                 "no_inicio_depois_dos_imports": true}
 *   {"op": "remover_decl", "id": "<declaracao>", "forcar": true?}   leva o comentario colado e os imports que so ela usava; recusa se ainda ha referencia (sem "forcar")
 *   {"op": "mover_decl", "id": "<declaracao>", "para": "<modulo>"}  fim do modulo de destino; imports e usos dos modulos tocados sao ajustados; ciclo e raiz importada recusados
 * Sem --aplicar nada e gravado: o stdout traz uma linha JSON com as edicoes, o diff e o resultado do check da copia. O "depois" roda numa copia do projeto em %TEMP%; so se ele nao
 * tem nenhum erro (o projeto passava limpo antes: mais erros ou erro diferente e a mesma coisa) as edicoes sao validas e, com --aplicar, gravadas (atomico, volta ao original se a gravacao
 * de um arquivo falha). Edicao invalida = DESFEITA: nenhum arquivo muda, "desfeito": true e "diagnosticos" traz o pacote 1.1 do check.
 * Exit: 0 = ok (dry-run valido ou aplicado); 1 = a edicao foi desfeita, a operacao e invalida (id inexistente, nome recusado) ou o projeto nao compila; 2 = uso (opcao, ops.json ilegivel
 * ou mal formado); 3 = erro interno (check da copia nao rodou, gravacao falhou). */
int RodarEditar(int argc, char **argv);

#endif
