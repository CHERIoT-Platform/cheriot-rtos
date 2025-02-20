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
	echo clang-format not found at ${CLANG_FORMAT}
	exit 1
fi

if which nproc ; then
	PARALLEL_JOBS=$(nproc)
else
	PARALLEL_JOBS=$(sysctl -n kern.smp.cpus)
fi
DIRECTORIES="sdk tests examples tests.extra"
# Standard headers should be included once we move to a clang-tidy that
# supports NOLINTBEGIN to disable specific checks over a whole file.
# In particular, modernize-redundant-void-arg should be disabled in any header
# file that's included from C.
#
# FreeRTOS-Compat headers follow FreeRTOS naming conventions and should be
# excluded for now.  Eventually they should be included for everything except
# the identifier naming checks.
HEADERS=$(find ${DIRECTORIES} -name '*.h' -or -name '*.hh' | grep -v -f scripts/run_clang_tidy_format.exempt_headers)
SOURCES=$(find ${DIRECTORIES} -name '*.cc' | grep -v -f scripts/run_clang_tidy_format.exempt_sources)

echo Headers: ${HEADERS}
echo Sources: ${SOURCES}

${CLANG_FORMAT} -i ${HEADERS} ${SOURCES}
if ! git diff --exit-code ${HEADERS} ${SOURCES} ; then
	echo clang-format applied changes
	exit 1
fi

rm -f tidy.fail-*
# sh syntax is -c "string" [name [args ...]], so "tidy" here is the name and not included in "$@"
echo ${HEADERS} ${SOURCES} | xargs -P${PARALLEL_JOBS} -n1 sh -c "${CLANG_TIDY} --extra-arg=-DCLANG_TIDY -export-fixes=\$(mktemp -p. tidy.fail-XXXX) \$@" tidy
if [ $(find . -maxdepth 1 -name 'tidy.fail-*' -size +0 | wc -l) -gt 0 ] ; then
	# clang-tidy put non-empty output in one of the tidy-*.fail files
	cat tidy.fail-*
	exit 1
fi
