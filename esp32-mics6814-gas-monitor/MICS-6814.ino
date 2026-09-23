#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <esp_adc_cal.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

/************ Wi-Fi Bilgileri ************/
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

/************ MQTT Bilgileri ************/
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "demo/gas-monitor";

/************ NTP ************/
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800);

/************ LED ************/
#define LED_PIN     5
#define NUM_LEDS    1
Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

/************ Sensör Pinleri ************/
const int PIN_CO  = 34;
const int PIN_NH3 = 35;
const int PIN_NO2 = 32;

/************ Donanım Sabitleri ************/
const float VREF = 3.3;
const int ADC_MAX = 4095;
const float RL = 10000.0;  // 10k direnç

/************ Kalibrasyon R0 Değerleri ************/
float R0_CO  = 8100.0;
float R0_NH3 = 3100.0;
float R0_NO2 = 13800.0;

/************ Dönüşüm Katsayıları ************/
const float m_CO  = 2.43,  c_CO  = -0.056;
const float m_NH3 = 1.37,  c_NH3 = -0.358;
const float m_NO2 = 4.05,  c_NO2 = -0.636;

/************ Ortalama Filtresi (SMA) ************/
const int SMA_WINDOW = 5;
float sma_co[SMA_WINDOW], sma_nh3[SMA_WINDOW], sma_no2[SMA_WINDOW];
int sma_idx = 0;

/************ Eşik Değerleri (ppm) ************/
float CO_THRESHOLD  = 20.0;
float NH3_THRESHOLD = 3.0;
float NO2_THRESHOLD = 7.0;

/************ Sistem Değişkenleri ************/
String groupId = "{YOUR-DEVICE-GROUP-ID}";
String deviceId = "mics6814_demo_01";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
const unsigned long MSG_INTERVAL = 60000; // 1 dakika
unsigned long lastBlink = 0;
bool ledState = false;

/************ Yardımcı Fonksiyonlar ************/
void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
}

float calcRs(float V) {
  if (V <= 0.0001) return 1e9;
  return RL * (VREF - V) / V;
}

float toPPM(float ratio, float m, float c) {
  if (ratio <= 0) return NAN;
  float log_ppm = m * log10(ratio) + c;
  float ppm = pow(10, log_ppm);
  return (ppm < 0 || ppm > 10000) ? NAN : ppm;
}

float updateSMA(float *buf, float val) {
  buf[sma_idx] = val;
  sma_idx = (sma_idx + 1) % SMA_WINDOW;
  float sum = 0; int count = 0;
  for (int i = 0; i < SMA_WINDOW; i++) {
    if (!isnan(buf[i])) { sum += buf[i]; count++; }
  }
  return (count > 0) ? sum / count : NAN;
}

String getDateTime() {
  timeClient.update();
  time_t t = timeClient.getEpochTime();
  struct tm *timeinfo = gmtime(&t);
  char buf[30];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  return String(buf);
}

/************ WiFi ve MQTT Fonksiyonları ************/
void setup_wifi() {
  setLEDColor(255, 0, 0);  // Kırmızı: bağlantı bekleniyor
  WiFi.begin(ssid, password);
  Serial.print("Wi-Fi bağlanıyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi bağlandı!");
  setLEDColor(0, 0, 255);  // Mavi: Wi-Fi bağlı
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("GasSensorClient")) {
      Serial.println("MQTT'ye bağlandı");
      setLEDColor(0, 255, 0);  // Yeşil: MQTT bağlı
      lastMsg = millis(); 
    } else {
      delay(5000);
    }
  }
}

/************ LED Renk Durumu ************/
void updateLED(float co, float nh3, float no2) {
  bool danger = false;
  if (co > CO_THRESHOLD || nh3 > NH3_THRESHOLD || no2 > NO2_THRESHOLD) danger = true;

  static uint8_t r = 0, g = 255, b = 0;
  if (danger) { r = 255; g = 0; b = 0; }  // Tehlike: kırmızı
  else { r = 0; g = 255; b = 0; }         // Normal: yeşil

  // WiFi bağlantısı yoksa yanıp sönme efekti
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastBlink > 500) {
      ledState = !ledState;
      lastBlink = millis();
      if (ledState) setLEDColor(r, g, b);
      else setLEDColor(0, 0, 0);
    }
  } else {
    setLEDColor(r, g, b);
  }
}

/************ MQTT Gönderim ************/
void sendSensorData(float co, float nh3, float no2) {
  String payload = "{";
  payload += "\"deviceGroupId\":\"" + groupId + "\",";
  payload += "\"deviceId\":\"" + deviceId + "\",";
  payload += "\"dateTime\":\"" + getDateTime() + "\",";
  payload += "\"data\":{";
  payload += "\"CO\":" + String(co, 2) + ",";
  payload += "\"NH3\":" + String(nh3, 2) + ",";
  payload += "\"NO2\":" + String(no2, 2);
  payload += "}}";

  if (client.publish(mqtt_topic, payload.c_str())) {
    Serial.println("MQTT Gönderildi: " + payload);
  } else {
    Serial.println("MQTT Gönderilemedi!");
  }
}

/************ Setup ************/
void setup() {
  Serial.begin(115200);
  led.begin();
  led.setBrightness(150);
  setLEDColor(255, 0, 0);

  setup_wifi();
  timeClient.begin();
  client.setServer(mqtt_server, mqtt_port);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_CO, ADC_11db);
  analogSetPinAttenuation(PIN_NH3, ADC_11db);
  analogSetPinAttenuation(PIN_NO2, ADC_11db);

  for (int i = 0; i < SMA_WINDOW; i++) {
    sma_co[i] = sma_nh3[i] = sma_no2[i] = NAN;
  }

  Serial.println("MiCS-6814 Ölçüm Başladı...");
}

/************ Loop ************/
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) reconnect();
    client.loop();
  }

  float v_co  = analogRead(PIN_CO)  * (VREF / ADC_MAX);
  float v_nh3 = analogRead(PIN_NH3) * (VREF / ADC_MAX);
  float v_no2 = analogRead(PIN_NO2) * (VREF / ADC_MAX);

  float Rs_co  = calcRs(v_co);
  float Rs_nh3 = calcRs(v_nh3);
  float Rs_no2 = calcRs(v_no2);

  float ratio_co  = Rs_co  / R0_CO;
  float ratio_nh3 = Rs_nh3 / R0_NH3;
  float ratio_no2 = Rs_no2 / R0_NO2;

  float ppm_co  = updateSMA(sma_co,  toPPM(ratio_co,  m_CO,  c_CO));
  float ppm_nh3 = updateSMA(sma_nh3, toPPM(ratio_nh3, m_NH3, c_NH3));
  float ppm_no2 = updateSMA(sma_no2, toPPM(ratio_no2, m_NO2, c_NO2));

  Serial.printf("CO: %.2f ppm | NH3: %.2f ppm | NO2: %.2f ppm\n", ppm_co, ppm_nh3, ppm_no2);

  updateLED(ppm_co, ppm_nh3, ppm_no2);

  if (WiFi.status() == WL_CONNECTED && millis() - lastMsg > MSG_INTERVAL) {
    sendSensorData(ppm_co, ppm_nh3, ppm_no2);
    lastMsg = millis();
  }

  delay(500);
}
