# ESP32 MiCS-6814 Gaz İzleme

MiCS-6814 gaz sensörü ile CO, NH3 ve NO2 ölçümü yapan ESP32 tabanlı örnektir. ADC okuma, sensör direnç oranından ppm hesaplama, hareketli ortalama filtreleme, eşik kontrolü, RGB LED durum göstergesi ve MQTT/JSON telemetri akışını içerir.

## Donanım

- ESP32
- MiCS-6814 gaz sensörü
- RGB / NeoPixel LED
- MQTT broker

## Öne Çıkanlar

- 12-bit ADC okuma ve pin attenuation ayarı
- CO/NH3/NO2 için ayrı kalibrasyon katsayıları
- Basit hareketli ortalama filtresi
- Eşik aşımında LED durum bildirimi
- JSON payload ile MQTT yayın

Wi-Fi ve MQTT bilgileri placeholder olarak bırakılmıştır.
