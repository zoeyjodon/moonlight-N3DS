#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
OPENSSL_DIR=$ROOT_DIR/third_party/openssl


cd $OPENSSL_DIR

./Configure 3ds \
    no-threads no-shared no-asm no-ui-console no-unit-test no-tests no-buildtest-c++ no-external-tests no-autoload-config \
    --with-rand-seed=os -static -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion
make build_generated -j$(nproc)
make libssl.a libcrypto.a -j$(nproc)

# Install the library files
cp libssl.a $DEVKITPRO/portlibs/3ds/lib/
cp libcrypto.a $DEVKITPRO/portlibs/3ds/lib/
cp -r include/openssl $DEVKITPRO/portlibs/3ds/include/openssl/
cp -r include/crypto $DEVKITPRO/portlibs/3ds/include/crypto/

cd -
