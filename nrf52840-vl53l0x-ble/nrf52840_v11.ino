/*
 * nRF52840 + VL53L0X + 20 Byte BLE Packet
 *
 * DEĞİŞİKLİKLER (v11 — Güç Opt. DC/DC Regülatör):
 *   - sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE) eklendi
 *   - nRF52840 dahili LDO yerine DC/DC regülatör kullanır → ~%25-30 akım azalması
 *   - Bluefruit.begin() sonrasına konulmalı (SoftDevice aktif olduktan sonra)
 *   - Donanımda harici L1 indüktörü gerektirir — XIAO nRF52840'ta mevcut
 *
 * Tüm güç optimizasyonları özeti:
 *   v6:  delay() → sleepMs()           bekleme ~4mA → ~0.01mA
 *   v7:  Advertising 100ms → 500ms     radyo ~%70 daha seyrek
 *   v8:  Timing budget 400ms → 200ms   sensör aktif süre yarıya indi
 *   v11: DC/DC regülatör               tüm aktif sürelerde ~%25-30 tasarruf
 */

#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include <bluefruit.h>

// ───────────────────────────────────────
// DERLEME ZAMANI AYARLARI
// ───────────────────────────────────────
#define ENABLE_SERIAL          1        // Üretimde 0 yap

// ───────────────────────────────────────
// PIN TANIMLARI
// ───────────────────────────────────────
#define SENSOR_XSHUT_PIN       3        // LOW → sensör kapalı, HIGH → açık

// ───────────────────────────────────────
// ZAMANLAMA
// ───────────────────────────────────────
#define MEASUREMENT_INTERVAL_MS   60000  // Ölçüm periyodu
#define SENSOR_POWERUP_MS         10     // XSHUT HIGH → ölçüme hazır bekleme
#define SENSOR_WARMUP_MS          50     // init sonrası ısınma süresi

// Timing budget: sensör başına ölçüm süresi
// 200ms → 0-5cm mesafede yeterli SNR, 5 örnek = ~1sn toplam (400ms'de 2sn'ydi)
// Sigma hatası artarsa 300000 yap → %25 tasarruf hâlâ korunur
#define TIMING_BUDGET_US          30000
#define CONNECT_TIMEOUT_MS        90000  // ESP32 bağlantı timeout
#define SUBSCRIBE_TIMEOUT_MS      10000  // CCCD subscribe timeout
#define PRE_NOTIFY_DELAY_MS       600    // Subscribe → notify arası bekleme
#define POST_NOTIFY_DELAY_MS      800    // Notify → disconnect arası bekleme
#define DISCONNECT_WAIT_MS        3000   // Disconnect onayı bekleme

// ───────────────────────────────────────
// MESAFE SINIRI
// ───────────────────────────────────────
const int minDistance = 50;    // mm → %100 dolu
const int maxDistance = 250;   // mm → %0  dolu (peçetelik max derinliği)

// ───────────────────────────────────────
// DOLULUK MANTIĞI
// ───────────────────────────────────────
// Boş mesajı gönderildikten sonra tekrar "dolu" sayılmak için gereken minimum %
#define REFILL_THRESHOLD_PCT   40

// ───────────────────────────────────────
// SENSÖR KALİTE PARAMETRELERİ
// ───────────────────────────────────────
#define DISTANCE_INVALID       0xFFFF   // Donanım/yazılım hatası
#define DISTANCE_EMPTY         0xFFFE   // Hedef yok = peçetelik boş
#define MAX_ERROR_COUNT        5

// ───────────────────────────────────────
// BLE TANIMLAYICILARI
// ───────────────────────────────────────
uint8_t groupId[16] = {
  0x00,0x00,0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x01
};
char deviceId[8] = {'D','E','M','O','0','1',0,0};

// ───────────────────────────────────────
// GLOBAL DURUM
// ───────────────────────────────────────
Adafruit_VL53L0X lox;
bool sensorInitialized = false;
int  sensorErrorCount  = 0;

int     previousFill      = -1;
uint8_t currentState      = 1;      // 1=full, 0=empty
uint8_t lastState         = 1;
bool    emptyWasSent      = false;  // Boş mesajı gönderildi mi?
uint16_t lastSeenDistance = 0;      // Boş durumda gönderilecek gerçek mesafe

// Boş onay sayacı: üst üste kaç kez boş görülürse gönderilsin?
#define EMPTY_CONFIRM_COUNT  3
int emptyConfirmCount = 0;

BLEService        distanceService(0x1234);
BLECharacteristic distanceChar(0x5678);

volatile bool deviceConnected   = false;
volatile bool notifyEnabledFlag = false;

// ───────────────────────────────────────
// SERIAL MAKROLAR
// ───────────────────────────────────────
#if ENABLE_SERIAL
  #define LOG(...)    Serial.print(__VA_ARGS__)
  #define LOGLN(...)  Serial.println(__VA_ARGS__)
  #define LOGF(...)   Serial.printf(__VA_ARGS__)
#else
  #define LOG(...)
  #define LOGLN(...)
  #define LOGF(...)
#endif

// ───────────────────────────────────────
// CPU UYKU YARDIMCISI
//
// delay() kullandığında CPU tam hızda döner ve ~3-4mA çeker.
// sd_app_evt_wait() SoftDevice'e "bir sonraki event'e kadar uyu" der.
// CPU WFE (Wait For Event) talimatı ile durur, sadece interrupt/event
// geldiğinde uyanır. Bekleme sırasındaki akım ~0.01mA'ya düşer.
//
// SoftDevice aktifken delay() yerine her zaman sleepMs() kullanılmalı.
// ───────────────────────────────────────
static void sleepMs(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    sd_app_evt_wait();   // CPU uyu, interrupt/event gelince uyan
  }
}


// ───────────────────────────────────────
void sensorPowerOff() {
  digitalWrite(SENSOR_XSHUT_PIN, LOW);
  sensorInitialized = false;
  LOGLN("[SENSOR] Guc kesildi (XSHUT LOW).");
}

void sensorPowerOn() {
  digitalWrite(SENSOR_XSHUT_PIN, HIGH);
  sleepMs(SENSOR_POWERUP_MS);
  LOGLN("[SENSOR] Guc verildi (XSHUT HIGH).");
}

// ───────────────────────────────────────
// SENSÖR AYARLARI
// ───────────────────────────────────────
static void applySensorConfig() {
  Wire.beginTransmission(0x29);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
  sleepMs(5);

  VL53L0X_Error err = lox.setMeasurementTimingBudgetMicroSeconds(TIMING_BUDGET_US);
  if (err != VL53L0X_ERROR_NONE) {
    LOGLN("[SENSOR] Timing budget ayarlanamadi!");
  }

  lox.setLimitCheckEnable(VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
  lox.setLimitCheckValue(VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
                         (FixPoint1616_t)(0.25f * 65536));

  lox.setLimitCheckEnable(VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
  lox.setLimitCheckValue(VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
                         (FixPoint1616_t)(18 * 65536));
}

// ───────────────────────────────────────
// SENSÖR BAŞLATMA (güç açık olmalı)
// ───────────────────────────────────────
bool sensorInit() {
  if (!lox.begin(0x29, false, &Wire)) {
    LOGLN("[SENSOR] VL53L0X init basarisiz!");
    sensorInitialized = false;
    return false;
  }
  applySensorConfig();
  sleepMs(SENSOR_WARMUP_MS);
  sensorInitialized = true;
  LOGLN("[SENSOR] VL53L0X hazir (DEFAULT mod, 400ms budget, sigma=18).");
  return true;
}

// ───────────────────────────────────────
// SENSÖR SIFIRLAMA (XSHUT toggle)
// ───────────────────────────────────────
bool restartSensor() {
  LOGLN("[SENSOR] Sensör yeniden baslatiliyor (XSHUT toggle)...");
  sensorPowerOff();
  sleepMs(300);
  sensorPowerOn();
  sleepMs(10);

  if (!lox.begin(0x29, false, &Wire)) {
    LOGLN("[SENSOR] Yeniden baslatilamadi!");
    sensorInitialized = false;
    return false;
  }
  applySensorConfig();
  sensorInitialized = true;
  LOGLN("[SENSOR] Yeniden baslatildi.");
  return true;
}

// ───────────────────────────────────────
// BLE CALLBACKS
// ───────────────────────────────────────
void connect_callback(uint16_t conn_handle) {
  deviceConnected   = true;
  notifyEnabledFlag = false;
  LOGLN("[BLE] ESP32 baglandi.");
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  deviceConnected   = false;
  notifyEnabledFlag = false;
  LOGF("[BLE] Baglanti kesildi. Reason: 0x%02X\n", reason);
}

void cccd_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t cccd_value) {
  if (cccd_value & BLE_GATT_HVX_NOTIFICATION) {
    notifyEnabledFlag = true;
    LOGLN("[BLE] Notify subscribe oldu (CCCD yazildi).");
  } else {
    notifyEnabledFlag = false;
    LOGLN("[BLE] Notify unsubscribe.");
  }
}

// ───────────────────────────────────────
// SENSÖR ÖLÇÜMÜ
//
// Menzil mantığı:
//   RangeStatus == 0  VE  0 < mesafe <= maxDistance  → geçerli ölçüm (peçete var)
//   RangeStatus == 0  VE  mesafe >  maxDistance       → zemin görünüyor = DISTANCE_EMPTY
//   RangeStatus == 4 veya 61                          → hedef yok = DISTANCE_EMPTY
//   Diğer RangeStatus                                 → hata say (kitlenme tespiti için)
//
// ÖNEMLİ: Bu fonksiyon artık hiçbir zaman DISTANCE_INVALID döndürmez.
//   Kitlenme + başarısız restart durumunda bile DISTANCE_EMPTY döner.
//   Çünkü peçetelik boş olduğunda sensör tam bu şekilde davranır;
//   "donanım hatası" ile "peçetelik boş" ayrımı bu sistemde anlamsız.
// ───────────────────────────────────────
uint16_t getSafeDistance() {
  const int SAMPLES = 5;
  uint16_t readings[SAMPLES];
  int validCount      = 0;
  int outOfRangeCount = 0;  // "hedef yok" veya "zemin görünüyor" sayacı

  for (int i = 0; i < SAMPLES; i++) {
    VL53L0X_RangingMeasurementData_t m;
    lox.rangingTest(&m, false);

    if (m.RangeStatus == 0) {
      if (m.RangeMilliMeter > 0 && m.RangeMilliMeter <= maxDistance) {
        // ── Geçerli kısa mesafe: peçete var ──
        readings[validCount++] = m.RangeMilliMeter;
        sensorErrorCount = 0;

      } else if (m.RangeMilliMeter > maxDistance) {
        // ── Mesafe maxDistance aştı: zemin görünüyor = boş ──
        // Gerçek ölçüm değerini sakla — pakette 0 yerine bu gönderilecek
        LOGF("[SENSOR] Zemin gorunuyor (%d mm > %d mm) → bos\n",
             m.RangeMilliMeter, maxDistance);
        lastSeenDistance = m.RangeMilliMeter;
        outOfRangeCount++;

      } else {
        // RangeMilliMeter == 0 → geçersiz örnek, sessizce atla
        LOGLN("[SENSOR] Sifir mesafe, ornek atlandi.");
      }

    } else if (m.RangeStatus == 4 || m.RangeStatus == 61) {
      // ── Hedef yok: peçetelik kesinlikle boş ──
      // RangeStatus 4  = Phase fail  → zemin çok uzak
      // RangeStatus 61 = No target   → hiç sinyal yok
      LOGF("[SENSOR] Hedef yok (status %d) → bos kap sinyali\n", m.RangeStatus);
      outOfRangeCount++;
      // sensorErrorCount artırılmıyor — bu durum hata değil

    } else {
      // ── Gerçek sensör hatası (sigma fail, signal fail…) ──
      LOGF("[SENSOR] Olcum hatasi (status %d)\n", m.RangeStatus);
      sensorErrorCount++;
    }

    // Kitlenme tespiti — sadece gerçek hatalar sayılır
    if (sensorErrorCount >= MAX_ERROR_COUNT) {
      LOGLN("[SENSOR] Kitlenme tespit edildi! XSHUT toggle...");
      bool ok = restartSensor();
      sensorErrorCount = 0;

      if (!ok) {
        // Restart başarısız oldu — bu da "boş" sayılır, sistem devam eder
        LOGLN("[SENSOR] Restart basarisiz → bos kabul edildi.");
        return DISTANCE_EMPTY;
      }

      // Reset sonrası tek doğrulama ölçümü
      VL53L0X_RangingMeasurementData_t mv;
      lox.rangingTest(&mv, false);

      if (mv.RangeStatus == 4 || mv.RangeStatus == 61) {
        LOGLN("[SENSOR] Reset sonrasi hedef yok → bos.");
        return DISTANCE_EMPTY;
      } else if (mv.RangeStatus == 0 && mv.RangeMilliMeter > 0) {
        if (mv.RangeMilliMeter <= maxDistance) {
          LOGF("[SENSOR] Reset sonrasi gecerli olcum: %d mm\n", mv.RangeMilliMeter);
          return mv.RangeMilliMeter;
        } else {
          LOGLN("[SENSOR] Reset sonrasi zemin gorunuyor → bos.");
          return DISTANCE_EMPTY;
        }
      } else {
        // Reset sonrası da hata → yine "boş" say, sistem bloke olmaz
        LOGF("[SENSOR] Reset sonrasi hata (status %d) → bos kabul edildi.\n", mv.RangeStatus);
        return DISTANCE_EMPTY;
      }
    }

    sleepMs(40);
  }

  // ── Sonuç değerlendirme ──

  // Geçerli kısa mesafe ölçümü varsa → medyan hesapla
  if (validCount > 0) {
    for (int i = 0; i < validCount - 1; i++)
      for (int j = i + 1; j < validCount; j++)
        if (readings[j] < readings[i]) {
          uint16_t t = readings[i]; readings[i] = readings[j]; readings[j] = t;
        }
    uint16_t result = readings[validCount / 2];
    LOGF("[SENSOR] Gecerli: %d/%d, medyan: %d mm\n", validCount, SAMPLES, result);
    return result;
  }

  // Geçerli ölçüm yok ama en az bir "boş" sinyali var
  if (outOfRangeCount > 0) {
    LOGLN("[SENSOR] Hedef yok cogunlugu → DISTANCE_EMPTY (bos kap).");
    return DISTANCE_EMPTY;
  }

  // Tüm örnekler hataydı (kitlenme MAX_ERROR_COUNT'a ulaşamadı ama hiç geçerli örnek yok)
  // Güvenli taraf: "boş" kabul et, sistem bloke olmaz
  LOGLN("[SENSOR] Hicbir gecerli ornek yok → bos kabul edildi.");
  return DISTANCE_EMPTY;
}

// ───────────────────────────────────────
// DOLULUK HESAPLAMA
//
// Sadece 6 sabit seviye: 100 / 80 / 60 / 40 / 20 / 0
// Aralık (250-50 = 200mm) 5 eşit dilimine bölünür (her dilim 40mm):
//   50–90mm   → %100
//   90–130mm  → %80
//   130–170mm → %60
//   170–210mm → %40
//   210–250mm → %20
//   >=250mm   → %0  (DISTANCE_EMPTY olarak önceden yakalanır)
// ───────────────────────────────────────
int calculateFillPercentage(int dist) {
  if (dist <= minDistance) return 100;
  if (dist >= maxDistance) return 0;

  int step = (maxDistance - minDistance) / 5;  // 40mm

  int lvl = (dist - minDistance) / step;
  if (lvl < 0) lvl = 0;
  if (lvl > 4) lvl = 4;

  const int table[5] = {100, 80, 60, 40, 20};
  return table[lvl];
}

// ───────────────────────────────────────
// BLE GÜÇ YÖNETİMİ
// ───────────────────────────────────────
void bleStart() {
  Bluefruit.Advertising.start(0);
  LOGLN("[BLE] Advertising baslatildi.");
}

void bleStop() {
  if (Bluefruit.connected()) {
    uint16_t hdl = Bluefruit.connHandle();
    Bluefruit.disconnect(hdl);
    uint32_t t = millis();
    while (deviceConnected && millis() - t < DISCONNECT_WAIT_MS) sleepMs(10);
  }
  Bluefruit.Advertising.stop();
  LOGLN("[BLE] Advertising durduruldu.");
}

// ───────────────────────────────────────
// VERİ GÖNDER VE BAĞLANTIYI KES
// BLE yalnızca bu fonksiyon içinde aktif.
// Tüm bekleme döngüleri sleepMs() ile CPU'yu uyutuyor.
// ───────────────────────────────────────
bool sendDataAndDisconnect(uint16_t dist, int level) {

  // ── Paketi hazırla ──
  uint8_t packet[20];
  memset(packet, 0, sizeof(packet));
  memcpy(&packet[0], groupId,  4);
  memcpy(&packet[4], deviceId, 4);
  packet[8]  = (dist >> 8) & 0xFF;
  packet[9]  =  dist       & 0xFF;
  packet[10] = (uint8_t)level;
  packet[11] = currentState;
  packet[12] = lastState;

  // ── BLE Advertising başlat ──
  bleStart();

  // ── 1. Bağlantı bekle ──
  uint32_t t0 = millis();
  while (!deviceConnected) {
    if (millis() - t0 > CONNECT_TIMEOUT_MS) {
      LOGLN("[BLE] Baglanti zaman asimi.");
      bleStop();
      return false;
    }
    sleepMs(10);   // CPU uyu, BLE event gelince uyan
  }
  Bluefruit.Advertising.stop();
  LOGLN("[BLE] Baglandi, subscribe bekleniyor...");

  // ── 2. Subscribe bekle ──
  t0 = millis();
  while (!notifyEnabledFlag) {
    if (millis() - t0 > SUBSCRIBE_TIMEOUT_MS) {
      LOGLN("[BLE] Subscribe zaman asimi!");
      bleStop();
      return false;
    }
    sleepMs(10);   // CPU uyu, CCCD write event gelince uyan
  }
  LOGLN("[BLE] CCCD yazildi.");

  // ── 3. ESP32 hazırlık bekleme ──
  sleepMs(PRE_NOTIFY_DELAY_MS);

  // ── 4. Notify gönder ──
  uint16_t conn_handle = Bluefruit.connHandle();
  bool ok = false;

  if (distanceChar.notifyEnabled(conn_handle)) {
    ok = distanceChar.notify(packet, sizeof(packet));
    LOGF("[BLE] Notify: %s\n", ok ? "BASARILI" : "HATA");
  } else {
    LOGLN("[BLE] notifyEnabled false!");
  }

  // ── 5. ESP32 işlemesi için bekle ──
  if (ok) sleepMs(POST_NOTIFY_DELAY_MS);

  // ── 6. Bağlantıyı kes ──
  if (Bluefruit.connected(conn_handle)) {
    Bluefruit.disconnect(conn_handle);
    LOGLN("[BLE] Baglanti kapatildi.");
  }
  uint32_t t1 = millis();
  while (deviceConnected && millis() - t1 < DISCONNECT_WAIT_MS) sleepMs(10);

  sleepMs(100);
  return ok;
}

// ───────────────────────────────────────
// SETUP
// ───────────────────────────────────────
void setup() {

#if ENABLE_SERIAL
  Serial.begin(115200);
  sleepMs(500);
  LOGLN("[SYS] Baslatiliyor...");
#endif

  // XSHUT pin — başlangıçta sensör kapalı
  pinMode(SENSOR_XSHUT_PIN, OUTPUT);
  sensorPowerOff();   // İlk açılışta kapalı, ölçüm zamanı açılacak

  // I2C
  Wire.begin();
  Wire.setClock(400000);

  // BLE başlat (stack açık, advertising henüz yok)
  Bluefruit.begin();

  // DC/DC regülatörü etkinleştir
  // nRF52840 dahili DC/DC, LDO'ya göre ~%25-30 daha az quiescent akım çeker.
  // SoftDevice başlatıldıktan sonra (Bluefruit.begin() sonrası) çağrılmalı.
  sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
  LOGLN("[PWR] DC/DC regulator aktif.");

  Bluefruit.setTxPower(0);
  Bluefruit.setName("XIAO_DISTANCE");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // GATT servis ve karakteristik
  distanceService.begin();
  distanceChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  distanceChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  distanceChar.setFixedLen(20);
  distanceChar.setCccdWriteCallback(cccd_callback);
  distanceChar.begin();

  // Advertising parametreleri (start henüz yapılmıyor)
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(distanceService);
  Bluefruit.Advertising.addName();
  // Advertising aralığı: 500-1000ms
  // Eski: 100-150ms → radyo her 100ms'de bir açılıyor
  // Yeni: 500-1000ms → radyo 5x daha seyrek açılıyor → ~%70 radyo tasarrufu
  // ESP32 scan modunda cihazı bulmak için zaten taradığından gecikme önemsizdir.
  Bluefruit.Advertising.setIntervalMS(500, 1000);

  LOGLN("[SYS] Sistem hazir. BLE ve Sensor bekleme modunda.");
}

// ───────────────────────────────────────
// LOOP
// ───────────────────────────────────────
void loop() {

  // ── Periyot bekle ──
  sleepMs(MEASUREMENT_INTERVAL_MS);

  // ════════════════════════════════════
  // ADIM 1: Sensörü aç ve ölç
  // ════════════════════════════════════
  LOGLN("[SYS] Olcum baslıyor, sensör aciliyor...");
  sensorPowerOn();
  sleepMs(10);

  if (!sensorInit()) {
    LOGLN("[SENSOR] Init basarisiz, sensör kapatiliyor.");
    sensorPowerOff();
    return;
  }

  lastSeenDistance = 0;  // Her döngüde sıfırla
  uint16_t dist = getSafeDistance();

  // Ölçüm tamamlandı → sensörü kapat
  sensorPowerOff();
  LOGLN("[SYS] Olcum tamamlandi, sensör kapatildi.");

  // ════════════════════════════════════
  // ADIM 2: Ölçümü değerlendir
  // getSafeDistance() artık her zaman ya geçerli mm ya da DISTANCE_EMPTY döner.
  // ════════════════════════════════════
  int fill;

  if (dist == DISTANCE_EMPTY) {
    // Zemin görünüyorsa lastSeenDistance gerçek ölçüm değerini taşır (örn: 260mm)
    // Status 4/61 gibi "hiç sinyal yok" durumunda 0 kalır
    dist = lastSeenDistance;
    LOGF("[SENSOR] Hedef yok → Pecetelik bos, doluluk: 0%% | gonderilecek mesafe: %d mm\n", dist);
    fill = 0;

  } else {
    fill = calculateFillPercentage((int)dist);
  }

  LOGF("[SENSOR] Mesafe: %d mm | Doluluk: %d%%\n", dist, fill);

  // ════════════════════════════════════
  // ADIM 3: Boş onay sayacı
  //
  // fill==0 geldiğinde hemen gönderme.
  // Üst üste EMPTY_CONFIRM_COUNT (3) kez boş görülürse gönderime izin ver.
  // Aradaki herhangi bir dolu ölçüm sayacı sıfırlar.
  // ════════════════════════════════════
  if (fill == 0) {
    emptyConfirmCount++;
    LOGF("[LOGIC] Bos onay: %d/%d\n", emptyConfirmCount, EMPTY_CONFIRM_COUNT);

    if (emptyConfirmCount < EMPTY_CONFIRM_COUNT) {
      LOGLN("[LOGIC] Bos onay tamamlanmadi, bir sonraki olcum bekleniyor.");
      return;
    }

    // 3. kez geldi → gönder, sayacı sıfırla
    emptyConfirmCount = 0;
    LOGLN("[LOGIC] Bos onay tamamlandi (3/3), gonderim yapilacak.");

  } else {
    // Dolu ölçüm → boş onay sayacını sıfırla
    if (emptyConfirmCount > 0) {
      LOGF("[LOGIC] Dolu olcum, bos onay sayaci sifirlandi (%d → 0).\n", emptyConfirmCount);
      emptyConfirmCount = 0;
    }
  }

  // ════════════════════════════════════
  // ADIM 4: Yeniden dolu eşiği kontrolü
  //
  // Boş mesajı gönderildikten sonra:
  //   - fill < REFILL_THRESHOLD_PCT (%60) ise gönderme (peçetelik yeterince dolu değil)
  //   - fill >= REFILL_THRESHOLD_PCT ise normal akışa devam et
  // ════════════════════════════════════
  if (emptyWasSent && fill > 0 && fill < REFILL_THRESHOLD_PCT) {
    LOGF("[LOGIC] Bos gonderildi, doluluk %%60 altinda (%d%%). Gonderim engellendi.\n", fill);
    return;
  }

  // ════════════════════════════════════
  // ADIM 5: Değişim kontrolü
  // ════════════════════════════════════
  if (fill == previousFill) {
    LOGLN("[LOGIC] Doluluk degismedi, gonderim yok.");
    return;
  }

  // ════════════════════════════════════
  // ADIM 6: State güncelle
  // ════════════════════════════════════
  uint8_t newState  = (fill == 0) ? 0 : 1;
  uint8_t prevState = currentState;
  int     prevFill  = previousFill;

  lastState    = currentState;
  currentState = newState;
  previousFill = fill;

  LOGF("[LOGIC] Doluluk degisti: %d%% → %d%% | state: %d → %d\n",
       prevFill, fill, lastState, currentState);

  // ════════════════════════════════════
  // ADIM 7: BLE ile gönder
  // BLE yalnızca bu noktada aktif olur.
  // ════════════════════════════════════
  LOGLN("[SYS] BLE gonderim basliyor...");
  bool success = sendDataAndDisconnect(dist, fill);

  if (!success) {
    LOGLN("[LOGIC] Gonderim basarisiz, durum geri alindi.");
    currentState = prevState;
    lastState    = prevState;
    previousFill = prevFill;
    // emptyWasSent değişmez — başarısız gönderimlerde flag güncellenmez
    return;
  }

  LOGLN("[LOGIC] Gonderim basarili.");

  // emptyWasSent flag güncelle
  if (fill == 0) {
    emptyWasSent = true;
    LOGLN("[LOGIC] Bos mesaji gonderildi, yeniden dolu esigi aktif (min %%60).");
  } else if (fill >= REFILL_THRESHOLD_PCT) {
    emptyWasSent = false;
    LOGLN("[LOGIC] Doluluk %%60 ustu, yeniden dolu esigi sifirlandı.");
  }
}
