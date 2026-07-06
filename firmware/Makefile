# Skeleton Eyes ESP32-S3 Makefile
# For ESP32-S3 SuperMini with dual GC9A01 displays

# Project configuration
PROJECT_NAME = skeleton-eyes
PLATFORM = espressif32
BOARD = esp32-s3-devkitc-1
ENVIRONMENT = esp32-s3-devkitc-1

# PlatformIO executable (adjust path if needed)
PIO = pio

# Default target
.PHONY: all
all: build

# Build the project
.PHONY: build compile
build compile:
	@echo "Building $(PROJECT_NAME)..."
	$(PIO) run -e $(ENVIRONMENT)

# Clean build files
.PHONY: clean
clean:
	@echo "Cleaning build files..."
	$(PIO) run -t clean -e $(ENVIRONMENT)

# Upload firmware to device
.PHONY: upload flash
upload flash:
	@echo "Uploading firmware to ESP32-S3..."
	$(PIO) run -t upload -e $(ENVIRONMENT)

# OTA upload: make ota-upload ESP_IP=192.168.1.100
.PHONY: ota-upload ota-deploy
ota-upload:
	@echo "Uploading firmware via OTA to $(ESP_IP)..."
	$(PIO) run -t upload --upload-port $(ESP_IP) -e $(ENVIRONMENT)

ota-deploy: build ota-upload

# Build and upload in one command
.PHONY: deploy
deploy: build upload

# Monitor serial output
.PHONY: monitor serial
monitor serial:
	@echo "Starting serial monitor..."
	$(PIO) device monitor -e $(ENVIRONMENT)

# Build, upload, and monitor
.PHONY: run
run: deploy monitor

# Install/update dependencies
.PHONY: deps dependencies
deps dependencies:
	@echo "Installing/updating dependencies..."
	$(PIO) lib install

# List connected devices
.PHONY: devices
devices:
	@echo "Connected devices:"
	$(PIO) device list

# Check code (lint/format)
.PHONY: check
check:
	@echo "Checking code..."
	$(PIO) check -e $(ENVIRONMENT)

# Get project info
.PHONY: info
info:
	@echo "Project: $(PROJECT_NAME)"
	@echo "Platform: $(PLATFORM)"
	@echo "Board: $(BOARD)"
	@echo "Environment: $(ENVIRONMENT)"
	$(PIO) project config

# Erase flash memory
.PHONY: erase
erase:
	@echo "Erasing flash memory..."
	$(PIO) run -t erase -e $(ENVIRONMENT)

# Update PlatformIO and platforms
.PHONY: update
update:
	@echo "Updating PlatformIO..."
	$(PIO) upgrade
	$(PIO) platform update

# Full clean rebuild
.PHONY: rebuild
rebuild: clean build

# Initialize project (run after cloning)
.PHONY: init setup
init setup:
	@echo "Initializing project..."
	$(PIO) project init --board $(BOARD)
	$(PIO) lib install

# Sprite generation targets
.PHONY: sprites sprites-test sprites-clean
sprites:
	@echo "Converting images to sprites..."
	@if [ ! -d "sprites" ] || [ -z "$$(ls -A sprites/ 2>/dev/null)" ]; then \
		echo "No images found in sprites/ — generating test sprites."; \
		python3 create_test_sprites.py; \
	fi
	python3 convert_sprites.py $(HALF)
	@echo "Done. Use 'make build' && make upload' to deploy."

sprites-half:
	$(MAKE) sprites HALF=--half

sprites-clean:
	@echo "Cleaning generated sprite data..."
	rm -f src/generated_sprites.h
	@echo "Sprite data cleaned."

sprites-info:
	@echo "Sprite System Information:"
	@echo "========================="
	@echo "Place PNG/JPG/BMP images in sprites/ (240x240 recommended)."
	@echo "They are sorted alphabetically and indexed 0, 1, 2, ..."
	@echo "Select by index via I2C command 0x18 (SET_SPRITE)."
	@echo ""
	@echo "Resolution:"
	@echo "  make sprites        - Full resolution (240x240, ~23KB per sprite)"
	@echo "  make sprites-half   - Half resolution (120x120, ~7KB per sprite)"
	@echo "  Half res stores 1/4 the pixels and upscales 2x at runtime."
	@echo "  You get ~3-4x more sprites for the same flash budget."
	@echo ""
	@echo "Commands:"
	@echo "  make sprites       - Convert images at full resolution"
	@echo "  make sprites-half  - Convert images at half resolution"
	@echo "  make sprites-test  - Generate test sprites"  
	@echo "  make sprites-clean - Remove generated sprite data"
	@echo "  make sprites-info  - Show this information"

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  build/compile - Build the project"
	@echo "  clean         - Clean build files"
	@echo "  upload/flash  - Upload firmware to device"
	@echo "  ota-upload    - Upload via OTA (set ESP_IP=x.x.x.x)"
	@echo "  ota-deploy    - Build + OTA upload"
	@echo "  deploy        - Build and upload (serial)"
	@echo "  monitor/serial- Start serial monitor"
	@echo "  run           - Build, upload, and monitor"
	@echo "  deps          - Install/update dependencies"
	@echo "  devices       - List connected devices"
	@echo "  check         - Check/lint code"
	@echo "  info          - Show project information"
	@echo "  erase         - Erase flash memory"
	@echo "  update        - Update PlatformIO and platforms"
	@echo "  rebuild       - Clean and rebuild"
	@echo "  init/setup    - Initialize project"
	@echo "  sprites       - Convert images to sprites"
	@echo "  sprites-info  - Show sprite system info"
	@echo "  help          - Show this help"

# Development targets
.PHONY: dev-build dev-upload dev-monitor
dev-build:
	$(PIO) run -e $(ENVIRONMENT) -v

dev-upload:
	$(PIO) run -t upload -e $(ENVIRONMENT) -v

dev-monitor:
	$(PIO) device monitor -e $(ENVIRONMENT) --baud 115200
