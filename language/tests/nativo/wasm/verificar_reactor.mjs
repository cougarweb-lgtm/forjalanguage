// verificar_reactor.mjs 1.1.0 - confere um .wasm WASI preview1 em modo reactor: sem _start, com procs export e memoria linear.
// Uso: node --no-warnings verificar_reactor.mjs <arquivo.wasm> [--struct]
//   sem flag: confere o reactor_basico (soma, mul, media, ler_i32, eco).
//   --struct: confere o reactor_struct (layout, struct plana por valor, sret, recursao, export que chama export).
// Saida: "reactor OK" (ou "reactor struct OK" com --struct) e exit 0; qualquer falha vai ao stderr como FALHA: <motivo> e sai 1.
// Verificacao: node --check tests/nativo/wasm/verificar_reactor.mjs (so sintaxe; a execucao depende do reactor do compilador).
// Versao: 1.1.0
// Changelog: 1.1.0 - caso --struct para reactor/reactor_struct.fj: tam/off conferidos com os exports, arity n+1 com sret, campo a campo via DataView little-endian, 2 chamadas reusando o bloco, sret igual ao parametro.
//            1.0.0 - primeira versao (soma, mul, media e somar_bytes; _start ausente; bytes escritos na memoria linear).

import { readFile } from 'node:fs/promises';
import { writeSync } from 'node:fs';
import path from 'node:path';
import { WASI } from 'node:wasi';

const CODIGO_OK = 0;
const CODIGO_FALHA = 1;
// 64 bytes no fim da memoria linear: area livre (depois do heap) onde o teste grava os bytes da soma.
const TAMANHO_AREA_TESTE = 64;
const VALOR_TESTE = 123456;

function escreverSaida(texto) {
  // writeSync: o process.exit logo depois nao pode cortar a linha num pipe assincrono.
  writeSync(1, texto + '\n');
}

function escreverErro(texto) {
  writeSync(2, texto + '\n');
}

function conferir(condicao, mensagem) {
  if (!condicao) throw new Error(mensagem);
}

async function instanciarReactor(caminho) {
  const bytes = await readFile(caminho);
  const modulo = new WebAssembly.Module(bytes);  // CompileError aqui ja vira FALHA com o motivo do V8
  const wasi = new WASI({ version: 'preview1', args: [path.basename(caminho)], env: {}, returnOnExit: true });
  const instancia = new WebAssembly.Instance(modulo, wasi.getImportObject());
  conferir(instancia.exports._start === undefined, 'o modulo exporta _start: nao e reactor');
  conferir(instancia.exports.memory instanceof WebAssembly.Memory, 'o modulo nao exporta memory');
  // initialize chama _initialize se existir; modulo reactor sem _initialize tambem e aceito.
  wasi.initialize(instancia);
  return instancia.exports;
}

function conferirFuncoes(exportacoes) {
  conferir(typeof exportacoes.soma === 'function', 'export soma ausente');
  conferir(typeof exportacoes.mul === 'function', 'export mul ausente');
  conferir(typeof exportacoes.media === 'function', 'export media ausente');
  conferir(typeof exportacoes.ler_i32 === 'function', 'export ler_i32 ausente');
  conferir(typeof exportacoes.eco === 'function', 'export eco ausente');
  const somaDeDoisTres = exportacoes.soma(2, 3);  // i32: number
  conferir(somaDeDoisTres === 5, `soma(2, 3) deu ${somaDeDoisTres}, esperado 5`);
  const produto = exportacoes.mul(2n, 3n);  // int e i64: BigInt nos dois sentidos
  conferir(produto === 6n, `mul(2, 3) deu ${produto}, esperado 6`);
  const media = exportacoes.media(1, 2);  // f64: number
  conferir(media === 1.5, `media(1, 2) deu ${media}, esperado 1.5`);
}

function conferirPonteiros(exportacoes) {
  const memoria = exportacoes.memory;
  const deslocamento = memoria.buffer.byteLength - TAMANHO_AREA_TESTE;
  // DataView criado logo antes do uso: uma chamada wasm pode ter trocado o buffer (memory.grow).
  new DataView(memoria.buffer).setInt32(deslocamento, VALOR_TESTE, true);
  // ponteiro atravessa a fronteira como i32 (number), nao BigInt
  const lido = exportacoes.ler_i32(deslocamento);
  conferir(lido === VALOR_TESTE, `ler_i32(${deslocamento}) deu ${lido}, esperado ${VALOR_TESTE}`);
  const devolvido = exportacoes.eco(deslocamento);
  conferir(devolvido === deslocamento, `eco(${deslocamento}) deu ${devolvido}`);
}

// Layout x64 (alinhamento natural) de Vec3 e Misto, ambos com 24 bytes; conferido contra os exports antes do uso.
const TAMANHO_STRUCT = 24n;
const OFF = { vy: 8, vz: 16, ma: 0, mb: 4, mc: 8, md: 16 };
const BLOCO_STRUCT = 64;  // cada bloco de teste tem 64 bytes; o caso usa ate 6 blocos
const AREA_STRUCT = 4096; // area no fim da memoria linear, livre depois do heap

function conferirLayout(exportacoes) {
  conferir(exportacoes.tam_vec3() === TAMANHO_STRUCT, `tam_vec3 deu ${exportacoes.tam_vec3()}, esperado 24`);
  conferir(exportacoes.tam_misto() === TAMANHO_STRUCT, `tam_misto deu ${exportacoes.tam_misto()}, esperado 24`);
  const esperados = { off_vec3_y: OFF.vy, off_vec3_z: OFF.vz, off_misto_a: OFF.ma, off_misto_b: OFF.mb, off_misto_c: OFF.mc, off_misto_d: OFF.md };
  for (const [nome, valor] of Object.entries(esperados)) {
    const obtido = exportacoes[nome]();
    conferir(obtido === BigInt(valor), `${nome} deu ${obtido}, esperado ${valor}`);
  }
}

function conferirAridades(exportacoes) {
  // length de uma funcao wasm = parametros; struct de retorno soma o sret inicial (n+1).
  const esperadas = { vetor_fixo: 1, soma: 1, dobra: 2, dobra_duas: 2, mistura: 4, empilha: 3 };
  for (const [nome, n] of Object.entries(esperadas)) {
    conferir(exportacoes[nome].length === n, `${nome}.length deu ${exportacoes[nome].length}, esperado ${n}`);
  }
}

function escreverVec3(exportacoes, deslocamento, x, y, z) {
  // DataView criado a cada escrita: uma chamada wasm pode ter trocado o buffer (memory.grow).
  const visao = new DataView(exportacoes.memory.buffer);
  visao.setFloat64(deslocamento, x, true);
  visao.setFloat64(deslocamento + OFF.vy, y, true);
  visao.setFloat64(deslocamento + OFF.vz, z, true);
}

function lerVec3(exportacoes, deslocamento) {
  const visao = new DataView(exportacoes.memory.buffer);
  return [visao.getFloat64(deslocamento, true), visao.getFloat64(deslocamento + OFF.vy, true), visao.getFloat64(deslocamento + OFF.vz, true)];
}

function conferirVec3(exportacoes, deslocamento, esperado, rotulo) {
  const obtido = lerVec3(exportacoes, deslocamento);
  conferir(obtido.join(',') === esperado.join(','), `${rotulo} deu [${obtido}], esperado [${esperado}]`);
}

function escreverMisto(exportacoes, deslocamento, a, b, c, d) {
  const visao = new DataView(exportacoes.memory.buffer);
  visao.setUint8(deslocamento + OFF.ma, a);
  visao.setInt32(deslocamento + OFF.mb, b, true);
  visao.setBigInt64(deslocamento + OFF.mc, c, true);
  visao.setFloat32(deslocamento + OFF.md, d, true);
}

function conferirMisto(exportacoes, deslocamento, esperado, rotulo) {
  const visao = new DataView(exportacoes.memory.buffer);
  const obtido = [visao.getUint8(deslocamento + OFF.ma), visao.getInt32(deslocamento + OFF.mb, true), visao.getBigInt64(deslocamento + OFF.mc, true), visao.getFloat32(deslocamento + OFF.md, true)];
  const texto = [esperado.a, esperado.b, esperado.c, esperado.d].map(String);
  conferir(obtido.map(String).join(',') === texto.join(','), `${rotulo} deu [${obtido}], esperado [${texto}]`);
}

function conferirVec3Struct(exportacoes, base) {
  const src = base;
  const dst = base + BLOCO_STRUCT;
  const dst2 = base + 2 * BLOCO_STRUCT;
  escreverVec3(exportacoes, src, 1.5, 2.5, 3);
  const total = exportacoes.soma(src);
  conferir(total === 7, `soma(src) deu ${total}, esperado 7`);
  exportacoes.vetor_fixo(dst);
  conferirVec3(exportacoes, dst, [1, 2, 3], 'vetor_fixo');
  exportacoes.dobra(dst2, src);
  conferirVec3(exportacoes, dst2, [3, 5, 6], 'dobra');
  exportacoes.dobra(dst2, src);  // 2a chamada reusando o mesmo bloco de retorno
  conferirVec3(exportacoes, dst2, [3, 5, 6], 'dobra (2a chamada)');
  exportacoes.dobra(src, src);   // sret igual ao bloco do parametro
  conferirVec3(exportacoes, src, [3, 5, 6], 'dobra in-place');
  escreverVec3(exportacoes, src, 1.5, 2.5, 3);
  exportacoes.dobra_duas(dst, src);
  conferirVec3(exportacoes, dst, [6, 10, 12], 'dobra_duas');
  exportacoes.empilha(dst, src, 0);
  conferirVec3(exportacoes, dst, [1.5, 2.5, 3], 'empilha n=0');
  exportacoes.empilha(dst, src, 3);
  conferirVec3(exportacoes, dst, [12, 20, 24], 'empilha n=3');
  exportacoes.empilha(dst, src, 3);  // 2a chamada reusando o bloco
  conferirVec3(exportacoes, dst, [12, 20, 24], 'empilha n=3 (2a chamada)');
  escreverVec3(exportacoes, src, 1.5, 2.5, 3);
  exportacoes.empilha(src, src, 3);  // sret igual ao parametro, recursivo
  conferirVec3(exportacoes, src, [12, 20, 24], 'empilha in-place');
}

function conferirMisturaStruct(exportacoes, base) {
  const mis = base + 3 * BLOCO_STRUCT;
  const wv = base + 4 * BLOCO_STRUCT;
  const out = base + 5 * BLOCO_STRUCT;
  escreverMisto(exportacoes, mis, 5, 100, 21n, 1.25);
  escreverVec3(exportacoes, wv, 1.5, 2.5, 3);
  exportacoes.mistura(out, 3, mis, wv);
  conferirMisto(exportacoes, out, { a: 8, b: 101, c: 42n, d: 1.75 }, 'mistura');
  exportacoes.mistura(mis, 3, mis, wv);  // sret igual ao bloco do Misto (parametro)
  conferirMisto(exportacoes, mis, { a: 8, b: 101, c: 42n, d: 1.75 }, 'mistura in-place');
}

function conferirStruct(exportacoes) {
  conferirLayout(exportacoes);
  conferirAridades(exportacoes);
  const base = exportacoes.memory.buffer.byteLength - AREA_STRUCT;
  conferirVec3Struct(exportacoes, base);
  conferirMisturaStruct(exportacoes, base);
}

async function principal(argv) {
  const [caminho, opcao] = argv;
  const comStruct = opcao === '--struct';
  if (!caminho || (opcao !== undefined && !comStruct)) {
    escreverErro('FALHA: uso: node --no-warnings verificar_reactor.mjs <arquivo.wasm> [--struct]');
    return CODIGO_FALHA;
  }
  try {
    const exportacoes = await instanciarReactor(caminho);
    if (comStruct) {
      conferirStruct(exportacoes);
    } else {
      conferirFuncoes(exportacoes);
      conferirPonteiros(exportacoes);
    }
  } catch (erro) {
    escreverErro(`FALHA: ${erro.message}`);
    return CODIGO_FALHA;
  }
  escreverSaida(comStruct ? 'reactor struct OK' : 'reactor OK');
  return CODIGO_OK;
}

process.exit(await principal(process.argv.slice(2)));
