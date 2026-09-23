#include <HardwareSerial.h>

#define M0 4
#define M1 5
#define RX 18
#define TX 19
#define RE 14
#define DE 27

// dı 17 Ro 16

const byte temp[] = {0x01, 0x03, 0x00, 0x13, 0x00, 0x01, 0x75, 0xcf};
const byte mois[] = {0x01, 0x03, 0x00, 0x12, 0x00, 0x01, 0x24, 0x0F};
const byte econ[] = {0x01, 0x03, 0x00, 0x15, 0x00, 0x01, 0x95, 0xce};
const byte ph[] = {0x01, 0x03, 0x00, 0x06, 0x00, 0x01, 0x64, 0x0b};
const byte nitro[] = {0x01, 0x03, 0x00, 0x1E, 0x00, 0x01, 0xE4, 0x0C};
const byte phos[] = {0x01, 0x03, 0x00, 0x1f, 0x00, 0x01, 0xb5, 0xcc};
const byte pota[] = {0x01, 0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xc0};

byte values[11];
HardwareSerial loraSerial(1); // Use Serial1 for LoRa
HardwareSerial mod(2); // Use Serial2 for sensors

unsigned long kanalBekleme_sure = 0;
int kanalBekleme_bekleme = 3000;

struct SensorData {
  float moisture;
  float temperature;
  int ec;
  float ph;
  int nitrogen;
  int phosphorous;
  int potassium;
};

SensorData sensorData;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  loraSerial.begin(115200, SERIAL_8N1, TX, RX);
  while (!loraSerial) {}

  mod.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, 0);
  digitalWrite(M1, 0);
  pinMode(14, OUTPUT);

  Serial.println("LoRa soil sensor node started");
  delay(100);

  ayarDegis();
}

void loop() {
  if (millis() > kanalBekleme_sure + kanalBekleme_bekleme) {
    kanalBekleme_sure = millis();
    sensorData.moisture = readMoisture();
    sensorData.temperature = readTemperature();
    sensorData.ec = readEConduc();
    sensorData.ph = readPhydrogen();
    sensorData.nitrogen = readNitrogen();
    sensorData.phosphorous = readPhosphorous();
    sensorData.potassium = readPotassium();

    gonder(&sensorData);
    Serial.println("Mesaj Gönderildi");
  }

  while (loraSerial.available()) {
    // Handle incoming messages if needed
  }
}

void ayarDegis() {
  loraSerial.print("++");
  delay(100);
  loraSerial.print("+++");
  delay(100);
  loraSerial.println("AT+WMCFG=1");
  delay(100);
  loraSerial.println("AT+PIDCFG=1453");
  delay(100);
  loraSerial.println("AT+CNCFG=128");
  delay(100);
  loraSerial.println("AT+TFOCFG=3");
  delay(100);
  loraSerial.println("AT+TFICFG=0");
  delay(100);
  loraSerial.println("AT+RSCFG=100");
  delay(100);
  loraSerial.println("AT+IOCFG=1");
  delay(100);
  loraSerial.println("AT+PWCFG=1");
  delay(100);
  loraSerial.println("AT+TLCFG=3");
  delay(100);
  loraSerial.println("AT+RSTART");

  while (loraSerial.available()) {
    Serial.write(loraSerial.read());
  }

  Serial.println("Ayarlar degisti, Mevcut Ayarlar Getiriliyor..");
  delay(3000);
}

void gonder(SensorData *data) {
  // Sensör verilerini ekrana yazdırma
  Serial.print("Nem (Moisture): ");
  Serial.print(data->moisture);
  Serial.println(" %");

  Serial.print("Sıcaklık (Temperature): ");
  Serial.print(data->temperature);
  Serial.println(" °C");

  Serial.print("Elektriksel İletkenlik (EC): ");
  Serial.print(data->ec);
  Serial.println(" µS/cm");

  Serial.print("pH Değeri: ");
  Serial.print(data->ph);
  Serial.println("");

  Serial.print("Azot (Nitrogen): ");
  Serial.print(data->nitrogen);
  Serial.println(" mg/kg");

  Serial.print("Fosfor (Phosphorous): ");
  Serial.print(data->phosphorous);
  Serial.println(" mg/kg");

  Serial.print("Potasyum (Potassium): ");
  Serial.print(data->potassium);
  Serial.println(" mg/kg");

  // Verileri LoRa seri hattı üzerinden gönderme
  loraSerial.write((uint8_t *)data, sizeof(SensorData));
}

float readMoisture() {
  float moisture = sendAndReceive(mois) / 10.0;
  return moisture_to_percentage(moisture);
}

float readTemperature() {
  return sendAndReceive(temp) / 10.0;
}

int readEConduc() {
  return sendAndReceive(econ);
}

float readPhydrogen() {
  return sendAndReceive(ph) / 10.0;
}

int readNitrogen() {
  return sendAndReceive(nitro);
}

int readPhosphorous() {
  return sendAndReceive(phos);
}

int readPotassium() {
  return sendAndReceive(pota);
}

byte sendAndReceive(const byte *command) {
  while (mod.available()) mod.read();
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(1);

  for (uint8_t i = 0; i < 8; i++) mod.write(command[i]);
  mod.flush();
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);
  delay(100);

  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
  }
  return values[4];
}

float moisture_to_percentage(float moisture) {
  if (moisture < 0) {
    moisture = 0;
  } else if (moisture > 25.5) {
    moisture = 25.5;
  }
  return (moisture / 25.5) * 100;
}
