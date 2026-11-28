# Forja Language

**Forja** is a general-purpose compiled programming language, "the C of the AI era": errors as values, explicit memory, units of measure, value ranges, no garbage collector and a small syntax designed for AI agents. Source files use the `.fj` extension. The compiler is **written in Forja** and generates native x64 code (`.exe`, ELF, DLL), WebAssembly (WASI), Android ARM64 and iOS ARM64.

**Forja** é uma linguagem compilada de propósito geral, "o C da era da IA": erros como valor, memória explícita, unidades de medida, faixas de valores, sem GC e uma sintaxe pequena, pensada para agentes de IA. O compilador é **escrito em Forja** e gera código nativo x64 (`.exe`, ELF, DLL), WebAssembly, Android ARM64 e iOS.

**Website: https://forjalanguage.online/** (pt, en, es, de, fr, it, ja, he)

**Forja in one page (English): https://forjalanguage.online/en/learn.html** — syntax, twelve complete programs with real output, command line, error codes and limits. Plain text: [llms-full.txt](https://forjalanguage.online/llms-full.txt).

## Quick start / Começo rápido (Windows x64)

`bin/windows-x64/forja.exe` is a single self-contained file: standard library, language spec and runtime are embedded. No installer and no gcc needed.
O `forja.exe` é um arquivo só: biblioteca padrão, especificação e runtime vão dentro dele. Não precisa instalar nada nem ter gcc.

```powershell
git clone https://github.com/cougarweb-lgtm/forjalanguage
cd forjalanguage
.\bin\windows-x64\forja.exe --versao
.\bin\windows-x64\forja.exe run examples\01_ola.fj
.\bin\windows-x64\forja.exe test examples\04_defer_e_testes.fj
.\bin\windows-x64\forja.exe spec --cartao
```

## Syntax at a glance

- One statement per line, blocks always use braces, no semicolons, no parentheses around `if` / `while` / `for` conditions.
- `let` is immutable, `var` is mutable, `const` requires a type.
- Logic operators are `and`, `or`, `not`. There is no `&&`, `||` or `!`.
- Errors are values: `T ! E` is "a value or an error". Use `try` to propagate, `or_else` for a default and `switch` to handle both cases.
- Optional values are `?T`; the empty value is `none`.
- No implicit conversions: use `as`.
- Memory is explicit: allocate with `alloc = heap`, release with `delete`, usually through `defer`.
- Naming is enforced by the compiler: `PascalCase` types and units, `UPPER_CASE` constants, `snake_case` for the rest. Comments are `//` only.

## Examples

Every example below is a complete program.

### Hello, default and named arguments

```forja
proc double(n: int, times: int = 2) -> int {
    return n * times
}

proc main() {
    print("Hello, Forja!")
    print(double(21))
    print(double(7, times = 3))
}
```

### Errors as values

```forja
enum SearchError { not_found }

proc find(id: i32) -> i32 ! SearchError {
    if id == 0 {
        return SearchError.not_found
    }
    return id * 10
}

// "try" passes the error to the caller, which must return the same error type
proc double_of_found(id: i32) -> i32 ! SearchError {
    let value = try find(id)
    return value * 2
}

proc main() {
    // "or_else" gives a default value only when there is an error
    print(find(3) or_else -1)
    print(find(0) or_else -1)

    // "switch" on a result must handle both the value and the error
    switch r in double_of_found(4) {
    case i32:
        print(r)
    case SearchError:
        print("not found")
    }
}
```

### Units of measure and value ranges

```forja
unit Meters: f32
unit Seconds: f32
unit MetersPerSecond = Meters / Seconds

struct Target {
    health: i32 in 0..=100 = 100
    level: int in 1..<10 = 1
}

proc main() {
    let distance: Meters = 10.0
    let elapsed: Seconds = 2.0
    let speed: MetersPerSecond = distance / elapsed
    print(speed as int)

    let target = Target{health = 55, level = 9}
    print(target.health)
    print(target.level)
    // distance + elapsed is a compile error (mixed units)
    // a value outside 0..=100 stops a dev build with a runtime error
}
```

### Loops, `defer` and built-in tests

```forja
proc sum_up_to(n: int) -> int {
    var total = 0
    for i in 1..=n {
        total += i
    }
    return total
}

proc main() {
    // deferred calls run when the block ends, in reverse order
    defer print("end")
    defer print("almost done")
    print(sum_up_to(10))
}

// run with: forja test file.fj
test "sum from 1 to 10" {
    expect(sum_up_to(10) == 55)
}

test "sum of zero terms" {
    expect(sum_up_to(0) == 0)
}
```

### Calling C functions

```forja
foreign {
    link "msvcrt"
    proc c_calloc = "calloc"(n: u64, size: u64) -> ?^i32
    proc c_free = "free"(p: rawptr)
}

// the slice is borrowed: whoever allocated the memory releases it
proc add_all(values: []i32) -> int {
    var total: int = 0
    for x in values {
        total = total + x as int
    }
    return total
}

proc main() {
    let n: i32 = 5
    let p = c_calloc(5, size = 4)
    var v = slice_from(p, len = n)
    for i in 0..<len(v) {
        v[i] = (i * i) as i32
    }
    print(len(v))
    print(add_all(v[..]))
    c_free(p as rawptr)
}
```

## Documentation / Documentação

The documents below are written in Portuguese (pt-BR); code, keywords and identifiers are ASCII.
Os documentos abaixo estão em português.

| File / Arquivo | Content / Conteúdo |
|---|---|
| [`language/doc/cartao.txt`](language/doc/cartao.txt) | syntax card, by topic / cartão de sintaxe por tema |
| [`language/doc/spec.txt`](language/doc/spec.txt) | language specification / especificação |
| [`language/doc/receitas.txt`](language/doc/receitas.txt) | recipes / receitas |
| [`language/doc/exemplos.txt`](language/doc/exemplos.txt) | examples / exemplos |
| [`language/doc/migracao.txt`](language/doc/migracao.txt) | migration notes / migração |
| [`examples/`](examples/) | small example programs / programas de exemplo |
| [`language/exemplos_libs/`](language/exemplos_libs/) | examples using libraries / exemplos com bibliotecas |

## What is here / O que tem aqui

| Path / Pasta | Content / Conteúdo |
|---|---|
| `bin/windows-x64/forja.exe` | the compiler (with `SHA256SUMS.txt`) / o compilador |
| `language/forja_fj/` | **the compiler source, written in Forja** / o código-fonte do compilador, em Forja |
| `language/doc/` | specification, syntax card, recipes / especificação, cartão, receitas |
| `language/std/`, `language/vendor/` | standard library and C library wrappers / biblioteca padrão e ligações com C |
| `examples/`, `language/exemplos_libs/` | example programs / programas de exemplo |
| `language/tests/`, `language/benchmark/` | tests and AI benchmark / testes e benchmark de IA |
| `language/semente/`, `language/semente_c/` | compiler bootstrap / bootstrap do compilador |

The prebuilt C libraries (`.a`/`.dll`, used by `vendor:*`) are in the **Releases** page, not in git.
As bibliotecas C pré-compiladas (`.a`/`.dll`, usadas por `vendor:*`) estão na página de **Releases**, não no git.

## For AI agents / Para agentes de IA

Read [`CLAUDE.md`](CLAUDE.md): it tells you where the syntax and examples are, the commands, how to rebuild the compiler, and what has not been proven yet.
Leia o [`CLAUDE.md`](CLAUDE.md): ele diz onde estão a sintaxe, os exemplos, os comandos, como recompilar o compilador e o que ainda não foi provado.

## License and AI use / Licença e uso por IA

[Apache-2.0 with the LLVM exception](LICENSE) (`Apache-2.0 WITH LLVM-exception`): you may download, use, modify and redistribute, including commercially. Programs you build with Forja are yours and can be distributed without the notices for the embedded runtime.

[Apache-2.0 com a exceção LLVM](LICENSE): você pode baixar, usar, modificar e redistribuir, inclusive comercialmente. Os programas que você compila com a Forja são seus e podem ser distribuídos sem os avisos do runtime embutido.

**Use by artificial intelligence is expressly permitted**: people and AIs (Claude, ChatGPT, Gemini, open models...) may download, read, learn from and train models on this repository, and generate Forja code, without asking. See the [`NOTICE`](NOTICE).
**Uso por inteligência artificial é expressamente permitido**: pessoas e IAs (Claude, ChatGPT, Gemini, modelos abertos...) podem baixar, ler, aprender com e treinar modelos neste repositório, e gerar código Forja, sem pedir autorização. Veja o [`NOTICE`](NOTICE).

Third-party libraries keep their own licenses: [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt).
Bibliotecas de terceiros mantêm as próprias licenças: [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt).

## Status / Estado

Compiler **0.47.0**, language **v1.13**. On Windows: `run`, `test`, `build`. The ARM64/Android and iOS targets were proven on emulators, simulators and CI, **not on real devices**. `forja.exe` is not code-signed: Windows may warn; check the SHA256.
Compilador **0.47.0**, linguagem **v1.13**. No Windows: `run`, `test`, `build`. Os alvos ARM64/Android e iOS foram provados em emuladores, simuladores e CI, **não em aparelhos reais**. O `forja.exe` não é assinado digitalmente: o Windows pode avisar; confira o SHA256.
