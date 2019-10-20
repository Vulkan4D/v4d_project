#!/bin/bash
cd `dirname $0`

# Specific branch for openssl
git submodule update --init src/openssl
cd src/openssl; git checkout OpenSSL_1_1_0-stable; git pull; cd ../..;

# Pull All submodules
git submodule update --remote --recursive --init
git submodule foreach --recursive git submodule update

git status
