#!/bin/bash

echo "=== Simple MQTT Test ==="
echo

# MQTT broker settings
BROKER="10.0.1.211"
PORT="1883"
DEVICE_ID="ESP32_58B8D8"
CONTROL_TOPIC="$DEVICE_ID/control"

echo "Testing MQTT for: $DEVICE_ID"
echo "Broker: $BROKER:$PORT"
echo "Control Topic: $CONTROL_TOPIC"
echo

echo "=== Sending test commands to ESP32 ==="
echo "Watch your ESP32 serial output for these messages:"
echo

echo "Test 1: Simple digital command"
echo "Expected: [INFO] Received MQTT message on topic: ESP32_58B8D8/control"
echo "Expected: [INFO] Command executed: Digital write: pin 2 = HIGH"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '{"type":"digital","pin":2,"value":1}'
echo "✓ Sent digital command"
echo

sleep 3

echo "Test 2: Simple analog command"
echo "Expected: [INFO] Command executed: Analog write: pin 3 = 128"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '{"type":"analog","pin":3,"value":128}'
echo "✓ Sent analog command"
echo

sleep 3

echo "Test 3: Invalid command"
echo "Expected: [WARN] Failed to parse message: No valid commands found"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '{"type":"invalid","pin":4,"value":1}'
echo "✓ Sent invalid command"
echo

sleep 3

echo "Test 4: Multiple commands"
echo "Expected: Two command execution messages"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '[{"type":"digital","pin":5,"value":1},{"type":"analog","pin":6,"value":255}]'
echo "✓ Sent multiple commands"
echo

echo "=== Test Complete ==="
echo
echo "If ESP32 shows NO logs for these tests:"
echo "1. ESP32 is not connected to MQTT broker"
echo "2. ESP32 is not subscribed to control topic"
echo "3. Message callback is not working"
echo
echo "If ESP32 shows connection logs but no message logs:"
echo "1. Check subscription status in ESP32 debug output"
echo "2. Verify topic name exactly matches"
echo "3. Check message callback registration"
