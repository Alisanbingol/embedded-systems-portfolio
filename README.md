# Gömülü Sistemler Portfolyosu

Bu repo, gömülü yazılım pozisyonları için seçilmiş proje örneklerinden oluşur. Kodlar ESP8266, ESP32, nRF52840 ve STM32 üzerinde sensör okuma, haberleşme, gerçek zamanlı olay işleme, MQTT telemetri ve donanım-yazılım entegrasyonu konularını göstermek için düzenlenmiştir.

## Projeler

| Proje | Platform | Öne Çıkanlar |
| --- | --- | --- |
| `esp8266-vl53l0x-person-counter` | ESP8266 | Çift VL53L0X ToF sensör, I2C adres atama, giriş/çıkış yön algılama, durum makinesi, MQTT/JSON yayın, WS2812 durum LED'leri |
| `esp32-smart-agriculture-lora-rs485` | ESP32 | RS485/Modbus toprak sensörü, LoRa UART telemetri, nem/sıcaklık/EC/pH/NPK veri paketi |
| `esp32-ble-beacon-scanner` | ESP32 | BLE aktif tarama, UUID filtreleme, RSSI ile en yakın beacon seçimi, ek olarak BLE beacon verici örneği |
| `esp32-mics6814-gas-monitor` | ESP32 | MiCS-6814 ile CO/NH3/NO2 ölçümü, ADC okuma, hareketli ortalama filtre, MQTT telemetri, RGB LED durum göstergesi |
| `mobile-robot-pid-controller` | Arduino uyumlu MCU | Quadrature encoder interrupt okuma, diferansiyel sürüş kinematiği, PID hız kontrolü, JSON seri komut/geri bildirim |
| `nrf52840-vl53l0x-ble` | nRF52840 | VL53L0X mesafe ölçümü, BLE servis/characteristic notification, düşük güç bekleme akışı |
| `stm32-uart-adc` | STM32F4 | STM32CubeMX proje özeti, ADC/UART çevre birimleri, HAL tabanlı C firmware modülleri |

## Güvenlik Notu

Gerçek Wi-Fi bilgileri, MQTT broker adresleri, cihaz kimlikleri ve üretim ortamına ait topic değerleri kaldırılmıştır. Çalıştırmadan önce `YOUR_WIFI_SSID`, `YOUR_WIFI_PASSWORD`, `YOUR_MQTT_BROKER` gibi placeholder alanlarını kendi test ortamınıza göre doldurun.

## Gösterilen Yetkinlikler

- C/C++ ile gömülü yazılım geliştirme
- I2C, UART, SPI, RS485/Modbus ve BLE kullanımı
- Interrupt, zamanlama, ADC/PWM/GPIO ve durum makinesi tasarımı
- Sensör verisi filtreleme, eşik kontrolü ve olay algılama
- MQTT/JSON telemetri
- STM32CubeMX/HAL iş akışı
- Donanım-yazılım entegrasyonu ve seri port üzerinden hata ayıklama
