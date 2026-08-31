

---

# ESP32 Wearable Bluetooth Glove Mouse

A wearable, wireless mouse built around an **ESP32** microcontroller, designed to replace traditional peripherals and offer intuitive cursor and interface control via hand movements.

<img width="600" height="400" alt="image" src="https://github.com/user-attachments/assets/1162d377-52fd-44aa-8412-28023413205d" />

**What you need:**
ESP-32U WROOM (supporting BT, WiFi, and operating on 5V), MPU6050 or MPU9250 gyroscope and accelerometer module, monostable push button, ON/OFF toggle switch, 16x2 alphanumeric LCD display with an I2C converter module, 2x B50K linear potentiometers, 2x LEDs, 2x 18650 3.7V Li-ion cells wired in parallel, Li-Po charging module with an integrated step-up converter, and 2x custom-made aluminum touch elements.
  
## Key Features

* **Cross-Platform Bluetooth HID:** Connects wirelessly to PCs, smartphones, smart TVs, and projectors as a standard Bluetooth HID device.
* **Gyroscope-Based Tracking:** Real-time cursor movement driven by the **MPU6050** sensor (using gyroscope axes for X and Y control) with integrated software offsets and deadzone filtering to prevent drift.
* **Dual-Axis Sensitivity Control:** Features two hardware potentiometers (salvaged from an old audio mixer) allowing users to independently adjust X and Y axis sensitivity on the fly.
* **Capacitive Touch Inputs:** Replaced traditional, high-force mechanical buttons with aluminum foil pads connected to the ESP32’s built-in `TouchPin` interface. Human body capacitance alters signal discharge time, ensuring smooth and effortless click detection.
* **Pause / Stop Mode:** Includes a dedicated physical toggle switch to pause tracking, allowing the user to comfortably rest their hand without triggering unwanted cursor actions.
* **LCD Status Display:** A 16x2 I2C LCD provides real-time feedback on connection status and current sensitivity percentages. Visual LED indicators signal active clicks.

## Hardware Architecture & Power Management

* **Microcontroller:** ESP32 (utilizing dual I2C buses: one for the MPU6050 and a separate bus for the LCD to avoid signal routing issues and extra soldering).
* **Power Supply:** Powered by two parallel 18650 Li-ion cells (3.7V), providing high current capacity and extended operating time compared to unstable 9V alternatives.
* **Powerbank Module:** Integrated step-up power module regulates and boosts voltage to 5V, featuring USB-A output for the system and a micro-USB port for safe on-board cell charging.

## Engineering Challenges & Solutions


* **Current Stability:** Initial tests with a 9V battery and single 18650 cells led to voltage drops and ESP32 reboots during peak power consumption. Shifting to a dual parallel 18650 configuration with a dedicated step-up module completely solved power stability issues.
* **Yaw & Magnetometer Workaround:** Due to supply constraints resulting in a magnetometer-free MPU6050 variant, the design relies purely on gyroscope rotational data. While this introduces minor cursor drift during full hand rotations, software scaling and deadzones make it an intuitive alternative control method.

  # Simplified connection diagram
  <img width="600" height="400" alt="image" src="https://github.com/user-attachments/assets/49fa2b1a-c32d-4576-8ac4-4ed175c0c30b" />

  # Original connection diagram
  <img width="600" height="700" alt="image" src="https://github.com/user-attachments/assets/ae44207d-879f-4e1e-b4a3-6d680c7b14c7" />
---
