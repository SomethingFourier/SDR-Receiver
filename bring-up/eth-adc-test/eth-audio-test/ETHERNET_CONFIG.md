# Ethernet Pin Configuration

## Custom Pin Mapping

This project uses a custom GPIO pin configuration for the Ethernet RMII interface. The pins are configured to match a specific board layout and are **permanently tracked in Git** to prevent losses.

### GPIO Pin Assignment

| Function | GPIO Pin | Purpose |
|----------|----------|---------|
| TX Enable | 10 | Ethernet TX Enable signal |
| TXD0 | 11 | Transmit Data 0 |
| TXD1 | 12 | Transmit Data 1 |
| CRS/DV | 13 | Carrier Sense / Data Valid (Collision Detection) |
| RXD0 | 14 | Receive Data 0 |
| RXD1 | 15 | Receive Data 1 |
| MDIO | 16 | Management Data Input/Output (LAN8720) |
| MDC | 17 | Management Data Clock (LAN8720) |
| Reset | 18 | LAN8720 Device Reset |
| 50MHz Ref Clock | 23 | 50MHz Reference Clock Output for LAN8720 |

## Configuration Location

The pin definitions are stored in two places to ensure they persist with git commits:

1. **CMakeLists.txt** - Compile-time definitions passed to the build system
   - Contains `ETHERNET_PIN_DEFS` variable with all GPIO mappings
   - These are applied as compiler flags to ensure the external library uses the correct pins

2. **src/main.c** - Runtime 50MHz clock configuration
   - `setup_50mhz_clock()` function generates the 50MHz clock on GPIO23
   - Called early in `main()` before Ethernet initialization

## How to Modify Pin Configuration

To change the pin configuration:

1. Open `CMakeLists.txt`
2. Update the `ETHERNET_PIN_DEFS` variable with new GPIO numbers
3. If changing the 50MHz clock pin, also update:
   - The pin parameter in `setup_50mhz_clock()` call in `src/main.c`
   - The `PICO_RMII_ETHERNET_RETCLK_PIN` definition in `CMakeLists.txt`

4. Rebuild the project:
   ```bash
   sh build.sh
   ```

## Why This Approach

This configuration method ensures:
- **Git Tracking**: Pin definitions are stored in version control and won't be lost
- **No External Library Modification**: Changes don't modify the external submodule, preventing conflicts
- **Easy Maintenance**: Configuration is clearly visible in CMakeLists.txt and main.c
- **Flexibility**: Pins can be easily changed for different board versions

## 50MHz Clock Setup

The RP2350's system clock is set to 200MHz by `arch_pico_init()`. The `setup_50mhz_clock()` function divides this by 4 to produce a 50MHz output clock on GPIO23, which the LAN8720 PHY requires as a reference clock.

## Related Files

- `/external/pico-rmii-ethernet/` - Ethernet library (git submodule, unchanged)
- `CMakeLists.txt` - Project build configuration
- `src/main.c` - Application code and runtime initialization
- `build.sh` - Build script
