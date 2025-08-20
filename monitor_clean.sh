#!/bin/bash
# ESP32 Monitor Script with ESPPORT cleanup
# Usage: ./monitor_clean.sh

# Clear any conflicting ESPPORT environment variable
unset ESPPORT

# Source ESP-IDF and run monitor
source ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbserial-2120 monitor
