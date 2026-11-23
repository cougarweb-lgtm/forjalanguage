# Forja Language

**Forja** é uma linguagem compilada de propósito geral, "o C da era da IA": erros como valor, memória explícita, unidades de medida, faixas de valores, sem GC e uma sintaxe pequena que agentes de IA acertam de primeira. O compilador é **escrito em Forja** e gera código nativo x64 (`.exe`, ELF, DLL), WebAssembly, Android ARM64 e iOS.

**Forja** is a general-purpose compiled language, "the C of the AI era": errors as values, explicit memory, units of measure, value ranges, no GC and a small syntax that AI agents get right on the first try. The compiler is **written in Forja** and generates native x64 code (`.exe`, ELF, DLL), WebAssembly, Android ARM64 and iOS.

**Site / Website: https://forjalanguage.online/** (pt, en, es, de, fr, it, ja, he)

## Começo rápido / Quick start (Windows x64)

O `forja.exe` está pronto em `bin/windows-x64/`. É um arquivo só: biblioteca padrão, especificação e runtime vão dentro dele. Não precisa instalar nada nem ter gcc.

```powershell
git clone https://github.com/cougarweb-lgtm/forjalanguage
cd forjalanguage
.\bin\windows-x64\forja.exe --versao
.\bin\windows-x64\forja.exe run examples\01_ola.fj
.\bin\windows-x64\forja.exe spec --cartao      # o cartão de sintaxe / the syntax card
```

```odin
proc dobro(n: int, vezes: int = 2) -> int {
    return n * vezes
}

proc main() {
    print("Olá, Forja!")
    print(dobro(21))
    print(dobro(7, vezes = 3))
}
```

## O que tem aqui / What is here

| Pasta / Path | Conteúdo / Content |
|---|---|
| `bin/windows-x64/forja.exe` | o compilador / the compiler (`SHA256SUMS.txt` ao lado) |
| `language/forja_fj/` | **o código-fonte do compilador, em Forja** / the compiler source, in Forja |
| `language/doc/` | especificação, cartão de sintaxe, receitas / spec, syntax card, recipes |
| `language/std/`, `language/vendor/` | biblioteca padrão e ligações com bibliotecas C / standard library and C library wrappers |
| `examples/`, `language/exemplos_libs/` | programas de exemplo / example programs |
| `language/tests/`, `language/benchmark/` | testes e benchmark de IA / tests and AI benchmark |
| `language/semente/`, `language/semente_c/` | bootstrap do compilador / compiler bootstrap |

As bibliotecas C pré-compiladas (`.a`/`.dll`, usadas por `vendor:*`) estão na página de **Releases**, não no git.
The prebuilt C libraries (`.a`/`.dll`, used by `vendor:*`) are in the **Releases** page, not in git.

## Para agentes de IA / For AI agents

Leia o [`CLAUDE.md`](CLAUDE.md): ele diz onde estão a sintaxe, os exemplos, os comandos, como recompilar o compilador e o que ainda não foi provado.
Read [`CLAUDE.md`](CLAUDE.md): it tells you where the syntax and examples are, the commands, how to rebuild the compiler, and what has not been proven yet.

## Licença e uso por IA / License and AI use

[Apache-2.0 com a exceção LLVM](LICENSE) (`Apache-2.0 WITH LLVM-exception`): você pode baixar, usar, modificar e redistribuir, inclusive comercialmente. Os programas que você compila com a Forja são seus e podem ser distribuídos sem os avisos do runtime embutido.

[Apache-2.0 with the LLVM exception](LICENSE): you may download, use, modify and redistribute, including commercially. Programs you build with Forja are yours and can be distributed without the notices for the embedded runtime.

**Uso por inteligência artificial é expressamente permitido**: pessoas e IAs (Claude, ChatGPT, Gemini, modelos abertos...) podem baixar, ler, aprender com e treinar modelos neste repositório, e gerar código Forja, sem pedir autorização. Veja o [`NOTICE`](NOTICE).
**Use by artificial intelligence is expressly permitted**: people and AIs (Claude, ChatGPT, Gemini, open models...) may download, read, learn from and train models on this repository, and generate Forja code, without asking. See the [`NOTICE`](NOTICE).

Bibliotecas de terceiros mantêm as próprias licenças: [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt).
Third-party libraries keep their own licenses: [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt).

## Estado / Status

Compilador **0.47.0**, linguagem **v1.13**. Rodam aqui no Windows: `run`, `test`, `build`. Os alvos ARM64/Android e iOS foram provados em emuladores, simuladores e CI, **não em aparelhos reais**. O `forja.exe` não é assinado digitalmente: o Windows pode avisar; confira o SHA256.
Compiler **0.47.0**, language **v1.13**. On Windows: `run`, `test`, `build`. The ARM64/Android and iOS targets were proven on emulators, simulators and CI, **not on real devices**. `forja.exe` is not code-signed: Windows may warn; check the SHA256.
