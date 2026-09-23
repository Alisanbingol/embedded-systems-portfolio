# ESP32 Akıllı Tarım LoRa RS485 Node

RS485/Modbus üzerinden çok parametreli toprak sensörü okunur ve toplanan veriler UART bağlı LoRa modülüyle iletilir. Proje; sensör komut frame'leri, half-duplex RS485 yön kontrolü ve yapılandırılmış sensör veri paketi kullanımını gösterir.

## Donanım

- ESP32
- RS485 toprak sensörü
- UART LoRa modülü
- RE/DE kontrollü RS485 transceiver

## Ölçülen Veriler

- Toprak nemi
- Sıcaklık
- Elektriksel iletkenlik
- pH
- Azot, fosfor ve potasyum
