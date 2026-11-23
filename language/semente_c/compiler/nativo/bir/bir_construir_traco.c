/* bir_construir_traco.c - IR -> BIR: o traco do diferencial (nivel 2, P18a). Depois da instrucao que grava local ou temporario escalar,
 * CHAMA forja_traco(funcao, instr, linha, coluna, bits) com esses argumentos.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M3 F9b-3: decisoes extraidas de DestinoPronto e BirTracar.
 *            0.1.0 - P18a: primeira versao. A numeracao vem de IrNumerarFuncao (a mesma funcao do emit_c); quem entra vem de IrTracoDestino e IrTracoFormaDe (idem).
 *                    bits = o valor estendido SEM sinal a 64 (BE_ZERO); real = os bits do f32/f64, passados por um slot de 8 bytes (grava como real, carrega como inteiro:
 *                    BIR_BITS nao existe), o mesmo slot em todo o corpo da funcao.
 * Verificacao: construir.bat && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta traco
 */
#include <stdio.h>

#include "nativo/bir/bir_interno.h"

#define TRACO_SLOT_BYTES 8u
#define TRACO_NOME_MAX 512
#define TRACO_NARGS 5u

/* Os bits do real: grava no slot de 8 bytes da funcao e carrega como inteiro da mesma largura (F32 -> I32, F64 -> I64). */
static BirOperando BitsDoReal(BirCtx *c, BirOperando real)
{
    uint8_t classe = real.classe == BC_F64 ? BC_I64 : BC_I32;
    uint32_t endereco, dst;

    if (c->slot_traco < 0)
        c->slot_traco = (int32_t)BirNovoSlot(c, TRACO_SLOT_BYTES, TRACO_SLOT_BYTES);
    endereco = BirEmitirEndSlot(c, (uint32_t)c->slot_traco, BirNovoVreg(c, BC_I64));
    BirEmitirGrava(c, endereco, 0, real);
    dst = BirNovoVreg(c, classe);
    BirEmitirCarrega(c, dst, endereco, 0);
    return OpVreg(dst, classe);
}

/* O valor segue estendido a 64 bits SEM sinal, como (uint64_t)(uintN_t)(x) em C. */
static BirOperando ComExtensaoZero(BirOperando o)
{
    return o.classe == BC_I64 ? o : OpComExtensao(o, BE_ZERO);
}

/* O destino ja existe (vreg definido, ou local em memoria) e tem a classe do tipo? Senao a instrucao nao grava um escalar que o traco conheca. */
static bool LocalPronto(const BirCtx *c, const IrInstr *i, uint8_t classe)
{
    int32_t vreg;

    if (i->destino < 0 || (uint32_t)i->destino >= c->n_local || c->tipo_local[i->destino] != i->tipo)
        return false;
    vreg = c->vreg_local[i->destino];
    return c->em_memoria[i->destino] || (vreg >= 0 && c->f->classe_vreg[vreg] == classe);
}

static bool TemporarioPronto(const BirCtx *c, const IrInstr *i, uint8_t classe)
{
    int32_t vreg;

    if (i->destino < 0 || (uint32_t)i->destino >= c->n_temp)
        return false;
    vreg = c->vreg_temp[i->destino];
    return vreg >= 0 && c->f->classe_vreg[vreg] == classe && (c->slot_temp == NULL || c->slot_temp[i->destino] < 0);
}

static bool DestinoPronto(const BirCtx *c, const IrInstr *i, bool e_local)
{
    uint8_t classe = BC_NADA;

    if (!BirClasseDoTipo(c->e->tipos, i->tipo, &classe))
        return false;
    return e_local ? LocalPronto(c, i, classe) : TemporarioPronto(c, i, classe);
}

/* forja_traco("fj_<proc>", instr, linha, coluna, bits): linha e coluna locais ao arquivo do .fj. */
static void ChamarTraco(BirCtx *c, const IrInstr *i, uint32_t indice, BirOperando bits)
{
    char nome[TRACO_NOME_MAX];
    BirOperando *args = BirArgs(c, TRACO_NARGS);
    FontePosicao pos = FonteLocalizarNoArquivo(c->e->fonte, i->pos);
    int n = snprintf(nome, sizeof nome, "fj_%s", InternerTexto(c->e->nomes, c->funcao_ir->nome));

    if (n >= (int)sizeof nome) /* snprintf devolve o tamanho que teria: o buffer guarda so sizeof - 1 bytes */
        n = (int)sizeof nome - 1;
    args[0] = OpVreg(BirVregDoTexto(c, nome, (uint32_t)n), BC_I64);
    args[1] = OpImed(indice, BC_I32);
    args[2] = OpImed(pos.linha, BC_I32);
    args[3] = OpImed(pos.coluna, BC_I32);
    args[4] = bits;
    BirChamarRuntime(c, &(BirChamadaRt){.funcao = "forja_traco", .retorno = BC_NADA, .nargs = TRACO_NARGS}, args);
}

/* O valor que o traco observa: o local ou temporario que a instrucao acabou de gravar. */
static bool ValorDoDestino(BirCtx *c, const IrInstr *i, bool e_local, BirOperando *valor)
{
    Valor destino = {.tipo_no = e_local ? V_LOCAL : V_TEMP, .tipo = i->tipo, .id = i->destino};

    return BirOperandoDe(c, i, &destino, valor);
}

/* Os bits que vao ao traco: real passa pelo slot, e tudo segue estendido a 64 SEM sinal. */
static BirOperando BitsDoTraco(BirCtx *c, BirOperando valor, bool real)
{
    return ComExtensaoZero(real ? BitsDoReal(c, valor) : valor);
}

bool BirTracar(BirCtx *c, const IrInstr *i)
{
    IrTracoForma forma = IrTracoFormaDe(c->e->tipos, i->tipo);
    BirOperando valor;
    bool e_local;
    uint32_t indice;

    if (forma.bits == 0 || !IrTracoDestino(i, &e_local) || !DestinoPronto(c, i, e_local))
        return true;
    indice = IrIndiceDe(&c->num, i);
    if (indice == IR_SEM_INDICE)
        return BirFalhaInconsistente(c, "instrucao fora da numeracao do traco");
    if (!ValorDoDestino(c, i, e_local, &valor))
        return false;
    ChamarTraco(c, i, indice, BitsDoTraco(c, valor, forma.real));
    return true;
}
