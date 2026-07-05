#!/bin/bash

# Script para compilar o projeto NKE ESP32C3
# Resolve problemas com caminhos contendo espaços

cd "$(dirname "$0")/src"

export MDK="$(cd ../mdk && pwd)"
export ARCH="esp32c3"
export PORT="/dev/ttyACM0"

make "$@"
