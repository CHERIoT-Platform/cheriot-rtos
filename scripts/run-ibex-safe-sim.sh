#!/bin/sh

set -e

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
	# The start of IROM is a vectored interrupt table.
	for I in `seq 32` ; do
		echo 00000000 >> firmware/cpu0_irom.vhx
	done
	# Offset 0x80 is the entry point.  Insert a single relative jump here that
	# jumps to the start of IRAM.
	echo 7813f06f >> firmware/cpu0_irom.vhx
fi

$(dirname $0)/ibex-build-firmware.sh $1

# Run the simulator.
${CHERIOT_IBEX_SAFE_SIM}
