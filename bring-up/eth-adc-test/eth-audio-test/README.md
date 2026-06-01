# eth-audio-test

Combined RP2350 bring-up test that keeps the LAN8720 web interface and adds UDP broadcast audio from the I2S ADC.

## What this does

- Keeps the existing HTTP server on port 80 (`<h1>Hello World!</h1>`).
- Captures stereo I2S samples from the ADC via PIO + DMA ring buffer.
- Broadcasts audio on UDP port `4951` using a tiny custom header.
- Uses 48 kHz, 24-bit packed stereo, 5 ms packets.

## UDP Packet Format

All fields are little-endian.

- `u32 magic`: `0x30445541` (`AUD0`)
- `u8 version`: `1`
- `u8 channels`: `2`
- `u8 bytes_per_sample`: `3`
- `u8 flags`: bit0 set for signed little-endian PCM
- `u32 sample_rate_hz`: `48000`
- `u32 sequence`: packet sequence counter
- `u32 timestamp_us`: `time_us_32()` at send time
- `u16 frame_count`: `240`
- `u16 reserved`: `0`
- payload: packed stereo 24-bit PCM (`L0[3],R0[3],L1[3],R1[3], ...`)

Packet size:

- Header: 24 bytes
- Payload: `240 * 2 * 3 = 1440` bytes
- Total UDP payload: 1464 bytes (fits under typical non-fragmented Ethernet UDP limit)

## Build and flash

From project root:

```bash
cd eth-audio-test
./build.sh
./flash.sh
```

## Verify HTTP server

After DHCP lease, open:

- `http://<board-ip>/`

You should see `Hello World!`.

## Receive audio on host

Run receiver:

```bash
python3 tools/rx_audio_udp.py --port 4951
```

Optional: save raw packed audio payload stream:

```bash
python3 tools/rx_audio_udp.py --port 4951 --raw-out capture_s24le.raw
```

## Bridge mode for future Quisk path

Forward stripped payload to localhost UDP port 4952:

```bash
python3 tools/udp_to_quisk_bridge.py --in-port 4951 --out-port 4952
```

This keeps firmware protocol simple and lets host-side software adapt format for Quisk integration.

## Runtime serial stats

Firmware prints periodic counters like:

- sent packets
- dropped packets
- send errors
- DMA ring overruns
- lost frames

Use these stats to quantify headroom before moving to 192 kHz mode.
