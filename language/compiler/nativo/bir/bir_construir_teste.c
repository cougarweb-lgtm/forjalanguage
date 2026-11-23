/* bir_construir_teste.c - IR -> BIR: o forja_programa do modo teste (forja test --backend x64).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M2 P12b: primeira versao. Para cada IrFuncao e_teste, na ordem do programa: CHAMA forja_teste_registrar(END_SIMBOLO <titulo>, END_SIMBOLO fj_<nome interno>);
 *                    depois dst = CHAMA forja_testes_rodar; RETORNA dst. Igual ao EmitRegistrosTeste do emit_c_funcao.c (o titulo e o rotulo; sem titulo, o nome).
 *                    fj_main entra no programa mas nao e chamado, como no C.
 * Verificacao: construir.bat && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/teste
 */
#include "nativo/bir/bir_interno.h"

#define ARGS_REGISTRO 2

/* forja_teste_registrar(titulo, endereco da funcao do teste). */
static void RegistrarTeste(BirCtx *c, const IrFuncao *irf)
{
    int32_t rotulo = irf->titulo >= 0 ? irf->titulo : irf->nome;
    BirOperando *args = BirArgs(c, ARGS_REGISTRO);
    uint32_t funcao = BirNovoVreg(c, BC_I64);
    BirInstr *chama;

    args[0] = OpVreg(BirVregDoTexto(c, InternerTexto(c->e->nomes, rotulo), InternerTam(c->e->nomes, rotulo)), BC_I64);
    BirEmitirPedido(c, (BirPedido){.op = BIR_END_SIMBOLO, .dst = funcao, .a = OpSimbolo(c->simbolo_do_nome[irf->nome])});
    args[1] = OpVreg(funcao, BC_I64);
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, "forja_teste_registrar");
    chama->args = args;
    chama->nargs = ARGS_REGISTRO;
}

bool BirProgramaTeste(BirCtx *c)
{
    const IrPrograma *ir = c->e->ir;
    uint32_t resultado = BIR_SEM_VREG;
    BirOperando valor;
    BirInstr *chama;

    for (uint32_t k = 0; k < ir->nfuncoes; k++) {
        if (ir->funcoes[k].e_teste)
            RegistrarTeste(c, &ir->funcoes[k]);
    }
    resultado = BirNovoVreg(c, BC_I32);
    chama = BirEmitir(c, BIR_CHAMA, BC_I32);
    chama->simbolo = BirSimboloRuntime(c, "forja_testes_rodar");
    chama->dst = resultado;
    valor = OpVreg(resultado, BC_I32);
    BirRetorna(c, &valor);
    return true;
}
