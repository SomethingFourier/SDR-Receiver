# SDR-Receiver 📻
**Super Heterodyne Direct Conversion Hybrid I/Q SDR Receiver**

An open-source, hybrid-architecture Software Defined Radio (SDR) receiver built around the RP2350 microcontroller. This project blends superheterodyne and direct-conversion (Tayloe detector) techniques. It's been designed to listen to the 2-meter band, but offers a wide range of tunable frequencies. It features both USB C (USB 1.1) and Ethernet data communication, as well as Power over Ethernet (PoE) capabilities.

## System Architecture


<img alt="System Block Diagram" src="SDR Block Diagram.drawio_light.png" />

The receiver is divided into distinct HF and VHF signal paths. Each path utilizes a dedicated antenna equipped with an isolation transformer to prevent ground loops.

* HF Path: The isolated signal is routed directly to a multiplexer.
* VHF Path: The signal passes through a low-noise amplifier (LNA) followed by a band-pass filter. This filter prevents image frequencies from interfering with the desired intermediate frequency (IF). The signal is then mixed down by a double-balanced diode ring mixer and sent to the multiplexer.

A multiplexer selects between the HF signal and the down-converted VHF signal, feeding the active path into a shared low-pass filter.

After the low-pass filter, the signal makes its way to the Tayloe detector. The Tayloe detector mixes the signal down to be centered at 0Hz with both an in-phase and a quadrature output. These I and Q outputs then are fed into the analog-to-digital converter. As this is a first revision board, as a backup and insurance measure, we've also connected the I and Q outputs to a 3.5mm jack for use with an external sound card.

The whole SDR is built around the rp2350 microprocessor. The rp2350 will talk to SDR software either over a USB C or Ethernet connection. It will receive commands to change the tuning frequency, and relay that information to the clock generator for this SDR, the si5351. It will also interface with a small I2C OLED display and a couple buttons for a local user interface. It will receive the SDR I and Q data from the ADC via I2S, and send that data over either the USB C or Ethernet connection to the computer running the SDR software.


## Hardware Specifications

* **Microcontroller:** Raspberry Pi RP2350A (with W25Q128JVS Flash)
* **Detector Type:** Quadrature Sampling Detector (Tayloe)
* **Clocking:** MS5351M (driven by a 24.576 MHz oscillator)
* **Network / Comms:** LAN8720A Ethernet PHY (10/100)
* **Power Supply:** USB-C (5V) or Power over Ethernet (PoE via Ag9905LP module delivering a clean 5.65V stepped down by LDOs)
* **Baseband Amplification:** OPA1612 op-amps for ultra-low noise I/Q amplification

---

## Filter Designs

### Charles (Selectable Filters 110-140 MHz)
*This filter simulation graph shows a passband attenuation much greater than what will actually be observed*
<img width="2113" height="1305" alt="Charles Filter Design" src="https://github.com/user-attachments/assets/c4828647-cf6e-4e16-ace0-0cf2b7938558" />

### Jaquavis (30 MHz LPF/HPF)
*This filter simulation graph shows a passband attenuation much greater than what will actually be observed*
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
