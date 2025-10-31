#!/usr/bin/env bash

# Generate compile commands files for all known projects in this repo


. "$(dirname $0)"/common.sh

ensure_cheriot_rtos_root

find_sdk $1

echo "Using SDK=$SDK"

# Generate compile_commands.json for all of the extra tests and examples.
for dir in tests.extra/*/ ex*/[[:digit:]]* ; do
    echo Generating compile_commands.json for $dir
    (cd $dir && xmake f --sdk="${SDK}" && xmake project -k compile_commands)
done

# Generate the top-level compile-commands.json
cd tests && xmake f --sdk="${SDK}" && xmake project -k compile_commands ..
