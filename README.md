# Embedded Systems Portfolio

This repository contains selected embedded software projects prepared as a portfolio for embedded firmware roles.

The code demonstrates microcontroller-based development with ESP8266, ESP32, nRF52840 and STM32, including sensor integration, serial communication, BLE, LoRa/RS485, PID control, interrupt-driven encoder reading, MQTT publishing and low-power BLE operation.

## Projects

| Project | Platform | Highlights |
| --- | --- | --- |
| `esp8266-vl53l0x-person-counter` | ESP8266 | Dual VL53L0X ToF sensors, I2C address assignment, state-machine based entry/exit counting, MQTT JSON publishing, WS2812 status LEDs |
| `esp32-smart-agriculture-lora-rs485` | ESP32 | RS485/Modbus soil sensor reading, LoRa UART telemetry, structured sensor data packet |
| `esp32-ble-beacon-scanner` | ESP32 | BLE active scan, service UUID filtering, nearest beacon selection using RSSI |
| `mobile-robot-pid-controller` | Arduino-compatible MCU | Quadrature encoder interrupts, differential drive kinematics, PID speed control, JSON serial command/feedback |
| `nrf52840-vl53l0x-ble` | nRF52840 | VL53L0X distance sensing, BLE service/characteristic notifications, low-power wait-for-event flow |
| `stm32-uart-adc` | STM32F4 | STM32CubeMX project excerpt, ADC/UART peripheral setup, HAL-based C firmware modules |

## Security Note

Real Wi-Fi credentials, MQTT broker addresses, device identifiers and production topics were removed. Replace placeholder values such as `YOUR_WIFI_SSID` and `YOUR_MQTT_BROKER` with local test values before running.

## Skills Shown

- C/C++ embedded firmware development
- I2C, UART, SPI, RS485/Modbus and BLE usage
- Interrupt handling, timers, ADC/PWM/GPIO and state machines
- Sensor data filtering and event detection
- MQTT/JSON telemetry
- STM32CubeMX/HAL workflow
- Hardware-software integration and debugging-oriented serial output
