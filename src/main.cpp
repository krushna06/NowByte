#include <WiFi.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ArduinoJson.h>

void sendSpotifyVolume(int volume);
void drawScreen();
void fetchPlaybackInfo();

const char* ssid = "";
const char* password = "";
String spotifyAccessToken = "";

const int potPin = 34;
int lastVolume = -1;

String currentSong = "";
String currentArtist = "";
int currentVolume = 0;
int progressMs = 0;
int durationMs = 1;

int scrollOffset = 0;
unsigned long lastScroll = 0;
const int scrollSpeed = 30;

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 2000;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, 22, 21
);

void setup() {
  Serial.begin(115200);

  u8g2.begin();
  u8g2.setFont(u8g2_font_7x14_tf);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  fetchPlaybackInfo();
}

void loop() {
  int raw = analogRead(potPin);
  int volume = map(raw, 0, 4095, 0, 100);

  if (abs(volume - lastVolume) > 3) {
    sendSpotifyVolume(volume);
    lastVolume = volume;
  }

  if (millis() - lastUpdate > updateInterval) {
    fetchPlaybackInfo();
    lastUpdate = millis();
  }

  drawScreen();
  delay(30);
}

void drawScreen() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);

  const int volumeBarWidth = 12;
  const int padding = 6;
  const int textAreaWidth = 128 - volumeBarWidth - padding;

  String line1 = currentSong + " - " + currentArtist;
  int textWidth = u8g2.getStrWidth(line1.c_str());

  u8g2.setClipWindow(
    0,              // left
    0,              // top
    textAreaWidth,  // right boundary
    16              // text area height
  );

  if (textWidth <= textAreaWidth) {
    int x = (textAreaWidth - textWidth) / 2;
    u8g2.drawStr(x, 14, line1.c_str());
  } else {
    if (millis() - lastScroll > scrollSpeed) {
      scrollOffset++;
      lastScroll = millis();
    }

    if (scrollOffset > textWidth + 30)
      scrollOffset = 0;

    u8g2.drawStr(-scrollOffset, 14, line1.c_str());
  }

  u8g2.setMaxClipWindow();

  int barY = 20;
  int barHeight = 6;

  int progressWidth = map(progressMs, 0, durationMs, 0, textAreaWidth - 2);

  u8g2.drawFrame(0, barY, textAreaWidth, barHeight);

  if (progressWidth > 2) {
    u8g2.drawBox(1, barY + 1, progressWidth - 2, barHeight - 2);
  }

  int volumeHeight = map(currentVolume, 0, 100, 0, 30);

  int volumeX = 128 - volumeBarWidth;

  u8g2.drawFrame(volumeX, 0, volumeBarWidth, 32);
  u8g2.drawBox(volumeX + 3, 32 - volumeHeight, 6, volumeHeight);

  u8g2.sendBuffer();
}

void sendSpotifyVolume(int volume) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(volume);

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  http.addHeader("Content-Length", "0");

  int code = http.PUT("");
  Serial.print("Volume HTTP: ");
  Serial.println(code);

  http.end();
}

void fetchPlaybackInfo() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("https://api.spotify.com/v1/me/player");
  http.addHeader("Authorization", "Bearer " + spotifyAccessToken);

  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();

    DynamicJsonDocument doc(4096);
    if (!deserializeJson(doc, payload)) {
      currentSong = doc["item"]["name"].as<String>();
      currentArtist = doc["item"]["artists"][0]["name"].as<String>();

      progressMs = doc["progress_ms"].as<int>();
      durationMs = doc["item"]["duration_ms"].as<int>();

      currentVolume = doc["device"]["volume_percent"].as<int>();
    }
  }

  http.end();
}