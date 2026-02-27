#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ArduinoJson.h>


void sendSpotifyVolume(int volume);
void drawScreen();
void fetchPlaybackInfo();


const char* ssid = "";
const char* password = "";
String spotifyAccessToken =
  "";

const int potPin = 34;

String currentSong = "";
String currentArtist = "";
String activeDeviceId = "";

int currentVolume = 0;
int progressMs = 0;
int durationMs = 1;

int scrollOffset = 0;
unsigned long lastScroll = 0;
const int scrollSpeed = 30;


unsigned long lastUpdate = 0;
const unsigned long updateInterval = 2000;


int lastVolume = -1;


U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, 22, 21
);


void setup() {
  Serial.begin(115200);
  delay(1000);

  u8g2.begin();
  u8g2.setFont(u8g2_font_7x14_tf);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  fetchPlaybackInfo();
}

void loop() {
  int raw = analogRead(potPin);
  int volume = map(raw, 0, 4095, 0, 100);

  if (abs(volume - lastVolume) > 3) {
    Serial.print("Sending volume: ");
    Serial.println(volume);
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

  const int volumeBarWidth = 12;
  const int padding = 6;
  const int textAreaWidth = 128 - volumeBarWidth - padding;

  String line1 = currentSong + " - " + currentArtist;
  int textWidth = u8g2.getStrWidth(line1.c_str());

  u8g2.setClipWindow(0, 0, textAreaWidth, 16);

  if (textWidth <= textAreaWidth) {
    int x = (textAreaWidth - textWidth) / 2;
    u8g2.drawStr(x, 14, line1.c_str());
  } else {
    if (millis() - lastScroll > scrollSpeed) {
      scrollOffset++;
      lastScroll = millis();
    }

    int gap = 30;
    int cycleWidth = textWidth + gap;

    if (scrollOffset >= cycleWidth)
      scrollOffset = 0;

    u8g2.drawStr(-scrollOffset, 14, line1.c_str());
    u8g2.drawStr(-scrollOffset + cycleWidth, 14, line1.c_str());
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
  if (activeDeviceId.length() == 0) {
    Serial.println("No active device ID!");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url =
    "https://api.spotify.com/v1/me/player/volume"
    "?device_id=" + activeDeviceId +
    "&volume_percent=" + String(volume);

  http.begin(client, url);
  http.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  http.addHeader("Content-Length", "0");

  int code = http.sendRequest("PUT");

  Serial.print("Volume HTTP Code: ");
  Serial.println(code);

  if (code != 204) {
    Serial.println("Volume change failed:");
    Serial.println(http.getString());
  }

  http.end();
}


void fetchPlaybackInfo() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player");
  http.addHeader("Authorization", "Bearer " + spotifyAccessToken);

  int code = http.GET();

  Serial.print("Playback HTTP Code: ");
  Serial.println(code);

  if (code == 200) {
    String payload = http.getString();
    Serial.println("JSON size: " + String(payload.length()));

    DynamicJsonDocument doc(12288);

    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      currentSong = doc["item"]["name"] | "";
      currentArtist = doc["item"]["artists"][0]["name"] | "";
      progressMs = doc["progress_ms"] | 0;
      durationMs = doc["item"]["duration_ms"] | 1;
      currentVolume = doc["device"]["volume_percent"] | 0;
      activeDeviceId = doc["device"]["id"] | "";

      Serial.println("Song: " + currentSong);
      Serial.println("Artist: " + currentArtist);
      Serial.println("Volume: " + String(currentVolume));
      Serial.println("Device: " + activeDeviceId);
    } else {
      Serial.println("JSON ERROR");
    }
  }

  http.end();
}