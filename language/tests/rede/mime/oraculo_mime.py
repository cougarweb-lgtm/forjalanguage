# Oraculo independente dos dourados MIME (R6): monta as 5 mensagens com as mesmas regras do mail.fj
# e grava tests\rede\mime\caso_N.eml + anexo_original.bin. Roda com: python -I OraculoMime.py
import base64
import datetime
import os
import urllib.parse

PastaMime = r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language\tests\rede\mime"
CRLF = b"\r\n"
ANEXO = bytes(range(256)) * 3
DIAS = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
MESES = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]


def AsciiPuro(Dados):
    return all(Byte < 128 for Byte in Dados)


def LinhasBase64(Dados):
    Codigo = base64.b64encode(Dados)
    Saida = bytearray()
    for Inicio in range(0, len(Codigo), 76):
        Saida += Codigo[Inicio:Inicio + 76] + CRLF
    return bytes(Saida)


def NormalizarQuebras(Dados):
    Saida = bytearray()
    for Indice, Byte in enumerate(Dados):
        if Byte == 10 and (Indice == 0 or Dados[Indice - 1] != 13):
            Saida.append(13)
        Saida.append(Byte)
    if len(Saida) > 0 and Saida[-1] != 10:
        Saida += b"\r\n"
    return bytes(Saida)


def PalavraRfc2047(Texto):
    Dados = Texto.encode("utf-8")
    if AsciiPuro(Dados):
        return Dados
    return b"=?UTF-8?B?" + base64.b64encode(Dados) + b"?="


def ParametroRfc2231(Nome, Valor):
    Dados = Valor.encode("utf-8")
    if AsciiPuro(Dados) and b'"' not in Dados and b"\\" not in Dados:
        return b"; " + Nome.encode() + b'="' + Dados + b'"'
    return b"; " + Nome.encode() + b"*=UTF-8''" + urllib.parse.quote(Dados, safe="-._~").encode()


def DataRfc():
    Dt = datetime.datetime(2026, 11, 18, 18, 5, 0)  # 21:05 UTC menos 3 h
    Txt = f"{DIAS[Dt.weekday()]}, {Dt.day:02} {MESES[Dt.month - 1]} {Dt.year:04} {Dt.hour:02}:{Dt.minute:02}:{Dt.second:02} -0300"
    return Txt.encode()


def IdDerivado(Limite):
    return b"<20261118210500." + Limite + b"@forja.local>"


def ParteTexto(Tipo, Corpo):
    Normal = NormalizarQuebras(Corpo.encode("utf-8"))
    Saida = b"Content-Type: " + Tipo + b"; charset=UTF-8\r\n"
    if AsciiPuro(Normal):
        return Saida + b"Content-Transfer-Encoding: 7bit\r\n\r\n" + Normal
    return Saida + b"Content-Transfer-Encoding: base64\r\n\r\n" + LinhasBase64(Normal)


def ParteCorpo(Msg, Limite):
    if Msg["text"] and Msg["html"]:
        Saida = b'Content-Type: multipart/alternative; boundary="' + Limite + b'"\r\n\r\n'
        Saida += b"--" + Limite + CRLF + ParteTexto(b"text/plain", Msg["text"])
        Saida += b"--" + Limite + CRLF + ParteTexto(b"text/html", Msg["html"])
        return Saida + b"--" + Limite + b"--\r\n"
    if Msg["html"]:
        return ParteTexto(b"text/html", Msg["html"])
    return ParteTexto(b"text/plain", Msg["text"])


def Misto(Msg, Limite):
    Saida = b'Content-Type: multipart/mixed; boundary="' + Limite + b'"\r\n\r\n'
    Saida += b"--" + Limite + CRLF
    Saida += ParteCorpo(Msg, Limite + b"-alt")
    for Nome, Ct, Dados in Msg["atts"]:
        Saida += b"--" + Limite + CRLF
        Saida += b"Content-Type: " + Ct + ParametroRfc2231("name", Nome) + CRLF
        Saida += b"Content-Transfer-Encoding: base64\r\n"
        Saida += b"Content-Disposition: attachment" + ParametroRfc2231("filename", Nome) + CRLF
        Saida += CRLF + LinhasBase64(Dados)
    return Saida + b"--" + Limite + b"--\r\n"


def Montar(Msg):
    Cab = b"Date: " + DataRfc() + CRLF
    Cab += b"From: " + Msg["from"] + CRLF
    Cab += b"To: " + Msg["to"] + CRLF
    if Msg.get("cc"):
        Cab += b"Cc: " + Msg["cc"] + CRLF
    Cab += b"Subject: " + PalavraRfc2047(Msg["subject"]) + CRLF
    Cab += b"Message-ID: " + (Msg["id"] or IdDerivado(Msg["boundary"])) + CRLF
    Cab += b"MIME-Version: 1.0" + CRLF
    if Msg["atts"]:
        return Cab + Misto(Msg, Msg["boundary"])
    return Cab + ParteCorpo(Msg, Msg["boundary"])


BASE = {"from": b"remetente@exemplo.com", "to": b"destino@exemplo.com", "html": "", "atts": [], "cc": None, "id": b""}

CASOS = {
    1: dict(BASE, subject="Teste simples", text="Linha 1\nLinha 2\n", boundary=b"fronteira-1"),
    2: dict(BASE, subject="Olá, mundo", text="Açúcar e ação\n", id=b"<teste-2@exemplo.com>", boundary=b"fronteira-2"),
    3: dict(BASE, subject="Texto e HTML", text="Oi\n", html="<p>Oi <b>mundo</b></p>\n", id=b"<teste-3@exemplo.com>", boundary=b"fronteira-3"),
    4: dict(BASE, subject="Anexo", text="Segue anexo.\n", cc=b"copia@exemplo.com", atts=[("dados.bin", b"application/octet-stream", ANEXO)], boundary=b"fronteira-4"),
    5: dict(BASE, subject="Dois anexos", text="Veja os anexos.\n", id=b"<teste-5@exemplo.com>",
            atts=[("relatório.txt", b"text/plain", "Relatório de março\n".encode("utf-8")),
                  ("dados-ção.bin", b"application/octet-stream", ANEXO)], boundary=b"fronteira-5"),
}

if __name__ == "__main__":
    os.makedirs(PastaMime, exist_ok=True)
    with open(os.path.join(PastaMime, "anexo_original.bin"), "wb") as Arquivo:
        Arquivo.write(ANEXO)
    for Numero, Msg in CASOS.items():
        Montado = Montar(Msg)
        with open(os.path.join(PastaMime, f"caso_{Numero}.eml"), "wb") as Arquivo:
            Arquivo.write(Montado)
        print("caso", Numero, len(Montado), "bytes")
