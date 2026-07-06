# I2C Eye Controller Makefile
# For ESP32-C3 LOLIN C3 Mini

PROJECT_NAME = eye-controller
ENVIRONMENT = lolin_c3_mini
PIO = pio

.PHONY: all
all: build

.PHONY: build compile
build compile:
	@echo "Building $(PROJECT_NAME)..."
	$(PIO) run -e $(ENVIRONMENT)

.PHONY: clean
clean:
	@echo "Cleaning build files..."
	$(PIO) run -t clean -e $(ENVIRONMENT)

.PHONY: upload flash
upload flash:
	@echo "Uploading $(PROJECT_NAME) to ESP32-C3..."
	$(PIO) run -t upload -e $(ENVIRONMENT)

.PHONY: deploy
deploy: build upload

.PHONY: monitor serial
monitor serial:
	@echo "Starting serial monitor..."
	$(PIO) device monitor -e $(ENVIRONMENT)

.PHONY: run
run: deploy monitor

.PHONY: devices
devices:
	@echo "Connected devices:"
	$(PIO) device list

.PHONY: erase
erase:
	@echo "Erasing flash memory..."
	$(PIO) run -t erase -e $(ENVIRONMENT)

.PHONY: rebuild
rebuild: clean build

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  build/compile - Build the project"
	@echo "  clean         - Clean build files"
	@echo "  upload/flash  - Upload firmware to device"
	@echo "  deploy        - Build and upload"
	@echo "  monitor/serial- Start serial monitor"
	@echo "  run           - Build, upload, and monitor"
	@echo "  devices       - List connected devices"
	@echo "  erase         - Erase flash memory"
	@echo "  rebuild       - Clean and rebuild"
	@echo "  help          - Show this help"
