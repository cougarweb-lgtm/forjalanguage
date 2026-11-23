/* sem_tipo.c - resolucao dos tipos escritos no codigo (TipoRef), compatibilidade e consultas de struct/enum.
 * Versao: 0.22.0
 * Changelog: 0.22.0 - M6c P82: c_long/c_ulong por alvo (TipoCLong, fallback de SemResolverNomeTipo; Semantica.alvo_lp64).
 *            0.21.0 - M5 P48 (S18): ResolverDinamico recusa [dynamic]T de tipo que nao se copia (E0339, SemExigeCopiavel).
 *            0.20.0 - B01a: SemResolverNomeTipo registra a struct oculta do Allocator no 1o uso do nome (SemAllocatorGarantir); ResolverTipoRefInterno recusa Allocator fora do parametro alloc (E0336).
 *            0.19.0 - IA1-L4: 'T ! E' com dono dentro (lista, map, struct com dono) deixa de ser E0311: o valor se move no return (L-01).
 *            0.18.0 -IA1-L3d1: SemAcharMembro registra o membro de enum de forma antiga em Diag.trocas para o forja migrar.
 *            0.17.0 - IA1-L3c2: SemAcharMembro acha o membro de enum pela forma antiga da tabela de renomeados (DbError.nao_abriu = not_opened; flag recusar -> E0110) e SemMembroAusente (E0201).
 *            0.16.3 - IA1-L2: tipo nao declarado 'string'/'String' = E0110 (SemIdiomaDeTipo) antes do E0201.
 *            0.16.2 - IA1-b15: tipo nao declarado, ja relatado no corpo, sai calado (SemNomeJaRelatado).
 *            0.16.1 - IA1-b08: o E0311 de resultado 'T ! E' com lista leva a forma e a sugestao da ocorrencia (SemFormaDoDono).
 *            0.16.0 - IA1-b07: SemCoagirLento chama SemFormaTipoDiferente no E0301 (forma por ocorrencia).
 *            0.15.0 - Linguagem v1.12 M3 P21v-a: SemResolverTipoRef conta a profundidade (prof_tipo: proc so vale no topo do tipo) e SemCompativel aceita proc de mesma assinatura (TipoProcCompativel, sem nomes).
 *            0.14.0 - Linguagem v1.11 L15a: SemResolverCapacidade (N de [dynamic, N]T, E0335) e o ramo TR_LISTA_FIXA de SemResolverTipoRef.
 *            0.13.0 - Linguagem v1.11 L14a: SemResolverNomeTipo registra a struct oculta do quat no 1o uso do nome (SemQuatGarantir).
 *            0.12.0 -Linguagem v1.11 L9: TamanhoPorNome recusa com E0328 a const cujo init usa size_of/align_of/offset_of.
 *            0.11.0 - Linguagem v1.11 L8: nome de tipo, enum de erro e const de tamanho envenenados valem TIPO_VENENO / erro calado (sem_veneno.c).
 *            0.10.0- Linguagem v1.11 L1: SemCoagirLento acrescenta o extra "sugestao" ao E0301 numerico (SemSugerirConversao).
 *            0.9.0 - Linguagem v1.9 P2: SemResolverTipoRef despacha forma >= TR_MAPA para SemResolverTipoV19 (provisorio).
 *            0.9.0 (P3) - texto de TamanhoPorNome "[Enum]T fica para depois da v1.9"; bit_set[E] resolvido de verdade (via sem_v19.c).
 *            0.8.3 - Linguagem v1.8b P5a: TR_PROC resolve por SemResolverTipoProc (sai o E0101 provisorio do P2).
 *            0.8.2 - Linguagem v1.8b P3a: E0908 em SemResolverNomeTipo (opaco fora de ^/?^) e sob_ponteiro em ResolverFormaComposta.
 *            0.8.1 - Linguagem v1.8b P2: TR_PROC = E0101 provisorio "tipo proc(...) chega no P5a".
 *            0.8.0 - Linguagem v1.8 P3: SemResolverNomeTipo recebe o modulo (E1003 em tipo qualificado); ResolverErro (modulo_erro) e TamanhoPorNome conferem a visibilidade.
 *            0.7.4- Linguagem v1.7 (desempenho): SemCoagir virou inline em sem_interno.h (tipos iguais = sem chamada); o resto e SemCoagirLento.
 *            0.7.3 - Linguagem v1.7 P4: SemCompativel diz E0314 quando um lado e tupla; TR_TUPLA fora de retorno = FalhaInterna; E0311 de 'T ! E' aponta '-> (lista: [dynamic]T, erro: ?E)'.
 *            0.7.2 - Linguagem v1.7 P3: union e nome de tipo (SIM_UNIAO); SemCoagir Variante -> Union (E0313 se nao e variante); SemTipoTemStructPorValor.
 *            0.7.1- Linguagem v1.7 P2: TR_OPCIONAL resolve (E0312 em ??T, ?rawptr, ??^T e ?dono); SemCoagir (T -> ?T reescreve o no para EX_EMBRULHA). TR_TUPLA segue provisorio ("chega no P4").
 *            0.7.0 - Linguagem v1.7 P1: SemResolverTipoRef recusa TR_OPCIONAL/TR_TUPLA com E0101 "chega no P2/P4" (provisorio).
 *            0.6.0 - Linguagem v1.6 P2: TR_DINAMICO resolve o elemento (E0308 se 'T ! E'); 'T ! E' com valor dono = E0311; saem a recusa do P1a e o ramo "dynamic" de TamanhoPorNome.
 *            0.5.0 - Linguagem v1.6 P1a: SemResolverTipoRef recusa TR_DINAMICO com E0308 "chega no P2" (provisorio).
 *            0.4.0 - Linguagem v1.5 P2: SemResolverTipoRef resolve TR_ARRAY (N literal ou const inteira, 1..2147483647) e TR_FATIA;
 *                    elemento 'T ! E', N invalido, [Enum]T e [dynamic]T = E0308. (Saiu a recusa provisoria do P1a.)
 *            0.3.1 - Linguagem v1.3: '^T' sobre qualquer tipo e '?^T'; SemDeclDaStruct desce um nivel; SemCompativel aceita ^T -> ?^T (D8).
 *            0.3.0 - Linguagem v1.3: SemResolverTipoRef recursivo (TR_NOME/TR_PONTEIRO/TR_ANULAVEL); '^' so sobre struct e '?^T' recusado ate o P4; TIPO_DO_NOME ganha str, cstr, rawptr.
 *            0.2.0 - Linguagem v1.2: TipoEmbutido vira a tabela TIPO_DO_NOME (i8..f64, i64 e u64 como alias).
 *            0.1.0 - Fase 5c: i32/int/f32/bool por id fixo (fora da tabela, para 'proc f32' do corpus valer); categorias do prototipo (E0301 categoria, E0412 dimensao) sobre TipoId; '^T', 'T ! E' e '! E'.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include "common/renomeados.h"

#include <stdio.h>
#include <string.h>

/* Nomes de tipo embutidos (ids fixos do lexer, token.h) -> TipoId; 0 nos demais. i64 = int e u64 = uint (mesmo tipo). */
static const TipoId TIPO_DO_NOME[SYM_QTD] = {
    [SYM_I32] = TIPO_I32, [SYM_INT] = TIPO_INT, [SYM_I64] = TIPO_INT, [SYM_F32] = TIPO_F32, [SYM_BOOL] = TIPO_BOOL,
    [SYM_I8] = TIPO_I8, [SYM_I16] = TIPO_I16, [SYM_U8] = TIPO_U8, [SYM_U16] = TIPO_U16, [SYM_U32] = TIPO_U32,
    [SYM_U64] = TIPO_UINT, [SYM_UINT] = TIPO_UINT, [SYM_F64] = TIPO_F64,
    [SYM_STR] = TIPO_TEXTO, [SYM_CSTR] = TIPO_CSTR, [SYM_RAWPTR] = TIPO_RAWPTR,
};

static TipoId TipoEmbutido(int32_t nome)
{
    return nome >= 0 && nome < SYM_QTD ? TIPO_DO_NOME[nome] : TIPO_NAO_RESOLVIDO;
}

/* c_long/c_ulong = long do C: 32 bits no Windows (LLP64), 64 no Linux (LP64). So roda quando o nome nao tem simbolo (caminho de erro). */
static bool TipoCLong(const Semantica *s, int32_t nome, TipoId *tipo)
{
    const char *texto = SemNomeTexto(s, nome);
    if (strcmp(texto, "c_long") == 0) *tipo = s->alvo_lp64 ? TIPO_INT : TIPO_I32;
    else if (strcmp(texto, "c_ulong") == 0) *tipo = s->alvo_lp64 ? TIPO_UINT : TIPO_U32;
    else return false;
    return true;
}

bool SemResolverNomeTipo(Semantica *s, int32_t nome, uint32_t pos, uint8_t modulo, TipoId *tipo)
{
    if (__builtin_expect(nome == SYM_QUAT, 0)) SemQuatGarantir(s); /* v1.11 L14a: a struct oculta nasce no 1o uso do nome */
    if (__builtin_expect(nome == SYM_ALLOCATOR, 0)) SemAllocatorGarantir(s); /* B01a: idem para o Allocator */
    const Simbolo *y = SemAcharSimbolo(s, nome, modulo);
    bool eh_tipo =y->categoria == SIM_UNIDADE || y->categoria == SIM_STRUCT || y->categoria == SIM_ENUM || y->categoria == SIM_UNIAO;
    NomeUsado uso = {nome, pos, modulo};
    if (!SemVisivel(s, y, &uso)) return false;
    *tipo = TipoEmbutido(nome);
    if (*tipo != TIPO_NAO_RESOLVIDO) return true;
    if (eh_tipo) {
        if (__builtin_expect(y->envenenado, 0)) { /* L8: declaracao com erro; o uso nao repete o erro */
            *tipo = SemUsoDeVeneno(s);
            return true;
        }
        if (__builtin_expect(s->tem_opaco, 0) && y->categoria == SIM_STRUCT && y->decl->externo && !s->sob_ponteiro) {
            return SemErro(s, E0908, pos, "o tipo opaco '%s' (de foreign) nao tem tamanho conhecido na Forja: use ^%s ou ?^%s",
                           SemNomeTexto(s, nome), SemNomeTexto(s, nome), SemNomeTexto(s, nome));
        }
        *tipo = y->tipo;
        return true;
    }
    if (y->categoria == SIM_NENHUM) {
        if (modulo == 0 && TipoCLong(s, nome, tipo)) return true; /* M6c P82: c_long/c_ulong so quando o nome nao tem simbolo */
        if (SemNomeJaRelatado(s, nome)) return SemErroCalado(s, pos); /* b15: cascata do mesmo nome, dentro de um corpo */
        if (modulo == 0 && SemIdiomaDeTipo(s, nome, pos)) return false; /* IA1-L2: string, String = E0110 */
        return SemErro(s, E0201, pos, "tipo '%s' nao declarado", SemNomeTexto(s, nome));
    }
    return SemErro(s, E0201, pos, "'%s' nao e um tipo", SemNomeTexto(s, nome));
}

/* Depois de '!': o enum de erro. base = tipo do valor (TIPO_VAZIO em '! E'). */
static bool ResolverErro(Semantica *s, TipoRef *t, TipoId base)
{
    const Simbolo *y = SemAcharSimbolo(s, t->erro, t->modulo_erro);
    uint32_t pos = SemPosDoErro(s, t->pos);
    if (t->modulo_erro != 0) pos = SemPosDoNomeQualificado(s, pos); /* 'T ! apelido.E': o erro aponta o nome */
    NomeUsado uso = {t->erro, pos, t->modulo_erro};
    if (!SemVisivel(s, y, &uso)) return false;
    if (SemSimboloVeneno(y)) { /* L8: enum de erro com erro: o resultado vale TIPO_VENENO */
        t->tipo = SemUsoDeVeneno(s);
        return true;
    }
    if (y->categoria == SIM_NENHUM) {
        return SemErro(s, E0201, pos, "tipo de erro '%s' nao declarado", SemNomeTexto(s, t->erro));
    }
    if (y->categoria != SIM_ENUM) {
        return SemErro(s, E0605, pos, "'%s' nao e um enum; o tipo depois de '!' precisa ser um enum de erro",
                       SemNomeTexto(s, t->erro));
    }
    if (base == y->tipo) {
        return SemErro(s, E0609, pos, "o valor e o erro de '%s ! %s' sao o mesmo tipo '%s': um return desse enum fica ambiguo",
                       SemNomeTexto(s, t->erro), SemNomeTexto(s, t->erro), SemNomeTexto(s, t->erro));
    }
    t->tipo = TipoResultado(&s->tipos, base, y->tipo);
    return true;
}

/* '^T' e '?^T' valem sobre qualquer tipo. */
static bool ResolverFormaComposta(Semantica *s, TipoRef *t, TipoId *base)
{
    if (s->tem_opaco) s->sob_ponteiro = t->alvo->forma == TR_NOME; /* v1.8b: opaco so atras de ponteiro */
    bool alvo_ok = SemResolverTipoRef(s, t->alvo);
    s->sob_ponteiro = false;
    if (!alvo_ok) return false;
    *base = t->forma == TR_ANULAVEL ? TipoAnulavel(&s->tipos, t->alvo->tipo) : TipoPonteiro(&s->tipos, t->alvo->tipo);
    return true;
}

enum { TAMANHO_MAX_ARRAY = 2147483647 };

/* Digitos de um literal inteiro como N de '[N]T': 1..2147483647. Erros em 'pos'. */
static bool LerTamanho(Semantica *s, const char *digitos, uint32_t pos, uint32_t *n)
{
    uint64_t v;
    if (!TipoLerLiteralU64(digitos, &v) || v > TAMANHO_MAX_ARRAY) {
        return SemErro(s, E0308, pos, "tamanho maximo %d", (int)TAMANHO_MAX_ARRAY);
    }
    if (v == 0) return SemErro(s, E0308, pos, "array de tamanho 0 nao existe");
    *n = (uint32_t)v;
    return true;
}

/* N escrito como nome: so const inteira cujo init e um literal (sem conta); os outros usos de colchete tem erro proprio. */
static bool TamanhoPorNome(Semantica *s, const TipoRef *t, uint32_t *n)
{
    const Simbolo *y = SemAcharSimbolo(s, t->nome, 0);
    NomeUsado uso = {t->nome, t->pos_tamanho, 0}; /* D31: so nome sem apelido */
    if (!SemVisivel(s, y, &uso)) return false;
    if (SemSimboloVeneno(y)) return SemErroCalado(s, t->pos_tamanho); /* L8: const (ou enum) com erro nao da N */
    if (y->categoria == SIM_CONST) {
        const Expr *init = y->decl->init;
        if (SemUsaLayoutNoTexto(init)) {
            return SemErro(s, E0328, t->pos_tamanho, "a const '%s' usa size_of, align_of ou offset_of e esses valores nao valem no tamanho de [N]T: escreva o numero",
                           SemNomeTexto(s, t->nome));
        }
        if (init->tipo_no != EX_INT || !TipoEhInteiro(y->tipo)) {
            return SemErro(s, E0308, t->pos_tamanho, "a const '%s' precisa ser um literal inteiro (sem conta) para ser tamanho de array",
                           SemNomeTexto(s, t->nome));
        }
        return LerTamanho(s, s->fonte->texto + init->pos_op, t->pos_tamanho, n);
    }
    if (y->categoria == SIM_ENUM) return SemErro(s, E0308, t->pos_tamanho, "[Enum]T fica para depois da v1.9: use [N]T com uma const N ou map[Enum]T");
    return SemErro(s, E0308, t->pos_tamanho, "o tamanho do array e literal inteiro ou const inteira");
}

static bool ResolverTamanho(Semantica *s, const TipoRef *t, uint32_t *n)
{
    if (t->nome < 0) return LerTamanho(s, s->fonte->texto + t->pos_tamanho, t->pos_tamanho, n);
    return TamanhoPorNome(s, t, n);
}

enum { CAPACIDADE_MAX_LISTA_FIXA = 65536 };

/* v1.11 L15a: a capacidade N de '[dynamic, N]T' (literal ou const como no array), de 1 a 65536; fora disso E0335 (const igual a 0 cai no E0308 do array). */
bool SemResolverCapacidade(Semantica *s, const TipoRef *t, uint32_t *n)
{
    uint64_t v;
    const char *causa = "a capacidade de [dynamic, N]T vai de 1 a 65536";
    if (t->nome < 0) {
        if (!TipoLerLiteralU64(s->fonte->texto + t->pos_tamanho, &v) || v == 0 || v > CAPACIDADE_MAX_LISTA_FIXA) {
            return SemErro(s, E0335, t->pos_tamanho, "%s", causa);
        }
        *n = (uint32_t)v;
        return true;
    }
    if (!ResolverTamanho(s, t, n)) return false;
    return *n <= CAPACIDADE_MAX_LISTA_FIXA || SemErro(s, E0335, t->pos_tamanho, "%s", causa);
}

/* '[N]T' e '[]T': o elemento nao pode ser resultado; N vem de ResolverTamanho. */
static bool ResolverColecao(Semantica *s, TipoRef *t, TipoId *base)
{
    uint32_t n = 0;
    if (!SemResolverTipoRef(s, t->alvo)) return false;
    if (TipoClasseDe(&s->tipos, t->alvo->tipo) == TC_RESULTADO) {
        return SemErro(s, E0308, t->alvo->pos, "array de 'T ! E' nao existe: guarde o valor ou o erro");
    }
    if (t->forma == TR_FATIA) {
        *base = TipoFatia(&s->tipos, t->alvo->tipo);
        return true;
    }
    if (!ResolverTamanho(s, t, &n)) return false;
    *base = TipoArray(&s->tipos, t->alvo->tipo, n);
    return true;
}

/* '[dynamic]T': o elemento nao pode ser resultado. */
static bool ResolverDinamico(Semantica *s, TipoRef *t, TipoId *base)
{
    if (!SemResolverTipoRef(s, t->alvo)) return false;
    if (TipoClasseDe(&s->tipos, t->alvo->tipo) == TC_RESULTADO) {
        return SemErro(s, E0308, t->alvo->pos, "lista de 'T ! E' nao existe: guarde o valor ou o erro");
    }
    if (!SemExigeCopiavel(s, t->alvo->tipo, t->alvo->pos, "elemento de [dynamic] por valor: a lista guarda o valor")) return false;
    *base = TipoDinamico(&s->tipos, t->alvo->tipo);
    return true;
}

/* '?T' (v1.7): o alvo nao pode ja guardar "ausente" (??T, ?^T, rawptr) nem ser dono (a lista vazia ja e a ausencia). Erros em t->pos (o '?' de fora). */
static bool ResolverOpcional(Semantica *s, TipoRef *t, TipoId *base)
{
    char texto[96];
    if (!SemResolverTipoRef(s, t->alvo)) return false;
    TipoId alvo = t->alvo->tipo;
    TipoClasse classe = TipoClasseDe(&s->tipos, alvo);
    if (classe == TC_OPCIONAL) return SemErro(s, E0312, t->pos, "??T nao existe: ?T ja guarda um valor ou none");
    if (classe == TC_ANULAVEL) return SemErro(s, E0312, t->pos, "?^T ja aceita none: escreva ?^T, sem o 2o '?'");
    if (classe == TC_RAWPTR) return SemErro(s, E0312, t->pos, "rawptr ja aceita none: use rawptr");
    if (SemTipoEhDono(s, alvo)) {
        SemDescrever(s, alvo, texto, sizeof texto);
        return SemErro(s, E0312, t->pos, "?%s nao existe: a lista vazia (len 0) ja e a ausencia", texto);
    }
    *base = TipoOpcional(&s->tipos, alvo);
    return true;
}

static bool ResolverTipoRefInterno(Semantica *s, TipoRef *t)
{
    TipoId base = TIPO_VAZIO;
    if (t->forma == TR_DINAMICO) {
        if (!ResolverDinamico(s, t, &base)) return false;
    } else if (t->forma == TR_ARRAY || t->forma == TR_FATIA) {
        if (!ResolverColecao(s, t, &base)) return false;
    } else if (t->forma == TR_OPCIONAL) {
        if (!ResolverOpcional(s, t, &base)) return false;
    } else if (t->forma == TR_LISTA_FIXA) {
        if (!SemResolverListaFixa(s, t, &base)) return false;
    } else if (t->forma == TR_TUPLA) { /* so existe em Decl.retorno e quem a resolve e DeclProc (sem_decl.c) */
        FalhaInterna("semantica: TR_TUPLA fora de Decl.retorno");
    } else if (t->forma != TR_NOME) {
        if (__builtin_expect(t->forma >= TR_MAPA, 0)) {
            if (!SemResolverTipoV19(s, t, &base)) return false;
        } else if (!ResolverFormaComposta(s, t, &base)) {
            return false;
        }
    } else if (t->nome >= 0 && !SemResolverNomeTipo(s, t->nome, t->modulo != 0 ? t->pos_tamanho : t->pos, t->modulo, &base)) {
        return false;
    }
    if (__builtin_expect(s->tipo_allocator != 0 && base == s->tipo_allocator, 0) && !SemAllocatorForaDeParametro(s, t)) return false; /* B01a: Allocator so no parametro alloc */
    if (t->erro >= 0) return ResolverErro(s, t, base);
    t->tipo = base;
    return true;
}

/* TR_PROC resolve por SemResolverTipoProc (sem '! E'); os demais contam a profundidade: proc so vale no topo do tipo escrito (v1.12). */
bool SemResolverTipoRef(Semantica *s, TipoRef *t)
{
    if (t->forma == TR_PROC) return SemResolverTipoProc(s, t);
    s->prof_tipo++;
    bool ok = ResolverTipoRefInterno(s, t);
    s->prof_tipo--;
    return ok;
}

void SemDescrever(const Semantica *s, TipoId tipo, char *saida, size_t cap)
{
    Buf b = {0};
    TipoAnexarTexto(&s->tipos, tipo, &b);
    snprintf(saida, cap, "%s", b.dados);
    BufLiberar(&b);
}

/* v1.7 (D12): a tupla nao e valor de primeira classe; so o erro de tipo muda de texto (caminho de erro de SemCompativel). */
static bool ErroDeTupla(Semantica *s, TipoId esperado, TipoId recebido, uint32_t pos)
{
    char texto[96];
    if (TipoClasseDe(&s->tipos, recebido) == TC_TUPLA) {
        SemDescrever(s, recebido, texto, sizeof texto);
        return SemErro(s, E0314, pos, "a chamada devolve %s (varios valores): desestruture com let a, b = f()", texto);
    }
    return SemErro(s, E0314, pos, "retorno multiplo pede 'return a, b' com %u valores", TipoTuplaQtd(&s->tipos, esperado));
}

bool SemCompativel(Semantica *s, TipoId esperado, TipoId recebido, uint32_t pos)
{
    char texto_esperado[96], texto_recebido[96];
    if (esperado == recebido || recebido == TIPO_VENENO || esperado == TIPO_VENENO) return true;
    if (TipoProcCompativel(&s->tipos, esperado, recebido)) return true; /* v1.12: mesma assinatura, nomes de parametro a parte */
    /* D8: a unica coercao implicita da linguagem, ^T -> ?^T (mesmo apontado) */
    if (TipoClasseDe(&s->tipos, esperado) == TC_ANULAVEL && TipoClasseDe(&s->tipos, recebido) == TC_PONTEIRO &&
        TipoDe(&s->tipos, esperado)->a == TipoDe(&s->tipos, recebido)->a) {
        return true;
    }
    if (TipoClasseDe(&s->tipos, recebido) == TC_TUPLA || TipoClasseDe(&s->tipos, esperado) == TC_TUPLA) {
        return ErroDeTupla(s, esperado, recebido, pos);
    }
    SemDescrever(s, esperado, texto_esperado, sizeof texto_esperado);
    SemDescrever(s, recebido, texto_recebido, sizeof texto_recebido);
    if (TipoClasseDe(&s->tipos, esperado) == TC_REAL && TipoClasseDe(&s->tipos, recebido) == TC_REAL) {
        return SemErro(s, E0412, pos, "dimensoes diferentes: esperado %s, recebido %s", texto_esperado, texto_recebido);
    }
    return SemErro(s, E0301, pos, "tipo diferente: esperado %s, recebido %s", texto_esperado, texto_recebido);
}

/* Mesma regra de SemCompativel sem relatar o erro: igualdade (o ^T -> ?^T nunca vale sob '?T', pois ?^T nao e TC_OPCIONAL). */
static bool CompativelSemErro(TipoId esperado, TipoId recebido) { return esperado == recebido; }

/* D3: embrulha 'e' no lugar. O no original vai para a arena e 'e' vira EX_EMBRULHA (a = original, op = tag da variante). */
static void Embrulhar(Semantica *s, Expr *e, TipoId tipo, uint8_t tag)
{
    Expr *dentro = ArenaAlocar(s->arena, sizeof *dentro);
    *dentro = *e;
    e->tipo_no = EX_EMBRULHA;
    e->a = dentro;
    e->b = NULL;
    e->args = NULL;
    e->nargs = 0;
    e->flexivel = false;
    e->ref = REF_NENHUM;
    e->ref_id = 0;
    e->op = tag;
    e->tipo = tipo;
}

/* Indice (ordem do fonte) da variante da union cujo tipo e 'tipo'; -1 se nao e variante. */
int32_t SemVarianteDe(const Semantica *s, TipoId uniao, TipoId tipo)
{
    const Decl *d = s->globais[TipoDe(&s->tipos, uniao)->a].decl;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (d->campos[i].tipo.tipo == tipo) return (int32_t)i;
    }
    return -1;
}

/* Lista "A, B, C" das variantes da union (corta em 160 bytes). */
static void ListarVariantes(const Semantica *s, TipoId uniao, char *saida, size_t cap)
{
    const Decl *d = s->globais[TipoDe(&s->tipos, uniao)->a].decl;
    char um[96];
    size_t usado = 0;
    saida[0] = '\0';
    for (uint32_t i = 0; i < d->ncampos && usado < cap; i++) {
        SemDescrever(s, d->campos[i].tipo.tipo, um, sizeof um);
        int n = snprintf(saida + usado, cap - usado, "%s%s", i == 0 ? "" : ", ", um);
        if (n < 0) break;
        usado += (size_t)n;
    }
}

/* D3: Variante -> Union; recebido que nao e variante = E0313 em pos. */
static bool CoagirParaUniao(Semantica *s, Expr *e, TipoId esperado, uint32_t pos)
{
    char recebido[96], nome[96], lista[160];
    if (e->tipo == TIPO_VENENO) return true;
    int32_t k = SemVarianteDe(s, esperado, e->tipo);
    if (k >= 0) {
        Embrulhar(s, e, esperado, (uint8_t)k);
        return true;
    }
    SemDescrever(s, e->tipo, recebido, sizeof recebido);
    SemDescrever(s, esperado, nome, sizeof nome);
    ListarVariantes(s, esperado, lista, sizeof lista);
    return SemErro(s, E0313, pos, "o valor de tipo %s nao e variante da union '%s' (variantes: %s)", recebido, nome, lista);
}

bool SemCoagirLento(Semantica *s, Expr *e, TipoId esperado, uint32_t pos)
{
    const TipoInfo *k = TipoDe(&s->tipos, esperado);
    if (k->classe == TC_OPCIONAL && CompativelSemErro((TipoId)k->a, e->tipo)) {
        Embrulhar(s, e, esperado, 0);
        return true;
    }
    if (k->classe == TC_UNIAO) return CoagirParaUniao(s, e, esperado, pos);
    if (SemCompativel(s, esperado, e->tipo, pos)) return true;
    if (s->diag->codigo == E0301 && s->diag->off == pos) {
        SemSugerirConversao(s, e, esperado); /* v1.11: o 'as' que resolve */
        SemFormaTipoDiferente(s, e, esperado); /* b07: contexto, tipos, nomes e a linha corrigida */
    }
    return false;
}

/* D10: o tipo guarda struct por valor (direto, em array, em ?T ou em alguma variante de union). */
bool SemTipoTemStructPorValor(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe == TC_STRUCT) return true;
    if (k->classe == TC_ARRAY || k->classe == TC_OPCIONAL) return SemTipoTemStructPorValor(s, (TipoId)k->a);
    if (k->classe != TC_UNIAO) return false;
    const Decl *d = s->globais[k->a].decl;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (SemTipoTemStructPorValor(s, d->campos[i].tipo.tipo)) return true;
    }
    return false;
}

Decl *SemDeclDaStruct(const Semantica *s, TipoId tipo)
{
    const TipoInfo *k = TipoDe(&s->tipos, tipo);
    if (k->classe == TC_PONTEIRO) { /* desce UM nivel so: '^^T' nao tem campo direto */
        const TipoInfo *alvo = TipoDe(&s->tipos, k->a);
        return alvo->classe == TC_STRUCT ? s->globais[alvo->a].decl : NULL;
    }
    return k->classe == TC_STRUCT ? s->globais[k->a].decl : NULL;
}

int32_t SemAcharCampo(const Decl *estrutura, int32_t nome)
{
    for (uint32_t i = 0; i < estrutura->ncampos; i++) {
        if (estrutura->campos[i].nome == nome) return (int32_t)i;
    }
    return -1;
}

static int32_t AcharMembroExato(const Decl *enumeracao, int32_t nome)
{
    for (uint32_t i = 0; i < enumeracao->nmembros; i++) {
        if (enumeracao->membros[i].nome == nome) return (int32_t)i;
    }
    return -1;
}

/* L3c2: a entrada de REN_MEMBRO cujo escopo tem o caminho do modulo do enum e o nome (novo) do enum, e que tem 'nome' (so a forma antiga interessa); NULL = nao ha. */
static const Renomeado *MembroRenomeado(const Semantica *s, const Decl *enumeracao, int32_t nome)
{
    const char *palavras[3] = {NULL, NULL, NULL};
    bool era_antigo = false;
    const Renomeado *r;

    if (s->modulos == NULL || enumeracao->modulo >= s->modulos->n) return NULL;
    palavras[0] = InternerTexto(s->nomes, s->modulos->itens[enumeracao->modulo].caminho);
    palavras[1] = SemNomeTexto(s, enumeracao->nome);
    r = RenomeadoAcharEm(REN_MEMBRO, palavras, SemNomeTexto(s, nome), &era_antigo);
    return r != NULL && era_antigo ? r : NULL;
}

int32_t SemAcharMembro(Semantica *s, const Decl *enumeracao, int32_t nome, uint32_t pos)
{
    int32_t idx = AcharMembroExato(enumeracao, nome);
    const Renomeado *r;

    if (idx >= 0) return idx;
    r = MembroRenomeado(s, enumeracao, nome);
    if (r == NULL) return -1;
    idx = AcharMembroExato(enumeracao, InternerAchar(s->nomes, r->novo, (uint32_t)strlen(r->novo)));
    if (idx >= 0) RenomeadoNotarTabela(s->diag, pos, InternerTam(s->nomes, nome), r);
    if (idx >= 0 && RenomeadoRecusa(r)) {
        RenomeadoRecusar(s->diag, pos, InternerTam(s->nomes, nome), r);
        return SEM_MEMBRO_RECUSADO;
    }
    return idx;
}

bool SemMembroAusente(Semantica *s, const Expr *e, const Decl *enumeracao, int32_t idx)
{
    SemParecido sp;

    if (idx == SEM_MEMBRO_RECUSADO) return false;
    SemParecidoIniciar(s, E0201, e->pos_op, e->nome, &sp);
    SemParecidosDeMembros(s, enumeracao, &sp);
    return SemErroParecido(s, &sp, "o enum '%s' nao tem o membro '%s'", SemNomeTexto(s, e->a->nome), SemNomeTexto(s, e->nome));
}
