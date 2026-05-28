# Quisk Ethernet Control Scheme

## Overview
This document defines the control architecture for interfacing Quisk with the custom RP2350-based Software Defined Radio (SDR) over an Ethernet connection. 

The goal is to allow Quisk to control the SDR's tuning frequency, and eventually stream I/Q audio data over the same Ethernet link.

## 1. Initial Phase: Frequency Control (Proof of Concept)

In the initial stage, we will establish bidirectional control communication to ensure the RP2350 can receive commands from Quisk and send responses back.

### Control Flow
1. **Command (Quisk -> RP2350):** Quisk sends a desired tuning frequency to the RP2350 SDR over Ethernet.
2. **Processing (RP2350):** The RP2350 receives the requested frequency and determines the closest actual frequency it can tune its hardware to (based on its local oscillators/PLL capabilities).
3. **Response (RP2350 -> Quisk):** The RP2350 sends a response packet back to Quisk over Ethernet, indicating the *actual* frequency it has tuned to.
4. **Verification (RP2350):** The RP2350 will print the requested and actual frequencies to the serial monitor to visually verify the control scheme is working.

### Network Protocol
- **Protocol:** UDP (User Datagram Protocol) is recommended for both control and future I/Q streaming due to its low overhead and latency.
- **Port:** A dedicated UDP port (e.g., `50000`) will be used for control messages.
- **Data Format:** Simple ASCII strings (e.g., `TUNE 7074000`) or a lightweight binary structure (C-struct) containing the command type and frequency value in Hz.

### Quisk Integration (`hardware.py`)
To make this work in Quisk, we need a custom configuration:
1. Create a custom `quisk_conf.py` and `hardware.py` module for Quisk.
2. In the `hardware.py` class, override the tuning methods (like `ChangeFrequency`).
3. In this method, open a UDP socket, send the requested frequency to the RP2350's IP address, wait for the response packet, and then return the actual tuned frequency to Quisk so the UI updates correctly.

## 2. Future Phase: I/Q Data Streaming

Once the control scheme is verified via serial output, the next step is to send the actual radio signals (I and Q data) over the Ethernet connection.

### Implementation Concept
- **Protocol:** UDP stream.
- **Data Format:** A continuous stream of UDP packets containing arrays of interleaved I and Q samples (e.g., 16-bit integers, depending on the ADC).
- **Quisk Integration:** Quisk will need to be configured to read its sample data from this network socket instead of a standard local sound card. We can either emulate an existing network radio protocol (like Hermes) that Quisk already understands, or write a custom socket receiver in Python.

## Open Questions & Next Steps
- **Data Format:** What data format should we use for the UDP control packets? (ASCII text is easier to debug initially, while binary is more efficient).
- **Network Config:** Do you have a preferred fixed IP address / Subnet / Gateway for the RP2350, or should it use DHCP?
- **Quisk Setup:** Where should the custom Quisk Python files be stored? Inside this repository for tracking, or directly in your `~/.quisk` directory?
