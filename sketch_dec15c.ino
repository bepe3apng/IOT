#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ILI9488_DMA.h>
#include <TJpg_Decoder.h>

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

ILI9488_DMA tft(TFT_CS, TFT_DC, TFT_RST);

// ====== WiFi ======
static const char* WIFI_SSID = "sharik";
static const char* WIFI_PASS = "temnota5";

// ====== PC gateway ======
static const char* PC_HOST = "192.168.1.2";
static const int   PC_PORT = 8080;
static const char* PC_PATH = "/fw";

// ====== FS ======
static const char* OUT_PATH = "/img.jpg";

// ====== Timeouts ======
static const uint32_t WIFI_TIMEOUT_MS    = 30000;
static const uint32_t STATUS_TIMEOUT_MS  = 15000;
static const uint32_t HEADER_LINE_TMO_MS = 15000;
static const uint32_t BODY_STALL_TMO_MS  = 30000;

// ====== Retry & Regeneration ======
static const uint8_t  MAX_REQUEST_RETRIES   = 3;
static const uint32_t RETRY_DELAY_MS         = 3000;
static const uint32_t REGENERATE_INTERVAL_MS = 1000UL * 30UL ; // 5 минут

static uint32_t lastGenerateTs = 0;
static bool generationInProgress = false;

// ====== JPEG callback ======
bool tftOutput(int16_t x, int16_t y,
               uint16_t w, uint16_t h,
               uint16_t* bitmap)
{
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

// ====== WiFi ======
static bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > WIFI_TIMEOUT_MS) return false;
    delay(500);
    yield();
  }
  return true;
}

// ====== FS ======
static bool ensureLittleFS() {
  if (LittleFS.begin()) return true;
  if (!LittleFS.format()) return false;
  return LittleFS.begin();
}

// ====== Networking helpers ======
static int readByteWithTimeout(WiFiClient& c, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (c.available()) return c.read();
    if (!c.connected()) return -1;
    delay(1);
    yield();
  }
  return -1;
}

static bool readLine(WiFiClient& c, String& out, uint32_t timeoutMs) {
  out = "";
  while (true) {
    int ch = readByteWithTimeout(c, timeoutMs);
    if (ch < 0) return false;
    if (ch == '\n') break;
    if (ch != '\r') out += (char)ch;
  }
  return true;
}

// ====== Read HTTP body ======
static bool readBodyToFile(WiFiClient& c, size_t len) {
  LittleFS.remove(OUT_PATH);
  File f = LittleFS.open(OUT_PATH, FILE_WRITE);
  if (!f) return false;

  size_t got = 0;
  uint8_t buf[2048];
  uint32_t lastProgress = millis();

  while (got < len) {
    if (!c.connected()) return false;
    int avail = c.available();
    if (avail <= 0) {
      if (millis() - lastProgress > BODY_STALL_TMO_MS) return false;
      delay(1);
      continue;
    }
    int n = c.read(buf, min(avail, (int)sizeof(buf)));
    if (n <= 0) continue;
    if (f.write(buf, n) != (size_t)n) return false;
    got += n;
    lastProgress = millis();
    yield();
  }
  f.close();
  return true;
}

// ====== HTTP request ======
static bool doRequestToPCAndDump() {
  WiFiClient client;
  if (!client.connect(PC_HOST, PC_PORT)) return false;

  String body =
    "{\"prompt\":\"A beautiful sunset over the ocean\","
    "\"cfg_scale\":7,\"width\":640,\"height\":1536,"
    "\"steps\":5,\"seed\":0}";

  client.printf(
    "POST %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json\r\n"
    "Accept: image/jpeg\r\n"
    "Connection: close\r\n"
    "Content-Length: %u\r\n\r\n%s",
    PC_PATH, PC_HOST, body.length(), body.c_str()
  );

  String line;
  if (!readLine(client, line, STATUS_TIMEOUT_MS)) return false;
  if (!line.startsWith("HTTP/1.1 200")) return false;

  int contentLength = -1;
  while (true) {
    if (!readLine(client, line, HEADER_LINE_TMO_MS)) return false;
    if (line.length() == 0) break;
    line.toLowerCase();
    if (line.startsWith("content-length:"))
      contentLength = line.substring(15).toInt();
  }

  if (contentLength <= 0) return false;
  if (!ensureLittleFS()) return false;

  bool ok = readBodyToFile(client, contentLength);
  client.stop();
  return ok;
}

// ====== Retry wrapper ======
static bool requestWithRetries() {
  for (uint8_t i = 1; i <= MAX_REQUEST_RETRIES; i++) {
    if (WiFi.status() != WL_CONNECTED && !connectWiFi()) {
      delay(RETRY_DELAY_MS);
      continue;
    }
    if (doRequestToPCAndDump()) return true;
    delay(RETRY_DELAY_MS);
  }
  return false;
}

// ====== Draw JPEG ======
static bool drawJpegFromFS() {
  if (!LittleFS.exists(OUT_PATH)) return false;
  tft.fillScreen(0x0000);
  TJpgDec.drawFsJpg(0, 0, OUT_PATH, LittleFS);
  return true;
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(200);

  // FS
  ensureLittleFS();

  // Display
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(0x0000);

  // JPEG decoder
  TJpgDec.setCallback(tftOutput);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setJpgScale(1);

  // ★★★ ВЫВОД ПОСЛЕДНЕГО СОХРАНЁННОГО ИЗОБРАЖЕНИЯ ★★★
  if (drawJpegFromFS()) {
    Serial.println("[BOOT] Last image displayed");
  } else {
    Serial.println("[BOOT] No saved image");
  }

  // WiFi запускаем, но не блокируем логику
  connectWiFi();

  // принудительная генерация по таймеру
  lastGenerateTs = 0;
}

// ====== LOOP ======
void loop() {
  uint32_t now = millis();

  if (!generationInProgress &&
      (now - lastGenerateTs >= REGENERATE_INTERVAL_MS)) {

    generationInProgress = true;

    if (requestWithRetries()) {
      drawJpegFromFS();
      lastGenerateTs = now;
    }

    generationInProgress = false;
  }

  yield();
}
