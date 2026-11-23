/* opcoes.h - opcoes da linha de comando do forja: leitura por tabela e validacao por comando (extraido do main.c no P7b do M1).
 * Versao: 0.25.0
 * Changelog: 0.25.0 - M8 P2: Opcoes.perfil (--perfil).
 *            0.24.0 - 0.21.0 F4 passo 2a: OpcoesConferirAposentados (publica, roda antes de OpcoesValidar); o comentario do OpcoesResolverBackend deixa de falar em --modo c.
 *            0.23.0 - M7a F0: Opcoes.sem_otim (--sem-otim).
 *            0.22.0 - O0 P13: Opcoes.checagem (--checagem sim|nao) e OpcoesChecagem.
 *            0.21.0 - M6e P93d: Opcoes.guarda_memoria (--guarda-memoria).
 *            0.20.0 - M6 P66: Opcoes.simbolos (--simbolos sim|nao, D-M6-5) e OpcoesSimbolos (padrao: sim em dev, nao em release).
 *            0.19.0 - F1 P2: Opcoes.canonico (--canonico; so check, o F0001 do driver\cmd_fmt.c).
 *            0.18.0 - S4 P2: Opcoes.alvo_teste, caminho_alvo e raiz_alvo (forja test <prefixo>:<nome>: raiz sintetica em memoria) e OpcoesPrepararAlvoDoTeste.
 *            0.17.1 - IA1-b13: so comentario de Opcoes.rastreio (o backend C passa a ter pilha).
 *            0.17.0 - IA1-b12: Opcoes.rastreio (--rastreio).
 *            0.16.0 - IA1-I1: Opcoes.no (--no <id>) e Opcoes.sem_pos (--sem-pos); so no comando ast.
 *            0.15.0 - IA1-b10: Opcoes.limite_tempo, limite_mem, limite_saida (--limite-tempo, --limite-mem, --limite-saida; run e test) e args_file (--args-file; so run).
 *            0.14.0 - IA1-b02: Opcoes.resumo (--resumo; check, build, run e test).
 *            0.13.0 - M5 P27a: AlvoEhLinux e OpcoesConferirBackendAlvo (vindo do main.c); OpcoesResolverBackend poe o x64 tambem para --target x86_64-linux.
 *            0.12.0 - M4 P25b (D-M4-8): OpcoesResolverBackend (o padrao do backend: x64 em dev, c em --release e --modo c).
 *            0.11.0 - M3 P22-O2: Opcoes.jobs (--jobs N, so com --projeto) e OpcoesJobsTexto (N de 1 a 64, tambem para FORJA_JOBS).
 *            0.10.0 -M3 P22-O1b: Opcoes.projeto, plano e tudo (--projeto [forja.proj], --plano, --tudo; so build).
 *            0.9.0 - M3 P22-O1a: Opcoes.entradas (--entradas), Opcoes.lib_pasta (--lib-pasta) e OpcoesConferirSoX64.
 *            0.8.0 - Linguagem v1.11 L16a: Opcoes.filtro (--filtro "<trecho>", so test) e OpcoesAmbienteDoTeste (FORJA_TESTE_FILTRO e FORJA_TESTE_JSON para o exe).
 *            0.7.0 - Linguagem v1.11 L7: Opcoes.max_erros (--max-erros N) e OpcoesMaxErros.
 *            0.6.0 - Linguagem v1.11 L6: Opcoes.stdin_fonte (--stdin, so check).
 *            0.5.0 - P18a: Opcoes.traco (--traco).
 *            0.4.0 - forja run ... -- args: Opcoes.args_programa e nargs_programa (tudo depois do '--' vai ao programa).
 *            0.3.0 - backend x64 P8b: Opcoes.suporte, json e linha; OpcoesLinhaDeComando.
 *            0.2.0 - backend x64 P8: Opcoes.debug_code (--debug-code <lista>).
 *            0.1.0 - backend x64 P7b: Opcoes, OpcoesLer, OpcoesValidar, ComandoEh, ModoEhC, ModoEhDll (extracao pura do main.c 0.18.0, sem mudar comportamento)
 *                    e BackendEhX64.
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_OPCOES_H
#define FORJA_DRIVER_OPCOES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *comando, *entrada, *saida, *modo, *tempos, *backend, *target, *prova, *debug_code;
    const char *max_erros;      /* --max-erros N (L7): texto do valor; use OpcoesMaxErros */
    const char *simbolos;       /* --simbolos sim|nao (M6 P66, D-M6-5): NULL = padrao; use OpcoesSimbolos */
    const char *checagem;       /* --checagem sim|nao (O0 P13): indice e nulo no release; NULL = padrao; use OpcoesChecagem */
    const char *sem_otim;       /* --sem-otim <passo[,passo...]|tudo> (M7a F0): desliga passes de otimizacao da BIR; so release; NULL = todos os passes ligados */
    const char *filtro;        /* --filtro "<trecho>" (L16a): so os testes cujo titulo contem o trecho rodam; so test */
    const char *entradas;       /* --entradas <arq> (P22-O1a): o build x64 grava o .entradas (o que leu: fj e libs, com tamanho, mtime e hash); so build --backend x64 */
    const char *lib_pasta;      /* --lib-pasta <pasta> (P22-O1a): pasta de busca das DLLs no fim da lista (antes do System32); so build --backend x64 */
    const char *jobs;           /* --jobs N (P22-O2): filhos em paralelo no build do projeto (1..64); use OpcoesJobsTexto; so com --projeto */
    const char *limite_tempo, *limite_mem, *limite_saida;  /* --limite-tempo <seg>, --limite-mem <MiB>, --limite-saida <bytes|KiB|MiB> (IA1-b10): texto do valor; use LimitesLer (driver\limites.h); run e test */
    const char *args_file;      /* --args-file <arq> (IA1-b10): os argumentos do programa, um por linha; so run; LimitesPrepararArgumentos poe args_programa em ordem */
    const char *no;             /* --no <id> (IA1-I1): so a subarvore do no com esse id; so ast */
    const char *alvo_teste;     /* forja test std:<m> (S4 P2): o alvo como veio (o texto do import); NULL = arquivo. entrada passa a ser caminho_alvo */
    char caminho_alvo[512];     /* forja test <alvo> (S4 P2): <pasta atual>\<prefixo>_<nome>.teste.fj, o caminho virtual da raiz sintetica (sem ':') */
    char raiz_alvo[320];        /* forja test <alvo> (S4 P2): o texto da raiz sintetica: import "<alvo>" as alvo_teste + proc main() {} */
    const char *linha;          /* a linha de comando inteira (OpcoesLinhaDeComando), so para o pacote de relato */
    const char *const *args_programa;   /* argv depois do '--' (forja run app.fj -- a "b c"); aponta para o argv do processo */
    int nargs_programa;
    bool tipos, release, suporte, json;
    bool sem_pos;               /* --sem-pos (IA1-I1): a AST sem posicoes, para comparar a estrutura; so ast */
    bool resumo;                /* --resumo (IA1-b02): diagnosticos so com codigo, arquivo, linha, coluna, causa e sugestao (DiagModoResumo); check, build, run e test */
    bool traco;                 /* --traco (P18a): o programa escreve o traco do diferencial (@traco ...) em stderr */
    bool pie;                   /* --pie (P41): ELF static-pie no alvo x86_64-linux (base 0, ASLR); so com --target x86_64-linux */
    bool guarda_memoria;        /* --guarda-memoria (M6e P93d): so build, run e test, dev, backend x64, exe; o exe leva a subtabela de linhas na FRT1 */
    bool perfil;                /* --perfil (M8 P2, D-M8-5): so build, run e test, exe (nao com --modo dll nem --suporte); o exe grava <exe>.perfil.json e <exe>.perfil.txt */
    bool rastreio;              /* --rastreio (IA1-b12): a linha R do runtime ganha coluna, valor e pilha .fj; nos 2 backends em dev (o release ignora a flag) */
    bool stdin_fonte;           /* --stdin (L6): o .fj vem da entrada padrao; o argumento e so o nome (JSON e imports); so check */
    bool projeto;               /* --projeto (P22-O1b): build do projeto (forja.proj em 'entrada', ou o da pasta atual); so build */
    bool plano, tudo;           /* --plano (so mostra o que recompilaria) e --tudo (forca todos os produtos); so com --projeto */
    bool canonico;              /* --canonico (F1 P2): o check tambem confere a forma canonica (F0001); so check */
} Opcoes;

/* argv montado em uma linha: "forja" e os argumentos (os que tem espaco, entre aspas). O que passa de n e cortado. */
void OpcoesLinhaDeComando(int argc, char **argv, char *saida, size_t n);

/* Le argv[1] (comando) e as opcoes de argv[2..]. Devolve NULL se ok, ou o motivo do uso errado. */
const char *OpcoesLer(int argc, char **argv, Opcoes *o);

/* Regras por comando (quais opcoes valem onde). Devolve NULL se ok, ou o motivo do uso errado. */
const char *OpcoesValidar(const Opcoes *o);

/* forja test <prefixo>:<nome> (S4 P2): monta alvo_teste, caminho_alvo e raiz_alvo e poe o caminho virtual em entrada. NULL se ok (ou se nao e alvo), ou o motivo do uso errado. */
const char *OpcoesPrepararAlvoDoTeste(Opcoes *o);

bool ComandoEh(const Opcoes *o, const char *nome);
bool ModoEhC(const Opcoes *o);
bool ModoEhDll(const Opcoes *o);

/* --max-erros N (teto de diagnosticos da semantica): N de 1 a 100; 0 = nao pedido (o padrao e da DiagLista) ou valor invalido (OpcoesValidar recusa). */
uint32_t OpcoesMaxErros(const Opcoes *o);

/* forja test (L16a): passa --filtro e --json ao exe pelas variaveis FORJA_TESTE_FILTRO e FORJA_TESTE_JSON do proprio processo (o filho herda);
 * variavel que o usuario ja tinha no ambiente e nao foi pedida na linha de comando e removida, para o exe nao filtrar sem querer. */
void OpcoesAmbienteDoTeste(const Opcoes *o);

/* "1".."64" (so digitos) vira *n e true; vazio, 0, 65, sinal ou texto = false. Vale para --jobs e para a variavel FORJA_JOBS. */
bool OpcoesJobsTexto(const char *texto, uint32_t *n);

/* M6 P66 (D-M6-5): secoes de debug (DWARF) ligadas? --simbolos sim|nao; sem a opcao, sim em dev e nao em release. */
bool OpcoesSimbolos(const Opcoes *o);

/* O0 P13: indice e nulo conferidos? Dev sempre; release por --checagem sim|nao (padrao em opcoes.c). */
bool OpcoesChecagem(const Opcoes *o);

/* true se o backend e o x64 (pedido com --backend x64 ou, depois de OpcoesResolverBackend, o padrao do dev). */
bool BackendEhX64(const Opcoes *o);

/* true se --target x86_64-linux (o ELF do M5: so o backend x64, so build e test com -o). */
bool AlvoEhLinux(const Opcoes *o);

/* Roda antes de ler o .fj: backend, --debug-code e alvo que nao existem nesta versao (ou combinacoes que o alvo nao faz) saem com X0003/X0002 em JSON no stderr; false = ja saiu o diagnostico (exit 2). */
bool OpcoesConferirBackendAlvo(const Opcoes *o);

/* --modo c e --prova-layout (backend C aposentado, 0.21.0): X0003 em JSON no stderr e false. Roda em PrepararOpcoes, antes de OpcoesValidar. */
bool OpcoesConferirAposentados(const Opcoes *o);

/* Padrao do backend (D-M4-8), calculado depois de ler e validar TODAS as opcoes: sem --backend, build/run/test de um arquivo (modo exe ou dll, dev ou --release)
 * ficam com "x64"; o build de projeto e os outros comandos ficam como estao (NULL). --backend explicito sempre vence. */
void OpcoesResolverBackend(Opcoes *o);

/* --entradas e --lib-pasta so existem no backend x64: sem ele, imprime X0003 (JSON em stderr) e devolve false. Roda depois de OpcoesValidar, antes de ler o .fj. */
bool OpcoesConferirSoX64(const Opcoes *o);

#endif
