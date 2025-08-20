#!/bin/bash
# ESP32 Flash Script with ESPPORT cleanup
# Usage: ./flash_clean.sh

# Clear any conflicting ESPPORT environment variable
unset ESPPORT

# Source ESP-IDF and run flash
source ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbserial-2120 flash
