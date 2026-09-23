#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <time.h>

// FastLED Ayarları
#define LED_PIN    D7
#define NUM_LEDS   3         
CRGB leds[NUM_LEDS];
unsigned long lastLEDUpdate = 0;
const unsigned long LEDUpdateInterval = 10; // LED'leri 10ms'de bir güncelle

/************ Wi-Fi Bilgileri ************/
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

/************ MQTT Bilgileri ************/
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "YOUR_MQTT_USERNAME";
const char* mqtt_pass   = "YOUR_MQTT_PASSWORD";
const char* mqtt_topic  = "demo/person-counter";

/************ Durum Makinesi Tanımlamaları ************/
enum State {
  IDLE,
  SENSOR1_ACTIVE,
  SENSOR2_ACTIVE
};

State currentState = IDLE;

WiFiClient espClient;
PubSubClient client(espClient);
bool mqttConnected = false;

// Sensör Ayarları
Adafruit_VL53L0X sensor1 = Adafruit_VL53L0X();
Adafruit_VL53L0X sensor2 = Adafruit_VL53L0X();
const int sensor1ShutdownPin = D5;
const int sensor2ShutdownPin = D6;
const uint8_t sensor1NewAddress = 0x30;
const uint8_t sensor2NewAddress = 0x31;

// Sistem Ayarları - İYİLEŞTİRİLMİŞ DEĞERLER
int currentCount = 0;      // İçerideki mevcut kişi sayısı
int totalCount = 0;        // Bugün giren toplam kişi sayısı
bool sensor1Triggered = false;
bool sensor2Triggered = false;
unsigned long lastSensor1Time = 0;
unsigned long lastSensor2Time = 0;
unsigned long lastEventTime = 0;
const unsigned long detectionWindow = 2500;  // 1000ms'den 2500ms'ye çıkarıldı (yavaş geçişler için)
const unsigned long eventCooldown = 1200;     // 2000ms'den 1200ms'ye düşürüldü (hızlı geçişler için)

// Mesafe Eşiği - HİSTEREZİS EKLENMIŞ
const int detectionThreshold = 800;  // Algılama eşiği (daha stabil)
const int clearThreshold = 6000;      // Temizleme eşiği (histerezis için)

// Zaman sunucusu ayarları
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600; // GMT+3
const int   daylightOffset_sec = 0;

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nWi-Fi'ye bağlanılıyor...");
  
  int retryCount = 0;
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(++retryCount > 20) {
      Serial.println("\nBağlantı başarısız! Yeniden başlatılıyor...");
      ESP.restart();
    }
  }
  Serial.println("\nWi-Fi bağlantısı başarılı!");
  Serial.print("IP Adresi: ");
  Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "2025-01-01T00:00:00Z";
  }
  
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buffer);
}

void sendPersonCount() {
  const size_t capacity = JSON_OBJECT_SIZE(10);
  DynamicJsonDocument doc(capacity);

  // DeviceGroupId formatı düzeltildi (süslü parantezler eklendi)
  doc["deviceGroupId"] = "{YOUR-DEVICE-GROUP-ID}";
  doc["deviceId"] = "person_counter_01";
  doc["dateTime"] = getTimestamp();

  JsonObject data = doc.createNestedObject("data");
  
  // Bir önceki totalCount'un durumu
  String lastStateValue = ((totalCount - 1) % 100 == 0) ? "check" : "noCheck";
  // Şu anki totalCount'un durumu
  String currentStateValue = (totalCount % 100 == 0) ? "check" : "noCheck";
  
  data["lastState"] = lastStateValue;
  data["currentState"] = currentStateValue;
  data["currentCount"] = currentCount;
  data["totalCount"] = totalCount;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  if(client.connected()) {
    client.publish(mqtt_topic, jsonBuffer);
    Serial.print("Gonderilen veri: ");
    Serial.println(jsonBuffer);
  }
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("MQTT bağlantısı kuruluyor...");
    if (client.connect("ESP8266_PersonCounter", mqtt_user, mqtt_pass)) {
      Serial.println("Bağlandı!");
      mqttConnected = true;
      // Bağlantı başarılı olduğunda LED1'i yeşil yap
      leds[0] = CRGB::Green;
      FastLED.show();
    } else {
      Serial.print("Hata kodu: ");
      Serial.print(client.state());
      Serial.println(", 5 saniye sonra tekrar denenecek...");
      mqttConnected = false;
      delay(5000);
    }
  }
}

void updateLEDs() {
  if(!mqttConnected) {
    // MQTT bağlı değilse tüm LED'ler kırmızı
    fill_solid(leds, NUM_LEDS, CRGB::Red);
  } else {
    // MQTT bağlıysa:
    leds[0] = CRGB::Green; // 1. LED sürekli yeşil
    leds[1] = sensor1Triggered ? CRGB::Blue : CRGB::Black; // 2. LED sensör1 durumu
    leds[2] = sensor2Triggered ? CRGB::Blue : CRGB::Black; // 3. LED sensör2 durumu
  }
  FastLED.show();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP8266 Kişi Sayaç Sistemi v2.0 ===");

  // LED başlatma ve kırmızı yap
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100); // %40 parlaklık
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();

  // Wi-Fi ve MQTT bağlantıları
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Sensör başlatma
  Serial.println("Sensörler başlatılıyor...");
  Wire.begin(D2, D1);
  pinMode(sensor1ShutdownPin, OUTPUT);
  pinMode(sensor2ShutdownPin, OUTPUT);
  digitalWrite(sensor1ShutdownPin, LOW);
  digitalWrite(sensor2ShutdownPin, LOW);
  delay(10);
  
  digitalWrite(sensor1ShutdownPin, HIGH);
  delay(10);
  if (!sensor1.begin(sensor1NewAddress)) {
    Serial.println("1. Sensör başlatılamadı!");
    while(1);
  }
  Serial.println("Sensör 1 başarılı (0x30)");
  
  digitalWrite(sensor2ShutdownPin, HIGH);
  delay(10);
  if (!sensor2.begin(sensor2NewAddress)) {
    Serial.println("2. Sensör başlatılamadı!");
    while(1);
  }

}

void loop() {
  if (!client.connected()) {
    mqttConnected = false;
    reconnect_mqtt();
  }
  client.loop();

  // Sensör okumaları
  VL53L0X_RangingMeasurementData_t measure1, measure2;
  sensor1.rangingTest(&measure1, false);
  sensor2.rangingTest(&measure2, false);
  
  int distance1 = measure1.RangeMilliMeter;
  int distance2 = measure2.RangeMilliMeter;

  // HİSTEREZİS İLE SENSÖR TETİKLEME KONTROLÜ
  // Sensör 1
  if (distance1 < detectionThreshold) {
    if(!sensor1Triggered) {
      lastSensor1Time = millis();
      Serial.println(">>> Sensor1 TETIKLENDI!");
    }
    sensor1Triggered = true;
  } else if (distance1 > clearThreshold) {
    sensor1Triggered = false;
  }
  
  // Sensör 2
  if (distance2 < detectionThreshold) {
    if(!sensor2Triggered) {
      lastSensor2Time = millis();
      Serial.println(">>> Sensor2 TETIKLENDI!");
    }
    sensor2Triggered = true;
  } else if (distance2 > clearThreshold) {
    sensor2Triggered = false;
  }

  // LED güncelleme
  if(millis() - lastLEDUpdate > LEDUpdateInterval) {
    updateLEDs();
    lastLEDUpdate = millis();
  }

  switch(currentState) {
    
    case IDLE:
      // Boşta - Her iki sensör de bekleniyor
      if(sensor1Triggered && (millis() - lastEventTime) > eventCooldown) {
        currentState = SENSOR1_ACTIVE;
        Serial.println("DURUM: SENSOR1_ACTIVE (Giriş bekleniyor)");
      }
      else if(sensor2Triggered && (millis() - lastEventTime) > eventCooldown) {
        currentState = SENSOR2_ACTIVE;
        Serial.println("DURUM: SENSOR2_ACTIVE (Çıkış bekleniyor)");
      }
      break;
      
    case SENSOR1_ACTIVE:
      // Sensör 1 aktif - Sensör 2'nin tetiklenmesini bekliyoruz (GİRİŞ)
      if(sensor2Triggered && (millis() - lastSensor1Time) < detectionWindow) {
        // GİRİŞ ALGILANDI
        currentCount++;
        totalCount++;
        Serial.println("GİRİŞ ALGILANDI");
        Serial.printf("İçerideki: %d | Toplam Giriş: %d\n", currentCount, totalCount);
        
        // Giriş bildirimi gönder
        sendPersonCount();
        
        // Durum sıfırlama
        currentState = IDLE;
        lastEventTime = millis();
        lastSensor1Time = 0;
        lastSensor2Time = 0;
        Serial.println("DURUM: IDLE (Cooldown başladı)\n");
      }
      else if(millis() - lastSensor1Time > detectionWindow) {
        // Zaman aşımı - Yarım geçiş
        Serial.println("TIMEOUT: Sensor1 tek başına tetiklendi (yarım geçiş)");
        currentState = IDLE;
        lastSensor1Time = 0;
        Serial.println("DURUM: IDLE\n");
      }
      else if(!sensor1Triggered) {
        // Sensör 1 erkenden temizlendi
        Serial.println(" Sensor1 erkenden temizlendi");
        currentState = IDLE;
        lastSensor1Time = 0;
        Serial.println("DURUM: IDLE\n");
      }
      break;
      
    case SENSOR2_ACTIVE:
      // Sensör 2 aktif - Sensör 1'in tetiklenmesini bekliyoruz (ÇIKIŞ)
      if(sensor1Triggered && (millis() - lastSensor2Time) < detectionWindow) {
        // ÇIKIŞ ALGILANDI
        if(currentCount > 0) {
          currentCount--;
          Serial.println("ÇIKIŞ ALGILANDI");
          Serial.printf("İçerideki: %d | Toplam Giriş: %d\n", currentCount, totalCount);
          sendPersonCount();
        } else {
          Serial.println(" ÇIKIŞ algılandı ama sayaç zaten 0!");
        }
        
        // Durum sıfırlama
        currentState = IDLE;
        lastEventTime = millis();
        lastSensor1Time = 0;
        lastSensor2Time = 0;
        Serial.println("DURUM: IDLE (Cooldown başladı)\n");
      }
      else if(millis() - lastSensor2Time > detectionWindow) {
        // Zaman aşımı - Yarım geçiş
        Serial.println("TIMEOUT: Sensor2 tek başına tetiklendi (yarım geçiş)");
        currentState = IDLE;
        lastSensor2Time = 0;
        Serial.println("DURUM: IDLE\n");
      }
      else if(!sensor2Triggered) {
        // Sensör 2 erkenden temizlendi
        Serial.println(" Sensor2 erkenden temizlendi");
        currentState = IDLE;
        lastSensor2Time = 0;
        Serial.println("DURUM: IDLE\n");
      }
      break;
  }
  
  // DEBUG BİLGİSİ (her 500ms'de bir)
  static unsigned long lastDebug = 0;
  if(millis() - lastDebug > 500) {
    Serial.printf("S1: %4dmm | S2: %4dmm | Durum: %d | İçeride: %d | Toplam: %d\n", 
                  distance1, distance2, currentState, currentCount, totalCount);
    lastDebug = millis();
  }
  
  delay(50);
}
