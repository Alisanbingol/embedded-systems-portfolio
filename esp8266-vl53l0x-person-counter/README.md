# ESP8266 VL53L0X Person Counter

Dual VL53L0X time-of-flight sensors are used to detect movement direction and count people entering or leaving an area. The firmware assigns separate I2C addresses through XSHUT pins, applies a simple state machine with hysteresis/cooldown logic, publishes JSON telemetry over MQTT and uses WS2812 LEDs for connection/sensor status.

## Hardware

- ESP8266 / NodeMCU
- 2x VL53L0X ToF distance sensors
- WS2812B LED strip or 3 addressable LEDs
- MQTT broker for telemetry

## Notes

Network and MQTT credentials are placeholders. Configure them locally before flashing.
