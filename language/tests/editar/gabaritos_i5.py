"""gabaritos_i5.py -- o oraculo ESCRITO A MAO da bateria do P-IA1-I5 (tests\\editar\\verificar_editar_i5.py).

Nao saiu da saida do forja: cada entrada diz, lendo o fonte da fixture, o que o arquivo tem de ter depois da operacao. Forma: {arquivo: [(trecho antigo, trecho novo), ...]}; o verificador aplica os
pares em ordem sobre o texto da fixture (cada trecho antigo tem de aparecer UMA vez, senao o gabarito e que esta errado) e compara o RESULTADO inteiro, byte a byte, com o arquivo que o forja gravou.
Arquivo que nao esta na entrada tem de ficar intacto.
Fixtures: tests\\editar\\proj (main, geo/vetor, geo/forma, util/conta) e tests\\editar\\proj2 (main, calc/aritmetica, calc/formas, util/texto).
Versao: 1.0.0
Changelog: 1.0.0 - P-IA1-I5: primeira versao (10 renomear e 10 trocar da fixture proj; 10 inserir, 10 remover e 10 mover da fixture proj2).
Verificacao: python verificar_editar_i5.py --selftest
"""

# ---------------------------------------------------------------- proj: renomear (10) e trocar (10)

MAIN = "main.fj"
VETOR = "geo/vetor.fj"
FORMA = "geo/forma.fj"
CONTA = "util/conta.fj"

PROJ_RENOMEAR = {
    "geo/vetor:somar_vetor -> juntar_vetores": {
        VETOR: [("pub proc somar_vetor(a: Vetor, b: Vetor) -> Vetor {", "pub proc juntar_vetores(a: Vetor, b: Vetor) -> Vetor {")],
        MAIN: [("    let soma = vet.somar_vetor(a, b = b)\n", "    let soma = vet.juntar_vetores(a, b = b)\n")],
    },
    "comprimento_quadrado -> norma_quadrada": {
        VETOR: [("pub proc comprimento_quadrado(v: Vetor)", "pub proc norma_quadrada(v: Vetor)")],
        MAIN: [('    print(f"{vet.comprimento_quadrado(soma)}")\n', '    print(f"{vet.norma_quadrada(soma)}")\n')],
    },
    "Vetor -> Vec2": {
        VETOR: [("pub struct Vetor {", "pub struct Vec2 {"),
                ("pub proc somar_vetor(a: Vetor, b: Vetor) -> Vetor {", "pub proc somar_vetor(a: Vec2, b: Vec2) -> Vec2 {"),
                ("    return Vetor{ex = a.ex + b.ex, ey = a.ey + b.ey}", "    return Vec2{ex = a.ex + b.ex, ey = a.ey + b.ey}"),
                ("pub proc comprimento_quadrado(v: Vetor) -> f32 {", "pub proc comprimento_quadrado(v: Vec2) -> f32 {")],
        FORMA: [("    centro: vet.Vetor\n", "    centro: vet.Vec2\n")],
        MAIN: [("    let a = vet.Vetor{ex = 1.5, ey = 2.0}\n", "    let a = vet.Vec2{ex = 1.5, ey = 2.0}\n"), ("    let b = vet.Vetor{ex = 0.5, ey = 1.0}\n", "    let b = vet.Vec2{ex = 0.5, ey = 1.0}\n")],
    },
    "Vetor.ex -> px": {
        VETOR: [("    ex: f32 = 0.0\n", "    px: f32 = 0.0\n"),
                ("    return Vetor{ex = a.ex + b.ex, ey = a.ey + b.ey}", "    return Vetor{px = a.px + b.px, ey = a.ey + b.ey}"),
                ("    return v.ex * v.ex + v.ey * v.ey", "    return v.px * v.px + v.ey * v.ey")],
        FORMA: [("    let extra = f.centro.ex as i32\n", "    let extra = f.centro.px as i32\n")],
        MAIN: [("    let a = vet.Vetor{ex = 1.5, ey = 2.0}\n", "    let a = vet.Vetor{px = 1.5, ey = 2.0}\n"), ("    let b = vet.Vetor{ex = 0.5, ey = 1.0}\n", "    let b = vet.Vetor{px = 0.5, ey = 1.0}\n")],
    },
    "ESCALA -> FATOR": {
        VETOR: [("pub const ESCALA: i32 = 3", "pub const FATOR: i32 = 3")],
        MAIN: [("    let escalado = ct.dobro(vet.ESCALA)\n", "    let escalado = ct.dobro(vet.FATOR)\n")],
    },
    "Tipo.quadrado -> retangulo": {
        FORMA: [("pub enum Tipo { circulo, quadrado, linha }", "pub enum Tipo { circulo, retangulo, linha }"), ("    if f.tipo == Tipo.quadrado {\n", "    if f.tipo == Tipo.retangulo {\n")],
        MAIN: [("tipo = fm.Tipo.quadrado,", "tipo = fm.Tipo.retangulo,")],
    },
    "area_aprox -> calcular_area": {
        FORMA: [("pub proc area_aprox(f: Forma)", "pub proc calcular_area(f: Forma)")],
        MAIN: [("    print(fm.area_aprox(forma))\n", "    print(fm.calcular_area(forma))\n")],
    },
    "acumular -> somar_passos": {
        CONTA: [("pub proc acumular(inicio: i32", "pub proc somar_passos(inicio: i32")],
        MAIN: [("    print(ct.acumular(5, passo = 2, vezes = ct.dobro(2)))\n", "    print(ct.somar_passos(5, passo = 2, vezes = ct.dobro(2)))\n"),
               ("    expect(ct.acumular(1) == 4)\n", "    expect(ct.somar_passos(1) == 4)\n")],
    },
    "Placar.rodadas -> partidas": {
        MAIN: [("    rodadas: i32 = 0\n", "    partidas: i32 = 0\n"),
               ("    return Placar{pontos = novos, rodadas = p.rodadas + 1}\n", "    return Placar{pontos = novos, partidas = p.partidas + 1}\n"),
               ('    print(f"placar {placar.pontos} em {placar.rodadas}")\n', '    print(f"placar {placar.pontos} em {placar.partidas}")\n')],
    },
    "main.soma -> resultante": {
        MAIN: [("    let soma = vet.somar_vetor(a, b = b)\n", "    let resultante = vet.somar_vetor(a, b = b)\n"),
               ("centro = soma, peso = 2}", "centro = resultante, peso = 2}"),
               ("{vet.comprimento_quadrado(soma)}", "{vet.comprimento_quadrado(resultante)}")],
    },
}

PROJ_TROCAR = {
    "trocar_no main:atualizar/corpo[0]/valor": {MAIN: [("    let novos = p.pontos + ganho\n", "    let novos = ganho + p.pontos\n")]},
    "trocar_no geo/forma:area_aprox/corpo[4]/valor": {FORMA: [("    return base * f.peso + extra\n", "    return extra + f.peso * base\n")]},
    "trocar_no util/conta:dobro/corpo[0]/valor": {CONTA: [("    return n * 2\n", "    return n + n\n")]},
    "trocar_no main:main/corpo[12]/valor": {MAIN: [("    let escalado = ct.dobro(vet.ESCALA)\n", "    let escalado = vet.ESCALA * 2\n")]},
    "trocar_no geo/forma:area_aprox/corpo[1]": {FORMA: [("        base = 4\n", "        base = 2 + 2\n")]},
    "trocar_corpo util/conta:dobro": {CONTA: [("    return n * 2\n", "    return n + n\n")]},
    "trocar_corpo geo/vetor:comprimento_quadrado": {VETOR: [("    return v.ex * v.ex + v.ey * v.ey\n", "    let xx = v.ex * v.ex\n    let yy = v.ey * v.ey\n    return xx + yy\n")]},
    "trocar_corpo main:nota_da_faixa": {MAIN: [("    if f == Faixa.alta {\n        return 3\n    }\n    if f == Faixa.media {\n        return 2\n    }\n    return 1\n",
                                                 "    var nota: i32 = 1\n    if f == Faixa.media {\n        nota = 2\n    }\n    if f == Faixa.alta {\n        nota = 3\n    }\n    return nota\n")]},
    "trocar_corpo main:test:acumular": {MAIN: [("    expect(ct.acumular(1) == 4)\n", "    expect(ct.acumular(1, passo = 1, vezes = 3) == 4)\n")]},
    "trocar_corpo geo/forma:area_aprox": {FORMA: [("    let extra = f.centro.ex as i32\n    return base * f.peso + extra\n", "    return base * f.peso + f.centro.ex as i32\n")]},
}

# ---------------------------------------------------------------- proj2

ARIT = "calc/aritmetica.fj"
FORMAS = "calc/formas.fj"
TEXTO = "util/texto.fj"
MAIN2 = "main.fj"

CABECALHO_TEXTO = "procs de texto sem dependencia de outro modulo.\n\n// Cerca"
FIM_DO_TEXTO = "    simbolo: i32 = 45\n}\n"
FIM_DA_ARIT = "    return somar(p.esq, b = p.dir)\n}\n"
FIM_DO_MAIN = "    expect(fo.area(fo.Retangulo{largura = 2, altura = 5}) == 10)\n}\n"

INSERIR = {
    "depois_de somar (aritmetica)": {ARIT: [(
        "    return a + b\n}\n\n// Dobra o valor",
        "    return a + b\n}\n\n// Subtrai dois inteiros.\npub proc subtrair(a: i32, b: i32) -> i32 {\n    return a - b\n}\n\n// Dobra o valor")]},
    "antes_de dobrar, com comentario colado (aritmetica)": {ARIT: [(
        "}\n\n// Dobra o valor sem passar do limite.\n",
        "}\n\npub proc metade(n: i32) -> i32 {\n    return n / 2\n}\n\n// Dobra o valor sem passar do limite.\n")]},
    "no_fim (aritmetica)": {ARIT: [(FIM_DA_ARIT, FIM_DA_ARIT + "\npub const TETO: i32 = 1000\n")]},
    "no_inicio_depois_dos_imports (formas)": {FORMAS: [(
        'import "calc/aritmetica" as ar\n\npub enum Cor',
        'import "calc/aritmetica" as ar\n\npub const LADOS: i32 = 4\n\npub enum Cor')]},
    "no_inicio_depois_dos_imports sem import (texto)": {TEXTO: [(CABECALHO_TEXTO, "procs de texto sem dependencia de outro modulo.\n\npub const SEPARADOR: i32 = 45\n\n// Cerca")]},
    "no_fim no arquivo raiz (main)": {MAIN2: [(FIM_DO_MAIN, FIM_DO_MAIN + "\nproc extra() -> i32 {\n    return 7\n}\n")]},
    "depois_de Retangulo (struct, formas)": {FORMAS: [(
        "    cor: Cor = Cor.vermelho\n}\n\n// Ponto no plano",
        "    cor: Cor = Cor.vermelho\n}\n\npub struct Ponto3 {\n    x: i32 = 0\n    y: i32 = 0\n    z: i32 = 0\n}\n\n// Ponto no plano")]},
    "antes_de Par, sem comentario (aritmetica)": {ARIT: [(
        "pub enum Modo { rapido, lento }\n\npub struct Par {",
        "pub enum Modo { rapido, lento }\n\npub enum Sinal { menos, mais }\n\npub struct Par {")]},
    "no_fim pelo arquivo util/texto.fj": {TEXTO: [(FIM_DO_TEXTO, FIM_DO_TEXTO + "\npub proc eco(n: i32) -> i32 {\n    return n\n}\n")]},
    "duas insercoes em pontos diferentes (texto)": {TEXTO: [
        ("    return s\n}\n\n// Quantas casas", "    return s\n}\n\npub const A1: i32 = 1\n\n// Quantas casas"),
        ("    return total as i32\n}\n\npub struct Marca", "    return total as i32\n}\n\npub const A2: i32 = 2\n\npub struct Marca")]},
}

REMOVER = {
    "obsoleta (pub, comentario colado, meio do arquivo)": {ARIT: [("// Ninguem usa esta.\npub proc obsoleta(n: i32) -> i32 {\n    return n - 1\n}\n\n", "")]},
    "moldura (primeira declaracao depois do cabecalho)": {TEXTO: [("// Cerca o texto de colchetes.\npub proc moldura(s: str) -> str {\n    if s == \"\" {\n        return \"vazio\"\n    }\n    return s\n}\n\n", "")]},
    "sobra (ultima declaracao do arquivo)": {FORMAS: [("    return 1\n}\n\n// Sem uso.\nproc sobra(r: Retangulo) -> i32 {\n    return r.altura\n}\n", "    return 1\n}\n")]},
    "MINIMO (const)": {ARIT: [("// Piso do limite; sem uso.\npub const MINIMO: i32 = 0\n\n", "")]},
    "Modo (enum)": {ARIT: [("// Modo de calculo; sem uso.\npub enum Modo { rapido, lento }\n\n", "")]},
    "Ponto (struct)": {FORMAS: [("// Ponto no plano; sem uso.\npub struct Ponto {\n    x: i32 = 0\n    y: i32 = 0\n}\n\n", "")]},
    "dobro_area (o import ar continua em uso)": {FORMAS: [("// Dobro da area; sem uso.\npub proc dobro_area(r: Retangulo) -> i32 {\n    return ar.somar(area(r), b = area(r))\n}\n\n", "")]},
    "legenda (o import tx fica sem uso e sai)": {MAIN2: [('import "util/texto" as tx\n', ""),
                                                       ("// Casas de um numero; sem uso.\nproc legenda(n: i32) -> i32 {\n    return tx.casas(n)\n}\n\n", "")]},
    "Marca (ultima, sem comentario)": {TEXTO: [("    return total as i32\n}\n\npub struct Marca {\n    simbolo: i32 = 45\n}\n", "    return total as i32\n}\n")]},
    "PASSOS (const logo depois dos imports)": {MAIN2: [("const PASSOS: i32 = 3\n\n", "")]},
}

COM_DOBRAR = "// Dobra o valor sem passar do limite.\npub proc dobrar(n: i32) -> i32 {\n    return limitar(n * 2)\n}\n\n"
IMPORT_ARIT_NO_TEXTO = (CABECALHO_TEXTO, 'procs de texto sem dependencia de outro modulo.\n\nimport "calc/aritmetica"\n\n// Cerca')
CAB_ARIT = "usam por apelido.\n\npub const LIMITE"

MOVER = {
    "dobrar: calc/aritmetica -> util/texto": {
        ARIT: [(COM_DOBRAR, ""), ("\nproc limitar(n: i32) -> i32 {", "\npub proc limitar(n: i32) -> i32 {")],
        FORMAS: [('import "calc/aritmetica" as ar\n\npub enum', 'import "calc/aritmetica" as ar\nimport "util/texto"\n\npub enum'),
                 ("    return ar.dobrar(r.largura + r.altura)\n", "    return texto.dobrar(r.largura + r.altura)\n")],
        MAIN2: [("    print(ar.dobrar(60))\n", "    print(tx.dobrar(60))\n")],
        TEXTO: [IMPORT_ARIT_NO_TEXTO,
                (FIM_DO_TEXTO, FIM_DO_TEXTO + "\n// Dobra o valor sem passar do limite.\npub proc dobrar(n: i32) -> i32 {\n    return aritmetica.limitar(n * 2)\n}\n")],
    },
    "triplicar: calc/aritmetica -> main (raiz)": {
        ARIT: [("// Triplica o valor; so o main usa.\npub proc triplicar(n: i32) -> i32 {\n    return n * 3\n}\n\n", "")],
        MAIN2: [("    print(ar.triplicar(7))\n", "    print(triplicar(7))\n"),
                (FIM_DO_MAIN, FIM_DO_MAIN + "\n// Triplica o valor; so o main usa.\npub proc triplicar(n: i32) -> i32 {\n    return n * 3\n}\n")],
    },
    "somar_par: calc/aritmetica -> util/texto": {
        ARIT: [("}\n\npub proc somar_par(p: Par) -> i32 {\n    return somar(p.esq, b = p.dir)\n}\n", "}\n")],
        MAIN2: [("    return ar.somar_par(par)\n", "    return tx.somar_par(par)\n")],
        TEXTO: [IMPORT_ARIT_NO_TEXTO,
                (FIM_DO_TEXTO, FIM_DO_TEXTO + "\npub proc somar_par(p: aritmetica.Par) -> i32 {\n    return aritmetica.somar(p.esq, b = p.dir)\n}\n")],
    },
    "Par: calc/aritmetica -> util/texto": {
        ARIT: [("pub struct Par {\n    esq: i32 = 0\n    dir: i32 = 0\n}\n\n", ""), (CAB_ARIT, 'usam por apelido.\n\nimport "util/texto"\n\npub const LIMITE'),
               ("pub proc somar_par(p: Par) -> i32 {", "pub proc somar_par(p: texto.Par) -> i32 {")],
        MAIN2: [("proc rotulo(par: ar.Par) -> i32 {", "proc rotulo(par: tx.Par) -> i32 {"), ("    print(rotulo(ar.Par{esq = 2, dir = 5}))\n", "    print(rotulo(tx.Par{esq = 2, dir = 5}))\n")],
        TEXTO: [(FIM_DO_TEXTO, FIM_DO_TEXTO + "\npub struct Par {\n    esq: i32 = 0\n    dir: i32 = 0\n}\n")],
    },
    "Cor: calc/formas -> calc/aritmetica": {
        FORMAS: [("pub enum Cor { vermelho, verde, azul }\n\n", ""), ("    cor: Cor = Cor.vermelho\n", "    cor: ar.Cor = ar.Cor.vermelho\n"),
                 ("pub proc codigo_da_cor(c: Cor) -> i32 {", "pub proc codigo_da_cor(c: ar.Cor) -> i32 {"),
                 ("    if c == Cor.verde {\n", "    if c == ar.Cor.verde {\n"), ("    if c == Cor.azul {\n", "    if c == ar.Cor.azul {\n")],
        MAIN2: [("cor = fo.Cor.verde}", "cor = ar.Cor.verde}"), ("    print(fo.codigo_da_cor(fo.Cor.azul))\n", "    print(fo.codigo_da_cor(ar.Cor.azul))\n")],
        ARIT: [(FIM_DA_ARIT, FIM_DA_ARIT + "\npub enum Cor { vermelho, verde, azul }\n")],
    },
    "LIMITE: calc/aritmetica -> util/texto": {
        ARIT: [("pub const LIMITE: i32 = 100\n\n", ""), (CAB_ARIT.replace("\n\npub const LIMITE", "\n\n// Piso do limite"), 'usam por apelido.\n\nimport "util/texto"\n\n// Piso do limite'),
               ("    if n > LIMITE {\n        return LIMITE\n", "    if n > texto.LIMITE {\n        return texto.LIMITE\n")],
        MAIN2: [("    print(ar.LIMITE)\n", "    print(tx.LIMITE)\n")],
        TEXTO: [(FIM_DO_TEXTO, FIM_DO_TEXTO + "\npub const LIMITE: i32 = 100\n")],
    },
    "legenda: main -> util/texto (o import tx sai do main)": {
        MAIN2: [('import "util/texto" as tx\n', ""), ("// Casas de um numero; sem uso.\nproc legenda(n: i32) -> i32 {\n    return tx.casas(n)\n}\n\n", "")],
        TEXTO: [(FIM_DO_TEXTO, FIM_DO_TEXTO + "\n// Casas de um numero; sem uso.\nproc legenda(n: i32) -> i32 {\n    return casas(n)\n}\n")],
    },
    "resumo: main -> calc/formas": {
        MAIN2: [("// Resumo de um retangulo: area mais um.\nproc resumo(r: fo.Retangulo) -> i32 {\n    return fo.area(r) + ar.somar(0, b = 1)\n}\n\n", ""),
                ("    return resumo(a) + resumo(b)\n", "    return fo.resumo(a) + fo.resumo(b)\n"),
                ('    print(f"resumo {resumo(r)} perimetro {fo.perimetro(r)}")\n', '    print(f"resumo {fo.resumo(r)} perimetro {fo.perimetro(r)}")\n')],
        FORMAS: [("    return r.altura\n}\n", "    return r.altura\n}\n\n// Resumo de um retangulo: area mais um.\npub proc resumo(r: Retangulo) -> i32 {\n    return area(r) + ar.somar(0, b = 1)\n}\n")],
    },
    "rotulo: main -> calc/aritmetica": {
        MAIN2: [("proc rotulo(par: ar.Par) -> i32 {\n    return ar.somar_par(par)\n}\n\n", ""),
                ("    print(rotulo(ar.Par{esq = 2, dir = 5}))\n", "    print(ar.rotulo(ar.Par{esq = 2, dir = 5}))\n")],
        ARIT: [(FIM_DA_ARIT, FIM_DA_ARIT + "\npub proc rotulo(par: Par) -> i32 {\n    return somar_par(par)\n}\n")],
    },
    "codigo_da_cor: calc/formas -> util/texto (texto passa a importar formas)": {
        FORMAS: [("// Valor numerico da cor.\npub proc codigo_da_cor(c: Cor) -> i32 {\n    if c == Cor.verde {\n        return 2\n    }\n    if c == Cor.azul {\n        return 3\n    }\n    return 1\n}\n\n", "")],
        MAIN2: [("    print(fo.codigo_da_cor(fo.Cor.azul))\n", "    print(tx.codigo_da_cor(fo.Cor.azul))\n")],
        TEXTO: [(CABECALHO_TEXTO, 'procs de texto sem dependencia de outro modulo.\n\nimport "calc/formas"\n\n// Cerca'),
                (FIM_DO_TEXTO, FIM_DO_TEXTO + "\n// Valor numerico da cor.\npub proc codigo_da_cor(c: formas.Cor) -> i32 {\n    if c == formas.Cor.verde {\n        return 2\n    }\n    if c == formas.Cor.azul {\n        return 3\n    }\n    return 1\n}\n")],
    },
}
