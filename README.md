# Smartwatch Integration System (ESP32-S3 + BLE + Flutter App)

[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.5.2-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/index.html)
[![LVGL Version](https://img.shields.io/badge/LVGL-v8.3-green.svg)](https://lvgl.io/)
[![Flutter Framework](https://img.shields.io/badge/Flutter-v3.x-cyan.svg)](https://flutter.dev/)
[![BLE Connectivity](https://img.shields.io/badge/Connectivity-Bluetooth%20LE%20(NimBLE)-purple.svg)](#system-behavior--workflow)
[![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

A comprehensive wearable IoT solution utilizing **ESP32-S3** and **FreeRTOS** paired with a **Flutter companion mobile app** via **Bluetooth Low Energy (BLE)**. The smartwatch features real-time health monitoring (Heart Rate, SpO2), GPS route tracking, Google Maps turn-by-turn navigation mirroring, local alarm scheduling, device settings customization, and a smooth UI rendered with **LVGL v8.3**.

---

## Table of Contents

- [Key Features](#key-features)
- [System Architecture](#system-architecture)
- [Tech Stack & Hardware](#tech-stack--hardware)
- [Memory Management (PSRAM vs SRAM)](#memory-management-psram-vs-sram)
- [Hardware Connections](#hardware-connections)
- [System Behavior & Workflow](#system-behavior--workflow)
- [Project Structure](#project-structure)
- [System Protection & Power Management](#system-protection--power-management)
- [Getting Started](#getting-started)
  - [1. Firmware Setup (ESP-IDF)](#1-firmware-setup-esp-idf)
  - [2. Mobile App Setup (Flutter)](#2-mobile-app-setup-flutter)
- [Author Information](#author-information)

---

## Key Features

* **Real-time Telemetry Mirroring:** Syncs heart rate, SpO2, step count, activity modes, and battery status directly to the Flutter companion app over NimBLE.
* **Google Maps Navigation Sync:** Mirrors turn-by-turn navigation steps (direction icon, distance to next turn, street name) from smartphone to watch display in real time.
* **Dual-Mode Fitness Tracking:**
  * **Walking Mode:** Tracks and displays step counts, active time, and cumulative distance.
  * **Cycling Mode:** Tracks speed, elevation, and distance using real-time GNSS telemetry.
* **Autonomous GPS Path Logging:** Logs coordinates and speed to the local SPIFFS filesystem and syncs to mobile for Google Maps route reconstruction.
* **Health Dashboard:** Integrates MAX30102 for PPG photoplethysmography heart rate and blood oxygen (SpO2) estimation.
* **Responsive LVGL GUI:** High-priority GUI thread driving a 1.83" ST7789 TFT display with CST816S capacitive touch.
* **Ultra-Low Power Standby:** 25 uA deep-sleep current using PMOS power gating and 32.768 kHz external crystal RTC timekeeping.
* **Power-efficient OTA Update:** Wi-Fi is kept disabled during regular operation and activated only on-demand for firmware OTA upgrades.

---

## System Architecture

```text
+-------------------------------------------------------------------------+
|                  ESP32-S3 DUAL-CORE FREERTOS ARCHITECTURE               |
|                                                                         |
|   +---------------------------------+   +---------------------------+   |
|   |        PRO_CPU (CORE 0)         |   |      APP_CPU (CORE 1)     |   |
|   |  - NimBLE Bluetooth Stack Task  |   |  - LVGL GUI Engine Task   |   |
|   |  - Sensor Hub (MAX30102+BMI270) |   |  - Touch Event Task       |   |
|   |  - GPS NMEA Parser Task         |   |  - Navigation Render Task |   |
|   |  - Power & Watchdog Supervisor  |   |  - Haptic Feedback Task   |   |
|   +----------------+----------------+   +-------------+-------------+   |
|                    |                                  |                 |
|                    v                                  v                 |
|   +-----------------------------------------------------------------+   |
|   |                FreeRTOS Thread-Safe Message Queues              |   |
|   |                  (HeartRate, StepCount, NavMsg)                 |   |
|   +-----------------------------------------------------------------+   |
|                                    |                                    |
|                                    v                                    |
|   +-----------------------------------------------------------------+   |
|   |                      HYBRID MEMORY SUBSYSTEM                    |   |
|   |  - Internal SRAM (512 KB): Real-time TCB stacks, partial buffer |   |
|   |  - External PSRAM (2 MB): LVGL assets, fonts, GPS route tables  |   |
|   +-----------------------------------------------------------------+   |
+-------------------------------------------------------------------------+
                                     |
                          BLE GAP / GATT (NimBLE)
                                     |
                                     v
+-------------------------------------------------------------------------+
|                     FLUTTER COMPANION MOBILE APP                        |
|  - Real-time Dashboard (Health, Steps, Battery)                         |
|  - Google Maps Route Viewer (GPS history import)                        |
|  - Turn-by-Turn Navigation Forwarding Service                           |
|  - Device Settings & Alarm Synchronizer                                 |
+-------------------------------------------------------------------------+
```

---

## Tech Stack & Hardware

* **Microcontroller:** ESP32-S3-WROOM-1 (N8R2) - Dual-Core Xtensa LX7 @ 240 MHz, 8MB Quad Flash, 2MB Octal PSRAM.
* **Display:** 1.83" ST7789 TFT LCD, 240x284 resolution, SPI interface.
* **Touch Controller:** CST816S Capacitive Touch (I2C Port 0).
* **Sensors:**
  * Maxim MAX30102: Pulse Oximetry and Heart Rate Monitor (I2C Port 1).
  * Bosch BMI270: 6-Axis Inertial Measurement Unit (I2C Port 1).
  * Maxim MAX17048G: Precision Li+ Battery Fuel Gauge (I2C Port 1).
  * Quectel / GT-U8: GNSS Module (UART1 @ 9600 bps + 1PPS time sync).
* **Haptics:** Coin ERM Vibration Motor driven by LEDC PWM.
* **Power Management:** 3.7V 300mAh Li-Po battery, TP4056 charging IC, PMOS high-side load switches for subsystem isolation.
* **Frameworks & SDKs:** ESP-IDF v5.5.2, FreeRTOS SMP, NimBLE Stack, LVGL v8.3, Flutter SDK v3.x (Dart).

---

## Memory Management (PSRAM vs SRAM)

To prevent heap fragmentation and maximize execution determinism, memory allocation is partitioned between internal SRAM and external PSRAM:

| Memory Region | Size | Allocation Policy | Purpose |
| :--- | :--- | :--- | :--- |
| **Internal SRAM** | 512 KB | DMA-capable, zero-wait-state | RTOS task stacks, interrupt service buffers, partial display buffer (33 KB). |
| **External PSRAM** | 2 MB | Octal SPI @ 80 MHz | LVGL image assets, pre-rendered fonts (12pt to 48pt), GPS coordinate history buffer, OTA chunks. |

---

## Hardware Connections

### Pinout Table

| Peripheral | Signal | ESP32-S3 GPIO | Bus / Interface | Details |
| :--- | :--- | :--- | :--- | :--- |
| **ST7789 LCD** | SCLK | **GPIO 40** | SPI2 Host | Master Clock |
| | MOSI | **GPIO 39** | SPI2 Host | Data Out |
| | RST | **GPIO 38** | GPIO Output | Hardware Reset |
| | DC | **GPIO 42** | GPIO Output | Data / Command Selection |
| | CS | **GPIO 41** | SPI2 Host | Chip Select |
| | BLK | **GPIO 2** | LEDC PWM | Backlight Brightness Control |
| **CST816S Touch** | SDA | **GPIO 48** | I2C Port 0 | Capacitive Touch Data (4.7k pull-up) |
| | SCL | **GPIO 47** | I2C Port 0 | Capacitive Touch Clock (4.7k pull-up) |
| | INT | **GPIO 14** | GPIO Input | Falling-edge Touch Trigger |
| | RST | **GPIO 21** | GPIO Output | Touch Controller Reset |
| **Sensor Hub** | SDA | **GPIO 19** | I2C Port 1 | Dedicated Sensor Bus (MAX30102, BMI270, MAX17048) |
| | SCL | **GPIO 8** | I2C Port 1 | Dedicated Sensor Bus Clock |
| | IMU INT | **GPIO 20** | GPIO Input | BMI270 Motion Interrupt |
| | HR INT | **GPIO 13** | GPIO Input | MAX30102 FIFO Almost-Full Interrupt |
| | BAT ALRT | **GPIO 11** | GPIO Input | Fuel Gauge Low Voltage Alert |
| **Haptic Motor**| PWM | **GPIO 12** | LEDC PWM | Vibration Pattern Generator |
| **Power Button** | KEY | **GPIO 1** | RTC GPIO | Long-press Power, Short-press Wakeup |
| **GNSS Module** | TX | **GPIO 5** | UART1 | GPS Telemetry Receive (NMEA 0183) |
| | RX | **GPIO 6** | UART1 | GPS Configuration Transmit |
| | PPS | **GPIO 4** | GPIO Input | 1PPS Precision Second Synchronization |

---

## System Behavior & Workflow

```text
[Power On / Reset]
       |
       v
Initialize PMU & Clocks (240 MHz CPU, 32.768 kHz External RTC)
       |
       +---> Initialize I2C0 (Touch) & I2C1 (Sensors)
       +---> Initialize SPI2 (ST7789 LCD) & Load LVGL Drivers
       +---> Start FreeRTOS Tasks on Core 0 & Core 1
       |
[Runtime Operation]
  Core 0: Sensor Sampling (100 Hz HR) + GPS Parsing + BLE Advertising
  Core 1: LVGL GUI Handler (16 ms loop) + Touch Polling + Animation
       |
[Incoming BLE Event: Navigation Instruction]
  NimBLE Callback -> Post to nav_queue -> Core 1 wakes up -> LVGL updates Screen
       |
[Inactivity Timeout (30s)]
  Screen turns off -> PMOS cuts power to GPS/Sensors -> ESP32-S3 enters Deep Sleep (25 uA)
       |
[Wakeup via CST816S Touch / Button Press]
  RTC wakes Core -> Restores peripheral power -> Screen turns on within 80 ms
```

---

## Project Structure

```text
esp32-smartwatch/
├── firmware/                        # ESP-IDF Firmware Project
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   ├── main.c                  # System initialization and FreeRTOS task spawns
│   │   ├── ble/                    # NimBLE GATT server services and callbacks
│   │   │   ├── ble_service.c
│   │   │   └── ble_service.h
│   │   ├── drivers/                # Peripheral drivers
│   │   │   ├── st7789.c            # LCD SPI driver
│   │   │   ├── cst816s.c           # Touch I2C driver
│   │   │   ├── max30102.c          # Heart rate sensor driver
│   │   │   ├── bmi270.c            # IMU sensor driver
│   │   │   ├── max17048.c          # Battery fuel gauge driver
│   │   │   └── gps_uart.c          # GNSS UART parser
│   │   ├── gui/                    # LVGL display layout and screens
│   │   │   ├── ui_manager.c
│   │   │   ├── screen_watchface.c
│   │   │   ├── screen_health.c
│   │   │   └── screen_navigation.c
│   │   └── power/                  # Deep sleep and PMOS power gating logic
│   │       ├── pmu_manager.c
│   │       └── pmu_manager.h
│   └── spiffs_image/               # Flash filesystem for assets and fonts
├── mobile_app/                      # Flutter Companion Application
│   ├── pubspec.yaml
│   ├── lib/
│   │   ├── main.dart
│   │   ├── services/               # BLE communication and navigation mirroring
│   │   │   ├── ble_manager.dart
│   │   │   └── nav_mirror_service.dart
│   │   ├── screens/                # Mobile UI screens
│   │   │   ├── home_dashboard.dart
│   │   │   ├── map_tracking.dart
│   │   │   └── device_settings.dart
│   │   └── models/
│   │       └── telemetry_data.dart
├── docs/                            # Hardware schematics and architectural diagrams
└── README.md
```

---

## System Protection & Power Management

* **Independent Dual I2C Buses:** Capacitive touch is isolated on I2C Port 0. Heavy sensor transactions (MAX30102 at 100 Hz) run on I2C Port 1. If a sensor stalls the bus, touch response remains completely unaffected.
* **PMOS High-Side Power Gating:** In sleep mode, physical power rails to the GPS module and health sensors are disconnected via PMOS switches, eliminating standby leakage.
* **I2C Bus Lockup Recovery:** The firmware implements a 9-clock-pulse recovery routine on SCL before initialization to release any slave device holding SDA low.
* **Task Watchdog Timer (TWDT):** GUI and communication loops are supervised by TWDT. A stalled thread triggers a diagnostic dump and graceful warm-reset.

---

## Getting Started

### 1. Firmware Setup (ESP-IDF)

#### Prerequisites
* ESP-IDF v5.5.2 installed and configured.

#### Build and Flash
```bash
# Clone the repository
git clone https://github.com/HuynhTran112/esp32-smartwatch.git
cd esp32-smartwatch/firmware

# Set target to ESP32-S3
idf.py set-target esp32s3

# Build firmware
idf.py build

# Flash to device and open serial monitor
idf.py -p COM_PORT flash monitor
```

### 2. Mobile App Setup (Flutter)

#### Prerequisites
* Flutter SDK (v3.19 or later).
* Android Studio or VSCode with Flutter extensions.

#### Run Application
```bash
cd esp32-smartwatch/mobile_app

# Fetch dependencies
flutter pub get

# Connect Android/iOS phone with Developer Mode enabled
flutter run
```

---

## Author Information

* **Tran Huynh** - Embedded Systems & Firmware Engineer
* **Email:** huynhtran30112004@gmail.com
* **GitHub:** [HuynhTran112](https://github.com/HuynhTran112)
* **LinkedIn:** [Tran Huynh](https://linkedin.com)
