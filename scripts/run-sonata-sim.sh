#!/bin/bash

set -e

# Specify the default environment variables if they haven't been already.
: "${SONATA_SIMULATOR:=/cheriot-tools/bin/sonata_simulator}"
: "${SONATA_SIMULATOR_BOOT_STUB:=/cheriot-tools/elf/sonata_simulator_hyperram_boot_stub}"
: "${SONATA_SIMULATOR_UART_LOG=uart0.log}"

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
rm -f "${SONATA_SIMULATOR_UART_LOG}"

if ! ${SONATA_SIMULATOR} -E "${SONATA_SIMULATOR_BOOT_STUB}" -E "$1"; then
	echo "Simulator exited with failure! UART output:"
	cat "${SONATA_SIMULATOR_UART_LOG}"
	exit 4
fi

# Check to see if the output indicates failure
if grep -i failure "${SONATA_SIMULATOR_UART_LOG}"; then
	echo "Log output contained 'failure'"
	exit 5
fi
