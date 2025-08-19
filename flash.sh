#!/bin/bash
# ESP32 Flash Script
# Usage: ./flash.sh

source ~/esp/esp-idf/export.sh

idf.py -p /dev/cu.usbserial-2120 flash
