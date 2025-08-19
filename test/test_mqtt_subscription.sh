#!/bin/bash

echo "=== MQTT Subscription Test for Dynamic ESP32 Device ID ==="
echo

# Check if mosquitto_pub is available
if ! command -v mosquitto_pub &> /dev/null; then
    echo "Error: mosquitto_pub not found. Please install mosquitto-clients:"
    echo "  On macOS: brew install mosquitto"
    echo "  On Ubuntu: sudo apt-get install mosquitto-clients"
    exit 1
fi

# MQTT broker settings (from config.cpp)
BROKER="10.0.1.211"
PORT="1883"

# Get the device ID dynamically (this would be the actual ESP32's ID)
# For testing, we'll use the expected format
DEVICE_ID="ESP32_58B8D8"
TOPIC="$DEVICE_ID/control"

echo "Testing MQTT subscription to: $TOPIC"
echo "Broker: $BROKER:$PORT"
echo "Note: The actual ESP32 will automatically generate its device ID from its MAC address"
echo

# Test 1: Digital command
echo "Test 1: Sending digital command..."
mosquitto_pub -h $BROKER -p $PORT -t $TOPIC -m '{"type": "digital", "pin": 13, "value": 1}'
echo "✓ Digital command sent"

# Wait a moment
sleep 2

# Test 2: Analog command
echo "Test 2: Sending analog command..."
mosquitto_pub -h $BROKER -p $PORT -t $TOPIC -m '{"type": "analog", "pin": 9, "value": 255}'
echo "✓ Analog command sent"

# Wait a moment
sleep 2

# Test 3: Multiple commands
echo "Test 3: Sending multiple commands..."
mosquitto_pub -h $BROKER -p $PORT -t $TOPIC -m '[
  {"type": "digital", "pin": 13, "value": 1},
  {"type": "analog", "pin": 9, "value": 128},
  {"type": "digital", "pin": 12, "value": 0}
]'
echo "✓ Multiple commands sent"

# Wait a moment
sleep 2

# Test 4: Invalid command (should be handled gracefully)
echo "Test 4: Sending invalid command..."
mosquitto_pub -h $BROKER -p $PORT -t $TOPIC -m '{"type": "invalid", "pin": 13, "value": 1}'
echo "✓ Invalid command sent"

echo
echo "=== MQTT Subscription Test Complete ==="
echo "Check your ESP32 serial output to see if messages were received and processed."
echo
echo "Expected ESP32 behavior:"
echo "✓ Digital command: Pin 13 set to HIGH"
echo "✓ Analog command: Pin 9 set to PWM value 255"
echo "✓ Multiple commands: All 3 commands executed"
echo "✓ Invalid command: Error logged, no action taken"
echo
echo "Device ID Information:"
echo "- The ESP32 automatically generates its device ID from its MAC address"
echo "- Format: ESP32_XXXXXX (where XXXXXX are the last 3 bytes of MAC)"
echo "- This ensures each ESP32 has a unique identifier"
echo "- No manual configuration required!"
