# Ethernet ADC Audio Test

This project demonstrates capturing high-quality I2S audio from an ADC and streaming it over an Ethernet connection using an RP2350 microcontroller. It serves as a bring-up test for an SDR Receiver project, with the ultimate goal of streaming 192kHz/24-bit audio.

## Hardware Setup
- **Microcontroller**: RP2350
- **Ethernet PHY**: LAN8720 connected via PIO (RMII interface)
- **ADC**: CJC5340 connected via PIO (I2S interface)

## Features
- **I2S Audio Capture**: Utilizes the RP2350's PIO to seamlessly clock in 24-bit audio data from the ADC.
- **Ethernet Streaming**: Implements an RMII MAC in PIO (via `pico-rmii-ethernet`) and uses the `lwIP` network stack to transmit audio packets over UDP.
- **Dynamic Unicast Routing**: To prevent overwhelming Wi-Fi networks with high-bandwidth UDP broadcasts, the firmware boots up silently. It waits to receive a specific "HELLO" packet from a host computer, at which point it dynamically locks onto the host's IP and streams the audio via fast, reliable Unicast.
- **Web Interface**: Runs a lightweight HTTP server on port 80 for basic connectivity testing.

## Building and Flashing

1. Make sure you have the Pico SDK configured.
2. Run the build script:
   ```bash
   cd eth-audio-test
   ./build.sh
   ```
3. Connect your RP2350 via USB while holding the `BOOTSEL` button.
4. Copy the generated `.uf2` file to the mounted drive:
   ```bash
   cp build/rp2350_app.uf2 /path/to/RPI-RP2350/
   ```

## Receiving Audio

A comprehensive Python script (`rx_audio_udp.py`) is provided in the `tools/` directory to interface with the board. It can receive the custom UDP packets, calculate drop/gap statistics, save the stream to a standard `.wav` file, and play the audio live.

### Prerequisites
To use the live playback feature, install PyAudio:
```bash
pip install pyaudio
```

### Usage

1. **Find the Board's IP**: Connect the board to your router and open a serial terminal. Note the IP address printed when the network connects (e.g., `netif status changed 192.168.1.5`).
2. **Run the Receiver Script**:
   
   To listen to the audio live and save it to a `.wav` file:
   ```bash
   python3 tools/rx_audio_udp.py --target <ACTUAL_BOARD_IP> --port 4951 --play --wav-out recording.wav
   ```

#### Script Arguments:
- `--target <IP>`: **Required** for Unicast mode. Sends the initial "HELLO" handshake packet to the board's IP to initiate the audio stream.
- `--play`: Plays the incoming 24-bit audio live through your default speakers. (Uses a robust background-threaded queue and 16-bit downsampling to prevent Linux ALSA deadlocks).
- `--wav-out <file.wav>`: Automatically reconstructs the incoming packets into a playable WAV file.
- `--print-frames`: Prints the raw integer values of the first audio frame in each packet for debugging ADC noise/clipping.
- `--raw-out <file.bin>`: Dumps the raw 24-bit little-endian PCM payload to a file.
- `--stats-interval <seconds>`: Adjusts how often the packet reception statistics (drop rate, throughput Mb/s) are printed to the console.

## How it Works

1. **ADC to Memory**: The I2S PIO state machine clocks in 24-bit samples from the CJC5340. A DMA channel transfers these samples into a ring buffer in RAM.
2. **Memory to Network**: The main CPU loop polls the ring buffer. When enough samples are gathered to form a packet (e.g., 5ms of audio), it constructs a custom UDP payload with a sequence number and timestamp.
3. **Network to Python**: The Python script binds to port 4951. Once it sends the "HELLO" packet to the board, the board begins transmitting the UDP packets back. The script validates the sequence numbers to track any network packet drops (gaps), strips the headers, and processes the raw PCM data for playback or saving.
