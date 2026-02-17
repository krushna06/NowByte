#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>

// ==========================
//  TOGGLES
// ==========================
#define USE_VOLUME   false
#define USE_DISPLAY  true

#define POT_PIN 34

#if USE_DISPLAY
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

const char* ssid = "Krushna";
const char* password = "9324399960";
const char* api_url = "https://nostep.xyz/api/lastfm";

unsigned long lastVolumeChange = 0;
unsigned long lastFetch = 0;

int lastVolume = -1;
int smoothedRaw = 0;

bool showingVolume = false;

String currentSong = "";
String lastDisplayed = "";

#if USE_DISPLAY
void showTextDynamic(String text) {

  if (text == lastDisplayed) return;
  lastDisplayed = text;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_unifont_t_korean2);

  int textWidth = u8g2.getUTF8Width(text.c_str());

  if (textWidth > 128) {
    int splitIndex = text.length() / 2;
    String line1 = text.substring(0, splitIndex);
    String line2 = text.substring(splitIndex);

    u8g2.drawUTF8(0, 14, line1.c_str());
    u8g2.drawUTF8(0, 30, line2.c_str());
  }
  else {
    int x = (128 - textWidth) / 2;
    u8g2.drawUTF8(x, 22, text.c_str());
  }

  u8g2.sendBuffer();
}
#else
void showTextDynamic(String text) {}
#endif

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void fetchNowPlaying() {

  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, api_url);

  int httpCode = http.GET();

  if (httpCode == 200) {

    String payload = http.getString();
    JsonDocument doc;

    if (deserializeJson(doc, payload)) {
      currentSong = "JSON Error";
      http.end();
      return;
    }

    if (!doc["success"] ||
        doc["nowPlaying"].isNull() ||
        !doc["nowPlaying"]["isNowPlaying"] ||
        doc["nowPlaying"]["name"].isNull()) {

      currentSong = "Nothing Playing";
      http.end();
      return;
    }

    String name = doc["nowPlaying"]["name"].as<String>();
    String artist = doc["nowPlaying"]["artist"].as<String>();

    if (artist == "Unknown" || artist.length() == 0)
      currentSong = name;
    else
      currentSong = name + " - " + artist;
  }
  else {
    currentSong = "HTTP Error";
  }

  http.end();
}

void setup() {

  Serial.begin(115200);

#if USE_DISPLAY
  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.enableUTF8Print();
  showTextDynamic("Booting...");
#endif

#if USE_VOLUME
  analogReadResolution(12);
  analogSetPinAttenuation(POT_PIN, ADC_11db);
  pinMode(POT_PIN, INPUT);
#endif

  delay(1000);

  connectWiFi();
  fetchNowPlaying();

#if USE_DISPLAY
  showTextDynamic(currentSong);
#endif
}

void loop() {

#if USE_VOLUME
  static int stableVolume = -1;
  static unsigned long volumeStableTimer = 0;

  int raw = analogRead(POT_PIN);
  smoothedRaw = (smoothedRaw * 4 + raw) / 5;
  int volume = map(smoothedRaw, 0, 4095, 0, 100);

  if (abs(volume - stableVolume) > 4) {
    stableVolume = volume;
    volumeStableTimer = millis();
  }

  if (millis() - volumeStableTimer > 150) {
    if (stableVolume != lastVolume) {
      lastVolume = stableVolume;
      lastVolumeChange = millis();
      showingVolume = true;
      showTextDynamic("Volume: " + String(lastVolume) + "%");
    }
  }

  if (showingVolume) {
    if (millis() - lastVolumeChange > 3000) {
      showingVolume = false;
      showTextDynamic(currentSong);
    }
  }
#endif

  if (!showingVolume) {
    if (millis() - lastFetch > 10000) {
      fetchNowPlaying();
#if USE_DISPLAY
      showTextDynamic(currentSong);
#endif
      lastFetch = millis();
    }
  }
}
