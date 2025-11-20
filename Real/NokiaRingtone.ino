// === LIBRARY ===
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <NewPing.h>
#include <Firebase_ESP_Client.h>
#include "pitches.h"

const char* ssid     = "ssid";
const char* password = "password";

#define DATABASE_URL "https://firebase-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "secret"

const uint8_t POMPA1_PIN = 23;
const uint8_t POMPA2_PIN = 32;
const uint8_t POMPA3_PIN = 33;
const uint8_t POMPA4_PIN = 26;
const uint8_t MOTOR_PIN  = 27;
const uint8_t SYSTEM_PIN = 14;

const uint8_t BUZZER_PIN  = 13;   // NOKIA
const uint8_t BUZZER2_PIN = 12;   // BIP 1 DETIK

// HC-SR04 utama
const uint8_t TRIG_PIN = 18;
const uint8_t ECHO_PIN = 19;

// HC-SR04 tangki
const uint8_t TRIG2_PIN = 17;
const uint8_t ECHO2_PIN = 5;

const unsigned int MAX_DISTANCE_CM = 400;

const uint8_t SDA_PIN = 21;
const uint8_t SCL_PIN = 22;

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM);
NewPing sonarTank(TRIG2_PIN, ECHO2_PIN, MAX_DISTANCE_CM);

bool loxReady = false;

const uint16_t TOF_EMPTY_MM  = 100;
const uint16_t ULTRASONIC_THRESHOLD_CM = 40;
const uint16_t TANK_FULL_THRESHOLD_CM = 10;

uint16_t tof_mm   = 65535;
uint16_t us_cm    = 65535;
uint16_t tank_cm  = 65535;

unsigned long lastSend = 0;
const unsigned long sendInterval = 3000;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===========================
//    RINGTONE NOKIA (PIN 13)
// ===========================
const int nokia_melody[] = {
  NOTE_E5, NOTE_D5, NOTE_FS4, NOTE_GS4,
  NOTE_CS5, NOTE_B4, NOTE_D4, NOTE_E4,
  NOTE_B4, NOTE_A4, NOTE_CS4, NOTE_E4,
  NOTE_A4
};

const int nokia_duration[] = {
  150,150,150,150,
  150,150,150,150,
  150,150,150,150,
  300
};

const int nokia_len = sizeof(nokia_melody) / sizeof(nokia_melody[0]);

bool nokiaActive = false;
unsigned long nokiaLastChange = 0;
int nokiaIndex = 0;

// ============================
// PLAY NOKIA BUZZER PIN 13
// ============================
void playNokia() {
  if (!nokiaActive) return;

  if (millis() - nokiaLastChange >= nokia_duration[nokiaIndex]) {
    int f = nokia_melody[nokiaIndex];

    if (f > 0) tone(BUZZER_PIN, f);
    else noTone(BUZZER_PIN);

    nokiaLastChange = millis();
    nokiaIndex++;

    if (nokiaIndex >= nokia_len)
      nokiaIndex = 0;   // Loop ringtone
  }
}


// ===================================================================
//                      SENSOR FUNCTIONS
// ===================================================================
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


// ===================================================================
//                     BUZZER CONTROLLER
// ===================================================================
void updateBuzzer() {

  bool tofEmpty = (tof_mm == 65535) || (tof_mm > TOF_EMPTY_MM);
  bool usEmpty  = (us_cm  == 65535) || (us_cm  > ULTRASONIC_THRESHOLD_CM);
  bool sensorsEmpty = tofEmpty && usEmpty;

  // =======================
  // BUZZER 12 → BIP 1 DETIK
  // =======================
  if (sensorsEmpty) {
    unsigned long t = millis() % 1000; // periode 1 detik

    if (t < 500)
      digitalWrite(BUZZER2_PIN, HIGH);   // 0–500 ms bunyi
    else
      digitalWrite(BUZZER2_PIN, LOW);    // 500–1000 ms diam

  } else {
    digitalWrite(BUZZER2_PIN, LOW);
  }

  // =======================
  // BUZZER 13 → NOKIA TONE
  // =======================
  if (digitalRead(SYSTEM_PIN) == HIGH) {

    if (!nokiaActive) {
      nokiaActive = true;
      nokiaLastChange = millis();
      nokiaIndex = 0;
    }

  } else {
    nokiaActive = false;
    nokiaIndex = 0;
    noTone(BUZZER_PIN);
  }

  playNokia();
}


// ===================================================================
void sendToFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;

  int pompa1 = readDevice(POMPA1_PIN, true);
  int pompa2 = readDevice(POMPA2_PIN, true);
  int pompa3 = readDevice(POMPA3_PIN, true);
  int pompa4 = readDevice(POMPA4_PIN, true);
  int motor  = readDevice(MOTOR_PIN,  true);
  int system = readDevice(SYSTEM_PIN, true);

  int tof_status   = (tof_mm  != 65535 && tof_mm  <= TOF_EMPTY_MM)            ? 1 : 0;
  int us_status    = (us_cm   != 65535 && us_cm   <= ULTRASONIC_THRESHOLD_CM) ? 1 : 0;
  int tank_status  = (tank_cm != 65535 && tank_cm <= TANK_FULL_THRESHOLD_CM)  ? 1 : 0;

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

  Firebase.RTDB.setJSON(&fbdo, "/data", &json);
}


// ===================================================================
void setup() {
  Serial.begin(115200);

  pinMode(POMPA1_PIN, INPUT_PULLUP);
  pinMode(POMPA2_PIN, INPUT_PULLUP);
  pinMode(POMPA3_PIN, INPUT_PULLUP);
  pinMode(POMPA4_PIN, INPUT_PULLUP);
  pinMode(MOTOR_PIN,  INPUT_PULLUP);
  pinMode(SYSTEM_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUZZER2_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(BUZZER2_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  loxReady = lox.begin();
  Serial.println(loxReady ? "VL53L0X ready" : "VL53L0X NOT detected");

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}


// ===================================================================
void loop() {
  readSensors();
  updateBuzzer();

  Serial.printf("TOF:%u | US:%u | TANK:%u | NOKIA:%d\n",
                tof_mm, us_cm, tank_cm,
                nokiaActive ? 1 : 0);

  if (millis() - lastSend >= sendInterval) {
    sendToFirebase();
    lastSend = millis();
  }
}
