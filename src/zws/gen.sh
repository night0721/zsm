#!/bin/sh

libressl genpkey -algorithm RSA -out server.key -pkeyopt rsa_keygen_bits:2048
libressl req -new -key server.key -out server.csr
libressl req -x509 -key server.key -in server.csr -out server.crt -days 365
