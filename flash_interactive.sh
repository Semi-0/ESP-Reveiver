#!/bin/bash

# Interactive ESP32 Flash Script
# Automatically detects available ports and allows user selection

echo "🔍 ESP32 Interactive Flash Script"
echo "=================================="

# Function to detect available serial ports
detect_ports() {
    echo "📡 Detecting available serial ports..."
    echo ""
    
    # Get all available serial ports
    ports=($(ls /dev/cu.* 2>/dev/null | grep -E "usbserial|usbmodem"))
    
    if [ ${#ports[@]} -eq 0 ]; then
        echo "❌ No USB serial ports found!"
        echo "   Please connect your ESP32 device and try again."
        exit 1
    fi
    
    echo "Available ports:"
    echo "----------------"
    
    for i in "${!ports[@]}"; do
        echo "[$((i+1))] ${ports[$i]}"
    done
    
    echo ""
}

# Function to test if a port is responsive (optional)
test_port() {
    local port=$1
    echo "🔧 Testing port $port..."
    
    # Try to get basic info from the port
    if command -v esptool.py >/dev/null 2>&1; then
        if esptool.py --chip esp32 -p "$port" chip_id >/dev/null 2>&1; then
            echo "✅ Port $port appears to be responsive"
            return 0
        else
            echo "⚠️  Port $port may not be responsive (but will try anyway)"
            return 1
        fi
    else
        echo "ℹ️  esptool.py not found, skipping port test"
        return 0
    fi
}

# Function to flash the selected port
flash_device() {
    local port=$1
    echo ""
    echo "🚀 Flashing device on port: $port"
    echo "=================================="
    
    # Unset any conflicting environment variables
    unset ESPPORT
    
    # Flash the device
    if idf.py -p "$port" flash; then
        echo ""
        echo "✅ Flash completed successfully!"
        echo "   Device should now be running."
        echo ""
        echo "💡 To monitor the device output, run:"
        echo "   ./monitor.sh"
    else
        echo ""
        echo "❌ Flash failed!"
        echo "   Please check your connection and try again."
        exit 1
    fi
}

# Main script logic
main() {
    # Check if we're in the right directory
    if [ ! -f "CMakeLists.txt" ] && [ ! -f "main/CMakeLists.txt" ]; then
        echo "❌ Error: This doesn't appear to be an ESP-IDF project directory."
        echo "   Please run this script from your project root."
        exit 1
    fi
    
    # Check if idf.py is available
    if ! command -v idf.py >/dev/null 2>&1; then
        echo "❌ Error: idf.py not found!"
        echo "   Please make sure ESP-IDF is installed and in your PATH."
        echo "   You can source it with: source ~/esp/esp-idf/export.sh"
        exit 1
    fi
    
    # Detect available ports
    detect_ports
    
    # Get user selection
    while true; do
        read -p "Select port number (1-${#ports[@]}) or 'q' to quit: " choice
        
        if [ "$choice" = "q" ] || [ "$choice" = "Q" ]; then
            echo "👋 Goodbye!"
            exit 0
        fi
        
        # Validate input
        if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "${#ports[@]}" ]; then
            selected_port="${ports[$((choice-1))]}"
            break
        else
            echo "❌ Invalid selection. Please enter a number between 1 and ${#ports[@]}."
        fi
    done
    
    # Test the selected port
    test_port "$selected_port"
    
    # Confirm before flashing
    echo ""
    read -p "Proceed with flashing to $selected_port? (y/N): " confirm
    
    # Trim whitespace and check for y/Y
    confirm=$(echo "$confirm" | tr '[:upper:]' '[:lower:]' | xargs)
    
    if [[ "$confirm" = "y" ]] || [[ "$confirm" = "yes" ]]; then
        flash_device "$selected_port"
    else
        echo "👋 Flash cancelled."
        exit 0
    fi
}

# Run the main function
main "$@"
