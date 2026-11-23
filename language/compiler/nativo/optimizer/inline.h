/* inline.h - passe `inline` (M7b F2): troca a chamada de uma funcao FOLHA pequena pela copia do corpo dela, na BIR do release x64.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M7b F2b (guarda de retorno antecipado): sitio que sobra como CHAMA e folha com "cmp param, #imed; desvia" e um destino que so RETORNA o parametro ou um imediato
 *                    ganha o caminho da folha no chamador (InlineGuardar); a via lenta mantem a chamada.
 *            0.3.0 - M7b F2 (fib_rec): recursao direta ganha 2 niveis (2a passada sobre o corpo original, teto ORC_RECURSAO_CRESCE_MULT x); a copia do corpo e de quem chama (CopiarCorpo/LiberarCorpo).
 *            0.2.0 - M7b R4a: folha pode ter laco (ate INLINE_BLOCOS_LACO_MAX blocos) e funcao com recursao DIRETA (so chama a si mesma, sem laco) clona o proprio corpo 1 nivel em cada sitio recursivo
 *                    (InlineClonar trabalha sobre uma copia da funcao quando o sitio e do proprio chamador).
 *            0.1.0 - M7b F2: primeira versao (inline.c decide quem entra; inline_clonar.c copia os blocos).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe inline
 *
 * Folha: sem CHAMA nem CHAMA_IND (logo sem foreign, import, runtime, --traco), com ou sem laco, retorno <= 64 B, END_SLOT so como base de memoria.
 * Recursao direta (R4a + F2): so CHAMA a si mesma, sem laco, corpo <= 64 instrucoes; entra no proprio corpo original em 2 niveis (1a passada: ate 2 sitios; 2a: as chamadas que sobraram nos clones,
 * com o chamador limitado a 10x o tamanho antes do passe, ORC_RECURSAO_CRESCE_MULT); recursao mutua e niveis > 2 ficam para o F8.
 * So um nivel (a lista de folhas sai UMA vez antes do passe). Orcamento nas constantes de inline.c. R4 (nao mover trap) vale: o clone mantem a ordem e o pos da folha.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_INLINE_H
#define FORJA_NATIVO_OPTIMIZER_INLINE_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

#define INLINE_BLOCOS_MAX 8u       /* blocos da folha sem laco (e da recursiva) que entram por inline */
#define INLINE_BLOCOS_LACO_MAX 12u /* blocos da folha com laco: cabeca, corpo e saida de ate 2 lacos aninhados */

/* Uma chamada a trocar: bloco e indice dela no chamador, indice da folha em BirPrograma.funcoes. instr_add (saida): instrucoes que o chamador ganhou. */
typedef struct { uint32_t bloco, instr, folha, instr_add; } SitioInline;

/* Roda o passe no programa inteiro (so release). relato != NULL recebe as linhas "inline ..." do --debug-code otim. */
void OtimInlineAplicar(BirPrograma *programa, Buf *relato);

/* Copia funda do corpo de uma funcao (blocos, classes de vreg, slots) e a liberacao dela. A copia e de quem chama e so existe para recursao. */
void CopiarCorpo(const BirFuncao *f, BirFuncao *c);
void LiberarCorpo(BirFuncao *c);

/* Guarda de retorno antecipado (F2b): a folha tem "cmp param, #imed; desvia" no bloco de entrada e um destino que so RETORNA o parametro ou um imediato.
 * aux = BirCond do cmp; param = indice do parametro (= vreg dele); limite = imediato; devolvido = vreg do parametro ou imediato; folha_verdadeira = a folha e o alvo[0]. */
typedef struct {
    uint8_t aux;
    uint32_t param;
    BirOperando limite, devolvido;
    bool folha_verdadeira;
} GuardaRetorno;

/* Sitio da guarda: chamada (bloco, instr) de funcoes[chamador] e a guarda da folha dela. */
typedef struct { uint32_t chamador, bloco, instr; GuardaRetorno gr; } SitioGuarda;

/* Parte o bloco do sitio na chamada: antes (com cmp + desvia) fica no lugar; a folha (copia ou constante + salta) e a via lenta (a chamada + salta) saem no fim do vetor.
 * Devolve o id do bloco do resto (o que vem depois da chamada): a varredura segue nele. Ids antigos nao mudam. */
uint32_t InlineGuardar(BirPrograma *programa, const SitioGuarda *s);

/* Troca cada sitio de funcoes[chamador] pela copia da folha. Sitios em ordem (bloco, instr), sem repetir, fora de bloco frio, ja aprovados por inline.c.
 * corpo: copia do corpo original do chamador (NULL se nao ha sitio recursivo); os sitios que apontam para o chamador usam esta copia. */
void InlineClonar(BirPrograma *programa, uint32_t chamador, SitioInline *sitios, uint32_t n, const BirFuncao *corpo);

#endif
