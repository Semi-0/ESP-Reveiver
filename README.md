# ESP32 MQTT Project with Juniper

This project demonstrates using the **Juniper functional reactive programming language** to create an ESP32 application with WiFi, MQTT, mDNS, and JSON parsing capabilities.

## What is Juniper?

Juniper is a functional reactive programming (FRP) language designed specifically for microcontrollers like Arduino and ESP32. It provides:

- ✅ **Functional Programming** - Pure functions, immutability, composition
- ✅ **Reactive Programming** - Event-driven, time-based programming
- ✅ **Pipe Operators** (`|`) - Chain functions together
- ✅ **Monads** - Maybe, Result, List monads for error handling
- ✅ **Compiles to C++** - Generates Arduino-compatible C++ code
- ✅ **ESP32 Support** - Full ESP32 library integration

## Project Features

- 🌐 **WiFi Connectivity** - Automatic WiFi connection management
- 📡 **MQTT Communication** - Subscribe to topics and process JSON messages
- 🔍 **mDNS Service Discovery** - Discover services on the network
- 📝 **JSON Parsing** - Parse and execute JSON commands
- 💡 **LED Status Indicators** - Visual feedback for system status
- 🔧 **Functional Reactive Programming** - Clean, composable code

## Project Structure

```
juniper_esp32_project/
├── esp32_mqtt_project.jun    # Main Juniper source file
├── platformio.ini           # PlatformIO configuration
├── juniper_build.py         # Build script for Juniper compilation
├── README.md               # This file
└── juniper-compiler/       # Juniper compiler (cloned from GitHub)
    └── Juniper/
        ├── cppstd/
        │   └── juniper.hpp  # Juniper runtime library
        └── examples/        # Example Juniper programs
```

## Setup Instructions

### Option 1: Quick Start (Fallback Mode)

If you don't want to build the Juniper compiler, the project includes a fallback mode:

```bash
# Install PlatformIO
pip install platformio

# Build and upload
platformio run
platformio run --target upload
platformio device monitor
```

The fallback mode will automatically generate C++ code from your Juniper files.

### Option 2: Full Juniper Setup

#### 1. Install Dependencies

```bash
# Install Mono (for Juniper compiler)
brew install mono

# Install PlatformIO
pip install platformio

# Install .NET (if needed for F#)
# Download from https://dotnet.microsoft.com/download
```

#### 2. Build Juniper Compiler (Optional)

```bash
# Navigate to Juniper compiler
cd juniper-compiler/Juniper

# Build with .NET (if you have it installed)
dotnet build

# Or use the pre-built version (if available)
# The project will automatically detect and use it
```

#### 3. Build and Upload

```bash
# Build the project
platformio run

# Upload to ESP32
platformio run --target upload

# Monitor serial output
platformio device monitor
```

## Juniper Code Example

Here's the main Juniper program (`esp32_mqtt_project.jun`):

```juniper
module ESP32MQTTProject
open(Prelude, Io, Time, Network, Json, Mqtt, Mdns)

-- Pin definitions
let ledPin : int16 = 2
let statusLed : int16 = 13

-- WiFi configuration
let wifiSSID : string = "V2_ Lab"
let wifiPassword : string = "end-of-file"

-- MQTT configuration
let mqttBroker : string = "10.0.1.211"
let mqttPort : uint16 = 1883
let mqttTopic : string = "ESP32_58B8D8/control"

-- JSON parsing with functional approach
fun parseJsonMessage(jsonStr : string) : maybe<jsonCommand> = (
    let parsed = Json:parse(jsonStr)
    case parsed of
        | Json:object(fields) -> (
            let cmdType = Json:getString(fields, "type")
            let pin = Json:getInt(fields, "pin")
            let value = Json:getInt(fields, "value")
            case cmdType of
                | "digital" -> Json:just(Json:digitalCommand(pin, value))
                | "analog" -> Json:just(Json:analogCommand(pin, value))
                | _ -> Json:nothing()
            end
        )
        | _ -> Json:nothing()
    end
)

-- MQTT message processing pipeline
fun processMqttMessage(message : string) : unit = (
    let parsedCommand = parseJsonMessage(message)
    case parsedCommand of
        | Json:just(cmd) -> (
            case cmd of
                | Json:digitalCommand(pin, value) -> executeDigitalCommand(pin, value)
                | Json:analogCommand(pin, value) -> executeAnalogCommand(pin, value)
            end
        )
        | Json:nothing() -> Io:log("Invalid JSON message")
    end
)
```

## Functional Programming Features

### 1. Pipe Operators

```juniper
-- Chain functions together
let result = input
    |> parseJsonMessage
    |> validateCommand
    |> executeCommand
    |> logResult
```

### 2. Function Composition

```juniper
-- Compose multiple functions
let pipeline = compose(
    parseJsonMessage,
    validateCommand,
    executeCommand,
    logResult
)
```

### 3. Monads for Error Handling

```juniper
-- Maybe monad for optional values
let maybeResult = Just(input)
    >>= parseJsonMessage
    >>= validateCommand
    >>= executeCommand

-- Result monad for error handling
let result = Ok(input)
    >>= parseJsonMessage
    >>= validateCommand
    >>= executeCommand
```

## Usage

### 1. Configure WiFi and MQTT

Edit the configuration in `esp32_mqtt_project.jun`:

```juniper
-- WiFi configuration
let wifiSSID : string = "Your_WiFi_SSID"
let wifiPassword : string = "Your_WiFi_Password"

-- MQTT configuration
let mqttBroker : string = "Your_MQTT_Broker_IP"
let mqttPort : uint16 = 1883
let mqttTopic : string = "Your_Device_Topic"
```

### 2. Send MQTT Commands

Send JSON messages to your MQTT topic:

```json
{"type": "digital", "pin": 2, "value": 1}
```

```json
{"type": "analog", "pin": 25, "value": 255}
```

### 3. Monitor Output

The ESP32 will:
- Connect to WiFi automatically
- Connect to MQTT broker
- Subscribe to the configured topic
- Parse and execute JSON commands
- Provide visual feedback via LEDs

## Troubleshooting

### Common Issues

1. **Juniper Compiler Not Found**
   - The project includes a fallback mode
   - It will automatically generate C++ code
   - Check the build output for details

2. **WiFi Connection Issues**
   - Verify WiFi credentials in the Juniper file
   - Check network availability
   - Monitor serial output for connection status

3. **MQTT Connection Issues**
   - Verify MQTT broker IP and port
   - Check network connectivity
   - Ensure MQTT broker is running

4. **Compilation Errors**
   - Ensure PlatformIO is installed
   - Check that ESP32 board support is installed
   - Verify all required libraries are available

### Debugging

- Enable debug output in `platformio.ini`:
  ```ini
  build_flags = -DCORE_DEBUG_LEVEL=5
  ```
- Monitor serial output for detailed logs
- Check LED status indicators

## Next Steps

1. **Extend the Project**
   - Add more sensor support
   - Implement additional MQTT topics
   - Add more complex functional patterns

2. **Learn Juniper**
   - Study the examples in `juniper-compiler/Juniper/examples/`
   - Read the grammar in `juniper-compiler/grammar.bnf`
   - Experiment with functional reactive programming

3. **Contribute**
   - Report issues with the Juniper compiler
   - Improve the build scripts
   - Add more examples

## Resources

- [Juniper GitHub Repository](https://github.com/davzucky/Juniper)
- [Functional Reactive Programming](https://en.wikipedia.org/wiki/Functional_reactive_programming)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [PlatformIO Documentation](https://docs.platformio.org/)

## License

This project is licensed under the MIT License. The Juniper compiler is licensed under its own terms.
