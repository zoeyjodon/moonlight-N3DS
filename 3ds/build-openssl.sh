#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
OPENSSL_DIR=$ROOT_DIR/third_party/openssl
OPENSSL_PATCH=openssl-3ds.patch


cd $OPENSSL_DIR

if ! [ -f $OPENSSL_DIR/$OPENSSL_PATCH ]; then
    cp $SCRIPT_DIR/$OPENSSL_PATCH $OPENSSL_DIR/$OPENSSL_PATCH
    git apply $OPENSSL_PATCH
    ./Configure 3ds \
        no-threads no-shared no-asm no-ui-console no-unit-test no-tests no-buildtest-c++ no-external-tests no-autoload-config \
        --with-rand-seed=os -static
fi

if ! [ -f $OPENSSL_DIR/include/openssl/x509v3.h ]; then
    make build_generated
fi

make libssl.a libcrypto.a

# Install the library files
cp libssl.a $DEVKITPRO/portlibs/3ds/lib/
cp libcrypto.a $DEVKITPRO/portlibs/3ds/lib/
cp -r include/openssl $DEVKITPRO/portlibs/3ds/include/openssl/
cp -r include/crypto $DEVKITPRO/portlibs/3ds/include/crypto/

cd -
