#include <Wire.h>
#include <Adafruit_CCS811.h>
#include <Adafruit_AHTX0.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ================== CẤU HÌNH ==================
#define FAKE_BATTERY 1

// ================== DEVICE ==================
const char *device_id = "ESP32_ENV_01";   // 🔧 Đổi tên thiết bị tại đây

// ================== WIFI ==================
const char *ssid = "Villa_3lau";
const char *pass = "23456778";
const char *serverName = "http://192.168.10.154:3000/api/devices/data";
// const char *ssid = "NW-TEST03";
// const char *pass = "ivc10nam2014";
const char *serverName ="http://192.168.10.154:3000/api/devices/data";
// ================== PIN ESP32-WROOM ==================
#define I2C_SDA    21
#define I2C_SCL    22

#define LDR_PIN    33
#define DUST_ADC   32
#define LED_DUST   25

// ================== PIN RANGE ==================
#define BAT_MIN 3.0
#define BAT_MAX 4.2
#define DUST_V0  0.25

// ================== THIẾT BỊ ==================
Adafruit_CCS811 ccs;
Adafruit_AHTX0 aht;

// ================== ĐỌC BỤI GP2Y1010 ==================
float readDustGP2Y() {
  digitalWrite(LED_DUST, LOW);
  delayMicroseconds(280);
  int adc = analogRead(DUST_ADC);
  delayMicroseconds(40);
  digitalWrite(LED_DUST, HIGH);
  delayMicroseconds(9680);

  float voltage = adc * 3.3 / 4095.0;
  float dust = (voltage - DUST_V0) / 0.005;
  return dust < 0 ? 0 : dust;
}

// ================== % PIN ==================
int batteryPercent(float v) {
  int p = (v - BAT_MIN) * 100 / (BAT_MAX - BAT_MIN);
  return constrain(p, 0, 100);
}

// ================== GỬI SERVER ==================
void sendData(float t, float h, float lux, int co2, float dust, int batPercent) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;

  doc["device_id"]   = device_id;
  doc["temperature"] = round(t * 10) / 10.0;
  doc["humidity"]    = round(h * 10) / 10.0;
  doc["lux"]         = (int)lux;
  doc["co2"]         = co2;
  doc["dust"]        = (int)dust;
  doc["battery"]     = batPercent;   // 🔋 chỉ gửi %

  String payload;
  serializeJson(doc, payload);

  Serial.println(" JSON gửi:");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  Serial.printf(" HTTP Response: %d\n", httpCode);

  http.end();
}

// ================== WIFI ==================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print(" Kết nối WiFi");
  unsigned long t0 = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(WiFi.status() == WL_CONNECTED ? "\n WiFi OK" : "\n WiFi FAIL");
}

// ================== SETUP ==================
void setup() {
  Serial.begin(9600);

  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(LED_DUST, OUTPUT);
  digitalWrite(LED_DUST, HIGH);

  analogReadResolution(12);

  connectWiFi();

  if (!aht.begin()) {
    Serial.println(" Không tìm thấy AHT20");
  }

  if (!ccs.begin()) {
    Serial.println(" Không tìm thấy CCS811");
  }

  randomSeed(esp_random());
}

// ================== LOOP ==================
void loop() {
  sensors_event_t hum, temp;
  aht.getEvent(&hum, &temp);

  int eco2 = 0;
  if (ccs.available() && !ccs.readData()) {
    eco2 = ccs.geteCO2();
  }

  float dust = readDustGP2Y();
  float lux  = analogRead(LDR_PIN) * 0.25;

#if FAKE_BATTERY
  float vbat = random(3000, 4201) / 1000.0;
#else
  float vbat = 3.9; // nếu dùng pin thật thì đọc ADC
#endif

  int batPercent = batteryPercent(vbat);

  Serial.printf(
    " %s | T %.1fC | H %.1f%% | CO2 %d | Dust %.0f | Lux %.0f | Bat %d%%\n",
    device_id,
    temp.temperature,
    hum.relative_humidity,
    eco2,
    dust,
    lux,
    batPercent
  );

  sendData(
    temp.temperature,
    hum.relative_humidity,
    lux,
    eco2,
    dust,
    batPercent
  );

  delay(1000);
}
