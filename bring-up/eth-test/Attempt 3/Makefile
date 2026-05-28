.PHONY: help build clean rebuild flash setup check

help:
	@echo "RP2350 Pico SDK Project Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  make build     - Build the project"
	@echo "  make clean     - Clean the build directory"
	@echo "  make rebuild   - Clean and build"
	@echo "  make setup     - Run setup script"
	@echo "  make check     - Check build requirements"
	@echo "  make flash     - Flash to RP2350 (requires BOOTSEL mode)"
	@echo "  make help      - Show this help message"

build:
	@./build.sh

clean:
	@./build.sh clean

rebuild:
	@./build.sh rebuild

setup:
	@./setup.sh

check:
	@if [ -z "$$PICO_SDK_PATH" ]; then \
		echo "ERROR: PICO_SDK_PATH not set. Run 'make setup' first."; \
		exit 1; \
	fi
	@echo "Checking environment..."
	@command -v cmake > /dev/null || (echo "ERROR: cmake not found"; exit 1)
	@command -v arm-none-eabi-gcc > /dev/null || (echo "ERROR: ARM toolchain not found"; exit 1)
	@echo "✓ Environment OK"
	@echo "✓ PICO_SDK_PATH: $$PICO_SDK_PATH"

flash: build
	@echo "Flashing RP2350..."
	@echo ""
	@echo "Important: The RP2350 must be in BOOTSEL mode:"
	@echo "1. Hold BOOTSEL button"
	@echo "2. Press and release RESET button"
	@echo "3. Release BOOTSEL button"
	@echo ""
	@UF2_FILE=$$(find build -name "*.uf2" -type f | head -n1); \
	if [ -z "$$UF2_FILE" ]; then \
		echo "ERROR: No .uf2 file found"; \
		exit 1; \
	fi; \
	MOUNT_POINT=$$(mount | grep 'RPI-RP2350' | awk '{print $$3}'); \
	if [ -z "$$MOUNT_POINT" ]; then \
		echo "ERROR: RP2350 not found. Check that it's in BOOTSEL mode."; \
		exit 1; \
	fi; \
	echo "✓ Found RP2350 at: $$MOUNT_POINT"; \
	cp "$$UF2_FILE" "$$MOUNT_POINT/"; \
	echo "✓ Flashed successfully!"
