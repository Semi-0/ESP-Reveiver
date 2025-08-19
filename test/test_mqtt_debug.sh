#!/bin/bash

echo "=== MQTT Reception Debug Test ==="
echo

# MQTT broker settings
BROKER="10.0.1.211"
PORT="1883"
DEVICE_ID="ESP32_58B8D8"
CONTROL_TOPIC="$DEVICE_ID/control"
STATUS_TOPIC="$DEVICE_ID/status"

echo "Testing MQTT reception for: $DEVICE_ID"
echo "Broker: $BROKER:$PORT"
echo "Control Topic: $CONTROL_TOPIC"
echo "Status Topic: $STATUS_TOPIC"
echo

# Check if mosquitto tools are available
if ! command -v mosquitto_pub &> /dev/null; then
    echo "Error: mosquitto_pub not found. Please install mosquitto-clients"
    exit 1
fi

if ! command -v mosquitto_sub &> /dev/null; then
    echo "Error: mosquitto_sub not found. Please install mosquitto-clients"
    exit 1
fi

echo "=== Step 1: Check MQTT broker connectivity ==="
echo "Testing basic connection to broker..."
timeout 5 mosquitto_pub -h $BROKER -p $PORT -t "test/connection" -m "hello" && echo "✓ Broker is reachable" || echo "✗ Cannot reach broker"
echo

echo "=== Step 2: Monitor status topic ==="
echo "Listening for ESP32 status messages (5 seconds)..."
timeout 5 mosquitto_sub -h $BROKER -p $PORT -t "$STATUS_TOPIC" -C 1 && echo "✓ ESP32 is publishing status" || echo "✗ No status messages from ESP32"
echo

echo "=== Step 3: Check if ESP32 is subscribed to control topic ==="
echo "Sending test message to control topic..."
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '{"type":"digital","pin":13,"value":1}'
echo "✓ Test message sent"
echo

echo "=== Step 4: Send multiple test commands ==="
echo "Test 1: Simple digital command"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '{"type":"digital","pin":2,"value":1}'
sleep 2

echo "Test 2: Simple analog command"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '{"type":"analog","pin":3,"value":128}'
sleep 2

echo "Test 3: Multiple commands"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '[{"type":"digital","pin":4,"value":1},{"type":"analog","pin":5,"value":255}]'
sleep 2

echo "Test 4: Invalid command (should show error)"
mosquitto_pub -h $BROKER -p $PORT -t "$CONTROL_TOPIC" -m '{"type":"invalid","pin":6,"value":1}'
sleep 2

echo "=== Step 5: Test QoS levels ==="
echo "Sending with QoS 0..."
mosquitto_pub -h $BROKER -p $PORT -q 0 -t "$CONTROL_TOPIC" -m '{"type":"digital","pin":7,"value":1}'
sleep 2

echo "Sending with QoS 1..."
mosquitto_pub -h $BROKER -p $PORT -q 1 -t "$CONTROL_TOPIC" -m '{"type":"digital","pin":8,"value":1}'
sleep 2

echo "=== Step 6: Check topic subscription ==="
echo "Verifying ESP32 subscription by monitoring both topics..."
echo "Starting 10-second monitor of both status and control topics:"
echo "- Status messages indicate ESP32 is alive"
echo "- Control message echoes would indicate ESP32 is receiving"
timeout 10 mosquitto_sub -h $BROKER -p $PORT -t "$DEVICE_ID/+" -v

echo
echo "=== Debug Test Complete ==="
echo
echo "Expected ESP32 behavior for each test:"
echo "Test 1: [INFO] Received MQTT message on topic: ESP32_58B8D8/control"
echo "        [INFO] Command executed: Digital write: pin 2 = HIGH"
echo "Test 2: [INFO] Command executed: Analog write: pin 3 = 128"
echo "Test 3: [INFO] Command executed: Digital write: pin 4 = HIGH"
echo "        [INFO] Command executed: Analog write: pin 5 = 255"
echo "Test 4: [WARN] Failed to parse message: No valid commands found"
echo
echo "If ESP32 is NOT showing these logs, the issue is:"
echo "1. ESP32 is not subscribed to the control topic"
echo "2. MQTT client subscription failed"
echo "3. Message callback is not being called"
echo
echo "Next steps:"
echo "1. Check ESP32 serial output for subscription logs"
echo "2. Verify MQTT connection and subscription success"
echo "3. Check if message callback is properly set"

