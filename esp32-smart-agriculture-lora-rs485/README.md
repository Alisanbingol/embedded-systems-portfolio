# ESP32 Smart Agriculture LoRa RS485 Node

Reads a multi-parameter soil sensor over RS485/Modbus and sends the collected data over a UART-connected LoRa module. The project demonstrates sensor command frames, half-duplex RS485 direction control and structured telemetry packaging.

## Hardware

- ESP32
- RS485 soil sensor
- LoRa UART module
- RS485 transceiver with RE/DE control pins

## Measurements

- Soil moisture
- Temperature
- Electrical conductivity
- pH
- Nitrogen, phosphorous and potassium
