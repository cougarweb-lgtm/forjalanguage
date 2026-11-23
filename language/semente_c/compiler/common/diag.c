/* diag.c - catalogo de diagnosticos e saida JSON.
 * Versao: 0.79.0
 * Changelog: 0.79.0 - Linguagem v1.13 L4b: textos do E1007 dizem caminho relativo a pasta raiz (caminho com unidade C: nao vale) e a causa real; codigo e tipo iguais.
 *            0.78.0 - Linguagem v1.13 L1a: catalogo do E1008 (conflito de caminho entre arquivo e pasta; o mesmo arquivo entraria duas vezes), depois do E0913.
 *            0.77.0 - M8 F0 [D-M8-7]: textos de X0801, X0802 e X0803 dizem exe x referencia (oraculo congelado ou build sem plantio) em vez de backend C x nativo; codigos e forma_certa iguais.
 *            0.76.0 - 0.21.0 F4 passo 2b: descricao do X0906 sem a conferencia da prova de layout do backend C (aposentado); codigo e forma_certa iguais.
 *            0.75.0 - 0.21.0 F4 passo 2a: forma_certa de X0101, X0103, X0105, X0109, X0801..X0803 e X0901..X0906 sem "use --backend c" (o backend C nao existe mais); codigos e textos de descricao iguais.
 *            0.74.0 - 0.21.0 F4 passo 1: textos do B0001 (historico, backend C aposentado), do X0001 (sem --backend c) e do X0003 (--backend c, --modo c e --prova-layout aposentados); codigos iguais.
 *            0.73.1 - O0 P13: textos de R0005 e R0006 dizem que o release confere com --checagem sim.
 *            0.73.0 - M6e P93g: catalogo do E0913 (ponteiro para dentro de lista/map invalidado pela mutacao da colecao), depois do E0912.
 *            0.72.4 - M6e P93c: texto do R0008 cita o campo bloco e as 3 pilhas (pilha, pilha_alocacao, pilha_delete).
 *            0.72.3 - M6e P93b: texto do R0008 diz que delete(p) zera o nome p.
 *            0.72.2 - M6d P92d: texto do R0009 cita o "tipo": "lib".
 *            0.72.1 - M6d P92b: texto do E0912 cita `ends`.
 *            0.72.0 - M6c P85: catalogo do E0912 (clausula creates/destroys/takes do proc foreign), depois do E0342.
 *0.71.0 - M6c P83: catalogo do E0342 (c_long/c_ulong so como tipo de fronteira), depois do X0114.
 *            0.70.0 - M6 P68: texto do X0905 cita PE e ELF (a autoconferencia dos dois escritores; pendencia do M5).
 *            0.69.0 - P72 (D0653 item 1): o diagnostico sem edicao ganha "correcao" pela tabela de diag_corrige.c (DiagCorrigirPadrao); TamanhoDoToken passa a DiagTamanhoDoToken (publico no diag_corrige.h).
 *            0.68.0 - M5c-b P61: X0113 e X0114 (TLS no ELF: ligacao crua e relaxamento GOTTPOFF) depois do X0112; X0111 so para simbolo TLS fora do bloco.
 *            0.67.2 - M5b P41: catalogo do X0112 (ELF --pie: ABS32/ABS32S e TEXTREL), depois do X0111.
 *            0.67.1 - M5b P37c: texto do X0003 (mesmo codigo) cita os motivos que ainda restam no alvo linux (std:thread/sync/atomic/net, lib .so, TLS); foreign com lib .a em vendor\linux ja liga.
 *            0.67.0 - M5b P35: catalogo do X0110 (tipo backend: relocacao ELF fora do censo, com o tipo e o membro), depois do E0339.
 *            0.66.0 - M5 P48 (S18): catalogo do E0339 (tipo nao copiavel), depois do E1007.
 *            0.65.1 - POS-Z A52: texto do X0109 com o teto de 17 DLLs (KERNEL32 e ADVAPI32 do runtime inclusas) e de 320 nomes importados (era 256).
 *            0.65.0 - B12: catalogo do E1007 (arquivo: #load que nao abre ou passa de 16 MiB), depois do F0001.
 *            0.64.0 - Portao Z z5: o diagnostico sem sugestao sai com o pacote (trecho, correcao [], confianca so_dica, relacionados, spec, id; DiagPacoteSemSugestao); catalogo do E0311 deixa de dizer que 'T ! E' com lista dentro nao existe.
 *            0.63.0 - F1 P2: catalogo do F0001 (tipo formato), depois do E0338.
 *            0.62.0 - S4 P1: catalogo do E0802 diz "unico no modulo" (o titulo e unico por modulo).
 *            0.61.0 - S3 P3: texto do E0334 cobre math.<fn> (std:math: f32 ou f64, mistura = E0334).
 *            0.60.0 - S2b: catalogo do E0338 (rng de shuffle e choice), depois do E0337.
 *            0.59.0 - K08b: catalogo do E0337 (ordenacao: binary_search e sort_by_key), depois do E0336.
 *            0.58.0 - B01a: catalogo do E0336 (tipo Allocator), depois do E0110.
 *            0.57.0 - IA1-L9a/L9b: catalogo do E0334 cobre wrapping_/saturating_/checked_/overflowing_, rotate, contagens de bits, byte_swap e mul_hi_u64.
 *            0.56.0 - IA1-L3f: catalogo do E0110 cobre tambem o NOME ANTIGO renomeado (em, escopo, para, std:arquivo, vendor:banco...): "forma antiga; rode forja migrar".
 *            0.55.0 - IA1-L3c: catalogo cita a forma nova (alloc = heap|frame|scope, transmute(x, to = T)) no lugar de em/escopo/para.
 *            0.54.0 - IA1-L3b: catalogo do E0502 (posicional depois de nomeado ou argumento a mais; V-06 = B).
 *            0.53.0- IA1-L1: catalogo do E0110 (forma de outra linguagem), depois do R0015.
 *            0.52.0 - IA1-b14: DiagRelatar zera os relacionados; o diagnostico sem sugestao que tem relacionados imprime "relacionados" antes do spec (o com sugestao imprime no pacote, diag_pacote.c); o diagnostico sem relacionados sai byte a byte igual.
 *            0.51.0 - IA1-b10: catalogo do R0013 (tempo), R0014 (memoria) e R0015 (saida), depois do E1006: os limites do forja run/test; DiagCodigoForma.
 *            0.50.0 - IA1-b09: o campo "spec" (diag_spec.c) sai em todo diagnostico que nao e --resumo (no fim da linha; no pacote do erro, antes do id), no forja explicar e no diagnostico sem fonte; DiagCodigoExplicacao.
 *            0.49.0 - IA1-b08: forma por ocorrencia de todo codigo (diag_forma.c, diag_forma_tab.c): a explicacao e a forma_certa saem do passo da analise, da regra da tabela ou da frase geral (linha, causa, forma do catalogo), nunca so do texto fixo; o catalogo deixa de citar tipo de exemplo inventado (Grifo, Bala, Vertice, Circulo, Particula, Janela, Bando, ErroCarga, ErroIo, Cor).
 *            0.48.0 - IA1-b07: forma por ocorrencia (Diag.explicacao/Diag.forma, DiagForma, DiagDescreverOnde): a explicacao e a forma_certa da ocorrencia saem no lugar das do catalogo, e o --resumo encurta a forma da ocorrencia; o catalogo do E0301 deixa de citar nome de exemplo.
 *            0.47.0 - IA1-b04: o diagnostico com sugestao (Diag.sug) imprime o pacote do erro (diag_pacote.c: trecho depois de coluna_fim; correcao, confianca, sugestao, alternativas, relacionados, spec e id no fim); DiagRelatar zera as sugestoes; ImprimirTextoJson deixa de ser static; o --resumo usa a descricao da sugestao. Diagnostico sem sugestao sai byte a byte igual.
 *            0.46.0 - IA1-b03: DiagImprimirJsonFerramenta (diagnostico sem fonte com "ferramenta": {"nome","saida"}, o B0004 com o que o ld escreveu); DiagImprimirJsonSemFonte sai igual; textos do B0004 (a mensagem do ld vai em ferramenta.saida) e da forma_certa do X0101 (o backend C tambem confere; sai o "use --backend c").
 *            0.45.0 - IA1-b02: DiagModoResumo (--resumo: codigo, arquivo, linha, coluna, causa e sugestao curta; sem fonte, codigo, causa e sugestao) e FORJA_LOG_DIAG (a mesma linha impressa em stdout/stderr tambem em anexo no arquivo); o modo completo sai byte a byte igual.
 *            0.44.0 - IA1-b01: catalogo do E1006 (tipo modulo), depois do R0012; E1001 cita std:os no lugar de std:proc; R0012 fala de std:os.
 *            0.43.0 - M5 P27a: textos do X0002 e do X0003 no catalogo (o alvo x86_64-linux existe: o X0003 cobre as combinacoes que ele recusa e a forma certa diz o caminho); nenhum codigo novo.
 *            0.42.0 - M5 P26b: catalogo do R0012 (tipo runtime), depois do X0109; exit(code) com codigo fora de 0..255.
 *            0.41.0- M3b P23y-2: catalogo do X0109 (tipo backend), depois do X0107; teto de DLLs com import (16, KERNEL32 do runtime inclusa) ou de nomes importados (256).
 *            0.40.0 - M3b P23f: catalogo do X0107 (tipo backend), depois do X0108; secao .tls (thread_local) de membro de biblioteca estatica.
 *            0.39.0 - M3b P23e: catalogo do X0108 (tipo backend), depois do X0106; dependencia implicita (.drectve /DEFAULTLIB ou -l) fora das pastas de busca.
 *            0.38.0 - M3b P23d: catalogo do X0106 (tipo backend), depois do B0008; simbolo exigido definido em duas bibliotecas estaticas.
 *            0.37.0 - M3 P22-O2: catalogo de B0006 (dependencia nao declarada) e B0008 (projeto em build por outro processo), depois do B0007.
 *            0.36.0 -M3 P22-O1b: catalogo de B0005 (forja.proj invalido) e B0007 (ciclo de dependencia entre produtos), depois do E0911.
 *            0.35.1 - Linguagem v1.12 M3 P21v-b: texto do R0005 cobre a chamada por proc nulo (so a descricao; nenhum codigo novo).
 *            0.35.0 -Linguagem v1.12 M3 P21v-a: catalogo de E0909 (assinatura de proc fora da fronteira C), E0910 ('as proc' de origem invalida) e E0911 (campo proc sem valor zero), depois do X0103; texto do E0907 (proc como valor).
 *            0.34.0 -M3 P19b: textos do X0105 (import library de 32 bits, ar malformado, dado/ordinal, exe sem export) e do X0103 (import library e exe ja ligam) no catalogo.
 *            0.33.0 - M3 P19: catalogo do X0101, X0102 e X0103 (tipo backend), depois do X0105; foreign sem simbolo, sem arquivo de biblioteca e com formato que ainda nao liga.
 *            0.32.0 - M3 P19a: catalogo do X0105 (tipo backend), depois do R0011; biblioteca achada que nao e do tipo pedido (PE de 32 bits).
 *            0.31.0 -Linguagem v1.11 L16b: catalogo do R0011 (tipo runtime), depois do E0335; excecao do processador dentro de um teste.
 *            0.30.0 -Linguagem v1.11 L15a: catalogo do E0335 (tipo array), depois do E0333; [dynamic, N]T de capacidade fixa; texto do R0007 cita append na [dynamic, N]T cheia.
 *            0.29.0 - Linguagem v1.11 L14a: catalogo do E0333 (tipo quat), depois do E0334; tipo quat, literal, campos, + - escalar e conj.
 *            0.28.0 -Linguagem v1.11 L13: catalogo do E0334 (tipo numerico), depois do E0332; min, max, abs, clamp e sqrt.
 *            0.27.0 -Linguagem v1.11 L12b: catalogo do E0332 (tipo array), depois do E0331; swizzle xyzw/rgba e escrita de uma letra so.
 *            0.26.0 - Linguagem v1.11 L12a: catalogo do E0331 (tipo array), depois do E1005; so + - * / e as formas += e -= (a linguagem ainda nao tem *= nem /=).
 *            0.25.1 -Linguagem v1.11 L10a-bis: textos do E0601, E0707, E0708 e E0709 (_ = try f(), var multipla, nome com sublinhado).
 *            0.25.0 - Linguagem v1.11 L10a: catalogo do E0707, E0708, E0709 (tipo fluxo) e E1005 (tipo modulo), depois do E0329.
 *            0.24.0 -Linguagem v1.11 L9: catalogo do E0328 e do E0329 (tipo layout), depois do E0330.
 *            0.23.0 - Linguagem v1.11 L8: DiagRelatar zera Diag.calado; DiagListaGuardar descarta o diagnostico calado (cascata de declaracao envenenada).
 *            0.22.0 -Linguagem v1.11 L7: DiagListaGuardar, DiagListaOrdenar (insercao, estavel) e DiagListaImprimir.
 *            0.21.0 - Linguagem v1.11 L6: campo "coluna_fim" nos E-codes (entre coluna e causa): fim em bytes do token que comeca na coluna (TamanhoDoToken).
 *            0.20.1 - Linguagem v1.11 L2: texto novo do E0706 (explicacao e forma_certa) para a fuga por fluxo e a cadeia.
 *            0.20.0 - Linguagem v1.11 L1: catalogo do E0330; texto novo do E0301 (forma); DiagAcrescentarExtra e Diag.extras impressos depois de forma_certa; DiagRelatar zera os extras.
 *            0.19.0 - P18a: catalogo de X0801, X0802 e X0803 (tipo diferencial; textos do contrato, forja explicar).
 *            0.18.0 - M2 P11b: texto de X0906 sem "RESERVADO para o M2" (a etapa layout do backend nativo o produz: LayoutConferir e forja_rt_layout do runtime).
 *            0.17.1 - portao M1: texto de X0906 declara que e RESERVADO para o M2 (sem produtor no M1).
 *            0.17.0 - backend x64 P8b: DiagImprimirJsonExtras (campos extras depois de forma_certa); DiagImprimirJson sai byte a byte igual.
 *            0.16.0 - backend x64 P3: textos de X0302 (backend) e X0901..X0906 (interno; causas do contrato M1, secao ESCOPO).
 *            0.15.0 - backend x64 M0-R: catalogo de X0001, X0002 e X0003 (tipo backend; textos do plano, secao 15.2).
 *            0.14.0 - Linguagem v1.10: textos de E0901 (struct por valor atravessa), E0310 (^T e ?^T de foreign), E0905 (alias) e G0013 (alias no lugar de "declare a mao").
 *            0.13.0 -Linguagem v1.9 P1: E0318 (map), E0319 (bit_set), E0320 (soa), E0321 (matrix); texto do E0308 sem a promessa da v1.9.
 *            0.12.0- Linguagem v1.8b P1: catalogo de E0907, E0908, R0010 e 22 codigos G; textos de E0904, E0310, R0005 e E1001 atualizados.
 *            0.11.0 - Linguagem v1.8 P1: catalogo de E0316, E0317, E1001..E1004 e R0009; textos de E0906 e E0310 atualizados; DiagImprimirJson usa FonteLocalizarNoArquivo.
 *            0.10.0 - Linguagem v1.7 P2: catalogo de E0312..E0315; textos de E0306, E0309, E0310, E0311, E0606, E0607, E0608, E0705, E0901 e E0904 atualizados.
 *            0.9.0 - Linguagem v1.6 P2: catalogo de E0310, E0311, R0007 e R0008; textos de E0308, E0309, E0705, E0901 e E0904 citam [dynamic]T.
 *            0.8.0 - Linguagem v1.5 P2: catalogo de E0308, E0309, E0421 e R0006; textos de E0705, E0706, E0901 e E0904 citam array e fatia.
 *            0.7.0 - Linguagem v1.4 P2: catalogo de E0904, E0905, E0906 e B0004; R0005 cobre o retorno NULL de proc foreign.
 *            0.6.1 - Linguagem v1.3: forma_certa do E0705 cita a escrita atraves de ponteiro.
 *            0.6.0 - Linguagem v1.3: catalogo de E0306, E0307, E0706 e R0005 (ainda sem uso na semantica; entram para o teste_common).
 *            0.5.0 - Linguagem v1.2: textos de E0303, E0304, E0305 e E0609; E0420, E0901 e R0002 passam a falar de largura e do 'as'.
 *            0.4.0 - Linguagem v1.1: textos de E0901..E0903, R0004 e B0003.
 *            0.3.0 - Fase 5h: textos de R0001..R0003 (runtime) e B0001..B0002 (build); DiagExplicarJson e
 *                    DiagImprimirJsonSemFonte.
 *            0.2.0 - Fase 5c: textos dos codigos novos (caixa, nome repetido, resultado, fluxo, test).
 *            0.1.0 - Fase 5a: textos do catalogo copiados do prototipo (contrato v1).
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/diag.h"

#include "common/diag_corrige.h"
#include "common/diag_forma.h"
#include "common/diag_pacote.h"
#include "common/diag_spec.h"

#include <stdarg.h>
#include <stdlib.h>

typedef struct { const char *codigo, *tipo, *explicacao, *forma; } ItemCatalogo;

static const ItemCatalogo CATALOGO[DIAG_QTD] = {
    {"E0001", "lexico", "O texto tem um caractere que a Forja nao reconhece ou uma string sem fechar na linha.", "Remova o caractere ou feche a string com \"."},
    {"E0101", "sintaxe", "O token nao cabe aqui pela gramatica da Forja.", "Confira o comando: um por linha, com os delimitadores no lugar."},
    {"E0201", "nome", "O nome nao foi declarado antes do uso (ou falta proc main sem parametros e sem retorno).", "Declare o nome ou corrija a grafia."},
    {"E0301", "tipo", "Os tipos nao pertencem a mesma categoria e a Forja nao converte sozinha.", "Converta com '<valor> as <Tipo>' ou ajuste o tipo declarado; literal inteiro vira real sozinho quando exato; real em inteiro pede 'as' (e trunca)."},
    {"E0302", "tipo", "Proc com retorno precisa terminar em return.", "Acrescente return <expr> como ultimo comando do corpo."},
    {"E0412", "unidade", "As dimensoes das unidades nao combinam, ou o resultado nao tem unidade declarada.", "Use a mesma unidade ou declare a unidade do resultado."},
    {"E0420", "faixa", "A constante esta fora da faixa declarada para o campo ou nao cabe na largura do tipo (u8 vai de 0 a 255; int de -9223372036854775808 a 9223372036854775807).", "Use um valor dentro da faixa ou da largura, declare um tipo maior (i64, uint) ou converta em runtime com 'as'."},
    {"E0501", "argumento", "Argumento nomeado fora da ordem declarada ou repetido.", "Passe os argumentos nomeados na ordem do proc, sem repetir."},
    {"E0502", "argumento", "Argumento posicional depois de um nomeado, ou argumento a mais que os parametros.", "Escreva os posicionais primeiro e os nomeados depois: proc(valor, nome = valor)."},
    {"E0503", "argumento", "Falta um argumento obrigatorio (sem padrao).", "Passe o argumento que falta ou de um padrao ao parametro."},
    {"E0504", "argumento", "O nome nao existe entre os parametros do proc ou os campos da struct.", "Use um nome declarado."},
    {"E0102", "caixa", "O nome nao segue a caixa da Forja: tipo, unidade, struct e enum em PascalCase; constante em MAIUSCULO; o resto em snake_case; sempre ASCII sem acento.", "Renomeie para a forma sugerida na causa (ex.: MetrosPorSegundo, VIDA_MAX, soma_ate)."},
    {"E0202", "nome", "O nome ja foi declarado (ou e reservado pela linguagem); a Forja nao tem sombra nem sobrecarga.", "Escolha outro nome."},
    {"E0601", "resultado", "Um valor 'T ! E' nao pode ser ignorado: o erro precisa ser tratado.", "Use try (repassa o erro), _ = try f() (repassa o erro e descarta o valor), or_else (valor padrao), switch (trata os dois casos) ou let (guarda o resultado)."},
    {"E0602", "resultado", "'try' so repassa o erro dentro de um proc cujo retorno e '! E' ou 'T ! E'.", "Declare o retorno do proc como 'T ! E' ou trate o resultado com or_else ou switch."},
    {"E0603", "resultado", "O enum de erro do 'try' e diferente do enum de erro declarado no retorno do proc.", "Use o mesmo enum de erro no retorno do proc ou trate o resultado com switch."},
    {"E0604", "resultado", "or_else exige um resultado com valor a esquerda e, a direita, um valor do mesmo tipo desse valor.", "Escreva <resultado> or_else <padrao> com padrao do tipo T de 'T ! E'."},
    {"E0605", "resultado", "O tipo depois de '!' precisa ser um enum de erro.", "Declare enum <NomeDoErro> { ... } e escreva T ! <NomeDoErro>."},
    {"E0606", "resultado", "'try' so se aplica a resultado ('T ! E'); 'or_else' aplica-se a resultado com valor, a ?T e a ?^T.", "r or_else padrao  -  x or_else 0 (x: ?i32)  -  p or_else &g (p: ?^T)  -  try so com 'T ! E'"},
    {"E0607", "resultado", "O switch precisa cobrir todos os casos (membros do enum; valor e erro; T e none; todas as variantes da union) ou ter 'default'.", "Acrescente os case que faltam ou um 'default:'."},
    {"E0608", "resultado", "O switch aceita enum, resultado, ?T, ?^T e union; cada case precisa ser valido, unico e do tipo do valor.", "enum: case Enum.membro:  -  resultado: case Tipo: / case Erro:  -  ?T: case T: / case none:  -  union: case Variante:"},
    {"E0701", "fluxo", "break e continue so existem dentro de while ou for.", "Mova o comando para dentro do laco ou remova-o."},
    {"E0702", "fluxo", "O erro de uma chamada adiada com defer nao tem para onde ir.", "Adie uma chamada que nao devolve resultado, ou trate o resultado antes."},
    {"E0703", "fluxo", "O return nao combina com o retorno do proc: em 'T ! E' devolva um T ou um membro do enum E.", "Use return <valor do tipo T> ou return <E>.membro."},
    {"E0704", "fluxo", "Comando depois de return, break ou continue nunca executa.", "Remova o comando ou mova-o para antes."},
    {"E0705", "fluxo", "Variavel de let, parametro, variavel de for e ligacao de switch sao imutaveis; so 'var' aceita atribuicao. Tambem vale para '&', fatia e 'for &item' de algo imutavel (let, parametro, item de for, valor de chamada); elemento de fatia e campo atraves de ponteiro sao alteraveis. Tambem vale para append(&x) e delete(x) de lista imutavel. '&x' de let ou parametro tambem e E0705: o endereco deixaria alterar o valor.","Declare com 'var' ou passe um ponteiro '^T' para alterar o valor, ou escreva atraves de um ponteiro (p^ = v, p.campo = v)  -  var a: [4]int = {} antes de for &x in a  -  var x = 3  let p = &x."},
    {"E0801", "teste", "expect so pode aparecer dentro de um bloco test.", "Use assert(cond, msg = \"...\") fora de test."},
    {"E0802", "teste", "O test precisa de um titulo entre aspas, nao vazio e unico no modulo.", "Escreva test \"o que o teste prova\" { ... }."},
    {"E0901", "exportacao", "Um proc exportado so aceita e devolve tipos de forma fixa em C: i8..i64, u8..u64, int, uint, f32, f64, bool, enum e ponteiro para struct. Struct por valor atravessa (o .h define a struct) se todos os campos atravessam; resultado 'T ! E' e texto (str) ainda nao atravessam a fronteira. Array e fatia nao atravessam: passe ^T do 1o elemento e n: u64. Lista [dynamic]T tambem nao atravessa. ?T (menos ?cstr), union e retorno multiplo nao atravessam. cstr devolvido sai so com return format_c(..., alloc = heap) direto, e o C libera com forja_liberar(p).", "Troque o tipo por um dos aceitos (ex.: devolva i32 com o codigo de erro, ou passe a struct por ponteiro ^<Struct>)  -  pub export proc nome() -> cstr { return format_c(f\"texto\", alloc = heap) }."},
    {"E0902", "exportacao", "O nome exportado vira simbolo de DLL e nao pode ser palavra do C, nome da libc usado pelo C gerado, 'main', nem comecar com fj_ ou forja_.", "Escolha outro nome para o proc exportado (ex.: somar_valores)."},
    {"E0903", "exportacao", "'export' so existe junto de 'pub' e so em proc; 'pub' nao vale em test.", "Escreva pub export proc <nome>(...) ou tire o 'export'."},
    {"E0303", "bits", "Operadores de bits (& | ~ << >>) so valem em inteiro sem sinal (u8, u16, u32, u64, uint); em inteiro com sinal o bit de sinal deixaria o resultado dependente da representacao.", "Declare o valor como sem sinal (let m: u32 = 1 << 3), ou converta: (x as u32) & 255, ou transmute(x, to = u32) para os bits crus."},
    {"E0304", "transmute", "transmute reinterpreta os bits de um valor como outro tipo; so vale entre tipos numericos do mesmo tamanho (1: i8 u8; 2: i16 u16; 4: i32 u32 f32 e unidades; 8: int uint i64 u64 f64).", "Use transmute(x, to = u32) com x de 4 bytes; para mudar o VALOR (e nao os bits) use <x> as <T>."},
    {"E0305", "tipo", "O '-' unario nao existe em inteiro sem sinal: o resultado seria negativo.", "Use um tipo com sinal (i32, int) ou escreva a subtracao explicita (0 - x trava em dev se x > 0)."},
    {"E0609", "resultado", "Em 'T ! E' o tipo do valor e o enum de erro precisam ser diferentes; iguais, um 'return E.membro' nao diz se e valor ou erro.", "Crie um enum de erro proprio (ex.: enum <ErroDoModulo> { ... }) e use T ! <ErroDoModulo>."},
    {"E0306", "ponteiro", "'^T' e o ponteiro que nunca e nulo: le-se com 'p^' (ou 'p.campo' em struct, um nivel so). '?^T' pode ser none: leia com switch q in p { case ^T: ... case none: ... } ou p or_else &padrao (o 'p as ^T' antigo continua); rawptr e o 'void *' do C e nao se le. Ponteiro nao tem conta nem ordem, nao vira numero, e 'as' nao troca o tipo apontado direto.", "if p != none { let q = p as ^T  q.campo = 1 }  -  pp^.campo para ^^T  -  (p as rawptr) as ^Outro para trocar o tipo apontado  -  let p: ?^T = none  -  switch q in p { case ^T: q.campo = 1 case none: }."},
    {"E0307", "texto", "'str' e o texto da Forja (ponteiro + tamanho em bytes UTF-8, sem NUL obrigatorio); 'cstr' e o texto do C terminado em NUL, para foreign e export. Um literal \"...\" vira o que o contexto pede. cstr -> str e 'c as str'; str -> cstr precisa alocar (v1.6). == em cstr compararia enderecos; str nao se soma nem tem ordem.", "let c: cstr = \"texto\"  -  (c as str) == \"abc\"  -  len(s) com s: str."},
    {"E0706", "fluxo", "Variavel local e array local deixam de existir quando o proc retorna. O endereco deles (&x, &x.campo, &a[i]) e a fatia de array local (a[..]) nao saem do proc por caminho nenhum: return (direto, por let/var, dentro de struct, array, tupla ou ?T), gravacao em global, em memoria de heap ou apontada por parametro, append/insert em lista que sai do proc, ou argumento de proc que guarda o parametro. A causa mostra a cadeia (x -> p -> return). Emprestar a um proc que so usa durante a chamada vale; foreign nao e conferido.", "Devolva o valor (return x) ou receba o destino do chamador por ponteiro (proc preencher(saida: ^T) { saida.campo = 1 }), ou copie o valor."},
    {"E0904", "foreign", "Um proc foreign e uma funcao C: so atravessam tipos de forma fixa em C - i8..i64, u8..u64, int, uint, f32, f64, bool, enum (int32_t), unidade (float), ^T, ?^T, rawptr, cstr (entrando e saindo) e struct por valor cujos campos atravessam. str, 'T ! E', '! E' e struct com str ficam de fora. Array e fatia nao atravessam: passe ^T do 1o elemento e n: u64. Lista [dynamic]T tambem nao atravessa. ?cstr (NULL = none) atravessa; outros ?T, union e retorno multiplo nao. Mapa do C: int = i32, long = i32 no Windows, long long = i64, size_t = u64, double = f64, char* de leitura = cstr, handle opaco = rawptr ou ^Struct sem campo; funcao C que devolve int como verdadeiro/falso = i32, nao bool. Callback: parametro 'proc(nome: T) -> R' (E0907). Tipo opaco: 'opaque Nome' e ^Nome (E0908). Struct por valor pode ter campo struct e array [N]T de tipos que atravessam. Funcao variadica ('...') nao atravessa: declare uma funcao C de parametros fixos. char ** = ?^cstr; retorno char * = ?cstr emprestado (delete nao libera).", "foreign { link \"m\"  proc sqrt(x: f64) -> f64 } (um item por linha)  -  erro do C volta como i32 e vira enum na Forja  -  texto para o C: cstr (literal ou let c: cstr = \"...\")."},
    {"E0905", "foreign", "O nome do proc foreign e o simbolo C, igual ao do header (a caixa do C vale; os parametros seguem snake_case); com alias 'proc c_remove = \"remove\"(...)' o nome e da Forja (snake_case) e o simbolo C vem entre aspas (identificador C, sem repetir outro nome de topo nem outro alias); 'main' e os prefixos fj_ e forja_ sao do compilador (no simbolo C tambem). Em 'link \"x\"' vai so o nome da biblioteca, que vira -lx no gcc: sem caminho, sem extensao, sem prefixo de opcao; o ld procura libx.a, libx.dll.a, x.lib, libx.dll e x.dll na pasta do .fj e nas pastas do gcc.", "link \"raylib\" (raylib.dll ou libraylib.a ao lado do .fj)  -  proc InitWindow(largura: i32, altura: i32, titulo: cstr)."},
    {"E0906", "foreign", "'export' nao se aplica a foreign: o proc foreign e um simbolo de outra biblioteca; quem exporta e a Forja, por 'pub export proc'. 'pub foreign' vale: deixa os procs visiveis a quem importa o modulo.", "pub export proc raiz(x: f64) -> f64 { return sqrt(x) }"},
    {"E0308", "array", "[N]T e um array de N elementos (N literal inteiro ou const inteira, maior que zero): e um valor - copia ao atribuir, passar e devolver. []T e uma fatia (ponteiro + tamanho) que empresta os elementos de um array e pode altera-los. ^[N]T aponta um array. Indexe com a[i], fatie com a[lo..<hi], a[lo..=hi] ou a[..]; len(x) da o tamanho (em array e constante e nao avalia o argumento). Nao existem nesta versao: ==/!=/ordem e conta em array ou fatia, campo (.x .y .z .w) e [Enum]T (depois da v1.9; para vetor e matriz com conta use matrix[R, C]T), indexar ou fatiar str, faixa em campo array, array de 'T ! E'. Lista [dynamic]T: mesmas leituras (l[i], fatia, len, for), sem ==, conta nem campo.","var a: [4]int = {1, 2, 3, 4}  -  a[0] = 9  -  for i, x in a { ... }  -  let s = a[1..<3]  -  igualdade: for i, x in a { if x != b[i] { ... } }"},
    {"E0309", "array", "O literal de array {a, b, c} pega o tipo do contexto (let/var tipado, atribuicao, argumento, return, campo) e tem exatamente N elementos; {} e o valor zero (todos zero) e, num contexto []T, a fatia vazia. Sem contexto nao ha tipo; ^T e union nao tem zero (o literal precisa de todos os elementos); fatia nao tem literal com elementos (declare o array e fatie). Lista dinamica nao tem literal: nasce de make.","let a: [3]int = {1, 2, 3}  -  var z: [8]f32 = {}  -  let p: [2]^T = {&a, &b}  -  let s = a[..]"},
    {"E0421", "array", "Indice ou limite de fatia conhecido na compilacao (literal, const ou conta de constantes) fora do array: [N]T aceita indice 0..<N e limite de fatia 0..=N; negativo nunca vale. Valor so conhecido em execucao e conferido em dev/test (R0006).", "a[len(a) - 1] e o ultimo  -  a[..] e o array inteiro  -  if i < len(a) { usar(a[i]) }"},
    {"E0310", "memoria", "Alocacao explicita: new(T, alloc = heap|frame) devolve ^T com o valor inicial de T (struct: os padroes); make([dynamic]T, alloc = heap|frame, cap = N) cria uma lista vazia com espaco para N; append(&lista, item = x) acrescenta; delete(p) libera o que veio de new e delete(lista) libera a lista (e a deixa vazia); frame_reset() esvazia o alocador frame (chame uma vez por quadro; memoria de frame so vale dentro do quadro). alloc so aceita heap ou frame nesta versao (Arena e Pool existem na v1.8). O resultado de new e make precisa ser guardado. new nao cria lista (use make) nem ^T sem valor; delete de lista apontada e delete(p^). format e format_c tambem alocam: o resultado precisa ser guardado; delete(s) libera str, cstr ou ?cstr vindo de format/format_c alloc = heap. Em v1.8: alloc recebe heap, frame, scope, Arena ou Pool (import \"std:mem\"); Pool so em new, e volta com delete(p, alloc = pool); objeto de Arena se libera com arena_reset/arena_destroy. Texto (?cstr ou cstr) e ponteiro (^T ou ?^T) devolvidos por proc foreign sao emprestados da biblioteca C: delete deles nao existe.","var lista = make([dynamic]T, alloc = heap, cap = 256)  -  append(&lista, item = valor)  -  let p = new(T, alloc = heap)  -  defer delete(p)  -  delete(lista)  -  frame_reset()"},
    {"E0311", "memoria", "[dynamic]T e dono da memoria dos elementos; struct com campo lista e array de listas tambem. Dono nao se copia (copiar criaria duas donas: um append numa deixaria a outra apontando memoria liberada). Para alterar passe o endereco (^[dynamic]T); para ler ou alterar elementos empreste uma fatia (l[..]); valores novos (make, chamada, literal) se movem para a variavel, e 'return local' move a lista para fora. Parametro por valor, 'for item' sobre listas e campo dono com padrao nao existem; 'T ! E' aceita a lista dona no sucesso (proc ler() -> [dynamic]u8 ! FileError).", "proc encher(l: ^[dynamic]int) { append(l, item = 1) }  -  encher(&lista)  -  somar(lista[..])  -  for &linha in grade { ... }  -  proc criar() -> [dynamic]int { var l = make(...)  return l }  -  proc ler() -> (lista: [dynamic]int, erro: ?E)"},
    {"E0312", "opcional", "?T guarda um valor de T ou none. Nasce de um T (a conversao e automatica) ou de none; le-se so desembrulhando: switch v in x { case T: ... case none: ... } ou x or_else padrao. Compara-se so com none (x == none, x != none); nao faz conta nem tem campo. Nao existem ??T, ?rawptr (rawptr ja aceita none), ??^T, nem ?dono ([dynamic]T ou struct com lista: a lista vazia ja e a ausencia). ?^T continua o ponteiro anulavel; ?cstr e o char* que pode ser NULL. try nao se aplica a ?T.", "let a: ?i32 = none  -  a = 5  -  let v = a or_else 0  -  switch v in a { case i32: print(v) case none: print(0) }  -  if a != none { ... }"},
    {"E0313", "union", "union Nome { A, B } guarda exatamente um valor de uma das variantes (2 a 255 tipos distintos); o valor da variante vira union sozinho (let f: U = A{...}). Le-se so por switch v in f { case A: ... }, cobrindo todas as variantes ou com default. Union nao tem valor zero, ==, campo, conta nem literal proprio, e nao atravessa a fronteira C. Variante nao pode ser ?T, union, 'T ! E', lista nem struct com lista. Literal solto precisa do tipo (3 as i32).", "union U { A, B }  -  let f: U = A{campo = 2.0}  -  switch c in f { case A: print(c.campo) case B: print(c.outro) }  -  let v: U = 3 as i32 (se i32 e variante de U)"},
    {"E0314", "retorno", "Retorno multiplo: proc f() -> (q: int, r: int) devolve de 2 a 8 valores nomeados com return a, b (na ordem) ou return g() do mesmo tipo. Quem chama desestrutura com let q, r = f() (ou var), um nome por valor; _ descarta. A tupla nao vira variavel, nao tem campo e nao vai para argumento, print ou conta. Nao existe '(..) ! E': ponha o erro num elemento ?E. Nao atravessa export nem foreign.", "proc dividir(a: int, b: int) -> (q: int, r: int) { return a / b, a % b }  -  let q, r = dividir(7, b = 2)  -  let q, _ = dividir(7, b = 2)  -  proc ler() -> (dados: [dynamic]u8, erro: ?E)"},
    {"E0315", "texto", "f\"...\" monta texto com pedacos e {expressao} ou {expressao:formato}; vale so como 1o argumento de print, format (devolve str) e format_c (devolve cstr terminado em NUL). {expressao} aceita inteiros, bool, str, cstr, f32, f64 e unidade. Formato: [0][largura 1..255][.precisao 0..17][x] - largura alinha a direita, 0 preenche com zeros (so numero), .precisao so em real, x (hexadecimal) so em inteiro sem sinal. {{ e }} escrevem uma chave; dentro de {} nao vale aspas nem chaves. format e format_c pedem alloc = heap|frame: heap se libera com delete(s), frame vale ate o frame_reset().", "print(f\"Vida: {g.vida}\")  -  let s = format(f\"{x:08.3}\", alloc = frame)  -  let c = format_c(f\"id {n:x}\", alloc = heap)  defer delete(c)  -  print(f\"{{chave}}\")"},
    {"E0316", "memoria", "alloc = scope aloca no heap e libera sozinho no fim do bloco do let (defer delete automatico). Por isso so vale direto no let/var e o valor nao sai do bloco: return, guardar em campo, elemento ou ponteiro, literal, append, copia para outro local, reatribuicao e delete sao recusados. Emprestar para um proc (argumento) vale.", "let p = new(T, alloc = scope)  -  var l = make([dynamic]int, alloc = scope)  -  para entregar a outro dono use alloc = heap e delete"},
    {"E0317", "memoria", "Memoria de heap tem UM dono, que precisa liberar (delete) ou entregar (return, proc, campo, append) o bloco antes do fim do bloco e antes de return, break ou continue. Copiar o dono para outro local criaria dois donos; reatribuir antes do delete perderia o bloco.", "let p = new(T, alloc = heap)  defer delete(p)  -  return p  -  guardar(p)  -  alloc = scope quando o valor nao sai do bloco"},
    {"E0318", "map", "map[K]V e uma tabela de chave para valor, DONA da memoria como a lista: nasce de make(map[K]V, alloc = heap, frame, scope ou uma Arena, cap = N) (Pool nao serve), copiar e recusado (E0311) e delete(m) libera e a deixa vazia com o mesmo alocador. Chave: inteiro, enum, bool ou str (o texto da chave e emprestado: precisa viver tanto quanto o map). Valor: qualquer tipo menos ?T, ?^T, rawptr, resultado com erro e dono. insert(&m, key = k, value = v) grava (sobrescreve se a chave existe); get(m, key = k) devolve ?V; get_ptr(&m, key = k) devolve ?^V, valido ate o proximo insert, remove ou delete do mesmo map; has(m, key = k) devolve bool; remove(&m, key = k) apaga; len(m) conta. for k, v in m (ou for k, &v in m) percorre na ordem de insercao; remove move o ultimo par para o buraco. Dentro do for de m, insert, remove e delete de m sao recusados. m[k] nao existe.", "var m = make(map[str]int, alloc = heap)  defer delete(m)  -  insert(&m, key = \"ouro\", value = 10)  -  let n = get(m, key = \"ouro\") or_else 0  -  for k, v in m { ... }"},
    {"E0319", "bit_set", "bit_set[E] guarda um conjunto de membros do enum E num inteiro sem sinal (1 bit por membro, ate 64 membros, enum sem valor explicito). Literal pelo contexto: let s: bit_set[E] = {E.a, E.b} ({} = vazio). Operadores: | (uniao), & (intersecao), - (diferenca), == e !=. insert(&s, key = m), remove(&s, key = m), has(s, key = m) e len(s) (quantos membros). for item in s percorre os membros na ordem do enum; for &item e for i, item nao existem. Ordem (<, >), <<, >> e indice s[k] nao existem.", "enum E { a, b, c }  -  var s: bit_set[E] = {E.a}  -  insert(&s, key = E.b)  -  if has(s, key = E.a) { ... }  -  let u = s | t"},
    {"E0320", "soa", "soa[N]S e soa[dynamic]S guardam structs S coluna por coluna (cada campo num array proprio: melhor para a cache em laco que le poucos campos). S e struct da Forja com ao menos 1 campo e sem campo dono. a[i].campo le e grava; a[i] le a struct inteira (copia) e a[i] = s grava todos os campos; &a[i].campo vale; a.campo e a fatia []T da coluna (pede var); len(a); for item in a e for i, item in a. &a[i] e for &item nao existem (a struct nao fica inteira na memoria). soa[dynamic]S e dona como a lista: make(soa[dynamic]S, alloc = heap, cap = N), append(&a, item = s), delete(a). soa[]S nao existe.", "var a: soa[1024]S = {}  -  a[i].x += a[i].vx  -  soma(a.x)  -  var b = make(soa[dynamic]S, alloc = heap)  append(&b, item = S{x = 1.0})"},
    {"E0321", "matrix", "matrix[R, C]T e uma matriz de R linhas e C colunas (R e C literais de 2 a 4; T = f32 ou f64), guardada por coluna (a ordem da GPU). Literal por linhas, com R*C elementos: let m: matrix[2, 2]f32 = {1.0, 2.0, 3.0, 4.0} ({} = zero). m[linha, coluna] le e grava; &m[i, j] vale. Contas: + e - entre matrizes do mesmo tipo, * de matriz por matriz (colunas da 1a = linhas da 2a), * por escalar T (dos dois lados), * por vetor [C]T (devolve [R]T), -m e transpose(m). Nao existem: ==, /, m[i], for, len, print e unidade no elemento.", "var mvp: matrix[4, 4]f32 = {}  -  let p = proj * view  -  let v = mvp * pos  -  mvp[0, 3] = 1.0  -  let t = transpose(mvp)"},
    {"E1001", "modulo", "import \"caminho\" so no topo do arquivo. O caminho e relativo a pasta do arquivo raiz, com segmentos snake_case separados por '/', sem extensao, sem '..' e sem caminho absoluto; \"std:mem\" e a biblioteca padrao embutida; \"vendor:x/y\" le vendor\\x\\y.fj ao lado do arquivo raiz (as bibliotecas de link do modulo ficam em vendor\\); \"engine:x\" le %FORJA_ENGINE%\\forja\\x.fj (bibliotecas em %FORJA_ENGINE%\\lib). O apelido (ultimo segmento ou 'as nome') e unico no arquivo.", "import \"std:mem\"  -  import \"jogo/fisica\"  -  import \"jogo/fisica\" as fis  -  import \"std:os\" (o antigo std:proc)"},
    {"E1002", "modulo", "Os imports formam um grafo sem ciclo: um modulo nao pode importar, direta ou indiretamente, quem o importa (nem o arquivo raiz). Leve o que os dois usam para um terceiro modulo.", "a importa comum e b importa comum (em vez de a <-> b)"},
    {"E1003", "modulo", "Nome de outro modulo se escreve apelido.nome e so e visivel se foi declarado com pub. Sem apelido, so os nomes do proprio arquivo (e os da linguagem) sao visiveis.", "pub proc area(l: int) -> int  -  geo.area(3)  -  let p: geo.Ponto"},
    {"E1004", "modulo", "'proc main' e 'pub export proc' so valem no arquivo raiz (o que o forja recebe): o modulo importado e uma biblioteca do programa.", "deixe main e os export no arquivo raiz e chame o modulo de la"},
    {"E0907", "foreign", "Callback: um parametro de proc foreign pode ter o tipo 'proc(nome: T, ...) -> R' (ponteiro de funcao do C). O argumento e o nome de um proc da Forja com a mesma assinatura; o compilador gera um trampolim C que confere os ^T e cstr que o C passa (R0005 se vier NULL). O tipo proc(nome: T, ...) -> R tambem vale como campo de struct (inclusive de foreign), let/var, parametro e retorno de proc da Forja, e o valor e o nome de um proc da Forja (sem captura: nada de closure); a assinatura precisa ser igual (os nomes dos parametros nao contam). E0907 diz onde isso NAO vale: ?proc, proc dentro de [N]/[]/[dynamic]/map/union/soa/^, proc foreign como valor, proc dentro da assinatura de outro proc e proc de assinatura diferente da pedida. O contexto do C (void *user_data) chega como rawptr: converta com 'ctx as ^T'.", "foreign { proc qsort(base: rawptr, n: u64, tamanho: u64, comparar: proc(a: rawptr, b: rawptr) -> i32) }  -  qsort(&v[0] as rawptr, n = 5, tamanho = 4, comparar = comparar_i32)"},
    {"E0908", "foreign", "Tipo opaco: 'opaque Nome' dentro de foreign declara um tipo cujo layout so a biblioteca C conhece (struct incompleta do header). Ele so existe atras de ponteiro: ^Nome ou ?^Nome em parametro, retorno, campo e variavel. Valor por valor, literal Nome{}, new(Nome), leitura p^ e acesso a campo nao existem.", "foreign { opaque Handle  proc criar() -> ?^Handle  proc fechar(h: ?^Handle) }  -  let h = criar()"},
    {"R0001", "runtime", "Um valor saiu da faixa declarada ('in a..=b' do campo ou da constante); a checagem so existe em dev e test.", "Corrija a conta ou o valor de entrada para ficar dentro da faixa declarada."},
    {"R0002", "runtime", "Uma conta de inteiro estourou o tipo (soma, subtracao, multiplicacao, negacao, MIN / -1) ou uma conversao 'as' recebeu um valor que nao cabe no destino (inclusive real NaN ou infinito), em dev e test. Em release a conta da a volta e a conversao de real satura.", "Use um tipo maior, limite os valores antes, ou mascare antes do 'as' ((x & 255) as u8) quando quiser truncar."},
    {"R0003", "runtime", "Um panic ou assert do programa falhou; fora de test o processo sai com 101.", "Corrija a condicao que falhou ou trate o caso antes de chamar panic."},
    {"R0004", "runtime", "O proc main devolveu um erro ('main -> ! E'); o programa sai com 1.", "Trate o erro dentro do main (switch ou or_else) se ele nao deve terminar o programa."},
    {"R0005", "runtime", "Um ponteiro nulo chegou onde a Forja exige '^T' ou cstr: 'as ^T' de none/rawptr nulo, 'as str' de cstr nulo, ou parametro '^T'/cstr de proc exportado chamado com NULL, ou um proc foreign com retorno '^T' ou cstr devolveu NULL. Checado em dev e test e, em release, com --checagem sim (--checagem nao tira a checagem). Tambem: parametro ^T/cstr de um proc da Forja chamado pelo C como callback recebeu NULL; chamada por proc nulo (campo proc de uma struct que o C deixou sem valor, 'as proc(...)' de rawptr ou ?^T nulo).", "Teste 'p != none' antes do 'as ^T'; num proc exportado que aceita NULL declare o parametro como '?^T' ou rawptr; proc foreign que pode devolver NULL: declare o retorno como ?^T ou rawptr e teste 'p != none' antes do 'as'."},
    {"R0006", "runtime", "Em execucao um indice saiu de 0..<len ou um limite de fatia saiu de 0..=len (ou o inicio passou do fim); o programa sai com 101 (tambem dentro de uma DLL). Vale em dev e test e, em release, com --checagem sim; com --checagem nao o acesso fora e comportamento indefinido do C.", "if i < len(a) { usar(a[i]) }  -  'for i, x in a' nunca sai dos limites"},
    {"R0007", "memoria", "Uma alocacao nao pode ser feita: sem memoria no heap, frame cheio (16 MiB; chame frame_reset() a cada quadro), cap negativo em make, append numa lista sem alocador (valor zero de campo omitido) ou append numa [dynamic, N]T cheia. Conferido em dev e em release; sai com 101 (tambem dentro de uma DLL).", "frame_reset() no fim de cada quadro  -  if n >= 0 { var l = make([dynamic]int, alloc = heap, cap = n) }  -  S{lista = make([dynamic]int, alloc = heap)}"},
    {"R0008", "memoria", "Em dev/test: delete de um ponteiro que nao veio de new(alloc = heap) ou que ja passou por delete (delete(p) zera o nome p, entao delete duplo pelo mesmo nome nao acontece: o R0008 vem de ponteiro guardado em estrutura ou liberado dentro de outro proc), ou uma lista criada no frame usada depois do frame_reset(). Em release nao ha checagem (comportamento indefinido do C). Memoria liberada fica envenenada (0xDD) em dev. A linha do x64 dev traz o campo bloco (heap, lista ou arena) e 3 pilhas: pilha (onde o uso falhou), pilha_alocacao (onde o bloco nasceu) e pilha_delete (onde morreu: delete, append que moveu a lista ou arena_destroy); em dev o append de lista de heap sempre move o buffer.", "delete uma vez so, de quem veio de new  -  lista de frame so dentro do quadro; para atravessar quadros use alloc = heap"},
    {"R0009", "memoria", "Em dev/test: no fim do programa ainda havia blocos de heap vivos (new, make, format ou format_c com alloc = heap sem delete). Cada linha diz onde o bloco foi alocado. Com \"tipo\": \"lib\" (dev, x64) a linha conta a memoria que uma biblioteca C estatica alocou e nao soltou. Em release nao ha checagem.", "delete(x) quando o valor deixa de ser usado  -  defer delete(x) logo depois do let  -  alloc = scope para valor que nao sai do bloco"},
    {"R0010", "runtime", "Uma excecao C++ escapou de uma funcao chamada pela Forja pelo wrapper C que 'forja ligar --cpp' gera. O wrapper a captura (excecao nao atravessa a fronteira C), escreve este JSON com Classe::metodo e o texto de what() e encerra o programa com 101. Vale em dev e release.", "Confira os argumentos antes da chamada (o JSON diz qual metodo falhou) ou trate o erro dentro do C++ e devolva um codigo (i32) que a Forja testa."},
    {"B0001", "build", "Codigo historico e reservado desde a Forja 0.21.0: o backend C foi aposentado, e o forja nao chama mais o compilador C (gcc) para o programa do usuario. O codigo fica no catalogo para a cobertura de docs.", "Nenhuma acao: use o backend x64 (o padrao) e nao precisa de gcc nem de FORJA_CC."},
    {"B0002", "build", "O gcc recusou o C gerado; o defeito esta no compilador ou no IR, nao no codigo .fj.", "Leia o erro do gcc acima; o .c mantido em %TEMP% serve para inspecao."},
    {"B0003", "build", "O build --modo dll precisa de pelo menos um 'pub export proc': sem ele a DLL nao teria nenhum simbolo para chamar.", "Marque o proc que PHP ou Python vao chamar com pub export proc <nome>(...)."},
    {"B0004", "build", "A ligacao com as bibliotecas de foreign falhou: o ld nao achou uma biblioteca de 'link' ou um proc foreign nao existe nela com esse nome (a mensagem do ld vai em ferramenta.saida deste JSON). O C gerado esta certo; o defeito esta no bloco foreign ou na pasta.", "Confira cada 'link \"x\"' (libx.a, libx.dll.a ou x.dll na pasta do .fj) e se cada proc tem o nome exato do C, com a mesma caixa; o exe que usa .dll precisa da .dll ao lado dele para rodar."},
    {"G0001", "ligar", "Tipo C sem forma na Forja: long double, __int128, _Complex, vetor SIMD, _Atomic ou array sem tamanho fora de struct. O simbolo que o usa fica fora do modulo gerado.", "Escreva um wrapper C com tipos simples (double, int64_t) e gere o modulo dele."},
    {"G0003", "ligar", "Ponteiro de funcao fora de parametro de funcao (campo de struct, retorno, variavel global ou ponteiro para ponteiro de funcao): na Forja o tipo proc(...) so existe em parametro de proc foreign.", "Guarde o ponteiro de funcao no lado C (wrapper que recebe o callback por parametro) ou use rawptr no campo."},
    {"G0004", "ligar", "Macro que nao vira const: macro com parametros (macro-funcao) ou cujo valor nao e um literal inteiro, real ou texto simples.", "Escreva a const a mao no .fj (pub const NOME: i32 = ...) ou um proc Forja que faz a conta da macro."},
    {"G0005", "ligar", "Simbolo sem ligacao externa: variavel global (extern int x) ou funcao static/inline do header; foreign so liga funcoes exportadas.", "Exponha a variavel por funcoes get/set num wrapper C; funcao inline: escreva o equivalente em Forja ou um wrapper C nao inline."},
    {"G0006", "ligar", "Struct com bitfield (int x : 3): a Forja nao tem campo de bits e o layout dependeria do compilador. A struct e os simbolos que a usam ficam fora.", "Passe a struct por ponteiro opaco e leia os campos por funcoes C."},
    {"G0007", "ligar", "Struct com layout diferente do natural (#pragma pack, packed ou aligned): a Forja so gera o layout natural do C.", "Use a struct por ponteiro (opaque) e acesse por funcoes C."},
    {"G0008", "ligar", "Funcao variadica (...) ou com va_list: no Windows x64 o C le os reais variadicos em outro registrador e a Forja nao gera essa chamada.", "Escreva uma funcao C de parametros fixos que chama a variadica (ex.: log_texto(const char *s) chamando log(\"%s\", s))."},
    {"G0013", "ligar", "Nome que nao vira simbolo Forja: palavra reservada da Forja, prefixo fj_ ou forja_, 'main', ou dois simbolos C que viram o mesmo nome na Forja.", "Use --prefixo para filtrar ou declare o proc a mao com alias do simbolo C: proc c_remove = \"remove\"(caminho: cstr) -> i32 (o forja ligar gera o alias sozinho com prefixo)."},
    {"G0014", "ligar", "Struct ou union anonima dentro de struct, ou membro flexivel (array sem tamanho no fim da struct): sem nome nem tamanho fixo na Forja.", "De nome ao tipo de dentro no header (typedef) ou use a struct por ponteiro."},
    {"G0016", "ligar", "Enum com valor fora de i32 ou membro que repete o valor de outro (apelido): o enum da Forja vira int no C e cada membro tem valor proprio.", "O apelido sai do modulo (use o membro original); valor fora de i32: declare a const a mao com i64 ou u32."},
    {"G0017", "ligar", "Tipo incompleto (struct so declarada no header) usado por valor: sem o layout nao da para passar por valor.", "Passe por ponteiro: struct incompleta atras de ponteiro vira opaque."},
    {"G0019", "ligar", "O simbolo usa um tipo que foi recusado antes (veja o JSON anterior com o codigo do tipo).", "Resolva a recusa do tipo (wrapper C) e gere de novo; o simbolo volta sozinho."},
    {"G0021", "ligar", "C++: funcao ou metodo sobrecarregado (mesmo nome, parametros diferentes); extern \"C\" nao tem sobrecarga.", "Declare no header uma funcao C++ de nome unico que chama a sobrecarga desejada."},
    {"G0022", "ligar", "C++: template (classe ou funcao); so instancias concretas atravessam.", "Escreva uma funcao ou classe concreta (sem template) que usa a instancia desejada."},
    {"G0023", "ligar", "C++: construcao fora do minimo do wrapper: operador, metodo estatico ou virtual, referencia (&), heranca, campo publico, mais de um construtor, classe aninhada ou tipo que nao e C (std::string, std::vector).", "Exponha funcoes livres e classes com metodos de tipos C (int32_t, double, const char *, ponteiro)."},
    {"G0025", "ligar", "Arquivo de assinaturas (--dll): o nome da linha nao esta na tabela de exportacao da DLL.", "Confira a grafia (a caixa vale) com 'forja ligar --dll x.dll --listar'."},
    {"G0026", "ligar", "Arquivo de assinaturas (--dll): a linha nao e 'proc nome(...) [-> T]', 'opaque Nome', comentario '//' nem linha vazia.", "opaque Handle  -  proc criar(n: i32) -> ?^Handle  (um item por linha, sintaxe do foreign)"},
    {"G0101", "ligar", "forja ligar roda um script Python embutido no forja.exe e o Python nao foi encontrado.", "Instale o Python 3.10+ (python no PATH) ou aponte FORJA_PYTHON para o python.exe."},
    {"G0102", "ligar", "O modulo Python clang.cindex (pacote pip libclang) nao foi encontrado: forja ligar le o header pelo clang.", "python -m pip install --user libclang"},
    {"G0103", "ligar", "O clang nao conseguiu ler o header (arquivo nao encontrado ou erro de compilacao); a causa traz a 1a mensagem do clang.", "Confira o caminho, passe -I <pasta> para os includes e -D NOME para as macros que o header exige."},
    {"G0104", "ligar", "Argumentos invalidos para forja ligar.", "forja ligar <header.h> --link <lib> [-o saida.fj] [--prefixo p] [-I pasta] [-D macro] [--estrito]  -  forja ligar --cpp <x.hpp> --link <lib> -o saida.fj  -  forja ligar --dll <x.dll> --assinaturas <arq> [-o saida.fj]  -  forja ligar --dll <x.dll> --listar"},
    {"G0105", "ligar", "Modo --dll: o modulo Python pefile nao foi encontrado, ou o arquivo nao e uma DLL PE valida.", "python -m pip install --user pefile; confira o caminho da DLL."},
    {"X0001", "backend", "Recurso da IR ainda nao suportado no backend nativo: op, op_aux e tipo dizem o que falta, e a linha do .fj mostra a construcao.", "Nao ha backend alternativo (o backend C foi aposentado na 0.21.0): reduza o trecho do .fj que usa o recurso ou envie o caso a quem mantem a Forja."},
    {"X0002", "backend", "Alvo desconhecido em --target. Os alvos validos sao x86_64-windows e x86_64-linux.", "Use --target x86_64-windows ou --target x86_64-linux."},
    {"X0003", "backend", "Combinacao ou opcao de backend ou de alvo ainda nao suportada nesta versao (ex.: --modo dll ou run no linux, std:thread/std:sync/std:atomic/std:net no linux, lib .so no linux, TLS, o escritor ELF), ou opcao aposentada na 0.21.0 (--backend c, --modo c, --prova-layout); a causa diz qual.", "Remova --backend c / --modo c / --prova-layout (o padrao e x64); no alvo x86_64-linux use build (ou test com -o) com --backend x64 e sem --modo dll; foreign so com lib estatica .a em vendor\\linux; no Windows, --target x86_64-windows."},
    {"X0302", "backend", "Nao consegui gravar o executavel de saida: arquivo em uso (o programa ainda roda) ou pasta sem escrita.", "Feche o programa ou escolha outro -o."},
    {"X0801", "diferencial", "Saida diferente entre o exe e a referencia (oraculo congelado ou build sem plantio) (stdout ou stderr): 'byte' e o offset do 1o byte divergente, 'esperado' e 'obtido' o trecho de cada lado.", "Rode o verificar_nativo com o traco (nivel 2) para achar a instrucao."},
    {"X0802", "diferencial", "Valor divergente no traco do exe x traco da referencia: a primeira gravacao de local ou temporario em que os dois discordam (funcao, IrInstr, linha e coluna do .fj; esperado = referencia, obtido = exe).", "O erro e do backend nativo na instrucao apontada; envie a pasta do relato."},
    {"X0803", "diferencial", "Codigo de saida diferente entre o exe e a referencia (oraculo congelado ou build sem plantio) (inclui queda do processo, ex.: 0xC0000005 ou 0xC0000094).", "Rode o verificar_nativo com o traco (nivel 2) para achar a instrucao."},
    {"X0901", "interno", "Erro interno do backend nativo: a selecao de instrucoes nao tem regra para uma operacao da BIR.", "Ajuste o programa: este recurso nao e aceito pelo backend x64 nesta versao; veja `forja explicar CODIGO`. Envie a pasta do relato."},
    {"X0902", "interno", "Erro interno do backend nativo: o codificador recusou uma instrucao de maquina.", "Ajuste o programa: este recurso nao e aceito pelo backend x64 nesta versao; veja `forja explicar CODIGO`. Envie a pasta do relato."},
    {"X0903", "interno", "Erro interno do backend nativo: simbolo ou relocacao sem definicao, ou formato COFF inesperado.", "Ajuste o programa: este recurso nao e aceito pelo backend x64 nesta versao; veja `forja explicar CODIGO`. Envie a pasta do relato."},
    {"X0904", "interno", "Erro interno do backend nativo: o validador da BIR ou das instrucoes de maquina achou uma forma invalida.", "Ajuste o programa: este recurso nao e aceito pelo backend x64 nesta versao; veja `forja explicar CODIGO`. Envie a pasta do relato."},
    {"X0905", "interno", "Erro interno do backend nativo: a autoconferencia do executavel (PE ou ELF) achou cabecalho, secao ou diretorio inconsistente; o arquivo nao foi aceito.", "Ajuste o programa: este recurso nao e aceito pelo backend x64 nesta versao; veja `forja explicar CODIGO`. Envie a pasta do relato."},
    {"X0906", "interno", "Erro interno do backend nativo: o layout calculado diverge do esperado (layout incoerente em algum tipo, ou o str do backend difere do str do runtime nativo).", "Ajuste o programa: este recurso nao e aceito pelo backend x64 nesta versao; veja `forja explicar CODIGO`. Envie a pasta do relato."},
    {"E0330", "tipo", "Um literal inteiro vira real sozinho no contexto real (let x: f32 = 1, x * 2, {1, 2, 3} em [3]f32, argumento, return, campo) quando o valor e exato no tipo: f32 guarda todo inteiro ate 16777216 (2^24) e f64 ate 9007199254740992 (2^53); acima disso so alguns valores existem.", "Escreva o real que voce quer (16777216.0) ou use f64; valor de execucao converte com <n> as f32."},
    {"E0328", "layout", "size_of(T) e align_of(T) dao o tamanho e o alinhamento em bytes (int constante) de T como o C o guarda; offset_of(Struct.campo) da o deslocamento do campo. Valem em expressao, const e static_assert; nao valem no tamanho de [N]T. Tipo opaco (so a biblioteca C conhece) e tipo proc nao tem tamanho.", "size_of(S)  -  align_of(f64)  -  offset_of(S.campo)  -  static_assert(size_of(S) == 32, msg = \"layout do shader\")"},
    {"E0329", "layout", "static_assert(cond, msg = \"...\") confere na compilacao uma condicao constante (literal, const, size_of, align_of, offset_of e contas entre eles); vale no topo do arquivo e dentro de proc. Falsa: a compilacao para com a msg na causa. Valor que so existe em execucao nao vale.", "static_assert(size_of(S) == 32, msg = \"S mudou: confira o shader\")  -  assert(x > 0) para valor de execucao"},
    {"E0707", "fluxo", "Uma var que nunca e alterada (atribuicao inteira ou de parte, '&', fatia, for &item, append) deve ser let: var avisa a quem le que o valor muda. Em 'var a, b = f()' a palavra var vale pela declaracao inteira: so e erro se nenhum dos nomes muda.", "let g = S{}  (troque var por let)  -  var n = 0  n += 1"},
    {"E0708", "fluxo", "let/var que nunca e lido e codigo morto ou erro de digitacao (o valor foi parar em outro nome). Vale fora de test; parametro, variavel de for e ligacao de switch nao entram; um nome com sublinhado na frente (_r, _unused) diz que o nao-uso e de proposito e fica isento.", "remova o let  -  let q, _ = dividir(7, b = 2)  -  _ = registrar(x) para descartar um retorno  -  let _r = f() para guardar sem ler"},
    {"E0709", "fluxo", "O valor devolvido por um proc (da Forja ou foreign) nao some em silencio: guarde, use, ou descarte de proposito com '_ = f()'. '_ =' so aceita chamada que devolve valor; 'T ! E' continua pedindo tratamento (E0601): descarte o payload repassando o erro com '_ = try f()'.", "let n = contar(l)  -  _ = puts(\"oi\")  -  _ = try ler(f)  -  if salvar(x) != 0 { ... }"},
    {"E1005", "modulo", "Import cujo apelido nunca aparece (apelido.nome) so atrasa o build e confunde quem le.", "remova a linha do import  -  use apelido.nome"},
    {"E0331", "array", "[N]T de numero (inteiro, real ou unidade) faz conta elemento a elemento: a + b, a - b, a * b, a / b (mesmo N e T; * e / sao por elemento, nao produto escalar), a * k, k * a e a / k com k do tipo T, -a e as formas += e -=. Cada elemento segue a regra do escalar (estouro trava em dev, divisao inteira por 0 da 0, unidade confere). Nao existem: N diferentes, bool/str/struct, ==, ordem, conta em fatia ou lista.", "pos += vel * dt  -  let m: [3]f32 = (a + b) / 2.0  -  for i, x in a { ... } para o resto"},
    {"E0332", "array", "Em [2]T, [3]T e [4]T as letras x y z w (ou r g b a) sao os elementos 0..3: v.x e v[0]; v.xy, v.zyx, c.bgra montam um array novo [k]T (1 a 4 letras do mesmo grupo, cada uma menor que N). Uma letra so tambem grava (v.x = 1.0) e aceita &v.x. Gravar varias letras (v.xy = ...) nao existe; em [5]T ou mais use o indice.", "let p = v.xy  -  c.a = 1.0  -  let bgr = cor.bgr  -  v.y = v.y - 9.8 * dt"},
    {"E0334", "tipo", "min(a, b = c) e max(a, b = c) devolvem o menor e o maior de dois valores do MESMO tipo numerico (inteiro, real, unidade; [N]T por elemento); com NaN devolvem a. clamp(x, lo = a, hi = b) prende x em [a, b] (lo > hi constante e erro). abs(x): inteiro com sinal (abs do minimo trava em dev), real (zera o bit de sinal), quat (norma); sem sinal nao tem abs. sqrt(x): f32 ou f64 sem unidade (negativo da nan). wrapping_, saturating_, checked_ e overflowing_ de add, sub e mul pedem dois inteiros do MESMO tipo; rotate_left/right, count_ones, leading_zeros, trailing_zeros e byte_swap pedem inteiro; mul_hi_u64 pede dois u64. math.<fn> (std:math, S3 P3): os argumentos reais sao todos f32 ou todos f64 (um literal sozinho vale f64 e se adapta ao outro); misturar f32 e f64, passar inteiro, bool, texto ou unidade = E0334 (math.sqrt nao existe: sqrt e embutido).", "let v = clamp(vida, lo = 0, hi = 100)  -  let m = min(a, b = 3.5)  -  let d = sqrt(x * x + y * y)  -  let p = math.pow(x32, y = 2.0)"},
    {"E0333", "quat", "quat e o quaternion embutido de f32 (campos x, y, z, w; 16 bytes, mesma ordem do Quaternion da raylib). Literal quat{x = .., y = .., z = .., w = ..} (omitido = 0.0; identidade = quat{w = 1.0}). Contas: q + r, q - r, -q, q * r (Hamilton), q * k e k * q com k: f32, q * v com v: [3]f32 (gira o vetor; q unitario), conj(q) e abs(q) (norma). Nao existem: q / r, ==, ordem, quat de f64, unidade.", "let giro = quat{y = 0.70710677, w = 0.70710677}  -  let v2 = giro * v  -  let u = q * (1.0 / abs(q))"},
    {"E0335", "array", "[dynamic, N]T e uma lista de capacidade fixa N dentro do proprio valor (sem alocador, sem delete): e VALOR como [N]T (copia ao atribuir, passar e devolver). Tem len, l[i] (fora de 0..<len = R0006), fatia l[..], for e append(&l, item = x); append com len == N e R0007 em dev e release. Nasce de {} (vazia); l = {} esvazia. N literal ou const de 1 a 65536. Nao existem: make, delete, cap e literal com elementos.", "var lista: [dynamic, 64]T = {}  -  append(&lista, item = valor)  -  if len(lista) < 64 { ... }"},
    {"R0011", "runtime", "Em test (dev): o teste causou uma excecao do processador (acesso a memoria invalida ou instrucao ilegal); o executor marca o teste como falho, diz o titulo e segue para o proximo. Fora de test o processo cai como antes.", "Confira ponteiros vindos de foreign e rawptr convertido com 'as ^T' no teste apontado."},
    {"X0105", "backend", "A biblioteca achada para um 'link' nao e do tipo que o backend nativo liga: DLL, executavel ou import library de 32 bits (PE32, i386), PE ou ar malformado, objeto /GL, simbolo de import library que e dado ou so por ordinal, ou executavel sem nenhum simbolo exportado.", "Use a versao de 64 bits (x64) da biblioteca ou importe so funcoes por nome."},
    {"X0101", "backend", "Um proc foreign chama um simbolo que nenhuma biblioteca do 'link' exporta: procurei_em lista as DLLs lidas, na ordem dos 'link'; proc e simbolo dizem quem falhou (o simbolo e o nome do proc, ou o alias depois do '=').", "Corrija o nome do proc (a caixa conta) ou o alias, confira o 'link' e a pasta da biblioteca."},
    {"X0102", "backend", "O backend nativo nao achou a biblioteca de um 'link': procurei_em lista os candidatos tentados em cada pasta (a do .fj, vendor\\ e %FORJA_ENGINE%\\lib quando ha vendor:/engine:) e no System32.", "Confira o nome do 'link' (lib<x>.dll.a, <x>.dll.a, lib<x>.a, <x>.lib, lib<x>.lib, lib<x>.dll ou <x>.dll) e ponha a biblioteca na pasta do .fj ou em vendor\\."},
    {"X0103", "backend", "A biblioteca achada para um 'link' tem um formato que o backend nativo ainda nao liga: DLL, executavel e import library (MSVC .lib curta, GNU .dll.a) ligam; .a e .lib estaticas chegam no M3b.", "Aponte o 'link' para a DLL ou para a import library (MSVC .lib curta, GNU .dll.a)."},
    {"E0909", "foreign", "A assinatura de um tipo proc(nome: T, ...) -> R usado como valor (campo, let/var, parametro, retorno, 'as proc', nome de proc da Forja passado como valor) e um ponteiro de funcao do C: so atravessam tipos de forma fixa em C (inteiros, f32, f64, bool, enum, ^T, ?^T, rawptr, cstr, struct por valor cujos campos atravessam). str, fatia, array, lista, ?T, 'T ! E' e retorno multiplo ficam de fora. A causa diz qual parametro (ou o retorno) e por que.", "proc(self: ^Objeto, n: i32) -> u32  -  troque str por cstr, fatia por ^T e n: u64"},
    {"E0910", "foreign", "'x as proc(...) -> R' so converte de rawptr, ^T ou ?^T (um endereco de funcao que veio do C, como o de GetProcAddress) ou de um proc com a mesma assinatura. Numero, texto, struct e outro tipo de proc nao viram proc: um endereco de funcao nao se inventa. Em dev, um rawptr/?^T NULL vira R0005 na conversao.", "let f = GetProcAddress(modulo, \"MessageBoxA\") as proc(janela: rawptr, texto: cstr, titulo: cstr, tipo: u32) -> i32"},
    {"E0911", "argumento","Um tipo proc nao tem valor zero (nao existe ponteiro de funcao nulo na Forja): uma struct com campo proc sem padrao precisa do campo em todo literal, e nao pode nascer de '{}', new/make nem de [N]S zerado. De o proc em todo literal que cria a struct; nesta versao o campo proc nao tem padrao.","WndClass{ procedimento = janela_proc, instancia = h }"},
    {"B0005", "build", "O forja.proj (manifesto do projeto) tem uma declaracao invalida; a causa diz o arquivo, a linha e o motivo: nome de produto repetido ou fora de [a-z0-9_]+, tipo diferente de exe e dll, arquivo de entrada que nao existe, 'depende' de produto que nao existe, 'saida' declarada duas vezes, nenhum produto, ou uma linha que nao e 'saida = <pasta>' nem 'produto'.", "saida = bin  -  produto motor dll src/motor.fj  -  produto jogo exe src/jogo.fj depende motor"},
    {"B0007", "build", "Os produtos do forja.proj dependem uns dos outros em ciclo: a causa lista o ciclo na ordem (a -> b -> a). Sem uma ordem de build nenhum deles pode ser compilado primeiro.", "Tire uma das dependencias do ciclo (a clausula 'depende' de um dos produtos listados) ou junte os produtos em um so."},
    {"B0006", "build", "O build de um produto ligou a saida de outro produto do forja.proj (uma DLL ou exe em <saida>) sem declarar 'depende': e uma corrida de build, porque nada garante que a saida do outro ja esteja pronta. A causa diz o produto, o arquivo e o produto dono dele.", "Acrescente o produto dono na clausula 'depende' do produto (produto jogo exe src/jogo.fj depende motor)."},
    {"B0008", "build", "Outro processo do forja ja esta construindo este projeto: a trava <pasta>\\.forja_cache\\trava esta aberta por ele (a trava some sozinha se o processo morrer). Dois orquestradores no mesmo projeto pisariam no estado e nas saidas um do outro.", "Espere o outro build terminar e rode de novo."},
    {"X0106", "backend", "Um simbolo que o programa exige e definido por membros de DUAS bibliotecas estaticas (.a ou .lib) diferentes dos 'link': a causa lista o simbolo, as duas bibliotecas e o membro de cada uma. O ligador nativo nao escolhe uma em silencio, porque os dois codigos podem divergir.", "Tire um dos 'link' (ou a biblioteca que repete o simbolo), ou ligue so a que traz a versao certa."},
    {"X0108", "backend", "Um membro de biblioteca estatica de terceiros pede, pelo .drectve (/DEFAULTLIB ou -l), outra biblioteca que o ligador nativo nao encontrou nas pastas de busca: a causa diz a biblioteca pedida e o membro.", "Ponha a biblioteca pedida numa das pastas de busca (ou em --lib-pasta), ou ligue com a toolchain que a lib usa."},
    {"X0107", "backend", "Um membro de biblioteca estatica de terceiros tem a secao .tls (dado thread_local do C: __thread, _Thread_local, __declspec(thread)): o ligador nativo ainda nao monta o diretorio TLS do PE, e sem ele cada thread leria o mesmo dado. A causa diz o membro e a secao.", "Ligue a biblioteca na DLL dela (o 'link' sem .a), ou use uma versao da biblioteca compilada sem armazenamento por thread."},
    {"X0109", "backend", "O programa pede mais DLLs com import (no maximo 17 no .idata, contando a KERNEL32 e a ADVAPI32 do runtime) ou mais nomes importados de DLL (no maximo 320 no total) do que o ligador nativo aceita. A causa diz quantos o programa pede e o teto.", "Tire 'link' ou procs de foreign que o programa nao usa."},
    {"R0012", "runtime", "exit(code) de std:os recebeu um codigo fora de 0..255: o sistema guarda so 8 bits (o Windows aceita mais, mas o Linux trunca), entao a Forja recusa em vez de truncar em silencio. O processo sai com 101.", "Passe um codigo de 0 a 255: exit(0) para sucesso, exit(1)..exit(255) para falha."},
    {"E1006", "modulo", "O ultimo segmento do caminho do import e palavra reservada da linguagem (proc, let, if...): sem 'as', ele viraria o apelido do modulo (apelido.nome), e uma palavra reservada nao pode ser apelido.", "import \"x/proc\" as processo  -  escreva 'as <nome>' com um nome que nao seja palavra reservada."},
    {"R0013", "runtime", "O programa que o forja run ou o forja test executa passou do limite de tempo de parede dado em --limite-tempo <seg>: o forja encerrou o processo e todos os filhos dele e saiu com 102. A saida que o programa ja tinha despejado ficou; o que estava no buffer dele se perde.", "Corrija o laco que nao termina, ou suba o limite (--limite-tempo 30); programa que espera entrada ou outro processo precisa receber o que espera."},
    {"R0014", "memoria", "O programa que o forja run ou o forja test executa passou do limite de memoria dado em --limite-mem <MiB> (memoria confirmada do processo e dos filhos): o forja encerrou a arvore de processos e saiu com 103. A saida que o programa ja tinha despejado ficou; o que estava no buffer dele se perde.", "Libere o que nao usa mais (delete) ou processe em partes; se o uso e legitimo, suba o limite (--limite-mem 512)."},
    {"R0015", "runtime", "O programa que o forja run ou o forja test executa escreveu em stdout e stderr mais bytes que o limite de --limite-saida <bytes|KiB|MiB>: a saida foi cortada nesse limite (o que saiu antes ficou), o resto foi descartado, a causa diz quantos bytes, e o forja encerrou a arvore de processos e saiu com 104.", "Pare o laco que imprime sem fim ou imprima menos; se a saida e legitima, suba o limite (--limite-saida 1MiB)."},
    {"E0110", "sintaxe", "O codigo esta escrito numa forma de outra linguagem (C, Java, JS, Python, Go, Rust...) que a Forja nao aceita nem tolera: ';', '&&', '||', '!x', 'i++', 'c ? a : b', 'x?', 'fn', ':=', 'elif', '/* */', 'try { } catch', bloco na mesma linha do '{', 'main' devolvendo inteiro. A causa diz a forma achada e a que a Forja usa no lugar; a forma nunca e aceita em silencio. Tambem e E0110 um NOME ANTIGO que a Forja renomeou para ingles (rotulo em = heap virou alloc = heap, o valor escopo virou scope, para = T virou to = T, std:arquivo virou std:file, vendor:banco virou vendor:db, abrir virou open, ErroBanco virou DbError...): a causa diz 'forma antiga: X agora e Y'.", "Troque pela forma da Forja que a causa e a sugestao indicam (forja corrigir --aplicar faz a troca quando ela e mecanica). Para um nome antigo renomeado rode forja migrar <arquivo.fj|pasta> --aplicar (sem --aplicar so mostra as trocas): ele troca no projeto todo, sem regex."},
    {"E0336", "memoria", "Allocator e o tipo embutido (sem import) do alocador que um proc do usuario recebe do chamador: proc nome(..., alloc: Allocator) -> T. So existe como PARAMETRO, chamado alloc, o ULTIMO da lista, uma vez so e sem valor padrao (o chamador sempre escolhe o alocador). Nao existe como let, var, campo de struct, retorno, ^Allocator, ?Allocator, elemento de array, lista ou map, nem em tipo proc(...); nao se escreve o literal Allocator{...} e o .id dele nao se le (o handle e do runtime). Em pub export e em foreign o tipo nao atravessa a fronteira C (E0901 e E0904).", "proc juntar(partes: []str, sep: str, alloc: Allocator) -> str  -  o parametro se chama alloc, vem por ultimo e nao tem valor padrao"},
    {"E0337", "ordenacao", "binary_search so busca em fatia de inteiro, real (f32, f64) ou str, porque compara o elemento inteiro com value; sort_by_key so ordena fatia de struct pela chave key = Struct.campo, com campo direto de inteiro, f32, f64 ou str. Enum, struct, bool, ponteiro e lista nao tem a ordem que o runtime usa. Campo aninhado e proc como chave nao valem: a chave e so um campo direto da struct do elemento.", "binary_search(ns, value = 3) com ns de enum  -  sort_by_key(ps, key = Pessoa.nome) com nome: str"},
    {"E0338", "colecoes", "shuffle e choice sorteiam com o gerador de std:rand e pedem rng = &r, onde r e um Rng de rand.seed (ponteiro ^rand.Rng de std:rand). O rng e obrigatorio: sem ele, passado por valor (rng = r, sem &) ou de outro modulo, o sorteio nao tem de onde tirar a sequencia, e o compilador nao inventa um gerador global.", "shuffle(l, rng = &r) com var r = rand.seed(42)  -  shuffle(l) sem rng  -  choice(l, rng = r) com r por valor"},
    {"F0001", "formato", "O arquivo passa no check mas nao esta na forma canonica da Forja (vaos entre tokens, indentacao, linha em branco, espaco no fim da linha, BOM ou CRLF). O forja fmt troca so os vaos, a indentacao e os fins de linha; token e quebra de linha ficam como estao.", "rode forja fmt --aplicar <arq>"},
    {"E1007", "arquivo", "O #load(\"arquivo\") nao consegue ler o arquivo: ele nao existe ou nao abre na pasta do arquivo raiz, ou passa do limite de 16 MiB (o conteudo vira um str do programa). O caminho e relativo a pasta do .fj raiz; '/' funciona no Windows; caminho com unidade (C:...) nao vale, pois nao junta com a pasta raiz.", "use um caminho relativo a pasta do arquivo raiz (sem unidade C:), que exista, de ate 16 MiB: #load(\"dados/tabela.bin\")"},
    {"E0339", "memoria", "Mutex, Cond e os Atomic* de std:sync e std:atomic guardam um estado que nao se copia: copiar um lvalor, passar por valor, devolver ou por em union, map ou [dynamic] criaria dois donos do mesmo lock ou contador. Como campo de struct pode (a struct herda a marca) e o valor zero e valido.", "passe ^Mutex (o endereco): use por & ou pelo campo; nunca copie"},
    {"X0110", "backend", "Um membro ELF de biblioteca estatica tem relocacao de um tipo fora do censo que o ligador nativo trata (o do libc.a do musl: R_X86_64_64, PC32, PLT32, 32, 32S, GOTPCREL, GOTPCRELX, REX_GOTPCRELX, TPOFF32, GOTTPOFF). A causa diz o tipo e o membro.", "Use uma biblioteca estatica cujas relocacoes sejam do censo do libc.a; o ligador nativo ainda nao aplica o tipo que a causa diz."},
    {"X0111", "backend", "Um membro ELF de biblioteca estatica pede relocacao de TLS (TPOFF32 ou GOTTPOFF) de um simbolo que nao esta no bloco TLS (indefinido ou que nao e __thread): o ligador nao sabe o deslocamento dele. O diagnostico cita o simbolo.", "Confira o simbolo: a variavel tem de ser __thread (ou _Thread_local) e estar definida no objeto ou na biblioteca linkada."},
    {"X0112", "backend", "Com --pie (ELF static-pie) o ligador nao aceita relocacao absoluta de 32 bits (ABS32 ou ABS32S: o campo nao recebe a base do carregador) nem ADDR64 dentro de .text (TEXTREL). O diagnostico cita o simbolo.", "Compile o objeto ou a biblioteca com -fPIC (sem enderecos absolutos de 32 bits) ou tire --pie: o ELF sem --pie segue como era."},
    {"X0113", "backend", "Um membro ELF de biblioteca estatica tem TLS (.tdata ou .tbss: variavel __thread) e a ligacao e crua (sem a libc musl): o bloco TLS so e montado com a semente musl, que o PT_TLS pede ao carregador. O diagnostico cita a secao e o objeto.", "Ligue pela variante libc (foreign no alvo linux, ou std:thread/std:async que ja a puxam), ou tire a variavel __thread do membro."},
    {"X0114", "backend", "Relocacao GOTTPOFF (initial-exec de __thread) com uma forma de instrucao que o relaxamento para local-exec nao reconhece: so mov e add com [rip+d] (8b ou 03 com modrm 05) sem prefixo REX.R. O diagnostico cita o simbolo.", "Compile o objeto com -ftls-model=local-exec, ou com a forma padrao do gcc (mov/add [rip+d] sem REX.R)."},
    {"E0342", "foreign", "c_long e c_ulong sao o long do C: 32 bits no Windows e 64 bits no Linux. So valem na fronteira com o C: parametro e retorno de proc foreign, campo de struct (espelho do header), let/var e ^c_long ou ?^c_long (saida). Fora dai (proc da Forja, pub export, array, fatia, lista, map, union, ?T, const) o tamanho mudaria por alvo e o codigo so compilaria num deles. Um c_long le-se como operando de 'as', argumento de parametro c_long ou '&x' para ^c_long; recebe literal, 'x as c_long' ou outro c_long; conta ('+=', '+') e mistura com i32/int sao o erro.", "converta nas pontas: labs(x) as i32 para ler, e (n as int + 1) as c_long para gravar"},
    {"E0912", "foreign", "Depois da assinatura de um proc foreign so cabe uma clausula de posse do handle: creates (o retorno e o handle novo), creates p (o parametro de saida ^H recebe o handle), destroys p (o proc fecha o handle de p) takes p (o handle de p passa a pertencer a outro objeto da lib) ou ends (sem parametro: o proc encerra a lib inteira). No maximo uma por proc; p tem de ser parametro do proc e de tipo handle (^T, ?^T, rawptr, inteiro ou struct por valor); creates sem nome precisa de retorno de handle e creates p precisa de p do tipo ^H.", "proc fopen(caminho: cstr, modo: cstr) -> ?^File creates"},
    {"E0913", "memoria", "Um ponteiro para dentro de uma lista ou map (&l[i], l[..], get_ptr(&m, ...)) guarda o endereco do buffer; append, insert, remove, delete, clear e resize da MESMA colecao (pelo mesmo nome) podem mover ou liberar o buffer, e o ponteiro passa a apontar para memoria velha. Erro quando o ponteiro e lido depois da mutacao, quando a mutacao esta num laco e o ponteiro, declarado fora, e usado no corpo, e quando a colecao muda dentro de for &x in l. Nova atribuicao ao ponteiro (p = &l[i]) o revalida. So se confere o mesmo nome no mesmo proc; entre procs a guarda de memoria (--guarda-memoria) e o R0008 cobrem.", "Guarde o indice e leia l[i] depois da mutacao; ou pegue o ponteiro de novo logo depois dela (var p = &l[0] ... append(&l, item = 2); p = &l[0]); no for &x, junte os novos numa lista auxiliar e faca o append depois do for."},
    {"E1008", "modulo", "O import de uma pasta junta todos os .fj dela num modulo so (pacote); o caminho tem de designar uma coisa so. Erro quando o mesmo caminho e um arquivo (geo.fj) e uma pasta com .fj (geo\\), e quando o mesmo arquivo entraria duas vezes no programa (import \"geo\" junto de import \"geo/ponto\", se ponto.fj mora em geo\\).", "renomeie a pasta ou o arquivo para o caminho nao repetir; ou importe so a pasta (import \"geo\") ou so o arquivo (import \"geo/ponto\"), nao os dois"},
};

bool DiagRelatar(Diag *d, DiagCodigo codigo, uint32_t off, const char *fmt, ...)
{
    va_list ap;
    d->tem = true;
    d->calado = false;
    d->codigo = codigo;
    d->off = off;
    d->extras[0] = '\0';
    d->sug.n = 0;
    d->rel.n = 0;
    d->explicacao[0] = '\0';
    d->forma[0] = '\0';
    va_start(ap, fmt);
    vsnprintf(d->causa, sizeof d->causa, fmt, ap);
    va_end(ap);
    return false;
}

void DiagForma(Diag *d, const char *explicacao, const char *forma)
{
    if (explicacao != NULL && explicacao[0] != '\0') snprintf(d->explicacao, sizeof d->explicacao, "%s", explicacao);
    if (forma != NULL && forma[0] != '\0') snprintf(d->forma, sizeof d->forma, "%s", forma);
}

void DiagDescreverOnde(const Fonte *f, uint32_t off, char *saida, size_t cap)
{
    FontePosicao pos = FonteLocalizarNoArquivo(f, off);
    const char *base = pos.caminho;

    for (const char *c = pos.caminho; *c != '\0'; c++) {
        if (*c == '/' || *c == '\\') base = c + 1;
    }
    if (base[0] != '\0') snprintf(saida, cap, "linha %u, coluna %u de %s", pos.linha, pos.coluna, base);
    else snprintf(saida, cap, "linha %u, coluna %u", pos.linha, pos.coluna);
}

void DiagAcrescentarExtra(Diag *d, const char *chave, const char *valor_json)
{
    size_t usado = strlen(d->extras);
    int n = snprintf(d->extras + usado, sizeof d->extras - usado, "%s\"%s\": %s", usado == 0 ? "" : ", ", chave, valor_json);
    if (n < 0 || (size_t)n >= sizeof d->extras - usado) d->extras[usado] = '\0'; /* nao coube: nenhum extra a meio */
}

void ImprimirTextoJson(FILE *saida, const char *s)
{
    fputc('"', saida);
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            fputc('\\', saida);
            fputc(ch, saida);
        } else if (ch < 0x20) {
            fprintf(saida, "\\u%04x", ch);
        } else {
            fputc(ch, saida);
        }
    }
    fputc('"', saida);
}

static void ImprimirCaminhoJson(FILE *saida, const char *caminho)
{
    char normal[1024];
    snprintf(normal, sizeof normal, "%s", caminho);
    for (char *p = normal; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    ImprimirTextoJson(saida, normal);
}

/* Classes de caractere do token (mesmas do lexer, que e static e nao se reaproveita aqui). */
static bool EhInicioDeNomeDiag(char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'; }
static bool EhDigitoDiag(char ch) { return ch >= '0' && ch <= '9'; }
static bool EhParteDeNomeDiag(char ch) { return EhInicioDeNomeDiag(ch) || EhDigitoDiag(ch); }

/* Fim (exclusivo) do texto entre aspas que abre em j: a aspa de fechamento na mesma linha, ou o fim da linha se faltar. */
static uint32_t FimDoTextoDiag(const char *s, uint32_t j)
{
    uint32_t k = j + 1;
    while (s[k] != '\0' && s[k] != '"' && s[k] != '\n') {
        bool escape = s[k] == '\\' && s[k + 1] != '\0' && s[k + 1] != '\n';
        k += escape ? 2 : 1;
    }
    return s[k] == '"' ? k + 1 : k;
}

/* Fim (exclusivo) do numero que comeca em i: 0x e 0b leem o corte colado; senao digitos e uma parte fracionaria (1.5). */
static uint32_t FimDoNumeroDiag(const char *s, uint32_t i)
{
    uint32_t k = i;
    if (s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'b')) {
        while (EhParteDeNomeDiag(s[k])) k++;
        return k;
    }
    while (EhDigitoDiag(s[k])) k++;
    if (s[k] == '.' && EhDigitoDiag(s[k + 1])) {
        k++;
        while (EhDigitoDiag(s[k])) k++;
    }
    return k;
}

/* Operadores de 3 e 2 caracteres do lexer; qualquer outro caractere vale 1 (o coluna + 1 do contrato). */
static uint32_t FimDoOperadorDiag(const char *s, uint32_t i)
{
    static const char *const TRES[] = {"..<", "..="};
    static const char *const DOIS[] = {"==", "!=", "<=", ">=", "<<", ">>", "+=", "-=", "->", ".."};

    for (size_t k = 0; k < sizeof TRES / sizeof TRES[0]; k++) {
        if (strncmp(s + i, TRES[k], 3) == 0) return i + 3;
    }
    for (size_t k = 0; k < sizeof DOIS / sizeof DOIS[0]; k++) {
        if (strncmp(s + i, DOIS[k], 2) == 0) return i + 2;
    }
    return i + 1;
}

/* Fim (exclusivo) do token que comeca em i: nome, f"...", numero, texto ou operador. */
static uint32_t FimDoTokenDiag(const char *s, uint32_t i)
{
    if (s[i] == 'f' && s[i + 1] == '"') return FimDoTextoDiag(s, i + 1);
    if (EhInicioDeNomeDiag(s[i])) {
        uint32_t k = i + 1;
        while (EhParteDeNomeDiag(s[k])) k++;
        return k;
    }
    if (EhDigitoDiag(s[i])) return FimDoNumeroDiag(s, i);
    if (s[i] == '"') return FimDoTextoDiag(s, i);
    return FimDoOperadorDiag(s, i);
}

/* Bytes do token que comeca em off, para coluna_fim (coluna + bytes = 1o byte depois do token). Fora do texto vale 1. */
uint32_t DiagTamanhoDoToken(const Fonte *f, uint32_t off)
{
    if (off >= f->tam) return 1;
    return FimDoTokenDiag(f->texto, off) - off;
}

/* Uma linha: os campos de sempre e, se extras_json nao for vazio, os campos extras logo depois de forma_certa (v0.17.0).
 * coluna_fim so nos E-codes (L6), entre coluna e causa. */
static void EscreverCompleto(const Diag *d, const Fonte *f, const char *extras_json, FILE *saida)
{
    const ItemCatalogo *item = &CATALOGO[d->codigo];
    FontePosicao pos = FonteLocalizarNoArquivo(f, d->off);
    FormaTextos forma;
    DiagSugestoes corrigida;
    Diag copia;

    if (DiagCorrigirPadrao(d, f, &corrigida)) { /* P72: erro sem edicao ganha a correcao da tabela; o diagnostico original nao muda */
        copia = *d;
        copia.sug = corrigida;
        d = &copia;
    }
    DiagFormaDaOcorrencia(d, f, item->forma, &forma); /* b08: explicacao e forma_certa da ocorrencia, nunca o texto fixo do catalogo sozinho */
    fprintf(saida, "{\"codigo\": \"%s\", \"tipo\": \"%s\", \"arquivo\": ", item->codigo, item->tipo);
    ImprimirCaminhoJson(saida, pos.caminho);
    fprintf(saida, ", \"linha\": %u, \"coluna\": %u", pos.linha, pos.coluna);
    if (item->codigo[0] == 'E') fprintf(saida, ", \"coluna_fim\": %u", pos.coluna + DiagTamanhoDoToken(f, d->off));
    DiagPacoteTrecho(d, f, DiagTamanhoDoToken(f, d->off), saida); /* z5: o trecho sai em todo diagnostico, com ou sem sugestao */
    fputs(", \"causa\": ", saida);
    ImprimirTextoJson(saida, d->causa);
    fputs(", \"explicacao\": ", saida);
    ImprimirTextoJson(saida, forma.explicacao);
    fputs(", \"forma_certa\": ", saida);
    ImprimirTextoJson(saida, forma.forma);
    if (d->extras[0] != '\0') fprintf(saida, ", %s", d->extras);
    if (extras_json != NULL && extras_json[0] != '\0') fprintf(saida, ", %s", extras_json);
    if (d->sug.n > 0) {
        DiagPacoteFim(d, f, item->codigo, saida);
    } else {
        DiagPacoteSemSugestao(d, f, item->codigo, saida); /* z5: sem sugestao o pacote sai com correcao [] e confianca "so_dica" (b14 relacionados, b09 spec) */
    }
    fputs("}\n", saida);
}

/* --resumo (IA1-b02): ligado pelo main depois de validar as opcoes. Vale so para a saida padrao (stdout e stderr), nunca para um arquivo (o erro.json do relato). */
static bool ModoResumo;

void DiagModoResumo(bool ligado)
{
    ModoResumo = ligado;
}

/* FORJA_LOG_DIAG=<arq.jsonl>: abre o log em anexo. NULL (variavel ausente ou vazia, ou arquivo que nao abre) = nada e gravado. */
static FILE *AbrirLogDiag(void)
{
    const char *caminho = getenv("FORJA_LOG_DIAG");

    if (caminho == NULL || caminho[0] == '\0') return NULL;
    return fopen(caminho, "ab");
}

/* Aspa de fechamento do texto JSON que abre em p (p aponta a aspa de abertura); NULL se o texto nao fecha. */
static const char *FimDoTextoJson(const char *p)
{
    for (p++; *p != '\0' && *p != '"'; p++) {
        if (*p == '\\' && p[1] != '\0') p++;
    }
    return *p == '"' ? p : NULL;
}

/* Sugestao da propria ocorrencia: o extra "sugestao": "<texto>" do Diag (ex.: o 'n as f64' do E0301). Devolve o texto JSON com as aspas em [*inicio, *inicio + *tam);
 * false = o Diag nao tem (o extra de outra forma, como o objeto do X0001, nao conta). */
static bool SugestaoDosExtras(const Diag *d, const char **inicio, size_t *tam)
{
    const char *chave = strstr(d->extras, "\"sugestao\": \"");
    const char *fim;

    if (chave == NULL) return false;
    *inicio = chave + strlen("\"sugestao\": ");
    fim = FimDoTextoJson(*inicio);
    if (fim == NULL) return false;
    *tam = (size_t)(fim - *inicio) + 1;
    return true;
}

/* A forma_certa do catalogo, encurtada para o --resumo (M-D7: linha media <= 250 caracteres): vai ate o primeiro ponto final, ponto e virgula, parenteses ou "  -  " (o exemplo)
 * e, se ainda passar de RESUMO_SUGESTAO_MAX bytes, corta na ultima palavra inteira. O texto inteiro segue em "forja explicar <codigo>". */
enum { RESUMO_SUGESTAO_MAX = 90, RESUMO_SUGESTAO_MINIMO = 12 };

/* Primeiro corte natural da forma_certa (o mais cedo que deixe ao menos RESUMO_SUGESTAO_MINIMO bytes); sem corte, o texto inteiro. */
static size_t PrimeiroCorteDaSugestao(const char *forma)
{
    static const char *const CORTES[] = {"  -  ", ". ", "; ", " ("};
    size_t fim = strlen(forma);

    for (size_t k = 0; k < sizeof CORTES / sizeof CORTES[0]; k++) {
        const char *achou = strstr(forma, CORTES[k]);
        if (achou != NULL && (size_t)(achou - forma) >= RESUMO_SUGESTAO_MINIMO && (size_t)(achou - forma) < fim) fim = (size_t)(achou - forma);
    }
    return fim;
}

static size_t FimDaSugestaoCurta(const char *forma)
{
    size_t fim = PrimeiroCorteDaSugestao(forma);

    if (fim > RESUMO_SUGESTAO_MAX) {
        fim = RESUMO_SUGESTAO_MAX;
        while (fim > RESUMO_SUGESTAO_MINIMO && forma[fim] != ' ') fim--;
    }
    while (fim > 0 && strchr(".,;:", forma[fim - 1]) != NULL) fim--;
    return fim;
}

/* O campo "sugestao" do --resumo: a da propria ocorrencia (extra "sugestao" do Diag; d NULL = sem Diag) ou a forma_certa encurtada. */
static void EscreverSugestaoResumo(const Diag *d, const Fonte *f, const ItemCatalogo *item, FILE *saida)
{
    const char *propria;
    size_t tam;
    char curta[RESUMO_SUGESTAO_MAX + 1];
    FormaTextos ocorrencia;

    if (d != NULL && d->sug.n > 0) {
        ImprimirTextoJson(saida, d->sug.itens[0].descricao); /* IA1-b04: a descricao da sugestao principal */
        return;
    }
    if (d != NULL && SugestaoDosExtras(d, &propria, &tam)) {
        fwrite(propria, 1, tam, saida);
        return;
    }
    const char *forma = item->forma;

    if (d != NULL) {
        DiagFormaDaOcorrencia(d, f, item->forma, &ocorrencia); /* b07/b08: a forma da ocorrencia (do passo, da tabela ou a geral) */
        forma = ocorrencia.forma;
    }
    tam = FimDaSugestaoCurta(forma);
    memcpy(curta, forma, tam);
    curta[tam] = '\0';
    ImprimirTextoJson(saida, curta);
}

/* Uma linha do --resumo: so codigo, arquivo, linha, coluna, causa e sugestao; sem tipo, coluna_fim, explicacao nem outros extras. */
static void EscreverResumo(const Diag *d, const Fonte *f, FILE *saida)
{
    const ItemCatalogo *item = &CATALOGO[d->codigo];
    FontePosicao pos = FonteLocalizarNoArquivo(f, d->off);

    fprintf(saida, "{\"codigo\": \"%s\", \"arquivo\": ", item->codigo);
    ImprimirCaminhoJson(saida, pos.caminho);
    fprintf(saida, ", \"linha\": %u, \"coluna\": %u, \"causa\": ", pos.linha, pos.coluna);
    ImprimirTextoJson(saida, d->causa);
    fputs(", \"sugestao\": ", saida);
    EscreverSugestaoResumo(d, f, item, saida);
    fputs("}\n", saida);
}

/* A saida padrao (stdout ou stderr): so ela vira --resumo e vai para o FORJA_LOG_DIAG. */
static bool SaidaPadrao(FILE *saida)
{
    return saida == stdout || saida == stderr;
}

static void EscreverNaForma(const Diag *d, const Fonte *f, const char *extras_json, FILE *saida, bool resumo)
{
    if (resumo) EscreverResumo(d, f, saida);
    else EscreverCompleto(d, f, extras_json, saida);
}

/* Imprime na saida e, com FORJA_LOG_DIAG, anexa a MESMA linha ao log (so para stdout e stderr). */
static void ImprimirDiagnostico(const Diag *d, const Fonte *f, const char *extras_json, FILE *saida)
{
    bool padrao = SaidaPadrao(saida);
    bool resumo = padrao && ModoResumo;
    FILE *log = padrao ? AbrirLogDiag() : NULL;

    if (!resumo) DiagSpecIniciarLinha(d->codigo); /* o "spec" do codigo sai inteiro so na 1a vez da execucao, na tela e no log */
    EscreverNaForma(d, f, extras_json, saida, resumo);
    fflush(saida);
    if (log == NULL) return;
    EscreverNaForma(d, f, extras_json, log, resumo);
    fclose(log);
}

void DiagImprimirJson(const Diag *d, const Fonte *f, FILE *saida)
{
    ImprimirDiagnostico(d, f, NULL, saida);
}

void DiagImprimirJsonExtras(const Diag *d, const Fonte *f, const char *extras_json, FILE *saida)
{
    ImprimirDiagnostico(d, f, extras_json, saida);
}

bool DiagListaGuardar(DiagLista *l, Diag *d, uint32_t modulo)
{
    uint32_t teto = l->teto == 0 ? DIAG_TETO_PADRAO : l->teto;
    if (teto > DIAG_MAX) teto = DIAG_MAX;
    bool calado = d->calado;
    d->tem = false;
    d->calado = false;
    if (calado) return true; /* cascata de declaracao envenenada: o erro de origem ja esta (ou vai estar) na lista */
    if (l->n >= teto) {
        l->cortado = true;
        return false;
    }
    l->itens[l->n] = *d;
    l->modulo[l->n] = modulo;
    l->n++;
    return true;
}

/* true se o item a deve sair antes do item b: menor modulo; no mesmo modulo, menor off. */
static bool SaiAntes(const DiagLista *l, uint32_t a, uint32_t b)
{
    if (l->modulo[a] != l->modulo[b]) return l->modulo[a] < l->modulo[b];
    return l->itens[a].off < l->itens[b].off;
}

/* Insercao: so move o item para tras de quem sai estritamente depois dele, entao empate mantem a ordem de guarda (estavel). */
void DiagListaOrdenar(DiagLista *l)
{
    for (uint32_t i = 1; i < l->n; i++) {
        for (uint32_t k = i; k > 0 && SaiAntes(l, k, k - 1); k--) {
            Diag item = l->itens[k];
            uint32_t modulo = l->modulo[k];
            l->itens[k] = l->itens[k - 1];
            l->modulo[k] = l->modulo[k - 1];
            l->itens[k - 1] = item;
            l->modulo[k - 1] = modulo;
        }
    }
}

void DiagListaImprimir(const DiagLista *l, const Fonte *f, FILE *saida)
{
    for (uint32_t k = 0; k < l->n; k++) DiagImprimirJson(&l->itens[k], f, saida);
}

const char *DiagCodigoTexto(DiagCodigo codigo)
{
    return CATALOGO[codigo].codigo;
}

const char *DiagCodigoExplicacao(DiagCodigo codigo)
{
    return CATALOGO[codigo].explicacao;
}

const char *DiagCodigoForma(DiagCodigo codigo)
{
    return CATALOGO[codigo].forma;
}

int DiagCodigoPorTexto(const char *texto)
{
    for (int i = 0; i < DIAG_QTD; i++) {
        if (strcmp(CATALOGO[i].codigo, texto) == 0) return i;
    }
    return -1;
}

/* Campos tipo, explicacao e forma_certa do item, depois de "codigo" (que o chamador ja escreveu). */
static void ImprimirResto(FILE *saida, const ItemCatalogo *item)
{
    fputs(", \"tipo\": ", saida);
    ImprimirTextoJson(saida, item->tipo);
    fputs(", \"explicacao\": ", saida);
    ImprimirTextoJson(saida, item->explicacao);
    fputs(", \"forma_certa\": ", saida);
    ImprimirTextoJson(saida, item->forma);
}

void DiagExplicarJson(DiagCodigo codigo, FILE *saida)
{
    const ItemCatalogo *item = &CATALOGO[codigo];

    fputs("{\"codigo\": ", saida);
    ImprimirTextoJson(saida, item->codigo);
    ImprimirResto(saida, item);
    DiagSpecImprimirCompleto(codigo, saida);
    fputs("}\n", saida);
    fflush(saida);
}

/* O que um diagnostico sem linha do .fj leva: o codigo, a causa e, se uma ferramenta externa falhou, o nome dela e o que ela escreveu (ferramenta NULL = nenhuma). */
typedef struct {
    DiagCodigo codigo;
    const char *causa;
    const char *ferramenta;
    const char *saida_ferramenta;
} DadosSemFonte;

/* Sem linha do .fj: codigo e causa, depois tipo, explicacao, forma_certa e "ferramenta": {"nome","saida"} (b03: a saida do gcc/ld so existe dentro do JSON);
 * no --resumo, codigo, causa e sugestao (a forma_certa), sem a ferramenta. */
static void EscreverSemFonte(const DadosSemFonte *d, FILE *saida, bool resumo)
{
    const ItemCatalogo *item = &CATALOGO[d->codigo];

    fputs("{\"codigo\": ", saida);
    ImprimirTextoJson(saida, item->codigo);
    fputs(", \"causa\": ", saida);
    ImprimirTextoJson(saida, d->causa);
    if (resumo) {
        fputs(", \"sugestao\": ", saida);
        EscreverSugestaoResumo(NULL, NULL, item, saida);
    } else {
        ImprimirResto(saida, item);
        if (d->ferramenta != NULL) {
            fputs(", \"ferramenta\": {\"nome\": ", saida);
            ImprimirTextoJson(saida, d->ferramenta);
            fputs(", \"saida\": ", saida);
            ImprimirTextoJson(saida, d->saida_ferramenta != NULL ? d->saida_ferramenta : "");
            fputc('}', saida);
        }
        DiagSpecImprimir(d->codigo, saida);
    }
    fputs("}\n", saida);
}

static void ImprimirSemFonte(const DadosSemFonte *d, FILE *saida)
{
    bool padrao = SaidaPadrao(saida);
    bool resumo = padrao && ModoResumo;
    FILE *log = padrao ? AbrirLogDiag() : NULL;

    if (!resumo) DiagSpecIniciarLinha(d->codigo);
    EscreverSemFonte(d, saida, resumo);
    fflush(saida);
    if (log == NULL) return;
    EscreverSemFonte(d, log, resumo);
    fclose(log);
}

void DiagImprimirJsonSemFonte(DiagCodigo codigo, const char *causa, FILE *saida)
{
    DadosSemFonte d = {.codigo = codigo, .causa = causa};

    ImprimirSemFonte(&d, saida);
}

void DiagImprimirJsonFerramenta(DiagCodigo codigo, const char *causa, const char *ferramenta, const char *saida_ferramenta, FILE *saida)
{
    DadosSemFonte d = {.codigo = codigo, .causa = causa, .ferramenta = ferramenta, .saida_ferramenta = saida_ferramenta};

    ImprimirSemFonte(&d, saida);
}
