#!/bin/sh

set -e

CORE_NAME=$1

if [ -z ${CHERIOT_MSFT_SAFE_SIM} ] ; then
	CHERIOT_MSFT_SAFE_SIM=/cheriot-tools/bin/cheriot_${CORE_NAME}_safe_sim
fi

if [ ! -x ${CHERIOT_MSFT_SAFE_SIM} ] ; then
	echo Unable to locate simulator, please set CHERIOT_MSFT_SAFE_SIM to the full path to the simulator.
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

$(dirname $0)/msft-safe-build-firmware.sh $2

# Run the simulator.
${CHERIOT_MSFT_SAFE_SIM}
