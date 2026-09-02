# Device Identity Scan Engine (Multi-Protocol SDR Wireless Sniffer & Tracker)

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Hardware](https://img.shields.io/badge/Hardware-HackRF%20One%20%7C%20USRP%20B210-green.svg)](https://greatscottgadgets.com/hackrf/one/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance C++17 Software-Defined Radio (SDR) Wireless Sniffer, Protocol Demodulator, and Device Identity Tracking Engine.

Supports simultaneous live capture, demodulation, packet dissection, and device tracking across:
- **Wi-Fi 802.11a/b/g/n/ac/ax** (2.4 GHz & 5 GHz)
- **Bluetooth Low Energy (BLE 4.x / 5.x)** (Wideband 20 MSPS Multi-Channel DDC Demodulation)
- **Zigbee 802.15.4** (All 16 channels via 4 wideband DDC blocks)
- **LoRa / LoRaWAN** (EU868, IN865, US915, 433 MHz ISM)

---

## 🌟 Key Features

### 1. Multi-Protocol Hardware Frontends
- **Great Scott Gadgets HackRF One**: Full live 20 MSPS streaming via direct `libhackrf` dynamic driver with hardware gain staging (LNA: 0–40 dB, VGA: 0–62 dB, +14 dB RF Amp).
- **Ettus Research USRP B210 / B200**: UHD dynamic driver with coherent dual-channel RX streaming and master clock synchronization.
- **Synthetic RF & Protocol Simulation Engine**: Built-in physical DSP waveform generator for offline development, testing, and CI verification.

### 2. Verified Protocol Hopping Engine
Automated multi-channel tuning and dwell scheduling:
- **Wi-Fi 2.4G Primary**: Channels 1, 6, 11 (20 MSPS, 450 ms dwell).
- **Wi-Fi 2.4G All**: Channels 1–14 (20 MSPS, 400 ms dwell).
- **Wi-Fi 5 GHz UNII**: Channels 36, 40, 44, 48, 149, 157, 165 (20 MSPS, 400 ms dwell).
- **Bluetooth BLE Wideband**: Channels 37, 38, 39 + Extended Data channels (20 MSPS, 600 ms dwell).
- **Zigbee 802.15.4**: All 16 channels in 4 wideband DDC blocks (20 MSPS, 600 ms dwell).
- **LoRa EU868 / IN865**: Wideband 8-channel coverage (4 MSPS, 1500 ms dwell).
- **LoRa US915**: All 64 channels across 8 sub-bands (4 MSPS, 1200 ms dwell).
- **Full Spectrum Sweep**: Sequential multi-protocol sweeping.

### 3. Preamble & Unencrypted Header Dissection
- **Wi-Fi 802.11**:
  - OFDM/DSSS Preamble + PLCP Header, Frame Control, Sequence Control.
  - SSID (UTF-8, hidden cloaked detection), BSSID, Transmitter MAC.
  - Security suite: WPA3-SAE Dragonfly, WPA2-PSK (AES-CCMP), WPA2-Enterprise (802.1X), PMF (802.11w).
  - Beacon Interval (TU & ms), DTIM Period & Count, 64-bit microsecond timestamp.
  - WPS 2.0 metadata, AP Setup Lockout vulnerability status, and vendor IEs (Microsoft, Broadcom, Qualcomm, Cisco, Apple).
- **Bluetooth BLE**:
  - GFSK 1 Mbps demodulation with dynamic Carrier Frequency Offset (CFO) DC block removal.
  - 8-bit alternating preamble validation + 32-bit Access Address (`0x8E89BED6`) matched filtering.
  - LFSR dewhitening & strict CRC-24 verification (`0x00065B` polynomial).
  - Complete Local Name (`0x09`) & Shortened Local Name (`0x08`) harvesting via `SCAN_RSP` dual-packet state merging.
  - Bluetooth SIG Company ID resolution (Apple, Samsung, Google, Microsoft, Sony, Bose, Xiaomi, Anker, Garmin, Espressif, Nordic, etc.).
  - Deep Apple Model Dissector (AirPods Pro/Max/3rd Gen, AirTags, FindMy Network, Continuity).
- **Zigbee 802.15.4**:
  - O-QPSK 250 kbps DSSS chip despreading (32-chip pseudo-random sequences).
  - SHR Synchronization Header (32-bit zero preamble + SFD `0xA7`).
  - PHR Frame Length, MHR MAC Header, PAN IDs, Coordinator/Router roles, Association Permit flag.
  - NWK Layer: Network Frame Control, Multicast, Security flag, Sequence numbers.
- **LoRa / LoRaWAN**:
  - Chirp Spread Spectrum (CSS) preamble detection (8 up-chirps + Sync Word `0x34`).
  - MHDR MAC Header: MType (Join-Request, Join-Accept, Unconfirmed Data Up, Confirmed Data Up).
  - DevEUI, AppEUI/JoinEUI, DevAddr, DevNonce, FCnt, FPort, MIC integrity verification.

### 4. Real-Time Hardware-Accelerated GUI
- **Dear ImGui (Dark Modern Glassmorphism Theme)** with docked panels:
  - **Live Unique Device Registry Table**: Deduplicated device entries with sorted columns, protocol badges, stable names, resolved manufacturer branding, sparkline RSSI history, and packet counters.
  - **Kismet-Style Wi-Fi Deep Parameter Inspector**: Detailed identity, RF band, security & AKM suite, beacon timers, WPS vulnerability, and PHY rate matrices.
  - **Hierarchical Dissection Tree & Raw Hex/ASCII Inspector**: Wireshark-like collapsible bitfield tree and synchronized hex view.
  - **Real-Time RF Spectrum & Waterfall Display**: 2048-point Blackman-Harris windowed FFT with 120 FPS hardware-accelerated OpenGL rendering.
  - **Live Capture Stream & IDS Anomaly Alert Feed**: Real-time packet scrolling and intrusion alert engine (WPS lockout, deauthentication attacks, open associations).

---

## 🛠️ Build & Installation

### Prerequisites (Ubuntu / Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config \
    libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev \
    libhackrf-dev libuhd-dev
```

### Clone & Build

```bash
git clone https://github.com/subeep/device-identity-scan.git
cd device-identity-scan
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run Unit Test Suite

```bash
./build/unit_tests
```

---

## 🚀 Usage

```bash
# Launch with physical HackRF One SDR:
./build/device_identity_scan --hackrf

# Launch with physical Ettus USRP B210 SDR:
./build/device_identity_scan --usrp

# Launch in Synthetic Simulation mode (no SDR hardware required):
./build/device_identity_scan --simulated

# Launch with custom initial frequency and sample rate:
./build/device_identity_scan --hackrf --freq 2437.0 --rate 20.0
```

---

## 📁 Repository Structure

```
device-identity-scan/
├── CMakeLists.txt              # CMake build configuration
├── README.md                   # Project documentation
├── include/                    # Header files
│   ├── common/                 # Core types, logger, OUI database
│   ├── core/                   # Device tracker, IDS engine, Wi-Fi live scanner
│   ├── dsp/                    # FFT analyzer, BLE, Zigbee, LoRa, Wi-Fi demodulators
│   ├── protocols/              # Wi-Fi, BLE, Zigbee, LoRa dissectors & packet definitions
│   ├── sdr/                    # SDR device interfaces (HackRF, USRP, Simulation, Manager)
│   └── ui/                     # ImGui user interface views & dark theme
├── src/                        # Implementation files (.cpp)
├── test/                       # Comprehensive unit test suite & hardware verification
└── vendor/                     # Dear ImGui & ImPlot libraries
```

---

## 📄 License

MIT License. Copyright (c) 2026.
