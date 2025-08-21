#!/usr/bin/env python3
"""
ESP32 MQTT Receiver - Dependency Installer
Automatically installs all required dependencies for the project.
"""

import os
import sys
import subprocess
import platform
import json
import shutil
from pathlib import Path
from typing import List, Dict, Optional, Tuple
import urllib.request
import zipfile
import tarfile

class DependencyInstaller:
    def __init__(self):
        self.project_root = Path.cwd()
        self.esp_idf_path = None
        self.system = platform.system().lower()
        self.arch = platform.machine().lower()
        
        # Colors for output
        self.colors = {
            'red': '\033[91m',
            'green': '\033[92m',
            'yellow': '\033[93m',
            'blue': '\033[94m',
            'purple': '\033[95m',
            'cyan': '\033[96m',
            'white': '\033[97m',
            'bold': '\033[1m',
            'end': '\033[0m'
        }
        
        # Dependencies to check and install
        self.dependencies = {
            'system': {
                'python': {'min_version': '3.7', 'check_cmd': 'python3 --version'},
                'pip': {'check_cmd': 'pip3 --version'},
                'git': {'check_cmd': 'git --version'},
                'cmake': {'check_cmd': 'cmake --version'},
                'ninja': {'check_cmd': 'ninja --version'},
            },
            'esp_idf': {
                'idf.py': {'check_cmd': 'idf.py --version'},
                'esptool.py': {'check_cmd': 'esptool.py version'},
                'esp-idf': {'version': 'v5.5.0', 'path': '~/esp/esp-idf'}
            },
            'python_packages': [
                'pyserial',
                'pyyaml',
                'click',
                'colorama',
                'requests'
            ],
            'project_specific': {
                'idf_component_manager': {'check_cmd': 'idf.py reconfigure'},
                'managed_components': {'path': 'managed_components'}
            }
        }

    def print_colored(self, text: str, color: str = 'white', bold: bool = False):
        """Print colored text to console."""
        color_code = self.colors.get(color, '')
        bold_code = self.colors.get('bold', '') if bold else ''
        print(f"{bold_code}{color_code}{text}{self.colors['end']}")

    def print_header(self, text: str):
        """Print a header with formatting."""
        self.print_colored(f"\n{'='*60}", 'cyan', bold=True)
        self.print_colored(f"  {text}", 'cyan', bold=True)
        self.print_colored(f"{'='*60}", 'cyan', bold=True)

    def print_step(self, text: str):
        """Print a step with formatting."""
        self.print_colored(f"\n🔧 {text}", 'blue')

    def print_success(self, text: str):
        """Print success message."""
        self.print_colored(f"✅ {text}", 'green')

    def print_warning(self, text: str):
        """Print warning message."""
        self.print_colored(f"⚠️  {text}", 'yellow')

    def print_error(self, text: str):
        """Print error message."""
        self.print_colored(f"❌ {text}", 'red')

    def run_command(self, cmd: str, capture_output: bool = True, check: bool = True) -> Tuple[int, str, str]:
        """Run a shell command and return result."""
        try:
            result = subprocess.run(
                cmd, 
                shell=True, 
                capture_output=capture_output, 
                text=True,
                check=check
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.CalledProcessError as e:
            return e.returncode, e.stdout, e.stderr

    def check_command_exists(self, cmd: str) -> bool:
        """Check if a command exists in PATH."""
        return shutil.which(cmd.split()[0]) is not None

    def check_python_version(self) -> bool:
        """Check if Python version meets requirements."""
        try:
            version = sys.version_info
            required = tuple(map(int, self.dependencies['system']['python']['min_version'].split('.')))
            return version >= required
        except Exception:
            return False

    def install_system_dependencies(self) -> bool:
        """Install system-level dependencies."""
        self.print_header("Installing System Dependencies")
        
        # Check Python
        if not self.check_python_version():
            self.print_error(f"Python {self.dependencies['system']['python']['min_version']}+ required")
            return False
        self.print_success(f"Python {sys.version.split()[0]} detected")

        # Check and install system tools
        system_tools = {
            'macos': {
                'git': 'brew install git',
                'cmake': 'brew install cmake',
                'ninja': 'brew install ninja'
            },
            'linux': {
                'git': 'sudo apt-get update && sudo apt-get install -y git',
                'cmake': 'sudo apt-get install -y cmake',
                'ninja': 'sudo apt-get install -y ninja-build'
            }
        }

        for tool, check_cmd in self.dependencies['system'].items():
            if tool == 'python':
                continue
                
            self.print_step(f"Checking {tool}...")
            if self.check_command_exists(tool):
                self.print_success(f"{tool} already installed")
            else:
                self.print_warning(f"{tool} not found")
                if self.system in system_tools and tool in system_tools[self.system]:
                    install_cmd = system_tools[self.system][tool]
                    self.print_step(f"Installing {tool}...")
                    returncode, stdout, stderr = self.run_command(install_cmd, check=False)
                    if returncode == 0:
                        self.print_success(f"{tool} installed successfully")
                    else:
                        self.print_error(f"Failed to install {tool}: {stderr}")
                        return False
                else:
                    self.print_error(f"Please install {tool} manually")
                    return False

        return True

    def install_python_packages(self) -> bool:
        """Install Python packages."""
        self.print_header("Installing Python Packages")
        
        for package in self.dependencies['python_packages']:
            self.print_step(f"Installing {package}...")
            returncode, stdout, stderr = self.run_command(f"pip3 install {package}", check=False)
            if returncode == 0:
                self.print_success(f"{package} installed successfully")
            else:
                self.print_warning(f"Failed to install {package}: {stderr}")
                # Continue with other packages

        return True

    def check_esp_idf_installation(self) -> bool:
        """Check if ESP-IDF is installed."""
        self.print_header("Checking ESP-IDF Installation")
        
        # Check if idf.py is available
        if self.check_command_exists('idf.py'):
            self.print_success("ESP-IDF found in PATH")
            return True
            
        # Check common installation paths
        common_paths = [
            Path.home() / "esp" / "esp-idf",
            Path.home() / "esp" / "v5.5" / "esp-idf",
            Path("/opt/esp/esp-idf"),
            Path("/usr/local/esp/esp-idf")
        ]
        
        for path in common_paths:
            if path.exists():
                self.print_success(f"ESP-IDF found at {path}")
                self.esp_idf_path = path
                return True
                
        self.print_warning("ESP-IDF not found")
        return False

    def install_esp_idf(self) -> bool:
        """Install ESP-IDF."""
        self.print_header("Installing ESP-IDF")
        
        esp_dir = Path.home() / "esp"
        esp_dir.mkdir(exist_ok=True)
        
        # Download and install ESP-IDF
        idf_version = self.dependencies['esp_idf']['esp-idf']['version']
        idf_url = f"https://github.com/espressif/esp-idf/archive/refs/tags/{idf_version}.zip"
        idf_path = esp_dir / "esp-idf"
        
        self.print_step(f"Downloading ESP-IDF {idf_version}...")
        
        try:
            # Download ESP-IDF
            zip_path = esp_dir / f"esp-idf-{idf_version}.zip"
            urllib.request.urlretrieve(idf_url, zip_path)
            
            # Extract
            self.print_step("Extracting ESP-IDF...")
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                zip_ref.extractall(esp_dir)
            
            # Rename if needed
            extracted_path = esp_dir / f"esp-idf-{idf_version}"
            if extracted_path.exists() and not idf_path.exists():
                extracted_path.rename(idf_path)
            
            # Clean up
            zip_path.unlink()
            
            self.print_success("ESP-IDF downloaded successfully")
            
            # Install ESP-IDF tools
            self.print_step("Installing ESP-IDF tools...")
            install_script = idf_path / "install.sh"
            if install_script.exists():
                returncode, stdout, stderr = self.run_command(f"cd {idf_path} && ./install.sh esp32", check=False)
                if returncode == 0:
                    self.print_success("ESP-IDF tools installed successfully")
                    return True
                else:
                    self.print_error(f"Failed to install ESP-IDF tools: {stderr}")
                    return False
            else:
                self.print_error("ESP-IDF install script not found")
                return False
                
        except Exception as e:
            self.print_error(f"Failed to download/install ESP-IDF: {e}")
            return False

    def setup_esp_idf_environment(self) -> bool:
        """Set up ESP-IDF environment."""
        self.print_header("Setting up ESP-IDF Environment")
        
        if not self.esp_idf_path:
            self.esp_idf_path = Path.home() / "esp" / "esp-idf"
        
        if not self.esp_idf_path.exists():
            self.print_error("ESP-IDF path not found")
            return False
            
        # Create environment setup script
        env_script = self.project_root / "setup_env.sh"
        env_content = f"""#!/bin/bash
# ESP-IDF Environment Setup
export IDF_PATH="{self.esp_idf_path}"
export PATH="$IDF_PATH/tools:$PATH"
source "$IDF_PATH/export.sh"
"""
        
        with open(env_script, 'w') as f:
            f.write(env_content)
        
        os.chmod(env_script, 0o755)
        self.print_success("Environment setup script created")
        
        return True

    def install_project_dependencies(self) -> bool:
        """Install project-specific dependencies."""
        self.print_header("Installing Project Dependencies")
        
        # Check if we're in an ESP-IDF project
        if not (self.project_root / "CMakeLists.txt").exists():
            self.print_error("Not in an ESP-IDF project directory")
            return False
            
        # Install managed components
        self.print_step("Installing managed components...")
        returncode, stdout, stderr = self.run_command("idf.py reconfigure", check=False)
        if returncode == 0:
            self.print_success("Managed components installed successfully")
        else:
            self.print_warning(f"Failed to install managed components: {stderr}")
            
        return True

    def create_installation_summary(self) -> None:
        """Create installation summary."""
        self.print_header("Installation Summary")
        
        summary = {
            "project": "ESP32 MQTT Receiver",
            "dependencies_installed": True,
            "esp_idf_path": str(self.esp_idf_path) if self.esp_idf_path else None,
            "setup_script": "setup_env.sh",
            "next_steps": [
                "Source the environment: source setup_env.sh",
                "Build the project: idf.py build",
                "Flash the device: ./flash_interactive.sh",
                "Monitor output: ./monitor.sh"
            ]
        }
        
        # Save summary to file
        summary_file = self.project_root / "installation_summary.json"
        with open(summary_file, 'w') as f:
            json.dump(summary, f, indent=2)
            
        self.print_success("Installation summary saved to installation_summary.json")
        
        # Print next steps
        self.print_colored("\n🎉 Installation completed successfully!", 'green', bold=True)
        self.print_colored("\nNext steps:", 'cyan', bold=True)
        for i, step in enumerate(summary["next_steps"], 1):
            self.print_colored(f"  {i}. {step}", 'white')

    def run(self) -> bool:
        """Run the complete installation process."""
        self.print_header("ESP32 MQTT Receiver - Dependency Installer")
        
        try:
            # Install system dependencies
            if not self.install_system_dependencies():
                return False
                
            # Install Python packages
            if not self.install_python_packages():
                return False
                
            # Check/Install ESP-IDF
            if not self.check_esp_idf_installation():
                if not self.install_esp_idf():
                    return False
                    
            # Setup ESP-IDF environment
            if not self.setup_esp_idf_environment():
                return False
                
            # Install project dependencies
            if not self.install_project_dependencies():
                return False
                
            # Create summary
            self.create_installation_summary()
            
            return True
            
        except KeyboardInterrupt:
            self.print_error("\nInstallation cancelled by user")
            return False
        except Exception as e:
            self.print_error(f"Installation failed: {e}")
            return False

def main():
    """Main entry point."""
    installer = DependencyInstaller()
    success = installer.run()
    
    if success:
        print("\n" + "="*60)
        installer.print_colored("🎉 All dependencies installed successfully!", 'green', bold=True)
        print("="*60)
        sys.exit(0)
    else:
        print("\n" + "="*60)
        installer.print_colored("❌ Installation failed. Please check the errors above.", 'red', bold=True)
        print("="*60)
        sys.exit(1)

if __name__ == "__main__":
    main()

