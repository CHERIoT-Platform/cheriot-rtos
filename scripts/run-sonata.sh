#!/bin/sh

set -ue

FIRMWARE_ELF=$1

SCRIPT_DIRECTORY="$(dirname "$(realpath "$0")")"
. ${SCRIPT_DIRECTORY}/includes/helper_find_llvm_install.sh

OBJCOPY=$(find_llvm_tool_required llvm-objcopy)

command -v uf2conv > /dev/null
if [ ! $? ] ; then
	echo "uf2conv not found.  On macOS / Linux systems with Python3 installed, you can install it with:"
	echo "python3 -m pip install --pre -U git+https://github.com/makerdiary/uf2utils.git@main"
fi

# Convert the ELF file to a binary file
${OBJCOPY} -O binary ${FIRMWARE_ELF} ${FIRMWARE_ELF}.bin
# Convert the binary to a UF2 (Microsoft USB Flashing Format) file
uf2conv ${FIRMWARE_ELF}.bin -b0x00101000 -co ${FIRMWARE_ELF}.uf2

# Try to copy the firmware to the SONATA drive, if we can find one.
try_copy()
{
	if [ -f $1/SONATA/OPTIONS.TXT ] ; then
		cp ${FIRMWARE_ELF}.uf2 $1/SONATA/firmware.uf2
		echo "Firmware copied to $1/SONATA/"
		exit
	fi
}

# Try some common mount points
try_copy /Volumes/
try_copy /run/media/$USER/
try_copy /run/media/
try_copy /mnt/

echo "Please copy $(pwd)/${FIRMWARE_ELF}.uf2 to the SONATA drive to load."
