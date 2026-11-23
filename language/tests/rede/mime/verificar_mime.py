#!/usr/bin/env python3
# verificar_mime.py 0.1.0 - prova do MIME de vendor:net/mail por outro parser (email da stdlib), marco rede R6.
# Uso: python -I tests\rede\mime\verificar_mime.py   (saida 0 = OK). Depende de: nada (stdlib). Le os dourados caso_N.eml desta pasta.
# Confere: CRLF em todo LF; caso 2 (assunto RFC 2047 e texto acentuado); caso 3 (multipart/alternative); caso 4 (1 anexo = bytes originais);
# caso 5 (2 anexos com nome RFC 2231, conteudo igual); Date e Message-ID do caso 4.
import email
import email.policy
import os
import sys

PastaMime = os.path.dirname(os.path.abspath(__file__))
PolicyPadrao = email.policy.default


def LerDourado(Nome):
    with open(os.path.join(PastaMime, Nome), "rb") as Arquivo:
        return Arquivo.read()


def ParsearMensagem(Bruto):
    return email.message_from_bytes(Bruto, policy=PolicyPadrao)


def ProvarCrlf(Bruto):
    return Bruto.count(b"\n") == Bruto.count(b"\r\n")


def ProvarAssuntoEAcento(Bruto):
    Msg = ParsearMensagem(Bruto)
    Corpo = Msg.get_body(preferencelist=("plain",))
    Texto = Corpo.get_payload(decode=True).decode("utf-8")
    return str(Msg["Subject"]) == "Olá, mundo" and Texto == "Açúcar e ação\r\n"


def ProvarAlternativo(Bruto):
    Msg = ParsearMensagem(Bruto)
    return Msg.get_content_type() == "multipart/alternative" and Msg.get_body(preferencelist=("html",)) is not None


def ProvarUmAnexo(Bruto, Original):
    Msg = ParsearMensagem(Bruto)
    Anexos = list(Msg.iter_attachments())
    return (len(Anexos) == 1 and Anexos[0].get_filename() == "dados.bin"
            and Anexos[0].get_payload(decode=True) == Original)


def ProvarDoisAnexos(Bruto, Original):
    Msg = ParsearMensagem(Bruto)
    Anexos = list(Msg.iter_attachments())
    Nomes = [Anexo.get_filename() for Anexo in Anexos]
    Conteudos = [Anexo.get_payload(decode=True) for Anexo in Anexos]
    return (Nomes == ["relatório.txt", "dados-ção.bin"]
            and Conteudos == ["Relatório de março\n".encode("utf-8"), Original])


def ProvarCabecalhosDoCaso4(Bruto):
    Msg = ParsearMensagem(Bruto)
    return (str(Msg["Date"]) == "Wed, 18 Nov 2026 18:05:00 -0300"
            and str(Msg["Message-ID"]) == "<20261118210500.fronteira-4@forja.local>")


def Principal():
    Original = LerDourado("anexo_original.bin")
    Falhas = []
    for Numero in range(1, 6):
        Bruto = LerDourado(f"caso_{Numero}.eml")
        if not ProvarCrlf(Bruto):
            Falhas.append(f"caso_{Numero} tem LF sem CR")
    if not ProvarAssuntoEAcento(LerDourado("caso_2.eml")):
        Falhas.append("caso_2 assunto ou texto acentuado")
    if not ProvarAlternativo(LerDourado("caso_3.eml")):
        Falhas.append("caso_3 multipart/alternative")
    if not ProvarUmAnexo(LerDourado("caso_4.eml"), Original):
        Falhas.append("caso_4 anexo diferente do original")
    if not ProvarCabecalhosDoCaso4(LerDourado("caso_4.eml")):
        Falhas.append("caso_4 Date ou Message-ID")
    if not ProvarDoisAnexos(LerDourado("caso_5.eml"), Original):
        Falhas.append("caso_5 nomes RFC 2231 ou conteudo dos anexos")
    if Falhas:
        print("mime python FALHOU", Falhas)
        return 1
    print("mime python OK (CRLF, RFC 2047, alternative, 1 anexo e 2 anexos RFC 2231 conferidos pelo email da stdlib)")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
