# ESP32-C3-TFT-CUBE

A wireless distance measurement system using ESP32-C3 microcontrollers with TFT displays like the [Spotpear MiniTV](https://spotpear.com/shop/ESP32-C3-desktop-trinket-Mini-TV-Portable-Pendant-LVGL-1.44inch-LCD-ST7735.html). The system consists of a sender and receiver pair that communicate using ESP-NOW protocol to measure and display the distance between them.

![Transmitter](doc/transmitter.gif) ![Receiver](doc/receiver.gif)

## Features

- Wireless distance measurement using multiple protocols:
  - RSSI (Received Signal Strength Indicator) via ESP-NOW
  - Fine Timing Measurement (FTM) via WiFi
- Interactive TFT display interface
- Real-time distance visualization
- Configurable operation modes:
  - ESP-NOW Sender/Receiver
  - FTM Client/Responder
- LED status indicators
- Calibration interface for RSSI measurements
- ESP-NOW and WiFi FTM communication protocols

## Hardware Requirements

- [ESP32-C3 Mini TV](https://spotpear.com/shop/ESP32-C3-desktop-trinket-Mini-TV-Portable-Pendant-LVGL-1.44inch-LCD-ST7735.html)
- ESP32-C3 development board with TFT display
- Onboard LED

## Software Components

### main.c
The main application file that handles:
- LVGL UI initialization and management
- Screen transitions and UI elements
- Button handling and user interaction
- Mode selection (EspNowSender/EspNowReceiver/FtmClient/FtmResponder)
- Display updates and animations
- Calibration interface

### FtmClient.c
Implements the FTM client functionality:
- WiFi FTM initialization and configuration
- FTM session management
- Distance measurement using FTM protocol
- AP scanning and connection

### FtmResponder.c
Implements the FTM responder functionality:
- WiFi FTM responder setup
- FTM session handling
- Response to FTM measurement requests

### EspNowSender.c
Implements the ESP-NOW sender functionality:
- ESP-NOW initialization and configuration
- Periodic data broadcasting
- Send status monitoring
- LED feedback for successful transmissions

### EspNowReceiver.c
Implements the ESP-NOW receiver functionality:
- ESP-NOW initialization and configuration
- Signal reception and processing
- Distance calculation using RSSI values
- Real-time distance updates

### common.c
Provides common functionality:
- Timestamp management for callbacks
- Shared utilities between all modes
- Calibration data management

### gpio.c
Handles GPIO operations:
- Button input processing
- LED control
- Hardware interface management

## Setup Instructions for [ESP32-C3 Mini TV](https://spotpear.com/shop/ESP32-C3-desktop-trinket-Mini-TV-Portable-Pendant-LVGL-1.44inch-LCD-ST7735.html)

1. Configure the onboard LED:
```bash
espefuse.py -p COM* burn_efuse VDD_SPI_AS_GPIO 1
```

2. Build and flash the project using ESP-IDF:
```bash
idf.py build
idf.py -p COM* flash
```

3. Configure the sender's MAC address in `sender.c` to match your receiver's MAC address.

## Distance Measurement

The system supports two methods of distance measurement:

### RSSI-based Measurement (ESP-NOW)
Uses a log-distance path loss model to estimate distance based on RSSI values. The calculation can be calibrated by adjusting:
- `RSSI_AT_1_METER`: Expected RSSI value at 1 meter distance
- `PATH_LOSS_EXPONENT`: Signal attenuation factor (typically 2.0-4.0)

### FTM-based Measurement (WiFi)
Uses the WiFi Fine Timing Measurement protocol for more accurate distance measurements:
- Supports burst periods and frame counts configuration
- Provides time-of-flight measurements
- Requires FTM-capable WiFi hardware

## License

MIT License

Copyright (c) 2024

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
