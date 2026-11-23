/* main_uso.c - mensagem de uso do 'forja' (ver main_uso.h).
 * Versao: 0.6.23
 * Changelog: 0.6.23 - M8 P2: linha do --perfil.
 *            0.6.22 - 0.21.0 F4 passo 1: sai a linha do --modo c e as mencoes ao backend c (--backend so aceita x64); o --prova-layout nao aparece no uso.
 *            0.6.21 - S3 (SIMD): --sem-otim cita o passo `slp` (gravacoes contiguas da mesma conta viram vetor).
 *            0.6.20 - R6a-3: --sem-otim cita o passo `endereco` (cada acesso ganha a propria conta de endereco, que o isel funde no modo [x+i*s+d]).
 *            0.6.19 - R6a: --sem-otim cita o passo `licm` (aritmetica pura invariante do laco sobe para o pre-cabecalho).
 *            0.6.18 - R5 (SIMD): --sem-otim cita o passo `selecao` (losango puro vira selecao sem desvio).
 *            0.6.17 - M7b R4b: --sem-otim cita os passos `cfg` (simplifica o grafo de blocos) e `layout` (ordem dos blocos).
 *            0.6.16 - M7b R4a: --sem-otim inline cita folha com laco e recursao direta de 1 nivel.
 *            0.6.15- M7b R3: --sem-otim cita o passo `numeracao` (conta, carga e copia repetidas).
 *            0.6.14- M7b F3: --sem-otim cita o passo `sroa` (agregado local pequeno vira variaveis em registrador).
 *            0.6.13 -M7b F2: --sem-otim cita o passo `inline` (funcao folha pequena vira copia do corpo).
 *            0.6.12 - M7b F1: --sem-otim cita o passo `divconst` (divisao e resto por constante; --sem-otim isel desliga tambem o divconst).
 *            0.6.11 - M7a: --sem-otim cita o passo `copia_arg`.
 *            0.6.10 - M7a F9a: --checagem com padrao sim no release; --sem-otim lista checagem, alocador e isel.
 *            0.6.9 - M7a F3: a linha do --sem-otim cita o passo `alocador`.
 *            0.6.8 - M7a F0: linha do --sem-otim.
 *            0.6.7 - O0 P13: linha do --checagem.
 *            0.6.6 - M6e P93d: linha do --guarda-memoria.
 *            0.6.5 - M6c P89: a linha do 'forja ligar' ganha [--propor].
 *            0.6.4 - M6b P79 (main): a linha do 'forja spec' ganha [--receitas]; a de 'consultar api' diz que o <arq.fj> e opcional em std:X e vendor:X.
 *            0.6.3 - M6b P75: a linha do --simbolos diz que o PE x64 grava o .pdb ao lado do exe.
 *            0.6.2 - M6 P69: a linha do --rastreio diz que no x64 dev a pilha e padrao (flag redundante) e no backend c e opt-in.
 *            0.6.1- M6 P68 (main): a linha do --simbolos tambem vale para o backend c (-g do gcc).
 *            0.6.0 - M6 P68: a linha do --simbolos diz "(alvos windows e linux)" (DWARF no PE e no ELF).
 *            0.5.0 - M6 P66: a linha de uso do --simbolos sim|nao (DWARF; hoje so no alvo linux).
 *            0.4.1 - F1 P2: [--canonico] na linha de uso do 'forja check'.
 *            0.4.0 - F1 P1b: a linha de uso do 'forja fmt' com arquivo, pasta, --aplicar, --json e --stdin.
 *            0.3.0 - F1 P1a: a linha de uso do 'forja fmt --stdin'.
 *            0.2.0 - IA1-L3d1: a linha de uso do 'forja migrar'.
 *            0.1.0 - IA1-I4: primeira versao; o texto e o do UsoErrado do main.c 0.49.0 com a linha do 'forja editar' depois da do 'corrigir'.
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <stdio.h>

#include "driver/main_uso.h"
#include "nativo/nativo.h"

int MainUsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: %s\n", motivo);
    fputs("uso: forja check [--stdin] [--canonico] [--max-erros N] <arq.fj> [--tempos <t.json>]\n"
          "     forja ast [--tipos] [--sem-pos] [--no <id>] [--projeto] <arq.fj | pasta> [--tempos <t.json>]\n"
          "     forja build [--modo exe] [--release] <arq.fj> -o <saida.exe> [--tempos <t.json>]\n"
          "     forja build --modo dll [--release] <arq.fj> -o <saida.dll> [--tempos <t.json>]\n"
          "     forja build --projeto [forja.proj] [--release] [--jobs N] [--plano] [--tudo] [--json]\n"
          "     forja test <arq.fj> [--filtro <trecho>] [--json] [--limite-tempo <seg>] [--limite-mem <MiB>] [--limite-saida <bytes|KiB|MiB>]\n"
          "     forja run [--release] <arq.fj> [--args-file <arq>] [limites do test] [-- <argumentos do programa>]\n"
          "     limites (run e test; valem so para o programa, nao para a compilacao): passou do tempo = R0013 e exit 102, da memoria = R0014 e exit 103, da saida = R0015 e exit 104 (saida cortada no limite)\n"
          "     --args-file: UTF-8, um argumento por linha (LF ou CRLF); linha vazia = argumento vazio; \\n e \\\\ escapam; vem antes dos argumentos depois do '--'\n"
          "     forja explicar <CODIGO> | --lista [--json] | [CODIGO] --exemplos\n"
          "     forja spec [--cartao] [--receitas] [--secao S] [--json] [--exemplo CODIGO] [CODIGO]\n"
          "     forja corrigir <arq.fj> [--codigo CODIGO] [--aplicar] [--tambem-provavel]\n"
          "     forja migrar <arq.fj | pasta> ... [--dry-run | --aplicar] [--comentarios] [--json]\n"
          "     forja fmt <arq.fj | pasta> ... [--aplicar] [--json] [--stdin]\n"
          "     forja editar <arq.fj | pasta> --ops <ops.json> [--aplicar]\n"
          "     forja consultar simbolos|escopo|refs|tipo|assinatura|campos|callers|callees|api <argumento> [<arq.fj | pasta>] [--modulo <caminho>] [--json]\n"
          "     forja consultar api std:<modulo> | vendor:<modulo>: sem arquivo; o E1001 de modulo ausente lista os disponiveis e sugere o parecido\n"
          "     forja consultar imports|testes|entrada [<arq.fj | pasta>] [--json]\n"
          "     forja mapa [<arq.fj | pasta>] [--json | --resumo] [--modulo <caminho>] [--limite <N>]\n"
          "     forja ligar <header.h> --link <lib> [-o saida.fj] [--propor: propoe creates/destroys nos procs, com comentario para conferir]\n"
          "     forja --versao\n"
          "     --resumo (check, build, run e test): cada diagnostico so com codigo, arquivo, linha, coluna, causa e sugestao; FORJA_LOG_DIAG=<arq.jsonl> anexa cada diagnostico ao arquivo\n"
          "     --rastreio (build, run e test): o erro de execucao (R-code) leva coluna, valor (R0006: indice e tamanho) e pilha .fj; no backend x64 em dev isso e o padrao (a flag fica redundante)\n"
          "     --checagem sim|nao (build, run e test): so com --release; padrao sim: confere indice/fatia (R0006) e nulo (R0005) como o dev; nao desliga por build; o dev sempre confere\n"
          "     --sem-otim <passo[,passo...]|tudo> (build, run e test): so com --release; desliga passes de otimizacao da BIR (passos registrados: checagem, que tira conferencia provada; copia_arg, que passa o endereco da origem no argumento agregado quando o chamado so le; inline, que troca a chamada de funcao folha pequena (com ou sem laco) pela copia do corpo e desenrola 1 nivel da recursao direta curta; sroa, que troca o agregado local pequeno de acesso simples por variaveis (registradores); slp, que troca 4 gravacoes f32 (2 f64) contiguas da mesma conta lane a lane por carga, conta e gravacao vetoriais (SSE); selecao, que troca losango ou triangulo puro (sem trap, memoria ou chamada) por selecao sem desvio (cmov; minss/maxss em real); cfg, que enfia salto em salto e funde bloco com o unico sucessor; costura, que copia bloco pequeno puro para a aresta em que o desvio dele ja se decide (jump threading com faixa do parametro);numeracao, que reaproveita conta, carga e copia ja calculadas; licm, que sobe para o pre-cabecalho a aritmetica pura invariante do laco; endereco, que recoloca a conta de endereco compartilhada na frente de cada carga e gravacao; layout, que ordena os blocos para o sucessor quente cair direto; alocador, que poe os inteiros em registradores; isel; divconst, que troca divisao e resto por constante por multiplicacao e deslocamento e so age com o isel ligado: --sem-otim isel desliga os dois)\n"
          "     --guarda-memoria (build, run e test): o exe leva a tabela de linhas para o relato de uso de memoria liberada; so vale em dev, backend x64, exe (nao com --release nem --modo dll)\n"
          "     --perfil (build, run e test; dev e --release; so exe, windows e linux): cada proc de usuario ganha entrada e saida medidas; ao fim o exe grava <exe>.perfil.json (Chrome Trace, abre no Perfetto) e <exe>.perfil.txt (agregado por proc, o que a IA le); FORJA_PERFIL_SAIDA=<base sem extensao> troca o caminho; procs inlinadas nao aparecem (--sem-otim inline); nao vale com --modo dll nem --suporte\n"
          "     --simbolos sim|nao (build e test): grava DWARF no executavel para depurar com gdb e, no windows x64, o .pdb ao lado do exe; padrao: sim em dev, nao em --release (alvos windows e linux)\n"
          "     --backend x64 (build, run e test; o unico valor; padrao: x64 em dev e --release)\n"
          "     --target x86_64-windows|x86_64-linux (build e test; o linux implica --backend x64 e test exige -o)\n", stderr);
    return SAIDA_USO;
}
