#!/usr/bin/env bash
#
# mayhem/build.sh — builds profanity's fuzz harnesses (sanitized, instrumented) and its
# unit-test + known-answer oracle (normal flags). Runs in the commit image as `mayhem` in /mayhem.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export CC CXX

cd "$SRC"

TARGETS=(fuzz_parser fuzz_jid fuzz_strings fuzz_autocomplete fuzz_stanza fuzz_form)
# fuzz_stanza/fuzz_form (and kat) include src/xmpp/connection.c via harness_xmpp.h; the rest link its object.
CONN_OBJ=build-fuzz/profanity.p/src_xmpp_connection.c.o

LIBS="$(pkg-config --libs glib-2.0 gio-2.0 gobject-2.0 libcurl sqlite3 libstrophe ncursesw readline) -lm -pthread"

# Compile flags meson used for src/xmpp/connection.c in <builddir>, minus the output/dependency bits.
meson_cflags() {
  ninja -C "$1" -t commands profanity.p/src_xmpp_connection.c.o | tail -n1 \
    | sed -E 's/^[^ ]+ //; s/ -MD -MQ [^ ]+ -MF [^ ]+//; s/ -o [^ ]+ -c [^ ]+$//'
}

# Objects of the profanity binary except main() and connection.c (the harnesses supply their own).
core_objects() {
  find "$1/profanity.p" -name '*.o' ! -name 'src_main.c.o' ! -name 'src_xmpp_connection.c.o' | sort
}

setup_meson() {
  local dir="$1"; shift
  if [ -f "$dir/meson-private/coredata.dat" ]; then
    meson setup --reconfigure "$dir" "$@"
  else
    meson setup "$dir" "$@"
  fi
}

# 1) Project with sanitizers + SanCov (fuzzed code is instrumented).
FUZZ_ARGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1"
setup_meson build-fuzz -Dplugins=disabled -Dbuildtype=plain -Db_lundef=false \
  "-Dc_args=$FUZZ_ARGS" "-Dc_link_args=$FUZZ_ARGS"
ninja -C build-fuzz -j"$MAYHEM_JOBS" profanity

rm -f build-fuzz/libprofcore.a
# shellcheck disable=SC2046
ar rcs build-fuzz/libprofcore.a $(core_objects build-fuzz)

# 2) Harnesses: fuzzer binary + standalone reproducer.
CFLAGS_FUZZ="$(meson_cflags build-fuzz)"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build-fuzz/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/standalone_main.o

for t in "${TARGETS[@]}"; do
  (cd build-fuzz && $CC $CFLAGS_FUZZ -I../mayhem/harnesses -c "../mayhem/harnesses/$t.c" -o "$t.o")
  extra=""
  grep -q harness_xmpp.h "mayhem/harnesses/$t.c" || extra="$CONN_OBJ"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "build-fuzz/$t.o" build-fuzz/lsan_off.o $extra \
    -Wl,--start-group build-fuzz/libprofcore.a -Wl,--end-group $LIBS -o "/mayhem/$t"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "build-fuzz/$t.o" build-fuzz/lsan_off.o build-fuzz/standalone_main.o $extra \
    -Wl,--start-group build-fuzz/libprofcore.a -Wl,--end-group $LIBS -o "/mayhem/$t-standalone"
done

# 3) Oracle: upstream's cmocka unit tests + the known-answer probe, built with normal flags.
setup_meson build-tests -Dplugins=disabled -Dtests=true -Dbuildtype=plain -Db_lundef=false \
  "-Dc_args=-O1 -g $COVERAGE_FLAGS" "-Dc_link_args=$COVERAGE_FLAGS"
ninja -C build-tests -j"$MAYHEM_JOBS" profanity unittests

rm -f build-tests/libprofcore.a
# shellcheck disable=SC2046
ar rcs build-tests/libprofcore.a $(core_objects build-tests)
CFLAGS_TESTS="$(meson_cflags build-tests)"
(cd build-tests && $CC $CFLAGS_TESTS -I../mayhem/harnesses -c ../mayhem/kat.c -o kat.o)
$CC -O1 $COVERAGE_FLAGS build-tests/kat.o -Wl,--start-group build-tests/libprofcore.a -Wl,--end-group $LIBS -o /mayhem/kat
