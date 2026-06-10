# RP2350 Ethernet & Audio Streamer

This project implements a 100Mbit Ethernet interface on a Raspberry Pi Pico RP2350 to run a simple web server and stream uncompressed high-fidelity audio over UDP. 

The core of the Ethernet implementation utilizes a modified version of the [pico-rmii-ethernet_nce](https://github.com/rscott2049/pico-rmii-ethernet_nce/tree/main) library, adapting the PIO (Programmable I/O) code to fit a custom hardware pinout.

## Features

* **Custom RMII Ethernet via PIO:** Emulates an Ethernet MAC using the RP2350's PIO state machines.
* **"Hello World" Web Server:** Hosts a basic web server to verify network connectivity and TCP stack operation.
* **Live UDP Audio Streaming:** Reads 24-bit audio from a CJC5340 ADC via I2S and broadcasts uncompressed audio packets over the network.
* **Dynamic Client Targeting:** Relies on a "heartbeat" packet from the receiving client (e.g., a laptop) every 3 seconds to dynamically assign the destination IP address for the UDP audio stream.

## Hardware & Clocking Setup

* **Microcontroller:** Raspberry Pi Pico RP2350.
* **System Clock:** Overclocked/configured to **200MHz**.
* **ADC:** CJC5340 (read via a dedicated I2S PIO program).
* **Clocking Architecture:** * The system starts with a standard 12MHz crystal.
    * This is scaled up via the RP2350's internal PLL to establish the 200MHz system clock.
    * For the RMII Ethernet interface, the microcontroller *supplies* the required 50MHz reference clock to the PHY. The Ethernet library divides the 200MHz system clock down to 50MHz and outputs it to the PHY chip.

<img width="1489" height="664" alt="Screenshot from 2026-06-10 07-39-48" src="https://github.com/user-attachments/assets/ff43f60b-91ce-490e-8c84-506371d2a0ab" />
<img width="655" height="655" alt="Screenshot from 2026-06-10 07-40-54" src="https://github.com/user-attachments/assets/a228ce95-a03b-48bb-977b-2d9e2db04875" />

## How It Works:

### 100BASE-TX Ethernet & The PHY Chip
Standard 100Mbit Ethernet (100BASE-TX) transmits data over twisted-pair copper cables. A microcontroller cannot connect directly to an ethernet cable because it only understands digital logic (0s and 1s at specific voltages). 
To bridge this gap, we use a **PHY (Physical Layer) chip**. The PHY translates the digital logic from our RP2350 MAC into the complex analog electrical signals (using MLT-3 encoding) required to travel over long copper wires. 

### What is RMII?
To communicate with the PHY chip, the RP2350 uses **RMII** (Reduced Media Independent Interface). 
Standard MII requires 16 pins to transmit and receive data. RMII cuts this pin count in half (saving valuable GPIO on the Pico) by doubling the clock speed. Both transmit and receive data paths use 2 data lines each, clocked at 50MHz (2 bits × 50MHz = 100 Megabits per second).

### Frame Checking and CRC
When data travels over wires at 100Mbps, electromagnetic interference or clock skew can easily flip a bit. To detect this, Ethernet uses a **CRC (Cyclic Redundancy Check)**. 
A CRC is a mathematical algorithm (specifically, polynomial division) run on the contents of the data packet before it is sent. The resulting "remainder" (a 32-bit number) is appended to the end of the packet. When the receiving side gets the packet, it runs the exact same math. If the calculated remainder doesn't match the one attached to the packet, the hardware knows the data was corrupted and drops the packet.

## Audio Pipeline & Streaming

The audio pipeline captures analog sound, digitizes it, and sends it to the network:
1. **I2S PIO:** The CJC5340 ADC sends digital audio to the RP2350 via the I2S protocol, handled by a dedicated PIO state machine.
2. **Packetization:** The incoming raw, uncompressed audio is buffered into discrete packets.
3. **UDP Streaming:** The packets are sent over UDP. 
4. **Heartbeat:** Because IP addresses and listeners can change, the receiver must ping the RP2350 with a heartbeat request at least once every 3 seconds. The RP2350 uses the source IP of this heartbeat as the destination for the audio stream.

### Sample Rates
The code is written to support both **48kHz** and **192kHz** (at 24-bit depth). However, for current demonstrations, we are using **48kHz**. Pushing uncompressed 192kHz/24-bit audio pushes the current hardware limits and significantly reduces reliability.

## ⚠️ Current Status & Known Issues

This project is highly experimental. The custom PCB is currently on its first revision (v1), and we are pushing the RP2350's PIO hard to act as an Ethernet MAC—something it wasn't strictly designed to do. Because of this, the system suffers from intermittent stability issues:

* **The "Lucky Boot" Syndrome:** Sometimes the board boots up, aligns its internal clocking/PIO state perfectly, and works flawlessly. 
* **CRC Errors:** Other times, the board will boot and immediately throw a wall of CRC errors in the serial monitor, failing to communicate.
* **Degradation over time:** Even on a "lucky boot," the system will occasionally desync after a few minutes of operation, resulting in a sudden flood of CRC errors and dropped packets.

These issues are likely caused by a combination of tight timing margins on the 200MHz PIO, clock skew on the 50MHz reference clock, or trace impedance mismatches on the v1 PCB. Resolving these physical and timing-layer bugs is the main focus for hardware v2.
