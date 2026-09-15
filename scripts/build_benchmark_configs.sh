#!/bin/sh

# Helper script to build multiple configurations of a benchmark.  Patches the
# board description files to enable and disable different combinations of
# hardware features.  Also patches include paths to keep track of the original
# locations.

set -e

SCRIPTPATH=$(dirname $(readlink -f $0))

BOARD=$(realpath $1)
BENCHMARK=$(realpath $2)
SDK=$(realpath $3)
BOARDPATH=$(dirname ${BOARD})
BOARDNAME=$(basename -s .json ${BOARD})

if [ ! -f "${BOARD}" ] ; then
	echo "First argument must be the path of a board description file.  '${BOARD}' is not a valid file"
	exit 1
fi
if [ ! -f "${BENCHMARK}/xmake.lua" ] ; then
	echo "Second argument must be the path of a benchmark file.  '${BENCHMARK}' does not contain an xmake.lua file"
	exit 1
fi
if [ ! -d "${SDK}" ] ; then
	echo "Third argument must be the path of the sdk.  '${SDK}' does not exist"
	exit 1
fi
if [ ! -f "${SDK}/bin/clang" ] ; then
	echo "Third argument must be the path of the sdk.  '${SDK}/bin/clang' does not exist"
	exit 1
fi

# Set up a temporary directory to work in.
DIR=$(mktemp -d benchmark.XXX) || exit 1
echo Working in ${DIR}...
DIR=$(pwd)/${DIR}
cd  ${DIR}


# Create a directory for the board files

mkdir boards
cd boards


# Our JSON files are slightly extended JSON, parse them with xmake's JSON
# parser and spit them out as standard JSON so that jq can consume them.
cp ${BOARD} basic.json
cat <<EOF > xmake.lua
target('fake')
	on_load(function (target)
		import("core.base.json")
		local board = json.loadfile(path.join(os.scriptdir(), 'basic.json'))
		json.savefile(path.join(os.scriptdir(), 'standard.json'), board)
	end
	)
EOF
xmake f > /dev/null
rm xmake.lua basic.json
rm -rf .xmake

# Create variants of the board with different versions of the revoker

jq '.revoker="software"' < standard.json > ${BOARDNAME}-software-revoker.json
jq '.revoker="hardware"' < standard.json > ${BOARDNAME}-hardware-revoker.json
jq 'del(.revoker)' < standard.json > ${BOARDNAME}-no-revoker.json
jq 'del(.revoker) | .defines[.defines| length]|= .+"CHERIOT_FAKE_REVOKER"' < standard.json > ${BOARDNAME}-fake-revoker.json
rm standard.json

CONFIGS="${BOARDNAME}-software-revoker ${BOARDNAME}-hardware-revoker ${BOARDNAME}-no-revoker ${BOARDNAME}-fake-revoker"

# Patch the include directories so that relative paths become absolute relative
# to the board file.
for I in ${CONFIGS}; do
	# Fix up includes if they expect to be relative to the board file path
	# In addition to absolute paths, also pass through anything that starts
	# with a $.  This is a substitution variable that xmake will fill in (for
	# example ${sdk} for the location of the SDK).
	jq ".driver_includes = (.driver_includes | map_values(. = if . | startswith(\"/\") or startswith(\"\$\") then . else \"${BOARDPATH}/\" + . end))" < $I.json > $I.json.fixed
	mv $I.json.fixed $I.json
done


cd ..

# Build each configuration
for I in ${CONFIGS}; do
	echo $I
	mkdir $I
	cd $I
	echo xmake f --sdk=${SDK} --board=${DIR}/boards/$I.json -P ${BENCHMARK}
	xmake f --sdk=${SDK} --board=${DIR}/boards/$I.json -P ${BENCHMARK}
	xmake -P ${BENCHMARK}
	cd ..
done

# Let the user know where we put them all.
for I in ${CONFIGS}; do
	echo Benchmark built in ${DIR}/$I
done
