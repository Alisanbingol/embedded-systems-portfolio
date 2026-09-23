# ESP8266 VL53L0X Kişi Sayma

Bu projede iki adet VL53L0X time-of-flight sensör ile geçiş yönü algılanır ve içeri giren/çıkan kişi sayısı hesaplanır. Firmware, XSHUT pinleriyle sensörlere ayrı I2C adresleri atar, histerezis ve bekleme süreleri içeren bir durum makinesi kullanır, veriyi MQTT üzerinden JSON olarak yayınlar ve WS2812 LED'lerle bağlantı/sensör durumunu gösterir.

## Donanım

- ESP8266 / NodeMCU
- 2x VL53L0X ToF mesafe sensörü
- WS2812B LED veya 3 adreslenebilir LED
- Telemetri için MQTT broker

## Not

Wi-Fi ve MQTT bilgileri placeholder olarak bırakılmıştır. Kartı programlamadan önce kendi test bilgilerinizi girmeniz gerekir.
