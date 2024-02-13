# Constant: location of the custom tool binaries in the dev container
DEV_CONTAINER_BIN="/cheriot-tools/bin/"

# Finds location of a given LLVM tool on the system.
#
# Argument 1: name of the LLVM tool to find (e.g.,
#             `llvm-objdump`, `llvm-objcopy`)
find_llvm_tool() {
	TOOL_NAME=$1
	LLVM_TOOL=${TOOL_NAME}
	if ! type ${LLVM_TOOL} >/dev/null 2>&1 ; then
		FROM_DEV_CONTAINER="${DEV_CONTAINER_BIN}/${TOOL_NAME}"
		if [ -x "${FROM_DEV_CONTAINER}" ] ; then
			LLVM_TOOL=${FROM_DEV_CONTAINER}
		else
			if [ -n "${TOOLS_PATH}" ] ; then
				WITH_TOOLS_PATH_SET="${TOOLS_PATH}/${TOOL_NAME}"
				if [ -x "${WITH_TOOLS_PATH_SET}" ] ; then
					LLVM_TOOL=${WITH_TOOLS_PATH_SET}
				fi
			fi
		fi
	fi
	echo "${LLVM_TOOL}"
}

# Wrapper for `find_llvm_tool` that does `exit 1` with an error message if the
# tool cannot be found.
#
# Arguments are the same as `find_llvm_tool`.
find_llvm_tool_required() {
	LLVM_TOOL=$(find_llvm_tool $1)

	if [ ! -x ${LLVM_TOOL} ] ; then
		echo Unable to locate $1, please set TOOLS_PATH to the directory containing the LLVM toolchain.
		exit 1
	fi

	echo "${LLVM_TOOL}"
}
