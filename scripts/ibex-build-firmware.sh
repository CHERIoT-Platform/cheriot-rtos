#!/bin/sh

# Find objcopy.
OBJCOPY=llvm-objcopy
if ! type ${OBJCOPY} >/dev/null 2>&1 ; then
	if [ -x "/cheriot-tools/bin/llvm-objcopy" ] ; then
		OBJCOPY=/cheriot-tools/bin/llvm-objcopy
	else
		if [ -n "${TOOLS_PATH}" ] ; then
			if [ -x "${TOOLS_PATH}/llvm-objcopy" ] ; then
				echo found ${TOOLS_PATH}/llvm-objcopy
				OBJCOPY=${TOOLS_PATH}/llvm-objcopy
			fi
		fi
	fi
fi

if [ ! -x ${OBJCOPY} ] ; then
	echo Unable to locate llvm-objcopy, please set TOOLS_PATH to the directory containing the LLVM toolchain.
	exit 1
fi

echo Using ${OBJCOPY}...

# Create the firmware directory if it does not already exist
if [ ! -d "firmware" ]; then
	mkdir firmware
fi

# Convert the ELF file to a hex file for the simulator
${OBJCOPY} -O binary $1 - | hexdump -v -e '"%08X" "\n"' > firmware/cpu0_iram.vhx
# Add a newline at the end of the vhx file
echo >> firmware/cpu0_iram.vhx
