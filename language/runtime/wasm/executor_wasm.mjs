// runtime/wasm/executor_wasm.mjs - executor do `forja run` e do `forja test` no alvo wasm32-wasip1
// Versao: 0.2.0 (0.34.0: args_sizes_get/args_get)
// Uso: node --no-warnings executor_wasm.mjs <arquivo.wasm> <run|test> [args do programa...]   (o driver da Forja grava este arquivo ao lado do .wasm temporario e o apaga depois)
// WASI minima: o modulo so importa proc_exit e fd_write. fd_write escreve direto em stdout/stderr (sincrono, na ordem), entao nada fica retido.
// run: roda _start uma vez e sai com o codigo do proc_exit (101 = falha de execucao, JSON R-code no stderr).
// test: o executor de testes em Forja (rt_wasm_teste.fj) roda os testes e imprime tudo; uma queda dentro de um teste (R-code, panic, assert, trap) termina o
// modulo, entao este executor imprime a linha desse teste, anota a falha no bloco de estado da memoria e reinstancia o modulo para retomar no teste seguinte.
// Bloco de estado em [384,496) (u64) e filtro em [832,1024): o mesmo mapa do rt_wasm_teste.fj. FORJA_TESTE_FILTRO e FORJA_TESTE_JSON chegam pelo ambiente.

import fs from "node:fs";

const ESTADO = { tabela: 384, total: 392, proximo: 408, falhas: 416, filtrados: 424, json: 432, emTeste: 440, filtroTam: 488 };
const FILTRO_BASE = 832;
const FILTRO_MAX = 191;
const ENTRADA_BYTES = 16;
const SAIDA_FALHA = 101;
const SAIDA_INTERNO = 3;
// Argumentos do programa em UTF-8: o .wasm (argv[0]) e o resto da linha de comando do node depois de <run|test>
const ARGUMENTOS = [process.argv[2] || "", ...process.argv.slice(4)].map((argumento) => Buffer.from(argumento, "utf8"));

class SaidaWasm {
    constructor(codigo) {
        this.codigo = codigo;
    }
}

function escrever(fd, bytes) {
    let inicio = 0;
    while (inicio < bytes.length) {
        try {
            inicio += fs.writeSync(fd, bytes, inicio, bytes.length - inicio);
        } catch (erro) {
            if (erro.code === "EAGAIN") {
                continue;
            }
            if (erro.code === "EPIPE") {
                return;
            }
            throw erro;
        }
    }
}

function texto(fd, conteudo) {
    escrever(fd, Buffer.from(conteudo, "utf8"));
}

// Escape de JSON do rt_teste.c: aspas, barra, \n, \r, \t; outro byte de controle vira \u00xx
function escapeJson(valor) {
    let saida = "";
    for (const c of String(valor)) {
        const k = c.codePointAt(0);
        if (c === "\"") saida += "\\\"";
        else if (c === "\\") saida += "\\\\";
        else if (c === "\n") saida += "\\n";
        else if (c === "\r") saida += "\\r";
        else if (c === "\t") saida += "\\t";
        else if (k < 32) saida += "\\u00" + k.toString(16).padStart(2, "0");
        else saida += c;
    }
    return saida;
}

// Instancia o modulo com a WASI minima; `capturaErro` junta o stderr da instancia (o JSON da falha que derrubou o teste)
function instanciar(modulo, capturaErro) {
    let memoria = null;
    const imports = {
        wasi_snapshot_preview1: {
            proc_exit(codigo) {
                throw new SaidaWasm(codigo);
            },
            // 0.34.0: argumentos do programa (so quando o modulo importa args_*): argv[0] = o .wasm, depois o que veio depois do `--`
            args_sizes_get(argcPtr, bufTamPtr) {
                const vista = new DataView(memoria.buffer);
                vista.setUint32(argcPtr, ARGUMENTOS.length, true);
                vista.setUint32(bufTamPtr, ARGUMENTOS.reduce((soma, bytes) => soma + bytes.length + 1, 0), true);
                return 0;
            },
            args_get(argvPtr, bufPtr) {
                const vista = new DataView(memoria.buffer);
                const bytes = new Uint8Array(memoria.buffer);
                let cursor = bufPtr;
                ARGUMENTOS.forEach((argumento, k) => {
                    vista.setUint32(argvPtr + 4 * k, cursor, true);
                    bytes.set(argumento, cursor);
                    bytes[cursor + argumento.length] = 0;
                    cursor += argumento.length + 1;
                });
                return 0;
            },
            fd_write(fd, iovs, quantos, escritos) {
                const vista = new DataView(memoria.buffer);
                let total = 0;
                for (let k = 0; k < quantos; k++) {
                    const ptr = vista.getUint32(iovs + 8 * k, true);
                    const tam = vista.getUint32(iovs + 8 * k + 4, true);
                    const bytes = Buffer.from(memoria.buffer, ptr, tam);
                    if (fd === 2) {
                        capturaErro.push(Buffer.from(bytes));
                    }
                    escrever(fd === 2 ? 2 : 1, bytes);
                    total += tam;
                }
                vista.setUint32(escritos, total, true);
                return 0;
            },
        },
    };
    const instancia = new WebAssembly.Instance(modulo, imports);
    memoria = instancia.exports.memory;
    return instancia;
}

function lerU64(memoria, endereco) {
    return Number(new DataView(memoria.buffer).getBigUint64(endereco, true));
}

function gravarU64(memoria, endereco, valor) {
    new DataView(memoria.buffer).setBigUint64(endereco, BigInt(valor), true);
}

function lerCstr(memoria, endereco) {
    const bytes = new Uint8Array(memoria.buffer);
    let fim = endereco;
    while (bytes[fim] !== 0) {
        fim++;
    }
    return Buffer.from(bytes.subarray(endereco, fim)).toString("utf8");
}

// Estado que atravessa as reinstancias: proximo teste, falhas e filtrados ja contados; json e filtro vem do ambiente
function gravarEstado(memoria, estado) {
    gravarU64(memoria, ESTADO.proximo, estado.proximo);
    gravarU64(memoria, ESTADO.falhas, estado.falhas);
    gravarU64(memoria, ESTADO.filtrados, estado.filtrados);
    gravarU64(memoria, ESTADO.json, estado.json ? 1 : 0);
    const filtro = Buffer.from(estado.filtro, "utf8").subarray(0, FILTRO_MAX);
    new Uint8Array(memoria.buffer).set(filtro, FILTRO_BASE);
    new Uint8Array(memoria.buffer)[FILTRO_BASE + filtro.length] = 0;
    gravarU64(memoria, ESTADO.filtroTam, filtro.length);
}

// Ultima linha JSON de falha (R-code ou panic) que o modulo imprimiu no stderr; null se nao houve
function ultimaFalhaJson(capturaErro) {
    const linhas = Buffer.concat(capturaErro).toString("utf8").split("\n");
    for (let k = linhas.length - 1; k >= 0; k--) {
        if (linhas[k].startsWith("{\"codigo\"")) {
            try {
                return JSON.parse(linhas[k]);
            } catch (erro) {
                return null;
            }
        }
    }
    return null;
}

// Linha do teste que caiu: "FALHOU <titulo>" ou o registro do --json com a causa da falha
function relatarQueda(titulo, falha, json) {
    if (!json) {
        texto(1, "FALHOU " + titulo + "\n");
        return;
    }
    texto(1, "{\"teste\": \"" + escapeJson(titulo) + "\", \"resultado\": \"falhou\", \"codigo\": \"" + escapeJson(falha.codigo) +
        "\", \"arquivo\": \"" + escapeJson(falha.arquivo) + "\", \"linha\": " + Number(falha.linha) +
        ", \"causa\": \"" + escapeJson(falha.causa) + "\"}\n");
}

function rodarUmaVez(modulo, estado, capturaErro) {
    const instancia = instanciar(modulo, capturaErro);
    if (estado) {
        gravarEstado(instancia.exports.memory, estado);
    }
    try {
        instancia.exports._start();
    } catch (erro) {
        if (erro instanceof SaidaWasm) {
            return { instancia, codigo: erro.codigo, trap: null };
        }
        return { instancia, codigo: SAIDA_FALHA, trap: erro };
    }
    return { instancia, codigo: 0, trap: null };
}

function rodar(modulo) {
    const capturaErro = [];
    const { trap, codigo } = rodarUmaVez(modulo, null, capturaErro);
    if (trap) {
        // R0016: recursao funda demais; o V8 lanca RangeError (pilha de chamadas), e o JSON e o mesmo do falha_pilha de rt_wasm.fj
        if (trap instanceof RangeError) {
            texto(2, "{\"codigo\": \"R0016\", \"tipo\": \"runtime\", \"causa\": \"estouro de pilha: recursao funda demais\"}\n");
        } else {
            texto(2, "forja: o modulo wasm parou numa trap: " + trap.message + "\n");
        }
    }
    return codigo;
}

function testar(modulo) {
    const estado = {
        proximo: 0,
        falhas: 0,
        filtrados: 0,
        json: process.env.FORJA_TESTE_JSON === "1",
        filtro: process.env.FORJA_TESTE_FILTRO || "",
    };
    for (;;) {
        const capturaErro = [];
        const { instancia, codigo, trap } = rodarUmaVez(modulo, estado, capturaErro);
        const memoria = instancia.exports.memory;
        const emTeste = lerU64(memoria, ESTADO.emTeste);
        if ((codigo !== SAIDA_FALHA) || (emTeste === 0)) {
            if (trap) {
                texto(2, "forja: o modulo wasm parou numa trap: " + trap.message + "\n");
            }
            return codigo;
        }
        const indice = emTeste - 1;
        const nome = lerU64(memoria, lerU64(memoria, ESTADO.tabela) + indice * ENTRADA_BYTES);
        const titulo = lerCstr(memoria, nome);
        let falha = ultimaFalhaJson(capturaErro);
        if (!falha || trap) {
            const causa = trap ? trap.message : "o teste terminou o modulo sem relatar a falha";
            if (trap) {
                texto(2, "forja: o teste \"" + titulo + "\" parou numa trap do wasm: " + trap.message + "\n");
            }
            falha = { codigo: "R0011", arquivo: "", linha: 0, causa: "o teste \"" + titulo + "\" causou uma excecao: " + causa };
        }
        relatarQueda(titulo, falha, estado.json);
        estado.falhas = lerU64(memoria, ESTADO.falhas) + 1;
        estado.filtrados = lerU64(memoria, ESTADO.filtrados);
        estado.proximo = indice + 1;
    }
}

function principal() {
    const caminho = process.argv[2];
    const modo = process.argv[3];
    if (!caminho || ((modo !== "run") && (modo !== "test"))) {
        texto(2, "uso: node executor_wasm.mjs <arquivo.wasm> <run|test>\n");
        return SAIDA_INTERNO;
    }
    let modulo = null;
    try {
        modulo = new WebAssembly.Module(fs.readFileSync(caminho));
    } catch (erro) {
        texto(2, "forja: nao consegui carregar " + caminho + ": " + erro.message + "\n");
        return SAIDA_INTERNO;
    }
    return modo === "test" ? testar(modulo) : rodar(modulo);
}

process.exitCode = principal();
