#!/bin/sh

SCRIPT_DIRECTORY="$(dirname "$(realpath "$0")")"
. ${SCRIPT_DIRECTORY}/includes/helper_find_llvm_install.sh

OBJCOPY=$(find_llvm_tool_required llvm-objcopy)

echo Using ${OBJCOPY}...

# Create the firmware directory if it does not already exist
if [ ! -d "firmware" ]; then
	mkdir firmware
fi

# Convert the ELF file to a hex file for the simulator
${OBJCOPY} -O binary $1 - | hexdump -v -e '"%08X" "\n"' > firmware/cpu0_iram.vhx
# Add a newline at the end of the vhx file
echo >> firmware/cpu0_iram.vhx
