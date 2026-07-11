#include "secrets.h"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Pin Mapping ----------
#define DHT_PIN     4
#define PIR_PIN     27
#define MQ2_PIN     34
#define BUZZER_PIN  25
#define RELAY_PIN   26

// ---------- Objek Sensor & LCD ----------
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // alamat I2C umum: 0x27, kalau blank coba 0x3F

// ---------- Threshold (ambang batas) ----------
const int GAS_THRESHOLD  = 2800;  // baseline aman ~843, bahaya mendekati 4041
const float TEMP_THRESHOLD = 32.0; // suhu kandang dalam Celsius

// ---------- Variabel Global ----------
unsigned long lastSensorRead = 0;
const long sensorInterval = 2000; // baca sensor tiap 2 detik

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  dht.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Kandang Pintar");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  delay(1500);
  lcd.clear();
}

void loop() {
  Blynk.run();

  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    readAndProcessSensors();
  }
}

// ---------- Noise Averaging untuk Sensor Gas (MQ2) ----------
int readGasAveraged() {
  const int numReadings = 10;
  long total = 0;
  for (int i = 0; i < numReadings; i++) {
    total += analogRead(MQ2_PIN);
    delay(5);
  }
  return total / numReadings;
}

void readAndProcessSensors() {
  // --- Baca DHT22 ---
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // --- Baca PIR ---
  int motionDetected = digitalRead(PIR_PIN);

  // --- Baca MQ2 ---
  int gasValue = readGasAveraged();

  // Validasi DHT22 (kadang gagal baca, harus dicek)
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Gagal membaca DHT22!");
    return;
  }

  // --- Kirim data ke Blynk ---
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, gasValue);
  Blynk.virtualWrite(V3, motionDetected);

  // --- Logika Alarm Gerakan (Predator/Pencuri) ---
  if (motionDetected == HIGH) {
    digitalWrite(BUZZER_PIN, HIGH);
    Blynk.logEvent("motion_alert", "Gerakan terdeteksi di kandang!");
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // --- Logika Gas Berbahaya ---
  bool gasDanger = gasValue > GAS_THRESHOLD;
  if (gasDanger) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(RELAY_PIN, HIGH); // nyalakan kipas exhaust
    Blynk.logEvent("gas_alert", "Gas berbahaya terdeteksi di kandang!");
  }

  // --- Logika Suhu Tinggi (tanpa alarm, cuma kipas) ---
  bool tempHigh = temperature > TEMP_THRESHOLD;
  if (tempHigh && !gasDanger) {
    digitalWrite(RELAY_PIN, HIGH);
  } else if (!tempHigh && !gasDanger) {
    digitalWrite(RELAY_PIN, LOW);
  }

  // --- Update LCD ---
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print((char)223); // simbol derajat
  lcd.print("C H:");
  lcd.print(humidity, 0);
  lcd.print("%  ");

  lcd.setCursor(0, 1);
  if (motionDetected == HIGH) {
    lcd.print("MOTION DETECTED!");
  } else if (gasDanger) {
    lcd.print("GAS BAHAYA!    ");
  } else {
    lcd.print("Status: Aman    ");
  }

  // --- Debug ke Serial Monitor ---
  Serial.print("Suhu: "); Serial.print(temperature);
  Serial.print(" C | Kelembaban: "); Serial.print(humidity);
  Serial.print(" % | Gas: "); Serial.print(gasValue);
  Serial.print(" | Motion: "); Serial.println(motionDetected);
}