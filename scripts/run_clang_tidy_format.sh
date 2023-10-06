#!/usr/bin/env bash
set -eo pipefail
if [ ! -d sdk ] ; then
	echo Please run this script from the root of the cheriot-rtos repository.
	exit 1
fi
CLANG_TIDY=/cheriot-tools/bin/clang-tidy
CLANG_FORMAT=/cheriot-tools/bin/clang-format
if [ -n "$1" ] ; then
	CLANG_TIDY=$1/clang-tidy
	CLANG_FORMAT=$1/clang-format
fi
if [ ! -x ${CLANG_TIDY} ] ; then
	echo Usage: $0 path/to/cheriot/tools/bin
	echo clang-tidy not found at ${CLANG_TIDY}
	exit 1
fi
if [ ! -x ${CLANG_FORMAT} ] ; then
	echo Usage: $0 path/to/cheriot/tools/bin
	echo clang-tidy not found at ${CLANG_FORMAT}
	exit 1
fi

if which nproc ; then
	PARALLEL_JOBS=$(nproc)
else
	PARALLEL_JOBS=$(sysctl -n kern.smp.cpus)
fi
DIRECTORIES="sdk tests examples"
# Standard headers should be included once we move to a clang-tidy that
# supports NOLINTBEGIN to disable specific checks over a whole file.
# In particular, modernize-redundant-void-arg should be disabled in any header
# file that's included from C.
HEADERS=$(find ${DIRECTORIES} -name '*.h' -or -name '*.hh' | grep -v libc++ | grep -v third_party | grep -v 'std.*.h' | grep -v errno.h | grep -v strings.h | grep -v string.h | grep -v -assembly.h | grep -v cdefs.h | grep -v /riscv.h | grep -v inttypes.h | grep -v /cheri-builtins.h | grep -v c++-config | grep -v ctype.h | grep -v switcher.h | grep -v assert.h | grep -v /build/ | grep -v microvium)
SOURCES=$(find ${DIRECTORIES} -name '*.cc' | grep -v /build/ | grep -v third_party)

echo Headers: ${HEADERS}
echo Sources: ${SOURCES}
rm -f tidy-*.fail

# sh syntax is -c "string" [name [args ...]], so "tidy" here is the name and not included in "$@"
echo ${HEADERS} ${SOURCES} | xargs -P${PARALLEL_JOBS} -n5 sh -c "${CLANG_TIDY} -export-fixes=\$(mktemp -p. tidy.fail-XXXX) \$@" tidy
if [ $(find . -maxdepth 1 -name 'tidy.fail-*' -size +0 | wc -l) -gt 0 ] ; then
	# clang-tidy put non-empty output in one of the tidy-*.fail files
	cat tidy.fail-*
	exit 1
fi

${CLANG_FORMAT} -i ${HEADERS} ${SOURCES}
if git diff --exit-code ${HEADERS} ${SOURCES} ; then
	exit 0
fi
echo clang-format applied changes
exit 1
