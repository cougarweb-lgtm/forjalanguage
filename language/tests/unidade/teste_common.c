/* teste_common.c - testes de unidade da base comum (arena, buf, vec, intern, fonte, diag) e das tabelas da semantica
 * (dimensao, tipos, escopo).
 * Versao: 0.35.0
 * Changelog: 0.35.0 - IA1-L8: NomeModuloVerificacoes (teste_common_nome_modulo.c: chave por modulo, tags, nome de exibicao e Decl.chave).
 *            0.34.0 - IA1-L3a: RenomeadosVerificacoes (teste_common_renomeados.c: tabela de renomeados sem campo vazio nem antigo repetido, API e secao RENOMEADOS da migracao.txt embutida igual a tabela).
 *            0.33.0 - IA1-b14: TestaDiagRelacionados (+6: limite de 5 e repetido, JSON do pacote com relacionados, sem sugestao antes do spec, corte de 120 bytes, DiagRelatar zera, ParecidosVerEm).
 *            0.32.0 - IA1-b09: SpecVerificacoes (teste_common_spec.c: doc embutida, mapa codigo -> secao em 100% do catalogo, spec do diagnostico e do explicar).
 *            0.31.0 - IA1-b08: a explicacao do catalogo (R0011, higiene) passa a ser conferida em DiagExplicarJson (LinhaDoCatalogo); a linha do Diag traz a explicacao da ocorrencia; E0328 confere offset_of(S.campo).
 *            0.30.0 - IA1-b04: TestaDiagSugestao (+5: limites da lista de sugestoes e da edicao, JSON do pacote do erro, spec na 1a vez e spec_ref na 2a, DiagRelatar zera as sugestoes).
 *            0.29.0 - IA1-b01: TestaDiagHigiene +1 (E1006 no catalogo, tipo modulo, explicacao e forma_certa).
 *            0.28.0 - M3 P22-O1b: +4 verificacoes em teste_common_projeto.c (ProjetoVerificacoes: parser do forja.proj e B0005; camadas e B0007; chave com valor fixo e estado ida e volta; motivo por prioridade).
 *            0.27.0 - M3 P22-O1a: +6 verificacoes em teste_common_atomico.c (AtomicoVerificacoes: Hash64 de "" e "a"; ArquivoGravarAtomico cria, substitui, falha em pasta inexistente com ENOENT e troca um destino carregado como DLL).
 *            0.26.0 -Linguagem v1.12 M3 P21v-a: TestaTipoProc usa ProcAssinatura (+1: nome na identidade do tipo proc, TipoProcCompativel e TipoProcNome).
 *            0.25.0 -Linguagem v1.11 L16b: TestaDiagQueda (+1: R0011 no catalogo, tipo runtime, explicacao e forma_certa).
 *            0.24.0 -Linguagem v1.11 L10a-bis: TestaDiagHigiene +4 (E0707 var multipla, E0708 com _r, E0709 e E0601 com _ = try f()); LocalInfo com o campo grupo.
 *            0.23.0 - Linguagem v1.11 L10a: TestaDiagHigiene (+4: E0707, E0708, E0709 e E1005 no catalogo); inicializadores de LocalInfo com pos e usos.
 *            0.22.0 -Linguagem v1.11 L9: TestaDiagLayout (+2: E0328 e E0329 no catalogo, tipo layout, causa, explicacao e forma_certa).
 *            0.21.0 - Linguagem v1.11 L8: TestaDiagCalado (+1: diagnostico calado nao entra na lista nem conta no teto; DiagRelatar zera o calado).
 *            0.20.0- Linguagem v1.11 L7: TestaDiagLista (+2: ordem estavel por modulo e off; teto, cortado e teto 0 = padrao 20).
 *            0.19.0 - Linguagem v1.11 L6: TestaColunaFim (+1: coluna_fim de identificador, numero, texto, operador de 2 caracteres e fim de arquivo).
 *            0.18.0 - Linguagem v1.11 L5: TestaParecido (+4: transposicao, lista vazia, empate pelo nome e limite len/3).
 *            0.17.0 - Linguagem v1.11 L3: TestaFugaPontoFixo (+1: ponto fixo de 3 procs mutuamente recursivos, teto e nao convergencia).
 *            0.16.0 -Linguagem v1.11 L2: TestaDiagV111 +1 (E0706 com o extra "cadeia" depois de forma_certa).
 *            0.15.0 - Linguagem v1.11 L1: TestaDiagV111 (+3: E0330 no catalogo, extras depois de forma_certa na ordem, sugestao e zerar extras).
 *            0.14.1 - M2 P10b: TestaRtNativoWin confere os dois objetos (rt_win_dev.o e rt_win_rel.o); mesmas 2 verificacoes.
 *            0.14.0 - Linguagem v1.10 P2b: TestaRtNativoWin (+2: tamanho > 0 e cabecalho COFF AMD64 do rt_win.o embutido).
 *            0.13.0 - Linguagem v1.9 P3: TestaTiposV19 (+2).
 *            0.12.0 - Linguagem v1.9 P1: TestaDiagV19 (+1).
 *            0.11.0- Linguagem v1.8b P5a: TestaTipoProc (+3).
 *            0.10.0 - Linguagem v1.8b P1: TestaDiagV18b (+1).
 *            0.9.1 - Linguagem v1.8 P6a: TestaEscopo com o inicializador completo de LocalInfo (campo origem).
 *            0.9.0 - Linguagem v1.8 P1: TestaFonteTrechos (+6) e TestaDiagV18 (+1).
 *            0.8.0 - Linguagem v1.7 P2: TestaDiagV17 (E0312, E0313, E0314, E0315).
 *            0.7.0 - Linguagem v1.6 P2: TestaDiagV16 (E0310, E0311, R0007, R0008).
 *            0.6.0 - Linguagem v1.5 P2: TestaDiagV15 (E0308, E0309, E0421, R0006).
 *            0.5.0 - Linguagem v1.4 P2: TestaDiagV14 (E0904, E0905, E0906, B0004).
 *            0.4.0 - Linguagem v1.3: TestaTiposPonteiros (ids 15/16, str, ^T sobre qualquer tipo, ?^T, nao numericos) e TestaDiagV13 (E0306, E0307, E0706, R0005); TIPO_FIXOS_QTD == 17.
 *            0.3.0 - Linguagem v1.2: TestaTiposNumericos (ids 8..14, largura, sinal, literal e faixa por tipo).
 *            0.2.2 - Fase 5i: TestaFonteLerBom (BOM UTF-8 descartado, NUL final, arquivo inexistente).
 *            0.2.1 - Fase 5i: TestaFonteTabelaContraVarredura (tabela de linhas x varredura em todo offset).
 *            0.2.0 - Fase 5c: TestaDimensao, TestaTipos e TestaEscopo.
 *            0.1.0 - Fase 5a.
 * Verificacao: (em language\compiler) construir.bat && teste_common.exe   -> "OK: N verificacoes"
 */
#include <stdio.h>
#include <stdlib.h>

#include "common/buf.h"
#include "common/diag.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "common/parecido.h"
#include "nativo/rt_embutido.h"
#include "semantic/dimensao.h"
#include "semantic/escopo.h"
#include "semantic/sem_fuga.h"
#include "semantic/tipos.h"

/* Hash64 e gravacao atomica (P22-O1a): tests\unidade\teste_common_atomico.c (entra no teste_common.exe pelo construir.bat). */
void AtomicoVerificacoes(int *total, int *falhas);
void ProjetoVerificacoes(int *total, int *falhas);
/* Motor de edicoes e leitor de JSON do driver (IA1-b05): tests\unidade\teste_common_edicoes.c. */
void EdicoesVerificacoes(int *total, int *falhas);

/* Doc embutida, mapa codigo -> secao e campo "spec" (IA1-b09): tests\unidade\teste_common_spec.c. */
void SpecVerificacoes(int *total, int *falhas);
/* Tabela de renomeados e a secao RENOMEADOS da migracao.txt (IA1-L3a): tests\unidade\teste_common_renomeados.c. */
void RenomeadosVerificacoes(int *total, int *falhas);
void NomeModuloVerificacoes(int *total, int *falhas);

static int g_total, g_falhas;

#define VERIFICA(cond) VerificaCondicao((cond), #cond, __LINE__)

static void VerificaCondicao(bool ok, const char *texto, int linha)
{
    g_total++;
    if (ok) return;
    g_falhas++;
    fprintf(stderr, "FALHOU linha %d: %s\n", linha, texto);
}

static void TestaArena(void)
{
    Arena a = {0};
    char *x = ArenaAlocar(&a, 3);
    char *y = ArenaAlocar(&a, 100);
    VERIFICA(x != y && ((uintptr_t)x & 7) == 0 && ((uintptr_t)y & 7) == 0);
    VERIFICA(x[0] == 0 && y[99] == 0);
    char *grande = ArenaAlocar(&a, (size_t)9 << 20); /* maior que um bloco */
    VERIFICA(grande != NULL && grande[(9 << 20) - 1] == 0);
    char *t = ArenaCopiarTexto(&a, "forja!", 5);
    VERIFICA(strcmp(t, "forja") == 0);
    ArenaLiberar(&a);
    VERIFICA(a.topo == NULL);
}

static void TestaBuf(void)
{
    Buf b = {0};
    BufAnexar(&b, "ola");
    BufAnexarU64(&b, 1234567890123ull);
    VERIFICA(strcmp(b.dados, "ola1234567890123") == 0 && b.tam == 16);
    char longo[1500];
    memset(longo, 'x', sizeof longo - 1);
    longo[sizeof longo - 1] = '\0';
    BufFormatar(&b, "[%s|%d]", longo, 7);
    VERIFICA(b.tam == 16 + 1 + 1499 + 3);
    VERIFICA(b.dados[b.tam - 1] == ']' && b.dados[b.tam - 2] == '7' && b.dados[b.tam - 3] == '|');
    BufLiberar(&b);
    VERIFICA(b.dados == NULL);
}

static void TestaVec(void)
{
    Vec v;
    VecIniciar(&v, sizeof(int));
    for (int i = 0; i < 1000; i++) *(int *)VecEmpurrar(&v) = i * 3;
    VERIFICA(v.n == 1000 && *(int *)VecEm(&v, 0) == 0 && *(int *)VecEm(&v, 999) == 2997);
    VecLiberar(&v);
}

static void TestaIntern(void)
{
    Arena a = {0};
    Interner in;
    InternerIniciar(&in, &a);
    int32_t proc = Internar(&in, "proc", 4);
    int32_t main_ = Internar(&in, "main", 4);
    VERIFICA(proc == 0 && main_ == 1 && Internar(&in, "proc", 4) == proc);
    VERIFICA(strcmp(InternerTexto(&in, main_), "main") == 0 && InternerTam(&in, main_) == 4);
    char nome[16];
    for (int i = 0; i < 5000; i++) {
        int n = snprintf(nome, sizeof nome, "v%d", i);
        Internar(&in, nome, (uint32_t)n);
    }
    VERIFICA(in.nomes.n == 5002 && Internar(&in, "v4999", 5) == 5001 && Internar(&in, "main", 4) == main_);
    InternerLiberar(&in);
    ArenaLiberar(&a);
}

static void TestaFonte(void)
{
    Fonte f = FonteDeTexto("a.fj", "ab\ncd\n");
    uint32_t l, c;
    FonteLocalizar(&f, 0, &l, &c);
    VERIFICA(l == 1 && c == 1);
    FonteLocalizar(&f, 1, &l, &c);
    VERIFICA(l == 1 && c == 2);
    FonteLocalizar(&f, 3, &l, &c);
    VERIFICA(l == 2 && c == 1);
    FonteLocalizar(&f, 4, &l, &c);
    VERIFICA(l == 2 && c == 2);
    VERIFICA(FonteContarLinhas(&f) == 2);
    Fonte sem_lf = FonteDeTexto("b.fj", "x\ny");
    VERIFICA(FonteContarLinhas(&sem_lf) == 2);
}

/* Oraculo: a mesma fonte sem tabela (varredura) e com tabela (busca binaria) tem de dar linha/coluna iguais em todo offset. */
static void TestaFonteTabelaContraVarredura(void)
{
    const char *textos[] = {"", "ab\ncd\n", "x\ny", "\n\n", "a\r\nb"};
    bool todos_iguais = true;
    bool indexou_todos = true;

    for (size_t t = 0; t < sizeof textos / sizeof textos[0]; t++) {
        Fonte varredura = FonteDeTexto("o.fj", textos[t]);
        Fonte tabela = FonteDeTexto("t.fj", textos[t]);
        indexou_todos = FonteIndexarLinhas(&tabela) && indexou_todos;
        for (uint32_t off = 0; off <= (uint32_t)varredura.tam + 3; off++) {
            uint32_t l1, c1, l2, c2;
            FonteLocalizar(&varredura, off, &l1, &c1);
            FonteLocalizar(&tabela, off, &l2, &c2);
            if (l1 != l2 || c1 != c2) {
                todos_iguais = false;
                fprintf(stderr, "tabela x varredura: texto %zu offset %u: (%u,%u) x (%u,%u)\n", t, off, l2, c2, l1, c1);
            }
        }
        free(tabela.inicio_linha);
    }
    VERIFICA(indexou_todos);
    VERIFICA(todos_iguais);
}

/* FonteLer: BOM UTF-8 sai do texto (linha/coluna batem com o editor); sem BOM nada muda; NUL final sempre presente. */
static void TestaFonteLerBom(void)
{
    const char *caminho = "teste_common_fonte_tmp.fj";
    const char com_bom[] = "\xEF\xBB\xBF" "ab\ncd\n";
    const char sem_bom[] = "ab\ncd\n";
    const char so_bom[] = "\xEF\xBB\xBF";
    const char bom_curto[] = "\xEF\xBB";
    Fonte f;
    uint32_t l, c;

    VERIFICA(ArquivoGravar(caminho, com_bom, sizeof com_bom - 1) && FonteLer(&f, caminho));
    VERIFICA(f.tam == 6 && f.texto[0] == 'a' && f.texto[6] == '\0');
    FonteLocalizar(&f, 3, &l, &c);
    VERIFICA(l == 2 && c == 1);
    FonteLiberar(&f);

    VERIFICA(ArquivoGravar(caminho, sem_bom, sizeof sem_bom - 1) && FonteLer(&f, caminho));
    VERIFICA(f.tam == 6 && memcmp(f.texto, "ab\ncd\n", 6) == 0 && f.texto[6] == '\0');
    FonteLiberar(&f);

    VERIFICA(ArquivoGravar(caminho, so_bom, sizeof so_bom - 1) && FonteLer(&f, caminho));
    VERIFICA(f.tam == 0 && f.texto[0] == '\0');
    FonteLiberar(&f);

    VERIFICA(ArquivoGravar(caminho, bom_curto, sizeof bom_curto - 1) && FonteLer(&f, caminho));
    VERIFICA(f.tam == 2 && (unsigned char)f.texto[0] == 0xEF);
    FonteLiberar(&f);

    remove(caminho);
    VERIFICA(!FonteLer(&f, caminho)); /* arquivo inexistente: recusa, sem travar */
}

static void TestaDiag(void)
{
    Fonte f = FonteDeTexto("pasta\\a.fj", "ab\ncd\n");
    Diag d = {0};
    VERIFICA(!DiagRelatar(&d, E0301, 4, "tipo \"%s\" errado", "x") && d.tem);
    FILE *arquivo_temporario = tmpfile();
    DiagImprimirJson(&d, &f, arquivo_temporario);
    rewind(arquivo_temporario);
    char linha[2048] = {0};
    VERIFICA(fgets(linha, sizeof linha, arquivo_temporario) != NULL);
    fclose(arquivo_temporario);
    VERIFICA(strstr(linha, "\"codigo\": \"E0301\"") != NULL && strstr(linha, "\"arquivo\": \"pasta/a.fj\"") != NULL);
    VERIFICA(strstr(linha, "\"linha\": 2, \"coluna\": 2") != NULL && strstr(linha, "tipo \\\"x\\\" errado") != NULL);
    VERIFICA(DiagCodigoPorTexto("E0504") == E0504 && DiagCodigoPorTexto("E9999") == -1);
    VERIFICA(strcmp(DiagCodigoTexto(E0420), "E0420") == 0);
    VERIFICA(DiagCodigoPorTexto("R0001") == R0001 && DiagCodigoPorTexto("B0002") == B0002);
    VERIFICA(strcmp(DiagCodigoTexto(R0003), "R0003") == 0 && strcmp(DiagCodigoTexto(B0001), "B0001") == 0);
}

static void TestaDimensao(void)
{
    DimTabela t;
    DimIniciar(&t);
    DimId metros = DimNovaBase(&t, 10), segundos = DimNovaBase(&t, 11);
    DimId velocidade = DimCombinar(&t, metros, segundos, -1);
    VERIFICA(metros != segundos && metros != DIM_ADIMENSIONAL && !DimDeclarada(&t, velocidade));
    DimNomear(&t, velocidade, 12);
    VERIFICA(DimDeclarada(&t, velocidade) && DimNome(&t, velocidade) == 12);
    VERIFICA(DimCombinar(&t, velocidade, segundos, 1) == metros);
    VERIFICA(DimCombinar(&t, metros, metros, -1) == DIM_ADIMENSIONAL);
    VERIFICA(DimCombinar(&t, metros, segundos, -1) == velocidade);
    for (int i = 0; i < 200; i++) DimNovaBase(&t, 20);
    VERIFICA(DimCombinar(&t, velocidade, segundos, 1) == metros && DimDeclarada(&t, DIM_ADIMENSIONAL));
    DimLiberar(&t);
}

static void TestaTipos(void)
{
    Arena a = {0};
    Interner nomes;
    DimTabela dims;
    Tipos t;
    Buf b = {0};
    InternerIniciar(&nomes, &a);
    DimIniciar(&dims);
    int32_t grifo = Internar(&nomes, "Grifo", 5), erro = Internar(&nomes, "ErroCarga", 9);
    TiposIniciar(&t, &nomes, &dims);
    VERIFICA(TipoReal(&t, DIM_ADIMENSIONAL) == TIPO_F32 && TipoClasseDe(&t, TIPO_INT) == TC_INT);
    TipoId g = TipoStruct(&t, grifo), e = TipoEnum(&t, erro);
    VERIFICA(g == TipoStruct(&t, grifo) && g != e && TipoPonteiro(&t, g) == TipoPonteiro(&t, g));
    TipoId r = TipoResultado(&t, g, e);
    VERIFICA(r == TipoResultado(&t, g, e) && r != TipoResultado(&t, TIPO_VAZIO, e));
    TipoAnexarTexto(&t, r, &b);
    BufAnexar(&b, "|");
    TipoAnexarTexto(&t, TipoPonteiro(&t, g), &b);
    BufAnexar(&b, "|");
    TipoAnexarTexto(&t, TipoResultado(&t, TIPO_VAZIO, e), &b);
    VERIFICA(strcmp(b.dados, "Grifo ! ErroCarga|^Grifo|! ErroCarga") == 0);
    BufLiberar(&b);
    TiposLiberar(&t);
    DimLiberar(&dims);
    InternerLiberar(&nomes);
    ArenaLiberar(&a);
}

/* v1.2: ids fixos 8..14, consultas por id e conferencia de literais/valores contra a largura. Cada VERIFICA e um item do contrato 9.1. */
static void TestaTiposNumericos(void)
{
    Arena a = {0};
    Interner nomes;
    DimTabela dims;
    Tipos t;
    Buf b = {0};
    uint64_t lido = 0;
    InternerIniciar(&nomes, &a);
    DimIniciar(&dims);
    TiposIniciar(&t, &nomes, &dims);
    const TipoId ids[] = {TIPO_I8, TIPO_I16, TIPO_U8, TIPO_U16, TIPO_U32, TIPO_UINT, TIPO_F64};
    const char *const textos[] = {"i8", "i16", "u8", "u16", "u32", "uint", "f64"};
    bool textos_certos = true;
    for (size_t k = 0; k < 7; k++) {
        b.tam = 0;
        TipoAnexarTexto(&t, ids[k], &b);
        textos_certos = textos_certos && ids[k] == (TipoId)(8 + k) && strcmp(b.dados, textos[k]) == 0;
    }
    b.tam = 0;
    TipoAnexarTexto(&t, TIPO_INT, &b);
    VERIFICA(textos_certos && strcmp(b.dados, "int") == 0 && TIPO_FIXOS_QTD == 17); /* (1) */
    TipoId metros = TipoReal(&t, 1);
    VERIFICA(TipoEhInteiro(TIPO_U8) && !TipoEhInteiro(TIPO_F64) && TipoEhSemSinal(TIPO_UINT) && !TipoEhSemSinal(TIPO_INT) &&
             TipoEhReal(&t, TIPO_F64) && TipoEhReal(&t, metros) && TipoEhNumerico(&t, TIPO_U32) && !TipoEhNumerico(&t, TIPO_BOOL) &&
             TipoLarguraBits(&t, TIPO_I8) == 8 && TipoLarguraBits(&t, TIPO_U16) == 16 && TipoLarguraBits(&t, TIPO_F32) == 32 &&
             TipoLarguraBits(&t, TIPO_UINT) == 64 && TipoLarguraBits(&t, TIPO_F64) == 64 && TipoLarguraBits(&t, metros) == 32 &&
             TipoLarguraBits(&t, TIPO_TEXTO) == 0); /* (2) */
    VERIFICA(TipoLerLiteralU64("18446744073709551615", &lido) && lido == UINT64_MAX && !TipoLerLiteralU64("18446744073709551616", &lido) &&
             TipoLerLiteralU64("42;", &lido) && lido == 42); /* (3) */
    VERIFICA(TipoInteiroCabe(TIPO_I8, true, 128) && !TipoInteiroCabe(TIPO_I8, false, 128) && TipoInteiroCabe(TIPO_U8, false, 255) &&
             !TipoInteiroCabe(TIPO_U8, false, 256) && TipoInteiroCabe(TIPO_U32, true, 0) && !TipoInteiroCabe(TIPO_U32, true, 1) &&
             TipoInteiroCabe(TIPO_UINT, false, UINT64_MAX) && TipoInteiroCabe(TIPO_INT, true, (uint64_t)INT64_MAX + 1u)); /* (4) */
    VERIFICA(TipoBitsCabem(TIPO_UINT, -1) && !TipoBitsCabem(TIPO_U8, 256) && TipoBitsCabem(TIPO_BOOL, 99) &&
             !TipoBitsCabem(TIPO_I8, 128) && TipoBitsCabem(TIPO_I8, -128) && !TipoBitsCabem(TIPO_I16, -32769)); /* (5) */
    VERIFICA(TipoInteiroMin(TIPO_I16) == -32768 && TipoInteiroMax(TIPO_I16) == 32767 && TipoInteiroMin(TIPO_U16) == 0 &&
             TipoInteiroMax(TIPO_U16) == 65535 && strcmp(TipoNomeFixo(TIPO_F64), "f64") == 0 && TipoNomeFixo(TIPO_FIXOS_QTD) == NULL); /* (6) */
    BufLiberar(&b);
    TiposLiberar(&t);
    DimLiberar(&dims);
    InternerLiberar(&nomes);
    ArenaLiberar(&a);
}

/* v1.3: ids 15/16 (rawptr, cstr), str = id 6, '^T' sobre qualquer tipo e '?^T'. Cada VERIFICA e um item do contrato 9.1. */
static void TestaTiposPonteiros(void)
{
    Arena a = {0};
    Interner nomes;
    DimTabela dims;
    Tipos t;
    Buf b = {0};
    InternerIniciar(&nomes, &a);
    DimIniciar(&dims);
    int32_t grifo = Internar(&nomes, "Grifo", 5);
    TiposIniciar(&t, &nomes, &dims);
    TipoId g = TipoStruct(&t, grifo);
    const TipoId ids[] = {TIPO_STR, TIPO_CSTR, TIPO_RAWPTR};
    const char *const textos[] = {"str", "cstr", "rawptr"};
    bool textos_certos = true;
    for (size_t k = 0; k < 3; k++) {
        b.tam = 0;
        TipoAnexarTexto(&t, ids[k], &b);
        textos_certos = textos_certos && strcmp(b.dados, textos[k]) == 0;
    }
    VERIFICA(TIPO_RAWPTR == 15 && TIPO_CSTR == 16 && (TipoId)TIPO_STR == (TipoId)TIPO_TEXTO && textos_certos); /* (1) */
    b.tam = 0;
    TipoAnexarTexto(&t, TipoPonteiro(&t, TIPO_I32), &b);
    BufAnexar(&b, "|");
    TipoAnexarTexto(&t, TipoPonteiro(&t, TipoPonteiro(&t, g)), &b);
    VERIFICA(strcmp(b.dados, "^i32|^^Grifo") == 0 && TipoPonteiro(&t, TipoPonteiro(&t, g)) == TipoPonteiro(&t, TipoPonteiro(&t, g))); /* (2) */
    b.tam = 0;
    TipoAnexarTexto(&t, TipoAnulavel(&t, g), &b);
    VERIFICA(strcmp(b.dados, "?^Grifo") == 0 && TipoAnulavel(&t, g) != TipoPonteiro(&t, g) &&
             TipoAnulavel(&t, g) == TipoAnulavel(&t, g)); /* (3) */
    const TipoId nao_numericos[] = {TIPO_STR, TIPO_CSTR, TIPO_RAWPTR, TipoPonteiro(&t, TIPO_I32), TipoAnulavel(&t, g)};
    bool todos_fora = true;
    for (size_t k = 0; k < 5; k++) {
        todos_fora = todos_fora && !TipoEhNumerico(&t, nao_numericos[k]) && TipoLarguraBits(&t, nao_numericos[k]) == 0;
    }
    VERIFICA(todos_fora); /* (4) */
    BufLiberar(&b);
    TiposLiberar(&t);
    DimLiberar(&dims);
    InternerLiberar(&nomes);
    ArenaLiberar(&a);
}

static void TestaDiagV13(void)
{
    const char *const codigos[] = {"E0306", "E0307", "E0706", "R0005"};
    bool todos = true;
    for (size_t k = 0; k < 4; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (5) */
}

static void TestaDiagV14(void)
{
    const char *const codigos[] = {"E0904", "E0905", "E0906", "B0004"};
    bool todos = true;
    for (size_t k = 0; k < 4; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (6) */
}

static void TestaDiagV15(void)
{
    const char *const codigos[] = {"E0308", "E0309", "E0421", "R0006"};
    bool todos = true;
    for (size_t k = 0; k < 4; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (7) */
}

static void TestaDiagV16(void)
{
    const char *const codigos[] = {"E0310", "E0311", "R0007", "R0008"};
    bool todos = true;
    for (size_t k = 0; k < 4; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (8) */
}

static void TestaDiagV17(void)
{
    const char *const codigos[] = {"E0312", "E0313", "E0314", "E0315"};
    bool todos = true;
    for (size_t k = 0; k < 4; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (9) */
}

static void TestaEscopo(void)
{
    Arena a = {0};
    Escopo e;
    EscopoIniciar(&e, &a, 8);
    LocalInfo x = {3, TIPO_INT, LOC_VAR, 0, 0, 0, 0, 0, 0}, y = {4, TIPO_BOOL, LOC_LET, 0, 1, 0, 0, 0, 0};
    EscopoEmpurrar(&e, &x);
    uint32_t altura = EscopoAltura(&e);
    EscopoEmpurrar(&e, &y);
    VERIFICA(EscopoBuscar(&e, 3)->tipo == TIPO_INT && EscopoBuscar(&e, 4)->id == 1 && EscopoBuscar(&e, 5) == NULL);
    EscopoRecuar(&e, altura);
    VERIFICA(EscopoBuscar(&e, 4) == NULL && EscopoBuscar(&e, 3) != NULL);
    EscopoLiberar(&e);
    ArenaLiberar(&a);
}

static void TestaFonteTrechos(void)
{
    char *raiz = malloc(7);
    memcpy(raiz, "ab\ncd\n", 7);
    Fonte f = FonteDeTexto("raiz.fj", raiz);
    FonteIndexarLinhas(&f);
    VERIFICA(FonteAnexarTexto(&f, "mods/x.fj", "x\nyy\n", 5) && f.ntrechos == 2 && f.trechos[1].inicio == 7 && f.tam == 12 &&
             f.texto[6] == '\n' && f.texto[12] == '\0'); /* (1) */
    FontePosicao p = FonteLocalizarNoArquivo(&f, 7);
    VERIFICA(strcmp(p.caminho, "mods/x.fj") == 0 && p.linha == 1 && p.coluna == 1); /* (2) */
    p = FonteLocalizarNoArquivo(&f, 9);
    VERIFICA(strcmp(p.caminho, "mods/x.fj") == 0 && p.linha == 2 && p.coluna == 1); /* (3) */
    p = FonteLocalizarNoArquivo(&f, 4);
    VERIFICA(strcmp(p.caminho, "raiz.fj") == 0 && p.linha == 2 && p.coluna == 2); /* (4) */
    uint32_t l, c;
    FonteLocalizar(&f, 9, &l, &c);
    VERIFICA(l == 5 && f.trechos[1].linha_base == 3); /* (5) */
    FonteLiberar(&f);
    Fonte g = FonteDeTexto("a.fj", "ab\ncd\n");
    p = FonteLocalizarNoArquivo(&g, 4);
    VERIFICA(strcmp(p.caminho, "a.fj") == 0 && p.linha == 2 && p.coluna == 2); /* (6) */
}

static void TestaDiagV18(void)
{
    const char *const codigos[] = {"E0316", "E0317", "E1001", "E1002", "E1003", "E1004", "R0009"};
    bool todos = true;
    for (size_t k = 0; k < 7; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (7) */
}

static void TestaDiagV18b(void)
{
    const char *const codigos[] = {"E0907", "E0908", "R0010", "G0001", "G0003", "G0004", "G0005", "G0006", "G0007",
                                   "G0008", "G0013", "G0014", "G0016", "G0017", "G0019", "G0021", "G0022", "G0023",
                                   "G0025", "G0026", "G0101", "G0102", "G0103", "G0104", "G0105"};
    bool todos = true;
    for (size_t k = 0; k < 25; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (8) */
}

static void TestaDiagV19(void)
{
    const char *const codigos[] = {"E0318", "E0319", "E0320", "E0321"};
    bool todos = true;
    for (size_t k = 0; k < 4; k++) {
        int c = DiagCodigoPorTexto(codigos[k]);
        todos = todos && c != -1 && strcmp(DiagCodigoTexto((DiagCodigo)c), codigos[k]) == 0;
    }
    VERIFICA(todos); /* (9) */
}

/* Uma linha do JSON do diagnostico (ate 4 KB). */
static void LinhaDoDiag(const Diag *d, const Fonte *f, char *linha, size_t cap)
{
    FILE *temporario = tmpfile();
    DiagImprimirJson(d, f, temporario);
    rewind(temporario);
    linha[0] = '\0';
    if (fgets(linha, (int)cap, temporario) == NULL) linha[0] = '\0';
    fclose(temporario);
}

/* IA1-b08: a explicacao do catalogo (forja explicar); a linha do Diag traz a explicacao da OCORRENCIA, nao a do catalogo. */
static void LinhaDoCatalogo(DiagCodigo codigo, char *linha, size_t cap)
{
    FILE *temporario = tmpfile();
    DiagExplicarJson(codigo, temporario);
    rewind(temporario);
    linha[0] = '\0';
    if (fgets(linha, (int)cap, temporario) == NULL) linha[0] = '\0';
    fclose(temporario);
}

/* v1.11 L1: E0330 no catalogo, Diag.extras na ordem fixa depois de forma_certa e o extra "sugestao" (zerado a cada DiagRelatar).
 * v1.11 L2: (4) o extra "cadeia" do E0706 fecha a linha JSON, depois de forma_certa. */
static void TestaDiagV111(void)
{
    Fonte f = FonteDeTexto("a.fj", "let x: i32 = 1.5\n");
    Diag d = {0};
    char linha[4096];
    int c = DiagCodigoPorTexto("E0330");
    VERIFICA(c == E0330 && strcmp(DiagCodigoTexto(E0330), "E0330") == 0); /* (1) */
    DiagRelatar(&d, E0301, 13, "tipo diferente");
    DiagAcrescentarExtra(&d, "sugestao", "\"1.5 as i32\"");
    DiagAcrescentarExtra(&d, "cadeia", "[\"x (local, 1:5)\"]");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    const char *forma = strstr(linha, "\"forma_certa\"");
    const char *sugestao = strstr(linha, "\"sugestao\": \"1.5 as i32\"");
    const char *cadeia = strstr(linha, "\"cadeia\": [");
    VERIFICA(forma != NULL && sugestao != NULL && cadeia != NULL && forma < sugestao && sugestao < cadeia); /* (2) */
    DiagRelatar(&d, E0301, 13, "outro");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(strstr(linha, "\"sugestao\"") == NULL && strstr(linha, "\"causa\": \"outro\"") != NULL); /* (3) */
    DiagRelatar(&d, E0706, 13, "'x' e local de 'caixa' e o endereco sai por return: x -> p -> return");
    DiagAcrescentarExtra(&d, "cadeia", "[\"x (local, 2:9)\", \"p (3:9)\", \"return (4:5)\"]");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    forma = strstr(linha, "\"forma_certa\"");
    cadeia = strstr(linha, "\"cadeia\": [\"x (local, 2:9)\", \"p (3:9)\", \"return (4:5)\"], \"correcao"); /* b09: o spec vem depois dos extras; Z z5: o pacote sem sugestao (correcao []) vem logo depois dos extras */
    VERIFICA(forma != NULL && cadeia != NULL && forma < cadeia && strstr(linha, "x -> p -> return") != NULL); /* (4) E0706 com a cadeia no JSON */
}

/* IA1-b04: sugestoes por ocorrencia. (1) a 3a sugestao e a edicao fora dos limites sao recusadas; (2) o JSON traz trecho, correcao, confianca, alternativas, spec e id; (3) a 2a vez do
 * codigo troca o spec por spec_ref; (4) DiagRelatar zera as sugestoes (o diagnostico sem sugestao sai sem o pacote). */
static void TestaDiagSugestao(void)
{
    Fonte f = FonteDeTexto("a.fj", "proc main() {\n    var n = 3\n}\n");
    Diag d = {0};
    char linha[8192];
    char longo[DIAG_NOVO_MAX + 2];
    memset(longo, 'x', sizeof longo - 1);
    longo[sizeof longo - 1] = '\0';
    DiagRelatar(&d, E0707, 22, "'n' nunca e alterada");
    VERIFICA(!DiagEditar(&d.sug, 18, 21, "let")); /* (1) sem sugestao aberta */
    bool abriu = DiagSugerir(&d.sug, DIAG_CERTA, "troque var por let") && DiagEditar(&d.sug, 18, 21, "let");
    abriu = abriu && DiagSugerir(&d.sug, DIAG_SO_DICA, "ou use o valor");
    VERIFICA(abriu && !DiagSugerir(&d.sug, DIAG_PROVAVEL, "terceira") && !DiagEditar(&d.sug, 18, 21, longo) && !DiagEditar(&d.sug, 0, DIAG_ANTIGO_MAX + 1, "")); /* (1) */
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(strstr(linha, "\"trecho\": [\"1 | proc main() {\", \"2 |     var n = 3\", \"  |         ^\", \"3 | }\"]") != NULL &&
             strstr(linha, "\"correcao\": [{\"arquivo\": \"a.fj\", \"linha\": 2, \"coluna\": 5, \"coluna_fim\": 8, \"antigo\": \"var\", \"novo\": \"let\"}], \"confianca\": \"certa\"") != NULL &&
             strstr(linha, "\"alternativas\": [{\"confianca\": \"so_dica\", \"sugestao\": \"ou use o valor\", \"correcao\": []}]") != NULL && strstr(linha, "\"spec\": ") != NULL &&
             strstr(linha, "\"id\": \"E0707@a.fj:2:9\"}") != NULL); /* (2) */
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(strstr(linha, "\"spec_ref\": \"forja explicar E0707\"") != NULL && strstr(linha, "\"spec\": ") == NULL); /* (3) */
    DiagRelatar(&d, E0707, 22, "outra");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(strstr(linha, "\"correcao\": [], \"confianca\": \"so_dica\"") != NULL && strstr(linha, "\"alternativas\"") == NULL && strstr(linha, "\"id\": \"E0707@a.fj:2:9\"}") != NULL); /* (4) Z z5: sem sugestao o pacote sai com correcao [] e so_dica */
}

/* IA1-b14: relacionados por ocorrencia. (1) o 6o e o repetido (mesmo papel e offset) sao recusados; (2) o pacote com sugestao traz papel, arquivo, linha, coluna, texto (a linha sem
 * indentacao e sem o '{') e nota; (3) sem sugestao, o campo sai antes do spec; (4) linha de mais de 120 bytes vira o corte com "..."; (5) DiagRelatar zera os relacionados;
 * (6) ParecidosVerEm guarda a posicao do candidato e ParecidosVer deixa PARECIDO_SEM_POS. */
static void TestaDiagRelacionados(void)
{
    Fonte f = FonteDeTexto("a.fj", "proc dobrar(x: i32) -> i32 {\n    return x\n}\n");
    uint32_t off_dobrar = (uint32_t)(strstr(f.texto, "dobrar") - f.texto);
    Diag d = {0};
    Parecidos p;
    char linha[8192], longa[300];
    const char *relacionados, *spec;

    DiagRelatar(&d, E0502, 40, "x");
    bool cabem = true;
    for (uint32_t k = 0; k < DIAG_REL_MAX; k++) cabem = cabem && DiagRelacionar(&d, DIAG_REL_PARECIDO, off_dobrar + k, "n%u", k);
    VERIFICA(cabem && !DiagRelacionar(&d, DIAG_REL_PARECIDO, off_dobrar + 9, "sexto") && d.rel.n == DIAG_REL_MAX); /* (1) */
    DiagRelatar(&d, E0502, 40, "x");
    VERIFICA(d.rel.n == 0 && DiagRelacionar(&d, DIAG_REL_ASSINATURA, off_dobrar, "assinatura de dobrar") && !DiagRelacionar(&d, DIAG_REL_ASSINATURA, off_dobrar, "repetido") &&
             DiagRelacionar(&d, DIAG_REL_TIPO, off_dobrar, "outro papel no mesmo offset")); /* (1) */
    d.rel.n = 1;
    DiagSugerir(&d.sug, DIAG_SO_DICA, "dica");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(strstr(linha, "\"relacionados\": [{\"papel\": \"assinatura\", \"arquivo\": \"a.fj\", \"linha\": 1, \"coluna\": 6, \"texto\": \"proc dobrar(x: i32) -> i32\", "
                           "\"nota\": \"assinatura de dobrar\"}], \"spec") != NULL); /* (2) */
    d.sug.n = 0;
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    relacionados = strstr(linha, "\"relacionados\": [{\"papel\": \"assinatura\"");
    spec = strstr(linha, "\"spec");
    VERIFICA(relacionados != NULL && spec != NULL && relacionados < spec && strstr(linha, "\"correcao\": []") != NULL); /* (3) Z z5: sem sugestao, correcao [] */
    memset(longa, 'y', sizeof longa - 1);
    longa[sizeof longa - 1] = '\0';
    f = FonteDeTexto("b.fj", longa);
    DiagRelatar(&d, E0502, 0, "x");
    DiagRelacionar(&d, DIAG_REL_DECLARACAO, 0, "longa");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(strstr(linha, "\"texto\": \"yyyyyyyyyy") != NULL && strstr(linha, "yyyy...\", \"nota\": \"longa\"") != NULL && strstr(linha, "yyyyy....") == NULL); /* (4) */
    DiagRelatar(&d, E0502, 0, "outra");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(d.rel.n == 0 && strstr(linha, "\"relacionados\": []") != NULL); /* (5) Z z5: sem relacionados o campo sai vazio */
    ParecidosIniciar(&p, "vida", 4);
    ParecidosVerEm(&p, "vidas", 5, 77);
    ParecidosVer(&p, "vidal", 5);
    VERIFICA(p.qtd == 2 && p.nomes[0].pos == PARECIDO_SEM_POS && p.nomes[1].pos == 77); /* (6) mesma distancia: "vidal" vem antes de "vidas" pelo nome */
}

/* v1.11 L16b: R0011 no catalogo (tipo runtime, causa do chamador, explicacao e forma_certa do contrato). */
static void TestaDiagQueda(void)
{
    Fonte f = FonteDeTexto("a.fj", "test \"x\" { }\n");
    Diag d = {0};
    char linha[4096], catalogo[4096];
    DiagRelatar(&d, R0011, 0, "o teste \"x\" causou uma excecao do processador: acesso a memoria invalida");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    LinhaDoCatalogo(R0011, catalogo, sizeof catalogo);
    VERIFICA(DiagCodigoPorTexto("R0011") == R0011 && strstr(linha, "\"tipo\": \"runtime\"") != NULL && strstr(catalogo, "excecao do processador (acesso a memoria invalida ou instrucao ilegal)") != NULL &&
             strstr(linha, "rawptr convertido com 'as ^T' no teste apontado") != NULL); /* (1) */
}

/* v1.11 L9: E0328 e E0329 no catalogo (tipo layout, causa do chamador, explicacao e forma_certa do contrato). */
static void TestaDiagLayout(void)
{
    Fonte f = FonteDeTexto("a.fj", "static_assert(size_of(int) == 8)\n");
    Diag d = {0};
    char linha[4096];
    DiagRelatar(&d, E0328, 0, "size_of(Janela): tipo opaco");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(DiagCodigoPorTexto("E0328") == E0328 && strstr(linha, "\"tipo\": \"layout\"") != NULL &&
             strstr(linha, "\"causa\": \"size_of(Janela): tipo opaco\"") != NULL && strstr(linha, "offset_of(S.campo)") != NULL); /* (1) */
    DiagRelatar(&d, E0329, 0, "static_assert falhou: msg");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    VERIFICA(DiagCodigoPorTexto("E0329") == E0329 && strstr(linha, "\"tipo\": \"layout\"") != NULL &&
             strstr(linha, "\"causa\": \"static_assert falhou: msg\"") != NULL && strstr(linha, "assert(x > 0) para valor de execucao") != NULL); /* (2) */
}

/* v1.11 L10a: um codigo da higiene no catalogo: tipo, causa do chamador e um trecho da explicacao e da forma_certa do contrato. */
static bool CodigoDeHigieneConfere(DiagCodigo codigo, const char *tipo, const char *trecho_explicacao, const char *trecho_forma)
{
    Fonte f = FonteDeTexto("a.fj", "var n = 0\n");
    Diag d = {0};
    char linha[4096], catalogo[4096];
    char esperado[64];
    snprintf(esperado, sizeof esperado, "\"tipo\": \"%s\"", tipo);
    DiagRelatar(&d, codigo, 0, "causa de teste");
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    LinhaDoCatalogo(codigo, catalogo, sizeof catalogo);
    return DiagCodigoPorTexto(DiagCodigoTexto(codigo)) == (int)codigo && strstr(linha, esperado) != NULL &&
           strstr(linha, "\"causa\": \"causa de teste\"") != NULL && strstr(catalogo, trecho_explicacao) != NULL && strstr(linha, trecho_forma) != NULL;
}

/* v1.11 L10a: E0707, E0708, E0709 (tipo fluxo) e E1005 (tipo modulo) no catalogo. */
static void TestaDiagHigiene(void)
{
    VERIFICA(CodigoDeHigieneConfere(E0707, "fluxo", "nunca e alterada", "troque var por let")); /* (1) */
    VERIFICA(CodigoDeHigieneConfere(E0708, "fluxo", "nunca e lido", "_ = registrar(x) para descartar um retorno")); /* (2) */
    VERIFICA(CodigoDeHigieneConfere(E0709, "fluxo", "nao some em silencio", "_ = puts(")); /* (3) */
    VERIFICA(CodigoDeHigieneConfere(E1005, "modulo", "apelido nunca aparece", "remova a linha do import")); /* (4) */
    VERIFICA(CodigoDeHigieneConfere(E0707, "fluxo", "vale pela declaracao inteira", "var n = 0")); /* (5) v1.11 L10a-bis: var multipla */
    VERIFICA(CodigoDeHigieneConfere(E0708, "fluxo", "(_r, _unused)", "let _r = f() para guardar sem ler")); /* (6) v1.11 L10a-bis: prefixo _ */
    VERIFICA(CodigoDeHigieneConfere(E0709, "fluxo", "'_ = try f()'", "_ = try ler(f)")); /* (7) v1.11 L10a-bis: descarte de T ! E */
    VERIFICA(CodigoDeHigieneConfere(E0601, "resultado", "tratado", "_ = try f() (repassa o erro e descarta o valor)")); /* (8) v1.11 L10a-bis */
    VERIFICA(CodigoDeHigieneConfere(E1006, "modulo", "palavra reservada", "as <nome>")); /* (9) IA1-b01: import com ultimo segmento reservado */
}

typedef struct { uint64_t m[3], semente[3]; } ResumosDeBrinquedo;

/* O proc i (de 3, cada um chamando o seguinte, o ultimo chamando o 1o) herda o resumo do callee: a mesma conta do resumo de verdade. */
static bool RodadaDeBrinquedo(void *ctx, uint32_t i)
{
    ResumosDeBrinquedo *r = ctx;
    uint64_t novo = r->m[i] | r->semente[i] | r->m[(i + 1) % 3];
    bool mudou = novo != r->m[i];
    r->m[i] = novo;
    return mudou;
}

static bool RodadaQueSempreCresce(void *ctx, uint32_t i)
{
    (void)ctx;
    (void)i;
    return true;
}

/* v1.11 L3: ponto fixo dos resumos por callee. A semente esta no ultimo dos 3 procs mutuamente recursivos e a ordem do fonte anda contra a
 * chamada: sao 3 voltas que crescem e 1 que confirma (teto 4 converge; teto 3 nao); rodada que sempre cresce nunca converge. */
static void TestaFugaPontoFixo(void)
{
    ResumosDeBrinquedo a = {{0, 0, 0}, {0, 0, 4}}, b = {{0, 0, 0}, {0, 0, 4}};
    bool convergiu = FugaPontoFixo(3, 4, RodadaDeBrinquedo, &a);
    bool curto = FugaPontoFixo(3, 3, RodadaDeBrinquedo, &b);
    VERIFICA(convergiu && a.m[0] == 4 && a.m[1] == 4 && a.m[2] == 4 && !curto && !FugaPontoFixo(3, 4, RodadaQueSempreCresce, &a));
}

/* v1.8b P5a: TipoProc (hash-consing, texto e consultas). Contrato 10.1. */
static void TestaTipoProc(void)
{
    Arena ar = {0};
    Interner nomes;
    DimTabela dims;
    Tipos t;
    Buf b = {0};
    InternerIniciar(&nomes, &ar);
    DimIniciar(&dims);
    TiposIniciar(&t, &nomes, &dims);
    const TipoId ps[] = {TIPO_I32, TIPO_RAWPTR};
    const int32_t nm1[] = {5, 6}, nm2[] = {7, 8};
    const ProcAssinatura sa = {ps, nm1, 2, TIPO_I32}, sv = {ps, nm1, 2, TIPO_VAZIO}, sz = {NULL, NULL, 0, TIPO_VAZIO}, sd = {ps, nm2, 2, TIPO_I32};
    TipoId a = TipoProc(&t, &sa), c = TipoProc(&t, &sa);
    TipoId v = TipoProc(&t, &sv), z = TipoProc(&t, &sz), d = TipoProc(&t, &sd);
    VERIFICA(a == c && a != v && TipoClasseDe(&t, a) == TC_PROC); /* (1) */
    VERIFICA(a != d && TipoProcCompativel(&t, a, d) && !TipoProcCompativel(&t, a, v) && TipoProcNome(&t, d, 1) == 8 && TipoProcNome(&t, a, 0) == 5); /* (4) v1.12: o nome entra na identidade, a compatibilidade o ignora */
    TipoAnexarTexto(&t, a, &b);
    BufAnexar(&b, "|");
    TipoAnexarTexto(&t, v, &b);
    BufAnexar(&b, "|");
    TipoAnexarTexto(&t, z, &b);
    VERIFICA(strcmp(b.dados, "proc(i32, rawptr) -> i32|proc(i32, rawptr)|proc()") == 0); /* (2) */
    VERIFICA(TipoProcQtd(&t, a) == 2 && TipoProcParam(&t, a, 1) == TIPO_RAWPTR && TipoDe(&t, a)->a == TIPO_I32 &&
             TipoProcQtd(&t, z) == 0); /* (3) */
    BufLiberar(&b);
    TiposLiberar(&t);
    DimLiberar(&dims);
    InternerLiberar(&nomes);
    ArenaLiberar(&ar);
}

/* v1.9 P3: TipoMapa e TipoMatriz (hash-consing, flags e texto). Contrato 10.10 (b). */
static void TestaTiposV19(void)
{
    Arena ar = {0};
    Interner nomes;
    DimTabela dims;
    Tipos t;
    Buf b = {0};
    InternerIniciar(&nomes, &ar);
    DimIniciar(&dims);
    TiposIniciar(&t, &nomes, &dims);
    TipoId mapa = TipoMapa(&t, TIPO_TEXTO, TIPO_INT), outro = TipoMapa(&t, TIPO_TEXTO, TIPO_INT);
    VERIFICA(mapa == outro && TipoClasseDe(&t, mapa) == TC_MAPA && t.tem_mapa && t.tem_dinamico && TiposTemV19(&t)); /* (1) */
    TipoId m = TipoMatriz(&t, TIPO_F32, 4, 3);
    TipoAnexarTexto(&t, m, &b);
    BufAnexar(&b, "|");
    TipoAnexarTexto(&t, mapa, &b);
    VERIFICA(TipoMatrizLinhas(&t, m) == 4 && TipoMatrizColunas(&t, m) == 3 &&
             strcmp(b.dados, "matrix[4, 3]f32|map[str]int") == 0); /* (2) */
    BufLiberar(&b);
    TiposLiberar(&t);
    DimLiberar(&dims);
    InternerLiberar(&nomes);
    ArenaLiberar(&ar);
}

/* v1.11 L5 (B02): transposicao conta 1; lista vazia; empate ordena pelo nome; limite = min(2, max(1, len/3)). */
static void TestaParecido(void)
{
    Parecidos p;
    VERIFICA(Parecido("vida", 4, "vdia", 4) == 1);
    ParecidosIniciar(&p, "vida", 4);
    VERIFICA(p.qtd == 0);
    ParecidosIniciar(&p, "abcdef", 6);
    ParecidosVer(&p, "abcdez", 6);
    ParecidosVer(&p, "abcdeg", 6);
    VERIFICA(p.qtd == 2 && strcmp(p.nomes[0].texto, "abcdeg") == 0 && strcmp(p.nomes[1].texto, "abcdez") == 0);
    ParecidosIniciar(&p, "vida", 4);
    ParecidosVer(&p, "vidaxx", 6);
    ParecidosVer(&p, "vidas", 5);
    VERIFICA(p.qtd == 1 && strcmp(p.nomes[0].texto, "vidas") == 0);
}

/* P2b: o rt_win.o embutido (nativo\rt_embutido.c) tem de ser um objeto COFF AMD64: Machine = 0x8664 em little-endian. */
static void TestaRtNativoWin(void)
{
    size_t n = 0, n_rel = 0;
    const uint8_t *obj = RtNativoWin(BM_DEV, &n);
    const uint8_t *obj_rel = RtNativoWin(BM_RELEASE, &n_rel);
    VERIFICA(n > 0 && n_rel > 0);
    VERIFICA(n >= 2 && obj[0] == 0x64 && obj[1] == 0x86 && n_rel >= 2 && obj_rel[0] == 0x64 && obj_rel[1] == 0x86);
}

/* v1.11 L6: coluna_fim do E-code = coluna + bytes do token (exclusivo): identificador, numero, texto, operador de 2 caracteres e
 * fim de arquivo (sem token, coluna + 1). Confere o campo na linha JSON, logo antes de "causa". */
static bool ColunaFimDoToken(const char *texto, uint32_t off, uint32_t fim_esperado)
{
    Fonte f = FonteDeTexto("cf.fj", texto);
    Diag d = {0};
    char linha[4096], esperado[64];

    d.tem = true;
    d.codigo = E0201;
    d.off = off;
    LinhaDoDiag(&d, &f, linha, sizeof linha);
    snprintf(esperado, sizeof esperado, "\"coluna_fim\": %u, \"trecho\"", fim_esperado); /* Z z5: o trecho sai em todo diagnostico, entre coluna_fim e causa */
    return strstr(linha, esperado) != NULL;
}

static void TestaColunaFim(void)
{
    const char *texto = "abcd = 1.5 \"oi\" a == b";
    VERIFICA(ColunaFimDoToken(texto, 0, 5) && ColunaFimDoToken(texto, 7, 11) && ColunaFimDoToken(texto, 11, 16)
             && ColunaFimDoToken(texto, 18, 21) && ColunaFimDoToken(texto, 22, 24));
}

static void GuardarDeTeste(DiagLista *l, uint32_t modulo, uint32_t off, const char *causa)
{
    Diag d = {0};
    DiagRelatar(&d, E0201, off, "%s", causa);
    DiagListaGuardar(l, &d, modulo);
}

/* v1.11 L7: DiagListaOrdenar (modulo, depois off, estavel: empate fica na ordem de guarda) e o teto (teto 0 = padrao 20; cortado e nada entra alem dele). */
static void TestaDiagLista(void)
{
    static DiagLista l;
    Diag d = {0};
    memset(&l, 0, sizeof l);
    GuardarDeTeste(&l, 1, 5, "e");
    GuardarDeTeste(&l, 0, 9, "a");
    GuardarDeTeste(&l, 1, 2, "d");
    GuardarDeTeste(&l, 0, 9, "b");
    DiagListaOrdenar(&l);
    VERIFICA(l.n == 4 && strcmp(l.itens[0].causa, "a") == 0 && strcmp(l.itens[1].causa, "b") == 0 && strcmp(l.itens[2].causa, "d") == 0
             && strcmp(l.itens[3].causa, "e") == 0);
    memset(&l, 0, sizeof l);
    l.teto = 2;
    GuardarDeTeste(&l, 0, 1, "x");
    GuardarDeTeste(&l, 0, 2, "y");
    bool cabe_antes = l.n == 2 && !l.cortado;
    DiagRelatar(&d, E0201, 3, "z");
    bool recusou = !DiagListaGuardar(&l, &d, 0) && !d.tem && l.n == 2 && l.cortado;
    memset(&l, 0, sizeof l);
    for (uint32_t k = 0; k < 25; k++) GuardarDeTeste(&l, 0, k, "p");
    VERIFICA(cabe_antes && recusou && l.n == DIAG_TETO_PADRAO && l.cortado);
}

/* v1.11 L8: diagnostico calado (cascata de declaracao envenenada) limpa o Diag e nao entra na lista, nem conta no teto; DiagRelatar zera o calado do reuso. */
static void TestaDiagCalado(void)
{
    static DiagLista l;
    Diag d = {0};
    memset(&l, 0, sizeof l);
    l.teto = 1;
    DiagRelatar(&d, E0301, 1, "cascata");
    d.calado = true;
    bool silenciou = DiagListaGuardar(&l, &d, 0) && !d.tem && !d.calado && l.n == 0 && !l.cortado;
    DiagRelatar(&d, E0201, 2, "origem");
    d.calado = true;
    DiagRelatar(&d, E0201, 3, "origem");
    bool entrou = DiagListaGuardar(&l, &d, 0) && l.n == 1 && l.itens[0].off == 3 && !l.itens[0].calado;
    VERIFICA(silenciou && entrou && !l.cortado);
}

int main(void)
{
    TestaDimensao();
    TestaTipos();
    TestaTiposNumericos();
    TestaTiposPonteiros();
    TestaTipoProc();
    TestaDiagV13();
    TestaDiagV14();
    TestaDiagV15();
    TestaDiagV16();
    TestaDiagV17();
    TestaEscopo();
    TestaArena();
    TestaBuf();
    TestaVec();
    TestaIntern();
    TestaFonte();
    TestaFonteTabelaContraVarredura();
    TestaFonteLerBom();
    TestaDiag();
    TestaFonteTrechos();
    TestaDiagV18();
    TestaDiagV18b();
    TestaParecido();
    TestaDiagV19();
    TestaDiagV111();
    TestaDiagQueda();
    TestaDiagSugestao();
    TestaDiagRelacionados();
    TestaDiagLayout();
    TestaDiagHigiene();
    TestaFugaPontoFixo();
    TestaTiposV19();
    TestaRtNativoWin();
    TestaColunaFim();
    TestaDiagLista();
    TestaDiagCalado();
    AtomicoVerificacoes(&g_total, &g_falhas);
    ProjetoVerificacoes(&g_total, &g_falhas);
    EdicoesVerificacoes(&g_total, &g_falhas);
    SpecVerificacoes(&g_total, &g_falhas);
    RenomeadosVerificacoes(&g_total, &g_falhas);
    NomeModuloVerificacoes(&g_total, &g_falhas);
    if (g_falhas != 0) {
        fprintf(stderr, "%d de %d verificacoes falharam\n", g_falhas, g_total);
        return 1;
    }
    printf("OK: %d verificacoes\n", g_total);
    return 0;
}
