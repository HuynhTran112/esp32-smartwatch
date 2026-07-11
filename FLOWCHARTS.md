# Sub-system Flowcharts Explanation

This document explains the algorithms and logic shown in the sub-flowchart diagrams (designed via Draw.io) for the smartwatch sensors, fitness tracking, and GPS routing.

---

## 1. Motion & Health Hardware Drivers

### 📌 1.1. IMU BMI270 Core Driver
![IMU Flowchart](./docs/images/1.8.2.IMU.drawio%20(6).png)

* **Description:** Initializes the 6-axis BMI270 sensor (accelerometer & gyroscope) via the shared I2C Port 1. Configures internal interrupt registers for motion detection (Any-motion) and enables the sensor's hardware-based step detection to offload calculations from the ESP32-S3 CPU.

### 📌 1.2. Pulse Oximeter MAX30102 Driver
![MAX30102 Flowchart](./docs/images/1.8.3.MAX30102.drawio%20(6).png)

* **Description:** Initializes the MAX30102 heart rate & pulse oximetry sensor over I2C Port 1. Configures LED drive currents (Red and IR channels) and sampling rates. Pins the `HR_INT` interrupt pin (GPIO 13) to wake the ESP32-S3 to read the 32-sample internal FIFO queue whenever new measurements are ready.

### 📌 1.3. Heart Rate Calculation Algorithm
![Heart Rate Flowchart](./docs/images/1.8.3.2.Hearate.drawio%20(11).png)

* **Description:** Applies a software bandpass filter to the raw optical AC signal to suppress low-frequency motion artifacts and high-frequency noise. Detects signal peaks (Peak Detection) or analyzes frequencies to determine the pulse cycle and calculate the Beats-Per-Minute (BPM).

### 📌 1.4. SpO2 (Blood Oxygen Saturation) Algorithm
![SpO2 Flowchart](./docs/images/1.8.3.1.SpO2.drawio%20(5).png)

* **Description:** Analyzes both Red and Infrared (IR) reflective channels. Extracts the AC amplitude and DC offset of both channels to compute the "ratio of ratios" $R$:
  $$R = \frac{AC_{Red} / DC_{Red}}{AC_{IR} / DC_{IR}}$$
  The $R$ value is matched against an empirical calibration table to output the blood oxygen saturation percentage ($SpO_2\%$).

---

## 2. GPS Navigation & Fitness Tracking

### 📌 2.1. GPS NMEA Data Processing
![GPS Flowchart](./docs/images/1.9.GPS_handle%20(2).drawio%20(7).png)

* **Description:** Receives raw NMEA 0183 sentences via UART 1. Parses `$GPRMC` and `$GPGGA` sentences to extract GPS coordinates (Latitude/Longitude), velocity, and UTC time. Writes coordinates directly to the local SPIFFS partition in a path log file for companion app map synchronization.

### 📌 2.2. Step Counter Algorithm
![Step Counter Flowchart](./docs/images/1.8.2.1.Step_counter.drawio%20(14).png)

* **Description:** Triggers when the IMU step interrupt fires. Reads step registers from the BMI270, filters data against time thresholds, updates the UI step telemetry, and packs data for BLE transmission.

### 📌 2.3. Distance Calculation Logic
![Distance Flowchart](./docs/images/1.8.2.2.Distance_count.drawio%20(7).png)

* **Description:** Computes cumulative active distance ($S$) based on step count and user stride profile configuration, or calculates physical distance between successive GPS coordinates during outdoor walking/cycling sessions to log precise active metrics.
