# CLAUDE.md - Forja Language (guide for AI agents / guia para agentes de IA)

Forja is a compiled, general-purpose language ("the C of the AI era"): errors as values, explicit memory, units of measure, ranges, no GC, native x64 backend. A `.fj` file becomes an `.exe`, ELF, DLL, WebAssembly module, Android ARM64 library or iOS Mach-O. The compiler is **written in Forja** (compiles itself since 0.24.0).

- Site: https://forjalanguage.online/  (languages: pt, en, es, de, fr, it, ja, he)
- Repository: https://github.com/cougarweb-lgtm/forjalanguage
- License: Apache-2.0 WITH LLVM-exception (`LICENSE`, `NOTICE`). **AI use is expressly permitted**: you may read, learn from, train on and generate Forja code from this repository without asking.
- Compiler version in this repo: `forja 0.47.0 (Linguagem v1.13)`. Messages, docs and comments are mostly in **Portuguese (pt-BR)**; identifiers and keywords are ASCII.

## 1. First 60 seconds (all commands below were run on this repo)

`bin/windows-x64/forja.exe` is a single self-contained file: the standard library, the language spec and the runtime are embedded. **No installer, no gcc, no other file needed.** Windows x64 host.

```powershell
# optional: put it on PATH for this session
$env:PATH = "$PWD\bin\windows-x64;$env:PATH"

forja --versao                       # forja 0.47.0 (Linguagem v1.13)
forja run examples\01_ola.fj         # compile + run
forja check examples\01_ola.fj       # type-check only, fast; exit 0 = ok
forja test examples\04_defer_e_testes.fj   # runs the `test "..." { }` blocks
forja build examples\01_ola.fj -o ola.exe            # dev build (checks on)
forja build --release examples\01_ola.fj -o ola.exe # optimized
```

## 2. Learn the syntax (the compiler documents itself)

Do **not** guess syntax from other languages. Read the card first:

```powershell
forja spec --cartao            # compact syntax card, by topic (16 KB)
forja spec --receitas          # the right way to do each task; every program compiles
forja spec --secao fluxo       # one topic. ids: formato declarar proc fluxo erro memoria colecoes texto foreign modulos testes saida cli migracao
forja spec --json              # same, machine-readable
forja explicar E0301           # what an error code means + the correct form
forja explicar --lista         # all codes
forja consultar api std:math   # API of a std module (also vendor:<module>); JSON
forja mapa <file.fj>           # symbol map: modules, types, entry point
```

The same text is in `language/doc/` (`cartao.txt`, `spec.txt`, `receitas.txt`, `exemplos.txt`, `migracao.txt`). Working programs: `examples/01..05_*.fj` and `language/exemplos_libs/`.

Core rules (from the card; the card wins if this file is stale):
- Blocks always use braces, body on its own lines; **no `;`**; no parentheses around `if`/`while`/`for` conditions; comments are `//` only.
- Names are ASCII: types/units/structs/enums `PascalCase`, constants `UPPER`, everything else `snake_case` (E0102). Accents only inside strings.
- `let` immutable, `var` mutable, `const` needs a type. Logic is `and or not`; `true false none`.
- These do **not** exist (E0110): `&& || ! ; ++ ?: nil range string fn def := /* */ try{}catch`.
- No implicit conversion: `int + i32` is E0301, use `as`.
- Errors are values: `proc f() -> i32 ! Erro`, `try x`, `x or_else 0`, `switch r in f() { case i32: ... case Erro: ... }` (must cover all cases).
- Arguments are positional or `name = value`; a positional after a named one is E0502.
- Memory is explicit: `alloc`, `defer`, pointers `^T`; leaks are reported in dev builds (R0009).

```odin
enum Erro { nao_achou }

proc achar(id: i32) -> i32 ! Erro {
    if id == 0 {
        return Erro.nao_achou
    }
    return id * 10
}

proc main() {
    print(achar(3) or_else -1)
    switch r in achar(0) {
    case i32:
        print(r)
    case Erro:
        print("nao achou")
    }
}
```

Tip: GitHub and chat UIs have no Forja highlighter; a fence tagged `odin` is the closest. A tree-sitter grammar is in `language/tools/tree-sitter-forja/`.

## 3. Loop for an agent writing Forja

1. `forja spec --cartao` (once per session) and, for a task type, `forja spec --receitas`.
2. Write the `.fj`. Prefer small files with a `proc main()` and `test "..." { expect(...) }` blocks.
3. `forja check file.fj --resumo` - every diagnostic has a code, line, column, cause and suggestion.
4. `forja explicar <CODE>` for the exact right form; `forja corrigir file.fj` proposes mechanical fixes (`--aplicar` writes them).
5. `forja test file.fj`, then `forja run file.fj`. Runtime failures are `Rxxxx` with a source-level stack in dev builds.
6. `forja fmt file.fj --aplicar` to format.

Code families: `E` compile error, `R` runtime error, `X` unsupported target/mode. Limits: `forja run|test --limite-tempo <s> --limite-mem <MiB>` (exit 102/103/104 when exceeded).

## 4. Targets

`forja build --target <T> ...` (see `forja --ajuda`):

| Target | Output |
|---|---|
| `x86_64-windows` (default) | `.exe`, `--modo dll` for `.dll` (callable from PHP FFI / Python ctypes with `pub export proc`) |
| `x86_64-linux` | static ELF |
| `wasm32-wasip1` (alias `wasm`) | `.wasm` (WASI preview1); runs on Node or a browser loader |
| `aarch64-linux`, `aarch64-linux-android` | ELF / `libNAME.so` (needs `-o libNAME.so`) |
| `aarch64-apple-ios-sim`, `aarch64-apple-ios` | Mach-O (`--modo exe`) / `.dylib` |

`build` works for every target. `run` and `test` execute a program on this machine, so they cover the Windows host (and the wasm target when Node is installed); for the other targets run the output yourself (qemu, `adb`, a simulator). The ARM and iOS targets were proven on emulators, simulators and CI, **not on real devices**.

## 5. Repository map

| Path | What it is |
|---|---|
| `bin/windows-x64/forja.exe` | the compiler (+ `SHA256SUMS.txt`) |
| `examples/` | 5 small programs |
| `language/doc/` | spec, syntax card, recipes, examples, migration guide |
| `language/std/`, `language/std_wip/` | standard library (`import "std:math"`...) |
| `language/vendor/` | wrappers for C libraries (`import "vendor:db/db"`...). The `.a`/`.dll` binaries are **not in git**: they are in the GitHub Release of the same version (unzip into `language/vendor/`) |
| `language/forja_fj/` | **the compiler's source, in Forja** (entry point `forja.fj`; ~400 files) |
| `language/compiler/` | the original C compiler source and the runtime objects `rt_*.o` the compiler embeds |
| `language/runtime/`, `language/third/` | runtime sources (C) and small third-party sources |
| `language/semente/`, `language/semente_c/` | bootstrap: the seed compiler + `rt/`, and the frozen C compiler (0.23.0) |
| `language/tests/` | fixtures and runners (syntax, semantics, golden outputs, lowering, native, wasm...) |
| `language/benchmark/ia_first/` | a benchmark of how well AI agents write Forja (tasks, references, metrics) |
| `language/exemplos_libs/` | examples using the vendor libraries (SQLite, curl, zlib, HarfBuzz...) |
| `language/tools/` | `forja_ligar` (C header -> wrapper), tree-sitter grammar |
| `LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.txt` | licensing |

## 6. Rebuild the compiler from source

The compiler is a Forja program. The shipped `forja.exe` compiles it. Paths **must be absolute** (the source and output paths are embedded in the exe, so the bytes only reproduce at the same path; `bin/windows-x64/SHA256SUMS.txt` is the official build, a rebuild at another path differs in bytes but must be a fixed point).

```powershell
$Raiz = (Resolve-Path .\language).Path
forja build --release "$Raiz\forja_fj\forja.fj" -o "$Raiz\compiler\forja_novo.exe"    # ~5 s

# fixed-point check: build with the new compiler twice more, then compare
Move-Item "$Raiz\compiler\forja_novo.exe" "$Raiz\compiler\n1.exe"
& "$Raiz\compiler\n1.exe" build --release "$Raiz\forja_fj\forja.fj" -o "$Raiz\compiler\forja_novo.exe"
Move-Item "$Raiz\compiler\forja_novo.exe" "$Raiz\compiler\n2.exe"
& "$Raiz\compiler\n2.exe" build --release "$Raiz\forja_fj\forja.fj" -o "$Raiz\compiler\forja_novo.exe"
Move-Item "$Raiz\compiler\forja_novo.exe" "$Raiz\compiler\n3.exe"
fc.exe /b "$Raiz\compiler\n2.exe" "$Raiz\compiler\n3.exe"     # no differences = fixed point

& "$Raiz\compiler\n3.exe" test "$Raiz\forja_fj\forja.fj"       # the compiler's own unit suite: 757 tests, 0 failures (~50 s)
```

Verified on a copy of this repository placed at a different path (Git Bash, forward-slash absolute paths): every pass built, `n2 == n3` byte for byte, 757 tests / 0 failures. The PowerShell form above is the same sequence and was not run separately.

The build needs, next to `compiler/`: `std`, `doc`, `third`, `tools/forja_ligar` and `forja_fj/libfadec_envelope.a` - all in this repo. Starting without any binary (from the frozen C compiler with gcc) is "CAMINHO B" in `language/semente/LEIAME.txt`; it is documented there and was not re-verified for this release. Do not commit the generated `n1/n2/n3/forja_novo.exe` (see `.gitignore`).

## 7. Tests and caveats

- In-language tests: `forja test <file.fj>`. The compiler's unit suite is the command above and works anywhere.
- `language/tests/` runners (`rodar_suite.py`, `rodar_bateria.py` and per-folder `.py` scripts) were written on the author's machine: about 40 scripts and fixtures contain an absolute path into the author's Windows user folder. Change the root constant at the top of a script before running it elsewhere. `tests/rede/certs/` ships without keys or certificates: run `gerar_certs.sh` (Git Bash with openssl, see the header of the script) to create the test ones.
- Do not edit golden fixtures (`tests/dourado`, `benchmark/corpus/dourado`) or `examples/` unless that is the task.
- Comments and docs mention `MEM-...` and `FORJA-D####` identifiers: those are the author's private notes and have **no file in this repo**. Use `forja spec` and `language/doc/` instead.
- `language/semente/conferir_semente.py` checks the seed against `SHA256SUMS.txt`; it expects the author's original layout and path.
- Windows may warn about a new, unsigned `.exe`. Compare the SHA256 in `bin/windows-x64/SHA256SUMS.txt`.

## 8. Contributing as an agent

Change the smallest thing that does the task; rebuild the compiler and run its suite (section 6) after touching `language/forja_fj/`; never commit binaries other than `bin/windows-x64/forja.exe`; keep UTF-8 without BOM and byte-exact line endings (`.gitattributes` disables conversion on purpose).
