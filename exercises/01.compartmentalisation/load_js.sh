#!/bin/sh
set -e


if [ -z "$1" ] ; then
	echo Usage: $0 {javascript file}
	exit 1
fi

if [ ! -p input ] ; then
	echo input fifo does not exist.  Make sure that the simulator was started with run_firmware.sh
	exit 1
fi

if [ ! -f ./node_modules/.bin/microvium ] ; then
	if ! type npm >/dev/null 2>&1 ; then
		if type apt >/dev/null 2>&1 ; then
			echo npm is not installed.  Trying to install it from apt.
			echo If you are not using the devcontainer, this may fail.
			sudo apt update
			sudo apt install nodejs make gcc g++
		else
			echo npm is not installed.  Please install it.
			exit 1
		fi
	fi
	npm install microvium
fi

echo Loading JavaScript:
cat $1

./node_modules/.bin/microvium -s /dev/null --output-bytes $1 > input
