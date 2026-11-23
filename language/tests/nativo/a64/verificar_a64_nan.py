#!/usr/bin/env python3
"""verificar_a64_nan.py - normalizacao da classe aceita `nan_bits` do verificar_a64 (sem WSL, sem compilador).

Motivo: o sinal do NaN e do hardware. x86 produz 0xfff8000000000000 (f64) e 0xffc00000 (f32); ARM produz
0x7ff8000000000000 e 0x7fc00000. So esses quatro tokens inteiros sao trocados no stdout dos dois lados; o harness aceita
so quando, depois da troca, stdout, exit e stderr sao iguais (a troca nunca vale para outro numero: o token precisa
ter caractere fora de [0-9a-fA-F] antes e depois).
Uso: importado pelo verificar_a64.py. Prova pura: python -I verificar_a64.py --selftest (provas em verificar_a64_selftest.py).
Versao: 1.0.0 (22/11/2026) A4.5 - classe nan_bits (3a vaga de divergencia aceita).
"""
import re

REGEX_NAN = re.compile(rb"(?<![0-9a-fA-F])(fff8000000000000|7ff8000000000000|ffc00000|7fc00000)(?![0-9a-fA-F])")
FORMA_CANONICA = {  # NaN com sinal -> NaN positivo do mesmo tamanho; os positivos ficam como estao
    b"fff8000000000000": b"7ff8000000000000",
    b"7ff8000000000000": b"7ff8000000000000",
    b"ffc00000": b"7fc00000",
    b"7fc00000": b"7fc00000",
}


def NormalizarNan(Texto: bytes) -> bytes:
    """Troca cada token de NaN pela forma positiva (f64 7ff8000000000000, f32 7fc00000); o resto do texto nao muda."""
    return REGEX_NAN.sub(lambda Casa: FORMA_CANONICA[Casa.group(1)], Texto)


def SaidaSoNan(Referencia: bytes, Obtido: bytes) -> bool:
    """True so se os stdouts diferem e, depois da normalizacao, sao iguais (a unica diferenca e o sinal de um NaN)."""
    return Referencia != Obtido and NormalizarNan(Referencia) == NormalizarNan(Obtido)
