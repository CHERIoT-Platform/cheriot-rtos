#!/bin/sh
if [ -z "${FLUTE_BUILD}" ] ; then
	echo The FLUTE_BUILD environment variable should be set to the flute build directory
	exit 0
fi
# This script depends on non-portable GNU extensions, so prefer the g-prefixed
# versions if they exist
TAIL=tail
if which gtail  ; then TAIL=gtail ; fi
HEAD=head
if which ghead  ; then HEAD=ghead ; fi
PASTE=paste
if which gpaste  ; then PASTE=gpaste ; fi
echo Using ${TAIL}, ${HEAD}, and ${PASTE}

if [ ! -f tail.hex ] ; then
	for I in $(seq 0 32768) ; do
		echo 00000000 >> tail.hex
	done
fi

${FLUTE_BUILD}/../../Tests/elf_to_hex/elf_to_hex $1 Mem.hex

awk '{print substr($0,33,8); print substr($0,0,8)}' Mem.hex > 1u-0.hex
awk '{print substr($0,41,8); print substr($0,9,8)}' Mem.hex > 0u-0.hex
awk '{print substr($0,49,8); print substr($0,17,8)}' Mem.hex > 1l-0.hex
awk '{print substr($0,57,8); print substr($0,25,8)}' Mem.hex > 0l-0.hex

${TAIL} -n +3 1u-0.hex > 1u-1.hex
${TAIL} -n +3 0u-0.hex > 0u-1.hex
${TAIL} -n +3 1l-0.hex > 1l-1.hex
${TAIL} -n +3 0l-0.hex > 0l-1.hex

${HEAD} -n -4 1u-1.hex > 1u-2.hex
${HEAD} -n -4 0u-1.hex > 0u-2.hex
${HEAD} -n -4 1l-1.hex > 1l-2.hex
${HEAD} -n -4 0l-1.hex > 0l-2.hex

${PASTE} -d \\n 1l-2.hex 1u-2.hex > 1-0.hex
${PASTE} -d \\n 0l-2.hex 0u-2.hex > 0-0.hex

cat 1-0.hex tail.hex | ${HEAD} -n 32768 > Mem-TCM-1.hex
cat 0-0.hex tail.hex | ${HEAD} -n 32768 > Mem-TCM-0.hex

${FLUTE_BUILD}/exe_HW_sim +tohost > /dev/null
