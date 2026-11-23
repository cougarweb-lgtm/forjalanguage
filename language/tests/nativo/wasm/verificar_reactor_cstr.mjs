// verificar_reactor_cstr.mjs 1.0.0 - confere um reactor wasm cujos exports devolvem cstr/?cstr (tests/nativo/wasm/reactor/reactor_cstr.fj): o modulo exporta forja_liberar.
// Uso: node --no-warnings verificar_reactor_cstr.mjs <arquivo.wasm> [--release]  (--release: modulo de release, que NAO exporta forja_relatar_vazamentos)
// Saida: "reactor cstr OK" e exit 0; qualquer falha vai ao stderr como FALHA: <motivo> e sai 1.
// Verificacao: node --check tests/nativo/wasm/verificar_reactor_cstr.mjs (so sintaxe; a execucao depende do reactor do compilador).
// Versao: 1.1.0
// Changelog: 1.1.0 - forja_relatar_vazamentos (dev): bloco liberado nao e relatado, bloco vivo da R0009 no stderr (capturado num arquivo) e conta >= 1; em --release o export nao existe.
// 1.0.0 - primeira versao (texto lido da memoria linear, none = 0, forja_liberar devolve o bloco ao heap: a 2a chamada reaproveita o mesmo offset).

import { readFile } from 'node:fs/promises';
import { closeSync, openSync, readFileSync, unlinkSync, writeSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { WASI } from 'node:wasi';

const CODIGO_OK = 0;
const CODIGO_FALHA = 1;

function conferir(condicao, mensagem) {
  if (!condicao) throw new Error(mensagem);
}

async function instanciarReactor(caminho, fdErro) {
  const bytes = await readFile(caminho);
  const modulo = new WebAssembly.Module(bytes);
  const wasi = new WASI({ version: 'preview1', args: [path.basename(caminho)], env: {}, stderr: fdErro, returnOnExit: true });
  const instancia = new WebAssembly.Instance(modulo, wasi.getImportObject());
  conferir(instancia.exports._start === undefined, 'o modulo exporta _start: nao e reactor');
  wasi.initialize(instancia);
  return instancia.exports;
}

// Le o texto terminado em NUL que comeca em `deslocamento` (o Uint8Array e recriado a cada leitura: memory.grow troca o buffer).
function lerCstr(exportacoes, deslocamento) {
  const bytes = new Uint8Array(exportacoes.memory.buffer);
  let fim = deslocamento;
  while (bytes[fim] !== 0) fim += 1;
  return new TextDecoder().decode(bytes.subarray(deslocamento, fim));
}

function conferirExports(exportacoes) {
  conferir(typeof exportacoes.saudacao === 'function', 'export saudacao ausente');
  conferir(typeof exportacoes.talvez === 'function', 'export talvez ausente');
  conferir(typeof exportacoes.forja_liberar === 'function', 'export forja_liberar ausente');
}

function conferirTextos(exportacoes) {
  const primeiro = exportacoes.saudacao(7);  // cstr = offset i32 (number)
  conferir(typeof primeiro === 'number' && primeiro > 0, `saudacao(7) deu ${primeiro}, esperado um offset > 0`);
  const texto = lerCstr(exportacoes, primeiro);
  conferir(texto === 'ola 7', `saudacao(7) deu "${texto}", esperado "ola 7"`);
  const nenhum = exportacoes.talvez(-1);  // ?cstr none = 0
  conferir(nenhum === 0, `talvez(-1) deu ${nenhum}, esperado 0 (none)`);
  const outro = exportacoes.talvez(5);
  const textoOutro = lerCstr(exportacoes, outro);
  conferir(textoOutro === 'n=5', `talvez(5) deu "${textoOutro}", esperado "n=5"`);
  exportacoes.forja_liberar(outro);
  exportacoes.forja_liberar(primeiro);
  // o bloco devolvido volta para a lista livre da mesma classe: a mesma chamada reaproveita o offset
  const denovo = exportacoes.saudacao(7);
  conferir(denovo === primeiro, `depois de forja_liberar, saudacao(7) deu ${denovo}, esperado o mesmo offset ${primeiro}`);
  exportacoes.forja_liberar(denovo);
}

// Dev: liberar antes de relatar nao deixa nada; bloco vivo sai como R0009 no stderr (arquivo) e conta >= 1 ponto; o 2o relatorio ja nao ve o bloco.
function conferirRelatorio(exportacoes, arquivoErro) {
  conferir(typeof exportacoes.forja_relatar_vazamentos === 'function', 'export forja_relatar_vazamentos ausente no modulo dev');
  const solto = exportacoes.saudacao(2);
  exportacoes.forja_liberar(solto);
  conferir(exportacoes.forja_relatar_vazamentos() === 0, 'bloco liberado foi relatado como vazamento');
  conferir(!readFileSync(arquivoErro, 'utf8').includes('R0009'), 'R0009 saiu sem vazamento');
  exportacoes.saudacao(3);  // vaza de proposito
  const pontos = exportacoes.forja_relatar_vazamentos();
  conferir(pontos >= 1, `forja_relatar_vazamentos devolveu ${pontos}, esperado >= 1`);
  conferir(readFileSync(arquivoErro, 'utf8').includes('R0009'), 'o stderr nao ganhou R0009');
  conferir(exportacoes.forja_relatar_vazamentos() === 0, 'o 2o relatorio ainda ve o bloco ja relatado');
}

async function principal(argv) {
  const release = argv.includes('--release');
  const [caminho] = argv.filter((a) => a !== '--release');
  if (!caminho) {
    writeSync(2, 'FALHA: uso: node --no-warnings verificar_reactor_cstr.mjs <arquivo.wasm>\n');
    return CODIGO_FALHA;
  }
  try {
    const arquivoErro = path.join(tmpdir(), `reactor_cstr_${process.pid}.err`);
    const fdErro = openSync(arquivoErro, 'w+');
    try {
      const exportacoes = await instanciarReactor(caminho, fdErro);
      conferirExports(exportacoes);
      conferirTextos(exportacoes);
      if (release) {
        conferir(exportacoes.forja_relatar_vazamentos === undefined, 'modulo release exporta forja_relatar_vazamentos');
      } else {
        conferirRelatorio(exportacoes, arquivoErro);
      }
    } finally {
      closeSync(fdErro);
      unlinkSync(arquivoErro);
    }
  } catch (erro) {
    writeSync(2, `FALHA: ${erro.message}\n`);
    return CODIGO_FALHA;
  }
  writeSync(1, 'reactor cstr OK\n');
  return CODIGO_OK;
}

process.exit(await principal(process.argv.slice(2)));
