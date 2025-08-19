# ESP32 MQTT Subscription Guide

## Overview

Your ESP32 is configured to subscribe to MQTT topics using the format `ESP32_58B8D8`. This guide explains how the subscription works and how to test it.

## Current Configuration

### MQTT Topics
- **Control Topic**: `ESP32_58B8D8/control` (ESP32 receives commands here)
- **Status Topic**: `ESP32_58B8D8/status` (ESP32 publishes status here)
- **Response Topic**: `ESP32_58B8D8/response` (ESP32 can publish responses here)

### MQTT Broker Settings
- **Broker**: `10.0.1.211`
- **Port**: `1883`
- **Username**: (empty)
- **Password**: (empty)

## How Subscription Works

### 1. Connection Process
When the ESP32 starts up:
1. Connects to WiFi
2. Connects to MQTT broker at `10.0.1.211:1883`
3. **Automatically subscribes** to `ESP32_58B8D8/control`
4. Publishes initial status to `ESP32_58B8D8/status`

### 2. Message Reception
Any message published to `ESP32_58B8D8/control` will be:
1. Received by the ESP32
2. Processed by the `write_income_message` function
3. Parsed as JSON
4. Executed as pin commands

### 3. Logging
The ESP32 now provides detailed logging:
```
[INFO] MQTT connected to broker
[INFO] Successfully subscribed to topic: ESP32_58B8D8/control (QoS: 0)
[INFO] MQTT subscription successful for topic: ESP32_58B8D8/control
[INFO] Received MQTT message on topic: ESP32_58B8D8/control
[INFO] Message data: {"type": "digital", "pin": 13, "value": 1}
[INFO] Command executed: Digital write: pin 13 = HIGH
```

## Testing the Subscription

### Method 1: Using the Test Script
```bash
cd test
./test_mqtt_subscription.sh
```

### Method 2: Manual Testing with mosquitto_pub

#### Install mosquitto-clients
```bash
# On macOS
brew install mosquitto

# On Ubuntu
sudo apt-get install mosquitto-clients
```

#### Send Test Commands

**Digital Command:**
```bash
mosquitto_pub -h 10.0.1.211 -p 1883 -t "ESP32_58B8D8/control" -m '{"type": "digital", "pin": 13, "value": 1}'
```

**Analog Command:**
```bash
mosquitto_pub -h 10.0.1.211 -p 1883 -t "ESP32_58B8D8/control" -m '{"type": "analog", "pin": 9, "value": 255}'
```

**Multiple Commands:**
```bash
mosquitto_pub -h 10.0.1.211 -p 1883 -t "ESP32_58B8D8/control" -m '[
  {"type": "digital", "pin": 13, "value": 1},
  {"type": "analog", "pin": 9, "value": 128},
  {"type": "digital", "pin": 12, "value": 0}
]'
```

### Method 3: Using MQTT Explorer or Similar Tools
1. Connect to broker: `10.0.1.211:1883`
2. Publish to topic: `ESP32_58B8D8/control`
3. Subscribe to topic: `ESP32_58B8D8/status` (to see ESP32 status)

## Expected Behavior

### When ESP32 Receives Commands:

**Digital Command Response:**
```
[INFO] Received MQTT message on topic: ESP32_58B8D8/control
[INFO] Message data: {"type": "digital", "pin": 13, "value": 1}
[INFO] Command executed: Digital write: pin 13 = HIGH
```

**Analog Command Response:**
```
[INFO] Received MQTT message on topic: ESP32_58B8D8/control
[INFO] Message data: {"type": "analog", "pin": 9, "value": 255}
[INFO] Command executed: Analog write: pin 9 = 255
```

**Invalid Command Response:**
```
[INFO] Received MQTT message on topic: ESP32_58B8D8/control
[INFO] Message data: {"type": "invalid", "pin": 13, "value": 1}
[WARN] Failed to parse message: No valid commands found
```

### Status Publishing:
The ESP32 automatically publishes status every 10 seconds:
```json
{
  "machine_id": "2",
  "configured_pins": 3,
  "status": "online"
}
```

## Troubleshooting

### 1. ESP32 Not Receiving Messages
- Check if ESP32 is connected to WiFi
- Check if ESP32 is connected to MQTT broker
- Verify the topic name is exactly `ESP32_58B8D8/control`
- Check broker IP address and port

### 2. Messages Not Being Processed
- Check ESP32 serial output for parsing errors
- Verify JSON format is correct
- Check if pin numbers are valid

### 3. Connection Issues
- Verify broker is running at `10.0.1.211:1883`
- Check network connectivity
- Verify no firewall blocking port 1883

## Functional Programming Benefits

The subscription system follows functional programming principles:

1. **Pure Parsing**: JSON parsing has no side effects
2. **Explicit Results**: Command execution returns explicit results
3. **Separated Concerns**: MQTT handling, parsing, and execution are separate
4. **Composable**: Each function can be tested independently

## Next Steps

1. **Flash the updated code** to your ESP32
2. **Monitor serial output** to see connection and subscription logs
3. **Test with the provided scripts** to verify functionality
4. **Send commands** to control your ESP32 pins remotely

Your ESP32 is now ready to receive commands on the `ESP32_58B8D8/control` topic! 🚀
