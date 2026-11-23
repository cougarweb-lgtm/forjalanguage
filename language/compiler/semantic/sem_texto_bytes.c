/* sem_texto_bytes.c - embutidos de texto sobre str e bytes (IA1-L9e, T10): clone(str), from_bytes e as_bytes.
 * Sintaxe (argumento posicional na ordem, ou nomeado em ordem crescente):
 *   clone(s, alloc = heap) -> str         copia os bytes de s para memoria nova; o chamador da delete (o clone de lista, do S1b, continua o mesmo nome: aqui vale se s e str)
 *   from_bytes(b, alloc = heap) -> str    str dona com a COPIA dos bytes de b ([]u8, [dynamic]u8 ou [N]u8); NAO valida UTF-8 (os bytes entram como vierem); delete(s) depois
 *   as_bytes(s) -> []u8                   VISTA dos mesmos bytes de s, sem copia e sem dono: so leitura (escrever nela e erro de execucao nao detectado, como em todo str
 *                                         literal) e vale enquanto s valer; o len e o do texto em bytes (UTF-8 multibyte conta cada byte)
 * clone e from_bytes reusam o caminho do format: o 1o argumento vira o f"{x}" de uma parte so (SemFTextoDoArgumento) e o alocador entra como no format. O lowering
 * (lower_texto_bytes.c) so emite IR_FORMATA; no from_bytes a parte e um transmute sintetico de []u8 para str (so 1 argumento, o lowering le so o valor).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9e: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

/* clone(str, alloc = A): alvo ja checado, e str. */
bool SemChamarCloneTexto(Semantica *s, Expr *e)
{
    if (!SemConferirAlocador(s, e, ArgumentoDoParametro(e, 1)) || !SemFTextoDoArgumento(s, e)) return false;
    e->tipo = TIPO_STR;
    return true;
}

/* O []u8 'bytes' visto como str (IR_TRANSMUTA): o valor que o f"{...}" do from_bytes copia. */
static Expr *VistaDeTexto(Semantica *s, const Expr *chamada, Expr *bytes)
{
    Expr *vista = ArenaAlocar(s->arena, sizeof *vista);
    Arg *argumento = ArenaAlocar(s->arena, sizeof *argumento);
    *argumento = (Arg){.nome = -1, .pos_nome = bytes->pos, .valor = bytes, .indice = 0};
    vista->tipo_no = EX_CHAMADA;
    vista->ref = REF_EMBUTIDO;
    vista->ref_id = EMB_TRANSMUTE;
    vista->nome = chamada->nome;
    vista->pos = bytes->pos;
    vista->pos_op = chamada->pos_op;
    vista->args = argumento;
    vista->nargs = 1;
    vista->tipo = TIPO_STR;
    return vista;
}

/* from_bytes(b, alloc = A): b vira fatia de u8 (lista, array e fatia entram). */
bool SemChamarFromBytes(Semantica *s, Expr *e)
{
    char texto[96];
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_BYTES], 2)) return false;
    if (!SemChecar(s, ArgumentoDoParametro(e, 0)->valor, TIPO_VAZIO)) return false;
    if (!SemColecaoJaChecada(s, e, 0, false, &elemento)) return false;
    if (elemento != TIPO_U8) {
        SemDescrever(s, elemento, texto, sizeof texto);
        return SemErro(s, E0301, e->pos_op, "from_bytes pede lista, array ou fatia de u8 (elemento %s)", texto);
    }
    if (!SemConferirAlocador(s, e, ArgumentoDoParametro(e, 1))) return false;
    e->args[0].valor = VistaDeTexto(s, e, e->args[0].valor);
    if (!SemFTextoDoArgumento(s, e)) return false;
    e->tipo = TIPO_STR;
    return true;
}

/* as_bytes(s): s e str. */
bool SemChamarAsBytes(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_TEXTO], 1)) return false;
    Expr *alvo = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, alvo, TIPO_VAZIO)) return false;
    if (alvo->tipo != TIPO_STR) {
        SemDescrever(s, alvo->tipo, texto, sizeof texto);
        return SemErro(s, E0301, alvo->pos, "as_bytes pede str (tipo %s)", texto);
    }
    e->tipo = TipoFatia(&s->tipos, TIPO_U8);
    return true;
}
