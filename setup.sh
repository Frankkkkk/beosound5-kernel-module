#/usb/bin/env bash

if [ `id -u` -ne 0 ]; then
    echo "You must be a superuser to run this script"
    exit 1
fi


# Insert custom USB keyboard driver
insmod beosound.ko
