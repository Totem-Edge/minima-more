#!/bin/sh
# Shared path/env defaults for the reference-bench scripts. Source this file.
# Resolve paths relative to this script, not the caller's cwd.
_BENCH_DIR=$(cd "$(dirname "$0")/.." && pwd)          # reference-bench/
PROJECT_DIR=$(cd "$_BENCH_DIR/.." && pwd)             # this C++ project
CRYSTAL_LABS=$(cd "$PROJECT_DIR/../.." && pwd)        # CRYSTAL_LABS/

MINIMA_CORE=${MINIMA_CORE:-$CRYSTAL_LABS/reference/minima-core}
MC_BUILD=${MC_BUILD:-/tmp/minima-core-build}
MC_BENCH_OUT=${MC_BENCH_OUT:-/tmp/mc-bench}

# Toolchain / platform paths. Homebrew on macOS, system packages on Linux (A1).
case "$(uname -s)" in
  Darwin)
    CXX=${CXX:-c++}
    OPENSSL_INCLUDE=${OPENSSL_INCLUDE:--isystem /opt/homebrew/include}
    OPENSSL_LIB=${OPENSSL_LIB:--L/opt/homebrew/opt/openssl@3/lib}
    JDK=${JDK:-/opt/homebrew/opt/openjdk@17/bin}
    SQLITE_LIB=${SQLITE_LIB:--L/opt/homebrew/opt/sqlite/lib -lsqlite3}
    ;;
  *)
    CXX=${CXX:-g++}
    OPENSSL_INCLUDE=${OPENSSL_INCLUDE:-}
    OPENSSL_LIB=${OPENSSL_LIB:-}
    JDK=${JDK:-/usr/bin}
    SQLITE_LIB=${SQLITE_LIB:--lsqlite3}
    ;;
esac
export CXX OPENSSL_INCLUDE OPENSSL_LIB JDK SQLITE_LIB
