#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

int scanTime = 5; // Tarama süresi (saniye)
BLEScan* pBLEScan;

// Beacon UUID'lerinde sabit kısım (prefix)
const String beaconPrefix = "00000000-0000-0000-0000";



void setup() {
  Serial.begin(115200);
  Serial.println("BLE Tarayıcı başlatılıyor...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();  // Tarayıcı oluşturuluyor
  pBLEScan->setActiveScan(true);     // Aktif tarama: daha hızlı sonuçlar (daha fazla güç tüketir)
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);           // setInterval değerine eşit veya daha düşük olmalı
}

void loop() {
  // Tarama başlatılıyor ve sonuçlar alınıyor
  BLEScanResults* foundDevices = pBLEScan->start(scanTime, false);
  Serial.printf("Toplam Cihaz Sayısı (filtrelenmeden): %d\n", foundDevices->getCount());

  // En iyi (en yüksek RSSI) beacon’u tespit etmek için değişkenler
  int bestRSSI = -999;  // RSSI değerleri genelde -100 civarında olduğundan, başlangıç değeri düşük tutuluyor.
  String bestBeaconUUID = "";

  // Bulunan cihazlar arasında döngü
  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);

    // Eğer cihaz servis UUID bilgisi içeriyorsa kontrol edelim:
    if (device.haveServiceUUID()) {
      String advUUID = device.getServiceUUID().toString();
      
      // Sabit prefix ile başlayan beacon cihazlarını filtrele:
      if (advUUID.startsWith(beaconPrefix)) {
        int rssi = device.getRSSI();
        Serial.printf("Beacon: %s, RSSI: %d\n", advUUID.c_str(), rssi);

        // En yüksek RSSI (en güçlü sinyal) kontrolü:
        if (rssi > bestRSSI) {
          bestRSSI = rssi;
          bestBeaconUUID = advUUID;
        }
      }
    }
  }

  // En yakın beacon (en yüksek RSSI) tespit edilmişse raporla:
  if (bestBeaconUUID.length() > 0) {
    Serial.printf("En Yakın Beacon: %s, RSSI: %d\n", bestBeaconUUID.c_str(), bestRSSI);
  } else {
    Serial.println("Hiçbir beacon bulunamadı.");
  }

  Serial.println("Tarama tamamlandı!\n");
  pBLEScan->clearResults();   // Tarama sonuçlarını temizle
  delay(2000);
}