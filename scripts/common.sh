# Common functions to include in multiple scripts

function error() {
    echo "Error: $1"
    exit 1
}

function ensure_cheriot_rtos_root () {
    [ -d sdk ] || error "Please run this script from the root of the cheriot-rtos repository."
}

function find_sdk () {
    if [ -n "$1" ]; then
        SDK="$(readlink -f $1)"
    elif [ -d "/cheriot-tools/bin" ]; then
        SDK=/cheriot-tools
    else
        error "No SDK found, please provide as first argument."
    fi
}
