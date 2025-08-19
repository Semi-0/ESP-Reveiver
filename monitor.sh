#!/bin/bash
# ESP32 Monitor Script
# Usage: ./monitor.sh

source ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbserial-2120 monitor

