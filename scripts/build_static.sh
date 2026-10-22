#!/bin/sh
# Build a fully static minima binary.
#
# The distro's static libcurl pulls a large set of optional static deps (GSSAPI,
# LDAP, SSH, ...), so we build a minimal static libcurl (OpenSSL + zlib only)
# and link everything statically. Requires: build tools, libssl-dev,
# libsqlite3-dev, zlib1g-dev, libboost-dev, curl (to fetch the tarball).
set -eu

CURL_VER=${CURL_VER:-8.10.1}
PREFIX=${CURL_PREFIX:-/tmp/curl-static}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
JOBS=$(nproc 2>/dev/null || echo 2)

MULTIARCH=$(gcc -print-multiarch 2>/dev/null || true)
[ -n "$MULTIARCH" ] || MULTIARCH=$(uname -m)-linux-gnu
LIBROOT="/usr/lib/$MULTIARCH"

if [ ! -f "$PREFIX/lib/libcurl.a" ]; then
    TMP=$(mktemp -d)
    (
        cd "$TMP"
        curl -fsSLO "https://curl.se/download/curl-$CURL_VER.tar.gz"
        tar xf "curl-$CURL_VER.tar.gz"
        cd "curl-$CURL_VER"
        ./configure --prefix="$PREFIX" --disable-shared --enable-static \
            --without-libidn2 --without-nghttp2 --without-librtmp --without-libssh2 \
            --without-brotli --without-zstd --without-libpsl \
            --disable-ldap --disable-ldaps --without-gssapi \
            --with-openssl --with-zlib
        make -j"$JOBS"
        make install
    )
    rm -rf "$TMP"
fi

if [ -f "$LIBROOT/liblmdb.a" ]; then
    LMDB_OPT="-DLMDB_LIBRARY=$LIBROOT/liblmdb.a"
else
    LMDB_OPT="-DMINIMA_STATIC_NO_LMDB=ON"
fi

cmake -S "$ROOT" -B "$ROOT/build-static" \
    -DCMAKE_BUILD_TYPE=Release \
    -DMINIMA_STATIC=ON \
    $LMDB_OPT \
    -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
    -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=OFF \
    -DSQLite3_LIBRARY="$LIBROOT/libsqlite3.a" \
    -DOPENSSL_SSL_LIBRARY="$LIBROOT/libssl.a" \
    -DOPENSSL_CRYPTO_LIBRARY="$LIBROOT/libcrypto.a" \
    -DZLIB_LIBRARY="$LIBROOT/libz.a" \
    -DCURL_LIBRARY="$PREFIX/lib/libcurl.a" \
    -DCURL_INCLUDE_DIR="$PREFIX/include" \
    -DCMAKE_CXX_FLAGS="-DCURL_STATICLIB"

cmake --build "$ROOT/build-static" --target minima -j"$JOBS"
file "$ROOT/build-static/minima"
