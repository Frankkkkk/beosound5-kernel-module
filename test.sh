#!/usr/bin/env bash

cleanup() {
    echo "Interrupt signal caught. Cleaning up..."
    ./restore.sh  # Assuming this script removes the module
    exit 2        # Exit with a specific error code to indicate script was interrupted
}

# Trap SIGINT and call the cleanup function
trap cleanup SIGINT

if [ `id -u` -ne 0 ]; then
    echo "You must be a superuser to run this script"
    exit 1
fi

if [[ $# != 1 ]]; then
    echo "usage: ${0} <timeout>"
    exit 1
fi

regex="^[0-9]+$"

if ! [[ "${1}" =~ $regex ]] ; then
   echo "<timeout> must be a positive integer"
   exit 1
fi

./setup.sh; sleep ${1}; ./restore.sh