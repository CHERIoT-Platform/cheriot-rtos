#!/bin/sh

set -e

FIRMWARE_ELF=$1

SCRIPT_DIRECTORY="$(dirname "$(realpath "$0")")"
. ${SCRIPT_DIRECTORY}/includes/helper_find_llvm_install.sh

STRIP=$(find_llvm_tool_required llvm-strip)

if ! command -v uf2conv > /dev/null ; then
	echo "uf2conv not found.  On macOS / Linux systems with Python3 installed, you can install it with:"
	echo "python3 -m pip install --pre -U git+https://github.com/makerdiary/uf2utils.git@main"
	exit 1
fi

# Strip the ELF file
${STRIP} ${FIRMWARE_ELF} -o ${FIRMWARE_ELF}.strip
# Convert the stripped elf to a UF2 (Microsoft USB Flashing Format) file
uf2conv ${FIRMWARE_ELF}.strip -b0x00000000 -f0x6CE29E60 -co ${FIRMWARE_ELF}.slot1.uf2
uf2conv ${FIRMWARE_ELF}.strip -b0x10000000 -f0x6CE29E60 -co ${FIRMWARE_ELF}.slot2.uf2
uf2conv ${FIRMWARE_ELF}.strip -b0x20000000 -f0x6CE29E60 -co ${FIRMWARE_ELF}.slot3.uf2


# Try to copy the firmware to the SONATA drive, if we can find one.
try_copy()
{
	if [ -f $1/SONATA/OPTIONS.TXT ] ; then
		cp ${FIRMWARE_ELF}.slot1.uf2 $1/SONATA/firmware.uf2
		echo "Firmware copied to $1/SONATA/"
		exit
	fi
}

# Try some common mount points
try_copy /Volumes/
try_copy /run/media/$USER/
try_copy /run/media/
try_copy /mnt/

cp ${FIRMWARE_ELF}.slot1.uf2 firmware.uf2

echo "Please copy $(pwd)/firmware.uf2 to the SONATA drive to load."
