#!/bin/sh

# Find objcopy.
OBJCOPY=llvm-objcopy
if ! type ${OBJCOPY} >/dev/null 2>&1 ; then
	if [ -x "/cheriot-tools/bin/llvm-objcopy" ] ; then
		OBJCOPY=/cheriot-tools/bin/llvm-objcopy
	else
		if [ -n "${TOOLS_PATH}" ] ; then
			if [ -x "${TOOLS_PATH}/llvm-objcopy" ] ; then
				echo found TOOLS_PATH/llvm-objcopy
				OBJCOPY=${TOOLS_PATH}/llvm-objcopy
			fi
		fi
	fi
fi

if [ ! -x ${OBJCOPY} ] ; then
	echo Unable to locate llvm-objcopy, please set TOOLS_PATH to the directory containing the LLVM toolchain.
	exit 1
fi

if [ -z ${CHERIOT_IBEX_SAFE_SIM} ] ; then
	CHERIOT_IBEX_SAFE_SIM=/cheriot-tools/bin/cheriot_ibex_safe_sim
fi

if [ ! -x ${CHERIOT_IBEX_SAFE_SIM} ] ; then
	echo Unable to locate simulator, please set CHERIOT_IBEX_SAFE_SIM to the full path to the simulator.
	exit 1
fi

# Prepare the firmware directory with an IROM that just contains a single jump
# instruction that branches to the start of IRAM
if [ ! -d firmware ] ; then
	mkdir firmware
	for I in `seq 32` ; do
		echo 00000000 >> firmware/cpu0_irom.vhx
	done
	# jump forward to the start of IRAM
	echo 7813f06f >> firmware/cpu0_irom.vhx
fi

# Convert the ELF file to a hex file for the simulator
${OBJCOPY} -O binary $1 - | hexdump -v -e '"%08X" "\n"' > firmware/cpu0_iram.vhx

# Run the simulator.
${CHERIOT_IBEX_SAFE_SIM}
