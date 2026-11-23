/* gerar_bir.c - modo "bir" do teste_nativo (P6): roda o front-end ate a IR, constroi a BIR, valida e imprime o dump. As verificacoes de unidade da BIR moram em gerar_bir_verifica.c.
 * Versao: 0.11.1
 * Changelog: 0.11.1 - Linguagem v1.13 L1a: AnalisarModulo grava Parser.arquivo_atual e registra o trecho no mapa trecho -> modulo (apelidos de import por arquivo).
 *            0.11.0 - M2 P15a: o arquivo passou de 800 linhas; as verificacoes de unidade (BirVerificacoes e as Verifica*, do 0.1.0 ao 0.10.0 abaixo) foram para gerar_bir_verifica.c e Frente e suas
 *                    quatro funcoes para gerar_bir_frente.h (agora externas). Comportamento igual (180 verificacoes antes e depois da divisao).
 *            0.10.0 - M2 P14b: VerificaListaConferida (+1: dev chama forja_lista_viva e forja_indice_lista, release nao); forja_rt_layout de 10 palavras (versao 2) e RtLayoutListaDivergente (+1: X0906 do [dynamic]T).
 *            0.9.0 - M2 P13b: VerificaIndiceConferido (+2: dev confere indice e fatia, release nao) e VerificaSliceFrom (+1: forja_vetor_c_confere so em dev).
 *            0.8.0 - M2 P13a: VerificaStrSemAddr64 (+1: literal de str gera REL32 e nenhuma ADDR64); o X0001 de VerificaDedupEX0001 passa de str para ?i32 (str roda no x64).
 *            0.7.0 - M2 P12a: VerificaResultadoEChaveia (+2): main -> ! E chama forja_main_erro e sai com 1; switch de enum com valor explicito compara o valor C do membro.
 *            0.6.0 - M2 P11b: VerificaLayoutEAbi (+4): AbiWinClassificar com tamanhos 3/16 (ponteiro) e 4/8 (bytes), AbiWinRetorno (12 B oculto), LayoutConferir com o str adulterado (X0906 de
 *                    JSON exato) e forja_rt_layout de versao desconhecida / tamanho divergente (X0906).
 *            0.5.0 - M2 P11a: VerificaEstragoMem (+3): COPIA_MEM com 0 bytes, CARREGA de classe NADA e slot com alinhamento 3 -> X0904.
 *            0.4.0 - M2 P10b: VerificaDadoDeduplicado (BS_DADO da mascara de sinal uma vez so), VerificaEstrago (CMP de real com BCOND inteira e F_PARA_F de mesma classe -> X0904);
 *                    o X0001 de VerificaDedupEX0001 passa de f32 para str.
 *            0.3.0 - M2 P9: VerificaGuarda (+4 verificacoes): DIV e SHL passam no BirValidar e, com o teste da guarda trocado, saem X0904.
 *            0.2.1 - VerificaRegras percorre o enum ate IR_FATIA_DE_PONTEIRO (ultimo op, ir.h 0.14.0).
 *            0.2.0 - backend x64 P7a: BirComPrograma (front-end + BIR validada entregues a um callback; o modo x64 de gerar_x64.c usa).
 *            0.1.0 - backend x64 P6: ModoBir (dump ou X0001 em JSON), ModoBirTempo (mediana de BirConstruir) e BirVerificacoes (regras, validador, dump).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe bir <arq.fj> [--release]   |   python -I ..\tests\nativo\bir\verificar_bir.py
 *
 * O front-end repete o main.c (cuja Compilacao e static): lexico, parser, modulos (arquivo e std), semantica e lowering. Saida de ModoBir:
 *   exit 0 = dump no stdout | exit 1 = diagnostico JSON no stdout (front-end ou X0001) | exit 3 = BIR invalida (X0904).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "frontend/lexer.h"
#include "frontend/modulos.h"
#include "frontend/parser.h"
#include "ir/lower.h"
#include "ir/layout.h"
#include "nativo/bir/bir.h"
#include "nativo/nativo_layout.h"
#include "nativo/obj/objeto.h"
#include "nativo/x64/x64_gerar.h"
#include "nativo/x64/x64_interno.h"
#include "semantic/semantic.h"
#include "gerar_bir.h"
#include "gerar_bir_frente.h"

#define GB_REPETICOES_TEMPO 15
#define GB_CAMINHO_MAX 1300

void IniciarFrente(Frente *f)
{
    InternerIniciar(&f->nomes, &f->arena);
    LexerNomesIniciais(&f->nomes);
    f->lexer = (Lexer){.fonte = &f->fonte, .nomes = &f->nomes, .diag = &f->diag};
    ModulosIniciar(&f->modulos, &f->nomes, f->fonte.caminho);
    f->parser = (Parser){.fonte = &f->fonte, .nomes = &f->nomes, .arena = &f->arena, .diag = &f->diag, .modulos = &f->modulos};
}

void LiberarFrente(Frente *f, bool texto_proprio)
{
    if (f->sem.tipos.tabela != NULL)
        SemanticaLiberar(&f->sem);
    ParserLiberar(&f->parser);
    ModulosLiberar(&f->modulos);
    LexerLiberar(&f->lexer);
    InternerLiberar(&f->nomes);
    ArenaLiberar(&f->arena);
    if (!texto_proprio)
        f->fonte.texto = NULL; /* texto em memoria do teste: nao e de malloc */
    FonteLiberar(&f->fonte);
}

/* ---- modulos (o mesmo caminho do main.c: ao lado da raiz, vendor\ ou a std embutida) ---- */

static void PastaDaRaiz(const Frente *f, char *destino, size_t tam)
{
    char absoluto[600];
    char *corte;

    if (_fullpath(absoluto, f->fonte.caminho, sizeof absoluto) == NULL)
        snprintf(absoluto, sizeof absoluto, ".\\x");
    corte = strrchr(absoluto, '\\');
    if (corte != NULL)
        *corte = '\0';
    snprintf(destino, tam, "%s", absoluto);
}

static bool LerModuloDeArquivo(Frente *f, const Modulo *mod, const char *import)
{
    char pasta[600], caminho[GB_CAMINHO_MAX];
    const char *resto = mod->origem == MOD_ARQUIVO ? import : import + 7;

    PastaDaRaiz(f, pasta, sizeof pasta);
    if (mod->origem == MOD_VENDOR)
        strncat(pasta, "\\vendor", sizeof pasta - strlen(pasta) - 1);
    snprintf(caminho, sizeof caminho, "%s\\%s.fj", pasta, resto);
    for (char *p = caminho + strlen(pasta) + 1; *p != '\0'; p++) {
        if (*p == '/')
            *p = '\\';
    }
    if (FonteAnexarArquivo(&f->fonte, ArenaCopiarTexto(&f->arena, caminho, strlen(caminho))))
        return true;
    return DiagRelatar(&f->diag, E1001, mod->pos_import, "arquivo '%s' nao encontrado", import);
}

static bool LerModuloDaFrente(Frente *f, uint32_t k)
{
    const Modulo *mod = &f->modulos.itens[k];
    const char *import = InternerTexto(&f->nomes, mod->caminho);
    size_t tam;
    const char *texto;

    if (mod->origem != MOD_STD)
        return LerModuloDeArquivo(f, mod, import);
    texto = ModulosTextoStd(import + 4, &tam);
    if (texto != NULL && FonteAnexarTexto(&f->fonte, import, texto, tam))
        return true;
    return DiagRelatar(&f->diag, E1001, mod->pos_import, "%s nao existe", import);
}

static bool AnalisarModulo(Frente *f, uint32_t k)
{
    f->lexer.inicio = f->fonte.trechos[f->fonte.ntrechos - 1].inicio;
    if (!LexerAnalisar(&f->lexer))
        return false;
    f->parser.modulo_atual = (uint8_t)k;
    f->parser.arquivo_atual = f->fonte.ntrechos - 1; /* v1.13: o trecho e o arquivo do modulo */
    ModulosRegistrarArquivo(&f->modulos, f->parser.arquivo_atual, (uint8_t)k);
    return ParserAnalisar(&f->parser);
}

static bool CarregarModulos(Frente *f)
{
    for (uint32_t k = 1; k < f->modulos.n; k++) {
        if (!LerModuloDaFrente(f, k) || !AnalisarModulo(f, k))
            return false;
    }
    if (!ModulosConferirCiclos(&f->modulos, &f->parser.programa, &f->diag))
        return false;
    if (f->modulos.n > 1)
        ModulosOrdenarDecls(&f->modulos, &f->parser.programa);
    return true;
}

/* Lexico, parser, modulos, semantica e lowering. false = diagnostico em f->diag. */
bool AnalisarFrente(Frente *f)
{
    if (!LexerAnalisar(&f->lexer))
        return false;
    f->parser.tokens = &f->lexer.tokens;
    if (!ParserAnalisar(&f->parser))
        return false;
    if (f->modulos.apelidos.n > 0 && !CarregarModulos(f))
        return false;
    f->sem = (Semantica){.modulos = &f->modulos, .qtd_modulos = f->modulos.n, .fonte = &f->fonte, .nomes = &f->nomes,
                         .arena = &f->arena, .diag = &f->diag, .programa = &f->parser.programa};
    SemanticaIniciar(&f->sem);
    return SemanticaAnalisar(&f->sem) && LowerPrograma(&f->sem, &f->ir);
}

BirEntrada EntradaDaFrente(const Frente *f, uint8_t modo)
{
    return (BirEntrada){.ir = &f->ir, .tipos = &f->sem.tipos, .nomes = &f->nomes, .fonte = &f->fonte, .modo = modo};
}

/* ---- modo bir ---- */

static int ImprimirDump(const Frente *f, uint8_t modo)
{
    BirEntrada entrada = EntradaDaFrente(f, modo);
    BirPrograma programa;
    Diag erro = {0};
    Buf dump = {0};
    int codigo = 0;

    if (!BirConstruir(&entrada, &programa, &erro)) {
        DiagImprimirJson(&erro, &f->fonte, stdout);
        codigo = erro.codigo == X0001 ? 1 : 3;
    } else if (!BirValidar(&programa, &erro)) {
        DiagImprimirJson(&erro, &f->fonte, stdout);
        codigo = 3;
    } else {
        BirImprimir(&programa, &dump);
        fwrite(dump.dados, 1, dump.tam, stdout);
    }
    BufLiberar(&dump);
    BirLiberar(&programa);
    return codigo;
}

int ModoBir(const char *arquivo, bool release)
{
    Frente *f = calloc(1, sizeof *f);
    int codigo;

    if (f == NULL)
        return 3;
    if (!FonteLer(&f->fonte, arquivo)) {
        free(f);
        return 2;
    }
    IniciarFrente(f);
    if (AnalisarFrente(f)) {
        codigo = ImprimirDump(f, release ? BM_RELEASE : BM_DEV);
    } else {
        DiagImprimirJson(&f->diag, &f->fonte, stdout);
        codigo = 1;
    }
    LiberarFrente(f, true);
    free(f);
    return codigo;
}

/* ---- BirComPrograma: front-end + BIR validada entregues a quem precisa (modo x64 do P7a) ---- */

static int UsarBir(const Frente *f, uint8_t modo, BirUsoFn uso, void *ctx)
{
    BirEntrada entrada = EntradaDaFrente(f, modo);
    BirPrograma programa;
    Diag erro = {0};
    int codigo;

    if (!BirConstruir(&entrada, &programa, &erro) || !BirValidar(&programa, &erro)) {
        DiagImprimirJson(&erro, &f->fonte, stdout);
        codigo = erro.codigo == X0001 ? 1 : 3;
    } else {
        codigo = uso(&programa, ctx);
    }
    BirLiberar(&programa);
    return codigo;
}

int BirComPrograma(const char *arquivo, uint8_t modo, BirUsoFn uso, void *ctx)
{
    Frente *f = calloc(1, sizeof *f);
    int codigo;

    if (f == NULL)
        return 3;
    if (!FonteLer(&f->fonte, arquivo)) {
        free(f);
        return 2;
    }
    IniciarFrente(f);
    if (AnalisarFrente(f)) {
        codigo = UsarBir(f, modo, uso, ctx);
    } else {
        DiagImprimirJson(&f->diag, &f->fonte, stdout);
        codigo = 1;
    }
    LiberarFrente(f, true);
    free(f);
    return codigo;
}

/* ---- modo bir-tempo: mediana de BirConstruir (e BirLiberar) em GB_REPETICOES_TEMPO rodadas ---- */

static int ComparaNs(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;

    return x < y ? -1 : (x > y ? 1 : 0);
}

int ModoBirTempo(const char *arquivo)
{
    Frente *f = calloc(1, sizeof *f);
    uint64_t ns[GB_REPETICOES_TEMPO];
    int codigo = 1;

    if (f == NULL)
        return 3;
    if (!FonteLer(&f->fonte, arquivo)) {
        free(f);
        return 2;
    }
    IniciarFrente(f);
    if (AnalisarFrente(f)) {
        BirEntrada entrada = EntradaDaFrente(f, BM_DEV);

        for (int k = 0; k < GB_REPETICOES_TEMPO; k++) {
            BirPrograma programa;
            Diag erro = {0};
            uint64_t t0 = RelogioNs();

            codigo = BirConstruir(&entrada, &programa, &erro) ? 0 : 1;
            ns[k] = RelogioNs() - t0;
            BirLiberar(&programa);
        }
        qsort(ns, GB_REPETICOES_TEMPO, sizeof ns[0], ComparaNs);
        printf("BirConstruir mediana=%llu ns minimo=%llu ns maximo=%llu ns (%d rodadas)\n", (unsigned long long)ns[GB_REPETICOES_TEMPO / 2],
               (unsigned long long)ns[0], (unsigned long long)ns[GB_REPETICOES_TEMPO - 1], GB_REPETICOES_TEMPO);
    }
    LiberarFrente(f, true);
    free(f);
    return codigo;
}
