#!/bin/sh

SCRIPT_DIRECTORY="$(dirname "$(realpath "$0")")"
. ${SCRIPT_DIRECTORY}/includes/helper_find_llvm_install.sh

OBJDUMP=$(find_llvm_tool_required llvm-objdump)

MACHINE_READABLE=0

print_compartment_size() {
	# Print all sections that match passed compartment name (potentially
	# multiple ones, as compartments covers code and various data
	# sections), and sum their sizes.
	#
	# Note that this does not validate compartment names. An invalid
	# compartment name may return either 0kB, or an arbitrary number if the
	# provided name matches legitimate compartments. This would be nice to
	# fix at some point.
	SIZE=$(${OBJDUMP} --headers $1 | grep -i $2 |
		awk '{ sum += "0x"$3 } END { print sum}')

	if [ "$MACHINE_READABLE" -eq 0 ]; then
		KB_SIZE=$(echo $SIZE | awk '{ print ($1 / 1024) "kB"}')
		echo "Size of compartment '$2': ${KB_SIZE}"
	else
		echo "${SIZE}"
	fi
}

print_full_code_size() {
	# Print all sections between the start of the firmware (`loader_start`)
	# and the end of `__cap_relocs`, and sum the sizes of sections
	SIZE=$(${OBJDUMP} --headers $1 |
		awk '/loader_start/{f=1} /__cap_relocs/{f=0;print} f' |
		awk '{ sum += "0x"$3 } END { print sum}')

	if [ "$MACHINE_READABLE" -eq 0 ]; then
		KB_SIZE=$(echo $SIZE | awk '{ print ($1 / 1024) "kB"}')
		echo "Size of the full binary: ${KB_SIZE}"
	else
		echo "${SIZE}"
	fi
}

help() {
   echo "Determine the size of a CHERIoT firmware image."
   echo
   echo "Syntax: $(basename "$0") {-m} [-h|-c|-f]"
   echo "  -m                                         Enable machine-readable output in B."
   echo "                                             Optional, must come before -c/-f."
   echo "  -h                                         Print this help."
   echo "  -f [firmware location]                     Print the size of the entire firmware image."
   echo "  -c [firmware location] [compartment name]  Print the size of passed compartment."
   echo
}

if [ "$#" -eq 0 ]; then
	echo "Error: Arguments missing."
	help
	exit
fi

while getopts ":hcfm" opt; do
case $opt in
	h)
		help
		exit;;
	m)
		MACHINE_READABLE=1
		;;
	c)
		EXPECTED=$(( 3 + ${MACHINE_READABLE}))
		if [ "$#" -ne "${EXPECTED}" ]; then
			echo "Error: Argument number incorrect."
			help
			exit
		fi

		shift $((OPTIND-1))
		print_compartment_size $@
		exit;;
	f)
		EXPECTED=$(( 2 + ${MACHINE_READABLE}))
		if [ "$#" -ne "${EXPECTED}" ]; then
			echo "Error: Argument number incorrect."
			help
			exit
		fi

		shift $((OPTIND-1))
		print_full_code_size $@
		exit;;
	\?)
		echo "Error: Invalid option."
		help
		exit;;
esac
done
