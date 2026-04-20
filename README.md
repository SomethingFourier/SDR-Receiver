# SDR-Receiver 📻
**Super Heterodyne Direct Conversion Hybrid I/Q SDR Receiver**

An open-source, hybrid-architecture Software Defined Radio (SDR) receiver built around the RP2350 microcontroller. This project blends superheterodyne and direct-conversion (Tayloe detector) techniques, featuring custom-designed filter banks, I2S audio sampling, and Power over Ethernet (PoE) capabilities.

## System Architecture

<img width="1247" height="371" alt="image" src="https://github.com/user-attachments/assets/fae45174-8ca4-442b-8cc7-b51d152f93e6" />

The receiver is divided into several highly specialized blocks (affectionately named during the whiteboard phase):

* **"His Excellence" (Input Stage):** Antenna input featuring a MABA-007159 transformer/balun.
* **"The William" (RF Front-End):** Low-Noise Amplifier (LNA) utilizing the PSA4-5043.
* **"Charles" & "Jaquavis" (Filter Banks):** Selectable filter pathways for signal conditioning before the mixer.
* **The Tayloe Detector:** An SN74CBT3253DBQR multiplexer acting as a quadrature detector/mixer, driven by an MS5351M clock generator.
* **"The Squared" (ADC):** A CJC5340 I2S Analog-to-Digital Converter handling the I/Q baseband signals.
* **"Marple" (The Brain):** The Raspberry Pi RP2350A handling DSP, I2S ingestion, and network communication.

## Hardware Specifications

* **Microcontroller:** Raspberry Pi RP2350A (with W25Q128JVS Flash)
* **Detector Type:** Quadrature Sampling Detector (Tayloe)
* **Clocking:** MS5351M (driven by a 24.576 MHz oscillator)
* **Network / Comms:** LAN8720A Ethernet PHY (10/100)
* **Power Supply:** USB-C (5V) or Power over Ethernet (PoE via Ag9905LP module delivering a clean 5.65V stepped down by LDOs)
* **Baseband Amplification:** OPA1612 op-amps for ultra-low noise I/Q amplification

---

## Filter Designs

Our RF pathways rely on custom-designed selectable filters for optimal signal-to-noise performance.

### Charles (Selectable Filters 28 - 148 MHz)
*Features 2 external filters (~1.5dB attenuation) and a 110-140 MHz ~6.0 attenuation filter.*
<img width="2113" height="1305" alt="Charles Filter Design" src="https://github.com/user-attachments/assets/c4828647-cf6e-4e16-ace0-0cf2b7938558" />

### Jaquavis (30 MHz LPF/HPF)
*Provides ~7dB of attenuation paired with a 10W attenuator loop.*
<img width="1793" height="1007" alt="Jaquavis Filter Design" src="https://github.com/user-attachments/assets/c8b24a2c-a5b2-4d73-b45e-37aa08584d01" />

---

## RP2350 GPIO Mapping

For firmware development, refer to the following PIO and hardware pin mapping based on our schematic routing:

| GPIO Pin | Function | Subsystem |
| :--- | :--- | :--- |
| **0 - 1** | `I2C SDA / SCL` | Control |
| **10 - 13**| `ETH TX_EN, TXD0, TXD1, CRS_DV`| Ethernet (LAN8720A) |
| **14 - 15**| `ETH RXD0, RXD1` | Ethernet (LAN8720A) |
| **18 - 19**| `I2C SDA / SCL` (Pico alternate) | Control / MS5351M |
| **26** | `I2S MCLK` | ADC ("The Squared") |
| **27** | `I2S WS` (Word Select) | ADC ("The Squared") |
| **28** | `I2S DATA` | ADC ("The Squared") |
| **56 - 60**| `QSPI` (SCLK, SDO, SD1-3, SS) | Flash Memory |

*(Note: Pin mapping is subject to change as the board layout is finalized. Refer to `pico.kicad_sch` for the raw schematic).*

---

## Getting Started

*(Instructions for cloning the repo, opening the KiCad project, and compiling the initial firmware will go here).*
