#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <esp_bt.h>

#define NUM_LEDS 1
#define DATA_PIN 5

CRGB leds[NUM_LEDS];

#define SERVICE_UUID "00000000-0000-0000-0000-000000000001"
#define DEVICE_NAME  "Portfolio Beacon 01"

void setup() {
  Serial.begin(115200);

  // Dusuk parlaklik, pil ile calisan beacon prototipleri icin daha uygundur.
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(30);
  leds[0] = CRGB::Red;
  FastLED.show();

  Serial.println("BLE Beacon baslatiliyor...");

  BLEDevice::init(DEVICE_NAME);

  // TX gucunu yuksek tutarak tarayicinin beacon'i yakalama olasiligini artir.
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  // Dusuk aralik daha sik yayin demektir; yakinlik takibi icin yakalama sansini artirir.
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE Beacon aktif - TX: +9dBm");
  Serial.print("UUID: ");
  Serial.println(SERVICE_UUID);

  leds[0] = CRGB::Green;
  FastLED.show();

  // Sadece BLE kullanildigi icin Classic Bluetooth bellegi serbest birakilir.
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
}

void loop() {
  // BLE advertising arka planda devam eder.
  delay(2000);
}
