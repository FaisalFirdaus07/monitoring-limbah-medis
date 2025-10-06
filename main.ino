#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <NewPing.h>
#include <Firebase_ESP_Client.h>

// ======= USER CONFIG =======
const char* ssid     = "ssid";
const char* password = "password";

// Firebase Project Settings
#define DATABASE_URL "Your_database_url/"
#define DATABASE_SECRET "secret"

// ======= PIN MAPPING =======
const uint8_t POMPA1_PIN = 14;
const uint8_t POMPA2_PIN = 33;
const uint8_t POMPA3_PIN = 26;
const uint8_t POMPA4_PIN = 32;
const uint8_t MOTOR_PIN  = 27;
const uint8_t SYSTEM_PIN = 23;

const uint8_t BUZZER_PIN = 13;

// HC-SR04 utama
const uint8_t TRIG_PIN = 18;
const uint8_t ECHO_PIN = 19;

// HC-SR04 tambahan (tangki)
const uint8_t TRIG2_PIN = 17;
const uint8_t ECHO2_PIN = 5;

const unsigned int MAX_DISTANCE_CM = 400;

const uint8_t SDA_PIN = 21;
const uint8_t SCL_PIN = 22;

// ======= SENSORS =======
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM);
NewPing sonarTank(TRIG2_PIN, ECHO2_PIN, MAX_DISTANCE_CM);

bool loxReady = false;

// ======= THRESHOLDS =======
const uint16_t TOF_EMPTY_MM  = 100;
const uint16_t ULTRASONIC_THRESHOLD_CM = 40;

// ======= RUNTIME VARS =======
uint16_t tof_mm   = 65535;
uint16_t us_cm    = 65535;
uint16_t tank_cm  = 65535;

bool buzzerOn = false;

// ======= BUZZER IRAMA =======
const unsigned long buzzOnTime  = 500;
const unsigned long buzzOffTime = 500;
unsigned long lastBuzzToggle = 0;
bool buzzerState = false;

// ======= TIMING FIREBASE =======
unsigned long lastSend = 0;
const unsigned long sendInterval = 3000;

// ======= FIREBASE OBJECT =======
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ======= HELPERS =======
int readOptoButton(uint8_t pin) {
  return (digitalRead(pin) == LOW) ? 1 : 0;
}

int readDevice(uint8_t pin, bool inverted = false) {
  int val = readOptoButton(pin);
  return inverted ? !val : val;
}

void readSensors() {
  if (loxReady) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    tof_mm = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : 65535;
  }

  us_cm = sonar.ping_cm();
  if (us_cm == 0) us_cm = 65535;

  tank_cm = sonarTank.ping_cm();
  if (tank_cm == 0) tank_cm = 65535;
}

void updateBuzzer() {
  bool tofEmpty = (tof_mm == 65535) || (tof_mm > TOF_EMPTY_MM);
  bool usEmpty  = (us_cm  == 65535) || (us_cm  > ULTRASONIC_THRESHOLD_CM);
  bool needBuzz = tofEmpty && usEmpty;

  unsigned long now = millis();
  buzzerOn = needBuzz;

  if (needBuzz) {
    if (buzzerState && now - lastBuzzToggle >= buzzOnTime) {
      buzzerState = false;
      lastBuzzToggle = now;
    } else if (!buzzerState && now - lastBuzzToggle >= buzzOffTime) {
      buzzerState = true;
      lastBuzzToggle = now;
    }
  } else {
    buzzerState = false;
    lastBuzzToggle = now;
  }

  digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
}

void sendToFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;

  // === Baca status perangkat ===
  int pompa1 = readDevice(POMPA1_PIN, true);
  int pompa2 = readDevice(POMPA2_PIN, true);
  int pompa3 = readDevice(POMPA3_PIN, true);
  int pompa4 = readDevice(POMPA4_PIN, true);
  int motor  = readDevice(MOTOR_PIN,  true);
  int system = readDevice(SYSTEM_PIN, true);

  // === Logika status sensor ===
  int tof_status   = (tof_mm  != 65535 && tof_mm  <= TOF_EMPTY_MM)            ? 1 : 0;
  int us_status    = (us_cm   != 65535 && us_cm   <= ULTRASONIC_THRESHOLD_CM) ? 1 : 0;
  int tank_status  = (tank_cm != 65535 && tank_cm <= ULTRASONIC_THRESHOLD_CM) ? 1 : 0;

  // === Buat JSON ===
  FirebaseJson json;
  json.set("pompa1", pompa1);
  json.set("pompa2", pompa2);
  json.set("pompa3", pompa3);
  json.set("pompa4", pompa4);
  json.set("motor", motor);
  json.set("system", system);
  json.set("tof", tof_status);
  json.set("us", us_status);
  json.set("tank", tank_status);

  // === Kirim ke Firebase ===
  if (Firebase.RTDB.setJSON(&fbdo, "/data", &json)) {
    Serial.println("Firebase update OK");
  } else {
    Serial.printf("Firebase update FAIL: %s\n", fbdo.errorReason().c_str());
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(POMPA1_PIN, INPUT_PULLUP);
  pinMode(POMPA2_PIN, INPUT_PULLUP);
  pinMode(POMPA3_PIN, INPUT_PULLUP);
  pinMode(POMPA4_PIN, INPUT_PULLUP);
  pinMode(MOTOR_PIN,  INPUT_PULLUP);
  pinMode(SYSTEM_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  loxReady = lox.begin();
  Serial.println(loxReady ? "VL53L0X ready" : "VL53L0X NOT detected");

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // Firebase Setup
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  readSensors();
  updateBuzzer();

  Serial.printf("TOF:%u | US:%u | TANK:%u | SYS:%d | BZ:%d\n",
                tof_mm, us_cm, tank_cm,
                readDevice(SYSTEM_PIN, true),
                buzzerOn ? 1 : 0);

  if (millis() - lastSend >= sendInterval) {
    sendToFirebase();
    lastSend = millis();
  }
}