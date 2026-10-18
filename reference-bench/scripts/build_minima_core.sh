#!/bin/sh
# Clone (if needed) and compile minima-core + the Java benchmark harness.
# Compiles sources directly with javac (jcenter() in build.gradle is dead;
# the jars are vendored in minima-core/lib).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

if [ ! -d "$MINIMA_CORE" ]; then
    echo "Cloning minima-core -> $MINIMA_CORE"
    mkdir -p "$(dirname "$MINIMA_CORE")"
    git clone --depth 1 https://github.com/spartacusrex-minima/minima-core.git "$MINIMA_CORE"
fi

CP=$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')
find "$MINIMA_CORE/src" -name '*.java' > /tmp/mc_srcs.txt

echo "Compiling minima-core ($(wc -l < /tmp/mc_srcs.txt) sources) with $JDK"
mkdir -p "$MC_BUILD"
"$JDK/javac" -nowarn -cp "$CP" -d "$MC_BUILD" @/tmp/mc_srcs.txt

echo "Compiling bench harness"
mkdir -p "$MC_BENCH_OUT"
"$JDK/javac" -cp "$CP$MC_BUILD" -d "$MC_BENCH_OUT" "$_BENCH_DIR/java/BenchMinima.java"

echo "minima-core commit: $(git -C "$MINIMA_CORE" rev-parse HEAD)"
echo "classes: $(find "$MC_BUILD" -name '*.class' | wc -l | tr -d ' ')"
echo "OK"
