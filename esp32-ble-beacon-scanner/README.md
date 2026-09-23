# ESP32 BLE Beacon Tarayıcı ve Verici

Bu klasörde iki tamamlayıcı BLE örneği bulunur.

- `beacon_alici.ino`: Aktif BLE tarama yapar, advertised service UUID değerlerini prefix'e göre filtreler ve RSSI değerine bakarak en yakın beacon cihazını seçer.
- `becon_verici.ino`: ESP32 üzerinde BLE beacon yayını başlatır, servis UUID yayınlar ve düşük güç tüketimi için döngüde bekleme yaklaşımı kullanır.

Personel/konum takibi prototipleri ve yakınlık tabanlı gömülü uygulamalar için temel bir alıcı-verici örneğidir.
