# ESP32 MQTT Receiver - Dependency Installer

This project includes an automated dependency installer that sets up all required tools and libraries for ESP32 development.

## 🚀 Quick Start

### Option 1: Using the Shell Script (Recommended)
```bash
./install_dependencies.sh
```

### Option 2: Using Python Directly
```bash
python3 install_dependencies.py
```

## 📋 What Gets Installed

### System Dependencies
- **Python 3.7+** - Required for ESP-IDF and development tools
- **Git** - For version control and downloading ESP-IDF
- **CMake** - Build system for ESP-IDF
- **Ninja** - Fast build system

### Python Packages
- **pyserial** - Serial communication with ESP32
- **pyyaml** - YAML configuration file handling
- **click** - Command-line interface utilities
- **colorama** - Colored terminal output
- **requests** - HTTP requests for downloads

### ESP-IDF Framework
- **ESP-IDF v5.5.0** - Official ESP32 development framework
- **esptool.py** - ESP32 flashing and debugging tools
- **idf.py** - ESP-IDF build and project management

### Project Dependencies
- **Managed Components** - Project-specific ESP-IDF components
- **Environment Setup** - Automatic PATH and environment configuration

## 🔧 Installation Process

The installer performs the following steps:

1. **System Check** - Verifies Python version and system tools
2. **System Dependencies** - Installs missing system tools (Git, CMake, Ninja)
3. **Python Packages** - Installs required Python packages via pip
4. **ESP-IDF Check** - Detects existing ESP-IDF installation
5. **ESP-IDF Installation** - Downloads and installs ESP-IDF if needed
6. **Environment Setup** - Creates environment configuration scripts
7. **Project Dependencies** - Installs project-specific managed components

## 📁 Generated Files

After successful installation, the following files are created:

- `setup_env.sh` - Environment setup script
- `installation_summary.json` - Installation summary and next steps
- `requirements.txt` - Python dependencies list

## 🎯 Next Steps After Installation

1. **Source the environment:**
   ```bash
   source setup_env.sh
   ```

2. **Build the project:**
   ```bash
   idf.py build
   ```

3. **Flash the device:**
   ```bash
   ./flash_interactive.sh
   ```

4. **Monitor output:**
   ```bash
   ./monitor.sh
   ```

## 🛠️ Manual Installation (if needed)

If the automated installer fails, you can install dependencies manually:

### macOS
```bash
# Install Homebrew if not installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install system dependencies
brew install git cmake ninja python3

# Install Python packages
pip3 install pyserial pyyaml click colorama requests
```

### Linux (Ubuntu/Debian)
```bash
# Update package list
sudo apt-get update

# Install system dependencies
sudo apt-get install -y git cmake ninja-build python3 python3-pip

# Install Python packages
pip3 install pyserial pyyaml click colorama requests
```

### ESP-IDF Manual Installation
```bash
# Create ESP directory
mkdir -p ~/esp
cd ~/esp

# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install ESP-IDF tools
./install.sh esp32

# Set up environment
source export.sh
```

## 🔍 Troubleshooting

### Common Issues

1. **Python version too old**
   - Install Python 3.7 or newer
   - macOS: `brew install python3`
   - Linux: `sudo apt-get install python3`

2. **Permission denied errors**
   - Use `sudo` for system package installation
   - Ensure you have write permissions to the project directory

3. **ESP-IDF not found**
   - The installer will automatically download and install ESP-IDF
   - If manual installation is needed, follow the ESP-IDF manual installation steps

4. **Network issues during download**
   - Check your internet connection
   - Try using a VPN if behind a corporate firewall
   - Download ESP-IDF manually and place it in `~/esp/esp-idf`

### Getting Help

If you encounter issues:

1. Check the error messages in the installer output
2. Verify your system meets the minimum requirements
3. Try running the installer with verbose output
4. Check the `installation_summary.json` file for details

## 📝 Requirements

### Minimum System Requirements
- **OS**: macOS 10.14+, Ubuntu 18.04+, or Windows 10+
- **Python**: 3.7 or newer
- **RAM**: 4GB minimum, 8GB recommended
- **Disk Space**: 2GB for ESP-IDF, 1GB for project

### Supported Platforms
- ✅ macOS (Intel/Apple Silicon)
- ✅ Ubuntu/Debian Linux
- ⚠️ Windows (may require manual setup)

## 🔄 Updating Dependencies

To update dependencies:

1. **Update Python packages:**
   ```bash
   pip3 install --upgrade -r requirements.txt
   ```

2. **Update ESP-IDF:**
   ```bash
   cd ~/esp/esp-idf
   git pull
   ./install.sh esp32
   ```

3. **Update project components:**
   ```bash
   idf.py reconfigure
   ```

## 📚 Additional Resources

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [ESP32 Development Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ESP-IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)

