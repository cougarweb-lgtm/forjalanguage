#!/bin/sh
# gerar_certs.sh 0.1.0 - gera a CA de teste e os certificados do servidor de mentira tests/rede/servidor_http.py --tls (marco rede R5).
# Uso (Git Bash, openssl do Git for Windows): PATH="/c/Program Files/Git/usr/bin:$PATH" OPENSSL_CONF="C:/Program Files/Git/usr/ssl/openssl.cnf" sh tests/rede/certs/gerar_certs.sh
# (o OPENSSL_CONF e preciso quando outro openssl do PC aponta o seu cfg por variavel de ambiente.)
# Saida nesta pasta: ca.pem (CA de teste, 20 anos), servidor.pem + servidor.key (SAN IP:127.0.0.1 e DNS:localhost),
# outro.pem (SAN DNS:outro.invalid, mesma CA e mesma chave de servidor.key: so o nome muda, para provar a recusa por SAN).
# A chave da CA nao fica no repositorio: vai para pasta temporaria e e apagada ao sair. Uso so em teste, nunca em producao.
set -eu
Pasta=$(cd "$(dirname "$0")" && pwd)
Temp=$(mktemp -d)
trap 'rm -rf "$Temp"' EXIT

printf '%s\n' "basicConstraints=CA:FALSE" "keyUsage=critical,digitalSignature,keyEncipherment" \
    "extendedKeyUsage=serverAuth" "subjectAltName=IP:127.0.0.1,DNS:localhost" > "$Temp/servidor.ext"
printf '%s\n' "basicConstraints=CA:FALSE" "keyUsage=critical,digitalSignature,keyEncipherment" \
    "extendedKeyUsage=serverAuth" "subjectAltName=DNS:outro.invalid" > "$Temp/outro.ext"

# CA raiz de teste: 7305 dias = ~20 anos.
openssl req -x509 -newkey rsa:2048 -sha256 -days 7305 -nodes \
    -keyout "$Temp/ca.key" -out "$Pasta/ca.pem" -subj "/CN=Forja CA de teste (rede)" \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign"

# Servidor: chave RSA 2048 + certificado assinado pela CA de teste.
openssl req -new -newkey rsa:2048 -sha256 -nodes \
    -keyout "$Pasta/servidor.key" -out "$Temp/servidor.csr" -subj "/CN=127.0.0.1"
openssl x509 -req -sha256 -in "$Temp/servidor.csr" -CA "$Pasta/ca.pem" -CAkey "$Temp/ca.key" \
    -CAserial "$Temp/ca.srl" -CAcreateserial -days 7305 -extfile "$Temp/servidor.ext" -out "$Pasta/servidor.pem"

# Outro: mesma CA e mesma chave, nome que nao bate com 127.0.0.1 nem localhost.
openssl req -new -sha256 -key "$Pasta/servidor.key" -out "$Temp/outro.csr" -subj "/CN=outro.invalid"
openssl x509 -req -sha256 -in "$Temp/outro.csr" -CA "$Pasta/ca.pem" -CAkey "$Temp/ca.key" \
    -CAserial "$Temp/ca.srl" -CAcreateserial -days 7305 -extfile "$Temp/outro.ext" -out "$Pasta/outro.pem"

openssl verify -CAfile "$Pasta/ca.pem" "$Pasta/servidor.pem" "$Pasta/outro.pem"
