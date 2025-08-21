#!/bin/bash

# ESP32 MQTT Receiver - Dependency Installer Wrapper
# This script runs the Python dependency installer

echo "🚀 ESP32 MQTT Receiver - Dependency Installer"
echo "=============================================="

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: Python 3 is required but not found."
    echo "   Please install Python 3 and try again."
    exit 1
fi

# Check if the Python script exists
if [ ! -f "install_dependencies.py" ]; then
    echo "❌ Error: install_dependencies.py not found."
    echo "   Please make sure you're in the project root directory."
    exit 1
fi

# Make the Python script executable
chmod +x install_dependencies.py

# Run the Python installer
echo "🔧 Running dependency installer..."
python3 install_dependencies.py

# Check the exit code
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Installation completed successfully!"
    echo ""
    echo "💡 Next steps:"
    echo "   1. Source the environment: source setup_env.sh"
    echo "   2. Build the project: idf.py build"
    echo "   3. Flash the device: ./flash_interactive.sh"
    echo "   4. Monitor output: ./monitor.sh"
else
    echo ""
    echo "❌ Installation failed. Please check the errors above."
    exit 1
fi

