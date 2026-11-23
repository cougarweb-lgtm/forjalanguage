/* main.c - linha de comando do compilador Forja (contrato v1: check, ast e build; 5d: build exe; 5h: test, run, explicar).
 * Versao: 0.65.0
 * Changelog: 0.65.0 - Linguagem v1.13 L4b: ResolverUmLoad recusa caminho com unidade (C:) com a causa real (E1007 com mensagem propria), em vez de 'nao abre'.
 *            0.64.0 - Linguagem v1.13 L1b: IniciarCompilacao e LiberarCompilacao cuidam de Compilacao.pastas_carregadas.
 *            0.63.0 - Linguagem v1.13 L1a (modulo de pasta): LerModuloArquivo chama CargaPacote (driver\carga_pacote.c) e E1008 se o arquivo ja entrou (CargaJaEntrou); AnalisarModulo (agora externa) grava o trecho em Parser.arquivo_atual e no mapa trecho -> modulo.
 *            0.62.0 - 0.21.0 F4 passo 2b: include driver/processo.h (cc.h saiu); ProcApagarExeDoRun e ProcExeDoRun no lugar dos Cc*.
 *            0.61.0 - 0.21.0 F4 passo 2a: o backend C saiu do main (GerarC, ProduzirC, BuildParaC, CompilarParaExe, CompilarERodar, prova de layout); build e run/test so pelo nativo x64; opcao aposentada (--modo c, --prova-layout) sai com X0003 antes da validacao (PrepararOpcoes), para `build --modo c x.fj` sem -o nao dar 'build exige -o'.
 *            0.60.0 - O0 P13: CcPedido.sem_checagem (--checagem nao no release vira -DFORJA_SEM_CHECAGEM).
 *0.59.0 - M6c P82: AnalisarSemantica preenche sem.alvo_lp64 (AlvoEhLinux) para c_long/c_ulong; ponto unico de todo comando que analisa.
 *            0.58.0 - M6b P79: E1001 de vendor: ausente lista os modulos disponiveis e cita o parecido (RelatarVendorAusente).
 *            0.57.0 - M6 P69: ModoComTraco, PastaDoFonte, PastasDoPrograma, CompilarC, PedidoNativo e o cabecalho da DLL foram para driver\main_opcoes.c (main.c passou de 800 linhas); sem mudar comportamento.
 *            0.56.0- M6 P68: CcPedido.simbolos nos 2 pedidos do backend C (exe e DLL): --simbolos sim vira -g no gcc.
 *            0.55.0 - M6 P66: PedidoNativo leva .simbolos (OpcoesSimbolos: --simbolos; padrao sim em dev).
 *            0.54.0 - M5 P54: ProgramaUsaNet (std:net liga a ws2_32 quando o programa tem forja_net_*) entra no changelog; foi acrescentado no P53 sem bump.
 *            0.53.0 - B12: ResolverLoads (antes de AnalisarSemantica) le cada #load pela pasta do arquivo raiz, troca o nome pelo conteudo e grava (no, caminho, Hash64) em Compilacao.resolvidos; E1007 se nao abre ou passa de LOAD_TAM_MAX.
 *            0.52.0 - F1 P2: RodarAnalise chama ConferirCanonicoSeHa (check --canonico; a linha trocada, o arquivo nao cresce); 0.51.0 - S4 P2: forja test <std|vendor|engine>:<modulo> (raiz sintetica em memoria; so o alvo tem test; o testes.fj da std entra no texto do alvo: LerModulo).
 *            0.50.3 - S4 P1: forja test liga os test dos modulos de origem arquivo (SelecionarTestesDeModulos, dentro de AnalisarSemantica por causa da carga tardia dos modulos).
 *            0.50.2 - IA1-L9b: LerModulo le a std por ModulosTextoStdAlvo (os.TARGET do --target x86_64-linux).
 *            0.50.1 - IA1-L2: import "os" (modulo da std sem o prefixo e sem arquivo ao lado) vira E0110 com a troca por "std:os" (RelatarStdSemPrefixo).
 *            0.50.0 - IA1-I4: o despacho dos subcomandos que nao passam por OpcoesLer (ligar, corrigir, editar, consultar, mapa, spec, explicar extenso) virou a tabela SUBCOMANDOS
 *            (DespacharSubcomando, driver\main_despacho.c; FORJA_VERSAO foi para driver\versao.h, mesmo valor); o resto do main foi para PrepararOpcoes e RodarComOpcoes (main com complexidade cognitiva baixa); o texto de uso saiu para driver\main_uso.c (MainUsoErrado).
 *            0.49.0 - IA1-I3: subcomando 'forja mapa' (RodarMapa, driver\cmd_mapa.c; despacho antes da leitura das opcoes) e as 6 perguntas novas do 'forja consultar' na linha de uso.
 *            0.48.0 - IA1-b15: o parser guarda os erros de sintaxe na mesma lista da semantica (Parser.erros = &sem.lista): varios erros por execucao.
 *            0.47.0 - IA1-b13: --rastreio no backend C (ModoComTraco liga EMIT_RASTREIO em dev e no build --modo c; o release o ignora) e a linha de uso.
 *            0.46.0 - IA1-b12: --rastreio chega ao NativoPedido (linha de uso do build, run e test).
 *            0.45.0 - IA1-I2: subcomando 'forja consultar' (RodarConsultar, driver\cmd_consultar.c; despacho antes da leitura das opcoes) e a linha de uso.
 *            0.44.0 - IA1-I1: 'forja ast' 1.0 em driver\cmd_ast.c (RodarAst, AstResolverEntrada: --sem-pos, --no, --projeto, pasta); Compilacao foi para driver\compilacao.h e as etapas (IniciarCompilacao, LiberarCompilacao, AnalisarSintaxe, CarregarModulos, AnalisarSemantica, RelatarFalha) ficam visiveis ao cmd_ast.c.
 *            0.43.0 - IA1-b10: run e test --limite-tempo/--limite-mem/--limite-saida (R0013..R0015, exit 102..104) e run --args-file (LimitesPrepararArgumentos antes de compilar); ExecutarPrograma roda o filho por LimitesExecutar (driver\limites.c).
 *            0.42.0 - IA1-b09: 'forja spec' (RodarSpec) e 'forja explicar --lista|--exemplos' (RodarExplicarExtenso), driver\cmd_spec.c e cmd_explicar.c, despachados antes da leitura das opcoes; FORJA_VERSAO_TEXTO; linhas de uso.
 *            0.41.0 - IA1-b05: subcomando 'forja corrigir' (RodarCorrigir, driver\cmd_corrigir.c; despacho antes da leitura das opcoes, como o 'ligar') e a linha de uso.
 *            0.40.0 - IA1-b03: paridade do backend C no link e no simbolo (ConferirBibliotecasDoC: NativoConferirBibliotecasDoC antes do gcc em CompilarParaExe e BuildParaDll; X0101/X0102 em stdout); Compilacao.opcoes.
 *            0.39.0 - IA1-b02: determinismo (forja run/test: exe em %TEMP%\forja_run\<hash do caminho do fonte>\<nome>.exe; .c temporario do backend C com nome por hash, CcCaminhoDoC); --resumo (DiagModoResumo depois de validar as opcoes); FORJA_VERSAO segue 0.19.0.
 *            0.38.0 - M5 P27a (D-M5-9): ConferirBackend/ConferirAlvo/ConferirDebugCode/ConferirBackendAlvo/X64NaoGera foram para o opcoes.c (OpcoesConferirBackendAlvo; main.c abaixo de 800 linhas, mesmo comportamento no alvo Windows);
 *                    forja test --target x86_64-linux so constroi o exe de teste em -o (GerarExeNativo).
 *            0.37.0 - M4 P25b (D-M4-8, degrau dev): FORJA_VERSAO 0.19.0 (--versao "forja 0.19.0 (Linguagem v1.11)", Q-M4-1); main chama OpcoesResolverBackend (x64 em dev, c em --release);
 *                    --debug-code sem --backend deixa de dar X0003 em dev (o padrao ja e x64) e o texto de uso diz o padrao.
 *            0.36.0 - M4 P24a: Compilacao.tempos (NativoTempos) guarda lex, parse, check e ir (BaixarIrMedido); o --tempos do build x64 grava emit_ns e os 13 campos do
 *                    registro "tempos" (NativoTemposGravar); o do backend C segue com os 6 campos de sempre.
 *            0.35.0 - M3 P22-O2: so o texto de uso do build --projeto ganha [--jobs N] (0 linha de logica); build --projeto sem --plano executa pelo OrquestrarProjeto.
 *            0.34.0 - M3 P22-O1b: build --projeto [forja.proj] [--release] [--plano] [--tudo] [--json] despacha para OrquestrarProjeto (driver\orquestrar.c; so o --plano ate o O2); uso do build com --projeto.
 *            0.33.0 - M3 P22-O1a: --entradas e --lib-pasta chegam ao NativoPedido (OpcoesConferirSoX64 recusa fora do x64 com X0003); GravarCabecalho grava o .h da DLL de forma atomica no backend x64.
 *            0.32.0 - M3 P22a: build --modo dll --backend x64 (X64NaoGera deixa de recusar a DLL; GerarExeNativo passa NativoPedido.dll, confere o B0003 e grava o .h pelo mesmo EmitirCabecalho do backend C; BuildParaDll desvia para ele).
 *            0.31.2 -Linguagem v1.12 M3 P21v-b: ConferirLayoutNoCheck volta a baixar a IR do programa com proc como valor.
 *            0.31.1 -Linguagem v1.12 M3 P21v-a: ConferirLayoutNoCheck nao baixa a IR do programa com proc como valor (static_assert espera o P21v-b).
 *            0.31.0 -M3 P19: GerarExeNativo passa a pasta do .fj e as extras (vendor:/engine:) ao NativoPedido, onde o nativo acha as bibliotecas dos 'link'.
 *            0.30.0 - Linguagem v1.11 L17: FORJA_VERSAO 0.18.1 e --versao "forja 0.18.1 (Linguagem v1.11)" (fechamento da v1.11).
 *            0.29.0 - Linguagem v1.11 L16a: forja test --filtro "<trecho>" e --json (RodarTest chama OpcoesAmbienteDoTeste: FORJA_TESTE_FILTRO e FORJA_TESTE_JSON para o exe); uso do test com [--filtro <trecho>] [--json].
 *            0.28.0 - Linguagem v1.11 L9: o check baixa a IR e a descarta quando Semantica.usa_layout (size_of, align_of, offset_of e static_assert so tem valor no lowering).
 *            0.27.0 - Linguagem v1.11 L7: RelatarFalha imprime a lista de diagnosticos da semantica; --max-erros N (teto da lista) chega a Semantica.lista.teto; uso do check com [--max-erros N].
 *            0.26.0 - Linguagem v1.11 L6: check --stdin (ExecutarCompilacao le a entrada padrao com FonteLerDeStream); uso do check com [--stdin].
 *            0.25.0 - fechamento do M2 (P18): FORJA_VERSAO 0.18.0 e --versao "forja 0.18.0 (Linguagem v1.10)" (backend x64 nativo com paridade de linguagem no Windows).
 *            0.24.0 - P18a: --traco (ModoComTraco liga EMIT_TRACO no backend C; NativoPedido.traco no x64); sem a opcao nada muda.
 *            0.23.0 - M2 P12b: forja test --backend x64 (X64NaoGera tira o test; RodarTest chama o nativo com NativoPedido.teste; sem mudanca para o backend C).
 *            0.22.2 - forja run app.fj -- a "b c": os argumentos do '--' chegam ao programa (Opcoes.args_programa -> CcExecutarComArgumentos);
 *                    ExecutarPrograma/ExecutarEApagar/CompilarERodar/RodarTest recebem Opcoes. Sem mudanca de saida quando nao ha '--'.
 *            0.22.1 - M2 P9a: a prova de layout recebe ProvaEntrada (LayoutEscreverProva com 2 parametros); sem mudanca de saida.
 *            0.22.0 - fechamento do M1: FORJA_VERSAO 0.17.0 e --versao "forja 0.17.0 (Linguagem v1.10)" (backend x64 nativo M1; layout v1.10).
 *            0.21.1 - portao M1: o enum SAIDA_* vem de nativo\nativo.h (fonte unica); GravarTempos documenta regs_ns/codificar_ns como nao medidos.
 *            0.21.0 - backend x64 P8b: build --backend x64 --suporte --json [<arq.fj>] (sem arquivo: a tabela REGRAS_IR; com arquivo: cada X0001, exit 0);
 *                    NativoPedido.comando (a linha de comando, para o pacote de relato). FORJA_VERSAO segue 0.16.0 (o portao sobe).
 *            0.20.0 - backend x64 P8: --debug-code <lista> (so build --backend x64; senao X0003) e --tempos com bir_ns/isel_ns/regs_ns/codificar_ns/ligar_ns/escrever_ns
 *                    no build x64. FORJA_VERSAO segue 0.16.0 (o portao sobe).
 *            0.19.0 - backend x64 P7b: build e run com --backend x64 vao para NativoConstruirExe (nativo\nativo.c); test, dll e --modo c com x64 sao X0003;
 *                    opcoes extraidas para driver\opcoes.c (extracao pura). FORJA_VERSAO segue 0.16.0 (o portao sobe).
 *            0.18.0 - Linguagem v1.10 P1: opcao --prova-layout <arq.c> (so build --modo c; fora do texto de uso) grava a prova de layout (layout/).
 *            0.17.0 - backend x64 M0-R: opcoes --backend e --target (so build, run e test); ConferirBackendAlvo responde X0002/X0003 por DiagImprimirJsonSemFonte (exit 2). FORJA_VERSAO segue 0.16.0.
 *            0.16.0 - Linguagem v1.9 P12: FORJA_VERSAO 0.16.0 e --versao "forja 0.16.0 (Linguagem v1.9)" (emit_c 0.12.0; runtime 0.9.0; ast 0.10.0; diag 0.13.0).
 *            0.15.0 - Linguagem v1.8b P7: 'forja ligar' despachado antes de LerOpcoes (driver/ligar.c).
 *                    Linguagem v1.8b P6: vendor:/engine: (PastaDoModulo, CaminhoDoArquivo com pasta, E1001 de FORJA_ENGINE); PastasDeLigacao (-L e PATH extras).
 *                    Linguagem v1.8b P9: FORJA_VERSAO 0.15.0 e --versao "forja 0.15.0 (Linguagem v1.8b)" (emit_c 0.11.0; runtime 0.8.0).
 *            0.14.4 - Linguagem v1.8 P6b: LigacaoDoPrograma so usa 2 passos com foreign nao interno (TemExternoDeFora).
 *            0.14.3 - Linguagem v1.8 P6a: LerModulo usa o texto embutido de std:mem (sai o provisorio "chega no P6a").
 *            0.14.2 - Linguagem v1.8 P3: AnalisarSemantica passa modulos/qtd_modulos a semantica (sai o provisorio de semantica do P2b).
 *            0.14.1- Linguagem v1.8 P2b: carga dos modulos (LerModulo, AnalisarModulo, CarregarModulos: E1001 de arquivo ausente, DFS/E1002, Decl em pos-ordem); semantica de modulos PROVISORIA (E0101 ate o P3).
 *            0.14.0 - Linguagem v1.8 P2a: Compilacao.modulos (ModulosIniciar/Liberar, parser.modulos), AnalisarSintaxe(c, carregar) com carga PROVISORIA (E0101 "import chega no P2b"); --versao fica para o P8/7.5.
 *            0.13.0 - Linguagem v1.7 P10: --versao "forja 0.13.0 (Linguagem v1.7)" (emit_c 0.9.0; runtime 0.7.0).
 *            0.12.0 - Linguagem v1.6: memoria explicita ([dynamic]T, new, make, append, delete, frame_reset, 'for' sobre lista) (--versao: Linguagem v1.6; emit_c 0.8.0; runtime 0.6.0).
 *            0.11.0 - Linguagem v1.5: arrays [N]T, fatias []T, a[i], a[lo..<hi], literal {...} e 'for item/&item/i,item' (--versao: Linguagem v1.5; emit_c 0.7.0; runtime 0.5.0).
 *            0.10.0 - Linguagem v1.4: bloco 'foreign' com 'link' (--versao: Linguagem v1.4; emit_c 0.6.0; runtime 0.4.0).
 *            0.9.1 - Linguagem v1.4 P5: CcPedido (bibliotecas de 'foreign', -L da pasta do .fj, 2 passos); B0004 sai com 1;
 *                    run/test poem a pasta do .fj no PATH do proprio processo. (--versao fica para o P6.)
 *            0.9.0 - Linguagem v1.3: ponteiros (^T, ?^T, none, p^, &x), rawptr, str/cstr, len() (--versao: Linguagem v1.3; emit_c 0.5.0).
 *            0.8.0 - Linguagem v1.2: numericos i8..u64/uint/f64, bits, 'as' checado, transmute (--versao: Linguagem v1.2).
 *            0.7.0 - Linguagem v1.1: --modo dll exporta so os 'pub export proc' (simbolo limpo), exige ao menos um (B0003) e grava
 *                    <saida>.h; 'main -> ! E' que devolve erro sai com 1 e R0004 (emit_c 0.4.0).
 *            0.6.0 - Fase 5i: 'build --modo dll [--release] <arq.fj> -o <saida.dll>' (EMIT_BIBLIOTECA + gcc -shared; procs fj_<nome> exportados).
 *            0.5.0 - Fase 5h: 'forja test <arq.fj>' roda os 'test' (EMIT_TESTE, dev); 'forja run <arq.fj> [--release]' compila num
 *                    exe temporario em %TEMP%, roda e devolve o exit do programa; 'forja explicar <CODIGO>' imprime o catalogo em JSON;
 *                    opcoes lidas por tabela (OPCOES_CLI); #line no C gerado (emit_c 0.3.0).
 *            0.4.0 - Fase 5d: 'build --modo c' roda o pipeline inteiro (lexico, parser, semantica, lowering, emissao) e grava o .c;
 *                    'build <arq.fj> -o <saida.exe>' (ou --modo exe) passa o C por gcc (driver/cc.c); --release tira -DFORJA_DEV;
 *                    --tempos grava emit_ns (lowering + emissao).
 *            0.3.0 - Fase 5c: 'check' roda lexico + parser + semantica; 'ast --tipos' roda a semantica e acrescenta "tipo" a cada no
 *                    (sem --tipos o 'ast' continua sem exigir semantica); --tempos grava check_ns de verdade (semantica inteira,
 *                    inclusive o preparo das tabelas).
 *            0.2.0 - Fase 5b: 'check' roda lexico + parser (sintaxe OK = exit 0 mudo; erro = 1 linha JSON, exit 1);
 *                    'ast' imprime a AST em JSON; --tempos grava lex_ns/parse_ns/total_ns/linhas (check_ns e emit_ns = 0).
 *                    'build' segue devolvendo 3 (pipeline incompleto ate a 5d).
 *            0.1.0 - Fase 5a: so le os argumentos e o arquivo.
 * Verificacao: construir.bat && forja.exe --versao && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>

#include "backend/emit_h.h"
#include "common/buf.h"
#include "common/diag.h"
#include "common/hash.h"
#include "common/idiomas.h"
#include "driver/carga_pacote.h"
#include "driver/cmd_ast.h"
#include "driver/limites.h"
#include "driver/main_despacho.h"
#include "driver/main_opcoes.h"
#include "driver/main_uso.h"
#include "driver/opcoes.h"
#include "driver/processo.h"
#include "driver/versao.h"
#include "driver/orquestrar.h"
#include "frontend/lexer.h"
#include "frontend/modulos.h"
#include "frontend/parser.h"
#include "ir/lower.h"
#include "nativo/nativo.h"
#include "nativo/nativo_biblioteca.h"
#include "semantic/semantic.h"

#define MAIN_TAM_LINHA 2048 /* a linha de comando que o pacote de relato guarda */

void IniciarCompilacao(Compilacao *c)
{
    InternerIniciar(&c->nomes, &c->arena);
    LexerNomesIniciais(&c->nomes);
    c->lexer = (Lexer){.fonte = &c->fonte, .nomes = &c->nomes, .diag = &c->diag, .erros = &c->sem.lista};
    ModulosIniciar(&c->modulos, &c->nomes, c->fonte.caminho);
    VecIniciar(&c->cargas, sizeof(Expr *));
    VecIniciar(&c->resolvidos, sizeof(NativoCarga));
    VecIniciar(&c->pastas_carregadas, sizeof(NativoPacote));
    c->parser =(Parser){.fonte = &c->fonte, .nomes = &c->nomes, .arena = &c->arena, .diag = &c->diag, .erros = &c->sem.lista, .modulos = &c->modulos, .cargas = &c->cargas};
}

void LiberarCompilacao(Compilacao *c)
{
    if (c->sem.tipos.tabela != NULL) SemanticaLiberar(&c->sem);
    ParserLiberar(&c->parser);
    VecLiberar(&c->cargas);
    VecLiberar(&c->resolvidos);
    VecLiberar(&c->pastas_carregadas);
    ModulosLiberar(&c->modulos);
    LexerLiberar(&c->lexer);
    InternerLiberar(&c->nomes);
    ArenaLiberar(&c->arena);
    FonteLiberar(&c->fonte);
}


/* Pasta do .fj de um modulo de arquivo: ao lado da raiz (MOD_ARQUIVO), vendor\ (MOD_VENDOR) ou %FORJA_ENGINE%\forja (MOD_ENGINE).
 * false = MOD_ENGINE sem a variavel FORJA_ENGINE. */
static bool PastaDoModulo(const Compilacao *c, const Modulo *mod, char *destino, size_t tam)
{
    if (mod->origem == MOD_ENGINE) {
        const char *engine = getenv("FORJA_ENGINE");
        if (engine == NULL || engine[0] == '\0') return false;
        snprintf(destino, tam, "%s\\forja", engine);
        return true;
    }
    PastaDoFonte(c, destino, tam);
    if (mod->origem == MOD_VENDOR) {
        size_t n = strlen(destino);
        snprintf(destino + n, tam - n, "\\vendor");
    }
    return true;
}

/* Caminho do .fj do modulo: pasta + '\' + resto do import ('/' vira '\') + ".fj", copiado na arena. */
static const char *CaminhoDoArquivo(Compilacao *c, const char *pasta, const char *resto)
{
    char caminho[1300];
    int n = snprintf(caminho, sizeof caminho, "%s\\%s.fj", pasta, resto);
    for (char *p = caminho + strlen(pasta) + 1; *p != '\0'; p++) {
        if (*p == '/') *p = '\\';
    }
    return ArenaCopiarTexto(&c->arena, caminho, n < 0 ? 0 : (size_t)n);
}

/* IA1-L2: import "os" (sem o prefixo, e sem um os.fj ao lado) e o jeito de Python e Go: E0110 com a troca por "std:os". false = o nome nao e de modulo da std. */
static bool RelatarStdSemPrefixo(Compilacao *c, const Modulo *mod, const char *import)
{
    IdiomaTroca troca = IDIOMA_TROCA_VAZIA;
    uint32_t tam = (uint32_t)strlen(import);
    char novo[DIAG_NOVO_MAX];

    if (!ModulosStdTem(import, tam)) return false;
    IdiomaRelatar(&c->diag, IDI_IMPORT_SEM_STD, mod->pos_import, import);
    snprintf(novo, sizeof novo, "\"std:%s\"", import);
    if (c->fonte.texto[mod->pos_import] == '"') IdiomaTrocaSomar(&troca, c->fonte.texto, mod->pos_import, mod->pos_import + tam + 2, novo);
    IdiomaSugerir(&c->diag, DIAG_CERTA, "escreva o caminho com o prefixo std: (a biblioteca padrao)", &troca);
    return true;
}

/* P79: vendor: ausente cita o parecido do mesmo grupo e lista os modulos disponiveis (std + vendor da pasta do arquivo raiz). false = E1001. */
static bool RelatarVendorAusente(Compilacao *c, const Modulo *mod, const char *import, const char *resto)
{
    char lista[440], parecido[128];

    ModulosListarDisponiveis(c->modulos.pasta, lista, sizeof lista);
    if (ModulosParecido(c->modulos.pasta, import, parecido, sizeof parecido)) {
        return DiagRelatar(&c->diag, E1001, mod->pos_import, "arquivo '%s' nao encontrado: quis dizer %s? (disponiveis: %s)", import, parecido, lista);
    }
    return DiagRelatar(&c->diag, E1001, mod->pos_import,
                       "arquivo '%s' nao encontrado: o modulo vendor fica em vendor\\%s.fj, ao lado do arquivo raiz (disponiveis: %s)", import, resto, lista);
}

/* Modulo de arquivo (relativo, vendor: ou engine:) ou de pasta (v1.13: *pacote = true, os trechos ja foram anexados e analisados). false = diagnostico no import que o trouxe. */
static bool LerModuloArquivo(Compilacao *c, uint32_t k, bool *pacote)
{
    const Modulo *mod = &c->modulos.itens[k];
    const char *import = InternerTexto(&c->nomes, mod->caminho);
    char pasta[600];
    const char *resto = mod->origem == MOD_ARQUIVO ? import : import + 7;
    if (!PastaDoModulo(c, mod, pasta, sizeof pasta)) {
        return DiagRelatar(&c->diag, E1001, mod->pos_import,
                           "\"%s\" precisa da variavel FORJA_ENGINE (pasta da engine): o modulo fica em %%FORJA_ENGINE%%\\forja\\%s.fj", import, resto);
    }
    const char *caminho = CaminhoDoArquivo(c, pasta, resto);
    CargaResultado carga = CargaPacote(c, k, caminho);
    if (carga != CARGA_SEGUE_ARQUIVO) return *pacote = carga == CARGA_PACOTE_CARREGADO;
    if (CargaJaEntrou(c, caminho)) return CargaRelatarRepetido(c, mod, caminho);
    if (FonteAnexarArquivo(&c->fonte, caminho)) return true;
    if (mod->origem == MOD_VENDOR) return RelatarVendorAusente(c, mod, import, resto);
    if (mod->origem == MOD_ENGINE) return DiagRelatar(&c->diag, E1001, mod->pos_import, "arquivo '%s' nao encontrado em %s", import, caminho);
    if (mod->origem == MOD_ARQUIVO && RelatarStdSemPrefixo(c, mod, import)) return false;
    return DiagRelatar(&c->diag, E1001, mod->pos_import, "arquivo '%s' nao encontrado (o caminho e relativo a pasta do arquivo raiz)", import);
}

/* S4 P2: o import e o alvo de forja test <modulo> (so ele leva o pedaco de testes da std). */
static bool EhAlvoDoTeste(const Compilacao *c, const char *import)
{
    return c->opcoes != NULL && c->opcoes->alvo_teste != NULL && strcmp(c->opcoes->alvo_teste, import) == 0;
}

/* Anexa ao texto da Fonte o conteudo do modulo k (arquivo ou std embutida). false = E1001 no import que o trouxe. */
static bool LerModulo(Compilacao *c, uint32_t k, bool *pacote)
{
    const Modulo *mod = &c->modulos.itens[k];
    const char *import = InternerTexto(&c->nomes, mod->caminho);
    if (mod->origem != MOD_STD) return LerModuloArquivo(c, k, pacote);
    size_t tam;
    const char *texto = ModulosTextoStdAlvo(import + 4, c->opcoes != NULL && AlvoEhLinux(c->opcoes), &c->arena, &tam);
    if (texto == NULL) return DiagRelatar(&c->diag, E1001, mod->pos_import, "%s nao existe", import);
    if (EhAlvoDoTeste(c, import)) texto = ModulosTextoStdComTestes(import + 4, texto, &c->arena, &tam);
    if (FonteAnexarTexto(&c->fonte, import, texto, tam)) return true;
    return DiagRelatar(&c->diag, E1001, mod->pos_import, "arquivo '%s' nao encontrado (o caminho e relativo a pasta do arquivo raiz)", import);
}

/* Lexico e parser do ultimo trecho anexado (arquivo do modulo k), somando os tempos. O trecho entra no mapa trecho -> modulo (v1.13: o modulo de pasta tem varios). */
bool AnalisarModulo(Compilacao *c, uint32_t k)
{
    uint64_t t0 = RelogioNs();
    c->parser.arquivo_atual = c->fonte.ntrechos - 1;
    ModulosRegistrarArquivo(&c->modulos, c->parser.arquivo_atual, (uint8_t)k);
    c->lexer.inicio = c->fonte.trechos[c->fonte.ntrechos - 1].inicio;
    bool ok = LexerAnalisar(&c->lexer);
    uint64_t t1 = RelogioNs();
    c->tempos.lex_ns += t1 - t0;
    if (!ok) return false;
    c->parser.modulo_atual = (uint8_t)k;
    ok = ParserAnalisar(&c->parser);
    c->tempos.parse_ns += RelogioNs() - t1;
    return ok;
}

/* Carga dos modulos importados (k cresce enquanto os imports de cada um registram novos), ciclos (E1002) e ordem das Decl. */
bool CarregarModulos(Compilacao *c)
{
    for (uint32_t k = 1; k < c->modulos.n; k++) {
        bool pacote = false; /* modulo de pasta: a carga ja analisou cada arquivo */
        if (!LerModulo(c, k, &pacote) || (!pacote && !AnalisarModulo(c, k))) return false;
    }
    if (!ModulosConferirCiclos(&c->modulos, &c->parser.programa, &c->diag)) return false;
    if (c->modulos.n > 1) ModulosOrdenarDecls(&c->modulos, &c->parser.programa);
    return true;
}

/* Lexico e parser da raiz, medindo cada um; com 'carregar' e algum import, carrega os modulos importados.
 * false = diagnostico em c->diag. */
bool AnalisarSintaxe(Compilacao *c, bool carregar)
{
    uint64_t t0 = RelogioNs();
    bool ok = LexerAnalisar(&c->lexer);
    uint64_t t1 = RelogioNs();
    c->tempos.lex_ns = t1 - t0;
    if (!ok) return false;
    c->parser.tokens = &c->lexer.tokens;
    ok = ParserAnalisar(&c->parser);
    c->tempos.parse_ns = RelogioNs() - t1;
    if (ok && carregar && c->modulos.apelidos.n > 0) return CarregarModulos(c);
    return ok;
}

/* S4 P2: indice do modulo alvo de forja test <modulo> (0 = nenhum alvo; a raiz e sempre 0). */
static uint32_t ModuloAlvoDoTeste(const Compilacao *c)
{
    if (c->opcoes == NULL || c->opcoes->alvo_teste == NULL) return 0;
    for (uint32_t k = 1; k < c->modulos.n; k++) {
        if (strcmp(InternerTexto(&c->nomes, c->modulos.itens[k].caminho), c->opcoes->alvo_teste) == 0) return k;
    }
    return 0;
}

/* S4 P1: no forja test, os test dos modulos de origem arquivo (transitivos) entram no lowering; std, vendor e engine nao.
 * S4 P2: com alvo (forja test std:<m>), so o alvo entra. Chamado depois de CarregarModulos (os modulos so existem apos a carga); os outros comandos nao ligam nada. */
static void SelecionarTestesDeModulos(Compilacao *c)
{
    uint32_t alvo;

    if (c->opcoes == NULL || !ComandoEh(c->opcoes, "test")) return;
    alvo = ModuloAlvoDoTeste(c);
    for (uint32_t k = 1; k < c->modulos.n; k++) {
        c->sem.modulo_testado[k] = alvo != 0 ? k == alvo : c->modulos.itens[k].origem == MOD_ARQUIVO;
    }
}

/* S4 P2: o import da raiz sintetica (linha 0 de import_usado) conta como usado: sem isso, E1005 sairia no proprio alvo. */
static void MarcarImportDoAlvo(Compilacao *c)
{
    uint32_t alvo = ModuloAlvoDoTeste(c);

    if (alvo != 0 && c->sem.import_usado != NULL) c->sem.import_usado[alvo] = 1;
}

/* B12: tamanho maximo de um #load (constante nomeada); acima dele e E1007, como arquivo que nao abre. */
#define LOAD_TAM_MAX ((size_t)16 * 1024 * 1024)

/* B12: le o arquivo inteiro (bytes crus, sem tirar BOM) para um malloc; false = nao abre, passa de LOAD_TAM_MAX ou nao le tudo (*dados fica NULL). */
static bool LerLoadArquivo(const char *caminho, char **dados, size_t *tam)
{
    FILE *f = fopen(caminho, "rb");
    long n = 0;
    bool ok = false;
    if (f == NULL) return false;
    if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) >= 0 && (size_t)n <= LOAD_TAM_MAX && fseek(f, 0, SEEK_SET) == 0) {
        *dados = malloc((size_t)n + 1);
        if (*dados != NULL) {
            *tam = fread(*dados, 1, (size_t)n, f);
            ok = *tam == (size_t)n;
            if (!ok) {
                free(*dados);
                *dados = NULL;
            }
        }
    }
    fclose(f);
    return ok;
}

/* B12: um #load: caminho = pasta do arquivo raiz + '\' + o texto ('/' vira '\'); o no troca o nome pelo conteudo internado e a Compilacao guarda (no, caminho, Hash64). */
static bool ResolverUmLoad(Compilacao *c, const char *pasta, Expr *no)
{
    char caminho[1300];
    char *dados = NULL;
    size_t tam = 0;
    const char *resto = InternerTexto(&c->nomes, no->nome);
    NativoCarga *r;
    /* L4b: caminho com unidade (C:...) nao junta com a pasta raiz; a causa e essa, nao 'nao abre'. */
    if (strlen(resto) > 1 && resto[1] == ':') {
        return DiagRelatar(&c->diag, E1007, no->pos, "#load(\"%s\"): caminho absoluto (com unidade, ex.: C:) nao e aceito; o caminho e relativo a pasta do arquivo raiz", resto);
    }
    snprintf(caminho, sizeof caminho, "%s\\%s", pasta, resto);
    for (char *p = caminho + strlen(pasta) + 1; *p != '\0'; p++) {
        if (*p == '/') *p = '\\';
    }
    if (!LerLoadArquivo(caminho, &dados, &tam)) {
        return DiagRelatar(&c->diag, E1007, no->pos, "#load(\"%s\"): o arquivo nao abre ou passa de %u bytes (16 MiB)", resto, (unsigned)LOAD_TAM_MAX);
    }
    r = VecEmpurrar(&c->resolvidos);
    r->caminho = InternerTexto(&c->nomes, Internar(&c->nomes, caminho, (uint32_t)strlen(caminho)));
    r->hash = Hash64(dados, tam, HASH64_SEMENTE);
    no->nome = Internar(&c->nomes, dados, (uint32_t)tam);
    free(dados);
    return true;
}

/* B12: antes da semantica, cada #load do parser (Compilacao.cargas, em ordem) le o arquivo; o 1o erro para (E1007). */
static bool ResolverLoads(Compilacao *c)
{
    char pasta[600];
    PastaDoFonte(c, pasta, sizeof pasta);
    for (size_t k = 0; k < c->cargas.n; k++) {
        if (!ResolverUmLoad(c, pasta, *(Expr **)VecEm(&c->cargas, k))) return false;
    }
    return true;
}

/* Semantica sobre a AST (tipos, unidades, nomes, resultado, fluxo, test), medindo o tempo. false = diagnostico em c->diag. */
bool AnalisarSemantica(Compilacao *c)
{
    uint64_t t0 = RelogioNs();
    if (!ResolverLoads(c)) return false;
    c->sem.modulos = &c->modulos;
    c->sem.qtd_modulos = c->modulos.n;
    c->sem.fonte = &c->fonte;
    c->sem.nomes = &c->nomes;
    c->sem.arena = &c->arena;
    c->sem.diag = &c->diag;
    c->sem.programa = &c->parser.programa;
    SelecionarTestesDeModulos(c);
    SemanticaIniciar(&c->sem);
    c->sem.alvo_lp64 = c->opcoes != NULL && AlvoEhLinux(c->opcoes);
    MarcarImportDoAlvo(c);
    bool ok = SemanticaAnalisar(&c->sem);
    c->tempos.check_ns = RelogioNs() - t0;
    return ok;
}

/* Diagnostico: a lista da semantica (uma linha JSON por erro, ja ordenada) ou o Diag unico do lexico, sintaxe, lowering e demais; exit 1.
 * Falha sem diagnostico = erro interno, exit 3. */
int RelatarFalha(const Compilacao *c)
{
    if (c->sem.lista.n > 0) {
        DiagListaImprimir(&c->sem.lista, &c->fonte, stdout);
        return SAIDA_DIAGNOSTICO;
    }
    if (!c->diag.tem) return SAIDA_INTERNO;
    DiagImprimirJson(&c->diag, &c->fonte, stdout);
    return SAIDA_DIAGNOSTICO;
}

/* Lowering da IR do caminho x64, medido em ir_ns. */
static bool BaixarIrMedido(Compilacao *c)
{
    uint64_t t0 = RelogioNs();
    bool ok = LowerPrograma(&c->sem, &c->ir);

    c->tempos.ir_ns = RelogioNs() - t0;
    return ok;
}

/* --backend x64: front-end e IR do main, depois o nativo gera o exe (ou a DLL, com o .h do mesmo EmitirCabecalho) em 'saida' (BIR -> x64 -> PE, sem gcc nem ld). */
static int GerarExeNativo(const Opcoes *o, Compilacao *c, const char *saida)
{
    NativoPedido pedido = PedidoNativo(o, c, saida);
    PastasDeLigacao pastas;
    int codigo;

    c->nativo_medido = true;
    if (!AnalisarSintaxe(c, true) || !AnalisarSemantica(c) || !BaixarIrMedido(c)) return RelatarFalha(c);
    if (pedido.dll && ContarExportados(&c->ir) == 0) return DllSemExportados();
    PastasDoPrograma(c, &pastas); /* M3 P19: onde o nativo acha as bibliotecas dos 'link' (a busca do -L) */
    pedido.pasta_libs = pastas.raiz;
    pedido.pastas_extra = pastas.extras;
    pedido.npastas_extra = pastas.nextras;
    codigo = NativoConstruirExe(&pedido);
    return codigo == SAIDA_OK && pedido.dll ? GravarCabecalho(o, saida, c) : codigo;
}

/* build --backend x64 --suporte --json <arq.fj>: front-end e IR, depois o relatorio do que o nativo recusa (exit 0). */
static int RodarSuporte(const Opcoes *o, Compilacao *c)
{
    NativoPedido pedido = PedidoNativo(o, c, NULL);

    if (!AnalisarSintaxe(c, true) || !AnalisarSemantica(c) || !LowerPrograma(&c->sem, &c->ir)) return RelatarFalha(c);
    return NativoSuportePrograma(&pedido);
}

/* build exe e dll (padrao): o nativo gera o exe (--release tira -DFORJA_DEV) ou a DLL, e a DLL ganha o cabecalho <saida>.h (EmitirCabecalho). */
static int RodarBuild(const Opcoes *o, Compilacao *c)
{
    if (o->suporte) return RodarSuporte(o, c);
    return GerarExeNativo(o, c, o->saida);
}

/* Roda o exe gerado com os argumentos de '--' e do --args-file (run), sob os limites pedidos, e devolve o exit code dele (102..104 se um limite estourou).
 * Falha ao iniciar = exit 3 com a mensagem no stderr. */
static int ExecutarPrograma(const Opcoes *o, const char *arq_exe)
{
    return LimitesExecutar(o, arq_exe);
}

/* Programa com 'link': o loader acha a .dll na pasta do .fj porque ela entra no inicio do PATH do PROPRIO forja (o filho
 * herda; nada fora do processo muda). */
static void PastaDoFonteNoPath(const Compilacao *c)
{
    Buf novo = {0};
    PastasDeLigacao pastas;
    const char *antigo = getenv("PATH");

    if (c->ir.nbibliotecas == 0) return;
    PastasDoPrograma(c, &pastas);
    BufFormatar(&novo, "%s", pastas.raiz);
    for (uint32_t k = 0; k < pastas.nextras; k++) BufFormatar(&novo, ";%s", pastas.extras[k]);
    BufFormatar(&novo, ";%s", antigo != NULL ? antigo : "");
    _putenv_s("PATH", novo.dados);
    BufLiberar(&novo);
}

/* Roda o exe temporario se a geracao deu certo (codigo == SAIDA_OK) e apaga o arquivo. Devolve o exit do programa, ou o codigo de erro. */
static int ExecutarEApagar(const Opcoes *o, const Compilacao *c, const char *arq_exe, int codigo)
{
    if (codigo == SAIDA_OK) PastaDoFonteNoPath(c);
    if (codigo == SAIDA_OK) codigo = ExecutarPrograma(o, arq_exe);
    ProcApagarExeDoRun(arq_exe);
    return codigo;
}

/* Exe temporario do run/test: %TEMP%\forja_run\<hash do caminho do fonte>\<nome>.exe, sem pid (D-IA1, M-D6: o argv[0] do programa e o mesmo em toda execucao).
 * A variante (comando, backend, release) entra no hash para execucoes simultaneas do mesmo fonte nao se pisarem. */
static bool ExeTemporarioDoRun(const Opcoes *o, char *arq_exe, size_t tam)
{
    char variante[64];

    snprintf(variante, sizeof variante, "%s/%s/%d", o->comando, o->backend != NULL ? o->backend : "c", o->release ? 1 : 0);
    return ProcExeDoRun(arq_exe, tam, o->entrada, variante);
}

/* forja run: o exe temporario vem do nativo, roda e e apagado. */
static int RodarRunNativo(const Opcoes *o, Compilacao *c)
{
    char arq_exe[512];

    if (!ExeTemporarioDoRun(o, arq_exe, sizeof arq_exe)) return SAIDA_INTERNO;
    return ExecutarEApagar(o, c, arq_exe, GerarExeNativo(o, c, arq_exe));
}

/* forja test: o exe de teste sai do nativo (NativoPedido.teste; --release nao existe no test, entao e dev); exit 0 se todos passaram, 1 se algum falhou.
 * O ELF (--target x86_64-linux) so se constroi (M5 P27a): rodar e no Linux. */
static int RodarTest(const Opcoes *o, Compilacao *c)
{
    OpcoesAmbienteDoTeste(o);
    if (AlvoEhLinux(o)) return GerarExeNativo(o, c, o->saida);
    return RodarRunNativo(o, c);
}

/* forja run: o programa roda como exe temporario; o exit code dele (101 em panic) sai como o do comando. */
static int RodarRun(const Opcoes *o, Compilacao *c)
{
    return RodarRunNativo(o, c);
}

/* v1.11 L9: size_of, align_of, offset_of e static_assert so tem valor no lowering; o check baixa a IR e a descarta. */
static bool ConferirLayoutNoCheck(Compilacao *c)
{
    return !c->sem.usa_layout || LowerPrograma(&c->sem, &c->ir);
}

/* check: lexico, parser, semantica e o layout; o ast tem comando proprio (driver\cmd_ast.c). */
static int RodarAnalise(const Opcoes *o, Compilacao *c)
{
    if (ComandoEh(o, "ast")) return RodarAst(o, c);
    return AnalisarSintaxe(c, true) && AnalisarSemantica(c) && ConferirLayoutNoCheck(c) ? (ConferirCanonicoSeHa(o->canonico, &c->fonte) ? SAIDA_OK : SAIDA_DIAGNOSTICO) : RelatarFalha(c);
}

static int RodarComando(const Opcoes *o, Compilacao *c)
{
    if (ComandoEh(o, "build")) return RodarBuild(o, c);
    if (ComandoEh(o, "test")) return RodarTest(o, c);
    if (ComandoEh(o, "run")) return RodarRun(o, c);
    return RodarAnalise(o, c);
}

/* forja explicar <CODIGO>: uma linha JSON do catalogo. Codigo desconhecido = exit 2 com mensagem. */
static int RodarExplicar(const char *codigo)
{
    int indice = DiagCodigoPorTexto(codigo);

    if (indice < 0) {
        fprintf(stderr, "forja: codigo de diagnostico desconhecido: %s\n", codigo);
        return SAIDA_USO;
    }
    DiagExplicarJson((DiagCodigo)indice, stdout);
    return SAIDA_OK;
}

/* --tempos: sem o build x64 (check, ast), as etapas do front-end, o emit_ns (0: o emissor C saiu) e o total; no build x64, o registro completo (NativoTemposGravar, a mesma tabela do .debug.jsonl). */
static bool GravarTempos(const char *caminho, Compilacao *c, uint64_t total_ns)
{
    char texto[300];
    int n;

    if (c->nativo_medido) return NativoTemposGravar(caminho, c->emit_ns, &c->tempos, &c->fonte);
    n = snprintf(texto, sizeof texto,
                 "{\"lex_ns\": %llu, \"parse_ns\": %llu, \"check_ns\": %llu, \"emit_ns\": %llu, \"total_ns\": %llu, \"linhas\": %llu}\n",
                 (unsigned long long)c->tempos.lex_ns, (unsigned long long)c->tempos.parse_ns, (unsigned long long)c->tempos.check_ns,
                 (unsigned long long)c->emit_ns, (unsigned long long)total_ns, (unsigned long long)FonteContarLinhas(&c->fonte));
    return n > 0 && ArquivoGravar(caminho, texto, (size_t)n);
}

/* S4 P2: o alvo de forja test <modulo> nao tem arquivo: a raiz sintetica (raiz_alvo) entra em memoria sob o caminho virtual. */
static bool LerEntrada(const Opcoes *o, Fonte *f)
{
    if (o->alvo_teste != NULL) return FonteAdotarTexto(f, o->entrada, o->raiz_alvo, strlen(o->raiz_alvo));
    return o->stdin_fonte ? FonteLerDeStream(f, stdin, o->entrada) : FonteLer(f, o->entrada);
}

/* Comandos que leem o .fj: le o arquivo, roda o comando e grava --tempos se pedido. */
static int ExecutarCompilacao(const Opcoes *o)
{
    uint64_t inicio = RelogioNs();
    Compilacao *c = calloc(1, sizeof *c);
    int codigo;

    if (c == NULL) FalhaInterna("memoria esgotada alocando a compilacao");
    if (!LerEntrada(o, &c->fonte)) {
        free(c);
        return SAIDA_USO;
    }
    IniciarCompilacao(c);
    c->opcoes = o;
    c->tempos.inicio_ns = inicio;
    c->sem.lista.teto = OpcoesMaxErros(o);
    codigo = RodarComando(o, c);
    if (o->tempos != NULL && !GravarTempos(o->tempos, c, RelogioNs() - inicio)) codigo = SAIDA_USO;
    LiberarCompilacao(c);
    free(c);
    return codigo;
}

/* Le e valida as opcoes de check/ast/build/run/test/explicar; devolve o motivo do uso errado ou NULL. 'linha' guarda a linha de comando para o pacote de relato.
 * LIMITE: parametros 5 (argc+argv e o par da linha de comando; o buffer linha+tam e a saida; funcao de uso unico, struct nao ganha clareza). */
static const char *PrepararOpcoes(int argc, char **argv, Opcoes *o, char *linha, size_t tam)
{
    const char *erro = OpcoesLer(argc, argv, o);

    OpcoesLinhaDeComando(argc, argv, linha, tam);
    o->linha = linha;
    if (erro == NULL && !OpcoesConferirAposentados(o)) erro = ""; /* "" = X0003 ja impresso: a validacao nao roda (--modo c sem -o) */
    if (erro == NULL) erro = OpcoesValidar(o);
    if (erro == NULL) erro = OpcoesPrepararAlvoDoTeste(o);
    if (erro == NULL) erro = LimitesPrepararArgumentos(o);
    return erro;
}

/* Opcoes validas: resolve o backend e despacha o comando. */
static int RodarComOpcoes(Opcoes *o)
{
    DiagModoResumo(o->resumo);
    OpcoesResolverBackend(o);
    if (!OpcoesConferirBackendAlvo(o)) return SAIDA_USO;
    if (ComandoEh(o, "explicar")) return RodarExplicar(o->entrada);
    if (ComandoEh(o, "ast") && !AstResolverEntrada(o)) return SAIDA_USO;
    if (o->projeto && !ComandoEh(o, "ast")) return OrquestrarProjeto(o);
    if (o->suporte && o->entrada == NULL) return NativoSuporteTabela();
    return ExecutarCompilacao(o);
}

int main(int argc, char **argv)
{
    Opcoes o = {0};
    char linha[MAIN_TAM_LINHA];
    const char *erro;
    int codigo;

    if (argc < 2) return MainUsoErrado("falta o comando");
    if (DespacharSubcomando(argc, argv, &codigo)) return codigo;
    if (argc == 2 && strcmp(argv[1], "--versao") == 0) {
        puts(FORJA_VERSAO_TEXTO);
        return SAIDA_OK;
    }
    erro = PrepararOpcoes(argc, argv, &o, linha, sizeof linha);
    if (erro != NULL && erro[0] == '\0') return SAIDA_USO;
    if (erro != NULL) return MainUsoErrado(erro);
    return RodarComOpcoes(&o);
}
