#include <ArduinoJson.h>

// === ROBOT GEOMETRİ PARAMETRELERİ ===
const float WHEEL_BASE = 0.30;     // metre (iki tekerlek arası mesafe)
const float WHEEL_DIAMETER = 0.10; // metre
const float WHEEL_CIRC = WHEEL_DIAMETER * 3.1415926535;

// === ZAMANLANDIRMA / KONTROL ARALIĞI ===
const unsigned long CONTROL_INTERVAL_MS = 50; // 20 Hz

// === ENCODER DEĞİŞKENLERİ ===
volatile long pulseCount1 = 0;
volatile long pulseDelta1 = 0;
volatile long pulseCount2 = 0;
volatile long pulseDelta2 = 0;

unsigned long lastControlTime = 0;
float rpm1 = 0.0, rpm2 = 0.0;

const int PPR = 7600; // pulses per revolution

float CAL_LEFT  = 1.00;   
float CAL_RIGHT = 1.022; 

// === MOTOR PINLERİ ===
#define MOTOR1_FWD 4
#define MOTOR1_REV 5
#define MOTOR2_FWD 6
#define MOTOR2_REV 7

// === PID PARAMETRELERİ ===
// Sol motor
float Kp1 = 0.45;
float Ki1 = 0.30;
float Kd1 = 0.00;

// Sağ motor
float Kp2 = 0.40;
float Ki2 = 0.28;
float Kd2 = 0.00;

// PID iç durumu
float integral1 = 0.0, previousError1 = 0.0;
float integral2 = 0.0, previousError2 = 0.0;

// Anti-windup sınırları
const float INTEGRAL_LIMIT = 500.0;

// PWM sınır
const float PWM_LIMIT = 255.0;

// Hedef RPM değerleri
float targetRPM1 = 0.0; // sol
float targetRPM2 = 0.0; // sağ

// PWM çıktı değişkenleri
float pwmOut1 = 0.0;
float pwmOut2 = 0.0;

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR1_FWD, OUTPUT);
  pinMode(MOTOR1_REV, OUTPUT);
  pinMode(MOTOR2_FWD, OUTPUT);
  pinMode(MOTOR2_REV, OUTPUT);

  // Encoder1
  pinMode(19, INPUT_PULLUP);
  pinMode(18, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(19), readA1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(18), readB1, CHANGE);

  // Encoder2
  pinMode(21, INPUT_PULLUP);
  pinMode(20, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(21), readA2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(20), readB2, CHANGE);

  lastControlTime = millis();
}

void loop() {
  parseSerial();

  unsigned long now = millis();
  unsigned long dt_ms = now - lastControlTime;

  if (dt_ms >= CONTROL_INTERVAL_MS) {
    noInterrupts();
    long d1 = pulseDelta1; pulseDelta1 = 0;
    long d2 = pulseDelta2; pulseDelta2 = 0;
    long pos1 = pulseCount1;
    long pos2 = pulseCount2;
    interrupts();

    float dt_s = (float)dt_ms / 1000.0;

    if (dt_s > 0.0) {
      rpm1 = ((float)d1 / (float)PPR) * (60.0 / dt_s);
      rpm2 = ((float)d2 / (float)PPR) * (60.0 / dt_s);
    }

    updatePID(dt_s);
    applyPWM();
    sendFeedback(rpm1, rpm2, pos1, pos2);

    lastControlTime = now;
  }
}

// === PID hesaplama ===
void updatePID(float dt_s) {
  if (dt_s <= 0.0) return;

  // Hedef hız çok küçükse hızlı durma modu
  if (fabs(targetRPM1) < 0.5 && fabs(targetRPM2) < 0.5) {
    pwmOut1 = 0;
    pwmOut2 = 0;
    integral1 = 0;
    integral2 = 0;
    previousError1 = 0;
    previousError2 = 0;

    // Aktif frenleme
    analogWrite(MOTOR1_FWD, 0);
    analogWrite(MOTOR1_REV, 0);
    analogWrite(MOTOR2_FWD, 0);
    analogWrite(MOTOR2_REV, 0);
    return;
  }

  // Sol motor PID
  float error1 = targetRPM1 - rpm1;
  integral1 += error1 * dt_s;
  if (integral1 > INTEGRAL_LIMIT) integral1 = INTEGRAL_LIMIT;
  if (integral1 < -INTEGRAL_LIMIT) integral1 = -INTEGRAL_LIMIT;
  float derivative1 = (error1 - previousError1) / dt_s;
  pwmOut1 = Kp1 * error1 + Ki1 * integral1 + Kd1 * derivative1;

  // Sağ motor PID
  float error2 = targetRPM2 - rpm2;
  integral2 += error2 * dt_s;
  if (integral2 > INTEGRAL_LIMIT) integral2 = INTEGRAL_LIMIT;
  if (integral2 < -INTEGRAL_LIMIT) integral2 = -INTEGRAL_LIMIT;
  float derivative2 = (error2 - previousError2) / dt_s;
  pwmOut2 = Kp2 * error2 + Ki2 * integral2 + Kd2 * derivative2;

  // PWM limit
  if (pwmOut1 > PWM_LIMIT) pwmOut1 = PWM_LIMIT;
  if (pwmOut1 < -PWM_LIMIT) pwmOut1 = -PWM_LIMIT;
  if (pwmOut2 > PWM_LIMIT) pwmOut2 = PWM_LIMIT;
  if (pwmOut2 < -PWM_LIMIT) pwmOut2 = -PWM_LIMIT;

  previousError1 = error1;
  previousError2 = error2;
}

// === Motor sürme ===
void applyPWM() {
  int duty1 = (int)abs(round(pwmOut1));
  int duty2 = (int)abs(round(pwmOut2));
  if (duty1 > 255) duty1 = 255;
  if (duty2 > 255) duty2 = 255;

  if (pwmOut1 >= 0) {
    analogWrite(MOTOR1_FWD, duty1);
    analogWrite(MOTOR1_REV, 0);
  } else {
    analogWrite(MOTOR1_FWD, 0);
    analogWrite(MOTOR1_REV, duty1);
  }

  if (pwmOut2 >= 0) {
    analogWrite(MOTOR2_FWD, duty2);
    analogWrite(MOTOR2_REV, 0);
  } else {
    analogWrite(MOTOR2_FWD, 0);
    analogWrite(MOTOR2_REV, duty2);
  }
}

// === Encoder ISR'leri ===
void readA1() {
  if (digitalRead(19) == digitalRead(18)) { pulseCount1++; pulseDelta1++; }
  else { pulseCount1--; pulseDelta1--; }
}
void readB1() {
  if (digitalRead(19) != digitalRead(18)) { pulseCount1++; pulseDelta1++; }
  else { pulseCount1--; pulseDelta1--; }
}
void readA2() {
  if (digitalRead(21) == digitalRead(20)) { pulseCount2++; pulseDelta2++; }
  else { pulseCount2--; pulseDelta2--; }
}
void readB2() {
  if (digitalRead(21) != digitalRead(20)) { pulseCount2++; pulseDelta2++; }
  else { pulseCount2--; pulseDelta2--; }
}

// === Serial parse ===
void parseSerial() {
  if (Serial.available() == 0) return;

  String input = Serial.readStringUntil('\n');
  input.trim();
  if (input.length() == 0) return;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, input);
  if (err) return;

  // Twist formatı
  if (doc.containsKey("linear") && doc.containsKey("angular")) {
    float linear_x = 0.0;
    float angular_z = 0.0;

    if (doc["linear"].is<float>() || doc["linear"].is<double>() || doc["linear"].is<long>()) {
      linear_x = doc["linear"].as<float>();
    } else if (doc["linear"].containsKey("x")) {
      linear_x = doc["linear"]["x"].as<float>();
    }

    if (doc["angular"].containsKey("z")) {
      angular_z = doc["angular"]["z"].as<float>();
    }

    linear_x = linear_x * 2.78;
    angular_z = angular_z * 2.78;
    
    float v_left  = linear_x - angular_z * (WHEEL_BASE / 2.0);
    float v_right = linear_x + angular_z * (WHEEL_BASE / 2.0);

    targetRPM1 = ((v_left / WHEEL_CIRC) * 60.0) * CAL_LEFT;
    targetRPM2 = ((v_right / WHEEL_CIRC) * 60.0) * CAL_RIGHT;
    return;
  }

  // Doğrudan RPM komutu
  if (doc.containsKey("rpm")) {
    if (doc["rpm"].containsKey("left"))  targetRPM1 = doc["rpm"]["left"].as<float>();
    if (doc["rpm"].containsKey("right")) targetRPM2 = doc["rpm"]["right"].as<float>();
    return;
  }
}

// === Feedback gönder ===
void sendFeedback(float r1, float r2, long pos1, long pos2) {
  StaticJsonDocument<192> doc;
  doc["rpm"]["left"]  = r1;
  doc["rpm"]["right"] = r2;
  doc["pos"]["left"]  = pos1;
  doc["pos"]["right"] = pos2;

  serializeJson(doc, Serial);
  Serial.println();
}
