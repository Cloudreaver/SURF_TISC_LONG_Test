#!/bin/bash
#
# This script unprotects the cpci memory mapped device files.
# ------------------------------------------------------------

chmod 777 /sys/class/uio/uio0/device/config
chmod 777 /sys/class/uio/uio0/device/resource0
chmod 777 /sys/class/uio/uio0/device/resource1

exit 0

