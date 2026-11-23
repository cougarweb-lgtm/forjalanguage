// rodar_wasm.mjs 1.0.0 - roda um .wasm WASI preview1 com node:wasi (sem npm).
// Uso: node --no-warnings rodar_wasm.mjs <arquivo.wasm> [args...]
// Saida: o codigo passado ao proc_exit; 4 = INVALIDO (nao valida nem compila), 3 = TRAP, 5 = ERRO de outra causa, 101 = R0016 (recursao funda demais: RangeError do V8).
// Verificacao: python language/tests/nativo/wasm/verificar_wasm.py --selftest
// Versao: 1.0.2
// Changelog: 1.0.2 - RangeError (pilha de chamadas do V8) vira o JSON R0016 no stderr e saida 101, como o executor_wasm.mjs; 1.0.1 - process.exitCode no lugar de process.exit (corrida do libuv no encerramento, real_bordas dava exit 127 em ~75% das rodadas); 1.0.0 - primeira versao (validate e Module para o motivo, WASI com returnOnExit, codigos 3/4/5).

import { readFile } from 'node:fs/promises';
import { writeSync } from 'node:fs';
import path from 'node:path';
import { WASI } from 'node:wasi';

const CODIGO_TRAP = 3;
const CODIGO_INVALIDO = 4;
const CODIGO_ERRO = 5;
const CODIGO_FALHA_PILHA = 101;

class ArquivoInvalido extends Error {}

function escreverErro(texto) {
  // writeSync: o process.exit logo depois nao pode cortar a linha de erro num pipe assincrono.
  writeSync(2, texto + '\n');
}

function compilarModulo(bytes) {
  // validate da o veredito; Module traz o motivo quando o modulo e recusado.
  let motivo = 'WebAssembly.validate recusou o modulo';
  try {
    const modulo = new WebAssembly.Module(bytes);
    if (WebAssembly.validate(bytes)) return modulo;
  } catch (erro) {
    motivo = erro.message;
  }
  throw new ArquivoInvalido(motivo);
}

function iniciarWasi(nome, argumentos) {
  // returnOnExit: o proc_exit devolve o codigo a quem chamou, em vez de encerrar o processo.
  return new WASI({ version: 'preview1', args: [nome, ...argumentos], env: {}, returnOnExit: true });
}

function exigirExports(instancia) {
  if (typeof instancia.exports._start !== 'function' || !(instancia.exports.memory instanceof WebAssembly.Memory)) {
    throw new Error('o modulo precisa exportar _start (funcao) e memory (memoria)');
  }
}

async function rodarModulo(caminho, argumentos) {
  const bytes = await readFile(caminho);
  const modulo = compilarModulo(bytes);
  const wasi = iniciarWasi(path.basename(caminho), argumentos);
  const instancia = new WebAssembly.Instance(modulo, wasi.getImportObject());
  exigirExports(instancia);
  return wasi.start(instancia);
}

async function principal(argv) {
  const [caminho, ...argumentos] = argv;
  if (!caminho) {
    escreverErro('ERRO: uso: node --no-warnings rodar_wasm.mjs <arquivo.wasm> [args...]');
    return CODIGO_ERRO;
  }
  try {
    return await rodarModulo(caminho, argumentos);
  } catch (erro) {
    if (erro instanceof ArquivoInvalido) {
      escreverErro(`INVALIDO: ${erro.message}`);
      return CODIGO_INVALIDO;
    }
    if (erro instanceof WebAssembly.RuntimeError) {
      escreverErro(`TRAP: ${erro.message}`);
      return CODIGO_TRAP;
    }
    if (erro instanceof RangeError) {
      // R0016: recursao funda demais; o JSON e o do falha_pilha de rt_wasm.fj (igual ao executor_wasm.mjs)
      escreverErro('{"codigo": "R0016", "tipo": "runtime", "causa": "estouro de pilha: recursao funda demais"}');
      return CODIGO_FALHA_PILHA;
    }
    escreverErro(`ERRO: ${erro.message}`);
    return CODIGO_ERRO;
  }
}

// exitCode em vez de process.exit: com o Node 24 no Windows, process.exit logo depois do start derrubava o processo
// com "Assertion failed: !(handle->flags & UV_HANDLE_CLOSING), src\win\async.c" (exit 127) quando o V8 ainda tinha
// compilacao em segundo plano do modulo (corrida no teardown do libuv); deixar o loop esvaziar elimina a corrida.
process.exitCode = await principal(process.argv.slice(2));
