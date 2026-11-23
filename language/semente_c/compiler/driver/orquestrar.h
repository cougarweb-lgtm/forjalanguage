/* orquestrar.h - orquestrador de projeto: forja build --projeto [forja.proj] [--release] [--jobs N] [--plano] [--tudo] [--json] (P22-O do M3).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M3 P22-O2: sem --plano o build executa (filhos em paralelo, orquestrar_exec.c); --jobs N e FORJA_JOBS; resumo, --json e exit agregado.
 *            0.1.0 - M3 P22-O1b: OrquestrarProjeto (so --plano: diz o que recompilaria e por que, sem disparar nada nem tocar no estado).
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#ifndef FORJA_DRIVER_ORQUESTRAR_H
#define FORJA_DRIVER_ORQUESTRAR_H

#include "driver/opcoes.h"

/* build --projeto: le o forja.proj (o.entrada, ou "forja.proj" da pasta atual), monta as camadas e:
 *  --plano: so diz o que recompilaria, em stdout, uma linha por produto na ordem (camada, nome), sem disparar nada nem tocar no estado:
 *   forja: [<nome>] recompilaria (<motivo>)  |  forja: [<nome>] reusado   e   forja: plano: <P> produtos, <R> recompilariam, <U> reusados
 *   --json: {"produto", "acao", "motivo"} por produto (motivo "" no reusado) e {"plano": {"produtos", "recompilariam", "reusados"}}.
 *   O plano usa o estado gravado: a dll que recompila tem a interface nova desconhecida ate o build, entao seus dependentes saem "reusado" ate la.
 *  sem --plano: um "forja build" filho por produto (processos, job object), ate --jobs N (1..64; senao FORJA_JOBS; senao o padrao) ao mesmo tempo, respeitando o grafo. O estado e
 *   gravado a cada produto pronto e a chave do dependente usa a hash_interface REAL da dll recem-feita. A saida de cada filho (stdout e depois stderr, bytes crus) sai em stderr
 *   ao termino dele, precedida de "forja: [<nome>] saida do build:"; no fim o resumo em stdout, na ordem (camada, nome):
 *   forja: [<nome>] recompilado (<motivo>) <ms> ms | reusado | falhou (exit <n>) | pulado (depende de <x>)  e
 *   forja: projeto: <P> produtos, <R> recompilados, <U> reusados, <F> falharam, <S> pulados, jobs=<J>, <ms> ms
 *   --json: {"produto", "acao", "motivo", "ms", "exit"} por produto e {"projeto": {..., "jobs", "ms", "filhos"}}.
 * --backend c = X0003; manifesto invalido = B0005; ciclo = B0007; dependencia nao declarada = B0006 (o produto falha, exit 1); projeto em build por outro processo = B0008.
 * Devolve o exit do forja: 0 | 2 (uso, B0005, B0007, B0008, X0003, --jobs ou FORJA_JOBS invalidos) e, na execucao, o MAIOR exit dos filhos que falharam (1..3). */
int OrquestrarProjeto(const Opcoes *o);

#endif
