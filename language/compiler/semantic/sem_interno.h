/* sem_interno.h - funcoes compartilhadas entre os arquivos da semantica (nome, tipo, expressao, chamada, comando, fluxo, decl).
 * Versao: 0.56.0
 * Changelog: 0.56.0 - Linguagem v1.13 L1a: SemEntrarNaDecl (grava modulo_atual e arquivo_atual).
 *            0.55.0 - M6e P93g: SemEmbutidoMutaColecao (inline) e SemEmprestimoCorpo (sem_emprestimo.c, E0913).
 *            0.54.0 - M6c P88: SemDestruidorDoTipo publica (sem_posse_handle.c).
 *            0.53.0 - M6c P87: E0317 de handle local (sem_posse_handle.c): ALOC_HANDLE, POSSE_GUARDADO, PONTO_EMPRESTIMO(_FORJA), LocalRastreado.tipo; SemPosseDosArgumentos recebe o proc chamado.
 *            0.52.0 - M6c P85: SemChecarPosseC (sem_posse_c.c, E0912).
 *            0.51.0 - M6c P83: SemCLongDecls e SemCLongCorpo (sem_clong.c, E0342).
 *            0.50.0 - M6 P65a: SemGravarNomeLocal (tabela de nomes por local_id, D-M6-13).
 *            0.49.0 - M5 P48 (S18): prototipos de sem_naocopia.c (SemTipoEhNaoCopiavel, SemConferirNaoCopiavel, SemExigeCopiavel, SemRegistrarNaoCopiavel); SemConferirMovimento confere a copia nao copiavel.
 *            0.48.0 - S3 P4: SemLibmNotarDecl e SemLibmNotarChamada (sem_math_std.c; migracao libm do forja migrar).
 *            0.47.0 - S3 P3: SemMathChamada (sem_math_std.c).
 *            0.46.0 - B01h: SemOrigemRestritiva e SemOrigemMaisRestritiva (origem de dois ramos); SemOrigemDoValor entra em EX_TRY (origem de e->a) e EX_OR_ELSE (restritiva vence).
 *            0.45.0 - B01g: SemOrigemDoValor devolve ALOC_HEAP tambem para chamada de proc do usuario com alloc = heap (o resultado e rastreado como o de new/make: E0317); LocalRastreado.de_chamada.
 *            0.44.0 - B01d: SemAllocatorInserirNoMigrar (sem_allocator.c).
 *            0.43.0 - B01c: SemAllocatorValorSolto, SemAllocatorConverterArg, SemAllocatorOrigemDaChamada, SemAlocacaoDoNome; SemOrigemDoValor nao le ALOC_HEAP de chamada de proc do usuario (o E0317 dele e da B01g).
 *            0.42.0 - B01b: SemAllocatorNomeForaDeParametro (sem_allocator.c).
 *            0.41.0 - B01a: SemAllocatorGarantir, SemAllocatorConferirParam, SemAllocatorForaDeParametro, SemAllocatorLerCampo, SemAllocatorLiteral e SemEhAllocator (sem_allocator.c).
 *            0.40.0 - IA1-L9e: SemEstatInternar, SemEstatRegistrar e SemChamarEstat (sem_estat.c).
 *            0.39.0 - IA1-L9c/L9d: SemListaInternar, SemListaRegistrar, SemListaMontar e SemChamarLista (sem_lista.c).
 *            0.38.0 - IA1-L9a/L9b: SemTiparArgumentos (sem_matematica.c), SemAritmeticaInternar, SemAritmeticaRegistrar e SemChamarAritmetica (sem_aritmetica.c).
 *            0.37.0 - IA1-L5b: SemChamarResize e SemChamarSort (sem_memoria.c).
 *            0.36.0 - IA1-L5a: SemFTextoDoArgumento (sem_formato.c).
 *            0.35.0 - IA1-L4: sai DONO_RESULTADO (resultado com dono dentro deixou de ser E0311).
 *            0.34.0 - IA1-L8: SemChavesPreencher (sem_chave.c).
 *            0.33.0 - IA1-L3c2: SemAcharMembro recebe a Semantica e a posicao (forma antiga de membro de enum); SEM_MEMBRO_RECUSADO e SemMembroAusente.
 *            0.32.0 - IA1-L2: prototipos de sem_idioma.c (SemIdiomaDeNome, SemIdiomaDeTipo, SemIdiomaDeChamada, SemIdiomaDeCampo): E0110 de nil/null/None, True, string, range, console.log, x.len(), x.push(v).
 *            0.31.0 - IA1-b15: SemRecuperar (sem_stmt.c) e SemNomeJaRelatado (sem_veneno.c): varios erros por corpo e cascata de nome sem declaracao calada.
 *            0.30.0 - IA1-b14: SemRelacionar e SemRelacionarParecidos (sem_relacionados.c): o campo "relacionados" do pacote do erro.
 *            0.29.0 - IA1-b08: SemTrechoDoFonte e SemLinhaDoFonte (sem_forma.c, exportados), SemFormaContaDeTexto (E0306 de texto em conta) e SemFormaDoDono (sem_dono.c: forma e sugestao do E0311).
 *            0.28.0 - IA1-b07: Conversao, SemConversaoObvia (sem_literal.c) e os prototipos de sem_forma.c (SemFormaTipoDiferente, SemFormaDoValor, SemFormaOperandos, SemFormaDoPrint, SemFormaDaRegra).
 *            0.27.0 - IA1-b06: prototipos de sem_sugestao.c (SemSugerir*, TrocaDeNome): sugestoes aplicaveis do lote 2 (E0102, E0201, E1003, E0202, E0307, E0310, E0311, E0502).
 *            0.26.0- IA1-b04: SemHigienePropor devolve bool (true = o achado ficou) e o achado de higiene ganha a sugestao (SemHigieneSugerir, SemHigieneEditar).
 *            0.25.0 - Linguagem v1.12 M3 P21v-a: prototipos de proc como valor (SemMarcarProcValor, SemConferirProcsDaStruct, SemConverterEmChamadaInd, SemChamadaInd, SemProcSemZero) e SemChamarComParams, SemMotivoDoProc.
 *            0.24.0 -Linguagem v1.11 L15a: prototipos de sem_lista_fixa.c (SemListasFixasIniciar, SemResolverListaFixa, SemResolverCapacidade, SemLiteralListaFixa, SemEhListaFixaOuPonteiro, SemErroDeListaFixa).
 *            0.23.1 - Linguagem v1.11 L14b: SemAbsDeQuat recebe a Expr da chamada (abs(q) existe: e->tipo = f32).
 *            0.23.0 - Linguagem v1.11 L14a: prototipos de sem_quat.c (SemQuatGarantir, SemContaQuat, SemComparacaoQuat, SemChamarConj, SemAbsDeQuat) e SemEhQuat.
 *            0.22.0 -Linguagem v1.11 L13: prototipo de SemChamarMatematica (sem_matematica.c).
 *            0.21.0 -Linguagem v1.11 L12a: SemAritmetica (publica em sem_expr.c) e prototipos de sem_vetor.c (SemContaVetor, SemNegarVetor, SemCompostaVetor).
 *            0.20.0 -Linguagem v1.11 L10a: prototipos de sem_higiene.c (SemHigiene*) e SemRaizDaCadeia (sem_stmt.c).
 *            0.19.0 -Linguagem v1.11 L9: prototipos de sem_layout.c (SemChamarLayout, SemConferirStaticAssert, SemDeclAssert, SemUsaLayoutNoTexto).
 *            0.18.0 - Linguagem v1.11 L8: SemSimboloVeneno, SemUsoDeVeneno (inline) e prototipos de sem_veneno.c (SemEnvenenar, SemErroCalado, SemVenenarCampos).
 *            0.17.0- Linguagem v1.11 L7: SemGuardarErro (lista de diagnosticos).
 *            0.16.0 -Linguagem v1.11 L5: SemParecido e prototipos de sem_parecido.c (SemParecidoIniciar, SemParecidosDe*, SemErroParecido).
 *            0.15.0 - Linguagem v1.11 L3: SemFugaTipoTemPonteiro e SemMarcarPonteiroDoCorpo (inline).
 *            0.14.0 -Linguagem v1.11 L1: prototipos SemLiteralInteiroEmReal e SemSugerirConversao (sem_literal.c).
 *            0.13.0 (P10) - prototipos de sem_mapa.c (ResolverMapa, SemChamarMapa, SemForMapa, SemConferirMapaNoFor) e ArgumentoDoParametro (deixa de ser static em sem_v19.c).
 *            0.13.0 (P7) - prototipos de sem_soa.c (ResolverSoa, SemLiteralSoa, SemIndiceSoa, SemColunaDaSoa, SemForSoa) e SemColecaoDona.
 *            0.13.0 (P5) - prototipos de sem_matriz.c (ResolverMatriz, SemLiteralMatriz, SemIndiceMatriz, SemContaMatriz, SemChamarTranspose).
 *            0.13.0 - Linguagem v1.9 P2: prototipos SemResolverTipoV19 e SemIndiceDuplo (sem_v19.c, provisorios).
 *            0.13.0 (P3) - prototipos dos despachos de sem_v19.c (SemIndiceV19, SemLiteralV19, SemContaV19, SemComparacaoV19, SemChamarV19, SemForV19, SemCampoV19, SemEhTipoV19, SemTipoTemV19), de sem_bitset.c e CorpoDeLaco (sem_stmt.c, deixa de ser static).
 *            0.12.0 - Linguagem v1.8b P6: SemOrigemDoValor le alocacao em toda chamada (ORIGEM_C de proc foreign; chamada de proc da Forja tem 0).
 *            0.11.0 - Linguagem v1.8b P5a: prototipos SemResolverTipoProc e SemProcComoCallback (sem_callback.c).
 *            0.10.0 - Linguagem v1.8b P4: prototipo SemStructExigeLiteral.
 *            0.9.0 - Linguagem v1.8b P3a: SemTipoEhOpaco (inline).
 *            0.8.2 - Linguagem v1.8 P7: tipos e prototipos da posse (sem_posse.c) e os inlines de caminho quente (rastreados.n == 0).
 *            0.8.1 - Linguagem v1.8 P6a: NovoLocal.origem, SemOrigemDoValor (inline) e SemConferirAlocador(s, chamada, em).
 *            0.8.0 - Linguagem v1.8 P3: NomeUsado, SemVisivel (inline, caminho rapido com 1 modulo) e SemVisivelLento (E1003); SemResolverNomeTipo ganha o modulo.
 *            0.7.3- Linguagem v1.7 (desempenho): SemCoagir inline com SemCoagirLento; SemChecarModificadores(s, categorias).
 *            0.7.2 - Linguagem v1.7 P5a: prototipos SemFTexto, SemChamarFormato e SemConferirAlocador.
 *            0.7.1 - Linguagem v1.7 P3: prototipo SemTipoTemStructPorValor.
 *            0.7.0- Linguagem v1.7 P2: prototipos SemCoagir e SemTipoDoCaso; inline SemCtxSemOpcional.
 *            0.6.1 - Linguagem v1.6 P4 (desempenho): SemConferirMovimento vira inline com atalho (Tipos.tem_dinamico).
 *            0.6.0 - Linguagem v1.6 P4: ColecaoForma += FORMA_DINAMICO, FORMA_PONTEIRO_DINAMICO; inline FormaTemTamanhoFixo.
 *            0.5.0 - Linguagem v1.6 P3a: prototipos SemLigarEmbutido, SemChamarMemoria e SemTipoSolto.
 *            0.4.0 - Linguagem v1.6 P2: prototipos de sem_memoria.c (SemTipoEhDono, SemConferirMovimento).
 *            0.3.0 - Linguagem v1.5 P3a: prototipos de sem_array.c e SemInteiroConstante.
 *            0.2.1 - Linguagem v1.4 P2: SemChecarExterno.
 *            0.2.0 - Linguagem v1.3: TipoRefSoErro (TipoRef recursivo).
 *            0.1.0 - Fase 5c: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#ifndef FORJA_SEM_INTERNO_H
#define FORJA_SEM_INTERNO_H

#include "common/parecido.h"
#include "frontend/token.h"
#include "semantic/semantic.h"

/* ---- "voce quis dizer" (sem_parecido.c, v1.11 L5): erro + candidatos parecidos; so os caminhos de erro montam isto ---- */
typedef struct { Parecidos busca; DiagCodigo codigo; uint32_t pos; } SemParecido;
void SemParecidoIniciar(const Semantica *s, DiagCodigo codigo, uint32_t pos, int32_t nome, SemParecido *sp);
/* modulo = 0: locais visiveis e globais visiveis (embutidos incluidos); modulo k: pub do modulo k (apelido.nome). */
void SemParecidosDeNome(const Semantica *s, uint8_t modulo, SemParecido *sp);
void SemParecidosDeCampos(const Semantica *s, const Decl *estrutura, SemParecido *sp);
void SemParecidosDeMembros(const Semantica *s, const Decl *enumeracao, SemParecido *sp);
void SemParecidosDeParams(const Semantica *s, const Param *params, uint32_t nparams, SemParecido *sp);
/* Erro com a causa do fmt; com candidatos, acrescenta " (parecido: 'a', 'b')" e o extra "parecidos" depois de forma_certa. */
bool SemErroParecido(Semantica *s, const SemParecido *sp, const char *fmt, ...) __attribute__((format(gnu_printf, 3, 4)));

/* ---- relacionados do pacote do erro (sem_relacionados.c, IA1-b14) ---- */
/* Preenche Diag.rel do diagnostico pendente (declaracao, tipo, assinatura, uso anterior) para E0201, E0202, E0301, E0310, E0311, E0502..E0504, E0607 e E1003; os outros
 * codigos ficam como estao. Chamado por SemErro e SemErroParecido, logo depois do DiagRelatar; sem relacionado especifico acrescenta a declaracao que contem o erro. */
void SemRelacionar(Semantica *s);
/* Os candidatos do "voce quis dizer" que tem posicao viram relacionados "parecido", na ordem do ranking (menor distancia primeiro). */
void SemRelacionarParecidos(Semantica *s, const Parecidos *busca);

/* ---- nucleo (semantic.c) ---- */
bool SemErro(Semantica *s, DiagCodigo codigo, uint32_t pos, const char *fmt, ...) __attribute__((format(gnu_printf, 4, 5)));
const char *SemNomeTexto(const Semantica *s, int32_t nome);
bool SemGuardarErro(Semantica *s, uint8_t modulo); /* L7: guarda o erro pendente de s->diag na lista (modulo = Decl.modulo) e libera s->diag; false = lista cheia */

/* ---- veneno e cascata calada (sem_veneno.c, v1.11 L8; regra no cabecalho do arquivo) ---- */
/* Simbolo sem tipo usavel: declaracao com erro (struct, enum, unit, union, const) ou proc de assinatura invalida (tipo TIPO_VENENO).
 * Proc com erro so no corpo (envenenado, tipo TIPO_VAZIO) continua usavel. */
static inline bool SemSimboloVeneno(const Simbolo *y) { return y->tipo == TIPO_VENENO || (y->envenenado && y->categoria != SIM_PROC); }
/* Uso de um simbolo envenenado: liga o toque (o erro desta declaracao sera calado) e devolve o tipo que o uso passa a ter. */
static inline TipoId SemUsoDeVeneno(Semantica *s)
{
    s->tocou_veneno = true;
    return TIPO_VENENO;
}
void SemEnvenenar(Semantica *s, Decl *d);              /* marca a declaracao (com erro) como envenenada; nome nao registrado entra na tabela */
bool SemErroCalado(Semantica *s, uint32_t pos);        /* false com diagnostico calado: o uso de um simbolo envenenado nao tem como seguir */
void SemVenenarCampos(Decl *d, uint32_t de);           /* campos/variantes nao resolvidos a partir de 'de' viram TIPO_VENENO */
/* IA1-b15: true se o nome sem declaracao ja foi relatado neste corpo (o uso seguinte e cascata: quem chama devolve SemErroCalado); false regista o nome.
 * Com o veneno ja tocado nada se regista (o erro vai sair calado e o nome continuaria sem relato). */
bool SemNomeJaRelatado(Semantica *s, int32_t nome);
/* IA1-b15: o comando falhou: guarda (ou cala, se tocou o veneno) o diagnostico pendente e deixa o bloco seguir. false = lista cheia (a analise para). */
bool SemRecuperar(Semantica *s);

/* ---- nomes (sem_nome.c) ---- */
typedef enum { CAIXA_PASCAL, CAIXA_MAIUSCULA, CAIXA_SNAKE } CaixaNome;
typedef struct { int32_t nome; uint32_t pos; TipoId tipo; uint8_t natureza; uint8_t origem; } NovoLocal;

/* B01h: origem que nao e heap (PARAM, ARENA, FRAME, POOL, texto literal ou de foreign). Heap e 0 nao sao restritivas. */
static inline bool SemOrigemRestritiva(uint8_t origem)
{
    return origem == ALOC_PARAM || origem == ALOC_ARENA || origem == ALOC_FRAME || origem == ALOC_POOL || origem == ORIGEM_LITERAL || origem == ORIGEM_C;
}

/* B01h: origem de dois ramos (try, or_else): a restritiva vence, PARAM antes das demais; heap com heap = heap; o resto = 0. */
static inline uint8_t SemOrigemMaisRestritiva(uint8_t a, uint8_t b)
{
    if (a == ALOC_PARAM || b == ALOC_PARAM) return ALOC_PARAM;
    if (SemOrigemRestritiva(a)) return a;
    if (SemOrigemRestritiva(b)) return b;
    return a == ALOC_HEAP && b == ALOC_HEAP ? ALOC_HEAP : ALOC_NENHUM;
}

/* v1.8: origem do valor de um let/var/atribuicao (1 leitura no caminho comum): ALOC_* de new/make/format e de chamada de proc com alloc (B01g: heap tambem), ORIGEM_LITERAL num texto literal, 0 nos demais.
 * B01h: try e or_else leem a origem da chamada (or_else: a e b); o parser nao tem no para parenteses, EX_CONVERSAO segue 0. */
static inline uint8_t SemOrigemDoValor(const Expr *e)
{
    if (e->tipo_no == EX_EMBRULHA || e->tipo_no == EX_TRY) return SemOrigemDoValor(e->a);
    if (e->tipo_no == EX_OR_ELSE) return SemOrigemMaisRestritiva(SemOrigemDoValor(e->a), SemOrigemDoValor(e->b));
    if (e->tipo_no == EX_CHAMADA) return e->alocacao;
    return e->tipo_no == EX_TEXTO ? ORIGEM_LITERAL : 0;
}

/* v1.8: nome usado numa expressao/tipo; modulo = 0 sem qualificador, k = 'apelido.' do modulo k; pos = onde o E1003 aponta. */
typedef struct { int32_t nome; uint32_t pos; uint8_t modulo; } NomeUsado;

bool SemVisivelLento(Semantica *s, const Simbolo *y, const NomeUsado *u);
/* Caminho quente (ExNome, SemChamada, SemResolverNomeTipo): com 1 modulo, ou nome do proprio modulo, nao sai da linha. always_inline:
 * funcao static com 2o chamador perde o inline (licao do MEM-LJ-COMPILADOR). */
static inline __attribute__((always_inline)) bool SemVisivel(Semantica *s, const Simbolo *y, const NomeUsado *u)
{
    if (s->qtd_modulos <= 1) return true;
    if (u->modulo == 0 && (y->decl == NULL || y->decl->modulo == s->modulo_atual)) return true;
    return SemVisivelLento(s, y, u);
}

/* v1.13: 'd' passa a ser a declaracao em analise: o modulo (pacote) dela e o arquivo (trecho da Fonte) onde ela esta. Sem pasta e um trecho por modulo (a conta e O(1)). */
static inline void SemEntrarNaDecl(Semantica *s, const Decl *d)
{
    s->modulo_atual = d->modulo;
    s->arquivo_atual = FonteTrechoDoOffset(s->fonte, d->pos);
}

bool SemVerificarCaixa(Semantica *s, int32_t nome, uint32_t pos, CaixaNome caixa);
bool SemDeclararGlobal(Semantica *s, const Decl *d, const Simbolo *novo); /* IA1-L8: grava em globais[d->chave]; E0202 so no mesmo modulo (ou entre export) */
bool SemDeclararLocal(Semantica *s, const NovoLocal *novo, int32_t *id);
void SemGravarNomeLocal(Semantica *s, int32_t nome, TipoId tipo, uint32_t pos); /* M6 P65a: grava na posicao do local_id que acabou de sair de prox_local++ */
uint32_t SemPosDepoisDaPalavra(const Semantica *s, uint32_t pos); /* posicao do nome que segue let/var/for/switch/case */
uint32_t SemPosDoNomeQualificado(const Semantica *s, uint32_t pos); /* v1.8: de 'apelido.nome' (pos no apelido) para o nome */
uint32_t SemPosDoErro(const Semantica *s, uint32_t pos_retorno);  /* posicao de E em 'T ! E' / '! E' */

/* '-> ! E' (so erro): TR_NOME sem nome. As formas '^T'/'?^T' nao guardam nome (nome = 0 pela arena), por isso a forma conta. */
static inline bool TipoRefSoErro(const TipoRef *t) { return t->forma == TR_NOME && t->nome < 0; }

/* ---- fuga de endereco (sem_fuga_expr.c): marca o corpo que tem ponteiro (parametro, retorno, local) para a fuga conferir so ele ---- */
bool SemFugaTipoTemPonteiro(const Semantica *s, TipoId t);
/* Caminho quente (todo local declarado): os tipos fixos de i32 a cstr, menos rawptr, nunca tem ponteiro. */
static inline void SemMarcarPonteiroDoCorpo(Semantica *s, TipoId t)
{
    if (!s->corpo_tem_ponteiro && (t >= TIPO_FIXOS_QTD || t == TIPO_RAWPTR)) s->corpo_tem_ponteiro = SemFugaTipoTemPonteiro(s, t);
}

/* ---- emprestimo invalidado pela mutacao da colecao (sem_emprestimo.c, M6e P93g: E0913) ---- */
/* Embutidos que mudam a lista/map (e podem mover o buffer): o gancho de ChamarEmbutido liga corpo_muta_colecao e o passe os procura de novo. */
static inline bool SemEmbutidoMutaColecao(int32_t id)
{
    return id == EMB_APPEND || id == EMB_INSERT || id == EMB_INSERT_AT || id == EMB_REMOVE || id == EMB_DELETE || id == EMB_CLEAR || id == EMB_RESIZE;
}
bool SemEmprestimoCorpo(Semantica *s, const Decl *d); /* confere o corpo ja tipado sem erro: E0913 guardado na lista; false = algum erro */

/* ---- tipos (sem_tipo.c) ---- */
bool SemResolverNomeTipo(Semantica *s, int32_t nome, uint32_t pos, uint8_t modulo, TipoId *tipo); /* modulo: 'apelido.' do tipo (0 = nenhum) */
bool SemResolverTipoRef(Semantica *s, TipoRef *t);
bool SemResolverTipoV19(Semantica *s, TipoRef *t, TipoId *base); /* v1.9 (sem_v19.c): map, bit_set, soa e matrix escritos */
bool SemCompativel(Semantica *s, TipoId esperado, TipoId recebido, uint32_t pos);
/* v1.7: atribuicao a 'esperado'. Tipos iguais = true sem custo; T -> ?T reescreve 'e' no lugar para EX_EMBRULHA; senao SemCompativel. */
bool SemCoagirLento(Semantica *s, Expr *e, TipoId esperado, uint32_t pos);
static inline bool SemCoagir(Semantica *s, Expr *e, TipoId esperado, uint32_t pos)
{
    return e->tipo == esperado || SemCoagirLento(s, e, esperado, pos); /* caminho quente: tipos iguais, sem chamada */
}
void SemDescrever(const Semantica *s, TipoId tipo, char *saida, size_t cap);
Decl *SemDeclDaStruct(const Semantica *s, TipoId tipo); /* struct ou ^struct -> Decl; NULL nos demais */
/* v1.8b: struct opaca de foreign ('opaque Nome'); so struct de foreign tem externo. */
static inline bool SemTipoEhOpaco(Semantica *s, TipoId t)
{
    return TipoClasseDe(&s->tipos, t) == TC_STRUCT && SemDeclDaStruct(s, t)->externo;
}
/* ---- callback de foreign (sem_callback.c, v1.8b) ---- */
bool SemResolverTipoProc(Semantica *s, TipoRef *t);                                   /* TR_PROC: E0907 fora do parametro de proc foreign */
bool SemProcComoCallback(Semantica *s, Expr *e, const Simbolo *y, TipoId ctx);       /* nome de proc da Forja como valor (v1.12: em qualquer posicao) */
/* v1.12 P21v-a: proc como valor e chamada indireta (sem_callback.c) */
void SemMarcarProcValor(Semantica *s, uint32_t pos);   /* liga usa_proc_valor (o lowering recusa ate o P21v-b); guarda o 1o uso */
bool SemConferirProcsDaStruct(Semantica *s, Decl *d);  /* passe A: E0909 dos campos proc, depois de todos os DeclStruct */
bool SemConverterEmChamadaInd(Semantica *s, Expr *e);  /* 'f(x)' de local/parametro proc vira EX_CHAMADA_IND */
bool SemChamadaInd(Semantica *s, Expr *e);             /* EX_CHAMADA_IND: argumentos pelo tipo proc do chamado, tipo = retorno */
bool SemProcSemZero(const Semantica *s, TipoId t);     /* o tipo nao tem valor zero por causa de um proc (E0911) */
bool SemChamarComParams(Semantica *s, Expr *e, const Param *params, uint32_t nparams); /* sem_call.c: liga e confere os argumentos contra 'params' */
int32_t SemAcharCampo(const Decl *estrutura, int32_t nome);
/* Indice do membro 'nome' (escrito em 'pos') do enum; -1 = nao tem. L3c2: a forma antiga da tabela de renomeados vale (DbError.nao_abriu); com recusar (L3f) devolve SEM_MEMBRO_RECUSADO
 * depois de relatar o E0110. SemMembroAusente: o E0201 de ExMembro (false sem novo relato quando idx e SEM_MEMBRO_RECUSADO). */
enum { SEM_MEMBRO_RECUSADO = -2 };
int32_t SemAcharMembro(Semantica *s, const Decl *enumeracao, int32_t nome, uint32_t pos);
bool SemMembroAusente(Semantica *s, const Expr *e, const Decl *enumeracao, int32_t idx);

/* ---- expressoes (sem_expr.c, sem_resultado.c) ---- */
void SemMarcarFlexivel(Expr *e);                       /* preenche Expr.flexivel na arvore toda; 1 vez por raiz */
bool SemChecar(Semantica *s, Expr *e, TipoId ctx);     /* expressao interna (a raiz ja foi marcada) */
bool SemExpr(Semantica *s, Expr *e, TipoId ctx);       /* raiz: marca e checa */
bool SemExprCom(Semantica *s, Expr *e, TipoId esperado); /* raiz + confere (e coage) o tipo esperado */
/* v1.7: contexto de um literal/conta sob ?T e o proprio T (o embrulho entra depois, em SemCoagir). */
static inline TipoId SemCtxSemOpcional(const Semantica *s, TipoId ctx)
{
    if (!s->tipos.tem_opcional || ctx == TIPO_VAZIO) return ctx;
    const TipoInfo *k = TipoDe(&s->tipos, ctx);
    return k->classe == TC_OPCIONAL ? (TipoId)k->a : ctx;
}
bool SemLiteralInteiroEmReal(Semantica *s, Expr *literal, TipoId ctx); /* v1.11 (sem_literal.c): literal inteiro em contexto real; E0330 se inexato */
void SemSugerirConversao(Semantica *s, const Expr *e, TipoId esperado); /* v1.11 (sem_literal.c): extra "sugestao": "<expr> as <tipo>" do E0301 numerico */
/* IA1-b07 (sem_literal.c): o trecho [ini, fim) da fonte que 'e' ocupa e o texto que o troca para virar 'esperado' ("1.5 as i32"); false = nao e conversao numerica ou a forma e longa/complexa. */
typedef struct { uint32_t ini, fim; char novo[288]; } Conversao;
bool SemConversaoObvia(const Semantica *s, const Expr *e, TipoId esperado, Conversao *c);

/* ---- forma por ocorrencia do E0301 (sem_forma.c, IA1-b07): chamadas logo DEPOIS do SemErro do E0301; escrevem Diag.explicacao e Diag.forma ---- */
void SemFormaTipoDiferente(Semantica *s, const Expr *valor, TipoId esperado); /* "tipo diferente": o contexto (declaracao, atribuicao, argumento, return, campo) vem de s->ctx_tipo */
void SemFormaDoValor(Semantica *s, const Expr *valor, TipoId esperado, const char *papel); /* valor de tipo errado num papel fixo: "o indice", "a condicao" */
bool SemErroDoInteiro(Semantica *s, const Expr *valor, const char *papel);    /* E0301 "<papel> precisa ser inteiro" (indice, limite) com a forma da ocorrencia; sempre false */
void SemFormaOperandos(Semantica *s, const Expr *e);                         /* operador de dois lados com tipos que nao se misturam */
void SemFormaDoPrint(Semantica *s, const Expr *valor);                        /* print de tipo que ele nao imprime */
void SemFormaDaRegra(Semantica *s);                                           /* E0301 sem forma propria: a causa vira a regra (chamada pelo SemErro) */
/* IA1-b08 (sem_forma.c): o trecho do fonte que comeca em pos (ate o fim do argumento; com ate_espaco, ate o 1o espaco fora de parenteses) e a linha que contem pos, sem recuo nem comentario; 'saida' tem 80 bytes. */
void SemTrechoDoFonte(const Semantica *s, uint32_t pos, bool ate_espaco, char *saida);
void SemLinhaDoFonte(const Semantica *s, uint32_t pos, char *saida);
void SemFormaContaDeTexto(Semantica *s, const Expr *e);                        /* E0306 de conta com texto (str, cstr ou literal) e outro valor: os tipos reais e o f"..." (so se algum lado e texto) */
/* IA1-b08 (sem_dono.c): E0311 (dono nao se copia) - forma da ocorrencia e sugestao por caso; chamada logo DEPOIS do SemErro do E0311. 'pos' = onde o caso mora no fonte (o valor copiado,
 * o item do for, o tipo do parametro, o padrao do campo, o tipo do retorno); 'tipo' = o tipo do valor copiado (so no DONO_COPIA; nos demais TIPO_VAZIO). */
typedef enum { DONO_COPIA, DONO_FOR, DONO_PARAMETRO, DONO_CAMPO_PADRAO } DonoCaso;
void SemFormaDoDono(Semantica *s, DonoCaso caso, uint32_t pos, TipoId tipo);
bool SemTipoTemStructPorValor(const Semantica *s, TipoId t); /* v1.7: struct por valor direto, em array, ?T ou variante de union */
int32_t SemVarianteDe(const Semantica *s, TipoId uniao, TipoId tipo); /* v1.7: indice da variante da union, ou -1 */
bool SemCondicao(Semantica *s, Expr *cond);            /* raiz que precisa ser bool */
bool SemTry(Semantica *s, Expr *e);
bool SemOrElse(Semantica *s, Expr *e);

/* ---- chamadas e literais de struct (sem_call.c) ---- */
bool SemChamada(Semantica *s, Expr *e);
bool SemMathChamada(Semantica *s, Expr *e, const Simbolo **y); /* S3 P3: std:math, tipo dos argumentos e gemea <fn>_f32 (sem_math_std.c) */
void SemLibmNotarDecl(const Semantica *s, const Decl *d); /* S3 P4: foreign de libm que migra: a linha sai (so com Diag.trocas ligado) */
void SemLibmNotarChamada(const Semantica *s, const Expr *e, const Decl *d); /* S3 P4: chamada desse foreign vira math.<fn> ou sqrt */
bool SemLiteralStruct(Semantica *s, Expr *e);
bool SemFaixaLiteral(Semantica *s, const Campo *campo, const Expr *valor);
bool SemLigarEmbutido(Semantica *s, Expr *e, const Param *params, uint32_t nparams); /* liga os argumentos (sem checar tipos) e confere faltantes */

/* ---- arrays e fatias (sem_array.c) ---- */
typedef enum { FORMA_NENHUMA, FORMA_ARRAY, FORMA_PONTEIRO_ARRAY, FORMA_FATIA, FORMA_DINAMICO, FORMA_PONTEIRO_DINAMICO } ColecaoForma; /* [N]T, ^[N]T, []T, [dynamic]T, ^[dynamic]T */
static inline bool FormaTemTamanhoFixo(ColecaoForma f) { return f == FORMA_ARRAY || f == FORMA_PONTEIRO_ARRAY; }
TipoId SemElementoDe(const Semantica *s, TipoId colecao, ColecaoForma *forma);
uint32_t SemTamanhoDe(const Semantica *s, TipoId colecao); /* N de [N]T ou ^[N]T */
bool SemTipoSemZero(const Semantica *s, TipoId t);         /* ^T, ou [N]X com X sem zero */
bool SemIndice(Semantica *s, Expr *e);
bool SemIndiceDuplo(Semantica *s, Expr *e);                /* v1.9 (sem_v19.c): m[i, j] */
bool SemFatia(Semantica *s, Expr *e);
bool SemLiteralArray(Semantica *s, Expr *e, TipoId ctx);
/* literal inteiro, '-literal' ou nome de const cujo init e literal inteiro: sinal + magnitude; false nos demais */
bool SemInteiroConstante(const Semantica *s, const Expr *e, bool *negativo, uint64_t *magnitude);

/* v1.8b: struct (sem_decl.c) com campo sem padrao que exige valor no literal (^T, union ou struct de dentro que exige) */
bool SemStructExigeLiteral(Semantica *s, const Decl *d);

/* ---- memoria (sem_memoria.c) ---- */
bool SemTipoEhDono(const Semantica *s, TipoId t);       /* [dynamic]T, [N]dono ou struct com campo dono */
bool SemConferirMovimentoDono(Semantica *s, const Expr *e); /* E0311 se 'e' copia um dono de lvalor */
/* ---- tipo que nao se copia (sem_naocopia.c, S18 / E0339): Mutex, Cond e Atomic* ---- */
bool SemTipoEhNaoCopiavel(const Semantica *s, TipoId t);   /* o tipo (ou struct/array/?/'T ! E' que o contem) nao se copia */
bool SemConferirNaoCopiavel(Semantica *s, const Expr *e);  /* E0339 se 'e' copia um lvalor de tipo marcado */
bool SemExigeCopiavel(Semantica *s, TipoId t, uint32_t pos, const char *motivo); /* E0339 se o lugar (parametro, retorno, union, [dynamic], map) recebe o tipo por valor */
void SemRegistrarNaoCopiavel(Semantica *s, const Decl *d); /* liga tem_nao_copiavel quando a struct e marcada da std */
/* Atalho do caminho quente (todo let/var/=/return/literal): sem nenhum '[dynamic]T' na tabela de tipos nao ha dono para conferir. */
static inline bool SemConferirMovimento(Semantica *s, const Expr *e)
{
    if (s->tem_nao_copiavel && !SemConferirNaoCopiavel(s, e)) return false;
    return !s->tipos.tem_dinamico || SemConferirMovimentoDono(s, e);
}
bool SemChamarMemoria(Semantica *s, Expr *e, int32_t embutido); /* new, make (e append, delete, frame_reset) */
bool SemChamarSliceFrom(Semantica *s, Expr *e);         /* slice_from(p, len = n): fatia emprestada de ^T / ?^T de foreign */
bool SemChamarResize(Semantica *s, Expr *e);            /* IA1-L5b: resize(&lista, len = n) (sem_memoria.c) */
bool SemChamarSort(Semantica *s, Expr *e);              /* IA1-L5b: sort(fatia) de inteiro, real ou str (sem_memoria.c) */
bool SemChamarSortBy(Semantica *s, Expr *e);            /* K08a: sort_by(fatia, less = p), estavel, com proc (^T, ^T) -> bool (sem_memoria.c) */
bool SemChamarSortByKey(Semantica *s, Expr *e);         /* K08b: sort_by_key(fatia, key = Struct.campo), estavel, pela chave do campo (sem_memoria.c) */
bool SemChamarBinarySearch(Semantica *s, Expr *e);      /* K08b: binary_search(fatia, value = v) -> (int, bool), lower bound e achou (sem_memoria.c) */
bool SemChamarMatematica(Semantica *s, Expr *e, int32_t embutido); /* min, max, abs, clamp, sqrt (sem_matematica.c, E0334) */
bool SemTiparArgumentos(Semantica *s, Expr *e, TipoId contexto_literal); /* tipa os argumentos ja ligados: o 1o que nao e literal manda; os literais adotam o tipo dele (sem_matematica.c) */
void SemAritmeticaInternar(Semantica *s);   /* IA1-L9a/b: interna os 19 nomes (antes de qtd_nomes; sem_aritmetica.c) */
void SemAritmeticaRegistrar(Semantica *s);  /* registra os 19 embutidos EMB_WRAPPING_ADD..EMB_MUL_HI_U64 (depois de globais) */
bool SemChamarAritmetica(Semantica *s, Expr *e, int32_t embutido); /* wrapping/saturating/checked/overflowing, bits e mul_hi_u64 (sem_aritmetica.c, E0334) */
void SemListaInternar(Semantica *s);   /* IA1-L9c/L9d: interna os 19 nomes dos embutidos de lista e os dos parametros novos (antes de qtd_nomes; sem_lista.c) */
void SemListaRegistrar(Semantica *s);  /* registra os 19 embutidos EMB_CAP..EMB_REPLACE_VALUE (depois de globais) */
void SemListaMontar(Semantica *s);     /* monta params_lista (as assinaturas; depois de internar) */
bool SemChamarLista(Semantica *s, Expr *e, int32_t embutido); /* cap, clear, reserve, truncate, pop, insert_at, remove*, e a busca de sem_lista_busca.c (E0310, E0311, E0301, E0421) */
bool SemConferirClearMapa(Semantica *s, Expr *colecao); /* IA1-L9e: clear(&m): var (E0705) e fora do for do mesmo map (E0318); sem_mapa.c */
void SemEstatInternar(Semantica *s);   /* IA1-L9e: interna os 8 nomes (min_of, max_of, sum, avg, keys, values, from_bytes, as_bytes) e os parametros novos (antes de qtd_nomes; sem_estat.c) */
void SemEstatRegistrar(Semantica *s);  /* registra os 8 embutidos EMB_MIN_OF..EMB_AS_BYTES (depois de globais) */
bool SemChamarEstat(Semantica *s, Expr *e, int32_t embutido); /* min_of, max_of, sum, avg (sem_estat.c) e keys, values, from_bytes, as_bytes (sem_mapa_vistas.c, sem_texto_bytes.c) */
bool SemTipoSolto(Semantica *s, Expr *e);               /* CHECAGENS[EX_TIPO]: tipo fora de new/make = E0310 */
bool SemConferirAlocador(Semantica *s, Expr *chamada, Arg *em); /* 'em' de new, make, format, format_c: grava chamada->alocacao */

/* ---- posse (sem_posse.c) ---- */
bool SemConferirDelete(Semantica *s, const Expr *e);    /* delete(local): a origem do local diz se o delete vale (E0310/E0316); libera o rastreado */

/* v1.8 P7: posse estatica SEM fluxo (D19). Rastreado = local de let/var iniciado direto por new/make/format com em = heap ou escopo. */
typedef enum { POSSE_VIVO, POSSE_ENTREGUE, POSSE_LIBERADO, POSSE_GUARDADO } PosseEstado; /* GUARDADO (M6c P87): handle dentro do then de 'if h == none' */
/* de_chamada: heap = o valor veio de proc do usuario com alloc = heap (B01g); handle = foi emprestado a um proc da Forja (R-M6c-3). tipo: tipo do local (destruidor da forma_certa do handle) */
typedef struct { int32_t local_id, nome; uint32_t pos; TipoId tipo; uint8_t origem, estado, de_chamada; } LocalRastreado;
/* PONTO_EMPRESTIMO: argumento de proc sem clausula de posse (foreign) ou da Forja (_FORJA): o handle nao muda de dono; heap trata como PONTO_ARGUMENTO */
typedef enum { PONTO_COPIA, PONTO_GUARDA, PONTO_SAIDA, PONTO_ARGUMENTO, PONTO_EMPRESTIMO, PONTO_EMPRESTIMO_FORJA } PontoDePosse;
enum { ALOC_HANDLE = ORIGEM_C + 1 }; /* M6c P87: origem do rastreado de handle de lib C (proc foreign 'creates'); depois de todo ALOC_* e ORIGEM_* */

bool SemConferirPosseLento(Semantica *s, const Expr *e, PontoDePosse ponto);
bool SemConferirReatribuicaoLento(Semantica *s, const Stmt *st);
bool SemPosseDosArgumentosLento(Semantica *s, const Expr *chamada, const Decl *proc);
bool SemConferirSaidaLenta(Semantica *s, uint32_t marca, const Stmt *st);
bool SemConferirFimDoBloco(Semantica *s, uint32_t marca);
bool SemRastrearLocal(Semantica *s, const Stmt *st, uint8_t origem);
void SemDeferDoEscopo(Semantica *s, Stmt *st);
LocalRastreado *SemAcharRastreado(Semantica *s, const Expr *e); /* sem_posse.c: o rastreado que o nome liga, ou NULL */

/* ---- posse de handle de lib C (sem_posse_handle.c, M6c P87): E0317 estatico SEM fluxo, clausula creates/destroys/takes do proc foreign ---- */
bool SemLetCriaHandle(Semantica *s, const Stmt *st);                      /* let/var h = f(...) com f 'creates': o local nasce rastreado */
bool SemHandleDaSaida(Semantica *s, const Expr *chamada, const Decl *proc); /* f(..., p = &h) com f 'creates p': h nasce rastreado (E0912 se o argumento nao e &local) */
bool SemPosseDoHandle(Semantica *s, const Expr *e, LocalRastreado *r, PontoDePosse ponto); /* o nome rastreado de handle chega a um ponto de posse */
bool SemReatribuicaoDoHandle(Semantica *s, const Stmt *st, LocalRastreado *r);
bool SemErroDoHandleVivo(Semantica *s, const LocalRastreado *r, uint32_t pos, bool saida); /* E0317 + forma_certa concreta */
const Decl *SemDestruidorDoTipo(const Semantica *s, TipoId tipo);          /* 1o proc foreign 'destroys' do mesmo tipo-base do handle (M6c P88: tambem o lowering); NULL se nenhum */
bool SemEntregaDoArgumento(const Decl *proc, int32_t indice);            /* o parametro 'indice' do proc e destroys ou takes? */
int32_t SemGuardaNoneAbrir(Semantica *s, const Expr *cond);               /* 'h == none': o handle h vira GUARDADO no then; devolve o local_id ou -1 */
void SemGuardaNoneFechar(Semantica *s, int32_t local_id);

/* Caminho quente: sem heap/escopo no proc (rastreados vazio) nenhuma destas sai da linha. */
static inline bool SemConferirPosse(Semantica *s, const Expr *e, PontoDePosse ponto)
{
    return s->rastreados.n == 0 || SemConferirPosseLento(s, e, ponto);
}
static inline bool SemConferirReatribuicao(Semantica *s, const Stmt *st)
{
    return s->rastreados.n == 0 || SemConferirReatribuicaoLento(s, st);
}
static inline bool SemPosseDosArgumentos(Semantica *s, const Expr *chamada, const Decl *proc)
{
    return s->rastreados.n == 0 || SemPosseDosArgumentosLento(s, chamada, proc);
}
static inline bool SemConferirSaida(Semantica *s, uint32_t marca, const Stmt *st)
{
    return s->rastreados.n == 0 || SemConferirSaidaLenta(s, marca, st);
}

/* ---- f"..." e format (sem_formato.c) ---- */
bool SemFTexto(Semantica *s, Expr *e);                  /* confere as partes; e->tipo = str */
bool SemFTextoDoArgumento(Semantica *s, Expr *chamada); /* L5a: print(real) e eprint(x): o 1o argumento (conferido) vira f"{x}" */
bool SemChamarFormato(Semantica *s, Expr *e);           /* format(f"...", em = A) -> str; format_c -> cstr */

/* ---- comandos (sem_stmt.c, sem_fluxo.c, sem_switch.c) ---- */
bool SemLvalorMutavel(Semantica *s, const Expr *alvo);                         /* escrita: E0705/E0301 se a cadeia nao e alteravel */
bool SemLvalorMutavelDe(Semantica *s, const Expr *alvo, const char *motivo);   /* idem, com o motivo de quem pede ('&', fatia, for &) */
bool CorpoDeLaco(Semantica *s, Stmt *corpo);                                   /* v1.9: corpo de laco (conta a profundidade e a marca de posse) */
bool SemBloco(Semantica *s, Stmt *primeiro);
bool SemComando(Semantica *s, Stmt *st);
bool SemReturn(Semantica *s, Stmt *st);
bool SemLacoControle(Semantica *s, Stmt *st);  /* break / continue */
bool SemDefer(Semantica *s, Stmt *st);
bool SemExpect(Semantica *s, Stmt *st);
bool SemSwitch(Semantica *s, Stmt *st);
bool SemTipoDoCaso(Semantica *s, Caso *c, TipoId *t); /* v1.7: tipo do 'case': tipo_escrito ou tipo_nome */

/* ---- declaracoes (sem_decl.c) ---- */
bool SemPasseA(Semantica *s);
bool SemPasseB(Semantica *s);
bool SemVerificarMain(Semantica *s);

/* ---- c_long/c_ulong so na fronteira (sem_clong.c, M6c P83 / E0342) ---- */
bool SemCLongDecls(Semantica *s);                 /* assinaturas, campos e const; liga s->usa_clong; true = nenhum E0342 (ou o programa nao cita c_long) */
bool SemCLongCorpo(Semantica *s, const Decl *d);  /* corpo de proc/test ANTES de tipar (regra sintatica, sem fluxo); false = E0342 pendente em s->diag */

/* ---- v1.9: despachos (sem_v19.c) e bit_set (sem_bitset.c); matrix, soa e map chegam nos passos P5, P7 e P10 ---- */
bool SemEhTipoV19(const Semantica *s, TipoId t);         /* classe >= TC_MAPA, ou ^ de uma */
bool SemTipoTemV19(const Semantica *s, TipoId t);        /* tipo v1.9 direto, em array ou em campo de struct */
bool SemIndiceV19(Semantica *s, Expr *e);                /* a[i] com base map, bit_set, soa ou matrix */
bool SemLiteralV19(Semantica *s, Expr *e, TipoId ctx);   /* '{...}' com contexto de classe >= TC_MAPA */
bool SemContaV19(Semantica *s, Expr *e);                 /* + - * / % e bits com operando de classe >= TC_MAPA */
bool SemComparacaoV19(Semantica *s, Expr *e);            /* == != < ... com operando de classe >= TC_MAPA */
bool SemChamarV19(Semantica *s, Expr *e, int32_t embutido); /* insert, get, get_ptr, has, remove, transpose */
bool SemForV19(Semantica *s, Stmt *st);                  /* for sobre map, bit_set, soa ou matrix */
bool SemCampoV19(Semantica *s, Expr *e);                 /* a.campo com base map, bit_set, soa ou matrix */
bool ResolverBitSet(Semantica *s, TipoRef *t, TipoId *base);
bool SemLiteralBitSet(Semantica *s, Expr *e, TipoId ctx);
bool SemContaBitSet(Semantica *s, Expr *e);
bool SemComparacaoBitSet(Semantica *s, Expr *e);
bool SemChamarBitSet(Semantica *s, Expr *e, int32_t embutido, Expr *colecao);
bool SemForBitSet(Semantica *s, Stmt *st);
bool ResolverMatriz(Semantica *s, TipoRef *t, TipoId *base);   /* sem_matriz.c (P5) */
bool SemLiteralMatriz(Semantica *s, Expr *e, TipoId ctx);
bool SemIndiceMatriz(Semantica *s, Expr *e);                   /* m[i, j] */
bool SemContaMatriz(Semantica *s, Expr *e);
bool ResolverSoa(Semantica *s, TipoRef *t, TipoId *base);      /* sem_soa.c (P7) */
bool SemLiteralSoa(Semantica *s, Expr *e, TipoId ctx);
bool SemIndiceSoa(Semantica *s, Expr *e);                      /* a[i] */
bool SemColunaDaSoa(Semantica *s, Expr *e);                    /* a.campo = fatia da coluna */
bool SemForSoa(Semantica *s, Stmt *st);
bool SemColecaoDona(const Semantica *s, TipoId t);             /* [dynamic]T, map ou soa[dynamic]S (sem_memoria.c) */
bool SemChamarTranspose(Semantica *s, Expr *e);
bool ResolverMapa(Semantica *s, TipoRef *t, TipoId *base);     /* sem_mapa.c (P10) */
bool SemChamarMapa(Semantica *s, Expr *e, int32_t embutido, Expr *colecao);
bool SemForMapa(Semantica *s, Stmt *st);
bool SemConferirMapaNoFor(Semantica *s, const Expr *onde, int32_t nome); /* E0318 se o map esta no for aberto */
Arg *ArgumentoDoParametro(Expr *e, int32_t k);                 /* o Arg com indice == k, ou NULL (sem_v19.c) */

/* ---- conta elemento a elemento em [N]T (sem_vetor.c, v1.11 L12a): E0331 ---- */
bool SemAritmetica(Semantica *s, Expr *e);      /* + - * / % entre dois operandos ja tipados (sem_expr.c); com colecao desvia para SemContaVetor */
bool SemContaVetor(Semantica *s, Expr *e);      /* + - * / com algum operando [N]T (ou colecao recusada): grava e->tipo = [N]R */
bool SemNegarVetor(Semantica *s, Expr *e);      /* -a com a de classe TC_ARRAY */
bool SemCompostaVetor(Semantica *s, Stmt *st);  /* alvo += expr e alvo -= expr com alvo [N]T */
bool SemEhSwizzle(const Semantica *s, const Expr *e);  /* v1.11 L12b: campo pedido em [N]T de numero com nome de 1 a 4 letras xyzw/rgba */
bool SemSwizzle(Semantica *s, Expr *e);         /* v.x vira v[k] (EX_INDICE); v.xy..v.xyzw ficam EX_CAMPO com REF_COMPONENTE; E0332 nos erros */

/* ---- quat (sem_quat.c, v1.11 L14a): E0333 ---- */
void SemQuatGarantir(Semantica *s);                   /* 1o uso do nome quat: registra a struct oculta {x, y, z, w: f32} (s->tipo_quat, s->decl_quat) */
bool SemContaQuat(Semantica *s, Expr *e);             /* + - * com algum operando quat (ja tipados); / % e as formas que nao existem = E0333 */
bool SemComparacaoQuat(Semantica *s, Expr *e);        /* == != e ordem com algum operando quat = E0333 */
bool SemChamarConj(Semantica *s, Expr *e);            /* conj(q) */
bool SemAbsDeQuat(Semantica *s, Expr *e);             /* abs(q): a norma, e->tipo = f32 (o argumento ja tipado e quat) */
static inline bool SemEhQuat(const Semantica *s, TipoId t) { return s->tipo_quat != 0 && t == s->tipo_quat; }

/* ---- Allocator (sem_allocator.c, B01a): o tipo embutido do parametro 'alloc: Allocator' de proc do usuario; E0336 ---- */
void SemAllocatorGarantir(Semantica *s);                              /* 1o uso do nome Allocator: registra a struct oculta {id: u32} (s->tipo_allocator, s->decl_allocator) */
bool SemAllocatorConferirParam(Semantica *s, const Decl *d, const Param *p, uint32_t i); /* parametro Allocator: nome alloc, ultimo, sem padrao (E0336) */
bool SemAllocatorForaDeParametro(Semantica *s, const TipoRef *t);     /* Allocator escrito fora do parametro 'alloc' (let, var, campo, retorno, ^, ?, array, map, tipo proc) = E0336 */
bool SemAllocatorNomeForaDeParametro(Semantica *s, uint32_t pos);     /* B01b: nome Allocator resolvido direto (transmute, case) = E0336 */
bool SemAllocatorLerCampo(Semantica *s, const Expr *e);              /* alloc.id = E0336 */
bool SemAllocatorLiteral(Semantica *s, const Expr *e);                /* Allocator{...} = E0336 */
bool SemAllocatorValorSolto(Semantica *s, const Expr *e);             /* B01c: ler o nome alloc fora do rotulo 'alloc =' (let, var, atribuicao...) = E0336 */
bool SemAllocatorInserirNoMigrar(Semantica *s, Expr *e, const Param *faltante); /* B01d: no forja migrar, chamada sem 'alloc' vale como alloc = heap e registra a insercao */
bool SemAllocatorConverterArg(Semantica *s, Expr *chamada, Arg *g);  /* B01c: 'alloc = heap|frame|Arena|^Arena|alloc' vira o valor Allocator{id} e grava chamada->alocacao */
void SemAllocatorOrigemDaChamada(const Semantica *s, Expr *e);        /* B01c: a origem so vale se o resultado da chamada carrega memoria (texto, ^T, ?^T, lista, map, soa) */
uint8_t SemAlocacaoDoNome(const Semantica *s, const Expr *v);         /* sem_memoria.c: heap, frame ou scope escrito como nome = ALOC_*; ALOC_NENHUM nos demais */
static inline bool SemEhAllocator(const Semantica *s, TipoId t) { return s->tipo_allocator != 0 && t == s->tipo_allocator; }

bool SemSimboloCEhAlias(Semantica *s, uint8_t modulo, int32_t nome); /* sem_export.c: o nome e o simbolo C do alias de um foreign deste modulo (IA1-L8) */
/* ---- chave por modulo (sem_chave.c, IA1-L8) ---- */
void SemChavesPreencher(Semantica *s); /* SemanticaIniciar, antes de fixar qtd_nomes: Decl.chave de toda declaracao */
void SemHomonimosIniciar(Semantica *s); /* SemanticaIniciar, depois de qtd_nomes: a cadeia de Decl.homonimo por id de nome */
/* Simbolo que o nome 'nome' (escrito com o qualificador 'qualificador', 0 = nenhum) designa no modulo em analise (s->modulo_atual). O proprio modulo vence; nome que o modulo
 * nao declara cai no que a linguagem ou a raiz tem em globais[nome] (SemVisivel decide se vale); nome nao declarado em lugar nenhum do modulo que a raiz escreveu sem apelido
 * devolve o simbolo do modulo que o declara, para o E1003 (SemVisivelLento). Nunca devolve NULL. */
const Simbolo *SemAcharSimboloLento(const Semantica *s, int32_t nome, uint8_t qualificador);
static inline __attribute__((always_inline)) const Simbolo *SemAcharSimbolo(const Semantica *s, int32_t nome, uint8_t qualificador)
{
    const Simbolo *y = &s->globais[nome];
    if (s->qtd_modulos <= 1 || (qualificador == 0 && s->modulo_atual == 0 && y->categoria != SIM_NENHUM)) return y; /* caminho quente */
    return SemAcharSimboloLento(s, nome, qualificador);
}

/* ---- lista de capacidade fixa (sem_lista_fixa.c, v1.11 L15a): [dynamic, N]T = struct oculta {itens: [N]T, n: int}; E0335 ---- */
void SemListasFixasIniciar(Semantica *s);                         /* SemanticaIniciar: nomes dos campos e das LISTAS_FIXAS_MAX structs ocultas */
bool SemResolverListaFixa(Semantica *s, TipoRef *t, TipoId *base); /* TR_LISTA_FIXA: elemento, N (1..65536) e a struct oculta */
bool SemResolverCapacidade(Semantica *s, const TipoRef *t, uint32_t *n); /* sem_tipo.c: o N de [dynamic, N]T (literal ou const), E0335 fora de 1..65536 */
bool SemLiteralListaFixa(Semantica *s, Expr *e, TipoId ctx);     /* '{}' em [dynamic, N]T; com elementos = E0335 */
bool SemEhListaFixaOuPonteiro(const Semantica *s, TipoId t);      /* [dynamic, N]T ou ^[dynamic, N]T */
bool SemErroDeListaFixa(Semantica *s, uint32_t pos, const char *causa); /* E0335 com a causa dada */

/* ---- layout (sem_layout.c, v1.11 L9): size_of, align_of, offset_of e static_assert; o valor sai no lowering (lower_layout.c) ---- */
bool SemChamarLayout(Semantica *s, Expr *e, int32_t embutido);  /* size_of, align_of, offset_of: tipam como int e conferem o tipo (E0328) */
bool SemConferirStaticAssert(Semantica *s, const Expr *e);      /* depois de ligar (cond, msg) como o assert: a condicao e constante (E0329) */
bool SemDeclAssert(Semantica *s, Decl *d);                      /* static_assert de topo (D_ASSERT) */
bool SemUsaLayoutNoTexto(const Expr *e);                        /* size_of/align_of/offset_of escrito dentro da expressao (a const nao serve de N de [N]T) */

/* ---- sugestoes do lote 2 (sem_sugestao.c, IA1-b06): chamadas logo DEPOIS do SemErro do codigo; abrem a sugestao em s->diag->sug ---- */
typedef struct { uint32_t pos; const char *antigo; size_t tam_antigo; const char *novo; } TrocaDeNome; /* o nome escrito em pos vira 'novo' */
void SemSugerirSoDica(Semantica *s, DiagCodigo codigo, const char *texto);                     /* descricao sem edicao */
void SemSugerirTroca(Semantica *s, const TrocaDeNome *troca, DiagConfianca confianca, const char *nota); /* troca o nome; 'nota' (pode ser "") fecha a descricao */
void SemSugerirCaixa(Semantica *s, int32_t nome, uint32_t pos, const char *certo);              /* E0102: o nome na caixa certa (provavel: os usos acompanham) */
void SemSugerirParecido(Semantica *s, const SemParecido *sp);                                  /* E0201/E1003 com um parecido unico (ou o 1o estritamente mais perto) */
void SemSugerirQualificar(Semantica *s, const NomeUsado *uso, const Decl *dono);               /* E1003 sem apelido: insere "apelido." */
void SemSugerirAliasDeForeign(Semantica *s, const Decl *d);                                    /* E0202 de proc foreign com nome reservado: proc c_nome = "nome" */
void SemSugerirRenomearProc(Semantica *s, int32_t nome, uint32_t pos);                         /* E0202 de proc da Forja que e o simbolo C de um alias */
void SemSugerirTirarNome(Semantica *s, const Arg *g, int32_t param0);                          /* E0502: 1o argumento nomeado; param0 = nome do 1o parametro (-1 se nao ha) */
void SemSugerirNomearArg(Semantica *s, const Arg *g, const char *nome);                        /* E0502: argumento posicional que pede "nome = " */
void SemSugerirTirarArg(Semantica *s, const Arg *g);                                           /* E0502: proc sem parametros recebeu argumento */
void SemSugerirEnderecoDoAppend(Semantica *s, const Expr *lista);                              /* E0310: append(l, ...) -> append(&l, ...) */
void SemSugerirEmprestimo(Semantica *s, const Expr *e);                                        /* E0311 de let/var b = a: &a e, em lista, a[..] */
void SemSugerirConcatenar(Semantica *s, const Expr *soma);                                     /* E0307 de "a" + "b": f"{a}{b}" em print; senao so dica */

/* ---- idiomas de outras linguagens (sem_idioma.c, IA1-L2): chamados ANTES do SemErro do E0201/E0308; true = relatou o E0110 (quem chama devolve false) ---- */
bool SemIdiomaDeNome(Semantica *s, const Expr *e);              /* nome sem declaracao: nil/null/None, True/False, console.log, fmt.Println */
bool SemIdiomaDeTipo(Semantica *s, int32_t nome, uint32_t pos); /* tipo sem declaracao: string, String */
bool SemIdiomaDeChamada(Semantica *s, const Expr *e);           /* proc sem declaracao: range(n), printf, println, puts */
bool SemIdiomaDeCampo(Semantica *s, const Expr *e);             /* campo em array/lista/texto: x.len(), x.length, x.push(v), x.append(v) */

/* ---- higiene (sem_higiene.c, v1.11 L10a): E0707 var nunca alterada, E0708 let/var nunca lido, E0709 retorno descartado, E1005 import sem uso ---- */
bool SemHigienePropor(Semantica *s, DiagCodigo codigo, uint32_t pos, const char *fmt, ...) __attribute__((format(gnu_printf, 4, 5))); /* false = ja havia um achado mais cedo no proc (nada mudou) */
void SemHigieneSugerir(Semantica *s, DiagConfianca confianca, const char *fmt, ...) __attribute__((format(gnu_printf, 3, 4))); /* sugestao do achado que SemHigienePropor acabou de aceitar */
void SemHigieneSugerirDescarteInutil(Semantica *s, uint32_t st_pos, bool valor_e_chamada);                                      /* sugestao do E0709 de '_ = valor' (o achado que SemHigienePropor acabou de aceitar) */
void SemHigieneEditar(Semantica *s, uint32_t ini, uint32_t fim, const char *novo);                                             /* edicao [ini, fim) -> novo da ultima sugestao do achado */
void SemHigieneAlterar(Semantica *s, const Expr *alvo);                  /* a raiz local da cadeia foi alterada (atribuicao, '&', fatia, for &item, append) */
bool SemHigieneRaizLida(Semantica *s, const Expr *alvo);                 /* antes de conferir o alvo de 'x = v': a raiz ja estava lida? */
void SemHigieneEscrita(Semantica *s, const Expr *alvo, bool lida_antes); /* depois de conferir o alvo: escrever em x nao le x (salvo por ponteiro ou fatia) */
void SemHigieneFechar(Semantica *s, uint32_t altura);                    /* fim de bloco: let/var sem alteracao (var) ou sem leitura */
void SemHigieneRetornoDescartado(Semantica *s, const Expr *chamada);     /* chamada de proc como comando com valor devolvido */
void SemHigieneIniciarCorpo(Semantica *s);
void SemHigieneGuardar(Semantica *s, Decl *d);                           /* proc fechado sem erro: o achado espera a fuga */
bool SemHigieneRelatar(Semantica *s);                                    /* depois da fuga: o erro dos procs que continuam sem erro */
void SemHigieneImportUsado(Semantica *s, uint8_t modulo);
bool SemHigieneImports(Semantica *s);                                    /* programa sem erro: E1005 */
const Expr *SemRaizDaCadeia(const Semantica *s, const Expr *x);         /* sem_stmt.c: raiz da cadeia sem ponteiro/fatia (NULL = passa por ponteiro ou fatia) */

/* ---- export (sem_export.c) ---- */
bool SemChecarModificadores(Semantica *s, uint32_t *categorias); /* 'pub'/'export' bem colocados (E0903); 1o passo do passe A; categorias = bit por DeclTipo presente */
bool SemChecarExportado(Semantica *s, const Decl *d); /* nome (E0902) e tipos da ABI C (E0901) de um proc exportado */
bool SemChecarPosseC(Semantica *s, const Decl *d);    /* M6c P85: o alvo da clausula creates/destroys/takes do proc foreign e handle de lib C (E0912; sem_posse_c.c) */
bool SemChecarExterno(Semantica *s, const Decl *d);   /* nome (E0905) e tipos da fronteira C (E0904) de um proc foreign */
const char *SemMotivoDoProc(Semantica *s, TipoId tipo, char *buf, size_t cap); /* v1.12: NULL = a assinatura do TC_PROC atravessa a fronteira C (buf >= 256) */
bool ErroSimboloCDeAlias(Semantica *s, int32_t nome, uint32_t pos); /* v1.10 (A6): E0202 de nome de proc que e o simbolo C de um alias foreign */

#endif
