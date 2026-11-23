/* gerar_bir_verifica.c - verificacoes de unidade da BIR (BirVerificacoes, chamada pelo teste_nativo): regras, dev x release, validador, guardas, layout e ABI, resultado e chaveia, ?T.
 * Versao: 0.8.3
 * Changelog: 0.8.3 - 0.21.1 M1: o dump do map[int]int traz forja_mapa_inserir_k8 (chave int de 8 bytes por valor), nao mais forja_mapa_inserir (mesma contagem).
 *            0.8.2 - 0.21.0 F4 residuo: JSON_X0906_ADULTERADO com explicacao e forma_certa do catalogo atual do X0906 (diag.c 0.75.0 e 0.76.0 tiraram "use --backend c"); mesma contagem.
 *            0.8.1 - IA1-b09: JsonDoDiagIgual aceita o campo "spec" no fim do JSON do X0906 (mesma contagem).
 *            0.8.0 - M3 P21v-c: VerificaChamadaInd (+3: chama_ind com end @fjt_ em dev e release, guarda de NULL so em dev) e o estrago do CHAMA_IND no validador (+2): +5 no total.
 *            0.7.1 - M3 P21v-b: VerificaRegras percorre o enum ate IR_CHAMADA_IND (ultimo op, ir.h 0.16.0): +1 verificacao.
 *            0.7.0 - M3 P21: o callback deixou de ser X0001 (nao ha mais X0001 real): VerificaDedupEX0001 vira VerificaDedupE (-2), e VerificaTrampolim (+3: fjt_ e V_PROC, R0005 do ^T em dev, release sem guarda)
 *                    e VerificaTrampolimComUnwind (+1: RUNTIME_FUNCTION do fjt_) entram: +2 no total.
 *            0.6.0 - M3 P19: o X0001 de VerificaDedupEX0001 passa da chamada a foreign (agora suportada) para o callback (proc da Forja passado a foreign, marco M2); mesma contagem.
 *            0.5.0 -Linguagem v1.11 L13: VerificaRaiz (+1: sqrt vira BIR_RAIZ em f32 e f64 e passa no validador).
 *            0.4.0 - M2 P17b: VerificaMapaESoa (+1: map e soa dinamica viram CHAMA ao runtime e o forja_rt_layout v3 confere o offset de valores do map e o cabecalho da soa); RTL_V3 (forja_rt_layout de 23 palavras)
 *                    nos testes de X0906 existentes; o X0001 de VerificaDedupEX0001 passa de map (agora suportado) para a chamada a foreign (M3).
 *            0.3.0 - M2 P16: VerificaConjuntoEMatriz (+1: len de bit_set CHAMA forja_rt_conta_bits, has com membro variavel passa pela guarda do SHL, o produto de matrix e desenrolado com soma a partir de 0).
 *            0.2.0 - M2 P15b: VerificaFormata (+1: IR_FORMATA vira forja_fmt_inicio, uma forja_fmt_<tipo> por parte com o formato, f32 alargado a double e o final imprimir/str_final).
 *            0.1.0 - M2 P15a: VerificaOpcional (+1: ?i32 grava tem e v, none zera o slot, or_else le o tem); o X0001 de VerificaDedupEX0001 passa de ?i32 para map (IR_MAPA_CRIA). As verificacoes saem de gerar_bir.c (que passou de 800 linhas) sem mudar o comportamento; o front-end vem de gerar_bir_frente.h. Historico anterior no cabecalho de gerar_bir.c
 *                    (0.2.1 a 0.10.0).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "ir/layout.h"
#include "nativo/bir/bir.h"
#include "nativo/nativo_layout.h"
#include "nativo/obj/objeto.h"
#include "nativo/x64/x64_gerar.h"
#include "nativo/x64/x64_interno.h"
#include "gerar_bir_frente.h"

/* ---- verificacoes de unidade ---- */

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (bir) linha %d: %s\n", linha, texto);
}

#define BIR_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* Fonte em memoria -> dump. false = o programa nao passou (diagnostico em *erro). */
static bool DumpDoTexto(const char *texto, uint8_t modo, Buf *dump, Diag *erro)
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    bool ok;

    f->fonte = FonteDeTexto("memoria.fj", texto);
    IniciarFrente(f);
    ok = AnalisarFrente(f);
    if (ok) {
        BirEntrada entrada = EntradaDaFrente(f, modo);

        ok = BirConstruir(&entrada, &programa, &f->diag) && BirValidar(&programa, &f->diag);
        if (ok)
            BirImprimir(&programa, dump);
        BirLiberar(&programa);
    }
    *erro = f->diag;
    LiberarFrente(f, false);
    free(f);
    return ok;
}

static bool TemTexto(const Buf *dump, const char *trecho)
{
    return dump->dados != NULL && strstr(dump->dados, trecho) != NULL;
}

static void VerificaRegras(const Contagem *c)
{
    bool visto[IR_CHAMADA_IND + 1] = {false};
    bool fn_certa = true, marco_certo = true;

    for (uint32_t k = 0; k < REGRAS_IR_QTD; k++) {
        const BirRegra *r = &REGRAS_IR[k];

        visto[r->op] = true;
        fn_certa = fn_certa && ((r->estado == BR_SUPORTADO) == (r->fn != NULL));
        marco_certo = marco_certo && (strcmp(r->marco, "M1") == 0 || strcmp(r->marco, "M2") == 0 || strcmp(r->marco, "M3") == 0);
    }
    for (int op = 0; op <= IR_CHAMADA_IND; op++)
        BIR_VERIFICA(c, visto[op]);
    BIR_VERIFICA(c, fn_certa);
    BIR_VERIFICA(c, marco_certo);
}

static const char FONTE_DOBRO[] = "proc dobro(n: int) -> int {\n    return n * 2\n}\n\nproc main() {\n    print(\"a\")\n    print(\"a\")\n    print(dobro(21))\n}\n";

static void VerificaDevERelease(const Contagem *c)
{
    Buf dev = {0}, rel = {0}, dev2 = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_DOBRO, BM_DEV, &dev, &erro));
    BIR_VERIFICA(c, TemTexto(&dev, "v1:i64 = mul_ov.s v0, #2:i64 ; frio b1 ; 2:14"));
    BIR_VERIFICA(c, TemTexto(&dev, "armadilha @forja_falha_estouro(v2, v0, #2:i64, v3, #2:i32) ; 2:14"));
    BIR_VERIFICA(c, TemTexto(&dev, "s0 = \"multiplicacao i64\""));
    BIR_VERIFICA(c, TemTexto(&dev, "funcao forja_programa() -> i32"));
    BIR_VERIFICA(c, DumpDoTexto(FONTE_DOBRO, BM_DEV, &dev2, &erro));
    BIR_VERIFICA(c, dev.tam == dev2.tam && memcmp(dev.dados, dev2.dados, dev.tam) == 0);
    BIR_VERIFICA(c, DumpDoTexto(FONTE_DOBRO, BM_RELEASE, &rel, &erro));
    BIR_VERIFICA(c, TemTexto(&rel, "v1:i64 = mul v0, #2:i64") && !TemTexto(&rel, "_ov"));
    BufLiberar(&dev);
    BufLiberar(&dev2);
    BufLiberar(&rel);
}

/* Proc da Forja passado a um foreign (callback): desde o M3 P21 o trampolim fjt_igual entra na BIR; o ^T do callback e conferido em dev (R0005). */
static const char FONTE_CALLBACK[] = "foreign {\n    proc qsort(base: rawptr, n: u64, tamanho: u64, comparar: proc(a: rawptr, b: rawptr) -> i32)\n}\n\n"
                                     "proc igual(a: rawptr, b: rawptr) -> i32 {\n    return 0\n}\n\nproc main() {\n    var v: [2]i32 = {2, 1}\n"
                                     "    qsort(&v[0] as rawptr, n = 2, tamanho = 4, comparar = igual)\n}\n";
static const char FONTE_CALLBACK_PONTEIRO[] = "foreign {\n    proc chama(cb: proc(p: ^i32, n: i32))\n}\n\n"
                                              "proc ao_chamar(p: ^i32, n: i32) {\n    p^ = n\n}\n\nproc main() {\n    chama(ao_chamar)\n}\n";

static void VerificaDedupE(const Contagem *c)
{
    Buf dump = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_DOBRO, BM_DEV, &dump, &erro));
    BIR_VERIFICA(c, TemTexto(&dump, "@s0") && !TemTexto(&dump, "  s3 = ")); /* "a" uma vez so */
    BufLiberar(&dump);
}

/* P21: o V_PROC vira END_SIMBOLO de fjt_<proc>; o trampolim chama fj_<proc>; em dev ^T vindo do C passa pela guarda de NULL (R0005 com o texto do backend C), em release nao. */
static void VerificaTrampolim(const Contagem *c)
{
    Buf dev = {0}, ponteiro_dev = {0}, ponteiro_rel = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_CALLBACK, BM_DEV, &dev, &erro) && TemTexto(&dev, "funcao fjt_igual(") && TemTexto(&dev, "end @fjt_igual") && TemTexto(&dev, "chama @fj_igual("));
    BIR_VERIFICA(c, DumpDoTexto(FONTE_CALLBACK_PONTEIRO, BM_DEV, &ponteiro_dev, &erro) && TemTexto(&ponteiro_dev, "armadilha @forja_falha_nulo(")
                        && TemTexto(&ponteiro_dev, "parametro 'p' do callback 'ao_chamar'"));
    BIR_VERIFICA(c, DumpDoTexto(FONTE_CALLBACK_PONTEIRO, BM_RELEASE, &ponteiro_rel, &erro) && TemTexto(&ponteiro_rel, "funcao fjt_ao_chamar(") && !TemTexto(&ponteiro_rel, "forja_falha_nulo"));
    BufLiberar(&dev);
    BufLiberar(&ponteiro_dev);
    BufLiberar(&ponteiro_rel);
}

static const char FONTE_CHAMADA_IND[] = "proc dobro(x: i32) -> i32 {\n    return x * 2\n}\n\nproc main() {\n    let f = dobro\n    print(f(21))\n}\n";

/* P21v-c: f(21) sobre um proc em let vira CHAMA_IND com o endereco (END_SIMBOLO de fjt_dobro) num vreg; em dev o alvo passa pela guarda de NULL (R0005 "chamada por proc nulo"), em release nao. */
static void VerificaChamadaInd(const Contagem *c)
{
    Buf dev = {0}, rel = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_CHAMADA_IND, BM_DEV, &dev, &erro) && TemTexto(&dev, "end @fjt_dobro") && TemTexto(&dev, "= chama_ind v0(#21:i32)"));
    BIR_VERIFICA(c, TemTexto(&dev, "armadilha @forja_falha_nulo(") && TemTexto(&dev, "chamada por proc nulo"));
    BIR_VERIFICA(c, DumpDoTexto(FONTE_CHAMADA_IND, BM_RELEASE, &rel, &erro) && TemTexto(&rel, "= chama_ind v") && !TemTexto(&rel, "forja_falha_nulo"));
    BufLiberar(&dev);
    BufLiberar(&rel);
}

/* O objeto gerado traz um RUNTIME_FUNCTION (.pdata com ADDR32NB ao simbolo da funcao) para o fjt_igual, como para qualquer funcao: sem ele o desenrolar de pilha para no trampolim. */
static void VerificaTrampolimComUnwind(const Contagem *c)
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    Objeto objeto;
    bool achou = false;

    f->fonte = FonteDeTexto("callback.fj", FONTE_CALLBACK);
    IniciarFrente(f);
    if (AnalisarFrente(f)) {
        BirEntrada entrada = EntradaDaFrente(f, BM_DEV);

        if (BirConstruir(&entrada, &programa, &f->diag) && BirValidar(&programa, &f->diag)) {
            ObjIniciar(&objeto);
            if (X64Gerar(&programa, &objeto, &f->diag, NULL)) {
                int64_t simbolo = ObjAcharSimbolo(&objeto, "fjt_igual");

                for (size_t k = 0; k < objeto.relocs.n && simbolo >= 0; k++) {
                    const ObjReloc *r = VecEm(&objeto.relocs, k);

                    achou = achou || (r->correcao.tipo == OR_ADDR32NB && r->correcao.simbolo == (uint32_t)simbolo && r->correcao.adendo == 0
                                      && ((const ObjSecao *)VecEm(&objeto.secoes, r->alvo.secao))->tipo == OS_PDATA);
                }
            }
            ObjLiberar(&objeto);
        }
        BirLiberar(&programa);
    }
    BIR_VERIFICA(c, achou);
    LiberarFrente(f, false);
    free(f);
}

static const char FONTE_STR_LITERAL[] = "proc main() {\n    let a: str = \"ola\"\n    print(a == \"ola\")\n    print(len(a))\n    print(\"fim\")\n}\n";

/* P13a: o literal de str e END_SIMBOLO (lea rip) mais o len gravado no slot; o objeto gerado tem relocacoes (REL32 aos textos) e nenhuma ADDR64. */
static void VerificaStrSemAddr64(const Contagem *c)
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    Objeto objeto;
    bool ok;
    bool sem_addr64 = true;

    f->fonte = FonteDeTexto("memoria.fj", FONTE_STR_LITERAL);
    IniciarFrente(f);
    ok = AnalisarFrente(f);
    if (ok) {
        BirEntrada entrada = EntradaDaFrente(f, BM_DEV);

        ok = BirConstruir(&entrada, &programa, &f->diag) && BirValidar(&programa, &f->diag);
        if (ok) {
            ObjIniciar(&objeto);
            ok = X64Gerar(&programa, &objeto, &f->diag, NULL) && objeto.relocs.n > 0;
            for (size_t k = 0; k < objeto.relocs.n; k++)
                sem_addr64 = sem_addr64 && ((const ObjReloc *)VecEm(&objeto.relocs, k))->correcao.tipo != OR_ADDR64;
            ObjLiberar(&objeto);
        }
        BirLiberar(&programa);
    }
    BIR_VERIFICA(c, ok && sem_addr64);
    LiberarFrente(f, false);
    free(f);
}

static const char FONTE_ARRAY_INDICE[] = "proc main() {\n    var a: [3]int = {1, 2, 3}\n    let i = 2\n    let s = a[0..<i]\n    print(a[i] + len(s))\n}\n";
static const char FONTE_SLICE_FROM[] = "proc main() {\n    var a: [2]i32 = {1, 2}\n    let v = slice_from(&a[0], len = 2)\n    print(len(v))\n}\n";

/* P13b: dev confere o indice (CMP sem sinal + frio forja_falha_indice) e a fatia (forja_fatia_confere); release nao tem nenhum dos dois. */
static void VerificaIndiceConferido(const Contagem *c)
{
    Buf dev = {0}, rel = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_ARRAY_INDICE, BM_DEV, &dev, &erro) && TemTexto(&dev, "cmp.ge_u") && TemTexto(&dev, "armadilha @forja_falha_indice")
                        && TemTexto(&dev, "chama @forja_fatia_confere"));
    BIR_VERIFICA(c, DumpDoTexto(FONTE_ARRAY_INDICE, BM_RELEASE, &rel, &erro) && !TemTexto(&rel, "forja_falha_indice") && !TemTexto(&rel, "forja_fatia_confere"));
    BufLiberar(&dev);
    BufLiberar(&rel);
}

/* P13b: slice_from chama forja_vetor_c_confere em dev e nao confere em release. */
static void VerificaSliceFrom(const Contagem *c)
{
    Buf dev = {0}, rel = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_SLICE_FROM, BM_DEV, &dev, &erro) && TemTexto(&dev, "chama @forja_vetor_c_confere")
                        && DumpDoTexto(FONTE_SLICE_FROM, BM_RELEASE, &rel, &erro) && !TemTexto(&rel, "forja_vetor_c_confere"));
    BufLiberar(&dev);
    BufLiberar(&rel);
}

static const char FONTE_LISTA[] = "proc main() {\n    var l = make([dynamic]int, alloc = frame, cap = 1)\n    append(&l, item = 7)\n    print(l[0] + len(l))\n    delete(l)\n}\n";

/* P14b: lista em dev chama forja_lista_viva e forja_indice_lista (R0008 e R0006); release so cresce e libera, sem nenhuma das duas. */
static void VerificaListaConferida(const Contagem *c)
{
    Buf dev = {0}, rel = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_LISTA, BM_DEV, &dev, &erro) && TemTexto(&dev, "chama @forja_lista_viva") && TemTexto(&dev, "chama @forja_indice_lista")
                        && DumpDoTexto(FONTE_LISTA, BM_RELEASE, &rel, &erro) && !TemTexto(&rel, "forja_lista_viva") && !TemTexto(&rel, "forja_indice_lista")
                        && TemTexto(&rel, "chama @forja_lista_crescer") && TemTexto(&rel, "chama @forja_lista_liberar"));
    BufLiberar(&dev);
    BufLiberar(&rel);
}

/* Estraga a BIR de proposito: o validador tem de recusar com X0904 e aceitar de volta quando desfeito. */
static void VerificaValidador(const Contagem *c)
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    Diag erro = {0};
    BirEntrada entrada;
    BirBloco *bloco;

    f->fonte = FonteDeTexto("memoria.fj", FONTE_DOBRO);
    IniciarFrente(f);
    BIR_VERIFICA(c, AnalisarFrente(f));
    entrada = EntradaDaFrente(f, BM_DEV);
    BIR_VERIFICA(c, BirConstruir(&entrada, &programa, &erro) && BirValidar(&programa, &erro));
    bloco = &programa.funcoes[0].blocos[0];
    bloco->instrs[bloco->n - 1].op = BIR_COPIA; /* bloco sem terminador */
    BIR_VERIFICA(c, !BirValidar(&programa, &erro) && erro.codigo == X0904);
    bloco->instrs[bloco->n - 1].op = BIR_RETORNA;
    BIR_VERIFICA(c, BirValidar(&programa, &erro));
    programa.funcoes[0].blocos[1].frio = false; /* destino do *_OV precisa ser frio */
    BIR_VERIFICA(c, !BirValidar(&programa, &erro));
    BirLiberar(&programa);
    LiberarFrente(f, false);
    free(f);
}

/* Primeiro "cmp vN, #imediato" com a condicao dada: o teste da guarda de um DIV ou SHL. */
static BirInstr *AcharTesteDeGuarda(BirPrograma *programa, uint8_t cond)
{
    for (uint32_t k = 0; k < programa->nfuncoes; k++) {
        for (uint32_t b = 0; b < programa->funcoes[k].nblocos; b++) {
            BirBloco *bloco = &programa->funcoes[k].blocos[b];

            for (uint32_t n = 0; n < bloco->n; n++) {
                if (bloco->instrs[n].op == BIR_CMP && bloco->instrs[n].aux == cond && bloco->instrs[n].b.tipo == BO_IMED)
                    return &bloco->instrs[n];
            }
        }
    }
    return NULL;
}

static const char FONTE_DIVISAO[] = "proc divide(a: i32, b: i32) -> i32 {\n    return a / b\n}\n\nproc main() {\n    print(divide(7, b = 2))\n}\n";
static const char FONTE_DESLOCA[] = "proc esq(a: u32, n: u32) -> u32 {\n    return a << n\n}\n\nproc main() {\n    print(esq(1, n = 4))\n}\n";

/* A BIR do programa passa no validador; trocar a condicao do teste da guarda faz o validador recusar com X0904 (DIV e SHL sem guarda). */
static void VerificaGuarda(const Contagem *c, const char *fonte, uint8_t cond)
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    Diag erro = {0};
    BirEntrada entrada;
    BirInstr *teste;

    f->fonte = FonteDeTexto("memoria.fj", fonte);
    IniciarFrente(f);
    BIR_VERIFICA(c, AnalisarFrente(f) && (entrada = EntradaDaFrente(f, BM_DEV), BirConstruir(&entrada, &programa, &erro)) && BirValidar(&programa, &erro));
    teste = AcharTesteDeGuarda(&programa, cond);
    if (teste != NULL)
        teste->aux = cond == BCOND_EQ ? BCOND_NE : BCOND_LT_U;
    BIR_VERIFICA(c, teste != NULL && !BirValidar(&programa, &erro) && erro.codigo == X0904);
    BirLiberar(&programa);
    LiberarFrente(f, false);
    free(f);
}

/* ---- M2 P10b: real ---- */

/* Duas negacoes de f64 dividem a mascara de sinal d0; a negacao de f32 leva a sua (d1). */
static const char FONTE_NEG_REAL[] = "proc main() {\n    let a: f64 = 1.0\n    let b: f32 = 2.0\n    print((-a) as int)\n    print((-a) as int)\n    print((-b) as int)\n}\n";
static const char FONTE_CMP_REAL[] = "proc menor(a: f64, b: f64) -> bool {\n    return a < b\n}\n\nproc main() {\n    print(menor(1.0, b = 2.0))\n}\n";
static const char FONTE_F_PARA_F[] = "proc larga(a: f32) -> f64 {\n    return a as f64\n}\n\nproc main() {\n    print(larga(1.5) as int)\n}\n";

static void VerificaDadoDeduplicado(const Contagem *c)
{
    Buf dump = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_NEG_REAL, BM_DEV, &dump, &erro));
    BIR_VERIFICA(c, TemTexto(&dump, "neg v1, @d1") && TemTexto(&dump, "neg v0, @d0"));
    BIR_VERIFICA(c, TemTexto(&dump, "  d1 = align 16 x\"") && !TemTexto(&dump, "  d2 = ")); /* a mascara de f64 uma vez so; a de f32 e outra */
    BufLiberar(&dump);
}

/* L13: sqrt vira BIR_RAIZ na classe do operando (f32 e f64) e a BIR passa no validador (RaizValida). */
static const char FONTE_RAIZ[] = "proc main() {\n    let a: f32 = 2.0\n    let b: f64 = 2.0\n    print(sqrt(a) as int)\n    print(sqrt(b) as int)\n}\n";

static void VerificaRaiz(const Contagem *c)
{
    Buf dump = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_RAIZ, BM_DEV, &dump, &erro) && TemTexto(&dump, ":f32 = raiz v") && TemTexto(&dump, ":f64 = raiz v"));
    BufLiberar(&dump);
}

static BirInstr *AcharOp(BirPrograma *programa, uint8_t op)
{
    for (uint32_t k = 0; k < programa->nfuncoes; k++) {
        for (uint32_t b = 0; b < programa->funcoes[k].nblocos; b++) {
            BirBloco *bloco = &programa->funcoes[k].blocos[b];

            for (uint32_t n = 0; n < bloco->n; n++) {
                if (bloco->instrs[n].op == op)
                    return &bloco->instrs[n];
            }
        }
    }
    return NULL;
}

/* A BIR do programa passa no validador; a 1a instrucao de op estragada por Estraga faz o validador recusar com X0904. */
static void VerificaEstrago(const Contagem *c, const char *fonte, uint8_t op, void (*Estraga)(BirInstr *))
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    Diag erro = {0};
    BirEntrada entrada;
    BirInstr *alvo;

    f->fonte = FonteDeTexto("memoria.fj", fonte);
    IniciarFrente(f);
    BIR_VERIFICA(c, AnalisarFrente(f) && (entrada = EntradaDaFrente(f, BM_DEV), BirConstruir(&entrada, &programa, &erro)) && BirValidar(&programa, &erro));
    alvo = AcharOp(&programa, op);
    if (alvo != NULL)
        Estraga(alvo);
    BIR_VERIFICA(c, alvo != NULL && !BirValidar(&programa, &erro) && erro.codigo == X0904);
    BirLiberar(&programa);
    LiberarFrente(f, false);
    free(f);
}

static void EstragaCmpInteiro(BirInstr *i)
{
    i->aux = BCOND_LT_S; /* condicao de inteiro numa comparacao de real */
}

static void EstragaFParaF(BirInstr *i)
{
    i->classe = i->a.classe; /* F_PARA_F com a mesma classe nos dois lados */
}

static void EstragaChamaInd(BirInstr *i)
{
    i->b.tam = 8; /* o endereco da funcao e escalar: agregado no lugar dele e invalido */
}

static const char FONTE_STRUCT_MEM[] =
    "struct Par {\n    a: i32\n    b: i32\n}\n\nproc main() {\n    let x = Par{a = 1, b = 2}\n    let y = x\n    print(y.a)\n}\n";

/* Estraga a BIR de memoria e devolve true se achou o alvo: COPIA_MEM e CARREGA vem da instrucao; o slot vem da funcao (main e a 1a com slots). */
static bool EstragaCopiaZero(BirPrograma *p)
{
    BirInstr *i = AcharOp(p, BIR_COPIA_MEM);

    if (i != NULL)
        i->desloc = 0; /* copia de 0 bytes */
    return i != NULL;
}

static bool EstragaCarregaNada(BirPrograma *p)
{
    BirInstr *i = AcharOp(p, BIR_CARREGA);

    if (i != NULL)
        i->classe = BC_NADA; /* carga sem classe de dado */
    return i != NULL;
}

static bool EstragaSlotAlinhamento(BirPrograma *p)
{
    for (uint32_t k = 0; k < p->nfuncoes; k++) {
        if (p->funcoes[k].nslots != 0) {
            p->funcoes[k].slots[0].alinhamento = 3; /* nao e potencia de 2 */
            return true;
        }
    }
    return false;
}

/* 1 verificacao: o programa passa no validador, a BIR estragada por Estraga e recusada com X0904. */
static void VerificaEstragoMem(const Contagem *c, bool (*Estraga)(BirPrograma *))
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    Diag erro = {0};
    BirEntrada entrada;
    bool ok;

    f->fonte = FonteDeTexto("memoria.fj", FONTE_STRUCT_MEM);
    IniciarFrente(f);
    ok = AnalisarFrente(f) && (entrada = EntradaDaFrente(f, BM_DEV), BirConstruir(&entrada, &programa, &erro)) && BirValidar(&programa, &erro);
    ok = ok && Estraga(&programa) && !BirValidar(&programa, &erro) && erro.codigo == X0904;
    BIR_VERIFICA(c, ok);
    BirLiberar(&programa);
    LiberarFrente(f, false);
    free(f);
}

/* ---- M2 P11b: agregado por valor na ABI Win64 e X0906 ---- */

/* O JSON de DiagImprimirJsonSemFonte (stderr da etapa de layout no formato de build) lido de volta de um arquivo temporario. */
static bool JsonDoDiagIgual(const Diag *d, const char *esperado)
{
    char caminho[600], lido[1500] = "";
    const char *pasta = getenv("TEMP") != NULL ? getenv("TEMP") : ".";
    FILE *f;
    size_t n;

    snprintf(caminho, sizeof caminho, "%s\\p11b_diag_teste.json", pasta);
    f = fopen(caminho, "wb");
    if (f == NULL)
        return false;
    DiagImprimirJsonSemFonte(d->codigo, d->causa, f);
    fclose(f);
    f = fopen(caminho, "rb");
    if (f == NULL)
        return false;
    n = fread(lido, 1, sizeof lido - 1, f);
    fclose(f);
    remove(caminho);
    lido[n] = '\0';
    /* b09: o JSON sai com o campo "spec" (trecho da doc, secao cli do X0906) depois de forma_certa; o resto confere byte a byte */
    size_t base = strlen(esperado) - 2;
    return n > base + 2 && strncmp(lido, esperado, base) == 0 && strncmp(lido + base, ", \"spec\": \"cli: ", 16) == 0 && strcmp(lido + n - 2, "}\n") == 0;
}

/* Agregado de 3 e 16 bytes vai por ponteiro (bytes = 0), de 4 e 8 bytes nos proprios bytes; o 5o argumento cai na pilha [rsp+32]. */
static bool AbiAgregadoPorPonteiroOuBytes(void)
{
    const AbiArg args[5] = {{BC_I64, 3}, {BC_I64, 16}, {BC_I64, 4}, {BC_I64, 1}, {BC_I64, 8}};
    AbiLocal onde[5];

    return AbiWinClassificar(args, 5, onde) && onde[0].bytes == 0 && onde[0].reg == X64_RCX && onde[1].bytes == 0 && onde[1].reg == X64_RDX &&
           onde[2].bytes == 4 && onde[2].reg == X64_R8 && onde[3].bytes == 1 && onde[3].reg == X64_R9 && onde[4].na_pilha && onde[4].desloc == 32 &&
           onde[4].bytes == 8;
}

/* Retorno de 12 B (e de 3 B) e por ponteiro oculto; 8, 4 e 1 B em RAX; real escalar em XMM0; inteiro escalar em RAX. */
static bool AbiRetornoOculto(void)
{
    return AbiWinRetorno(BC_I64, 12) == ABI_RET_OCULTO && AbiWinRetorno(BC_I64, 3) == ABI_RET_OCULTO && AbiWinRetorno(BC_I64, 8) == ABI_RET_RAX &&
           AbiWinRetorno(BC_I64, 4) == ABI_RET_RAX && AbiWinRetorno(BC_I64, 1) == ABI_RET_RAX && AbiWinRetorno(BC_F32, 0) == ABI_RET_XMM0 &&
           AbiWinRetorno(BC_I32, 0) == ABI_RET_RAX && AbiWinRetorno(BC_NADA, 0) == ABI_RET_VAZIO;
}

#define JSON_X0906_ADULTERADO \
    "{\"codigo\": \"X0906\", \"causa\": \"layout de 'str', campo 1: offset 40 B + 8 B passa do tamanho 16 B\", \"tipo\": \"interno\", " \
    "\"explicacao\": \"Erro interno do backend nativo: o layout calculado diverge do esperado (layout incoerente em algum tipo, ou o str do backend " \
    "difere do str do runtime nativo).\", \"forma_certa\": \"Ajuste o programa: este recurso nao e aceito pelo backend x64 nesta versao; veja " \
    "`forja explicar CODIGO`. Envie a pasta do relato.\"}\n"

/* O layout de verdade passa; com o offset de len do str adulterado, LayoutConferir reprova com o X0906 de JSON exato. */
static bool LayoutAdulteradoDaX0906(Layouts *layouts, const Tipos *tipos)
{
    Diag erro = {0};
    bool intacto = LayoutConferir(layouts, tipos, &erro);

    layouts->tipos[TIPO_STR].offs[1] = 40;
    return intacto && !LayoutConferir(layouts, tipos, &erro) && erro.codigo == X0906 && JsonDoDiagIgual(&erro, JSON_X0906_ADULTERADO);
}

/* Runtime sintetico com forja_rt_layout = rtl em .rdata; devolve o resultado de NativoRtLayoutConferir (a causa fica em *erro). */
static bool RtLayoutConfere(const uint32_t rtl[23], const Layouts *layouts, const Tipos *tipos, Diag *erro)
{
    Objeto rt;
    uint32_t secao;
    bool ok;

    ObjIniciar(&rt);
    secao = ObjSecaoAbrir(&rt, ".rdata", OS_RDATA, 4);
    BufAnexarN(&((ObjSecao *)VecEm(&rt.secoes, secao))->bytes, (const char *)rtl, 23 * sizeof rtl[0]);
    ObjSimboloDefinir(&rt, "forja_rt_layout", (int32_t)secao, 0);
    ok = NativoRtLayoutConferir(&rt, layouts, tipos, erro);
    ObjLiberar(&rt);
    return ok;
}

/* O forja_rt_layout v3 do runtime: str 16/8/8, lista 32/8/8/16/24/28, map 56 (chaves 0, valores 8, len 24, epoca 48) e o cabecalho da soa dinamica de 1 a 8 colunas (8 * ncol + 24). */
#define RTL_V3(versao, str_tam, epoca_lista) \
    {(versao), (str_tam), 8, 8, 32, 8, 8, 16, 24, (epoca_lista), 56, 0, 8, 24, 48, 32, 40, 48, 56, 64, 72, 80, 88}

/* Versao 4 e desconhecida (X0906); versao 3 com os valores do backend passa; tamanho de str diferente reprova com a causa do contrato. */
static bool RtLayoutDivergente(const Layouts *layouts, const Tipos *tipos)
{
    const uint32_t versao_nova[23] = RTL_V3(4, 16, 28), igual[23] = RTL_V3(3, 16, 28), tam_maior[23] = RTL_V3(3, 24, 28);
    Diag erro_versao = {0}, erro_igual = {0}, erro_tam = {0};
    bool versao_ok = !RtLayoutConfere(versao_nova, layouts, tipos, &erro_versao) && erro_versao.codigo == X0906 && strstr(erro_versao.causa, "versao 4") != NULL;
    bool tam_ok = !RtLayoutConfere(tam_maior, layouts, tipos, &erro_tam) && erro_tam.codigo == X0906 &&
                  strcmp(erro_tam.causa, "layout de 'str': backend 16 B, runtime 24 B") == 0;

    return versao_ok && RtLayoutConfere(igual, layouts, tipos, &erro_igual) && tam_ok;
}

/* P14b: o [dynamic]T do runtime com o offset de epoca em 32 (o layout\ diz 28) reprova com a causa do contrato (X0906). */
static bool RtLayoutListaDivergente(const Layouts *layouts, const Tipos *tipos)
{
    const uint32_t epoca_longe[23] = RTL_V3(3, 16, 32);
    Diag erro = {0};

    return !RtLayoutConfere(epoca_longe, layouts, tipos, &erro) && erro.codigo == X0906 && strcmp(erro.causa, "layout de '[dynamic]T': offset de epoca backend 28 B, runtime 32 B") == 0;
}

static void VerificaLayoutEAbi(const Contagem *c)
{
    Frente *f = calloc(1, sizeof *f);
    Arena arena = {0};
    Layouts layouts;
    BirEntrada entrada;
    bool frente_ok;

    f->fonte = FonteDeTexto("memoria.fj", FONTE_LISTA);
    IniciarFrente(f);
    frente_ok = AnalisarFrente(f);
    entrada = EntradaDaFrente(f, BM_DEV);
    if (frente_ok)
        LayoutCalcular(entrada.tipos, entrada.ir, &arena, &layouts);
    BIR_VERIFICA(c, AbiAgregadoPorPonteiroOuBytes());
    BIR_VERIFICA(c, AbiRetornoOculto());
    BIR_VERIFICA(c, frente_ok && LayoutAdulteradoDaX0906(&layouts, entrada.tipos));
    if (frente_ok)
        layouts.tipos[TIPO_STR].offs[1] = 8; /* desfaz a adulteracao */
    BIR_VERIFICA(c, frente_ok && RtLayoutDivergente(&layouts, entrada.tipos));
    BIR_VERIFICA(c, frente_ok && RtLayoutListaDivergente(&layouts, entrada.tipos));
    ArenaLiberar(&arena);
    LiberarFrente(f, false);
    free(f);
}

static const char FONTE_MAIN_ERRO[] = "enum Erro { a, b }\n\nproc main() -> ! Erro {\n    print(1)\n    return Erro.b\n}\n";
static const char FONTE_ENUM_VALORES[] =
    "enum Modo { a = 0, b = 5 }\n\nproc num(m: Modo) -> i32 {\n    switch m {\n    case Modo.a:\n        return 1\n    case Modo.b:\n        return 2\n    }\n    return 0\n}\n\n"
    "proc main() {\n    print(num(Modo.b))\n}\n";

/* main -> ! E: forja_programa chama forja_main_erro e sai com 1; switch de enum com valor explicito compara o valor C do membro (5), nunca o indice (1). */
static void VerificaResultadoEChaveia(const Contagem *c)
{
    Buf main_erro = {0}, enum_valores = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_MAIN_ERRO, BM_DEV, &main_erro, &erro) && TemTexto(&main_erro, "chama @forja_main_erro(") && TemTexto(&main_erro, "retorna #1:i32"));
    BIR_VERIFICA(c, DumpDoTexto(FONTE_ENUM_VALORES, BM_DEV, &enum_valores, &erro) && TemTexto(&enum_valores, "cmp.eq v0, #5:i32") && !TemTexto(&enum_valores, "cmp.eq v0, #1:i32"));
    BufLiberar(&main_erro);
    BufLiberar(&enum_valores);
}

static const char FONTE_OPCIONAL[] =
    "proc achar(n: i32) -> ?i32 {\n    if n > 0 {\n        return n\n    }\n    return none\n}\n\nproc main() {\n    print(achar(3) or_else 0)\n}\n";

/* P15a: ?i32 e {u8 tem; i32 v} (8 B, v em +4): EMBRULHA grava tem = 1 e o valor, none zera o slot e o or_else le o tem (xor #1 para "e none") e o v. */
static void VerificaOpcional(const Contagem *c)
{
    Buf dump = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_OPCIONAL, BM_DEV, &dump, &erro) && TemTexto(&dump, "grava [v2+0], #1:i8") && TemTexto(&dump, "grava [v2+4], v0") &&
                        TemTexto(&dump, "zera_mem [v3] n=8") && TemTexto(&dump, "xor v3, #1:i8") && TemTexto(&dump, "carrega [v0+4]"));
    BufLiberar(&dump);
}

static const char FONTE_FORMATA[] =
    "proc main() {\n    let n: i32 = -42\n    let x: f32 = 2.5\n    print(f\"n={n:05} {x:.2}\")\n    let s = format(f\"s{n}\", alloc = heap)\n    delete(s)\n}\n";

/* P15b: f"..." e forja_fmt_inicio, uma forja_fmt_<tipo>(valor, formato) por parte (o i32 estendido pelo sinal com largura 5 + zeros = 8197, o f32 alargado a double com precisao .2 = 768), e o final
 * (imprimir, ou str_final com o alocador heap #1 e o str de 16 B no retorno agregado). */
static void VerificaFormata(const Contagem *c)
{
    Buf dump = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_FORMATA, BM_DEV, &dump, &erro) && TemTexto(&dump, "chama @forja_fmt_inicio()") && TemTexto(&dump, "chama @forja_fmt_i64(v0.sx, #8197:i32)") &&
                        TemTexto(&dump, "f_para_f") && TemTexto(&dump, "chama @forja_fmt_f32(") && TemTexto(&dump, ", #768:i32)") && TemTexto(&dump, "chama @forja_fmt_imprimir()") &&
                        TemTexto(&dump, "chama @forja_fmt_str_final(#1:i32, "));
    BufLiberar(&dump);
}

static const char FONTE_V19[] =
    "enum Cor { a, b, c }\n\nproc main() {\n    var s: bit_set[Cor] = {}\n    insert(&s, key = Cor.c)\n    let k = Cor.b\n    print(len(s))\n    print(has(s, key = k))\n"
    "    let m: matrix[2, 2]f32 = {1.0, 2.0, 3.0, 4.0}\n    let p = m * m\n    let x = p[1, 0]\n    print(f\"{x}\")\n}\n";

/* P16: len(bit_set) CHAMA forja_rt_conta_bits(u64) (alargado com ext_z); has com membro em variavel passa pela guarda b < 64 do SHL; o produto de matrix e desenrolado
 * (cada soma comeca em const 0.0 e soma mul por mul, em ordem de k, sem chamada ao runtime). */
static void VerificaConjuntoEMatriz(const Contagem *c)
{
    Buf dump = {0};
    Diag erro = {0};

    BIR_VERIFICA(c, DumpDoTexto(FONTE_V19, BM_DEV, &dump, &erro) && TemTexto(&dump, "ext_z") && TemTexto(&dump, "chama @forja_rt_conta_bits(") && TemTexto(&dump, "cmp.ge_u v") &&
                        TemTexto(&dump, ", #64:i64") && TemTexto(&dump, "= shl #1:i64, v") && TemTexto(&dump, ":f32 = const #0:f32(0x00000000)") && TemTexto(&dump, ":f32 = mul v") &&
                        TemTexto(&dump, ":f32 = add v") && !TemTexto(&dump, "forja_matriz"));
    BufLiberar(&dump);
}

static const char FONTE_MAPA_SOA[] =
    "struct Ponto {\n    x: int = 0\n    y: int = 0\n}\n\nproc main() {\n    var m = make(map[int]int, alloc = heap)\n    defer delete(m)\n    insert(&m, key = 1, value = 2)\n"
    "    var a = make(soa[dynamic]Ponto, alloc = heap)\n    defer delete(a)\n    append(&a, item = Ponto{x = 1})\n    print(len(m) + len(a))\n}\n";

/* O forja_rt_layout v3 com o offset de valores do map em 16 (o layout.h diz 8) e o cabecalho da soa de 2 colunas em 48 (o layout\ diz 40) reprovam com a causa do contrato (X0906); o igual passa. */
static bool RtLayoutMapaSoaDivergente(const Layouts *layouts, const Tipos *tipos)
{
    uint32_t igual[23] = RTL_V3(3, 16, 28), valores_longe[23] = RTL_V3(3, 16, 28), soa_larga[23] = RTL_V3(3, 16, 28);
    Diag erro_igual = {0}, erro_mapa = {0}, erro_soa = {0};

    valores_longe[12] = 16;
    soa_larga[16] = 48;
    return RtLayoutConfere(igual, layouts, tipos, &erro_igual) && !RtLayoutConfere(valores_longe, layouts, tipos, &erro_mapa) && erro_mapa.codigo == X0906 &&
           strcmp(erro_mapa.causa, "layout de 'map': offset de valores backend 8 B, runtime 16 B") == 0 && !RtLayoutConfere(soa_larga, layouts, tipos, &erro_soa) &&
           strcmp(erro_soa.causa, "layout de 'soa[dynamic]S': cabecalho de 2 colunas backend 40 B, runtime 48 B") == 0;
}

/* P17b: map e soa dinamica viram CHAMA ao runtime (forja_mapa_inserir_k8 desde o M1 do lote 0.21.1: chave int de 8 bytes vai por valor; forja_soa_criar) e o forja_rt_layout v3 confere o map e o cabecalho da soa. */
static void VerificaMapaESoa(const Contagem *c)
{
    Frente *f = calloc(1, sizeof *f);
    Arena arena = {0};
    Layouts layouts;
    BirEntrada entrada;
    Buf dump = {0};
    Diag erro = {0};
    bool frente_ok;

    f->fonte = FonteDeTexto("mapa.fj", FONTE_MAPA_SOA);
    IniciarFrente(f);
    frente_ok = AnalisarFrente(f);
    entrada = EntradaDaFrente(f, BM_DEV);
    if (frente_ok)
        LayoutCalcular(entrada.tipos, entrada.ir, &arena, &layouts);
    BIR_VERIFICA(c, frente_ok && RtLayoutMapaSoaDivergente(&layouts, entrada.tipos) && DumpDoTexto(FONTE_MAPA_SOA, BM_DEV, &dump, &erro) &&
                        TemTexto(&dump, "chama @forja_mapa_inserir_k8(") && TemTexto(&dump, "chama @forja_soa_criar(") && TemTexto(&dump, "chama @forja_soa_crescer("));
    BufLiberar(&dump);
    ArenaLiberar(&arena);
    LiberarFrente(f, false);
    free(f);
}

void BirVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    VerificaRegras(&c);
    VerificaDevERelease(&c);
    VerificaDedupE(&c);
    VerificaTrampolim(&c);
    VerificaTrampolimComUnwind(&c);
    VerificaChamadaInd(&c);
    VerificaStrSemAddr64(&c);
    VerificaIndiceConferido(&c);
    VerificaSliceFrom(&c);
    VerificaListaConferida(&c);
    VerificaValidador(&c);
    VerificaGuarda(&c, FONTE_DIVISAO, BCOND_EQ);
    VerificaGuarda(&c, FONTE_DESLOCA, BCOND_GE_U);
    VerificaDadoDeduplicado(&c);
    VerificaRaiz(&c);
    VerificaEstrago(&c, FONTE_CMP_REAL, BIR_CMP, EstragaCmpInteiro);
    VerificaEstrago(&c, FONTE_F_PARA_F, BIR_F_PARA_F, EstragaFParaF);
    VerificaEstrago(&c, FONTE_CHAMADA_IND, BIR_CHAMA_IND, EstragaChamaInd);
    VerificaEstragoMem(&c, EstragaCopiaZero);
    VerificaEstragoMem(&c, EstragaCarregaNada);
    VerificaEstragoMem(&c, EstragaSlotAlinhamento);
    VerificaLayoutEAbi(&c);
    VerificaResultadoEChaveia(&c);
    VerificaOpcional(&c);
    VerificaFormata(&c);
    VerificaConjuntoEMatriz(&c);
    VerificaMapaESoa(&c);
}
