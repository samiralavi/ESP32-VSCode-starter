# ESP32-VSCode-starter
Starter project for developing firmware in VSCode for ESP32 system-on-chips (SoCs).

This project provides a modern C++20 foundation for ESP32 development with WiFi provisioning, HTTP server, CLI interface, and comprehensive tooling support.

## 📋 Table of Contents

- [Features](#-features)
- [Prerequisites](#-prerequisites)
- [Quick Start](#-quick-start)
- [Project Structure](#-project-structure)
- [Configuration](#-configuration)
- [Building and Flashing](#-building-and-flashing)
- [Development Workflow](#-development-workflow)
- [Debugging](#-debugging)
- [Contributing](#-contributing)

## ✨ Features

- **Modern C++20** development environment
- **WiFi Provisioning** with SoftAP scheme
- **HTTP Server** with custom handlers
- **Interactive CLI** with command menu system
- **Logging** with formatted output using espp components
- **OTA Updates** support
- **Development Container** for consistent environment
- **Automated Scripts** for building, flashing, and monitoring
- **Static Analysis** with clang-tidy integration
- **Component Management** with ESP Component Registry

## 🛠 Prerequisites

### Hardware
- ESP32, ESP32-S2, ESP32-S3, or ESP32-C3 development board
- USB cable for programming and serial communication

### Software
If using the dev container (recommended):
- [Visual Studio Code](https://code.visualstudio.com/)
- [Docker](https://www.docker.com/)
- [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)

If setting up locally:
- [ESP-IDF v5.0+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- Python 3.8+
- Git

## 🚀 Quick Start

### Option 1: Using Dev Container (Recommended)

1. **Clone the repository:**
   ```bash
   git clone https://github.com/samiralavi/ESP32-VSCode-starter.git
   cd ESP32-VSCode-starter
   ```

2. **Open in VS Code:**
   ```bash
   code .
   ```

3. **Reopen in Container:**
   - VS Code will prompt to reopen in container, or
   - Press `Ctrl+Shift+P` → "Dev Containers: Reopen in Container"

4. **Prepare the environment:**
   ```bash
   ./scripts/prepare_environment.sh
   ```

5. **Build the project:**
   ```bash
   ./scripts/build_firmware.sh
   ```

6. **Flash to your ESP32:**
   ```bash
   ./scripts/flash_firmware.sh
   ```

7. **Monitor serial output:**
   ```bash
   ./scripts/monitor_serial.sh
   ```

### Option 2: Local Setup

1. **Install ESP-IDF:**
   Follow the [official ESP-IDF installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

2. **Clone and setup:**
   ```bash
   git clone https://github.com/samiralavi/ESP32-VSCode-starter.git
   cd ESP32-VSCode-starter
   source $IDF_PATH/export.sh
   ./scripts/prepare_environment.sh
   ```

3. **Build and flash:**
   ```bash
   idf.py build
   idf.py flash monitor
   ```

## 📁 Project Structure

```
ESP32-VSCode-starter/
├── main/                    # Main application source code
│   ├── main.cpp            # Application entry point
│   ├── app.cpp/.h          # Main application logic
│   ├── http_handlers.cpp/.h # HTTP server endpoints
│   ├── blufi.cpp/.h        # BluFi provisioning (optional)
│   ├── board.cpp/.h        # Board-specific configuration
│   ├── utils.cpp/.h        # Utility functions
│   └── www/                # Web assets (if any)
├── components/             # Custom components
│   └── activecpp/          # ActiveC++ component integration
├── managed_components/     # ESP Component Registry dependencies
├── scripts/                # Build and development scripts
│   ├── build_firmware.sh   # Build the firmware
│   ├── flash_firmware.sh   # Flash firmware to device
│   ├── monitor_serial.sh   # Monitor serial output
│   ├── prepare_environment.sh # Setup development environment
│   └── run_clang_tidy.sh   # Run static analysis
├── containers/dev/         # Development container configuration
├── CMakeLists.txt          # Main CMake configuration
├── sdkconfig.defaults      # Default ESP-IDF configuration
└── partitions.csv          # Flash partition table
```

## ⚙️ Configuration

### Device Configuration

Edit the configuration in `main/Kconfig.projbuild` or use `idf.py menuconfig`:

```bash
idf.py menuconfig
```

Key configurations:
- **Device ID**: Unique identifier for your device
- **MAC Address**: Device MAC address
- **WiFi Password**: Default WiFi password for provisioning
- **SSID Prefix**: Prefix for the device's AP mode SSID

### Build Configuration

Modify `CMakeLists.txt` to add or remove ESP-IDF components:

```cmake
set(COMPONENTS
  main esptool_py activecpp wifi_provisioning esp_http_server cxx 
  # Add your components here
)
```

## 🔨 Building and Flashing

### Using Scripts (Recommended)

```bash
# Build firmware
./scripts/build_firmware.sh

# Flash firmware to device
./scripts/flash_firmware.sh

# Monitor serial output
./scripts/monitor_serial.sh

# All-in-one: build, flash, and monitor
./scripts/build_firmware.sh && ./scripts/flash_firmware.sh && ./scripts/monitor_serial.sh
```

### Using ESP-IDF Commands

```bash
# Configure target (ESP32/ESP32-S2/ESP32-S3/ESP32-C3)
idf.py set-target esp32s3

# Configure project
idf.py menuconfig

# Build
idf.py build

# Flash
idf.py flash

# Monitor
idf.py monitor

# Combined flash and monitor
idf.py flash monitor
```

## 💻 Development Workflow

### Adding New Features

1. **Create source files** in `main/` directory
2. **Update CMakeLists.txt** in `main/` to include new files
3. **Add component dependencies** in root `CMakeLists.txt` if needed
4. **Build and test** your changes

### Adding Components

1. **ESP Component Registry:**
   ```bash
   idf.py add-dependency "espressif/component_name"
   ```

2. **Local components:**
   - Add to `components/` directory
   - Include in `EXTRA_COMPONENT_DIRS` in root CMakeLists.txt

### Code Quality

Run static analysis:
```bash
./scripts/run_clang_tidy.sh
```

## 🐛 Debugging

### Serial Monitoring
```bash
./scripts/monitor_serial.sh
# or
idf.py monitor
```

### Enable Debug Logs
In `sdkconfig.defaults` or via menuconfig:
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y
```

### GDB Debugging
```bash
# Start GDB server (in another terminal)
openocd -f board/esp32-wrover-kit-3.3v.cfg

# Start GDB session
xtensa-esp32-elf-gdb build/esp32_starter.elf
```

## 📱 Usage

Once flashed and running:

1. **WiFi Provisioning:**
   - Device creates AP with SSID: `ACTIVECPP_<device_id>`
   - Connect and provision WiFi credentials

2. **Web Interface:**
   - Access via device IP after WiFi connection
   - HTTP server provides REST endpoints

3. **CLI Interface:**
   - Available via serial console
   - Interactive command system with help

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run clang-tidy: `./scripts/run_clang_tidy.sh`
5. Test your changes
6. Submit a pull request

## 📄 License

This project is licensed under the terms specified in the LICENSE file.

---

**Happy ESP32 Development!** 🚀
