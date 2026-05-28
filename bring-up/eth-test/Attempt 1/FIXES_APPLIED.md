# RP2350 LAN8720 Link Status Fixes

## Issue
- Ethernet link light ON
- But `link=0` in status command
- PHY not establishing link properly

## Root Cause
**The RMII clock was starting AFTER the reset sequence, but the LAN8720A datasheet requires the clock to be running DURING reset.** This prevented proper PHY initialization.

## Fixes Applied

### 1. **CRITICAL: Fixed Reset Sequence** (main.c)
```
Before:
  lan8720_init_pins(&phy);
  rmii_refclk_start_dual(...);   // Start clock
  lan8720_reset(&phy);            // Then reset

After:
  lan8720_init_pins(&phy);
  rmii_refclk_start_dual(...);   // Start clock FIRST
  sleep_ms(1);                    // Stabilize
  lan8720_reset(&phy);            // Then reset with clock running
```

**Why:** LAN8720A REQUIRES the RMII clock to be running while reset is asserted and for 25ms after.

### 2. **Improved Autonegotiation Detection** (main.c)
- Added explicit wait for autonegotiation complete (BMSR bit 5)
- Now waiting for BOTH link status (bit 2) AND autoneg complete (bit 5)
- Increased timeout to 5 seconds with better diagnostics
- Double-read BMSR per IEEE 802.3 standard

### 3. **Updated Link Status Function** (lan8720.c - `lan8720_link_up`)
```c
// Now requires both conditions:
bool link_up = (status & LAN8720_BMSR_LINK_STATUS) != 0;
bool autoneg_complete = (status & (1u << 5)) != 0;
return link_up && autoneg_complete;
```

### 4. **Enhanced Diagnostics** (main.c)
- Boot sequence now shows detailed progress
- Status callback displays:
  - Speed (10M vs 100M)
  - Duplex mode (Half vs Full)  
  - Autonegotiation status
  - PHY IDs

## Testing Steps

1. **Build the project**
   ```bash
   cmake -B build_rp2350 -DPICO_BOARD=pico2 -S .
   make -j -C build_rp2350
   ```

2. **Flash the firmware**
   ```bash
   cp build_rp2350/rp2350_lan8720_eth.uf2 /path/to/pico/
   ```

3. **Monitor Serial Output**
   - You should see detailed boot sequence showing:
     ```
     Starting RMII clock on GPIO XX...
     Resetting LAN8720A with clock running...
     LAN8720 detected at MDIO address X
     Starting PHY bring-up...
     Soft resetting PHY...
     Reset complete after XX ms
     Setting ANAR to 0x01E1...
     Writing BMCR with autoneg enable and restart...
     Waiting for autonegotiation to complete...
       [0ms] BMSR=0x7809 link=0 autoneg_done=0
       [500ms] BMSR=0x782D link=1 autoneg_done=1
     ✓ Autonegotiation complete and link up at 500ms
     Final PHY state:
       PHYID1=0x0007 PHYID2=0xC0F1
       BMCR=0x1200 BMSR=0x782D
     ```

4. **Check Link Status**
   - The green LED should now stay ON when link is established
   - Status command should show `link=1`

## If Link Still Doesn't Work

1. **Verify RMII Clock Signal**
   - Check GPIO 23 (BOARD_ETH_REFCLK_PIN) with oscilloscope
   - Should see 50 MHz clock (or see PIO-generated clock)

2. **Verify MDIO Communication**
   - Check GPIO 16 (MDIO) and GPIO 17 (MDC)
   - Should see proper MDIO handshake during probe/reads

3. **Check Reset Pin**
   - GPIO 18 should go LOW during reset, then HIGH
   - Verify with oscilloscope if available

4. **Compare with Reference Implementation**
   - Reference: https://github.com/rscott2049/pico-rmii-ethernet_nce
   - They use interrupt-driven MDIO (more robust)
   - They have full lwIP integration

## Key Differences from Reference Implementation

| Aspect | Your Code | Reference |
|--------|-----------|-----------|
| RMII Clock | Basic PIO | Generated in TX PIO |
| Link Detection | Simple bit-banging | Interrupt-driven state machine |
| PHY Integration | Minimal | Full lwIP stack |
| Speed Support | 100Mbps | 10/100/Auto-negotiation |

## Next Steps for Higher Speed (>10Mbps)

The reference implementation achieves 94.9 Mbps. To reach higher speeds:

1. Use DMA for packet transfers (not just MDIO)
2. Implement full lwIP at higher clock speeds (300 MHz for RP2350)
3. Use interrupt-driven MDIO instead of bit-banging
4. Consider PIO-generated RMII clock (avoids clock domain crossing issues)

See the reference repository for a complete production-ready implementation.
