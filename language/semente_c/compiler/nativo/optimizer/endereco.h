/* endereco.h - passe `endereco` (R6a-3, D0704 fatia C): recoloca a conta de endereco colada a cada CARREGA/GRAVA que a compartilhava, para o isel x64 funde-la no modo [x+i*s+d].
 * Versao: 0.1.0
 * Changelog: 0.1.0 - R6a-3: primeira versao. Passe proprio (e nao uma funcao do isel ou da numeracao) porque precisa das analises de grafo (dominadores, caminhos def->uso) e porque roda DEPOIS do
 *                    licm (que sobe o `add x, imm` da cadeia) e ANTES do layout/alocador; o isel so funde cadeia de temporario de uso unico e adjacente, e a numeracao (que juntou as contas repetidas) e quem as compartilha.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe> && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe endereco,licm
 *
 * Cadeia: t = ADD(m, b) com m = MUL(i, s), s em {1,2,4,8} (indices do isel x64_isel_otim_mem.c, formas A e B), b = vreg x ou w = ADD(x, imm32). Cada definicao unica no programa, so ADD/MUL de I64 sem trap.
 * Quando todo uso de t e a base de um CARREGA/GRAVA e ha 2 ou mais usos (o isel nao funde temporario compartilhado), cada acesso ganha a propria copia da cadeia (w', m', t') colada na frente dele; a
 * instrucao de memoria NAO muda de lugar (nenhum trap novo, R0005/R0006 e o rastreio ficam como estao). Legalidade: i e x (ou o b da soma) nao podem ser redefinidos em nenhum caminho entre a definicao
 * de quem os le e o acesso (a definicao domina o acesso; so ha redefinicao fora da regiao de caminhos que nao repassam pela propria definicao). O que ficar sem uso (t, m, w) e removido. Sem --simbolos.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_ENDERECO_H
#define FORJA_NATIVO_OPTIMIZER_ENDERECO_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

/* Passe da tabela (passes.c): programa inteiro, so release. O relato ganha `endereco <funcao>: N cadeias recolocadas, M instrucoes removidas` por funcao em que algo mudou. */
void OtimEnderecoAplicar(BirPrograma *programa, Buf *relato);

/* Usos por vreg numa funcao: usos = todos os operandos que leem o vreg; usos_base = so os que sao a base (operando a) de CARREGA/GRAVA (a GRAVA do proprio endereco como valor nao conta). */
typedef struct { uint32_t n; uint32_t *usos, *usos_base; } EnderecoUsos;

void OtimEnderecoUsosCalcular(const BirFuncao *funcao, EnderecoUsos *u);
void OtimEnderecoUsosLiberar(EnderecoUsos *u);

/* `soma` e ADD I64 de vreg com imediato int32 cujo UNICO uso e a base de um CARREGA/GRAVA (o isel funde no modo [x+imm+d]; subi-la so gasta registrador). Usado pelo licm. */
bool OtimEnderecoSomaFundivel(const EnderecoUsos *u, const BirInstr *soma);

#endif
