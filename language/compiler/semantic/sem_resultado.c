/* sem_resultado.c - 'try' e 'or_else' sobre valores resultado (T ! E).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - IA1-L4: 'T ! E' com T dono: try confere o movimento (E0311 se copia de lvalor) e or_else e E0604 (o padrao alocaria a toa).
 *            0.2.0 - Linguagem v1.7 P2: or_else aceita ?T e ?^T; try sobre ?T/?^T = E0606 com causa propria.
 *            0.1.0 - Fase 5c: try so em proc cujo retorno e resultado com o mesmo E; or_else com valor do tipo T.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

bool SemTry(Semantica *s, Expr *e)
{
    char texto[96], texto_erro[96], texto_proc[96];
    if (s->erro_do_proc == 0) {
        return SemErro(s, E0602, e->pos, "try so vale dentro de proc com retorno '! E' ou 'T ! E'");
    }
    if (!SemChecar(s, e->a, TIPO_VAZIO)) return false;
    SemDescrever(s, e->a->tipo, texto, sizeof texto);
    TipoClasse classe = TipoClasseDe(&s->tipos, e->a->tipo);
    if (classe == TC_OPCIONAL || classe == TC_ANULAVEL) {
        return SemErro(s, E0606, e->a->pos, "try nao se aplica a %s: use or_else ou switch (try so repassa o erro de 'T ! E')", texto);
    }
    if (classe != TC_RESULTADO) {
        return SemErro(s, E0606, e->a->pos, "try exige um valor resultado ('T ! E'), recebeu %s", texto);
    }
    const TipoInfo resultado = *TipoDe(&s->tipos, e->a->tipo);
    if (!SemConferirMovimento(s, e->a)) return false; /* L-01: o dono sai do resultado; de um lvalor seria copia (E0311) */
    if (resultado.b != s->erro_do_proc) {
        SemDescrever(s, resultado.b, texto_erro, sizeof texto_erro);
        SemDescrever(s, s->erro_do_proc, texto_proc, sizeof texto_proc);
        return SemErro(s, E0603, e->pos, "o erro %s do try e diferente do erro %s do retorno do proc", texto_erro, texto_proc);
    }
    e->tipo = resultado.a;
    return true;
}

/* Tipo do valor que o or_else devolve: o de ?T, o ^T de ?^T ou o T de 'T ! E'. Erros em e->a->pos. */
static bool ValorDoOrElse(Semantica *s, const Expr *esquerda, TipoId *valor)
{
    char texto[96];
    const TipoInfo k = *TipoDe(&s->tipos, esquerda->tipo); /* copia: TipoPonteiro pode crescer a tabela */
    SemDescrever(s, esquerda->tipo, texto, sizeof texto);
    if (k.classe == TC_OPCIONAL) *valor = k.a;
    else if (k.classe == TC_ANULAVEL) *valor = TipoPonteiro(&s->tipos, k.a);
    else if (k.classe != TC_RESULTADO) {
        return SemErro(s, E0606, esquerda->pos, "or_else exige ?T, ?^T ou resultado ('T ! E') a esquerda, recebeu %s", texto);
    } else if (k.a == TIPO_VAZIO) {
        return SemErro(s, E0604, esquerda->pos, "or_else precisa de um resultado com valor, mas %s so tem erro", texto);
    } else if (SemTipoEhDono(s, k.a)) {
        return SemErro(s, E0604, esquerda->pos, "or_else sobre %s alocaria o padrao a toa: o valor e dono; use try ou 'switch r in f()'", texto);
    } else *valor = k.a;
    return true;
}

bool SemOrElse(Semantica *s, Expr *e)
{
    char texto[96], texto_valor[96];
    TipoId valor = TIPO_VAZIO;
    if (!SemChecar(s, e->a, TIPO_VAZIO) || !ValorDoOrElse(s, e->a, &valor)) return false;
    if (!SemChecar(s, e->b, valor)) return false;
    if (e->b->tipo != valor) {
        SemDescrever(s, valor, texto_valor, sizeof texto_valor);
        SemDescrever(s, e->b->tipo, texto, sizeof texto);
        return SemErro(s, E0604, e->b->pos, "o padrao do or_else deve ser %s, recebeu %s", texto_valor, texto);
    }
    e->tipo = valor;
    return true;
}
