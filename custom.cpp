#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ================== CẤU HÌNH ==================
#define FAKE_SENSOR   1     // 1 = giả lập toàn bộ cảm biến
#define FAKE_BATTERY  1

// ================== WIFI ==================
const char *ssid = "NW-TEST02";
const char *pass = "ivc10nam2014";
const char *serverName = "http://192.168.121.114:3000/api/devices/data";

// ================== NTP ==================
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 7 * 3600
#define DAYLIGHT_OFFSET_SEC 0

// ================== PIN (giữ để sau dùng sensor thật) ==================
#define I2C_SDA    21
#define I2C_SCL    22
#define LDR_PIN    33
#define DUST_ADC   32
#define LED_DUST   25

// ================== PIN RANGE ==================
#define BAT_MIN 3.0
#define BAT_MAX 4.2
#define DUST_V0  0.25

// ================== BIẾN TOÀN CỤC ==================
String deviceMac;

// ================== GIẢ LẬP SENSOR ==================
float fakeTemperature() { return random(250, 350) / 10.0; }
float fakeHumidity()    { return random(400, 800) / 10.0; }
int   fakeCO2()         { return random(400, 1500); }
float fakeDust()        { return random(5, 150); }
float fakeLux()         { return random(50, 1000); }
float fakeBatteryVolt() { return random(3000, 4201) / 1000.0; }

// ================== % PIN ==================
int batteryPercent(float v) {
  int p = (v - BAT_MIN) * 100 / (BAT_MAX - BAT_MIN);
  return constrain(p, 0, 100);
}

// ================== LẤY THỜI GIAN ==================
String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00";
  }
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
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

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi OK");
  } else {
    Serial.println("\n WiFi FAIL");
  }
}

// ================== GỬI SERVER ==================
void sendData(float t, float h, float lux, int co2, float dust, int batPercent) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["mac_addr"]   = deviceMac;
  doc["temperature"] = round(t * 10) / 10.0;
  doc["humidity"]    = round(h * 10) / 10.0;
  doc["lux"]         = (int)lux;
  doc["co2"]         = co2;
  doc["dust"]        = (int)dust;
  doc["battery"]     = batPercent;
  doc["timestamp"]   = getTimeString();

  String payload;
  serializeJson(doc, payload);

  Serial.println(" JSON gửi:");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  Serial.printf(" HTTP Response: %d\n", httpCode);

  http.end();
}

// ================== SETUP ==================
void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire.begin(I2C_SDA, I2C_SCL);
  pinMode(LED_DUST, OUTPUT);
  digitalWrite(LED_DUST, HIGH);

  analogReadResolution(12);
  randomSeed(esp_random());

  connectWiFi();

  deviceMac = WiFi.macAddress();
  Serial.print(" MAC ESP32: ");
  Serial.println(deviceMac);

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
}

// ================== LOOP ==================
void loop() {
  float t, h, dust, lux, vbat;
  int eco2;

#if FAKE_SENSOR
  t     = fakeTemperature();
  h     = fakeHumidity();
  eco2  = fakeCO2();
  dust  = fakeDust();
  lux   = fakeLux();
  vbat  = fakeBatteryVolt();
#else
  // Khi dùng sensor thật sẽ đọc ở đây
#endif

  int batPercent = batteryPercent(vbat);
  String timeStr = getTimeString();

  Serial.printf(
    " %s | %s | T %.1fC | H %.1f%% | CO2 %d | Dust %.0f | Lux %.0f | Bat %d%%\n",
    deviceMac.c_str(),
    timeStr.c_str(),
    t, h, eco2, dust, lux, batPercent
  );

  sendData(t, h, lux, eco2, dust, batPercent);

  delay(1000);
}
