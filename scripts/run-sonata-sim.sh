#!/bin/bash

set -e

if [ -z "${SONATA_SIMULATOR}" ] ; then
	SONATA_SIMULATOR=/cheriot-tools/bin/sonata_simulator
fi

if [ -z "${SONATA_SIMULATOR_BOOT_STUB}" ] ; then
	SONATA_SIMULATOR_BOOT_STUB=/cheriot-tools/elf/sonata_simulator_boot_stub
fi

if [ -z "$1" ] ; then
	echo You must specify an elf file to run.
	exit 1
fi

if [ ! -x "${SONATA_SIMULATOR}" ] ; then
	echo Unable to locate Sonata simulator, please set SONATA_SIMULATOR to the full path of the simulator.
	exit 2
fi

if [ ! -x "${SONATA_SIMULATOR_BOOT_STUB}" ] ; then
	echo Unable to locate Sonata simulator boot stub, please set SONATA_SIMULATOR_BOOT_STUB to the full path of the boot stub.
	exit 3
fi

# Remove old uart log
UART_LOG=uart0.log
rm -f ${UART_LOG}

# If a second argument is provided, check content of UART log.
if [ -n "$2" ] ; then
	# Run the simulator in the background.
	${SONATA_SIMULATOR} -E "${SONATA_SIMULATOR_BOOT_STUB}" -E "$1" &
	LOOP_TRACKER=0
	while (( LOOP_TRACKER <= 60 ))
	do
		sleep 1s
		# Returns 0 if found and 1 if not.
		MATCH_FOUND=$(cat ${UART_LOG} | grep -q -F -f "$2"; echo $?)
		if (( MATCH_FOUND == 0 )) ; then
			# Match was found so exit with success
			pkill -P $$
			exit 0
		fi
		LOOP_TRACKER=$((LOOP_TRACKER+1))
	done
	# Timeout was hit so no success.
	pkill -P $$
	exit 4
else
	# If there is no second argument, run simulator in foreground.
	${SONATA_SIMULATOR} -E "${SONATA_SIMULATOR_BOOT_STUB}" -E "$1"
fi
