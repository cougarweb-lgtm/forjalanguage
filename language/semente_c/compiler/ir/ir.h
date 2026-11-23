/* ir.h - IR estruturada da Forja: expressoes achatadas em temporarios, sem unidades nem argumentos nomeados.
 * Versao: 0.30.0
 * Changelog: 0.30.0 - Linguagem v1.13 L1a-ir: IrExterno.modulo e IrTrampolim.modulo viram uint32_t arquivo (trecho da Fonte); a IR e o exe de programa sem pasta ficam iguais.
 *            0.29.0 - M6d P92e (D-M6d-7): IR_LIB_ENCERRA (no fim do enum) e IrPrograma.fins/nfins (simbolos C de todo foreign `ends` declarado).
 *            0.28.0 - M6c P88 (D-M6c-6): IR_HANDLE_NASCE e IR_HANDLE_SOLTA (no fim do enum): rastreio automatico de handle de lib C ao lado da chamada foreign com clausula creates/destroys/takes.
 *            0.27.0 - M6 P65a (D-M6-13): IrFuncao.nomes_locais/nnomes_locais (tabela de nomes dos locais do usuario, vinda da Decl).
 *            0.26.0 - K17: IR_HEAP (no fim do enum, heap_push/heap_pop; HEAP_SUBIR, HEAP_SUBIR_BY, HEAP_RETIRA_TOPO, HEAP_RETIRA_TOPO_BY) e IrPrograma.usa_heap.
 *            0.25.0 - K08b: IR_BUSCA_BINARIA (no fim do enum, binary_search; BUSCA_POSICAO/BUSCA_ACHOU), ORDENA_GERAL_CHAVE (sort_by_key), IrInstr.desloc_chave e tipo_chave, IrPrograma.usa_ordenar_chave.
 *            0.24.0 - K08a: IrPrograma.usa_ordenar_estavel (sort_by; o runtime proprio, forja_rt_ordenar_estavel.h, so entra com ele).
 *            0.23.0 - K08a: IR_ORDENA_GERAL (no fim do enum, sort_by) e ORDENA_GERAL_ESTAVEL.
 *            0.22.0 - to_str aceita builder de qualquer alocador (REDIM_PARA_STR nao exige mais heap; so comentario).
 *            0.21.0 - IA1-L9f: IR_REDIMENSIONA ganha op_aux REDIM_PARA_STR (to_str: move os bytes do builder para uma str dona; tem destino). Nenhuma IrOp nova.
 *            0.20.0 - IA1-L9c: IR_REDIMENSIONA ganha op_aux REDIM_CAP (le a capacidade; tem destino) e REDIM_RESERVA (cresce sem mudar o len). Nenhuma IrOp nova.
 *            0.19.0 - IA1-L5b: IR_REDIMENSIONA, IR_ANEXA_FATIA e IR_ORDENA (no fim do enum), ORDENA_* e IrPrograma.usa_ordenar.
 *            0.18.0 - IA1-L5a: IR_FORMATA ganha op_aux 3 (imprime em stderr, eprint); o formato da parte ganha o bit FMT_CURTO (real no formato curto). Nenhuma IrOp nova.
 *            0.17.0 - IR_ALOCA ganha op_aux IR_ALOCA_ZERADO (1): o bloco novo sai zerado, sem IR_GRAVA_PONTEIRO depois (new([N]T) grande sem valor inicial; o zero em temporario estourava a pilha do x64).
 *            0.16.0 - Linguagem v1.12 M3 P21v-b: IR_CHAMADA_IND (chamada por valor de tipo proc, no fim do enum); V_PROC vale em qualquer posicao de valor (atribuicao, campo, parametro, retorno).
 *            0.15.0 - Linguagem v1.11 L13: IR_UNARIO ganha op_aux = OP_RAIZ (raiz quadrada de f32/f64, sqrt do fonte). Nenhuma IrOp nova.
 *            0.14.0 -Linguagem v1.11: IR_FATIA_DE_PONTEIRO (slice_from), no fim do enum.
 *            0.13.1- so comentario (M0-D): semantica operacional de cada IrOp, antes do enum.
 *            0.13.0 - Linguagem v1.10 (A6): IrExterno.simbolo (simbolo C do foreign; igual a nome quando nao ha alias) e IrExterno.nome volta a ser o nome Forja.
 *            0.12.0 - Linguagem v1.9: IR_CONJUNTO (P4), IR_MATRIZ/IR_MATRIZ_ELEMENTO/IR_MATRIZ_END_ELEMENTO (P6), IR_SOA_* (P8),
 *                    IR_MAPA_* (P11); IR_LIBERA op_aux 4 e 5.
 *            0.11.0 - Linguagem v1.8b P3b: IrStruct.opaco ('opaque' de foreign) e IrEnum.valores (enum com valor).
 *            Linguagem v1.8b P5b: V_PROC, IrTrampolim e IrPrograma.trampolins (args de IR_CHAMADA podem ter V_PROC).
 *            0.10.0 - Linguagem v1.8 P4: IrExterno.modulo. Linguagem v1.8 P6b: IrExterno.interno; alocador Valor u32 (Arena/Pool); IR_LIBERA op_aux 3.
 *            0.9.0 -Linguagem v1.7 P7: IR_EMBRULHA, IR_OPC_TEM, IR_OPC_VALOR, IR_TUPLA, IR_TUPLA_ELEMENTO, IR_FORMATA (no fim); IrCaso.tipo;
 *                    IrUniao e IrPrograma.unioes; IR_LIBERA op_aux 2 (str); IR_CHAVEIA com ?T, ?^T e union.
 *            0.8.0 -Linguagem v1.6 P6a: IR_ALOCA, IR_LISTA_CRIA, IR_ANEXA, IR_LIBERA, IR_FRAME_RESET; IR_TAMANHO_TEXTO, IR_ELEMENTO,
 *                    IR_ENDERECO_ELEMENTO e IR_FATIA aceitam [dynamic]T e ^[dynamic]T.
 *            0.7.0 - Linguagem v1.5: IR_ELEMENTO, IR_ENDERECO_ELEMENTO, IR_ENDERECO_CAMPO, IR_ARRAY e IR_FATIA (arrays e fatias).
 *            0.6.0 - Linguagem v1.4 P4: IrExterno e IrPrograma.externos/bibliotecas (foreign).
 *            0.5.0 - Linguagem v1.3: V_NULO, IR_GRAVA_PONTEIRO, IR_TAMANHO_TEXTO (declarado), IrFuncao.linha; OP_ENDERECO de qualquer
 *                    tipo e OP_DEREF em IR_UNARIO; IR_CAMPO sobre ^Struct; IR_CONVERSAO de ponteiro (tabela 3.5 do contrato).
 *            0.4.0 - Linguagem v1.2: IR_TRANSMUTA; documentacao dos numericos novos e dos bits.
 *            0.3.0 - Linguagem v1.1: IrFuncao.exportado ('pub export proc').
 *            0.2.0 - Fase 5c'': contrato do backend emit_c. IrPrograma ganha IrStruct (campos com faixa), IrEnum (membros) e
 *                    IrConst (valor escalar ja dobrado). IrCaso ganha 'ligacao' (local do 'switch r in'). Tabela de campos por IrOp
 *                    abaixo, com o que cada op le e escreve em IrInstr. Mudanca de campo = bump aqui + nota no MEM-LJ-ADR-38.
 *            0.1.0 - Fase 5a: contrato do lowering AST -> IR (5d) e do backend IR -> C11 (5c''). Estruturada (if/loop
 *                    continuam blocos) porque o alvo e C: nada de CFG ou SSA agora. 5d pode refinar campos.
 * Verificacao: construir.bat (compila com -Werror); tests\backend\rodar_teste_emit.bat (IR escrita a mao -> C -> gcc).
 *
 * Regras da IR: a unidade some (so o tipo numerico fica); o argumento nomeado ja esta na ordem dos parametros com os
 * padroes preenchidos; 'defer' ja foi expandido antes de cada saida (return/break/continue/try); a checagem de faixa
 * e explicita (IR_VERIFICA_FAIXA); T ! E e um tipo comum da IR (struct com tag), tratado pelo backend.
 *
 * Convencao de tipo: IrInstr.tipo e o tipo do DESTINO quando a op produz um destino (temp ou local); se nao ha destino,
 * e o tipo que a op manipula (ver tabela). Valor.tipo e sempre o tipo do operando.
 */
#ifndef FORJA_IR_H
#define FORJA_IR_H

#include "common/base.h"
#include "frontend/ast.h"

typedef enum { V_NENHUM, V_TEMP, V_LOCAL, V_INT, V_REAL, V_BOOL, V_TEXTO, V_NULO,
               V_PROC /* v1.8b: proc da Forja passado como callback; id = nome internado do proc; tipo = o TC_PROC do parametro; sai em C como fjt_<nome> */
} ValorTipo;

/* Operando simples: sempre local, temporario ou constante (nunca uma expressao).
 * V_INT de tipo enum: inteiro = indice do membro em IrEnum.membros. V_NENHUM: sem valor (vazio, ou zero em IR_LOCAL).
 * V_NULO: none (tipo = ?^T ou rawptr); sai NULL em C. */
typedef struct {
    uint8_t tipo_no; /* ValorTipo */
    TipoId tipo;
    int32_t id;      /* V_TEMP/V_LOCAL: numero; V_TEXTO: nome internado do texto (texto ja decodificado, sem aspas) */
    int64_t inteiro; /* V_INT, V_BOOL (0/1) */
    double real;     /* V_REAL */
} Valor;

/* Tabela de campos por op. "destino" e numero de temp ou de local conforme a op; "-" = nao usado.
 *
 * IR_LOCAL        destino = local | tipo = tipo do local | a = valor inicial (V_NENHUM = zero) | mutavel = var (true) / let
 * IR_UNARIO       destino = temp | tipo = resultado | op_aux = OP_NEGAR | OP_NAO | OP_ENDERECO | a = operando
 *                 | OP_DEREF (OP_ENDERECO: a = V_LOCAL de qualquer tipo, tipo = ^T; OP_DEREF: a = ^T, tipo = apontado)
 *                 | OP_RAIZ (a e tipo = f32 ou f64: raiz quadrada; negativo da NaN)
 * IR_BINARIO      destino = temp | tipo = resultado (bool nas comparacoes) | op_aux = Operador | a, b = operandos
 *                 (a.tipo = tipo dos operandos; decide a forma de emitir + - * / % em i32/int)
 * IR_CONVERSAO    destino = temp | tipo = alvo | a = origem
 * IR_CHAMADA      destino = temp ou -1 | tipo = retorno (vazio com destino -1) | nome = proc | args[nargs] na ordem dos parametros
 * IR_CHAMADA_IND  destino = temp ou -1 | tipo = retorno (vazio com destino -1) | a = o proc chamado (tipo TC_PROC; local, temp ou V_PROC) | args[nargs] na ordem dos parametros do tipo proc | pos
 * IR_CAMPO       destino = temp | tipo = tipo do campo | a = struct ou ponteiro (a.tipo: ponteiro usa '->') | nome = campo
 * IR_STRUCT       destino = temp | tipo = struct | args[nargs] na ordem de IrStruct.campos (nargs = ncampos)
 * IR_ATRIBUICAO   destino = local | tipo = tipo do alvo | a = valor | nome = -1: alvo e o local; nome >= 0: alvo e o campo nome
 *                 do local (local ponteiro usa '->'; local struct usa '.')
 * IR_VERIFICA_FAIXA a = valor | faixa_min, faixa_max, faixa_excl | nome = campo | tipo = -
 * IR_GRAVA_PONTEIRO sem destino | a = ponteiro ^T | b = valor | nome = campo do apontado struct, ou -1 = grava o apontado
 *                 inteiro | tipo = tipo gravado
 * IR_TAMANHO_TEXTO destino = temp | tipo = int | a = str, []T, [dynamic]T (".len") ou ^[dynamic]T ("->len")
 * IR_ELEMENTO     destino = temp | tipo = T | a = [N]T (V_LOCAL/V_TEMP, por valor), ^[N]T, []T, [dynamic]T ou ^[dynamic]T | b = indice (int)
 *                 | op_aux = 1: indice provado (sem forja_indice) | pos
 * IR_ENDERECO_ELEMENTO destino = temp | tipo = ^T | a = ^[N]T, []T, [dynamic]T ou ^[dynamic]T | b = indice (int) | op_aux como acima | pos
 * IR_ENDERECO_CAMPO destino = temp | tipo = ^C | a = ^Struct | nome = campo
 * IR_FATIA        destino = temp | tipo = []T | a = ^[N]T, []T, [dynamic]T ou ^[dynamic]T | b = inicio (int) | args[0] = fim (int), nargs = 1
 *                 | faixa_excl = '..<' | op_aux = 1: limites provados (sem forja_fatia_confere) | pos
 * IR_ARRAY        destino = temp | tipo = [N]T (args[N] na ordem do fonte; nargs 0 = zero), []T (nargs 0 = fatia vazia)
 *                 ou [dynamic]T (nargs 0 = lista zero)
 * IR_ALOCA        destino = temp | tipo = ^T | a = V_INT alocador (1 heap, 2 frame) ou Valor u32 (handle de Arena/Pool) | op_aux 0 normal, IR_ALOCA_ZERADO (1) bloco zerado | pos
 * IR_LISTA_CRIA   destino = temp | tipo = [dynamic]T | a = alocador (como IR_ALOCA) | b = cap (int; V_INT 0 sem 'cap') | pos
 * IR_ANEXA        sem destino | tipo = T (elemento) | a = ^[dynamic]T | b = item (T) | pos
 * IR_LIBERA       sem destino | op_aux 0: a = ^T, ?^T, cstr ou ?cstr (bloco de new / de format_c) | op_aux 1: a = ^[dynamic]T (endereco
 *                 da lista) | op_aux 2: a = str (libera a.ptr; bloco de format) | op_aux 3: a = ^T, b = handle do Pool (u32) | op_aux 4: a = ^map (P11)
 *                 | op_aux 5: a = ^soa[dynamic]S | pos
 * IR_FRAME_RESET  sem destino e sem operandos | pos
 * IR_EMBRULHA     destino = temp | tipo = ?T ou union | a = valor | b = V_INT tag (u32) em union; V_NENHUM em ?T
 * IR_OPC_TEM      destino = temp | tipo = bool | a = ?T ou ?^T | op_aux 0 = tem valor, 1 = e none
 * IR_OPC_VALOR    destino = temp | tipo = T (?T) ou ^T (?^T) | a = ?T ou ?^T (so quando tem valor)
 * IR_TUPLA        destino = temp | tipo = tupla | args[nargs] = valores na ordem
 * IR_TUPLA_ELEMENTO destino = temp | tipo = elemento k | a = tupla | b = V_INT k
 * IR_FORMATA      destino = temp (op_aux 1 str, 2 cstr) ou -1 (op_aux 0 imprime) | tipo = str, cstr ou vazio | a = alocador
 *                 (u32: V_INT 1 heap, 2 frame, 0 no print; ou Valor com o handle de Arena) | args = PARES (formato V_INT tipo TIPO_U32, valor) por parte, na ordem do
 *                 texto (pedaco = (0, V_TEXTO str)) | pos
 * IR_CONJUNTO     destino = temp ou -1 | tipo = resultado | op_aux: 2 diferenca (a - b; bit_set), 3 tem (bool: a contem o membro
 *                 b), 4 insere (a = ^bit_set, b = membro; sem destino), 5 remove (idem), 6 conta (int: membros de a), 7 adiciona
 *                 (destino = a com o membro b), 8 membro (destino enum = o membro de indice b (int))
 * IR_MATRIZ       destino = temp | tipo = resultado | op_aux: 0 soma, 1 subtracao, 2 matriz x matriz, 3 matriz x escalar (a
 *                 matriz, b escalar), 4 escalar x matriz (a escalar, b matriz), 5 matriz x vetor (b = [C]T, destino [R]T), 6
 *                 negacao (a), 7 transposta (a), 8 literal (args[nargs] = os R*C elementos EM ORDEM DE LINHAS)
 * IR_MATRIZ_ELEMENTO     destino = temp (T) | a = matriz ou ^matriz | b = linha (int) | args[0] = coluna (int), nargs = 1
 * IR_MATRIZ_END_ELEMENTO destino = temp (^T) | idem
 * IR_SOA_ELEMENTO  destino = temp (S) | a = soa ou ^soa | b = indice (int): copia o elemento (gather)
 * IR_SOA_CAMPO     destino = temp (tipo do campo) | a = soa ou ^soa | b = indice | nome = campo
 * IR_SOA_END_CAMPO destino = temp (^campo) | idem
 * IR_SOA_GRAVA     sem destino | a = ^soa | b = indice | args[0] = valor S, nargs = 1 (scatter)
 * IR_SOA_COLUNA    destino = temp ([]campo) | a = ^soa | nome = campo
 * IR_SOA_CRIA      destino = temp | tipo = soa[dynamic]S | a = alocador | b = cap
 * IR_SOA_ANEXA     sem destino | tipo = S | a = ^soa[dynamic]S | b = valor S
 * IR_MAPA_CRIA     destino = temp | tipo = map | a = alocador | b = cap
 * IR_MAPA_INSERE   sem destino | tipo = V | a = ^map | b = chave | args[0] = valor, nargs = 1
 * IR_MAPA_ACHA     destino = temp (?V, ?^X ou ?cstr) | a = map ou ^map | b = chave
 * IR_MAPA_PONTEIRO destino = temp (?^V) | a = ^map | b = chave
 * IR_MAPA_TEM      destino = temp (bool) | a = map ou ^map | b = chave
 * IR_MAPA_REMOVE   sem destino | tipo = V | a = ^map | b = chave
 * IR_MAPA_ITEM     destino = temp | a = map ou ^map | b = indice denso (int, 0..<len) | op_aux 0 chave (K), 1 valor (V), 2 endereco
 *                  do valor (^V)
 * IR_FATIA_DE_PONTEIRO destino = temp | tipo = []T | a = ^T ou ?^T (vindo de foreign) | b = quantidade de elementos (int) | pos
 * IR_IMPRIME     a = valor (despacha por a.tipo: i32, int, bool, texto, f32)
 * IR_SE           cond = bloco calculado antes | a = condicao | corpo = entao | senao = senao (pode ser vazio)
 * IR_ENQUANTO     cond = bloco recalculado a cada volta | a = condicao | corpo
 * IR_PARA         destino = local do contador | tipo = tipo do contador (i32 ou int) | a = inicio | b = fim | faixa_excl | corpo
 * IR_CHAVEIA      a = valor | tipo = tipo do valor | casos[]: enum -> membro = indice do membro; resultado (T ! E) ->
 *                 membro = TipoId do caso (T ou E), ligacao = local do 'r' (-1 sem ligacao); padrao = caso default;
 *                 ?T/?^T -> membro 1 = valor, 0 = none; union -> membro = tag; IrCaso.tipo = tipo da ligacao (o do case; no
 *                 default e no none, o valor inteiro)
 * IR_RETORNA      a = valor (V_NENHUM em proc sem retorno)
 * IR_PARE         - (sai do laco mais interno)
 * IR_CONTINUA     - (volta ao topo do laco mais interno)
 * IR_RES_OK       destino = temp | tipo = T ! E | a = valor de T (V_NENHUM se T e vazio)
 * IR_RES_ERRO     destino = temp | tipo = T ! E | a = erro (valor de enum)
 * IR_RES_E_ERRO   destino = temp | tipo = bool | a = resultado
 * IR_RES_VALOR    destino = temp | tipo = T | a = resultado (so quando nao e erro)
 * IR_RES_ERRO_DE  destino = temp | tipo = E | a = resultado (so quando e erro)
 * IR_EXPECT       a = condicao (bool) | nome = texto internado da condicao | pos
 * IR_ASSERT       a = condicao (bool) | b = V_TEXTO da mensagem | pos
 * IR_PANIC        a = V_TEXTO da mensagem | pos
 * IR_TRANSMUTA    destino = temp | tipo = alvo | a = origem (mesmo tamanho em bytes que o alvo)
 *
 * Linguagem v1.2 (numericos):
 *  - Valor.inteiro (V_INT) guarda BITS: tipo sem sinal le (uint64_t)inteiro; com sinal le int64.
 *  - IR_UNARIO op_aux += OP_BIT_NAO; IR_BINARIO op_aux += OP_BIT_E, OP_BIT_OU, OP_BIT_XOU, OP_DESLOCA_ESQ, OP_DESLOCA_DIR.
 *  - IR_CONVERSAO: o par (a.tipo, tipo) decide cast puro ou checado.
 *  - IR_PARA: contador de qualquer inteiro. IR_VERIFICA_FAIXA: a.tipo sem sinal usa a funcao _u64.
 *  - IR_IMPRIME: despacha por a.tipo: todos os inteiros, bool, texto.
 *  - IrCampo e IrInstr.faixa_min/max continuam int64_t (limites <= INT64_MAX garantidos pelo parser).
 */

/* Semantica operacional de cada IrOp (0.13.1), para quem escreve um segundo backend sem ler o codigo do backend x64.
 * Convencoes: as instrucoes de um bloco rodam em ordem; todo operando (a, b, args) ja esta calculado em local, temporario ou constante,
 * entao a ordem de avaliacao das expressoes do fonte e a ordem das instrucoes. "dev" = build com checagens (FORJA_DEV, tambem o de teste);
 * "release" = sem checagens (custo zero). Falha de runtime em dev = escreve uma linha JSON em stderr com o codigo Rxxxx, arquivo e linha
 * do .fj (IrInstr.pos) e termina o processo com 101 (dentro de teste: marca o teste como falho e volta ao executor). Sem falha em
 * release, o resultado e o do C bruto (wrap, ou comportamento indefinido onde dito).
 *
 * IR_LOCAL            Declara o local `destino` com o valor de `a`; com a = V_NENHUM o local nasce zerado (todos os bytes 0). So o valor inicial
 *                     e gravado aqui; 'mutavel' e informacao do front-end, sem efeito de execucao.
 * IR_UNARIO           temp = op a. OP_NEGAR: inteiro = 0 - a checado (dev: R0002 em INT_MIN; release: wrap); real = -a. OP_NAO: nega o bool.
 *                     OP_BIT_NAO: complemento de bits, truncado ao tipo do resultado. OP_ENDERECO: temp = endereco do local `a`
 *                     (a precisa ser V_LOCAL). OP_DEREF: temp = o valor apontado por `a` (sem checar nulo). OP_RAIZ: temp = raiz quadrada de `a`
 *                     (f32 ou f64, igual ao sqrtss/sqrtsd; negativo da NaN, -0.0 da -0.0).
 * IR_BINARIO          temp = a op b. OU/E logicos e comparacoes (== != < <= > >=) dao bool, sobre operandos ja calculados (sem curto-circuito
 *                     aqui); == e != de str comparam bytes e tamanho (nao o ponteiro). + - * de inteiro: dev falha R0002 no estouro, release da wrap.
 *                     / e % de inteiro: divisor 0 da 0 nos dois modos; MIN/-1 falha R0002 em dev e da wrap (resto 0) em release. Real: conta
 *                     de ponto flutuante direta (resto de real nao existe). & | ^ truncam ao tipo; << e >> (so tipos sem sinal) com
 *                     deslocamento >= largura dao 0 (sem trap) nos dois modos; o bit que sai pela esquerda some, sem estouro.
 * IR_CONVERSAO        temp = a convertido ('as'). Inteiro para inteiro que sempre cabe, e qualquer destino real: conversao pura. Inteiro que
 *                     pode nao caber, e real para inteiro: dev falha R0002 se fora da faixa do destino; release inteiro->inteiro trunca (wrap
 *                     em modulo 2^N) e real->inteiro satura nos limites do destino (NaN vira 0; sem sinal, negativo vira 0). Ponteiro: cast puro, exceto rawptr ou ?^T para ^T / cstr, que em dev falham R0005 se nulos (release nao
 *                     confere). cstr para str mede o texto (strlen); nulo falha R0005 em dev e vira str vazia em release.
 * IR_CHAMADA          Chama o proc `nome` com args[0..nargs) na ordem dos parametros; destino >= 0 recebe o retorno, -1 descarta. Foreign
 *                     tambem entra por aqui (a chamada vai ao involucro do proc, que chama o simbolo C).
 * IR_CAMPO            temp = campo `nome` da struct `a` (por valor), ou do apontado quando `a` e ponteiro. Sem checar nulo.
 * IR_STRUCT           temp = struct nova com os campos = args na ordem de IrStruct.campos (nargs = ncampos); sem campos = struct zerada.
 * IR_ATRIBUICAO       Grava `a` no local `destino` (nome = -1) ou no campo `nome` do local `destino` (struct por valor). Sem temp de saida.
 * IR_VERIFICA_FAIXA   Confere que o inteiro `a` esta em [faixa_min, faixa_max] (faixa_excl: max excluso; sem sinal compara em 64 bits sem
 *                     sinal); `nome` e o campo, so para a mensagem. Dev: fora da faixa falha R0001. Release: nao faz nada.
 * IR_IMPRIME          Escreve `a` em stdout seguido de '\n', conforme o tipo: i8/i16/i32 em decimal, int em decimal, u8..uint em decimal sem
 *                     sinal, bool como true/false, str pelos `len` bytes, cstr ate o NUL, f32 com 9 digitos significativos (%.9g). Outro
 *                     tipo e erro interno do backend. f64 nao e impresso por aqui.
 * IR_SE               Roda o bloco `cond`, depois `corpo` se a for true, senao `senao` (pode ser vazio). `cond` roda uma vez, antes do teste.
 * IR_ENQUANTO         Laco: a cada volta roda `cond` (recalculado) e testa `a`; false sai do laco; true roda `corpo` e volta ao topo.
 * IR_PARA             Laco de contador. `a` (inicio) e `b` (fim) sao lidos UMA vez, antes da 1a volta, num contador interno de 64 bits
 *                     (sem sinal se o tipo e sem sinal). Se inicio > fim (ou >= com faixa_excl), nao roda nenhuma vez. A cada volta o
 *                     local `destino` recebe o contador antes do `corpo`; alterar `destino` no corpo nao muda o laco. O incremento so
 *                     acontece depois do corpo (tambem apos CONTINUA), e o laco termina sem estourar mesmo com fim = maximo do tipo.
 * IR_CHAVEIA          Escolhe UM caso de `casos[]` pelo valor de `a` e roda o `corpo` dele; nao ha queda para o caso seguinte. Enum: casa
 *                     pelo valor C do membro (indice, ou IrEnum.valores). Resultado: valor ou erro conforme e_erro. ?T e ?^T: none ou
 *                     tem valor. union: pela tag. Caso `padrao` pega o que nenhum outro pegou. Antes do corpo, se `ligacao` >= 0, o local
 *                     `ligacao` recebe o conteudo (valor ou erro do resultado, valor do ?T, variante da union).
 * IR_RETORNA          Sai da funcao devolvendo `a` (V_NENHUM em proc sem retorno).
 * IR_PARE             Sai do laco mais interno (inclusive de dentro de um IR_CHAVEIA, que nao e laco).
 * IR_CONTINUA         Vai ao fim da volta do laco mais interno: ENQUANTO reavalia `cond`; PARA incrementa o contador e testa.
 * IR_RES_OK           temp = resultado T ! E em estado "ok" com o valor `a` (V_NENHUM se T e vazio).
 * IR_RES_ERRO         temp = resultado T ! E em estado "erro" com o erro `a` (valor de enum). Sem valor de T.
 * IR_RES_E_ERRO       temp(bool) = true se o resultado `a` esta em estado de erro.
 * IR_RES_VALOR        temp = o valor T dentro do resultado `a`. So valido quando nao e erro; nao confere.
 * IR_RES_ERRO_DE      temp = o erro E dentro do resultado `a`. So valido quando e erro; nao confere.
 * IR_EXPECT           Se a for false: dentro de teste marca o teste como falho, imprime "expect falhou: <texto nome>" em stderr e SEGUE;
 *                     fora de teste vira panic (R0003). Vale em dev e em release.
 * IR_ASSERT           Se a for false, falha R0003 com a mensagem b (str): JSON em stderr e termina com 101. Vale em dev e em release.
 * IR_PANIC            Falha R0003 incondicional com a mensagem `a` (str). Nao retorna. Vale em dev e em release.
 * IR_TRANSMUTA        temp = os bytes de `a` reinterpretados como o tipo do destino (mesmo tamanho, garantido pela semantica; copia de
 *                     bytes, sem conversao nem checagem).
 * IR_GRAVA_PONTEIRO   Grava b no apontado de `a` (nome = -1) ou no campo `nome` do apontado (a e ^Struct). Sem checar nulo.
 * IR_TAMANHO_TEXTO    temp(int) = `len` de a: bytes do str, elementos da fatia ou da lista [dynamic]T (tambem por ^[dynamic]T).
 * IR_ELEMENTO         temp = copia do elemento b de `a` (array por valor, ^array, fatia, lista ou ^lista). op_aux 1 = indice ja provado,
 *                     sem checagem. Senao dev falha R0006 se b < 0 ou b >= tamanho; em lista confere tambem a epoca do frame (R0008 se a lista do
 *                     frame sobreviveu a um frame_reset). Release: sem checagem (indice fora e comportamento indefinido).
 * IR_ENDERECO_ELEMENTO temp(^T) = endereco do elemento b de `a`; mesmas bases, checagens e op_aux de IR_ELEMENTO.
 * IR_ENDERECO_CAMPO   temp(^C) = endereco do campo `nome` do apontado de `a` (^Struct). Sem checar nulo.
 * IR_FATIA            temp([]T) = visao (ponteiro + tamanho, sem copiar) de a[b ..< args[0]] (faixa_excl) ou a[b ..= args[0]]. Em lista
 *                     confere a epoca do frame antes (dev, R0008). Dev confere os limites (R0006) salvo op_aux 1; release nao confere.
 * IR_ARRAY            temp = array [N]T com args[k] no elemento k; nargs 0 = array todo zerado (ou fatia vazia se o tipo e []T).
 * IR_ALOCA            temp(^T) = bloco novo de sizeof(T) bytes no alocador `a` (1 heap, 2 frame, ou handle u32 de Arena/Pool). Sem memoria:
 *                     R0007. Dev: heap carrega cabecalho de rastreio (vazamento acusado no fim, R0009). O conteudo NAO e zerado (no
 *                     heap de release e malloc cru) (a confirmar para frame/Arena/Pool). Com op_aux IR_ALOCA_ZERADO o bloco inteiro sai
 *                     com bytes 0 (BIR_ZERA_MEM / memset do runtime no nativo); o lowering so pede isso quando o zero do
 *                     tipo e todo bytes 0 e grande (array), no lugar de montar o zero num temporario e gravar com IR_GRAVA_PONTEIRO.
 * IR_LISTA_CRIA       temp([dynamic]T) = lista vazia (len 0) com capacidade b no alocador `a`; guarda `cap` e `alocador` na propria lista
 *                     e, no frame, a epoca. cap 0 = sem bloco (ptr nulo); cap negativo ou grande demais falha R0007.
 * IR_ANEXA            append no ^[dynamic]T `a` do item b: dev confere a epoca do frame (R0008); se len == cap, cresce (cap <4 vira 8,
 *                     senao dobra; copia para bloco novo no alocador da lista; alocador 0 falha R0007; Pool recusa); grava b em ptr[len] e
 *                     faz len += 1. O ponteiro ptr pode mudar; fatias antigas da lista ficam invalidas.
 * IR_LIBERA           Devolve memoria, conforme op_aux. 0: libera o bloco apontado por `a` (new, ou format_c; nulo e bloco do frame sao
 *                     ignorados; dev falha R0008 se nao veio do heap ou ja foi liberado). 1: libera o buffer da ^[dynamic]T `a` (so se o
 *                     alocador for heap) e zera a lista mantendo o alocador. 2: libera o bloco de ptr do str `a` (format). 3: devolve o
 *                     bloco `a` ao Pool de handle `b` (dev: R0008 se o bloco nao e deste Pool). 4: libera o map apontado por `a` (so
 *                     o bloco, se heap) e o zera mantendo o alocador. 5: libera as colunas da ^soa[dynamic]S `a` (se heap) e zera a soa
 *                     mantendo o alocador.
 * IR_FRAME_RESET      Zera o frame (alocador 2): o topo volta a 0 e a epoca avanca; em dev o conteudo e preenchido com 0xDD. Listas do frame
 *                     antigo passam a falhar R0008 em dev quando usadas.
 * IR_EMBRULHA         temp = `a` embrulhado. Em ?T de struct opcional: tem = true e valor a. Em ?cstr e ?^T: o proprio ponteiro (nulo = none).
 *                     Em union: tag = b (V_INT) e valor a na variante da tag.
 * IR_OPC_TEM          temp(bool) sobre o opcional `a`: op_aux 0 = tem valor; 1 = e none. Em ?cstr/?^T: ponteiro nao nulo / nulo.
 * IR_OPC_VALOR        temp = o valor dentro do opcional `a` (T do ?T, ou o proprio ponteiro em ?cstr/?^T). Nao confere se tem valor.
 * IR_TUPLA            temp = tupla com args[k] na posicao k.
 * IR_TUPLA_ELEMENTO   temp = elemento de indice b (V_INT, constante) da tupla `a`.
 * IR_FORMATA          Texto formatado f"...": zera o buffer unico de formatacao, anexa na ordem cada par de args (formato V_INT, valor)
 *                     (pedaco literal = valor V_TEXTO; formato leva largura, zeros, hex, precisao), e termina conforme op_aux: 0 imprime o
 *                     buffer em stdout com '\n'; 3 imprime o buffer em stderr com '\n' (eprint, sem temp); 1 temp(str) = copia do buffer num bloco novo no alocador `a`; 2 temp(cstr) = idem com NUL
 *                     no fim. Sem memoria: R0007.
 * IR_CONJUNTO         Operacao sobre bit_set conforme op_aux: 2 temp = a sem os membros de b; 3 temp(bool) = a contem o membro de indice b;
 *                     4 insere o membro b no bit_set apontado por `a` (sem temp); 5 remove o membro b do apontado por `a`; 6 temp(int) = numero
 *                     de bits ligados em a; 7 temp = a com o membro b ligado; 8 temp = bit_set so com o membro de indice b.
 *                     Membro fora do tamanho do bit_set: nao confere (a confirmar).
 * IR_MATRIZ           temp = conta de matrix (guardada por colunas), conforme op_aux: 0/1 soma/subtracao elemento a elemento de a e b; 2
 *                     produto a[R,K] x b[K,C], cada soma em ordem de k crescente a partir de 0, sem fusao multiplica-soma; 3 cada elemento
 *                     de a vezes o escalar b; 4 o escalar a vezes cada elemento de b; 5 a[R,C] x vetor b[C] = vetor [R]; 6 negacao de a;
 *                     7 transposta de a; 8 literal: args em ordem de linhas (R*C) montam a matriz. Sem checar estouro (conta de real).
 * IR_MATRIZ_ELEMENTO  temp = elemento (linha b, coluna args[0]) de `a` (matrix ou ^matrix). Dev confere os dois indices (R0006).
 * IR_MATRIZ_END_ELEMENTO temp(^T) = endereco do elemento (linha b, coluna args[0]); mesmas checagens.
 * IR_SOA_ELEMENTO     temp = struct S montada juntando o campo de cada coluna na posicao b (gather). Indice conferido em dev (R0006;
 *                     soa dinamica tambem a epoca do frame, R0008).
 * IR_SOA_CAMPO        temp = valor do campo `nome` da posicao b da soa `a` (ou ^soa). Mesma checagem de indice.
 * IR_SOA_END_CAMPO    temp(^campo) = endereco do campo `nome` na posicao b. Mesma checagem de indice.
 * IR_SOA_GRAVA        Espalha a struct args[0] pelas colunas da posicao b da ^soa `a` (scatter). Mesma checagem de indice.
 * IR_SOA_COLUNA       temp([]campo) = visao da coluna `nome` inteira (len = N na fixa, len atual na dinamica). Dinamica: confere a epoca (R0008).
 * IR_SOA_CRIA         temp = soa dinamica vazia (len 0) com capacidade b no alocador `a`, um bloco para todas as colunas; guarda cap e
 *                     alocador. Sem memoria: R0007.
 * IR_SOA_ANEXA        append na ^soa[dynamic]S `a` do item b: confere a epoca (dev), cresce todas as colunas se len == cap (alocador 0
 *                     falha), espalha os campos de b na posicao len, len += 1.
 * IR_MAPA_CRIA        temp = map vazio com capacidade inicial b no alocador `a` (tabela hash com pares densos em ordem de insercao).
 * IR_MAPA_INSERE      Insere o par (chave b, valor args[0]) no ^map `a`; se a chave ja existe, troca o valor. Cresce a tabela quando cheia.
 *                     Chaves str comparam por conteudo; as demais por bytes. Dev confere a epoca (R0008).
 * IR_MAPA_ACHA        temp = ?V: o valor de chave b se existir (none se nao). Em V ponteiro/cstr o none e nulo; senao struct com tem/v.
 * IR_MAPA_PONTEIRO    temp(?^V) = endereco do valor de chave b, ou nulo se a chave nao existe.
 * IR_MAPA_TEM         temp(bool) = a chave b existe no map.
 * IR_MAPA_REMOVE      Remove a chave b do ^map `a`; ausente = nada acontece. O ultimo par denso ocupa a posicao do removido (a ordem de
 *                     iteracao muda).
 * IR_MAPA_ITEM        Par denso de indice b (0..<len, sem checar) do map `a`: op_aux 0 temp = a chave (dev: confere a epoca primeiro), 1 temp =
 *                     o valor, 2 temp(^V) = o endereco do valor. Mexer no map durante a iteracao reordena/invalida os pares.
 * IR_FATIA_DE_PONTEIRO temp([]T) = visao {ptr = a, len = b} de um vetor C de b elementos (slice_from). Nao e dona e nao copia. Dev: b < 0 falha
 *                     R0006; a nulo com b > 0 falha R0005; a nulo com b == 0 da a fatia vazia. Release: nao confere.
 * IR_REDIMENSIONA     resize: `a` e ^[dynamic]T ou [dynamic]T (variavel/temp), b = novo len (int), tipo = T (inteiro, real ou bool). b < 0 falha R0007. Cresce (forja_lista_crescer repetido)
 *                     ate cap >= b, zera com bytes 0 os elementos de len ate b quando b > len e grava len = b. Dev confere a epoca do frame (R0008). Encolher so muda o len.
 *                     op_aux REDIM_CAP (L9c): temp(int) = capacidade da lista `a` (b nao usado). op_aux REDIM_RESERVA (L9c): cresce ate cap >= b (int), len e conteudo intactos (b <= cap nao faz nada).
 *                     op_aux REDIM_PARA_STR (L9f, to_str): `a` = ^[dynamic]u8 de qualquer alocador (heap, frame ou Arena: o bloco fica no alocador dele), temp(str) = os bytes MOVIDOS para uma str dona: garante cap > len, grava NUL em
 *                     ptr[len], o str e {ptr, len} (o delete(str) libera o bloco inteiro) e a lista volta a {ptr = 0, len = 0, cap = 0} mantendo alocador e epoca (b e tipo = str, sem b).
 * IR_ANEXA_FATIA      append de fatia: `a` e ^[dynamic]T, b = fatia []T (ou str numa lista de u8), tipo = T. Dev confere a epoca; cresce ate cap >= len + b.len,
 *                     copia b.len elementos para ptr[len] (a fatia nao pode ser da propria lista) e faz len += b.len.
 * IR_ORDENA           ordena no lugar a fatia `a` ([]T), crescente, op_aux = ORDENA_*; f32/f64 com NaN no fim, str por bytes (depois pelo tamanho). Nao e estavel.
 * IR_ORDENA_GERAL     (K08a) ordena no lugar a fatia `a` ([]T) com o comparador `b` (valor de proc, less: (^T, ^T) -> bool; less(x, y) = x vem antes de y); tipo = T (o
 *                     tamanho do elemento vem dele); op_aux ORDENA_GERAL_ESTAVEL: iguais mantem a ordem. Dev/release: sem checagem de proc nulo nesta fatia.
 *                     (K08b) op_aux ORDENA_GERAL_CHAVE = sort_by_key: sem proc; a chave e o campo desloc_chave (byte) de T, do tipo tipo_chave (ORDENA_*); b nao usado.
 * IR_BUSCA_BINARIA    (K08b) binary_search(a, value = b): a = fatia ordenada de T, b = o valor (local de T, tipo = T, tipo_chave = ORDENA_*); op_aux BUSCA_POSICAO
 *                     -> destino(int) = lower bound; BUSCA_ACHOU -> destino(bool) = igual ao valor na posicao do lower bound. Sem checagem de limite (fatia vazia = 0 / falso).
 * IR_HEAP            (K17) heap_push / heap_pop sobre a lista `a` ([dynamic]T, ja com o append do push feito): b = proc less (so nos _BY), tipo = T,
 *                     tipo_chave = ORDENA_* de T (nos _BY vale 0); op_aux HEAP_*. SUBIR sifta o ultimo (n - 1); RETIRA_TOPO troca l[0] com l[n - 1] e desce.
 * IR_HANDLE_NASCE     (M6c P88) logo DEPOIS da IR_CHAMADA de um foreign com `creates`: nome = simbolo C do criador (o alias, se houver; o kind do R0009) | a = o handle (o temp do retorno, ou, com op_aux = 1,
 *                     o argumento ^?^T de saida, de onde o runtime le o handle que a lib escreveu) | b = V_TEXTO com o nome do destruidor declarado do mesmo tipo (V_NENHUM = nenhum).
 *                     Sem destino e sem efeito no programa. Dev: forja_handle_nasce (handle nulo ignorado). Release: o x64 NAO a transforma em nada (a BIR descarta ANTES de ler o operando,
 *                     da vivacidade e da alocacao): o exe sai byte a byte igual ao do fonte sem clausula.
 * IR_HANDLE_SOLTA     (M6c P88) logo ANTES da IR_CHAMADA de um foreign com `destroys` ou `takes`: a = o argumento do parametro da clausula (ponteiro ou inteiro). Dev: forja_handle_solta
 *                     (handle desconhecido = silencio). Release: igual a IR_HANDLE_NASCE. Os dois so existem quando o handle e escalar (struct por valor fica sem rastreio).
 * IR_LIB_ENCERRA      (M6d P92e) logo DEPOIS da IR_CHAMADA de um foreign com `ends`: nome = simbolo C do proc que encerra a lib. Sem destino e sem efeito no programa. Dev: forja_libmem_encerrou(simbolo)
 *                     (marca a lib como encerrada no runtime). Release: igual a IR_HANDLE_NASCE (a BIR nao emite nada).
 * IR_CHAMADA_IND      Chama o endereco de funcao `a` (valor de tipo proc) com args[0..nargs) na ordem dos parametros do tipo; destino >= 0 recebe o
 *                     retorno, -1 descarta. Dev: `a` nulo falha R0005 ("chamada por proc nulo"); release nao confere. A convencao de chamada e a da
 *                     fronteira C (a mesma do trampolim fjt_ e do foreign). IR_CONVERSAO tambem converte de/para proc: 'x as proc(...)' de rawptr, ^T
 *                     ou ?^T (dev: rawptr e ?^T nulos falham R0005, como o 'as ^T') e 'f as rawptr'; V_PROC (o nome de um proc da Forja) vale como `a`
 *                     de IR_LOCAL, IR_ATRIBUICAO, IR_STRUCT, IR_RETORNA e argumento de IR_CHAMADA / IR_CHAMADA_IND.
 */
typedef enum {
    IR_LOCAL,          /* destino = a  (declara; 'mutavel' diz var/let) */
    IR_UNARIO,         /* destino = op a */
    IR_BINARIO,        /* destino = a op b */
    IR_CONVERSAO,      /* destino = (tipo) a */
    IR_CHAMADA,        /* destino (ou -1) = proc(args...) */
    IR_CAMPO,          /* destino = a.campo (por valor ou por ponteiro, conforme o tipo de a) */
    IR_STRUCT,         /* destino = Struct{campo0, campo1, ...} (args na ordem dos campos) */
    IR_ATRIBUICAO,     /* alvo = a (alvo e local ou campo) */
    IR_VERIFICA_FAIXA, /* trava em runtime se a fora de [faixa_min, faixa_max] (dev/test; release remove) */
    IR_IMPRIME,
    IR_SE, IR_ENQUANTO, IR_PARA, IR_CHAVEIA, /* controle: usam os blocos filhos */
    IR_RETORNA, IR_PARE, IR_CONTINUA,
    IR_RES_OK, IR_RES_ERRO, /* destino = T ! E construido a partir de a */
    IR_RES_E_ERRO,          /* destino(bool) = a e erro */
    IR_RES_VALOR, IR_RES_ERRO_DE, /* destino = valor/erro de a */
    IR_EXPECT, IR_ASSERT, IR_PANIC,
    IR_TRANSMUTA,      /* destino = bits de a reinterpretados como tipo (mesmo tamanho; sem checagem) */
    IR_GRAVA_PONTEIRO, /* *a = b  ou  a->campo = b (nome = campo; -1 = o apontado inteiro) */
    IR_TAMANHO_TEXTO,  /* destino(int) = bytes de a (str) ou len de a ([]T) */
    IR_ELEMENTO,          /* destino = a[b] (Linguagem v1.5) */
    IR_ENDERECO_ELEMENTO, /* destino = &a[b] */
    IR_ENDERECO_CAMPO,    /* destino = &a->campo */
    IR_FATIA,             /* destino = fatia de a [b, fim) */
    IR_ARRAY,             /* destino = literal de array (ou zero com nargs 0) */
    IR_ALOCA,             /* destino = new(T, em = A) (Linguagem v1.6) */
    IR_LISTA_CRIA,        /* destino = make([dynamic]T, em = A, cap = N) */
    IR_ANEXA,             /* append(a, item = b) */
    IR_LIBERA,            /* delete: op_aux 0 = bloco de new, 1 = buffer de lista */
    IR_FRAME_RESET,       /* frame_reset() */
    IR_EMBRULHA,          /* destino = T -> ?T ou variante -> union (Linguagem v1.7) */
    IR_OPC_TEM,           /* destino(bool) = a tem valor (op_aux 0) / e none (op_aux 1) */
    IR_OPC_VALOR,         /* destino = valor de a (?T / ?^T) */
    IR_TUPLA,             /* destino = tupla dos args */
    IR_TUPLA_ELEMENTO,    /* destino = elemento b da tupla a */
    IR_FORMATA,           /* destino = format / format_c, ou imprime f"..." (op_aux 0) */
    IR_CONJUNTO,          /* v1.9: operacoes de bit_set (op_aux 2..8) */
    IR_MATRIZ,            /* v1.9: contas e literal de matrix (op_aux 0..8) */
    IR_MATRIZ_ELEMENTO,   /* v1.9: destino = a[b, args[0]] de matrix */
    IR_MATRIZ_END_ELEMENTO, /* v1.9: destino = &a[b, args[0]] de matrix */
    IR_SOA_ELEMENTO,      /* v1.9: destino = a[b] de soa inteiro (gather) */
    IR_SOA_CAMPO,         /* v1.9: destino = a[b].campo de soa */
    IR_SOA_END_CAMPO,     /* v1.9: destino = &a[b].campo de soa */
    IR_SOA_GRAVA,         /* v1.9: a[b] = args[0] (scatter) */
    IR_SOA_COLUNA,        /* v1.9: destino = fatia da coluna a.campo */
    IR_SOA_CRIA,          /* v1.9: destino = make(soa[dynamic]S) */
    IR_SOA_ANEXA,         /* v1.9: append(&a, item = b) em soa dinamica */
    IR_MAPA_CRIA,         /* v1.9: destino = make(map[K]V) */
    IR_MAPA_INSERE,       /* v1.9: insert(&m, key = b, value = args[0]) */
    IR_MAPA_ACHA,         /* v1.9: destino = get(m, key = b) */
    IR_MAPA_PONTEIRO,     /* v1.9: destino = get_ptr(&m, key = b) */
    IR_MAPA_TEM,          /* v1.9: destino = has(m, key = b) */
    IR_MAPA_REMOVE,       /* v1.9: remove(&m, key = b) */
    IR_MAPA_ITEM,         /* v1.9: chave, valor ou endereco do valor do par denso b (for k, v in m) */
    IR_FATIA_DE_PONTEIRO, /* v1.11: destino = slice_from(a, len = b): fatia emprestada de um vetor C */
    IR_CHAMADA_IND,       /* v1.12: destino = a(args...) sobre valor de tipo proc (chamada indireta) */
    IR_REDIMENSIONA,      /* IA1-L5b: resize(&a, len = b); make(..., len = b) */
    IR_ANEXA_FATIA,       /* IA1-L5b: append(&a, items = b) */
    IR_ORDENA,            /* IA1-L5b: sort(a) de fatia de inteiro, real ou str */
    IR_ORDENA_GERAL,      /* K08a: sort_by(a, less = b) (e a base do sort_by_key): op_aux = ORDENA_GERAL_* */
    IR_BUSCA_BINARIA,     /* K08b: binary_search(a, value = b): op_aux BUSCA_POSICAO (lower bound) ou BUSCA_ACHOU (igualdade) */
    IR_HEAP,              /* K17: heap_push/heap_pop sobre a lista `a` (^[dynamic]T): op_aux HEAP_* (sift-up do ultimo ou retirada do topo) */
    IR_HANDLE_NASCE,      /* M6c P88: depois do foreign `creates`: o handle `a` (op_aux 1 = ponteiro de saida) passa a ser rastreado em dev */
    IR_HANDLE_SOLTA,      /* M6c P88: antes do foreign `destroys`/`takes`: o handle `a` deixa de ser rastreado em dev */
    IR_LIB_ENCERRA        /* M6d P92e: depois do foreign `ends`: nome = simbolo C; a lib passa a "encerrada" no runtime dev */
} IrOp;

/* IR_ORDENA op_aux: o tipo do elemento da fatia (o runtime escolhe a comparacao por ele). */
enum { ORDENA_I8, ORDENA_I16, ORDENA_I32, ORDENA_I64, ORDENA_U8, ORDENA_U16, ORDENA_U32, ORDENA_U64, ORDENA_F32, ORDENA_F64, ORDENA_STR };

/* IR_ORDENA_GERAL op_aux (K08a): ESTAVEL = forja_ordenar_estavel (sort_by): a ordem dos iguais se mantem, sem buffer.
 * CHAVE (K08b, sort_by_key): forja_ordc_ordenar com a chave no campo IrInstr.desloc_chave, do tipo ORDENA_* IrInstr.tipo_chave; b nao usado. */
enum { ORDENA_GERAL_ESTAVEL, ORDENA_GERAL_CHAVE };

/* IR_BUSCA_BINARIA op_aux (K08b): POSICAO = destino(int) = lower bound de b em a; ACHOU = destino(bool) = igual a b na posicao do lower bound (NaN nunca). */
enum { BUSCA_POSICAO, BUSCA_ACHOU };

/* IR_HEAP op_aux (K17): SUBIR = heap_push (o ultimo da lista sobe; o append ja foi feito); RETIRA_TOPO = heap_pop (o topo vai para l[len - 1], a lista ainda
 * tem len; o IR le e encolhe depois). Os _BY usam o proc de b (less); os outros comparam pela ordem do sort (tipo_chave = ORDENA_* de T). */
enum { HEAP_SUBIR, HEAP_SUBIR_BY, HEAP_RETIRA_TOPO, HEAP_RETIRA_TOPO_BY };

#define IR_ALOCA_ZERADO 1u /* IR_ALOCA op_aux: o bloco novo sai zerado (sem IR_GRAVA_PONTEIRO) */

/* IR_REDIMENSIONA op_aux (IA1-L9c): REDIM_RESIZE = resize/make(len =) como no L5b; REDIM_CAP = destino(int) = a capacidade da lista `a` (tipo = int, sem b);
 * REDIM_RESERVA = cresce ate cap >= b sem mexer em len nem zerar (tipo = T; b <= cap, negativo inclusive, nao faz nada). */
enum { REDIM_RESIZE, REDIM_CAP, REDIM_RESERVA, REDIM_PARA_STR }; /* REDIM_PARA_STR (IA1-L9f): to_str(&buf); destino = temp(str), tipo = str */

typedef struct IrInstr IrInstr;
typedef struct { IrInstr *primeira, *ultima; } IrBloco;

typedef struct {
    int32_t membro;   /* enum: indice do membro; resultado: TipoId do caso (valor T ou erro E) */
    bool padrao;
    int32_t ligacao;  /* resultado: local do 'r' neste caso (-1 sem ligacao) */
    TipoId tipo;      /* tipo da ligacao (o do case; no default e no none, o valor inteiro) */
    IrBloco corpo;
} IrCaso;

struct IrInstr {
    uint8_t op;       /* IrOp */
    uint8_t op_aux;   /* Operador (unario/binario) */
    bool mutavel;
    uint32_t pos;     /* offset na fonte, para #line, runtime e mensagens */
    int32_t destino;  /* numero do temp/local; -1 sem destino */
    int32_t nome;     /* proc chamado, campo acessado ou variavel de ligacao */
    TipoId tipo;
    Valor a, b;
    Valor *args; uint32_t nargs;
    int64_t faixa_min, faixa_max; bool faixa_excl;
    int64_t desloc_chave; uint8_t tipo_chave; /* K08b: IR_ORDENA_GERAL (chave): deslocamento do campo na struct e ORDENA_* do campo; IR_BUSCA_BINARIA: tipo_chave = ORDENA_* do elemento */
    IrBloco cond, corpo, senao; /* IR_SE: cond calculada antes de testar 'a'; IR_ENQUANTO: cond recalculada a cada volta */
    IrCaso *casos; uint32_t ncasos;
    IrInstr *prox;
};

typedef struct { int32_t nome; TipoId tipo; } IrParam;

typedef struct {
    int32_t nome;
    TipoId retorno;
    IrParam *params; uint32_t nparams;
    uint32_t ntemps, nlocais; /* locais 0..nparams-1 sao os parametros */
    const LocalNome *nomes_locais; /* M6 P65a: nome/tipo por local_id do usuario (0..nnomes_locais-1); id >= isso e temporario do lowering. NULL/0 no proc oculto */
    uint32_t nnomes_locais;
    IrBloco corpo;
    bool e_main, e_teste;
    bool exportado; /* 'pub export proc': na DLL (EMIT_BIBLIOTECA) ganha um involucro de nome limpo, sem o prefixo fj_ */
    int32_t titulo; /* e_teste: titulo internado */
    uint32_t linha; /* linha do 'proc' no .fj; preenchida so quando exportado (involucro, mensagem do JSON) */
} IrFuncao;

/* Campo de struct com a faixa declarada (a checagem em si ja e IR_VERIFICA_FAIXA no corpo das funcoes). */
typedef struct {
    int32_t nome;
    TipoId tipo;
    bool tem_faixa;
    int64_t faixa_min, faixa_max;
    bool faixa_excl;
} IrCampo;

typedef struct { int32_t nome; IrCampo *campos; uint32_t ncampos; bool opaco; /* v1.8b: 'opaque' de foreign; so o typedef antecipado sai no C */ } IrStruct;

/* membros: nomes internados, na ordem; valores (v1.8b): NULL = sequencial 0..n-1, senao o valor C de cada membro */
typedef struct { int32_t nome; int32_t *membros; uint32_t nmembros; int64_t *valores; } IrEnum;

/* union Nome { A, B }: variantes na ordem do fonte (tag = indice). */
typedef struct { int32_t nome; TipoId tipo; TipoId *variantes; uint32_t nvariantes; } IrUniao;

/* Constante ja dobrada pelo lowering: valor escalar (V_INT, V_REAL, V_BOOL ou V_TEXTO). */
typedef struct { int32_t nome; TipoId tipo; Valor valor; } IrConst;

/* Proc de 'foreign': so a assinatura (o corpo esta na biblioteca C). IrFuncao nunca representa foreign. */
typedef struct {
    int32_t nome;              /* nome Forja (nome do involucro fj_<nome>); sem alias e igual ao simbolo C */
    int32_t simbolo;           /* v1.10 (A6): simbolo C chamado (rotulo asm); = nome quando nao ha alias */
    TipoId retorno;            /* TIPO_VAZIO sem retorno */
    IrParam *params; uint32_t nparams;
    uint32_t linha;            /* linha do 'proc' no .fj do modulo dele (texto do R0005 do retorno ^T/cstr) */
    uint32_t arquivo;          /* v1.13: trecho da Fonte (arquivo) que declarou o foreign (0 = raiz); sem pasta e igual ao modulo; escolhe o caminho do R0005 */
    bool interno;              /* v1.8: foreign da std (nome forja_*): so o involucro, o simbolo C ja existe no runtime */
} IrExterno;

/* Proc da Forja usado como callback de foreign (v1.8b): nome do proc, o TC_PROC, linha do 'proc' no arquivo do modulo e o modulo
 * (para o R0005 do trampolim fjt_<nome>). v1.13: 'arquivo' e o trecho da Fonte (um modulo de pasta tem varios arquivos). */
typedef struct { int32_t nome; TipoId tipo; uint32_t linha; uint32_t arquivo; } IrTrampolim;

typedef struct {
    IrFuncao *funcoes; uint32_t nfuncoes;
    IrStruct *structs; uint32_t nstructs;
    IrEnum *enums; uint32_t nenums;
    IrConst *consts; uint32_t nconsts;
    IrExterno *externos; uint32_t nexternos;      /* procs de 'foreign': IR_CHAMADA a eles chama o involucro fj_<nome> */
    int32_t *bibliotecas; uint32_t nbibliotecas;  /* 'link', ids internados, ordem do fonte, sem repeticao */
    IrUniao *unioes; uint32_t nunioes;            /* union, na ordem do fonte (v1.7) */
    IrTrampolim *trampolins; uint32_t ntrampolins; /* procs da Forja passados como callback, sem repeticao (v1.8b) */
    int32_t *fins; uint32_t nfins;                 /* M6d P92e: simbolos C (internados) de TODO foreign `ends` declarado, usado ou nao */
    bool usa_ordenar;                              /* IA1-L5b: algum IR_ORDENA: marca o runtime de forja_rt_ordenar.h (nenhum leitor no x64) */
    bool usa_ordenar_estavel;                      /* K08a: algum IR_ORDENA_GERAL (sort_by): marca forja_rt_ordenar_estavel.h (nenhum leitor no x64) */
    bool usa_ordenar_chave;                        /* K08b: algum IR_ORDENA_GERAL de chave (sort_by_key) ou IR_BUSCA_BINARIA: marca forja_rt_ordenar_chave.h (nenhum leitor no x64) */
    bool usa_heap;                                 /* K17: algum IR_HEAP: marca forja_rt_heap.h (nenhum leitor no x64; o heap usa a comparacao de forja_rt_ordenar_chave.h) */
    bool usa_real_curto;                           /* Ryu P1: algum f"..." com parte FMT_CURTO (print/eprint de real): marca forja_rt_ryu.h (nenhum leitor no x64) */
    /* Tipos (struct, enum, resultado) vivem na tabela de tipos de semantic/; o backend le de la. */
} IrPrograma;

#endif
